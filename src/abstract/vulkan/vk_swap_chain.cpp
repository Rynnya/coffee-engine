#include <coffee/abstract/vulkan/vk_swap_chain.hpp>

#include <coffee/abstract/vulkan/vk_command_buffer.hpp>
#include <coffee/utils/log.hpp>
#include <coffee/utils/vk_utils.hpp>

#include <array>
#include <stdexcept>

namespace coffee {

    VulkanSwapChain::VulkanSwapChain(VulkanDevice& device, VkSurfaceKHR surface, VkExtent2D extent, PresentMode preferablePresentMode)
            : device_ { device }
            , surface_ { surface } {
        checkSupportedPresentModes();

        createSwapChain(extent, preferablePresentMode);
        createSyncObjects();
    }

    VulkanSwapChain::~VulkanSwapChain() noexcept {
        for (const auto& framePoolsAndBuffers : poolsAndBuffers_) {
            for (auto& [pool, commandBuffer] : framePoolsAndBuffers) {
                vkFreeCommandBuffers(device_.getLogicalDevice(), pool, 1U, &commandBuffer);
                device_.returnCommandPool(pool);
            }
        }

        images.clear();

        vkDestroySwapchainKHR(device_.getLogicalDevice(), handle_, nullptr);
        handle_ = nullptr;

        for (size_t i = 0; i < AbstractDevice::maxOperationsInFlight; i++) {
            vkDestroySemaphore(device_.getLogicalDevice(), renderFinishedSemaphores_[i], nullptr);
            vkDestroySemaphore(device_.getLogicalDevice(), imageAvailableSemaphores_[i], nullptr);
        }
    }

    bool VulkanSwapChain::acquireNextImage() {
        device_.waitForAcquire();

        VkResult result = vkAcquireNextImageKHR(
            device_.getLogicalDevice(),
            handle_,
            std::numeric_limits<uint64_t>::max(),
            imageAvailableSemaphores_[device_.getCurrentOperationInFlight()],
            nullptr,
            &currentFrame
        );

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            return false;
        }

        COFFEE_THROW_IF(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR, "Failed to acquire swap chain image!");

        for (auto& [pool, commandBuffer] : poolsAndBuffers_[currentFrame]) {
            vkFreeCommandBuffers(device_.getLogicalDevice(), pool, 1U, &commandBuffer);
            device_.returnCommandPool(pool);
        }

        poolsAndBuffers_[currentFrame].clear();

        return true;
    }

    void VulkanSwapChain::submitCommandBuffers(std::vector<GraphicsCommandBuffer>&& commandBuffers) {
        std::vector<VkCommandBuffer> commandBuffersToUpload {};

        for (auto& commandBuffer : commandBuffers) {
            if (commandBuffer == nullptr) {
                continue;
            }

            VulkanGraphicsCB* commandBufferImpl = static_cast<VulkanGraphicsCB*>(commandBuffer.get());

            if (commandBufferImpl->renderPassActive) {
                commandBufferImpl->endRenderPass();
            }

            COFFEE_THROW_IF(
                vkEndCommandBuffer(commandBufferImpl->commandBuffer) != VK_SUCCESS, "Failed to end command buffer recording!");

            commandBuffersToUpload.push_back(commandBufferImpl->commandBuffer);
            poolsAndBuffers_[currentFrame].push_back({ commandBufferImpl->pool, commandBufferImpl->commandBuffer });

            // Take ownership of pool
            commandBufferImpl->pool = nullptr;
        }

        SwapChainSubmit submitInfo {};
        submitInfo.waitSemaphone = imageAvailableSemaphores_[device_.getCurrentOperationInFlight()];
        submitInfo.signalSemaphone = renderFinishedSemaphores_[device_.getCurrentOperationInFlight()];
        submitInfo.currentFrame = &currentFrame;
        submitInfo.swapChain = handle_;
        device_.sendCommandBuffers(submitInfo, commandBuffersToUpload);
    }

    void VulkanSwapChain::recreate(uint32_t width, uint32_t height, PresentMode mode) {
        images.clear();

        VkSwapchainKHR oldSwapChain = handle_;
        createSwapChain({ width, height }, mode, oldSwapChain);

        vkDestroySwapchainKHR(device_.getLogicalDevice(), oldSwapChain, nullptr);
    }

    void VulkanSwapChain::waitIdle() {
        const auto& queueFence = device_.getQueueFences();
        vkWaitForFences(device_.getLogicalDevice(), queueFence.size(), queueFence.data(), VK_TRUE, std::numeric_limits<uint64_t>::max());
    }

    void VulkanSwapChain::checkSupportedPresentModes() noexcept {
        VkUtils::SwapChainSupportDetails swapChainSupport = VkUtils::querySwapChainSupport(device_.getPhysicalDevice(), surface_);

        mailboxSupported_ =
            (VkUtils::choosePresentMode(swapChainSupport.presentModes, VK_PRESENT_MODE_MAILBOX_KHR) == VK_PRESENT_MODE_MAILBOX_KHR);
        immediateSupported_ =
            (VkUtils::choosePresentMode(swapChainSupport.presentModes, VK_PRESENT_MODE_IMMEDIATE_KHR) == VK_PRESENT_MODE_IMMEDIATE_KHR);
    }

    void VulkanSwapChain::createSwapChain(VkExtent2D extent, PresentMode preferablePresentMode, VkSwapchainKHR oldSwapchain) {
        VkPresentModeKHR preferableVSync = VK_PRESENT_MODE_FIFO_KHR;

        if (preferablePresentMode == PresentMode::Immediate) {
            if (immediateSupported_) {
                preferableVSync = VK_PRESENT_MODE_IMMEDIATE_KHR;
            }

            if (mailboxSupported_) {
                preferableVSync = VK_PRESENT_MODE_MAILBOX_KHR;
            }
        }

        VkUtils::SwapChainSupportDetails swapChainSupport = VkUtils::querySwapChainSupport(device_.getPhysicalDevice(), surface_);
        VkSurfaceFormatKHR surfaceFormat = device_.getSurfaceFormat();
        VkPresentModeKHR nativePresentMode = VkUtils::choosePresentMode(swapChainSupport.presentModes, preferableVSync);
        VkExtent2D selectedExtent = VkUtils::chooseExtent(extent, swapChainSupport.capabilities);
        uint32_t imageCount = device_.getSwapChainImageCount();

        VkSwapchainCreateInfoKHR createInfo { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        createInfo.surface = surface_;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = selectedExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        VkUtils::QueueFamilyIndices indices = VkUtils::findQueueFamilies(device_.getPhysicalDevice(), surface_);
        uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

        if (indices.graphicsFamily == indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }
        else {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = nativePresentMode;
        createInfo.clipped = VK_TRUE;

        createInfo.oldSwapchain = oldSwapchain;

        COFFEE_THROW_IF(
            vkCreateSwapchainKHR(device_.getLogicalDevice(), &createInfo, nullptr, &handle_) != VK_SUCCESS, "Failed to create swap chain!");

        vkGetSwapchainImagesKHR(device_.getLogicalDevice(), handle_, &imageCount, nullptr);

        std::vector<VkImage> swapChainImages {};
        swapChainImages.resize(imageCount);
        poolsAndBuffers_.resize(imageCount);
        images.reserve(imageCount);

        vkGetSwapchainImagesKHR(device_.getLogicalDevice(), handle_, &imageCount, swapChainImages.data());

        for (size_t i = 0; i < swapChainImages.size(); i++) {
            images.emplace_back(std::make_shared<VulkanImage>(
                device_,
                selectedExtent.width,
                selectedExtent.height,
                surfaceFormat.format,
                swapChainImages[i]
            ));
        }

        presentMode = nativePresentMode == VK_PRESENT_MODE_FIFO_KHR ? PresentMode::FIFO : PresentMode::Immediate;
        imageFormat = VkUtils::transformFormat(surfaceFormat.format);
        depthFormat = VkUtils::transformFormat(VkUtils::findDepthFormat(device_.getPhysicalDevice()));
    }

    void VulkanSwapChain::createSyncObjects() {
        VkSemaphoreCreateInfo semaphoreInfo { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

        for (size_t i = 0; i < AbstractDevice::maxOperationsInFlight; i++) {
            COFFEE_THROW_IF(
                vkCreateSemaphore(device_.getLogicalDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores_[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device_.getLogicalDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]) != VK_SUCCESS,
                "Failed to create synchronization objects for a frame!");
        }
    }

} // namespace coffee
