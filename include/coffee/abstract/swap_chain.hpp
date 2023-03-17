#ifndef COFFEE_ABSTRACT_SWAP_CHAIN
#define COFFEE_ABSTRACT_SWAP_CHAIN

#include <coffee/abstract/command_buffer.hpp>
#include <coffee/utils/non_moveable.hpp>
#include <coffee/types.hpp>

namespace coffee {

    class AbstractSwapChain : NonMoveable {
    public:
        AbstractSwapChain() noexcept = default;
        virtual ~AbstractSwapChain() noexcept = default;

        PresentMode getPresentMode() const noexcept;
        Format getColorFormat() const noexcept;
        Format getDepthFormat() const noexcept;

        size_t getImagesSize() const noexcept;
        const std::vector<Image>& getPresentImages() const noexcept;

        uint32_t getImageIndex() const noexcept;
        // Implementation must return true if swapchain images is OK, otherwise, if they must be resized, it must return false
        // On error implementation must throw an exception
        virtual bool acquireNextImage() = 0;
        // Implementation must return true if swapchain images is OK, otherwise, if they must be resized, it must return false
        // On error implementation must throw an exception
        virtual bool submitCommandBuffers(const std::vector<CommandBuffer>& commandBuffers) = 0;

        virtual void recreate(uint32_t width, uint32_t height, PresentMode mode) = 0;
        virtual void waitIdle() = 0;

    protected:
        PresentMode presentMode = PresentMode::FIFO;
        Format imageFormat = Format::Undefined;
        Format depthFormat = Format::Undefined;
        std::vector<Image> images {};
        Extent2D currentExtent { 0U, 0U };
        uint32_t currentFrame = 0;
        uint32_t currentFrameInFlight = 0;
    };

}

#endif