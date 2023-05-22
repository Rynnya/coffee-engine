#include <coffee/graphics/swap_chain.hpp>

#include <coffee/utils/exceptions.hpp>
#include <coffee/utils/log.hpp>
#include <coffee/utils/vk_utils.hpp>

#include <array>
#include <stdexcept>

namespace coffee {

    SwapChain::SwapChain(const GPUDevicePtr& device, VkSurfaceKHR surface, VkExtent2D extent, VkPresentModeKHR preferablePresentMode)
        : device_ { device }
        , surface_ { surface }
    {
        checkSupportedPresentModes();

        createSwapChain(extent, preferablePresentMode);
        createSyncObjects();
    }

    SwapChain::~SwapChain() noexcept
    {
        images_.clear();

        vkDestroySwapchainKHR(device_->logicalDevice(), handle_, nullptr);
        handle_ = nullptr;

        for (size_t i = 0; i < GPUDevice::maxOperationsInFlight; i++) {
            vkDestroySemaphore(device_->logicalDevice(), renderFinishedSemaphores_[i], nullptr);
            vkDestroySemaphore(device_->logicalDevice(), imageAvailableSemaphores_[i], nullptr);
        }
    }

    bool SwapChain::acquireNextImage()
    {
        // This waits for imageAvailableSemaphores_ if previous operation in flight isn't done already
        device_->waitForAcquire();

        VkResult result = vkAcquireNextImageKHR(
            device_->logicalDevice(),
            handle_,
            std::numeric_limits<uint64_t>::max(),
            imageAvailableSemaphores_[device_->currentOperationInFlight()],
            VK_NULL_HANDLE,
            &currentFrame_
        );

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            return false;
        }

        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            COFFEE_ERROR("Failed to acquire swap chain image!");

            throw RegularVulkanException { result };
        }

        return true;
    }

    void SwapChain::submitCommandBuffers(std::vector<CommandBuffer>&& commandBuffers)
    {
        device_->sendCommandBuffers(
            std::move(commandBuffers),
            handle_,
            imageAvailableSemaphores_[device_->currentOperationInFlight()],
            renderFinishedSemaphores_[device_->currentOperationInFlight()],
            &currentFrame_
        );
    }

    void SwapChain::recreate(uint32_t width, uint32_t height, VkPresentModeKHR mode)
    {
        images_.clear();

        VkSwapchainKHR oldSwapChain = handle_;
        createSwapChain({ width, height }, mode, oldSwapChain);

        vkDestroySwapchainKHR(device_->logicalDevice(), oldSwapChain, nullptr);
    }

    void SwapChain::waitIdle() { device_->waitForRelease(); }

    void SwapChain::checkSupportedPresentModes() noexcept
    {
        VkUtils::SwapChainSupportDetails swapChainSupport = VkUtils::querySwapChainSupport(device_->physicalDevice(), surface_);

        relaxedFIFOSupported_ =
            VkUtils::choosePresentMode(swapChainSupport.presentModes, VK_PRESENT_MODE_FIFO_RELAXED_KHR) == VK_PRESENT_MODE_FIFO_RELAXED_KHR;
        mailboxSupported_ =
            VkUtils::choosePresentMode(swapChainSupport.presentModes, VK_PRESENT_MODE_MAILBOX_KHR) == VK_PRESENT_MODE_MAILBOX_KHR;
        immediateSupported_ =
            VkUtils::choosePresentMode(swapChainSupport.presentModes, VK_PRESENT_MODE_IMMEDIATE_KHR) == VK_PRESENT_MODE_IMMEDIATE_KHR;
    }

    void SwapChain::createSwapChain(VkExtent2D extent, VkPresentModeKHR preferablePresentMode, VkSwapchainKHR oldSwapchain)
    {
        currentPresentMode_ = VK_PRESENT_MODE_FIFO_KHR;

        if (relaxedFIFOSupported_) {
            currentPresentMode_ = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
        }

        if (preferablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR || preferablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            if (immediateSupported_) {
                currentPresentMode_ = VK_PRESENT_MODE_IMMEDIATE_KHR;
            }

            if (mailboxSupported_) {
                currentPresentMode_ = VK_PRESENT_MODE_MAILBOX_KHR;
            }
        }

        VkUtils::SwapChainSupportDetails swapChainSupport = VkUtils::querySwapChainSupport(device_->physicalDevice(), surface_);
        VkSurfaceFormatKHR surfaceFormat = device_->surfaceFormat();
        VkExtent2D selectedExtent = VkUtils::chooseExtent(extent, swapChainSupport.capabilities);
        uint32_t imageCount = device_->imageCount();

        VkSwapchainCreateInfoKHR createInfo { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        createInfo.surface = surface_;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = selectedExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        VkUtils::QueueFamilyIndices indices = VkUtils::findQueueFamilies(device_->physicalDevice(), surface_);
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
        createInfo.presentMode = currentPresentMode_;
        createInfo.clipped = VK_TRUE;

        createInfo.oldSwapchain = oldSwapchain;
        VkResult result = vkCreateSwapchainKHR(device_->logicalDevice(), &createInfo, nullptr, &handle_);

        if (result != VK_SUCCESS) {
            COFFEE_ERROR("Failed to create swap chain!");

            throw RegularVulkanException { result };
        }

        vkGetSwapchainImagesKHR(device_->logicalDevice(), handle_, &imageCount, nullptr);

        std::vector<VkImage> swapChainImages {};
        swapChainImages.resize(imageCount);
        images_.reserve(imageCount);

        vkGetSwapchainImagesKHR(device_->logicalDevice(), handle_, &imageCount, swapChainImages.data());

        for (auto& image : swapChainImages) {
            images_.emplace_back(new Image { device_, surfaceFormat.format, image, selectedExtent.width, selectedExtent.height });
        }
    }

    void SwapChain::createSyncObjects()
    {
        VkSemaphoreCreateInfo semaphoreInfo { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VkDevice device = device_->logicalDevice();
        VkResult result = VK_SUCCESS;

        for (size_t i = 0; i < GPUDevice::maxOperationsInFlight; i++) {
            if ((result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores_[i])) != VK_SUCCESS) {
                COFFEE_ERROR("Failed to create semaphore for notifying available images!");

                throw RegularVulkanException { result };
            }

            if ((result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i])) != VK_SUCCESS) {
                COFFEE_ERROR("Failed to create semaphore for waiting on render!");

                throw RegularVulkanException { result };
            }
        }
    }

} // namespace coffee
