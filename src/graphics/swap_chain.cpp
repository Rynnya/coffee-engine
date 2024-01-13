#include <coffee/graphics/swap_chain.hpp>

#include <coffee/graphics/exceptions.hpp>
#include <coffee/utils/log.hpp>
#include <coffee/utils/vk_utils.hpp>

#include <array>
#include <stdexcept>

namespace coffee { namespace graphics {

    SwapChain::SwapChain(const DevicePtr& device, VkSurfaceKHR surface, VkExtent2D extent, VkPresentModeKHR preferablePresentMode)
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

        waitForRelease();
        vkDestroySwapchainKHR(device_->logicalDevice(), handle_, nullptr);

        for (size_t i = 0; i < Device::kMaxOperationsInFlight; i++) {
            vkDestroySemaphore(device_->logicalDevice(), renderFinishedSemaphores_[i], nullptr);
            vkDestroySemaphore(device_->logicalDevice(), imageAvailableSemaphores_[i], nullptr);
        }
    }

    bool SwapChain::acquireNextImage()
    {
        auto& previousOperation = fencesInFlight_[device_->currentOperationInFlight()];

        previousOperation->wait();

        VkResult result = vkAcquireNextImageKHR(
            device_->logicalDevice(),
            handle_,
            std::numeric_limits<uint64_t>::max(),
            imageAvailableSemaphores_[device_->currentOperationInFlight()],
            VK_NULL_HANDLE,
            &presentIndex_
        );

        if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR) {
            return false;
        }

        if (result != VK_SUCCESS) {
            COFFEE_ERROR("Failed to acquire swap chain image!");

            throw RegularVulkanException { result };
        }

        previousOperation->reset();

        return true;
    }

    void SwapChain::submit(std::vector<CommandBuffer>&& commandBuffers)
    {
        if (commandBuffers.empty()) {
            return;
        }

        VkResult result = VK_SUCCESS;
        Device::SubmitInfo submitInfo {};

        submitInfo.submitType = CommandBufferType::Graphics;
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

        submitInfo.waitSemaphoresCount = 1U;
        submitInfo.signalSemaphoresCount = 1U;

        submitInfo.waitSemaphores = std::make_unique<VkSemaphore[]>(1U);
        submitInfo.waitSemaphores[0] = imageAvailableSemaphores_[device_->currentOperationInFlight()];
        submitInfo.waitDstStageMasks = std::make_unique<VkPipelineStageFlags[]>(1U);
        submitInfo.waitDstStageMasks[0] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        submitInfo.signalSemaphores = std::make_unique<VkSemaphore[]>(1U);
        submitInfo.signalSemaphores[0] = renderFinishedSemaphores_[device_->currentOperationInFlight()];

        submitInfo.swapChain = handle_;
        submitInfo.swapChainWaitSemaphore = renderFinishedSemaphores_[device_->currentOperationInFlight()];
        submitInfo.currentFrame = &presentIndex_;

        device_->endGraphicsSubmit(std::move(submitInfo), fencesInFlight_[device_->currentOperationInFlight()], false);
    }

    void SwapChain::recreate(const VkExtent2D& extent, VkPresentModeKHR mode)
    {
        images_.clear();

        VkSwapchainKHR oldSwapChain = handle_;
        createSwapChain(extent, mode, oldSwapChain);

        waitForRelease();
        vkDestroySwapchainKHR(device_->logicalDevice(), oldSwapChain, nullptr);
    }

    void SwapChain::checkSupportedPresentModes() noexcept
    {
        VkUtils::SwapChainSupportDetails swapChainSupport = VkUtils::querySwapChainSupport(device_->physicalDevice(), surface_);

        relaxedFIFOSupported_ =
            VkUtils::choosePresentMode(swapChainSupport.presentModes, VK_PRESENT_MODE_FIFO_RELAXED_KHR) == VK_PRESENT_MODE_FIFO_RELAXED_KHR;
        mailboxSupported_ = VkUtils::choosePresentMode(swapChainSupport.presentModes, VK_PRESENT_MODE_MAILBOX_KHR) == VK_PRESENT_MODE_MAILBOX_KHR;
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
        VkExtent2D selectedExtent = VkUtils::chooseExtent(extent, swapChainSupport.capabilities);
        uint32_t imageCount = device_->imageCount();

        VkSwapchainCreateInfoKHR createInfo { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        createInfo.surface = surface_;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = device_->surfaceFormat();
        createInfo.imageColorSpace = device_->surfaceColorSpace();
        createInfo.imageExtent = selectedExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
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
            images_.emplace_back(new Image { device_, device_->surfaceFormat(), image, selectedExtent.width, selectedExtent.height });
        }
    }

    void SwapChain::createSyncObjects()
    {
        VkSemaphoreCreateInfo semaphoreInfo { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VkDevice device = device_->logicalDevice();
        VkResult result = VK_SUCCESS;

        for (size_t i = 0; i < Device::kMaxOperationsInFlight; i++) {
            fencesInFlight_[i] = Fence::create(device_, true);

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

    void SwapChain::waitForRelease()
    {
        VkFence fences[Device::kMaxOperationsInFlight] {};

        for (size_t index = 0; index < Device::kMaxOperationsInFlight; index++) {
            fences[index] = fencesInFlight_[index]->fence();
        }

        vkWaitForFences(device_->logicalDevice(), Device::kMaxOperationsInFlight, fences, VK_TRUE, std::numeric_limits<uint64_t>::max());
    }

}} // namespace coffee::graphics
