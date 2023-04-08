#include <coffee/objects/texture.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    TextureImpl::TextureImpl(Image&& texture, ImageView&& textureView, const std::string& filepath, TextureType type)
        : filepath { filepath }
        , type { type }
        , texture { std::move(texture) }
        , textureView { std::move(textureView) }
    {
        COFFEE_ASSERT(!filepath.empty(), "Invalid filename provided.");

        COFFEE_ASSERT(this->texture != nullptr, "Invalid texture provided.");
        COFFEE_ASSERT(this->textureView != nullptr, "Invalid texture view provided.");
    }

} // namespace coffee