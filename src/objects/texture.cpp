#include <coffee/objects/texture.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    TextureImpl::TextureImpl(Image&& texture, const std::string& filepath, TextureType type)
        : texture_ { std::move(texture) }
        , filepath_ { filepath }
        , type_ { type }
    {
        COFFEE_ASSERT(!filepath_.empty(), "Invalid filename provided.");
        COFFEE_ASSERT(texture_ != nullptr, "Invalid texture provided.");
    }

    const std::string& TextureImpl::getFilePath() const noexcept {
        return filepath_;
    }

    TextureType TextureImpl::getType() const noexcept {
        return type_;
    }

}