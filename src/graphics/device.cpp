#include <coffee/graphics/device.hpp>

#include <coffee/graphics/command_buffer.hpp>
#include <coffee/graphics/exceptions.hpp>
#include <coffee/graphics/monitor.hpp>
#include <coffee/utils/log.hpp>
#include <coffee/utils/utils.hpp>
#include <coffee/utils/vk_utils.hpp>

#define VOLK_IMPLEMENTATION

#define VMA_ASSERT(expr) COFFEE_ASSERT(expr, "VMA assertion failed!");
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
#define VMA_IMPLEMENTATION

#include <basis_universal/basisu_transcoder.cpp>
#include <vma/vk_mem_alloc.h>
#include <volk/volk.h>

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

            vkDestroyDevice(logicalDevice_, nullptr);
            vkDestroyInstance(instance_, nullptr);

            tbb::queuing_mutex::scoped_lock lock { initializationMutex };

            if (initializationCounter-- == 1) {
                deinitializeGlobalEnvironment();
            }
        }

        DevicePtr Device::create() { return std::shared_ptr<Device>(new Device {}); }

        void Device::waitDeviceIdle()
        {
            std::vector<VkFence> fences {};

            {
                tbb::queuing_mutex::scoped_lock lock { tasksMutex_ };

                fences.reserve(runningTasks_.size());

                for (auto& task : runningTasks_) {
                    if (task.fenceHandle == VK_NULL_HANDLE) {
                        continue;
                    }

                    fences.push_back(task.fenceHandle);
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

                for (auto& task : runningTasks_) {
                    if (task.taskType != CommandBufferType::Transfer) {
                        continue;
                    }

                    if (task.fenceHandle == VK_NULL_HANDLE) {
                        continue;
                    }

                    fences.push_back(task.fenceHandle);
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

                for (auto& task : runningTasks_) {
                    if (task.taskType != CommandBufferType::Compute) {
                        continue;
                    }

                    if (task.fenceHandle == VK_NULL_HANDLE) {
                        continue;
                    }

                    fences.push_back(task.fenceHandle);
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

                for (auto& task : runningTasks_) {
                    if (task.taskType != CommandBufferType::Graphics) {
                        continue;
                    }

                    if (task.fenceHandle == VK_NULL_HANDLE) {
                        continue;
                    }

                    fences.push_back(task.fenceHandle);
                }
            }

            if (fences.empty()) {
                return;
            }

            vkWaitForFences(logicalDevice_, static_cast<uint32_t>(fences.size()), fences.data(), VK_TRUE, std::numeric_limits<uint64_t>::max());

            clearCompletedWork();
        }

        void Device::submit(CommandBuffer&& commandBuffer, const SubmitSemaphores& semaphores, const FencePtr& fence, bool waitAndReset)
        {
            VkResult result = VK_SUCCESS;
            SubmitInfo submitInfo {};

            if ((result = vkEndCommandBuffer(commandBuffer)) != VK_SUCCESS) {
                COFFEE_FATAL("Failed to end command buffer!");

                // Having this issue most likely mean that command buffer construction is broken
                // So our only valid case is throw fatal exception, even tho device might be active and everything is working correctly
                throw FatalVulkanException { result };
            }

            submitInfo.submitType = commandBuffer.type;
            submitInfo.commandBuffersCount = 1U;
            submitInfo.commandBuffers = std::make_unique<VkCommandBuffer[]>(1U);
            submitInfo.commandBuffers[0] = commandBuffer.buffer_;
            submitInfo.commandPools = std::make_unique<VkCommandPool[]>(1U);
            submitInfo.commandPools[0] = std::exchange(commandBuffer.pool_, VK_NULL_HANDLE);

            translateSemaphores(submitInfo, semaphores);

            if (fence != nullptr) {
                submitInfo.fence = fence->fence();
            }

            switch (submitInfo.submitType) {
                case CommandBufferType::Graphics:
                    return endGraphicsSubmit(std::move(submitInfo), fence, waitAndReset);
                case CommandBufferType::Compute:
                    return endComputeSubmit(std::move(submitInfo), fence, waitAndReset);
                case CommandBufferType::Transfer:
                    return endTransferSubmit(std::move(submitInfo), fence, waitAndReset);
            }
        }

        void Device::submit(std::vector<CommandBuffer>&& commandBuffers, const SubmitSemaphores& semaphores, const FencePtr& fence, bool waitAndReset)
        {
            if (commandBuffers.empty()) {
                return;
            }

            VkResult result = VK_SUCCESS;
            SubmitInfo submitInfo {};

            // Ensured to have at least 1 element by statement above
            submitInfo.submitType = commandBuffers[0].type;
            submitInfo.commandBuffersCount = static_cast<uint32_t>(commandBuffers.size());
            submitInfo.commandBuffers = std::make_unique<VkCommandBuffer[]>(commandBuffers.size());
            submitInfo.commandPools = std::make_unique<VkCommandPool[]>(commandBuffers.size());

            for (size_t index = 0; index < commandBuffers.size(); index++) {
                auto& commandBuffer = commandBuffers[index];

                COFFEE_ASSERT(submitInfo.submitType == commandBuffer.type, "All command buffers inside single submit must match by it's type.");

                if ((result = vkEndCommandBuffer(commandBuffer)) != VK_SUCCESS) {
                    COFFEE_FATAL("Failed to end command buffer!");

                    // Having this issue most likely mean that command buffer construction is broken
                    // So our only valid case is throw fatal exception, even tho device might be active and everything is working correctly
                    throw FatalVulkanException { result };
                }

                submitInfo.commandBuffers[index] = commandBuffer.buffer_;
                submitInfo.commandPools[index] = std::exchange(commandBuffer.pool_, VK_NULL_HANDLE);
            }

            translateSemaphores(submitInfo, semaphores);

            if (fence != nullptr) {
                submitInfo.fence = fence->fence();
            }

            switch (submitInfo.submitType) {
                case CommandBufferType::Graphics:
                    return endGraphicsSubmit(std::move(submitInfo), fence, waitAndReset);
                case CommandBufferType::Compute:
                    return endComputeSubmit(std::move(submitInfo), fence, waitAndReset);
                case CommandBufferType::Transfer:
                    return endTransferSubmit(std::move(submitInfo), fence, waitAndReset);
            }
        }

        void Device::present()
        {
            tbb::queuing_mutex::scoped_lock lock { pendingPresentsMutex_ };

            if (pendingPresents_.empty()) {
                return;
            }

            if (currentOperation_ % imageCountForSwapChain_ == 0) {
                clearCompletedWork();
            }

            std::unique_ptr<VkSemaphore[]> waitSemaphores = std::make_unique<VkSemaphore[]>(pendingPresents_.size());
            std::unique_ptr<VkSwapchainKHR[]> swapChains = std::make_unique<VkSwapchainKHR[]>(pendingPresents_.size());
            std::unique_ptr<uint32_t[]> imageIndices = std::make_unique<uint32_t[]>(pendingPresents_.size());

            VkPresentInfoKHR presentInfo { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
            presentInfo.waitSemaphoreCount = static_cast<uint32_t>(pendingPresents_.size());
            presentInfo.pWaitSemaphores = waitSemaphores.get();
            presentInfo.swapchainCount = static_cast<uint32_t>(pendingPresents_.size());
            presentInfo.pSwapchains = swapChains.get();
            presentInfo.pImageIndices = imageIndices.get();
            presentInfo.pResults = nullptr;

            for (size_t index = 0; index < pendingPresents_.size(); index++) {
                auto& pendingPresent = pendingPresents_[index];

                waitSemaphores[index] = pendingPresent.waitSemaphore;
                swapChains[index] = pendingPresent.swapChain;
                imageIndices[index] = *pendingPresent.currentFrame;

                *pendingPresent.currentFrame = (*pendingPresent.currentFrame + 1U) % imageCountForSwapChain_;
            }

            VkResult result = vkQueuePresentKHR(presentQueue_, &presentInfo);

            if (result != VK_SUCCESS) {
                COFFEE_FATAL("Failed to present image!");

                throw FatalVulkanException { result };
            }

            pendingPresents_.clear();

            currentOperationInFlight_ = (currentOperationInFlight_ + 1U) % Device::kMaxOperationsInFlight;
            currentOperation_ = (currentOperation_ + 1U) % imageCountForSwapChain_;

            vmaSetCurrentFrameIndex(allocator_, currentOperation_);
        }

        void Device::clearCompletedWork()
        {
            tbb::queuing_mutex::scoped_lock lock { tasksMutex_ };

            auto removePosition = std::remove_if(runningTasks_.begin(), runningTasks_.end(), [&](const Task& running) {
                bool canBeSafelyDeleted = true;

                if (running.fenceHandle != VK_NULL_HANDLE) {
                    canBeSafelyDeleted &= (vkGetFenceStatus(logicalDevice_, running.fenceHandle) == VK_SUCCESS);
                }

                if (canBeSafelyDeleted) {
                    cleanupCompletedTask(running);
                }

                return canBeSafelyDeleted;
            });

            runningTasks_.erase(removePosition, runningTasks_.end());
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
            createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
            createInfo.pQueueCreateInfos = queueCreateInfos.data();
            createInfo.pEnabledFeatures = &deviceFeatures;

            auto availableExtensions = VkUtils::getDeviceExtensions(physicalDevice_);
            std::vector<const char*> extensions {};

            // Required Vulkan extensions
            extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

            if (VkUtils::isExtensionsAvailable(availableExtensions, dedicatedAllocationExts)) {
                extensions.insert(extensions.end(), dedicatedAllocationExts.begin(), dedicatedAllocationExts.end());
                dedicatedAllocationExtensionEnabled = true;
            }

            if (memoryPriorityAndBudgetExtensionsEnabled && VkUtils::isExtensionsAvailable(availableExtensions, memoryPriorityAndBudgetDeviceExts)) {
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

                size_t memcpySize = semaphores.waitDstStageMasks.size() * sizeof(VkPipelineStageFlags);
                submitInfo.waitDstStageMasks = std::make_unique<VkPipelineStageFlags[]>(semaphores.waitDstStageMasks.size());
                std::memcpy(submitInfo.waitDstStageMasks.get(), semaphores.waitDstStageMasks.data(), memcpySize);
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

        void Device::returnFence(VkFence fence)
        {
            vkResetFences(logicalDevice_, 1, &fence);
            fencesPool_.push(fence);
        }

        void Device::notifyFenceCleanup(VkFence cleanedFence) noexcept
        {
            tbb::queuing_mutex::scoped_lock lock { tasksMutex_ };

            for (auto& task : runningTasks_) {
                if (task.fenceHandle == cleanedFence) {
                    task.fenceHandle = VK_NULL_HANDLE;
                }
            }
        }

        void Device::endGraphicsSubmit(SubmitInfo&& submit, const FencePtr& fence, bool waitAndReset)
        {
            bool fenceRequired = fence == nullptr;
            VkFence submitFence = fenceRequired ? acquireFence() : fence->fence();

            endSubmit(graphicsQueue_, graphicsQueueMutex_, submit, submitFence);

            // swapChain is not VK_NULL_HANDLE only for graphics submits
            if (submit.swapChain != VK_NULL_HANDLE) {
                tbb::queuing_mutex::scoped_lock lock { pendingPresentsMutex_ };

                PendingPresent pendingPresent {};
                pendingPresent.swapChain = submit.swapChain;
                pendingPresent.waitSemaphore = submit.swapChainWaitSemaphore;
                pendingPresent.currentFrame = submit.currentFrame;

                pendingPresents_.push_back(std::move(pendingPresent));
            }

            Task task {};
            task.taskType = submit.submitType;
            task.fenceHandle = submitFence;
            task.implementationProvidedFence = fenceRequired;
            task.commandBuffersCount = submit.commandBuffersCount;
            task.commandBuffers = std::move(submit.commandBuffers);
            task.commandPools = std::move(submit.commandPools);

            if (waitAndReset) {
                vkWaitForFences(logicalDevice_, 1, &submitFence, VK_TRUE, std::numeric_limits<uint64_t>::max());
                vkResetFences(logicalDevice_, 1, &submitFence);

                cleanupCompletedTask(task);

                return;
            }

            tbb::queuing_mutex::scoped_lock lock { tasksMutex_ };

            runningTasks_.push_back(std::move(task));
        }

        void Device::endComputeSubmit(SubmitInfo&& submit, const FencePtr& fence, bool waitAndReset)
        {
            if (computeQueue_ == VK_NULL_HANDLE) {
                return endGraphicsSubmit(std::move(submit), fence, waitAndReset);
            }

            bool fenceRequired = fence == nullptr;
            VkFence submitFence = fenceRequired ? acquireFence() : fence->fence();

            endSubmit(computeQueue_, computeQueueMutex_, submit, submitFence);

            Task task {};
            task.taskType = submit.submitType;
            task.fenceHandle = submitFence;
            task.implementationProvidedFence = fenceRequired;
            task.commandBuffersCount = submit.commandBuffersCount;
            task.commandBuffers = std::move(submit.commandBuffers);
            task.commandPools = std::move(submit.commandPools);

            if (waitAndReset) {
                vkWaitForFences(logicalDevice_, 1, &submitFence, VK_TRUE, std::numeric_limits<uint64_t>::max());
                vkResetFences(logicalDevice_, 1, &submitFence);

                cleanupCompletedTask(task);

                return;
            }

            tbb::queuing_mutex::scoped_lock lock { tasksMutex_ };

            runningTasks_.push_back(std::move(task));
        }

        void Device::endTransferSubmit(SubmitInfo&& submit, const FencePtr& fence, bool waitAndReset)
        {
            if (transferQueue_ == VK_NULL_HANDLE) {
                return endComputeSubmit(std::move(submit), fence, waitAndReset);
            }

            bool fenceRequired = fence == nullptr;
            VkFence submitFence = fenceRequired ? acquireFence() : fence->fence();

            endSubmit(transferQueue_, transferQueueMutex_, submit, submitFence);

            Task task {};
            task.taskType = submit.submitType;
            task.fenceHandle = submitFence;
            task.implementationProvidedFence = fenceRequired;
            task.commandBuffersCount = submit.commandBuffersCount;
            task.commandBuffers = std::move(submit.commandBuffers);
            task.commandPools = std::move(submit.commandPools);

            if (waitAndReset) {
                vkWaitForFences(logicalDevice_, 1, &submitFence, VK_TRUE, std::numeric_limits<uint64_t>::max());
                vkResetFences(logicalDevice_, 1, &submitFence);

                cleanupCompletedTask(task);

                return;
            }

            tbb::queuing_mutex::scoped_lock lock { tasksMutex_ };

            runningTasks_.push_back(std::move(task));
        }

        void Device::endSubmit(VkQueue queue, tbb::queuing_mutex& mutex, SubmitInfo& submit, VkFence fence)
        {
            tbb::queuing_mutex::scoped_lock lock { mutex };

            VkSubmitInfo submitInfo { VK_STRUCTURE_TYPE_SUBMIT_INFO };
            submitInfo.commandBufferCount = submit.commandBuffersCount;
            submitInfo.pCommandBuffers = submit.commandBuffers.get();
            submitInfo.waitSemaphoreCount = submit.waitSemaphoresCount;
            submitInfo.pWaitSemaphores = submit.waitSemaphores.get();
            submitInfo.pWaitDstStageMask = submit.waitDstStageMasks.get();
            submitInfo.signalSemaphoreCount = submit.signalSemaphoresCount;
            submitInfo.pSignalSemaphores = submit.signalSemaphores.get();

            VkResult result = vkQueueSubmit(queue, 1, &submitInfo, fence);

            if (result != VK_SUCCESS) {
                COFFEE_FATAL("Failed to submit to graphics queue!");

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
            if (computeQueue_ == VK_NULL_HANDLE) {
                return returnGraphicsCommandPoolAndBuffer(pool, buffer);
            }

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
            if (transferQueue_ == VK_NULL_HANDLE) {
                return returnComputeCommandPoolAndBuffer(pool, buffer);
            }

            vkResetCommandPool(logicalDevice_, pool, 0);
            transferPools_.push({ pool, buffer });
        }

        void Device::cleanupCompletedTask(const Task& task)
        {
            for (size_t index = 0; index < task.commandBuffersCount; index++) {
                auto& commandBuffer = task.commandBuffers[index];
                auto& commandPool = task.commandPools[index];

                switch (task.taskType) {
                    case CommandBufferType::Graphics:
                        returnGraphicsCommandPoolAndBuffer(commandPool, commandBuffer);
                        break;
                    case CommandBufferType::Compute:
                        returnComputeCommandPoolAndBuffer(commandPool, commandBuffer);
                        break;
                    case CommandBufferType::Transfer:
                        returnTransferCommandPoolAndBuffer(commandPool, commandBuffer);
                        break;
                }
            }

            if (task.implementationProvidedFence) {
                returnFence(task.fenceHandle);
            }
        }

    } // namespace graphics

} // namespace coffee