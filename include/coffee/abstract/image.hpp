#ifndef COFFEE_ABSTRACT_IMAGE
#define COFFEE_ABSTRACT_IMAGE

#include <coffee/abstract/sampler.hpp>
#include <coffee/types.hpp>

namespace coffee {

    class AbstractImage : NonMoveable {
    public:
        AbstractImage(ImageType type, uint32_t width, uint32_t height, uint32_t depth) noexcept;
        virtual ~AbstractImage() noexcept = default;

        ImageType getType() const noexcept;
        uint32_t getWidth() const noexcept;
        uint32_t getHeight() const noexcept;
        uint32_t getDepth() const noexcept;

    private:
        ImageType type_;
        uint32_t width_;
        uint32_t height_;
        uint32_t depth_;
    };

    using Image = std::shared_ptr<AbstractImage>;

}

#endif