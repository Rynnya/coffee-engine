#ifndef COFFEE_OBJECTS_MATERIALS
#define COFFEE_OBJECTS_MATERIALS

#include <coffee/objects/texture.hpp>
#include <coffee/utils/math.hpp>
#include <coffee/utils/non_copyable.hpp>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace coffee {

    class Materials : NonCopyable {
    public:
        Materials(const Texture& defaultTexture);

        void write(const Texture& texture);
        const Texture& read(TextureType type) const noexcept;
        void reset(TextureType type);

        TextureType getTextureFlags() const noexcept;

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

        const Texture defaultTexture;

    private:
        std::array<Texture, 7> textures_;
        TextureType textureFlags_ = TextureType::None;
    };

} // namespace coffee

#endif
