#ifndef COFFEE_OBJECTS_MATERIALS
#define COFFEE_OBJECTS_MATERIALS

#include <coffee/graphics/image.hpp>
#include <coffee/utils/math.hpp>
#include <coffee/utils/non_copyable.hpp>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <tbb/queuing_mutex.h>

namespace coffee {

namespace graphics {

    class Materials : NonCopyable {
    public:
        Materials(const graphics::ImageViewPtr& defaultTexture);

        Materials(Materials&& other) noexcept;
        Materials& operator=(Materials&& other) noexcept;

        void write(const graphics::ImageViewPtr& texture, TextureType type);
        graphics::ImageViewPtr read(TextureType type) const noexcept;
        void reset(TextureType type);

        TextureType textureFlags() const noexcept;
        const graphics::ImageViewPtr& defaultTexture() const noexcept;

        struct Modifiers {
            // Diffuse color component of mesh
            // RGB Formula if diffuse texture is provided: diffuseTexture.rgb * diffuseColor
            // RGB Formula if diffuse texture isn't provided: diffuseColor
            glm::vec3 diffuseColor {};
            // Specular color component of mesh
            // RGB Formula if specular texture is provided: specularTexture.rgb * specularColor
            // RGB Formula if specular texture isn't provided: specularColor
            glm::vec3 specularColor {};
            // Metallic factor must be used in PBR equations
            float metallicFactor = 0.0f;
            // Roughness factor must be used in PBR equations
            float roughnessFactor = 0.0f;
        } modifiers {};

    private:
        static constexpr uint32_t kAmountOfTexturesPerMesh = 7U;

        mutable tbb::queuing_mutex mutex_ {};
        graphics::ImageViewPtr defaultTexture_;
        std::array<graphics::ImageViewPtr, kAmountOfTexturesPerMesh> textures_;
        TextureType textureFlags_ = TextureType::None;
    };

} // namespace graphics

} // namespace coffee

#endif
