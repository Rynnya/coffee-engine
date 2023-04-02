#include <coffee/abstract/device.hpp>

namespace coffee {

    uint32_t AbstractDevice::getSwapChainImageCount() const noexcept {
        return imageCountForSwapChain;
    }

    uint32_t AbstractDevice::getCurrentOperation() const noexcept {
        return currentOperation;
    }

    uint32_t AbstractDevice::getCurrentOperationInFlight() const noexcept {
        return currentOperationInFlight;
    }

} // namespace coffee