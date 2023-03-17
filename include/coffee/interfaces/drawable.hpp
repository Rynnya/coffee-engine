#ifndef COFFEE_INTERFACE_DRAWABLE
#define COFFEE_INTERFACE_DRAWABLE

#include <coffee/abstract/command_buffer.hpp>

namespace coffee {

    class Drawable {
    public:
        Drawable() noexcept = default;
        virtual ~Drawable() noexcept = default;

        virtual void draw(const CommandBuffer& commandBuffer) = 0;
    };

}

#endif