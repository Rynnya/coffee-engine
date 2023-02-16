#ifndef COFFEE_ABSTRACT_SWAP_CHAIN
#define COFFEE_ABSTRACT_SWAP_CHAIN

#include <coffee/utils/non_moveable.hpp>

namespace coffee {

    class AbstractSwapChain : NonMoveable {
    public:
        AbstractSwapChain() noexcept = default;
        virtual ~AbstractSwapChain() noexcept = default;
    };

}

#endif