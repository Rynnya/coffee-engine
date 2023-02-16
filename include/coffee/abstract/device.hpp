#ifndef COFFEE_ABSTRACT_DEVICE
#define COFFEE_ABSTRACT_DEVICE

#include <coffee/abstract/buffer.hpp>
#include <coffee/abstract/image.hpp>
#include <coffee/utils/non_moveable.hpp>

namespace coffee {

    class AbstractDevice : NonMoveable {
    public:
        AbstractDevice() noexcept = default;
        virtual ~AbstractDevice() noexcept = default;
    };

}

#endif