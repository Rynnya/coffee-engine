#ifndef COFFEE_OBJECTS_TEXTURE
#define COFFEE_OBJECTS_TEXTURE

#include <coffee/graphics/image.hpp>
#include <coffee/types.hpp>

namespace coffee {

    class TextureImpl {
    public:
        TextureImpl(Image&& texture, ImageView&& textureView, const std::string& filepath, TextureType type);
        ~TextureImpl() noexcept = default;

        const std::string& filepath;
        const TextureType type;

        const Image texture;
        const ImageView textureView;
    };

    using Texture = std::shared_ptr<TextureImpl>;

} // namespace coffee

#endif