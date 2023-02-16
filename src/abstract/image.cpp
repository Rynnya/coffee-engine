#include <coffee/abstract/image.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    AbstractImage::AbstractImage(ImageType type, uint32_t width, uint32_t height, uint32_t depth) noexcept : type_ { type } {
        width_ = width;
        height_ = type != ImageType::OneDimensional ? height : 1;
        depth_ = type == ImageType::ThreeDimensional ? depth : 1;
    }

    ImageType AbstractImage::getType() const noexcept {
        return type_;
    }

    uint32_t AbstractImage::getWidth() const noexcept {
        return width_;
    }

    uint32_t AbstractImage::getHeight() const noexcept {
        return height_;
    }

    uint32_t AbstractImage::getDepth() const noexcept {
        return depth_;
    }

}