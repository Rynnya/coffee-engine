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

    static std::mutex initializationMutex {};
    static uint32_t initializationCounter = 0;

    namespace graphics {

        Device::Device()
        {
            {
                std::unique_lock<std::mutex> lock { initializationMutex };

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

            for (size_t i = 0; i < poolsAndBuffers_.size(); i++) {
                clearCommandBuffers(i);
            }

            while (graphicsPools_.try_pop(poolAndBuffer)) {
                auto& [commandPool, commandBuffer] = poolAndBuffer;
                vkFreeCommandBuffers(logicalDevice_, commandPool, 1, &commandBuffer);
                vkDestroyCommandPool(logicalDevice_, commandPool, nullptr);
            }

            while (computePools_.try_pop(poolAndBuffer)) {
                auto& [commandPool, commandBuffer] = poolAndBuffer;
                vkFreeCommandBuffers(logicalDevice_, commandPool, 1, &commandBuffer);
                vkDestroyCommandPool(logicalDevice_, commandPool, nullptr);
            }

            while (transferPools_.try_pop(poolAndBuffer)) {
                auto& [commandPool, commandBuffer] = poolAndBuffer;
                vkFreeCommandBuffers(logicalDevice_, commandPool, 1, &commandBuffer);
                vkDestroyCommandPool(logicalDevice_, commandPool, nullptr);
            }

            VkFence fence = VK_NULL_HANDLE;

            while (singleTimeFences_.try_pop(fence)) {
                vkDestroyFence(logicalDevice_, fence, nullptr);
            }

            if (vkDestroyDebugUtilsMessengerEXT != nullptr) {
                vkDestroyDebugUtilsMessengerEXT(instance_, debugMessenger_, nullptr);
            }

            vmaDestroyAllocator(allocator_);
            vkDestroyDescriptorPool(logicalDevice_, descriptorPool_, nullptr);

            for (size_t i = 0; i < kMaxOperationsInFlight; i++) {
                vkDestroyFence(logicalDevice_, inFlightFences_[i], nullptr);
            }

            vkDestroyDevice(logicalDevice_, nullptr);
            vkDestroyInstance(instance_, nullptr);

            std::unique_lock<std::mutex> lock { initializationMutex };

            if (initializationCounter-- == 1) {
                deinitializeGlobalEnvironment();
            }
        }

        DevicePtr Device::create() { return std::shared_ptr<Device>(new Device {}); }

        void Device::waitForAcquire()
        {
            vkWaitForFences(logicalDevice_, 1U, &inFlightFences_[currentOperationInFlight_], VK_TRUE, std::numeric_limits<uint64_t>::max());
        }

        void Device::waitForRelease()
        {
            vkWaitForFences(
                logicalDevice_,
                static_cast<uint32_t>(inFlightFences_.size()),
                inFlightFences_.data(),
                VK_TRUE,
                std::numeric_limits<uint64_t>::max()
            );
        }

        void Device::sendCommandBuffer(
            CommandBuffer&& commandBuffer,
            VkSwapchainKHR swapChain,
            VkSemaphore waitSemaphone,
            VkSemaphore signalSemaphone,
            uint32_t* currentFrame
        )
        {
            SubmitInfo info {};
            info.swapChain = swapChain;
            info.waitSemaphone = waitSemaphone;
            info.signalSemaphone = signalSemaphone;
            info.currentFrame = currentFrame;
            VkResult result = VK_SUCCESS;

            std::scoped_lock<std::mutex> lock { submitMutex_ };

            if (poolsAndBuffersClearFlags_[currentOperation_]) {
                clearCommandBuffers(currentOperation_);
            }

            if ((result = vkEndCommandBuffer(commandBuffer)) != VK_SUCCESS) {
                COFFEE_FATAL("Failed to end command buffer!");

                // Having this issue most likely mean that command buffer construction is broken
                // So our only valid case is throw fatal exception, even tho device might be active and everything is working correctly
                throw FatalVulkanException { result };
            }

            poolsAndBuffers_[currentOperation_].push_back({ std::exchange(commandBuffer.pool_, VK_NULL_HANDLE), commandBuffer });
            info.commandBuffers.push_back(commandBuffer);
            pendingSubmits_.push_back(std::move(info));
        }

        void Device::sendCommandBuffers(
            std::vector<CommandBuffer>&& commandBuffers,
            VkSwapchainKHR swapChain,
            VkSemaphore waitSemaphone,
            VkSemaphore signalSemaphone,
            uint32_t* currentFrame
        )
        {
            SubmitInfo info {};
            info.swapChain = swapChain;
            info.waitSemaphone = waitSemaphone;
            info.signalSemaphone = signalSemaphone;
            info.currentFrame = currentFrame;
            VkResult result = VK_SUCCESS;

            std::scoped_lock<std::mutex> lock { submitMutex_ };

            if (poolsAndBuffersClearFlags_[currentOperation_]) {
                clearCommandBuffers(currentOperation_);
            }

            for (auto& commandBuffer : commandBuffers) {
                if ((result = vkEndCommandBuffer(commandBuffer)) != VK_SUCCESS) {
                    COFFEE_FATAL("Failed to end command buffer!");

                    // Having this issue most likely mean that command buffer construction is broken
                    // So our only valid case is throw fatal exception, even tho device might be active and everything is working correctly
                    throw FatalVulkanException { result };
                }

                poolsAndBuffers_[currentOperation_].push_back({ std::exchange(commandBuffer.pool_, VK_NULL_HANDLE), commandBuffer });
                info.commandBuffers.push_back(commandBuffer);
            }

            pendingSubmits_.push_back(std::move(info));
            commandBuffers.clear();
        }

        void Device::submitPendingWork()
        {
            // This WHOLE operation must be atomic, that's why using tbb container here irrelevant
            std::scoped_lock<std::mutex> submitLock { submitMutex_ };

            if (pendingSubmits_.empty()) {
                return;
            }

            static constexpr VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
            poolsAndBuffersClearFlags_[currentOperation_] = true;

            std::vector<VkSubmitInfo> submitInfos {};
            std::vector<VkSwapchainKHR> swapChains {};
            std::vector<VkSemaphore> swapChainSemaphores {};
            std::vector<uint32_t> imageIndices {};

            for (const auto& submitInfo : pendingSubmits_) {
                VkSubmitInfo nativeSubmitInfo { VK_STRUCTURE_TYPE_SUBMIT_INFO };

                if (submitInfo.swapChain != VK_NULL_HANDLE) {
                    COFFEE_ASSERT(submitInfo.waitSemaphone != VK_NULL_HANDLE, "Invalid wait semaphore provided.");
                    COFFEE_ASSERT(submitInfo.signalSemaphone != VK_NULL_HANDLE, "Invalid signal semaphore provided.");
                    COFFEE_ASSERT(submitInfo.currentFrame != nullptr, "Invalid current frame pointer provided.");

                    nativeSubmitInfo.waitSemaphoreCount = 1;
                    nativeSubmitInfo.pWaitSemaphores = &submitInfo.waitSemaphone;
                    nativeSubmitInfo.pWaitDstStageMask = waitStages;

                    nativeSubmitInfo.signalSemaphoreCount = 1;
                    nativeSubmitInfo.pSignalSemaphores = &submitInfo.signalSemaphone;

                    swapChains.push_back(submitInfo.swapChain);
                    swapChainSemaphores.push_back(submitInfo.signalSemaphone);
                    imageIndices.push_back(*submitInfo.currentFrame);

                    uint32_t* currentSwapChainFrame = submitInfo.currentFrame;
                    *currentSwapChainFrame = (*currentSwapChainFrame + 1) % imageCountForSwapChain_;
                }

                nativeSubmitInfo.commandBufferCount = static_cast<uint32_t>(submitInfo.commandBuffers.size());
                nativeSubmitInfo.pCommandBuffers = submitInfo.commandBuffers.data();

                submitInfos.push_back(std::move(nativeSubmitInfo));
            }

            VkPresentInfoKHR presentInfo { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };

            presentInfo.waitSemaphoreCount = static_cast<uint32_t>(swapChainSemaphores.size());
            presentInfo.pWaitSemaphores = swapChainSemaphores.data();
            presentInfo.swapchainCount = static_cast<uint32_t>(swapChains.size());
            presentInfo.pSwapchains = swapChains.data();
            presentInfo.pImageIndices = imageIndices.data();

            // Push waiting for swapchain as far as possible
            if (operationsInFlight_[currentOperation_] != nullptr) {
                vkWaitForFences(logicalDevice_, 1, &operationsInFlight_[currentOperation_], VK_TRUE, std::numeric_limits<uint64_t>::max());
            }

            operationsInFlight_[currentOperation_] = inFlightFences_[currentOperationInFlight_];
            vkResetFences(logicalDevice_, 1, &inFlightFences_[currentOperationInFlight_]);

            std::scoped_lock<std::mutex> queueLock { graphicsQueueMutex_ };

            VkResult result = vkQueueSubmit(
                graphicsQueue_,
                static_cast<uint32_t>(submitInfos.size()),
                submitInfos.data(),
                inFlightFences_[currentOperationInFlight_]
            );

            if (result != VK_SUCCESS) {
                COFFEE_FATAL("Failed to submit command buffers!");

                throw FatalVulkanException { result };
            }

            if (swapChains.size() != 0) {
                result = vkQueuePresentKHR(presentQueue_, &presentInfo);

                if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR) {
                    COFFEE_FATAL("Failed to present images!");

                    throw FatalVulkanException { result };
                }
            }

            currentOperationInFlight_ = (currentOperationInFlight_ + 1) % kMaxOperationsInFlight;
            currentOperation_ = (currentOperation_ + 1) % imageCountForSwapChain_;

            pendingSubmits_.clear();

            vmaSetCurrentFrameIndex(allocator_, currentOperation_);
        }

        ScopeExit Device::singleTimeTransfer(CommandBuffer&& transferCommandBuffer)
        {
            COFFEE_ASSERT(
                transferCommandBuffer.type == CommandBufferType::Transfer,
                "Provided command buffer must been allocated for transfer."
            );

            if (transferQueue_ != VK_NULL_HANDLE) {
                return endSingleTimeCommands(transferQueue_, transferQueueMutex_, std::move(transferCommandBuffer));
            }

            // Try to submit into compute queue if possible
            if (computeQueue_ != VK_NULL_HANDLE) {
                return endSingleTimeCommands(computeQueue_, computeQueueMutex_, std::move(transferCommandBuffer));
            }

            // Fallback to graphics queue, potentially can cause a stallers because of pipeline barriers
            return endSingleTimeCommands(graphicsQueue_, graphicsQueueMutex_, std::move(transferCommandBuffer));
        }

        ScopeExit Device::singleTimeTransfer(std::vector<CommandBuffer>&& transferCommandBuffers)
        {
            for ([[maybe_unused]] auto& transferCommandBuffer : transferCommandBuffers) {
                COFFEE_ASSERT(
                    transferCommandBuffer.type == CommandBufferType::Transfer,
                    "Provided command buffer must been allocated for transfer."
                );
            }

            if (transferQueue_ != VK_NULL_HANDLE) {
                return endSingleTimeCommands(transferQueue_, transferQueueMutex_, std::move(transferCommandBuffers));
            }

            // Try to submit into compute queue if possible
            if (computeQueue_ != VK_NULL_HANDLE) {
                return endSingleTimeCommands(computeQueue_, computeQueueMutex_, std::move(transferCommandBuffers));
            }

            // Fallback to graphics queue, potentially can cause a stallers because of pipeline barriers
            return endSingleTimeCommands(graphicsQueue_, graphicsQueueMutex_, std::move(transferCommandBuffers));
        }

        ScopeExit Device::singleTimeCompute(CommandBuffer&& computeCommandBuffer)
        {
            COFFEE_ASSERT(
                computeCommandBuffer.type != CommandBufferType::Graphics,
                "Provided command buffer must been allocated for transfer or compute."
            );

            if (computeQueue_ != VK_NULL_HANDLE) {
                return endSingleTimeCommands(computeQueue_, computeQueueMutex_, std::move(computeCommandBuffer));
            }

            // Fallback to graphics queue, potentially can cause a stallers because of pipeline barriers
            return endSingleTimeCommands(graphicsQueue_, graphicsQueueMutex_, std::move(computeCommandBuffer));
        }

        ScopeExit Device::singleTimeCompute(std::vector<CommandBuffer>&& computeCommandBuffers)
        {
            for ([[maybe_unused]] auto& computeCommandBuffer : computeCommandBuffers) {
                COFFEE_ASSERT(
                    computeCommandBuffer.type != CommandBufferType::Graphics,
                    "Provided command buffer must been allocated for transfer or compute."
                );
            }

            if (computeQueue_ != VK_NULL_HANDLE) {
                return endSingleTimeCommands(computeQueue_, computeQueueMutex_, std::move(computeCommandBuffers));
            }

            // Fallback to graphics queue, potentially can cause a stallers because of pipeline barriers
            return endSingleTimeCommands(graphicsQueue_, graphicsQueueMutex_, std::move(computeCommandBuffers));
        }

        ScopeExit Device::singleTimeGraphics(CommandBuffer&& graphicsCommandBuffer)
        {
            return endSingleTimeCommands(graphicsQueue_, graphicsQueueMutex_, std::move(graphicsCommandBuffer));
        }

        ScopeExit Device::singleTimeGraphics(std::vector<CommandBuffer>&& graphicsCommandBuffers)
        {
            return endSingleTimeCommands(graphicsQueue_, graphicsQueueMutex_, std::move(graphicsCommandBuffers));
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
            // TODO: Potential data race with Window class

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
            surfaceFormat_ =
                VkUtils::chooseSurfaceFormat(physicalDevice_, VkUtils::querySwapChainSupport(physicalDevice_, surface).formats);

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

            poolsAndBuffersClearFlags_.resize(imageCountForSwapChain_);
            poolsAndBuffers_.resize(imageCountForSwapChain_);

            VkFenceCreateInfo fenceCreateInfo { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
            fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            VkResult result = VK_SUCCESS;

            for (size_t i = 0; i < kMaxOperationsInFlight; i++) {
                if ((result = vkCreateFence(logicalDevice_, &fenceCreateInfo, nullptr, &inFlightFences_[i])) != VK_SUCCESS) {
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
            VmaVulkanFunctions vulkanFunctions = {};
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

        VkFence Device::acquireSingleTimeFence()
        {
            VkFence fence = VK_NULL_HANDLE;

            if (singleTimeFences_.try_pop(fence)) {
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

        ScopeExit Device::endSingleTimeCommands(VkQueue queue, std::mutex& mutex, CommandBuffer&& commandBuffer)
        {
            VkFence fence = acquireSingleTimeFence();
            VkResult result = vkEndCommandBuffer(commandBuffer);

            if (result != VK_SUCCESS) {
                COFFEE_ERROR("Failed to end single time command buffer!");

                throw RegularVulkanException { result };
            }

            VkCommandBuffer vkCommandBuffer = commandBuffer;
            VkSubmitInfo submitInfo { VK_STRUCTURE_TYPE_SUBMIT_INFO };
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &vkCommandBuffer;

            {
                std::scoped_lock<std::mutex> lock { mutex };
                result = vkQueueSubmit(queue, 1, &submitInfo, fence);

                if (result != VK_SUCCESS) {
                    COFFEE_FATAL("Failed to submit single time command buffer!");

                    throw FatalVulkanException { result };
                }
            }

            return ScopeExit([this, fence, cb = std::move(commandBuffer)]() {
                vkWaitForFences(logicalDevice_, 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
                vkResetFences(logicalDevice_, 1, &fence);

                try {
                    singleTimeFences_.push(fence);
                }
                catch (...) {
                    vkDestroyFence(logicalDevice_, fence, nullptr);
                }
            });
        }

        ScopeExit Device::endSingleTimeCommands(VkQueue queue, std::mutex& mutex, std::vector<CommandBuffer>&& commandBuffers)
        {
            VkFence fence = acquireSingleTimeFence();
            VkResult result = VK_SUCCESS;

            std::vector<VkCommandBuffer> implementationCommandBuffers {};
            implementationCommandBuffers.resize(commandBuffers.size());

            for (size_t i = 0; i < commandBuffers.size(); i++) {
                result = vkEndCommandBuffer(commandBuffers[i]);

                if (result != VK_SUCCESS) {
                    COFFEE_ERROR("Failed to end single time command buffer!");

                    throw RegularVulkanException { result };
                }

                implementationCommandBuffers[i] = commandBuffers[i];
            }

            VkSubmitInfo submitInfo { VK_STRUCTURE_TYPE_SUBMIT_INFO };
            submitInfo.commandBufferCount = implementationCommandBuffers.size();
            submitInfo.pCommandBuffers = implementationCommandBuffers.data();

            {
                std::scoped_lock<std::mutex> lock { mutex };
                result = vkQueueSubmit(queue, 1, &submitInfo, fence);

                if (result != VK_SUCCESS) {
                    COFFEE_FATAL("Failed to submit single time command buffer!");

                    throw FatalVulkanException { result };
                }
            }

            return ScopeExit([this, fence, cbs = std::move(commandBuffers), implcbs = std::move(implementationCommandBuffers)]() {
                vkWaitForFences(logicalDevice_, 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
                vkResetFences(logicalDevice_, 1, &fence);

                try {
                    singleTimeFences_.push(fence);
                }
                catch (...) {
                    vkDestroyFence(logicalDevice_, fence, nullptr);
                }
            });
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
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
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

        void Device::returnGraphicsCommandPoolAndBuffer(const std::pair<VkCommandPool, VkCommandBuffer>& buffer)
        {
            vkResetCommandBuffer(buffer.second, 0);
            graphicsPools_.push(buffer);
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
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
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

        void Device::returnComputeCommandPoolAndBuffer(const std::pair<VkCommandPool, VkCommandBuffer>& buffer)
        {
            vkResetCommandBuffer(buffer.second, 0);
            computePools_.push(buffer);
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
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
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

        void Device::returnTransferCommandPoolAndBuffer(const std::pair<VkCommandPool, VkCommandBuffer>& buffer)
        {
            vkResetCommandBuffer(buffer.second, 0);
            transferPools_.push(buffer);
        }

        void Device::clearCommandBuffers(size_t index)
        {
            waitForAcquire();

            for (auto& commandPoolAndBuffer : poolsAndBuffers_[index]) {
                returnGraphicsCommandPoolAndBuffer(commandPoolAndBuffer);
            }

            poolsAndBuffers_[index].clear();
            poolsAndBuffersClearFlags_[index] = false;
        }

    } // namespace graphics

} // namespace coffee