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
        VulkanSwapChain(VulkanDevice& device, VkSurfaceKHR surface, VkExtent2D extent, PresentMode preferablePresentMode);
        ~VulkanSwapChain() noexcept;

        bool acquireNextImage() override;
        void submitCommandBuffers(std::vector<GraphicsCommandBuffer>&& commandBuffers) override;

        void recreate(uint32_t width, uint32_t height, PresentMode mode) override;
        void waitIdle() override;

    private:
        void checkSupportedPresentModes() noexcept;
        void createSwapChain(VkExtent2D extent, PresentMode preferablePresentMode, VkSwapchainKHR oldSwapchain = nullptr);
        void createSyncObjects();

        VulkanDevice& device_;
        VkSurfaceKHR surface_;
        VkSwapchainKHR handle_ = nullptr;
        std::vector<std::vector<std::pair<VkCommandPool, VkCommandBuffer>>> poolsAndBuffers_ {};

        std::array<VkSemaphore, AbstractDevice::maxOperationsInFlight> imageAvailableSemaphores_ {};
        std::array<VkSemaphore, AbstractDevice::maxOperationsInFlight> renderFinishedSemaphores_ {};

        bool mailboxSupported_ = false;
        bool immediateSupported_ = false;
    };

} // namespace coffee

#endif