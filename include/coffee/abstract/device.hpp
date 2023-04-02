#ifndef COFFEE_ABSTRACT_DEVICE
#define COFFEE_ABSTRACT_DEVICE

#include <coffee/abstract/buffer.hpp>
#include <coffee/abstract/image.hpp>
#include <coffee/utils/non_moveable.hpp>

namespace coffee {

    class AbstractDevice : NonMoveable {
    public:
        // This is shared among ALL swap chains (= windows) so we can synchronize them without too
        // much effort. Implementation can ignore this value if they didn't use
        // double-triple-(you-get-it) buffering
        static constexpr size_t maxOperationsInFlight = 2;

        AbstractDevice() noexcept = default;
        virtual ~AbstractDevice() noexcept = default;

        uint32_t getSwapChainImageCount() const noexcept;
        uint32_t getCurrentOperation() const noexcept;
        uint32_t getCurrentOperationInFlight() const noexcept;

        // This must be called before swap chain acquire function, otherwise you might get
        // synchronization errors
        virtual void waitForAcquire() = 0;

    protected:
        // Implementation must set this value to desirable amount of images in swap chains
        // This value MUST NOT change after complete creation of device
        uint32_t imageCountForSwapChain = 0;
        // Implementation can ignore this values if they didn't use double-triple-(you-get-it)
        // buffering
        uint32_t currentOperation = 0;         // Must be in range of [0, imageCountForSwapChain]
        uint32_t currentOperationInFlight = 0; // Must be in range of [0, maxOperationsInFlight]
    };

} // namespace coffee

#endif