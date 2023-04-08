#ifndef COFFEE_VK_SWAP_CHAIN
#define COFFEE_VK_SWAP_CHAIN

#include <coffee/graphics/command_buffer.hpp>
#include <coffee/graphics/framebuffer.hpp>
#include <coffee/graphics/image.hpp>
#include <coffee/utils/non_moveable.hpp>

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace coffee {

    class SwapChain {
    public:
        SwapChain(Device& device, VkSurfaceKHR surface, VkExtent2D extent, VkPresentModeKHR preferablePresentMode);
        ~SwapChain() noexcept;

        bool acquireNextImage();
        void submitCommandBuffers(std::vector<CommandBuffer>&& commandBuffers);

        void recreate(uint32_t width, uint32_t height, VkPresentModeKHR mode);
        void waitIdle();

        inline uint32_t currentFrame() const noexcept
        {
            return currentFrame_;
        }

        inline const std::vector<Image>& presentImages() const noexcept
        {
            return images_;
        }

        inline VkPresentModeKHR presentMode() const noexcept
        {
            return currentPresentMode_;
        }

    private:
        void checkSupportedPresentModes() noexcept;
        void createSwapChain(VkExtent2D extent, VkPresentModeKHR preferablePresentMode, VkSwapchainKHR oldSwapchain = nullptr);
        void createSyncObjects();

        Device& device_;

        VkSurfaceKHR surface_ = VK_NULL_HANDLE;
        VkSwapchainKHR handle_ = VK_NULL_HANDLE;

        uint32_t currentFrame_ = 0U;

        std::vector<Image> images_ {};
        std::vector<std::vector<std::pair<VkCommandPool, VkCommandBuffer>>> poolsAndBuffers_ {};

        std::array<VkSemaphore, Device::maxOperationsInFlight> imageAvailableSemaphores_ {};
        std::array<VkSemaphore, Device::maxOperationsInFlight> renderFinishedSemaphores_ {};

        VkPresentModeKHR currentPresentMode_ = VK_PRESENT_MODE_FIFO_KHR;
        bool mailboxSupported_ = false;
        bool immediateSupported_ = false;
    };

} // namespace coffee

#endif