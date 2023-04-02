#include <coffee/abstract/vulkan/vk_device.hpp>

#include <coffee/abstract/vulkan/vk_buffer.hpp>
#include <coffee/abstract/vulkan/vk_image.hpp>
#include <coffee/utils/log.hpp>
#include <coffee/utils/vk_utils.hpp>

#define VOLK_IMPLEMENTATION
#include <volk.h>

#include <GLFW/glfw3.h>

#include <array>
#include <set>
#include <stdexcept>
#include <vector>

namespace coffee {

    VulkanDevice::VulkanDevice() {
        COFFEE_THROW_IF(volkInitialize() != VK_SUCCESS, "Failed to find Vulkan library!");

        createInstance();
        createDebugMessenger();
        volkLoadInstance(instance_);

        GLFWwindow* window = nullptr;
        VkSurfaceKHR surface = nullptr;

        {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

            window = glfwCreateWindow(1, 1, "Temp", nullptr, nullptr);
            COFFEE_THROW_IF(window == nullptr, "Failed to create temporary window for surface!");
            COFFEE_THROW_IF(glfwCreateWindowSurface(instance_, window, nullptr, &surface) != VK_SUCCESS, "Failed to create temporary surface!");

            glfwDefaultWindowHints();
        }

        pickPhysicalDevice(surface);
        createLogicalDevice(surface);

        createSyncObjects();
        createCommandPool();
        createDescriptorPool();

        {
            vkDestroySurfaceKHR(instance_, surface, nullptr);
            glfwDestroyWindow(window);
        }
    }

    VulkanDevice::~VulkanDevice() {
        for (const auto& pool : pools_) {
            vkDestroyCommandPool(logicalDevice_, pool, nullptr);
        }

        auto func =
            reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));

        if (func != nullptr) {
            func(instance_, debugMessenger_, nullptr);
        }

        vkDestroyDescriptorPool(logicalDevice_, descriptorPool_, nullptr);
        vkDestroyCommandPool(logicalDevice_, deviceCommandPool_, nullptr);

        for (size_t i = 0; i < AbstractDevice::maxOperationsInFlight; i++) {
            vkDestroyFence(logicalDevice_, inFlightFences_[i], nullptr);
        }

        vkDestroyDevice(logicalDevice_, nullptr);
        vkDestroyInstance(instance_, nullptr);
    }

    bool VulkanDevice::isDeviceSuitable(
        VkPhysicalDevice device,
        VkSurfaceKHR surface,
        const std::vector<const char*>& additionalExtensions
    ) {
        // TODO: Change to rating system and allow user in future to select which GPU must be used
        VkUtils::QueueFamilyIndices indices = VkUtils::findQueueFamilies(device, surface);
        bool extensionsSupported = checkDeviceExtensionsSupport(device, additionalExtensions);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            VkUtils::SwapChainSupportDetails swapChainSupport = VkUtils::querySwapChainSupport(device, surface);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        VkPhysicalDeviceFeatures features {};
        vkGetPhysicalDeviceFeatures(device, &features);

        return indices.isComplete() && extensionsSupported && swapChainAdequate && features.samplerAnisotropy;
    }

    bool VulkanDevice::checkDeviceExtensionsSupport(VkPhysicalDevice device, const std::vector<const char*>& extensions) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions { extensionCount };
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> extensionsSet {};
        extensionsSet.insert(extensions.begin(), extensions.end());

        for (const VkExtensionProperties& extension : availableExtensions) {
            extensionsSet.erase(extension.extensionName);
        }

        return extensionsSet.empty();
    }

    VkCommandPool VulkanDevice::getUniqueCommandPool() {

        {
            std::scoped_lock<std::mutex> lock { poolsMutex_ };

            if (!pools_.empty()) {
                VkCommandPool alreadyConstructed = pools_.back();
                pools_.pop_back();

                return alreadyConstructed;
            }
        }

        VkCommandPool pool = nullptr;

        VkCommandPoolCreateInfo poolInfo { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        poolInfo.flags = 0;
        poolInfo.queueFamilyIndex = indices_.graphicsFamily.value();

        COFFEE_THROW_IF(
            vkCreateCommandPool(logicalDevice_, &poolInfo, nullptr, &pool) != VK_SUCCESS, "Failed to create command pool!");

        return pool;
    }

    void VulkanDevice::returnCommandPool(VkCommandPool pool) {
        std::scoped_lock<std::mutex> lock { poolsMutex_ };

        pools_.push_back(pool);
    }

    void VulkanDevice::copyBuffer(const Buffer& srcBuffer, const Buffer& dstBuffer) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VulkanBuffer* srcBufferImpl = static_cast<VulkanBuffer*>(srcBuffer.get());
        VulkanBuffer* dstBufferImpl = static_cast<VulkanBuffer*>(dstBuffer.get());

        VkBufferCopy copyRegion {};
        copyRegion.srcOffset = 0; // Optional
        copyRegion.dstOffset = 0; // Optional
        copyRegion.size = srcBufferImpl->getTotalSize();
        vkCmdCopyBuffer(commandBuffer, srcBufferImpl->buffer, dstBufferImpl->buffer, 1, &copyRegion);

        endSingleTimeCommands(commandBuffer);
    }

    void VulkanDevice::copyBufferToImage(const Buffer& srcBuffer, const Image& dstImage) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VulkanBuffer* srcBufferImpl = static_cast<VulkanBuffer*>(srcBuffer.get());
        VulkanImage* dstImageImpl = static_cast<VulkanImage*>(dstImage.get());

        VkImageMemoryBarrier copyBarrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        copyBarrier.srcAccessMask = 0;
        copyBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        copyBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        copyBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copyBarrier.image = dstImageImpl->image;
        copyBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyBarrier.subresourceRange.levelCount = 1;
        copyBarrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &copyBarrier
        );

        VkBufferImageCopy copyRegion {};
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent.width = dstImage->getWidth();
        copyRegion.imageExtent.height = dstImage->getHeight();
        copyRegion.imageExtent.depth = 1;
        vkCmdCopyBufferToImage(
            commandBuffer,
            srcBufferImpl->buffer,
            dstImageImpl->image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &copyRegion
        );

        VkImageMemoryBarrier useBarrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        useBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        useBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        useBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        useBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        useBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        useBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        useBarrier.image = dstImageImpl->image;
        useBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        useBarrier.subresourceRange.levelCount = 1;
        useBarrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &useBarrier
        );

        endSingleTimeCommands(commandBuffer);
    }

    void VulkanDevice::waitForAcquire() {
        vkWaitForFences(logicalDevice_, 1, &inFlightFences_[currentOperationInFlight], VK_TRUE, std::numeric_limits<uint64_t>::max());
    }

    void VulkanDevice::sendCommandBuffers(const SwapChainSubmit& swapChainSubmit, const std::vector<VkCommandBuffer>& commandBuffers) {
        std::scoped_lock<std::mutex> lock {
            queueMutex_
        }; // This call must be synchronized because it can be called from swapchains or backend
        pendingSubmits_.push_back({ swapChainSubmit, commandBuffers });
    }

    void VulkanDevice::submitPendingWork() {
        std::scoped_lock<std::mutex> lock { queueMutex_ };

        std::vector<VkSubmitInfo> submitInfos {};
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        for (const auto& submitInfo : pendingSubmits_) {
            VkSubmitInfo nativeSubmitInfo { VK_STRUCTURE_TYPE_SUBMIT_INFO };

            if (submitInfo.swapChainSubmit.swapChain != nullptr) {
                nativeSubmitInfo.waitSemaphoreCount = 1;
                nativeSubmitInfo.pWaitSemaphores = &submitInfo.swapChainSubmit.waitSemaphone;
                nativeSubmitInfo.pWaitDstStageMask = waitStages;

                nativeSubmitInfo.signalSemaphoreCount = 1;
                nativeSubmitInfo.pSignalSemaphores = &submitInfo.swapChainSubmit.signalSemaphone;

                swapChains_.push_back(submitInfo.swapChainSubmit.swapChain);
                swapChainSemaphores_.push_back(submitInfo.swapChainSubmit.signalSemaphone);
                imageIndices_.push_back(*submitInfo.swapChainSubmit.currentFrame);

                uint32_t* currentSwapChainFrame = submitInfo.swapChainSubmit.currentFrame;
                *currentSwapChainFrame = (*currentSwapChainFrame + 1) % AbstractDevice::imageCountForSwapChain;
            }

            nativeSubmitInfo.commandBufferCount = static_cast<uint32_t>(submitInfo.commandBuffers.size());
            nativeSubmitInfo.pCommandBuffers = submitInfo.commandBuffers.data();

            submitInfos.push_back(std::move(nativeSubmitInfo));
        }

        // Push waiting for swapchain as far as possible
        if (operationsInFlight_[currentOperation] != nullptr) {
            vkWaitForFences(logicalDevice_, 1, &operationsInFlight_[currentOperation], VK_TRUE, std::numeric_limits<uint64_t>::max());
        }

        operationsInFlight_[currentOperation] = inFlightFences_[currentOperationInFlight];
        vkResetFences(logicalDevice_, 1, &inFlightFences_[currentOperationInFlight]);

        COFFEE_THROW_IF(
            vkQueueSubmit(
                graphicsQueue_, 
                static_cast<uint32_t>(submitInfos.size()), 
                submitInfos.data(), 
                inFlightFences_[currentOperationInFlight]) != VK_SUCCESS,
            "Failed to submit draw command buffers!");

        VkPresentInfoKHR presentInfo { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };

        presentInfo.waitSemaphoreCount = static_cast<uint32_t>(swapChainSemaphores_.size());
        presentInfo.pWaitSemaphores = swapChainSemaphores_.data();

        presentInfo.swapchainCount = static_cast<uint32_t>(swapChains_.size());
        presentInfo.pSwapchains = swapChains_.data();
        presentInfo.pImageIndices = imageIndices_.data();

        VkResult result = vkQueuePresentKHR(presentQueue_, &presentInfo);

        COFFEE_THROW_IF(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR, "Failed to present images!");

        currentOperationInFlight = (currentOperationInFlight + 1) % AbstractDevice::maxOperationsInFlight;
        currentOperation = (currentOperation + 1) % AbstractDevice::imageCountForSwapChain;

        pendingSubmits_.clear();
        swapChains_.clear();
        swapChainSemaphores_.clear();
        imageIndices_.clear();
    }

    VkInstance VulkanDevice::getInstance() const noexcept {
        return instance_;
    }

    VkPhysicalDevice VulkanDevice::getPhysicalDevice() const noexcept {
        return physicalDevice_;
    }

    VkDevice VulkanDevice::getLogicalDevice() const noexcept {
        return logicalDevice_;
    }

    VkQueue VulkanDevice::getGraphicsQueue() const noexcept {
        return graphicsQueue_;
    }

    VkQueue VulkanDevice::getPresentQueue() const noexcept {
        return presentQueue_;
    }

    const std::array<VkFence, AbstractDevice::maxOperationsInFlight>& VulkanDevice::getQueueFences() const noexcept {
        return inFlightFences_;
    }

    VkDescriptorPool VulkanDevice::getDescriptorPool() const noexcept {
        return descriptorPool_;
    }

    const VkPhysicalDeviceProperties& VulkanDevice::getProperties() const noexcept {
        return properties_;
    }

    const VkSurfaceFormatKHR& VulkanDevice::getSurfaceFormat() const noexcept {
        return surfaceFormat_;
    }

    void VulkanDevice::createInstance() {
        VkApplicationInfo appInfo { VK_STRUCTURE_TYPE_APPLICATION_INFO };
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
        createInfo.pApplicationInfo = &appInfo;

        std::vector<const char*> requiredExtensions = []() -> std::vector<const char*> {
            uint32_t glfwExtensionCount = 0;
            const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

            COFFEE_THROW_IF(glfwExtensions == nullptr, "Vulkan doesn't supported on this machine!");
            std::vector<const char*> extensions { glfwExtensions, glfwExtensions + glfwExtensionCount };

#ifdef COFFEE_DEBUG
            // Debug extensions
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

            return extensions;
        }();

        std::vector<const char*> validationLayers = []() -> std::vector<const char*> {
#ifdef COFFEE_DEBUG
            return { "VK_LAYER_KHRONOS_validation" };
#else
            return {};
#endif
        }();

        createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();

        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        for ([[maybe_unused]] const char* extension : requiredExtensions) {
            COFFEE_INFO("Enabled extension: {}", extension);
        }

        for ([[maybe_unused]] const auto& validationLayer : validationLayers) {
            COFFEE_INFO("Enabled validation layer: {}", validationLayer);
        }

        COFFEE_THROW_IF(vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS, "Failed to create instance!");
    }

    void VulkanDevice::createDebugMessenger() {
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

        auto func =
            reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));

        if (func == nullptr) {
            COFFEE_WARNING("Failed to get vkCreateDebugUtilsMessengerEXT! Validation logging will be unavailable!");
            return;
        }

        static_cast<void>(func(instance_, &createInfo, nullptr, &debugMessenger_));
#endif
    }

    void VulkanDevice::pickPhysicalDevice(VkSurfaceKHR surface) {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
        COFFEE_THROW_IF(deviceCount == 0, "Failed to find GPU with Vulkan support!");

        std::vector<VkPhysicalDevice> devices { deviceCount };
        vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

        for (const VkPhysicalDevice& device : devices) {
            if (isDeviceSuitable(device, surface)) {
                physicalDevice_ = device;
                break;
            }
        }

        COFFEE_THROW_IF(physicalDevice_ == nullptr, "Failed to find suitable GPU for Vulkan!");
        vkGetPhysicalDeviceProperties(physicalDevice_, &properties_);

        COFFEE_INFO("Selected physical device: {}", properties_.deviceName);

        // Surface is still alive at this point, so we can use this for our thingies
        imageCountForSwapChain = VkUtils::getOptimalAmountOfFramebuffers(physicalDevice_, surface);
        surfaceFormat_ = VkUtils::chooseSurfaceFormat(VkUtils::querySwapChainSupport(physicalDevice_, surface).formats);
    }

    void VulkanDevice::createLogicalDevice(VkSurfaceKHR surface) {
        float queuePriority = 1.0f;

        indices_ = VkUtils::findQueueFamilies(physicalDevice_, surface);
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos {};
        std::set<uint32_t> uniqueQueueFamilies { indices_.graphicsFamily.value(), indices_.presentFamily.value() };

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

        const std::vector<const char*> extensions = []() -> std::vector<const char*> {
            std::vector<const char*> exts {};

            // Vulkan extensions
            exts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

            return exts;
        }();

        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        COFFEE_THROW_IF(
            vkCreateDevice(physicalDevice_, &createInfo, nullptr, &logicalDevice_) != VK_SUCCESS, "Failed to create logical device!");

        vkGetDeviceQueue(logicalDevice_, indices_.graphicsFamily.value(), 0, &graphicsQueue_);
        vkGetDeviceQueue(logicalDevice_, indices_.presentFamily.value(), 0, &presentQueue_);
    }

    void VulkanDevice::createSyncObjects() {
        operationsInFlight_.resize(AbstractDevice::imageCountForSwapChain);

        VkFenceCreateInfo fenceCreateInfo { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < AbstractDevice::maxOperationsInFlight; i++) {
            COFFEE_THROW_IF(
                vkCreateFence(logicalDevice_, &fenceCreateInfo, nullptr, &inFlightFences_[i]) != VK_SUCCESS, "Failed to create queue fence!");
        }
    }

    void VulkanDevice::createCommandPool() {
        VkCommandPoolCreateInfo poolInfo { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        poolInfo.queueFamilyIndex = indices_.graphicsFamily.value();
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        COFFEE_THROW_IF(
            vkCreateCommandPool(logicalDevice_, &poolInfo, nullptr, &deviceCommandPool_) != VK_SUCCESS, "Failed to create command pool!");
    }

    void VulkanDevice::createDescriptorPool() {
        constexpr std::array<VkDescriptorPoolSize, 11> descriptorSizes = {
            VkDescriptorPoolSize {VK_DESCRIPTOR_TYPE_SAMPLER,                 16U},
            VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64U},
            VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          64U},
            VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          64U},
            VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   64U},
            VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   64U},
            VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         64U},
            VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         64U},
            VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 8U },
            VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 8U },
            VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       8U }
        };

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

        COFFEE_THROW_IF(
            vkCreateDescriptorPool(logicalDevice_, &descriptorPoolInfo, nullptr, &descriptorPool_) != VK_SUCCESS, 
            "Failed to create descriptor pool!");
    }

    VkCommandBuffer VulkanDevice::beginSingleTimeCommands() {
        VkCommandBufferAllocateInfo allocInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = deviceCommandPool_;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(logicalDevice_, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        return commandBuffer;
    }

    void VulkanDevice::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(graphicsQueue_, 1, &submitInfo, nullptr);
        vkQueueWaitIdle(graphicsQueue_);

        vkFreeCommandBuffers(logicalDevice_, deviceCommandPool_, 1, &commandBuffer);
    }

} // namespace coffee