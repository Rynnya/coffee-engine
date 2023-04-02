#ifndef COFFEE_ABSTRACT_FRAMEBUFFER
#define COFFEE_ABSTRACT_FRAMEBUFFER

#include <coffee/utils/non_moveable.hpp>

#include <memory>

namespace coffee {

    class AbstractFramebuffer : NonMoveable {
    public:
        AbstractFramebuffer(uint32_t width, uint32_t height) noexcept;
        virtual ~AbstractFramebuffer() noexcept = default;

        uint32_t getWidth() const noexcept;
        uint32_t getHeight() const noexcept;

    private:
        uint32_t width_;
        uint32_t height_;
    };

    using Framebuffer = std::unique_ptr<AbstractFramebuffer>;

} // namespace coffee

#endif