#include <coffee/abstract/framebuffer.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    AbstractFramebuffer::AbstractFramebuffer(uint32_t width, uint32_t height) noexcept
        : width_ { width }
        , height_ { height }
    {}

    uint32_t AbstractFramebuffer::getWidth() const noexcept {
        return width_;
    }

    uint32_t AbstractFramebuffer::getHeight() const noexcept {
        return height_;
    }

}