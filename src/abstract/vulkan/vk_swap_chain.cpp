#include <coffee/abstract/vulkan/vk_swap_chain.hpp>

#include <coffee/utils/log.hpp>
#include <coffee/utils/vk_utils.hpp>

#include <array>
#include <stdexcept>

namespace coffee {

    VulkanSwapChain::VulkanSwapChain(
        VulkanDevice& device, 
        VkExtent2D extent,
        std::optional<VkPresentModeKHR> preferableVerticalSynchronization
    ) : device_ { device } {
        initialize(extent, preferableVerticalSynchronization, nullptr);
    }

    VulkanSwapChain::VulkanSwapChain(
        VulkanDevice& device,
        VkExtent2D extent,
        std::optional<VkPresentModeKHR> preferableVerticalSynchronization,
        std::unique_ptr<VulkanSwapChain>& oldSwapChain
    ) : device_ { device } {
        verticalSynchronization_ = std::move(oldSwapChain->verticalSynchronization_);
        initialize(extent, preferableVerticalSynchronization, oldSwapChain->handle_);
    }

    VulkanSwapChain::~VulkanSwapChain() {
        images_.clear();

        vkDestroySwapchainKHR(device_.getLogicalDevice(), handle_, nullptr);
        handle_ = nullptr;

        for (size_t i = 0; i < maxFramesInFlight; i++) {
            vkDestroySemaphore(device_.getLogicalDevice(), renderFinishedSemaphores_[i], nullptr);
            vkDestroySemaphore(device_.getLogicalDevice(), imageAvailableSemaphores_[i], nullptr);
            vkDestroyFence(device_.getLogicalDevice(), inFlightFences_[i], nullptr);
        }
    }

    bool VulkanSwapChain::operator==(const VulkanSwapChain& other) const noexcept {
        return static_cast<uint32_t>(imageFormat_) == static_cast<uint32_t>(other.imageFormat_);
    }

    bool VulkanSwapChain::operator!=(const VulkanSwapChain& other) const noexcept {
        return !(*this == other);
    }

    VkPresentModeKHR VulkanSwapChain::getVSyncMode() const noexcept {
        return verticalSynchronization_.value();
    }

    Format VulkanSwapChain::getImageFormat() const noexcept {
        return imageFormat_;
    }

    Format VulkanSwapChain::getDepthFormat() const noexcept {
        return depthFormat_;
    }

    size_t VulkanSwapChain::getImagesSize() const noexcept {
        return images_.size();
    }

    const std::vector<Image>& VulkanSwapChain::getPresentImages() const noexcept {
        return images_;
    }

    VkResult VulkanSwapChain::acquireNextImage(uint32_t& imageIndex) {
        vkWaitForFences(device_.getLogicalDevice(), 1, &inFlightFences_[currentFrame_], VK_TRUE, std::numeric_limits<uint64_t>::max());
        return vkAcquireNextImageKHR(device_.getLogicalDevice(), handle_, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphores_[currentFrame_], nullptr, &imageIndex);
    }

    VkResult VulkanSwapChain::submitCommandBuffers(const std::vector<VkCommandBuffer>& commandBuffers, uint32_t imageIndex) {
        if (imagesInFlight_[imageIndex] != nullptr) {
            vkWaitForFences(device_.getLogicalDevice(), 1, &imagesInFlight_[imageIndex], VK_TRUE, std::numeric_limits<uint64_t>::max());
        }

        imagesInFlight_[imageIndex] = inFlightFences_[currentFrame_];
        VkSubmitInfo submitInfo { VK_STRUCTURE_TYPE_SUBMIT_INFO };

        VkSemaphore waitSemaphores[] = { imageAvailableSemaphores_[currentFrame_] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
        submitInfo.pCommandBuffers = commandBuffers.data();

        VkSemaphore signalSemaphores[] = { renderFinishedSemaphores_[currentFrame_] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        vkResetFences(device_.getLogicalDevice(), 1, &inFlightFences_[currentFrame_]);
        COFFEE_THROW_IF(
            vkQueueSubmit(device_.getGraphicsQueue(), 1, &submitInfo, inFlightFences_[currentFrame_]) != VK_SUCCESS,
            "Failed to submit draw command buffers!");

        VkPresentInfoKHR presentInfo { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        presentInfo.pImageIndices = &imageIndex;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &handle_;

        currentFrame_ = (currentFrame_ + 1) % maxFramesInFlight;
        return vkQueuePresentKHR(device_.getPresentQueue(), &presentInfo);
    }

    void VulkanSwapChain::initialize(
        VkExtent2D extent,
        std::optional<VkPresentModeKHR> preferableVerticalSynchronization,
        VkSwapchainKHR oldSwapchain
    ) {
        createSwapChain(extent, preferableVerticalSynchronization, oldSwapchain);
        createSyncObjects();
    }

    void VulkanSwapChain::createSwapChain(
        VkExtent2D extent,
        std::optional<VkPresentModeKHR> preferableVerticalSynchronization, 
        VkSwapchainKHR oldSwapchain
    ) {
        std::optional<VkPresentModeKHR> preferablePresentMode {};

        if (verticalSynchronization_.has_value()) {
            preferablePresentMode = verticalSynchronization_;
        }

        if (preferableVerticalSynchronization.has_value()) {
            preferablePresentMode = preferableVerticalSynchronization;
        }

        VkUtils::SwapChainSupportDetails swapChainSupport = VkUtils::querySwapChainSupport(device_.getPhysicalDevice(), device_.getSurface());
        VkSurfaceFormatKHR surfaceFormat = VkUtils::chooseSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = VkUtils::choosePresentMode(swapChainSupport.presentModes, preferablePresentMode);
        VkExtent2D selectedExtent = VkUtils::chooseExtent(extent, swapChainSupport.capabilities);
        uint32_t imageCount = VkUtils::getOptionalAmountOfFramebuffers(device_.getPhysicalDevice(), device_.getSurface());

        VkSwapchainCreateInfoKHR createInfo { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        createInfo.surface = device_.getSurface();
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = selectedExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        VkUtils::QueueFamilyIndices indices = VkUtils::findQueueFamilies(device_.getPhysicalDevice(), device_.getSurface());
        uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

        // If objects were in same family, then we must set permissions to explicit, otherwise to relaxed
        // Explicit - gains maximum performance, permissions must be moved explicitly from one family to another
        // Relaxed - objects can be transfered between families without moving permissions explicitly
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
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;

        createInfo.oldSwapchain = oldSwapchain;

        COFFEE_THROW_IF(
            vkCreateSwapchainKHR(device_.getLogicalDevice(), &createInfo, nullptr, &handle_) != VK_SUCCESS, "Failed to create swap chain!");

        vkGetSwapchainImagesKHR(device_.getLogicalDevice(), handle_, &imageCount, nullptr);

        std::vector<VkImage> images {};
        images.resize(imageCount);
        images_.reserve(imageCount);

        vkGetSwapchainImagesKHR(device_.getLogicalDevice(), handle_, &imageCount, images.data());

        for (size_t i = 0; i < images.size(); i++) {
            images_.emplace_back(
                std::make_shared<VulkanImage>(device_, selectedExtent.width, selectedExtent.height, surfaceFormat.format, images[i]));
        }

        verticalSynchronization_ = std::make_optional(presentMode);
        imageFormat_ = VkUtils::transformFormat(surfaceFormat.format);
        depthFormat_ = VkUtils::transformFormat(VkUtils::findDepthFormat(device_.getPhysicalDevice()));
    }

    void VulkanSwapChain::createSyncObjects() {
        imagesInFlight_.resize(images_.size());

        VkSemaphoreCreateInfo semaphoreInfo { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VkFenceCreateInfo fenceInfo { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < maxFramesInFlight; i++) {
            COFFEE_THROW_IF(
                vkCreateSemaphore(device_.getLogicalDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores_[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device_.getLogicalDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]) != VK_SUCCESS ||
                vkCreateFence(device_.getLogicalDevice(), &fenceInfo, nullptr, &inFlightFences_[i]) != VK_SUCCESS,
                "Failed to create synchronization objects for a frame!");
        }
    }

}

