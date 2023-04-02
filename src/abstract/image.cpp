#include <coffee/abstract/image.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    AbstractImage::AbstractImage(
        ImageType type,
        ImageAspect aspects,
        ImageUsage usageFlags,
        ResourceState layout,
        uint32_t width,
        uint32_t height,
        uint32_t depth
    ) noexcept
            : layout { layout }
            , type_ { type }
            , aspects_ { aspects }
            , width_ { width }
            , height_ { type != ImageType::OneDimensional ? height : 1 }
            , depth_ { type == ImageType::ThreeDimensional ? depth : 1 }
            , usageFlags_ { usageFlags } {}

    ImageType AbstractImage::getType() const noexcept {
        return type_;
    }

    ImageAspect AbstractImage::getAspects() const noexcept {
        return aspects_;
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

    ResourceState AbstractImage::getLayout() const noexcept {
        return layout;
    }

    ImageUsage AbstractImage::getUsageFlags() const noexcept {
        return usageFlags_;
    }

} // namespace coffee