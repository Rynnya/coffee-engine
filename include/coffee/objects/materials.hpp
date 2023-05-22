#ifndef COFFEE_OBJECTS_MATERIALS
#define COFFEE_OBJECTS_MATERIALS

#include <coffee/graphics/image.hpp>
#include <coffee/utils/math.hpp>
#include <coffee/utils/non_copyable.hpp>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace coffee {

    class Materials : NonCopyable {
    public:
        Materials(const ImageViewPtr& defaultTexture);

        void write(const ImageViewPtr& texture, TextureType type);
        const ImageViewPtr& read(TextureType type) const noexcept;
        void reset(TextureType type);

        TextureType textureFlags() const noexcept;

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

        const ImageViewPtr defaultTexture;

    private:
        std::array<ImageViewPtr, 7> textures_;
        TextureType textureFlags_ = TextureType::None;
    };

} // namespace coffee

#endif
