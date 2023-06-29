#include <coffee/graphics/device.hpp>

#include <coffee/graphics/command_buffer.hpp>
#include <coffee/graphics/exceptions.hpp>
#include <coffee/graphics/monitor.hpp>
#include <coffee/utils/log.hpp>
#include <coffee/utils/utils.hpp>
#include <coffee/utils/vk_utils.hpp>

#define VOLK_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#define VMA_IMPLEMENTATION

#include <volk/volk.h>

#include <GLFW/glfw3.h>
#include <basis_universal/basisu_transcoder.cpp>
#include <vma/vk_mem_alloc.h>

#include <array>
#include <set>
#include <stdexcept>
#include <vector>

namespace coffee {

    static const std::vector<const char*> dedicatedAllocationExts = { VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
                                                                      VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME };
    static const std::vector<const char*> memoryPriorityAndBudgetInstanceExts = { VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME };
    static const std::vector<const char*> memoryPriorityAndBudgetDeviceExts = { VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME,
                                                                                VK_EXT_MEMORY_BUDGET_EXTENSION_NAME };

#ifdef COFFEE_DEBUG
    static const std::vector<const char*> instanceDebugExtensions = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
    static const std::vector<const char*> instanceDebugLayers = { "VK_LAYER_KHRONOS_validation" };
#else
    static const std::vector<const char*> instanceDebugExtensions = {};
    static const std::vector<const char*> instanceDebugLayers = {};
#endif

    static tbb::queuing_mutex initializationMutex {};
    static uint32_t initializationCounter = 0;

    namespace graphics {

        Device::Device()
        {
            {
                tbb::queuing_mutex::scoped_lock lock { initializationMutex };

                if (initializationCounter == 0) {
                    initializeGlobalEnvironment();
                }

                // This must be done explicitly because initializeGlobalEnvironment might throw an exception
                initializationCounter++;
            }

            createInstance();
            createDebugMessenger();

            GLFWwindow* window = createTemporaryWindow();
            VkSurfaceKHR surface = createTemporarySurface(window);

            pickPhysicalDevice(surface);
            createLogicalDevice(surface);

            createSyncObjects();
            createDescriptorPool();
            createAllocator();

            destroyTemporarySurface(surface);
            destroyTemporaryWindow(window);
        }

        Device::~Device() noexcept
        {
            vkDeviceWaitIdle(logicalDevice_);

            std::pair<VkCommandPool, VkCommandBuffer> poolAndBuffer { VK_NULL_HANDLE, VK_NULL_HANDLE };

            clearCompletedWork();

            while (graphicsPools_.try_pop(poolAndBuffer)) {
                auto& [commandPool, commandBuffer] = poolAndBuffer;
                vkDestroyCommandPool(logicalDevice_, commandPool, nullptr);
            }

            while (computePools_.try_pop(poolAndBuffer)) {
                auto& [commandPool, commandBuffer] = poolAndBuffer;
                vkDestroyCommandPool(logicalDevice_, commandPool, nullptr);
            }

            while (transferPools_.try_pop(poolAndBuffer)) {
                auto& [commandPool, commandBuffer] = poolAndBuffer;
                vkDestroyCommandPool(logicalDevice_, commandPool, nullptr);
            }

            VkFence fence = VK_NULL_HANDLE;

            while (fencesPool_.try_pop(fence)) {
                vkDestroyFence(logicalDevice_, fence, nullptr);
            }

            if (vkDestroyDebugUtilsMessengerEXT != nullptr) {
                vkDestroyDebugUtilsMessengerEXT(instance_, debugMessenger_, nullptr);
            }

            vmaDestroyAllocator(allocator_);
            vkDestroyDescriptorPool(logicalDevice_, descriptorPool_, nullptr);

            for (size_t i = 0; i < kMaxOperationsInFlight; i++) {
                vkDestroyFence(logicalDevice_, fencesInFlight_[i], nullptr);
            }

            vkDestroyDevice(logicalDevice_, nullptr);
            vkDestroyInstance(instance_, nullptr);

            tbb::queuing_mutex::scoped_lock lock { initializationMutex };

            if (initializationCounter-- == 1) {
                deinitializeGlobalEnvironment();
            }
        }

        DevicePtr Device::create() { return std::shared_ptr<Device>(new Device {}); }

        void Device::waitForAcquire()
        {
            vkWaitForFences(logicalDevice_, 1U, &fencesInFlight_[currentOperationInFlight_], VK_TRUE, std::numeric_limits<uint64_t>::max());
        }

        void Device::waitForRelease()
        {
            uint32_t fencesSize = static_cast<uint32_t>(fencesInFlight_.size());
            vkWaitForFences(logicalDevice_, fencesSize, fencesInFlight_.data(), VK_TRUE, std::numeric_limits<uint64_t>::max());
        }

        void Device::waitDeviceIdle()
        {
            std::vector<VkFence> fences {};

            {
                tbb::queuing_mutex::scoped_lock lock { tasksMutex_ };

                for (auto& runningTask : runningTasks_) {
                    if (runningTask.globalCompletionFence != VK_NULL_HANDLE) {
                        fences.push_back(runningTask.globalCompletionFence);
                    }

                    fences.reserve(fences.size() + runningTask.computeCompletionFences.size() + runningTask.transferCompletionFences.size());

                    for (auto& computeOwnership : runningTask.computeCompletionFences) {
                        fences.push_back(computeOwnership);
                    }

                    for (auto& transferOwnership : runningTask.transferCompletionFences) {
                        fences.push_back(transferOwnership);
                    }
                }
            }

            if (fences.empty()) {
                return;
            }

            vkWaitForFences(logicalDevice_, static_cast<uint32_t>(fences.size()), fences.data(), VK_TRUE, std::numeric_limits<uint64_t>::max());

            clearCompletedWork();
        }

        void Device::waitTransferQueueIdle()
        {
            std::vector<VkFence> fences {};

            {
                tbb::queuing_mutex::scoped_lock lock { tasksMutex_ };

                for (auto& runningTask : runningTasks_) {
                    if (runningTask.globalCompletionFence != VK_NULL_HANDLE && runningTask.globalFenceType == CommandBufferType::Transfer) {
                        fences.push_back(runningTask.globalCompletionFence);
                    }

                    fences.reserve(fences.size() + runningTask.transferCompletionFences.size());

                    for (auto& transferOwnership : runningTask.transferCompletionFences) {
                        fences.push_back(transferOwnership);
                    }
                }
            }

            if (fences.empty()) {
                return;
            }

            vkWaitForFences(logicalDevice_, static_cast<uint32_t>(fences.size()), fences.data(), VK_TRUE, std::numeric_limits<uint64_t>::max());

            clearCompletedWork();
        }

        void Device::waitComputeQueueIdle()
        {
            std::vector<VkFence> fences {};

            {
                tbb::queuing_mutex::scoped_lock lock { tasksMutex_ };

                for (auto& runningTask : runningTasks_) {
                    if (runningTask.globalCompletionFence != VK_NULL_HANDLE && runningTask.globalFenceType == CommandBufferType::Compute) {
                        fences.push_back(runningTask.globalCompletionFence);
                    }

                    fences.reserve(fences.size() + runningTask.computeCompletionFences.size());

                    for (auto& computeOwnership : runningTask.computeCompletionFences) {
                        fences.push_back(computeOwnership);
                    }
                }
            }

            if (fences.empty()) {
                return;
            }

            vkWaitForFences(logicalDevice_, static_cast<uint32_t>(fences.size()), fences.data(), VK_TRUE, std::numeric_limits<uint64_t>::max());

            clearCompletedWork();
        }

        void Device::waitGraphicsQueueIdle()
        {
            std::vector<VkFence> fences {};

            {
                tbb::queuing_mutex::scoped_lock lock { tasksMutex_ };

                for (auto& runningTask : runningTasks_) {
                    if (runningTask.globalCompletionFence != VK_NULL_HANDLE && runningTask.globalFenceType == CommandBufferType::Graphics) {
                        fences.push_back(runningTask.globalCompletionFence);
                    }
                }
            }

            if (fences.empty()) {
                return;
            }

            vkWaitForFences(logicalDevice_, static_cast<uint32_t>(fences.size()), fences.data(), VK_TRUE, std::numeric_limits<uint64_t>::max());

            clearCompletedWork();
        }

        void Device::sendCommandBuffer(
            CommandBuffer&& commandBuffer,
            const SubmitSemaphores& submitSemaphores,
            const FencePtr& computeFence,
            const FencePtr& transferFence
        )
        {
            SubmitInfo info {};

            info.submitType = commandBuffer.type;
            translateSemaphores(info, submitSemaphores);

            if (computeFence != nullptr) {
                info.computeUserFence = computeFence->fence();
            }

            if (transferFence != nullptr) {
                info.transferUserFence = transferFence->fence();
            }

            std::vector<CommandBuffer> commandBuffers {};
            commandBuffers.push_back(std::move(commandBuffer));

            transferSubmitInfo(std::move(info), std::move(commandBuffers));
        }

        void Device::sendCommandBuffers(
            CommandBufferType submitType,
            std::vector<CommandBuffer>&& commandBuffers,
            const SubmitSemaphores& submitSemaphores,
            const FencePtr& computeFence,
            const FencePtr& transferFence
        )
        {
            SubmitInfo info {};

            info.submitType = submitType;
            translateSemaphores(info, submitSemaphores);

            if (computeFence != nullptr) {
                info.computeUserFence = computeFence->fence();
            }

            if (transferFence != nullptr) {
                info.transferUserFence = transferFence->fence();
            }

            transferSubmitInfo(std::move(info), std::move(commandBuffers));
        }

        void Device::submitPendingWork()
        {
            tbb::queuing_mutex::scoped_lock submitLock { submitMutex_ };

            if (pendingSubmits_.empty()) {
                return;
            }

            if (currentOperation_ % imageCountForSwapChain_ == 0) {
                clearCompletedWork();
            }

            std::vector<VkSubmitInfo> graphicsInfos {};
            std::vector<std::vector<VkSubmitInfo>> computeInfos {};
            std::vector<VkFence> computeFences {};
            std::vector<std::vector<VkSubmitInfo>> transferInfos {};
            std::vector<VkFence> transferFences {};
            std::vector<VkSwapchainKHR> swapChains {};
            std::vector<VkSemaphore> swapChainSemaphores {};
            std::vector<uint32_t> imageIndices {};

            computeInfos.push_back({});
            computeFences.push_back(VK_NULL_HANDLE);
            transferInfos.push_back({});
            transferFences.push_back(VK_NULL_HANDLE);

            for (const auto& submitInfo : pendingSubmits_) {
                VkSubmitInfo nativeSubmitInfo { VK_STRUCTURE_TYPE_SUBMIT_INFO };

                nativeSubmitInfo.commandBufferCount = submitInfo.commandBuffersCount;
                nativeSubmitInfo.pCommandBuffers = submitInfo.commandBuffers.get();
                nativeSubmitInfo.waitSemaphoreCount = submitInfo.waitSemaphoresCount;
                nativeSubmitInfo.pWaitSemaphores = submitInfo.waitSemaphores.get();
                nativeSubmitInfo.pWaitDstStageMask = submitInfo.waitDstStageMasks.get();
                nativeSubmitInfo.signalSemaphoreCount = submitInfo.signalSemaphoresCount;
                nativeSubmitInfo.pSignalSemaphores = submitInfo.signalSemaphores.get();

                if (submitInfo.swapChain != VK_NULL_HANDLE) {
                    // Swapchain always submitted through Graphics queue, so we don't need to check request type
                    // Same applies for currentFrame which is always not nullptr

                    swapChains.push_back(submitInfo.swapChain);
                    swapChainSemaphores.push_back(submitInfo.swapChainWaitSemaphore);
                    imageIndices.push_back(*submitInfo.currentFrame);

                    uint32_t* currentSwapChainFrame = submitInfo.currentFrame;
                    *currentSwapChainFrame = (*currentSwapChainFrame + 1) % imageCountForSwapChain_;
                }

                // User might wanna do multiple submits per frame, every of them might have it own fence
                // We must ensure that submits are tightly packed so we can reduce some overhead to driver

                switch (submitInfo.submitType) {
                    case CommandBufferType::Graphics:
                        // Graphics always have only one fence, provided by implementation itself
                        // So we will just accumulate all submits into one to reduce overhead
                        graphicsInfos.push_back(std::move(nativeSubmitInfo));
                        break;
                    case CommandBufferType::Compute:
                        if (computeFences.back() == VK_NULL_HANDLE) {
                            computeFences.back() = submitInfo.computeUserFence;
                        }
                        else if (submitInfo.computeUserFence != VK_NULL_HANDLE && computeFences.back() != submitInfo.computeUserFence) {
                            computeFences.push_back(submitInfo.computeUserFence);
                            computeInfos.emplace_back();
                        }

                        computeInfos.back().push_back(std::move(nativeSubmitInfo));
                        break;
                    case CommandBufferType::Transfer:
                        if (transferFences.back() == VK_NULL_HANDLE) {
                            transferFences.back() = submitInfo.transferUserFence;
                        }
                        else if (submitInfo.transferUserFence != VK_NULL_HANDLE && transferFences.back() != submitInfo.transferUserFence) {
                            transferFences.push_back(submitInfo.transferUserFence);
                            transferInfos.emplace_back();
                        }

                        transferInfos.back().push_back(std::move(nativeSubmitInfo));
                        break;
                }
            }

            VkPresentInfoKHR presentInfo { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
            presentInfo.waitSemaphoreCount = static_cast<uint32_t>(swapChainSemaphores.size());
            presentInfo.pWaitSemaphores = swapChainSemaphores.data();
            presentInfo.swapchainCount = static_cast<uint32_t>(swapChains.size());
            presentInfo.pSwapchains = swapChains.data();
            presentInfo.pImageIndices = imageIndices.data();

            if (operationsInFlight_[currentOperation_] != nullptr) {
                vkWaitForFences(logicalDevice_, 1, &operationsInFlight_[currentOperation_], VK_TRUE, std::numeric_limits<uint64_t>::max());
            }

            operationsInFlight_[currentOperation_] = fencesInFlight_[currentOperationInFlight_];
            vkResetFences(logicalDevice_, 1, &fencesInFlight_[currentOperationInFlight_]);

            if (!graphicsInfos.empty()) {
                pendingTask_.globalCompletionFence = { fencesInFlight_[currentOperationInFlight_], false };
                pendingTask_.globalFenceType = CommandBufferType::Graphics;

                endSubmit(graphicsQueue_, graphicsQueueMutex_, graphicsInfos, fencesInFlight_[currentOperationInFlight_]);
            }

            if (!computeInfos.empty()) {
                VkQueue submitQueue = computeQueue_ != VK_NULL_HANDLE ? computeQueue_ : graphicsQueue_;
                tbb::queuing_mutex& submitMutex = computeQueue_ != VK_NULL_HANDLE ? computeQueueMutex_ : graphicsQueueMutex_;

                for (size_t index = 0; index < computeInfos.size(); index++) {
                    bool fenceRequired = computeFences[index] == VK_NULL_HANDLE;
                    pendingTask_.computeCompletionFences.push_back({ fenceRequired ? acquireFence() : computeFences[index], fenceRequired });

                    ScopeGuard fenceGuard([&]() { returnFence(pendingTask_.computeCompletionFences.back()); });
                    endSubmit(submitQueue, submitMutex, computeInfos[index], pendingTask_.computeCompletionFences[index]);
                    fenceGuard.release();
                }
            }

            if (!transferInfos.empty()) {
                VkQueue submitQueue = transferQueue_ != VK_NULL_HANDLE  ? transferQueue_
                                      : computeQueue_ != VK_NULL_HANDLE ? computeQueue_
                                                                        : graphicsQueue_;
                tbb::queuing_mutex& submitMutex =
                    transferQueue_ != VK_NULL_HANDLE  ? transferQueueMutex_
                    : computeQueue_ != VK_NULL_HANDLE ? computeQueueMutex_
                                                      : graphicsQueueMutex_;

                for (size_t index = 0; index < transferInfos.size(); index++) {
                    bool fenceRequired = transferFences[index] == VK_NULL_HANDLE;
                    pendingTask_.transferCompletionFences.push_back({ fenceRequired ? acquireFence() : transferFences[index], fenceRequired });

                    ScopeGuard fenceGuard([&]() { returnFence(pendingTask_.transferCompletionFences.back()); });
                    endSubmit(submitQueue, submitMutex, transferInfos[index], pendingTask_.transferCompletionFences[index]);
                    fenceGuard.release();
                }
            }

            if (!swapChains.empty()) {
                VkResult result = vkQueuePresentKHR(presentQueue_, &presentInfo);

                if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR) {
                    COFFEE_FATAL("Failed to present images!");

                    throw FatalVulkanException { result };
                }
            }

            currentOperationInFlight_ = (currentOperationInFlight_ + 1) % kMaxOperationsInFlight;
            currentOperation_ = (currentOperation_ + 1) % imageCountForSwapChain_;

            {
                tbb::queuing_mutex::scoped_lock lock { tasksMutex_ };
                runningTasks_.push_back(std::move(pendingTask_));
            }

            pendingTask_.clear();
            pendingSubmits_.clear();

            vmaSetCurrentFrameIndex(allocator_, currentOperation_);
        }

        void Device::clearCompletedWork()
        {
            tbb::queuing_mutex::scoped_lock lock { tasksMutex_ };

            auto removePosition = std::remove_if(runningTasks_.begin(), runningTasks_.end(), [&](const RunningGPUTask& running) {
                bool canBeSafelyDeleted = true;

                if (canBeSafelyDeleted && running.globalCompletionFence != VK_NULL_HANDLE) {
                    canBeSafelyDeleted &= (vkGetFenceStatus(logicalDevice_, running.globalCompletionFence) == VK_SUCCESS);
                }

                for (size_t index = 0; index < running.computeCompletionFences.size() && canBeSafelyDeleted; index++) {
                    if (running.computeCompletionFences[index] != VK_NULL_HANDLE) {
                        canBeSafelyDeleted &= (vkGetFenceStatus(logicalDevice_, running.computeCompletionFences[index]) == VK_SUCCESS);
                    }
                }

                for (size_t index = 0; index < running.transferCompletionFences.size() && canBeSafelyDeleted; index++) {
                    if (running.transferCompletionFences[index] != VK_NULL_HANDLE) {
                        canBeSafelyDeleted &= (vkGetFenceStatus(logicalDevice_, running.transferCompletionFences[index]) == VK_SUCCESS);
                    }
                }

                if (canBeSafelyDeleted) {
                    for (auto& runningCommandBuffer : running.commandBuffers) {
                        switch (runningCommandBuffer.commandBufferType) {
                            case CommandBufferType::Graphics:
                                returnGraphicsCommandPoolAndBuffer(runningCommandBuffer.pool, runningCommandBuffer.buffer);
                                break;
                            case CommandBufferType::Compute:
                                returnComputeCommandPoolAndBuffer(runningCommandBuffer.pool, runningCommandBuffer.buffer);
                                break;
                            case CommandBufferType::Transfer:
                                returnTransferCommandPoolAndBuffer(runningCommandBuffer.pool, runningCommandBuffer.buffer);
                                break;
                        }
                    }

                    returnFence(running.globalCompletionFence);

                    for (auto& computeFenceOwnership : running.computeCompletionFences) {
                        returnFence(computeFenceOwnership);
                    }

                    for (auto& transferFenceOwnership : running.transferCompletionFences) {
                        returnFence(transferFenceOwnership);
                    }
                }

                return canBeSafelyDeleted;
            });

            runningTasks_.erase(removePosition, runningTasks_.end());
        }

        void Device::singleTimeOperation(CommandBuffer&& commandBuffer, const FencePtr& submitFence)
        {
            bool fenceRequired = submitFence == nullptr;
            FenceOwnership fenceOwnership { fenceRequired ? acquireFence() : submitFence->fence(), fenceRequired };

            if (transferQueue_ != VK_NULL_HANDLE && commandBuffer.type == CommandBufferType::Transfer) {
                return endSingleTimeCommands(transferQueue_, transferQueueMutex_, std::move(commandBuffer), std::move(fenceOwnership));
            }

            if (computeQueue_ != VK_NULL_HANDLE && commandBuffer.type != CommandBufferType::Graphics) {
                return endSingleTimeCommands(computeQueue_, computeQueueMutex_, std::move(commandBuffer), std::move(fenceOwnership));
            }

            return endSingleTimeCommands(graphicsQueue_, graphicsQueueMutex_, std::move(commandBuffer), std::move(fenceOwnership));
        }

        void Device::singleTimeTransfer(std::vector<CommandBuffer>&& transferCommandBuffers, const FencePtr& submitFence)
        {
            bool fenceRequired = submitFence == nullptr;
            FenceOwnership fenceOwnership { fenceRequired ? acquireFence() : submitFence->fence(), fenceRequired };

            if (transferQueue_ != VK_NULL_HANDLE) {
                return endSingleTimeCommands(transferQueue_, transferQueueMutex_, std::move(transferCommandBuffers), std::move(fenceOwnership));
            }

            if (computeQueue_ != VK_NULL_HANDLE) {
                return endSingleTimeCommands(computeQueue_, computeQueueMutex_, std::move(transferCommandBuffers), std::move(fenceOwnership));
            }

            return endSingleTimeCommands(graphicsQueue_, graphicsQueueMutex_, std::move(transferCommandBuffers), std::move(fenceOwnership));
        }

        void Device::singleTimeCompute(std::vector<CommandBuffer>&& computeCommandBuffers, const FencePtr& submitFence)
        {
            bool fenceRequired = submitFence == nullptr;
            FenceOwnership fenceOwnership { fenceRequired ? acquireFence() : submitFence->fence(), fenceRequired };

            if (computeQueue_ != VK_NULL_HANDLE) {
                return endSingleTimeCommands(computeQueue_, computeQueueMutex_, std::move(computeCommandBuffers), std::move(fenceOwnership));
            }

            return endSingleTimeCommands(graphicsQueue_, graphicsQueueMutex_, std::move(computeCommandBuffers), std::move(fenceOwnership));
        }

        void Device::singleTimeGraphics(std::vector<CommandBuffer>&& graphicsCommandBuffers, const FencePtr& submitFence)
        {
            bool fenceRequired = submitFence == nullptr;
            FenceOwnership fenceOwnership { fenceRequired ? acquireFence() : submitFence->fence(), fenceRequired };

            return endSingleTimeCommands(graphicsQueue_, graphicsQueueMutex_, std::move(graphicsCommandBuffers), std::move(fenceOwnership));
        }

        void Device::initializeGlobalEnvironment()
        {
            VkResult result = volkInitialize();

            if (result != VK_SUCCESS) {
                COFFEE_FATAL("Failed to load Vulkan library through Volk!");

                throw FatalVulkanException { result };
            }

            glfwInitVulkanLoader(vkGetInstanceProcAddr);

            Monitor::initialize();

            basist::basisu_transcoder_init();
        }

        void Device::deinitializeGlobalEnvironment() noexcept { Monitor::deinitialize(); }

        GLFWwindow* Device::createTemporaryWindow()
        {
            // ThreadSafety: Potential data race with Window class

            glfwDefaultWindowHints();
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

            GLFWwindow* window = glfwCreateWindow(1, 1, "Temp", nullptr, nullptr);

            if (window == nullptr) {
                COFFEE_FATAL("Failed to create temporary window for surface!");

                throw FatalVulkanException { VK_ERROR_EXTENSION_NOT_PRESENT };
            }

            return window;
        }

        VkSurfaceKHR Device::createTemporarySurface(GLFWwindow* window)
        {
            VkSurfaceKHR surface = VK_NULL_HANDLE;
            VkResult result = glfwCreateWindowSurface(instance_, window, nullptr, &surface);

            if (result != VK_SUCCESS) {
                COFFEE_FATAL("Failed to create temporary surface!");

                throw FatalVulkanException { result };
            }

            return surface;
        }

        void Device::destroyTemporarySurface(VkSurfaceKHR surface) { vkDestroySurfaceKHR(instance_, surface, nullptr); }

        void Device::destroyTemporaryWindow(GLFWwindow* window) { glfwDestroyWindow(window); }

        void Device::createInstance()
        {
            VkApplicationInfo appInfo { VK_STRUCTURE_TYPE_APPLICATION_INFO };
            appInfo.apiVersion = VK_API_VERSION_1_0;

            VkInstanceCreateInfo createInfo { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
            createInfo.pApplicationInfo = &appInfo;

            uint32_t glfwExtensionCount = 0;
            const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

            if (glfwExtensions == nullptr) {
                COFFEE_FATAL("Vulkan doesn't supported on this machine!");

                throw FatalVulkanException { VK_ERROR_INCOMPATIBLE_DRIVER };
            }

            auto availableExtensions = VkUtils::getInstanceExtensions();
            std::vector<const char*> extensions { glfwExtensions, glfwExtensions + glfwExtensionCount };

            // VMA extensions if present
            if (VkUtils::isExtensionsAvailable(availableExtensions, memoryPriorityAndBudgetInstanceExts)) {
                extensions.insert(extensions.end(), memoryPriorityAndBudgetInstanceExts.begin(), memoryPriorityAndBudgetInstanceExts.end());
                memoryPriorityAndBudgetExtensionsEnabled = true; // This will be set to false later if device isn't support extensions
            }

            // Debug extensions if debug build
            extensions.insert(extensions.end(), instanceDebugExtensions.begin(), instanceDebugExtensions.end());

            createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
            createInfo.ppEnabledExtensionNames = extensions.data();

            std::vector<const char*> instanceLayers {};

            // Debug layers if debug build
            instanceLayers.insert(instanceLayers.end(), instanceDebugLayers.begin(), instanceDebugLayers.end());

            createInfo.enabledLayerCount = static_cast<uint32_t>(instanceLayers.size());
            createInfo.ppEnabledLayerNames = instanceLayers.data();
            VkResult result = vkCreateInstance(&createInfo, nullptr, &instance_);

            if (result != VK_SUCCESS) {
                COFFEE_FATAL("Failed to create instance!");

                throw FatalVulkanException { result };
            }

            for ([[maybe_unused]] const char* extension : extensions) {
                COFFEE_INFO("Enabled instance extension: {}", extension);
            }

            for ([[maybe_unused]] const char* instanceLayer : instanceLayers) {
                COFFEE_INFO("Enabled instance layer: {}", instanceLayer);
            }

            volkLoadInstanceOnly(instance_);
        }

        void Device::createDebugMessenger()
        {
#ifdef COFFEE_DEBUG
            VkDebugUtilsMessengerCreateInfoEXT createInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };

            createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

            createInfo.pUserData = nullptr;
            createInfo.pfnUserCallback =
                [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                   VkDebugUtilsMessageTypeFlagsEXT messageType,
                   const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                   void* pUserData) -> VkBool32 {
                if (messageSeverity < VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                    return VK_FALSE;
                }

                switch (messageSeverity) {
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: {
                        COFFEE_INFO("{}", pCallbackData->pMessage);
                        return VK_FALSE;
                    }
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: {
                        COFFEE_WARNING("{}", pCallbackData->pMessage);
                        return VK_FALSE;
                    }
                    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: {
                        COFFEE_ERROR("{}", pCallbackData->pMessage);
                        return VK_FALSE;
                    }
                    default: {
                        return VK_FALSE;
                    }
                }

                return VK_FALSE;
            };

            // Volk will extract this pointer when vkLoadInstance is called
            if (vkCreateDebugUtilsMessengerEXT == nullptr) {
                COFFEE_WARNING("Failed to load vkCreateDebugUtilsMessengerEXT! Validation logging will be unavailable!");
                return;
            }

            static_cast<void>(vkCreateDebugUtilsMessengerEXT(instance_, &createInfo, nullptr, &debugMessenger_));
#endif
        }

        void Device::pickPhysicalDevice(VkSurfaceKHR surface)
        {
            uint32_t deviceCount = 0;
            vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);

            if (deviceCount == 0) {
                COFFEE_FATAL("Failed to find GPU with Vulkan support!");

                throw FatalVulkanException { VK_ERROR_INCOMPATIBLE_DRIVER };
            }

            std::vector<VkPhysicalDevice> devices { deviceCount };
            vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

            for (const VkPhysicalDevice& device : devices) {
                if (isDeviceSuitable(device, surface)) {
                    physicalDevice_ = device;
                    break;
                }
            }

            if (physicalDevice_ == VK_NULL_HANDLE) {
                COFFEE_FATAL("Failed to find suitable GPU for Vulkan!");

                throw FatalVulkanException { VK_ERROR_INCOMPATIBLE_DRIVER };
            }

            vkGetPhysicalDeviceProperties(physicalDevice_, &properties_);
            COFFEE_INFO("Selected physical device: {}", properties_.deviceName);

            // Surface is still alive at this point, so we can use this for our thingies
            imageCountForSwapChain_ = VkUtils::getOptimalAmountOfFramebuffers(physicalDevice_, surface);
            surfaceFormat_ = VkUtils::chooseSurfaceFormat(physicalDevice_, VkUtils::querySwapChainSupport(physicalDevice_, surface).formats);

            optimalDepthFormat_ = VkUtils::findDepthFormat(physicalDevice_);
            optimalDepthStencilFormat_ = VkUtils::findDepthStencilFormat(physicalDevice_);
        }

        void Device::createLogicalDevice(VkSurfaceKHR surface)
        {
            float queuePriority = 1.0f;

            indices_ = VkUtils::findQueueFamilies(physicalDevice_, surface);
            std::vector<VkDeviceQueueCreateInfo> queueCreateInfos {};
            std::set<uint32_t> uniqueQueueFamilies { indices_.graphicsFamily.value(), indices_.presentFamily.value() };

            if (indices_.computeFamily.has_value()) {
                uniqueQueueFamilies.insert(indices_.computeFamily.value());
            }

            if (indices_.transferFamily.has_value()) {
                uniqueQueueFamilies.insert(indices_.transferFamily.value());
            }

            for (uint32_t queueFamily : uniqueQueueFamilies) {
                VkDeviceQueueCreateInfo queueCreateInfo { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
                queueCreateInfo.queueFamilyIndex = queueFamily;
                queueCreateInfo.queueCount = 1;
                queueCreateInfo.pQueuePriorities = &queuePriority;

                queueCreateInfos.push_back(queueCreateInfo);
            }

            VkPhysicalDeviceFeatures deviceFeatures {};
            deviceFeatures.samplerAnisotropy = VK_TRUE;

            VkDeviceCreateInfo createInfo { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
            createInfo.pQueueCreateInfos = queueCreateInfos.data();
            createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
            createInfo.pEnabledFeatures = &deviceFeatures;

            auto availableExtensions = VkUtils::getDeviceExtensions(physicalDevice_);
            std::vector<const char*> extensions {};

            // Required Vulkan extensions
            extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

            if (VkUtils::isExtensionsAvailable(availableExtensions, dedicatedAllocationExts)) {
                extensions.insert(extensions.end(), dedicatedAllocationExts.begin(), dedicatedAllocationExts.end());
                dedicatedAllocationExtensionEnabled = true;
            }

            if (memoryPriorityAndBudgetExtensionsEnabled) {
                VkPhysicalDeviceFeatures2KHR deviceFeatures { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
                VkPhysicalDeviceMemoryPriorityFeaturesEXT memoryPriority { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT };

                deviceFeatures.pNext = &memoryPriority;
                vkGetPhysicalDeviceFeatures2KHR(physicalDevice_, &deviceFeatures);

                if (memoryPriority.memoryPriority == VK_TRUE &&
                    VkUtils::isExtensionsAvailable(availableExtensions, memoryPriorityAndBudgetDeviceExts)) {
                    extensions.insert(extensions.end(), memoryPriorityAndBudgetDeviceExts.begin(), memoryPriorityAndBudgetDeviceExts.end());
                    memoryPriorityAndBudgetExtensionsEnabled = true;
                }
                else {
                    // Tell VMA that we didn't support such functionality on device level
                    memoryPriorityAndBudgetExtensionsEnabled = false;
                }
            }

            createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
            createInfo.ppEnabledExtensionNames = extensions.data();
            VkResult result = vkCreateDevice(physicalDevice_, &createInfo, nullptr, &logicalDevice_);

            if (result != VK_SUCCESS) {
                COFFEE_FATAL("Failed to create logical device!");

                throw FatalVulkanException { result };
            }

            for ([[maybe_unused]] const char* extension : extensions) {
                COFFEE_INFO("Enabled device extension: {}", extension);
            }

            volkLoadDevice(logicalDevice_);

            vkGetDeviceQueue(logicalDevice_, indices_.graphicsFamily.value(), 0, &graphicsQueue_);
            vkGetDeviceQueue(logicalDevice_, indices_.presentFamily.value(), 0, &presentQueue_);

            if (indices_.computeFamily.has_value()) {
                vkGetDeviceQueue(logicalDevice_, indices_.computeFamily.value(), 0, &computeQueue_);
            }

            if (indices_.transferFamily.has_value()) {
                vkGetDeviceQueue(logicalDevice_, indices_.transferFamily.value(), 0, &transferQueue_);
            }
        }

        void Device::createSyncObjects()
        {
            operationsInFlight_.resize(imageCountForSwapChain_);

            VkFenceCreateInfo fenceCreateInfo { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
            fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            VkResult result = VK_SUCCESS;

            for (size_t i = 0; i < kMaxOperationsInFlight; i++) {
                if ((result = vkCreateFence(logicalDevice_, &fenceCreateInfo, nullptr, &fencesInFlight_[i])) != VK_SUCCESS) {
                    COFFEE_FATAL("Failed to create queue fence!");

                    throw FatalVulkanException { result };
                }
            }
        }

        void Device::createDescriptorPool()
        {
            // clang-format off
            constexpr std::array<VkDescriptorPoolSize, 4> descriptorSizes = {
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_SAMPLER,                512U  },
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096U },
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1024U },
                VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1024U },
            };
            // clang-format on

            constexpr uint32_t totalSize = [&]() -> uint32_t {
                uint32_t size = 0;

                for (size_t i = 0; i < descriptorSizes.size(); i++) {
                    size += descriptorSizes[i].descriptorCount;
                }

                return size;
            }();

            VkDescriptorPoolCreateInfo descriptorPoolInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
            descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(descriptorSizes.size());
            descriptorPoolInfo.pPoolSizes = descriptorSizes.data();
            descriptorPoolInfo.maxSets = totalSize;
            descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            VkResult result = vkCreateDescriptorPool(logicalDevice_, &descriptorPoolInfo, nullptr, &descriptorPool_);

            if (result != VK_SUCCESS) {
                COFFEE_FATAL("Failed to create descriptor pool!");

                throw FatalVulkanException { result };
            }
        }

        void Device::createAllocator()
        {
            VmaVulkanFunctions vulkanFunctions {};
            vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
            vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
            vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
            vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
            vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
            vulkanFunctions.vkFreeMemory = vkFreeMemory;
            vulkanFunctions.vkMapMemory = vkMapMemory;
            vulkanFunctions.vkUnmapMemory = vkUnmapMemory;
            vulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
            vulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
            vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
            vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
            vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
            vulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
            vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
            vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
            vulkanFunctions.vkCreateImage = vkCreateImage;
            vulkanFunctions.vkDestroyImage = vkDestroyImage;
            vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;

            if (dedicatedAllocationExtensionEnabled) {
                vulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2KHR;
                vulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2KHR;
            }

            if (memoryPriorityAndBudgetExtensionsEnabled) {
                vulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2KHR;
            }

            VmaAllocatorCreateInfo allocatorCreateInfo {};
            allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_0;
            allocatorCreateInfo.physicalDevice = physicalDevice_;
            allocatorCreateInfo.device = logicalDevice_;
            allocatorCreateInfo.instance = instance_;
            allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

            if (dedicatedAllocationExtensionEnabled) {
                allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
            }

            if (memoryPriorityAndBudgetExtensionsEnabled) {
                allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
                allocatorCreateInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;
            }

            // Actual vmaCreateAllocator implementation ALWAYS returns VK_SUCCESS
            vmaCreateAllocator(&allocatorCreateInfo, &allocator_);

            vmaSetCurrentFrameIndex(allocator_, currentOperation_);
        }

        bool Device::isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface, const std::vector<const char*>& additionalExtensions)
        {
            VkUtils::QueueFamilyIndices indices = VkUtils::findQueueFamilies(device, surface);

            std::set<std::string> requiredExtensions {};
            requiredExtensions.insert(additionalExtensions.begin(), additionalExtensions.end());
            auto availableExtensions = VkUtils::getDeviceExtensions(device);

            for (const VkExtensionProperties& extension : availableExtensions) {
                requiredExtensions.erase(extension.extensionName);
            }

            bool swapChainAdequate = false;
            if (requiredExtensions.empty()) {
                VkUtils::SwapChainSupportDetails swapChainSupport = VkUtils::querySwapChainSupport(device, surface);
                swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
            }

            VkPhysicalDeviceFeatures features {};
            vkGetPhysicalDeviceFeatures(device, &features);

            return indices.isSuitable() && requiredExtensions.empty() && swapChainAdequate && features.samplerAnisotropy;
        }

        void Device::translateSemaphores(SubmitInfo& submitInfo, const SubmitSemaphores& semaphores)
        {
            COFFEE_ASSERT(
                semaphores.waitSemaphores.size() == semaphores.waitDstStageMasks.size(),
                "Amount of wait stages must be equal to amount of wait semaphores."
            );

            if (!semaphores.waitSemaphores.empty()) {
                submitInfo.waitSemaphoresCount = static_cast<uint32_t>(semaphores.waitSemaphores.size());
                submitInfo.waitSemaphores = std::make_unique<VkSemaphore[]>(semaphores.waitSemaphores.size());

                for (size_t index = 0; index < semaphores.waitSemaphores.size(); index++) {
                    COFFEE_ASSERT(semaphores.waitSemaphores[index] != nullptr, "Invalid wait semaphore provided.");

                    submitInfo.waitSemaphores[index] = semaphores.waitSemaphores[index]->semaphore();
                }

                submitInfo.waitDstStageMasks = std::make_unique<VkPipelineStageFlags[]>(semaphores.waitDstStageMasks.size());
                std::memcpy(
                    submitInfo.waitDstStageMasks.get(),
                    semaphores.waitDstStageMasks.data(),
                    semaphores.waitDstStageMasks.size() * sizeof(VkPipelineStageFlags)
                );
            }

            if (!semaphores.signalSemaphores.empty()) {
                submitInfo.signalSemaphoresCount = static_cast<uint32_t>(semaphores.signalSemaphores.size());
                submitInfo.signalSemaphores = std::make_unique<VkSemaphore[]>(semaphores.signalSemaphores.size());

                for (size_t index = 0; index < semaphores.signalSemaphores.size(); index++) {
                    COFFEE_ASSERT(semaphores.signalSemaphores[index] != nullptr, "Invalid signal semaphore provided.");

                    submitInfo.signalSemaphores[index] = semaphores.signalSemaphores[index]->semaphore();
                }
            }
        }

        void Device::transferSubmitInfo(SubmitInfo&& submitInfo, std::vector<CommandBuffer>&& commandBuffers)
        {
            COFFEE_ASSERT(!commandBuffers.empty(), "Application shouldn't send empty command buffer list.");

            VkResult result = VK_SUCCESS;
            submitInfo.commandBuffersCount = static_cast<uint32_t>(commandBuffers.size());
            submitInfo.commandBuffers = std::make_unique<VkCommandBuffer[]>(commandBuffers.size());

            tbb::queuing_mutex::scoped_lock lock { submitMutex_ };

            for (size_t index = 0; index < commandBuffers.size(); index++) {
                auto& commandBuffer = commandBuffers[index];

                COFFEE_ASSERT(
                    isCommandBufferAllowedForSubmitType(submitInfo.submitType, commandBuffer.type),
                    "Invalid command buffer submit: Command buffer requires higher level of queue than requested."
                );

                if ((result = vkEndCommandBuffer(commandBuffer)) != VK_SUCCESS) {
                    COFFEE_FATAL("Failed to end command buffer!");

                    // Having this issue most likely mean that command buffer construction is broken
                    // So our only valid case is throw fatal exception, even tho device might be active and everything is working correctly
                    throw FatalVulkanException { result };
                }

                RunningCommandBuffer runningCommandBuffer {};
                runningCommandBuffer.commandBufferType = commandBuffer.type;
                runningCommandBuffer.pool = std::exchange(commandBuffer.pool_, VK_NULL_HANDLE);
                runningCommandBuffer.buffer = std::exchange(commandBuffer.buffer_, VK_NULL_HANDLE);

                submitInfo.commandBuffers[index] = runningCommandBuffer.buffer;
                pendingTask_.commandBuffers.push_back(std::move(runningCommandBuffer));
            }

            commandBuffers.clear();

            pendingSubmits_.push_back(std::move(submitInfo));
        }

        VkFence Device::acquireFence()
        {
            VkFence fence = VK_NULL_HANDLE;

            if (fencesPool_.try_pop(fence)) {
                return fence;
            }

            VkFenceCreateInfo createInfo { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
            VkResult result = vkCreateFence(logicalDevice_, &createInfo, nullptr, &fence);

            if (result != VK_SUCCESS) {
                COFFEE_ERROR("Failed to create fence to single time command buffer!");

                throw RegularVulkanException { result };
            }

            return fence;
        }

        void Device::returnFence(const FenceOwnership& fenceOwnership)
        {
            if (!fenceOwnership.owned || fenceOwnership == VK_NULL_HANDLE) {
                return;
            }

            VkFence fence = fenceOwnership;
            vkResetFences(logicalDevice_, 1, &fence);

            fencesPool_.push(fenceOwnership);
        }

        void Device::notifyFenceDestruction(VkFence destroyedFence) noexcept
        {
            tbb::queuing_mutex::scoped_lock lock { tasksMutex_ };

            for (auto& task : runningTasks_) {
                if (task.globalCompletionFence == destroyedFence) {
                    task.globalCompletionFence = VK_NULL_HANDLE;
                }

                for (auto& computeFence : task.computeCompletionFences) {
                    if (computeFence == destroyedFence) {
                        computeFence = VK_NULL_HANDLE;
                    }
                }

                for (auto& transferFence : task.transferCompletionFences) {
                    if (transferFence == destroyedFence) {
                        transferFence = VK_NULL_HANDLE;
                    }
                }
            }
        }

        void Device::endSingleTimeCommands(VkQueue queue, tbb::queuing_mutex& mutex, CommandBuffer&& buffer, FenceOwnership&& fence)
        {
            ScopeGuard fenceGuard([&]() { returnFence(fence); });

            VkResult result = vkEndCommandBuffer(buffer);

            if (result != VK_SUCCESS) {
                COFFEE_ERROR("Failed to end single time command buffer!");

                throw RegularVulkanException { result };
            }

            VkCommandBuffer vkCommandBuffer = buffer;
            VkSubmitInfo submitInfo { VK_STRUCTURE_TYPE_SUBMIT_INFO };
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &vkCommandBuffer;

            {
                tbb::queuing_mutex::scoped_lock lock { mutex };
                result = vkQueueSubmit(queue, 1, &submitInfo, fence);

                if (result != VK_SUCCESS) {
                    COFFEE_FATAL("Failed to submit single time command buffer!");

                    throw FatalVulkanException { result };
                }
            }

            RunningCommandBuffer operation {};
            operation.commandBufferType = buffer.type;
            operation.pool = std::exchange(buffer.pool_, VK_NULL_HANDLE);
            operation.buffer = std::exchange(buffer.buffer_, VK_NULL_HANDLE);

            RunningGPUTask task {};
            task.globalCompletionFence = std::move(fence);
            task.globalFenceType = buffer.type;
            task.commandBuffers.push_back(std::move(operation));

            tbb::queuing_mutex::scoped_lock lock { tasksMutex_ };
            runningTasks_.push_back(std::move(task));

            fenceGuard.release();
        }

        void Device::endSingleTimeCommands(VkQueue queue, tbb::queuing_mutex& mutex, std::vector<CommandBuffer>&& buffers, FenceOwnership&& fence)
        {
            ScopeGuard fenceGuard([&]() { returnFence(fence); });

            if (buffers.empty()) {
                return;
            }

            VkResult result = VK_SUCCESS;

            std::vector<VkCommandBuffer> implementationCommandBuffers {};
            implementationCommandBuffers.resize(buffers.size());

            RunningGPUTask task {};

            for (size_t i = 0; i < buffers.size(); i++) {
                result = vkEndCommandBuffer(buffers[i]);

                if (result != VK_SUCCESS) {
                    COFFEE_ERROR("Failed to end single time command buffer!");

                    throw RegularVulkanException { result };
                }

                implementationCommandBuffers[i] = buffers[i];

                RunningCommandBuffer operation {};
                operation.commandBufferType = buffers[i].type;
                operation.pool = std::exchange(buffers[i].pool_, VK_NULL_HANDLE);
                operation.buffer = std::exchange(buffers[i].buffer_, VK_NULL_HANDLE);
                task.commandBuffers.push_back(std::move(operation));

                task.globalFenceType = buffers[i].type;
            }

            VkSubmitInfo submitInfo { VK_STRUCTURE_TYPE_SUBMIT_INFO };
            submitInfo.commandBufferCount = implementationCommandBuffers.size();
            submitInfo.pCommandBuffers = implementationCommandBuffers.data();

            {
                tbb::queuing_mutex::scoped_lock lock { mutex };
                result = vkQueueSubmit(queue, 1, &submitInfo, fence);

                if (result != VK_SUCCESS) {
                    COFFEE_FATAL("Failed to submit single time command buffer!");

                    throw FatalVulkanException { result };
                }
            }

            task.globalCompletionFence = std::move(fence);

            tbb::queuing_mutex::scoped_lock lock { tasksMutex_ };
            runningTasks_.push_back(std::move(task));

            fenceGuard.release();
        }

        void Device::endSubmit(VkQueue queue, tbb::queuing_mutex& mutex, const std::vector<VkSubmitInfo>& submits, VkFence fence)
        {
            tbb::queuing_mutex::scoped_lock queueLock { mutex };

            VkResult result = vkQueueSubmit(queue, static_cast<uint32_t>(submits.size()), submits.data(), fence);

            if (result != VK_SUCCESS) {
                COFFEE_FATAL("Failed to submit command buffers through graphics queue!");

                throw FatalVulkanException { result };
            }
        }

        std::pair<VkCommandPool, VkCommandBuffer> Device::acquireGraphicsCommandPoolAndBuffer()
        {
            std::pair<VkCommandPool, VkCommandBuffer> poolAndBuffer { VK_NULL_HANDLE, VK_NULL_HANDLE };

            if (graphicsPools_.try_pop(poolAndBuffer)) {
                return poolAndBuffer;
            }

            auto& [commandPool, commandBuffer] = poolAndBuffer;
            VkResult result = VK_SUCCESS;

            VkCommandPoolCreateInfo poolInfo { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
            poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            poolInfo.queueFamilyIndex = indices_.graphicsFamily.value();
            result = vkCreateCommandPool(logicalDevice_, &poolInfo, nullptr, &commandPool);

            if (result != VK_SUCCESS) {
                COFFEE_ERROR("Failed to create command pool from graphics queue!");

                throw RegularVulkanException { result };
            }

            VkCommandBufferAllocateInfo allocateInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
            allocateInfo.commandPool = commandPool;
            allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocateInfo.commandBufferCount = 1;

            if ((result = vkAllocateCommandBuffers(logicalDevice_, &allocateInfo, &commandBuffer)) != VK_SUCCESS) {
                COFFEE_ERROR("Failed to allocate command buffer!");

                vkDestroyCommandPool(logicalDevice_, commandPool, nullptr);

                throw RegularVulkanException { result };
            }

            return poolAndBuffer;
        }

        void Device::returnGraphicsCommandPoolAndBuffer(VkCommandPool pool, VkCommandBuffer buffer)
        {
            vkResetCommandPool(logicalDevice_, pool, 0);
            graphicsPools_.push({ pool, buffer });
        }

        std::pair<VkCommandPool, VkCommandBuffer> Device::acquireComputeCommandPoolAndBuffer()
        {
            if (computeQueue_ == VK_NULL_HANDLE) {
                return acquireGraphicsCommandPoolAndBuffer();
            }

            std::pair<VkCommandPool, VkCommandBuffer> poolAndBuffer { VK_NULL_HANDLE, VK_NULL_HANDLE };

            if (computePools_.try_pop(poolAndBuffer)) {
                return poolAndBuffer;
            }

            auto& [commandPool, commandBuffer] = poolAndBuffer;
            VkResult result = VK_SUCCESS;

            VkCommandPoolCreateInfo poolInfo { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
            poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            poolInfo.queueFamilyIndex = indices_.computeFamily.value();
            result = vkCreateCommandPool(logicalDevice_, &poolInfo, nullptr, &commandPool);

            if (result != VK_SUCCESS) {
                COFFEE_ERROR("Failed to create command pool from compute queue!");

                throw RegularVulkanException { result };
            }

            VkCommandBufferAllocateInfo allocateInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
            allocateInfo.commandPool = commandPool;
            allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocateInfo.commandBufferCount = 1;

            if ((result = vkAllocateCommandBuffers(logicalDevice_, &allocateInfo, &commandBuffer)) != VK_SUCCESS) {
                COFFEE_ERROR("Failed to allocate command buffer!");

                vkDestroyCommandPool(logicalDevice_, commandPool, nullptr);

                throw RegularVulkanException { result };
            }

            return poolAndBuffer;
        }

        void Device::returnComputeCommandPoolAndBuffer(VkCommandPool pool, VkCommandBuffer buffer)
        {
            vkResetCommandPool(logicalDevice_, pool, 0);
            computePools_.push({ pool, buffer });
        }

        std::pair<VkCommandPool, VkCommandBuffer> Device::acquireTransferCommandPoolAndBuffer()
        {
            if (transferQueue_ == VK_NULL_HANDLE) {
                return acquireComputeCommandPoolAndBuffer();
            }

            std::pair<VkCommandPool, VkCommandBuffer> poolAndBuffer { VK_NULL_HANDLE, VK_NULL_HANDLE };

            if (transferPools_.try_pop(poolAndBuffer)) {
                return poolAndBuffer;
            }

            auto& [commandPool, commandBuffer] = poolAndBuffer;
            VkResult result = VK_SUCCESS;

            VkCommandPoolCreateInfo poolInfo { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
            poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            poolInfo.queueFamilyIndex = indices_.transferFamily.value();
            result = vkCreateCommandPool(logicalDevice_, &poolInfo, nullptr, &commandPool);

            if (result != VK_SUCCESS) {
                COFFEE_ERROR("Failed to create command pool from transfer queue!");

                throw RegularVulkanException { result };
            }

            VkCommandBufferAllocateInfo allocateInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
            allocateInfo.commandPool = commandPool;
            allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocateInfo.commandBufferCount = 1;

            if ((result = vkAllocateCommandBuffers(logicalDevice_, &allocateInfo, &commandBuffer)) != VK_SUCCESS) {
                COFFEE_ERROR("Failed to allocate command buffer!");

                vkDestroyCommandPool(logicalDevice_, commandPool, nullptr);

                throw RegularVulkanException { result };
            }

            return poolAndBuffer;
        }

        void Device::returnTransferCommandPoolAndBuffer(VkCommandPool pool, VkCommandBuffer buffer)
        {
            vkResetCommandPool(logicalDevice_, pool, 0);
            transferPools_.push({ pool, buffer });
        }

        bool Device::isCommandBufferAllowedForSubmitType(CommandBufferType submitType, CommandBufferType commandBufferType)
        {
            if (submitType == CommandBufferType::Graphics) {
                return true;
            }

            if (submitType == CommandBufferType::Compute) {
                return commandBufferType != CommandBufferType::Graphics;
            }

            return commandBufferType == CommandBufferType::Transfer;
        }

    } // namespace graphics

} // namespace coffee