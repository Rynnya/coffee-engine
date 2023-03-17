#include <coffee/abstract/swap_chain.hpp>

namespace coffee {

    PresentMode AbstractSwapChain::getPresentMode() const noexcept {
        return presentMode;
    }

    Format AbstractSwapChain::getColorFormat() const noexcept {
        return imageFormat;
    }

    Format AbstractSwapChain::getDepthFormat() const noexcept {
        return depthFormat;
    }

    size_t AbstractSwapChain::getImagesSize() const noexcept {
        return images.size();
    }

    const std::vector<Image>& AbstractSwapChain::getPresentImages() const noexcept {
        return images;
    }

    uint32_t AbstractSwapChain::getImageIndex() const noexcept {
        return currentFrame;
    }

}