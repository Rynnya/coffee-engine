#ifndef COFFEE_VK_SWAP_CHAIN
#define COFFEE_VK_SWAP_CHAIN

#include <coffee/abstract/swap_chain.hpp>
#include <coffee/abstract/vulkan/vk_device.hpp>
#include <coffee/abstract/vulkan/vk_framebuffer.hpp>
#include <coffee/abstract/vulkan/vk_image.hpp>
#include <coffee/utils/non_moveable.hpp>

#include <vulkan/vulkan.h>

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace coffee {

    class VulkanSwapChain : public AbstractSwapChain {
    public:
        static constexpr size_t maxFramesInFlight = 2;

        VulkanSwapChain(
            VulkanDevice& device, 
            VkExtent2D extent,
            std::optional<VkPresentModeKHR> preferableVerticalSynchronization
        );
        VulkanSwapChain(
            VulkanDevice& device, 
            VkExtent2D extent,
            std::optional<VkPresentModeKHR> preferableVerticalSynchronization,
            std::unique_ptr<VulkanSwapChain>& oldSwapChain
        );
        ~VulkanSwapChain();

        bool operator==(const VulkanSwapChain& other) const noexcept;
        bool operator!=(const VulkanSwapChain& other) const noexcept;

        VkPresentModeKHR getVSyncMode() const noexcept;

        Format getImageFormat() const noexcept;
        Format getDepthFormat() const noexcept;

        size_t getImagesSize() const noexcept;
        const std::vector<Image>& getPresentImages() const noexcept;

        VkResult acquireNextImage(uint32_t& imageIndex);
        VkResult submitCommandBuffers(const std::vector<VkCommandBuffer>& commandBuffers, uint32_t imageIndex);

    private:
        void initialize(
            VkExtent2D extent, std::optional<VkPresentModeKHR> preferableVerticalSynchronization, VkSwapchainKHR oldSwapchain);
        void createSwapChain(
            VkExtent2D extent, std::optional<VkPresentModeKHR> preferableVerticalSynchronization, VkSwapchainKHR oldSwapchain);
        void createSyncObjects();

        // Implementation
        VulkanDevice& device_;
        VkSwapchainKHR handle_ = nullptr;

        // Images
        Format imageFormat_ = Format::Undefined;
        Format depthFormat_ = Format::Undefined;
        std::vector<Image> images_ {};
        Extent2D extent_ { 0U, 0U };
        
        // Synchronization
        std::optional<VkPresentModeKHR> verticalSynchronization_ {};
        std::array<VkSemaphore, maxFramesInFlight> imageAvailableSemaphores_ {};
        std::array<VkSemaphore, maxFramesInFlight> renderFinishedSemaphores_ {};
        std::array<VkFence, maxFramesInFlight> inFlightFences_ {};
        std::vector<VkFence> imagesInFlight_ {};
        uint32_t currentFrame_ = 0;

    };

}

#endif