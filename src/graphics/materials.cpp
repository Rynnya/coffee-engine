#include <coffee/graphics/materials.hpp>

#include <coffee/utils/log.hpp>

namespace coffee { namespace graphics {

    static uint32_t textureTypeToIndex(TextureType textureType)
    {
        COFFEE_ASSERT(Math::hasSingleBit(static_cast<uint32_t>(textureType)), "textureType must set ONLY one bit.");

        return Math::indexOfHighestBit(static_cast<uint32_t>(textureType));
    }

    Materials::Materials(const graphics::ImageViewPtr& defaultTexture) : defaultTexture_ { defaultTexture }
    {
        COFFEE_ASSERT(defaultTexture != nullptr, "Invalid defaultTexture provided.");

        textures_.fill(defaultTexture);
    }

    Materials::Materials(Materials&& other) noexcept
        : modifiers { std::move(other.modifiers) }
        , defaultTexture_ { other.defaultTexture_ }
        , textures_ { std::move(other.textures_) }
        , textureFlags_ { other.textureFlags_ }
    {
        other.textures_.fill(other.defaultTexture_);
        other.textureFlags_ = TextureType::None;
    }

    Materials& Materials::operator=(Materials&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }

        modifiers = std::move(other.modifiers);
        defaultTexture_ = other.defaultTexture_;
        textures_ = std::move(other.textures_);
        textureFlags_ = other.textureFlags_;

        other.textures_.fill(other.defaultTexture_);
        other.textureFlags_ = TextureType::None;

        return *this;
    }

    void Materials::write(const graphics::ImageViewPtr& texture, TextureType type)
    {
        if (texture == nullptr) {
            return;
        }

        tbb::queuing_mutex::scoped_lock lock { mutex_ };

        textures_[textureTypeToIndex(type)] = texture;
        reinterpret_cast<uint32_t&>(textureFlags_) |= static_cast<uint32_t>(type);
    }

    graphics::ImageViewPtr Materials::read(TextureType type) const noexcept
    {
        tbb::queuing_mutex::scoped_lock lock { mutex_ };

        return textures_[textureTypeToIndex(type)];
    }

    void Materials::reset(TextureType type)
    {
        tbb::queuing_mutex::scoped_lock lock { mutex_ };

        textures_[textureTypeToIndex(type)] = defaultTexture_;
        reinterpret_cast<uint32_t&>(textureFlags_) &= ~static_cast<uint32_t>(type);
    }

    TextureType Materials::textureFlags() const noexcept
    {
        tbb::queuing_mutex::scoped_lock lock { mutex_ };

        return textureFlags_;
    }

    const graphics::ImageViewPtr& Materials::defaultTexture() const noexcept { return defaultTexture_; }

}} // namespace coffee::graphics