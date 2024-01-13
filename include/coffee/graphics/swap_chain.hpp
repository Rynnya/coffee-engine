#ifndef COFFEE_GRAPHICS_SWAP_CHAIN
#define COFFEE_GRAPHICS_SWAP_CHAIN

#include <coffee/graphics/command_buffer.hpp>
#include <coffee/graphics/framebuffer.hpp>
#include <coffee/graphics/image.hpp>
#include <coffee/utils/non_moveable.hpp>

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace coffee { namespace graphics {

    class SwapChain {
    public:
        SwapChain(const DevicePtr& device, VkSurfaceKHR surface, VkExtent2D extent, VkPresentModeKHR preferablePresentMode);
        ~SwapChain() noexcept;

        bool acquireNextImage();
        void submit(std::vector<CommandBuffer>&& commandBuffers);

        void recreate(const VkExtent2D& extent, VkPresentModeKHR mode);

        inline uint32_t getPresentIndex() const noexcept { return presentIndex_; }

        inline const std::vector<ImagePtr>& getPresentImages() const noexcept { return images_; }

        inline VkPresentModeKHR getPresentMode() const noexcept { return currentPresentMode_; }

    private:
        void checkSupportedPresentModes() noexcept;
        void createSwapChain(VkExtent2D extent, VkPresentModeKHR preferablePresentMode, VkSwapchainKHR oldSwapchain = nullptr);
        void createSyncObjects();

        void waitForRelease();

        DevicePtr device_;

        VkSurfaceKHR surface_ = VK_NULL_HANDLE;
        VkSwapchainKHR handle_ = VK_NULL_HANDLE;

        uint32_t presentIndex_ = 0U;

        std::vector<ImagePtr> images_ {};
        std::array<FencePtr, Device::kMaxOperationsInFlight> fencesInFlight_ {};
        std::array<VkSemaphore, Device::kMaxOperationsInFlight> imageAvailableSemaphores_ {};
        std::array<VkSemaphore, Device::kMaxOperationsInFlight> renderFinishedSemaphores_ {};

        VkPresentModeKHR currentPresentMode_ = VK_PRESENT_MODE_FIFO_KHR;
        bool relaxedFIFOSupported_ = false;
        bool mailboxSupported_ = false;
        bool immediateSupported_ = false;
    };

}} // namespace coffee::graphics

#endif