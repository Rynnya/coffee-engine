#include <coffee/objects/materials.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    static uint32_t textureTypeToIndex(TextureType textureType) {
        COFFEE_ASSERT(Math::hasSingleBit(static_cast<uint32_t>(textureType)), "textureType must set ONLY one bit.");

        return Math::indexOfHighestBit(static_cast<uint32_t>(textureType));
    }

    Materials::Materials(const Texture& defaultTexture) : defaultTexture { defaultTexture } {
        COFFEE_ASSERT(defaultTexture != nullptr, "Invalid defaultTexture provided.");

        textures_.fill(defaultTexture);
    }

    void Materials::write(const Texture& texture) {
        TextureType type = texture->getType();

        textures_[textureTypeToIndex(type)] = texture;

        if (texture != defaultTexture) {
            reinterpret_cast<uint32_t&>(textureFlags_) |= static_cast<uint32_t>(type);
        }
    }

    const Texture& Materials::read(TextureType type) const noexcept {
        return textures_[textureTypeToIndex(type)];
    }

    void Materials::reset(TextureType type) {
        textures_[textureTypeToIndex(type)] = defaultTexture;
        reinterpret_cast<uint32_t&>(textureFlags_) &= ~static_cast<uint32_t>(type);
    }

    TextureType Materials::getTextureFlags() const noexcept {
        return textureFlags_;
    }

}