#include <coffee/objects/materials.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    Materials::Materials(const Texture& defaultTexture) : defaultTexture { defaultTexture } {
        COFFEE_ASSERT(defaultTexture != nullptr, "Invalid defaultTexture provided.");

        textures_.fill(defaultTexture);
    }

    void Materials::write(const Texture& texture) {
        uint32_t nativeType = static_cast<uint32_t>(texture->getType()) - 1;

        COFFEE_ASSERT(nativeType >= 0 && nativeType <= 8, "Invalid type index provided. Update this assert if new materials where added.");

        textures_[nativeType] = texture;
    }

    const Texture& Materials::read(TextureType type) const noexcept {
        uint32_t nativeType = static_cast<uint32_t>(type) - 1;

        COFFEE_ASSERT(nativeType >= 0 && nativeType <= 8, "Invalid type index provided. Update this assert if new materials where added.");

        return textures_[nativeType];
    }

    void Materials::reset(TextureType type) {
        uint32_t nativeType = static_cast<uint32_t>(type) - 1;

        COFFEE_ASSERT(nativeType >= 0 && nativeType <= 8, "Invalid type index provided. Update this assert if new materials where added.");

        textures_[nativeType] = defaultTexture;
    }

}