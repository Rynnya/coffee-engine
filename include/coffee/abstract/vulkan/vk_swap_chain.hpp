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

        VulkanSwapChain(VulkanDevice& device, VkSurfaceKHR surface, VkExtent2D extent, std::optional<PresentMode> preferablePresentMode = std::nullopt);
        ~VulkanSwapChain() noexcept;

        bool acquireNextImage() override;
        bool submitCommandBuffers(const std::vector<CommandBuffer>& commandBuffers) override;

        void recreate(uint32_t width, uint32_t height, PresentMode mode) override;
        void waitIdle() override;

    private:
        void checkSupportedPresentModes() noexcept;
        void createSwapChain(VkExtent2D extent, std::optional<PresentMode> preferablePresentMode, VkSwapchainKHR oldSwapchain = nullptr);
        void createSyncObjects();

        VulkanDevice& device_;
        VkSurfaceKHR surface_;
        std::vector<std::vector<std::pair<VkCommandPool, VkCommandBuffer>>> poolsAndBuffers_ {};
        VkSwapchainKHR handle_ = nullptr;

        bool mailboxSupported_ = false;
        bool immediateSupported_ = false;
        std::array<VkSemaphore, maxFramesInFlight> imageAvailableSemaphores_ {};
        std::array<VkSemaphore, maxFramesInFlight> renderFinishedSemaphores_ {};
        std::array<VkFence, maxFramesInFlight> inFlightFences_ {};
        std::vector<VkFence> imagesInFlight_ {};
    };

}

#endif