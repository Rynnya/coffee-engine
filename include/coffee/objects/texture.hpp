#ifndef COFFEE_OBJECTS_TEXTURE
#define COFFEE_OBJECTS_TEXTURE

#include <coffee/abstract/image.hpp>

namespace coffee {

    class TextureImpl {
    public:
        TextureImpl(Image&& texture, const std::string& filepath, TextureType type);
        ~TextureImpl() noexcept = default;

        const std::string& getFilePath() const noexcept;
        TextureType getType() const noexcept;

    private:
        Image texture_;
        std::string filepath_;
        TextureType type_;

        friend class DescriptorWriter;
    };

    using Texture = std::shared_ptr<TextureImpl>;

}

#endif