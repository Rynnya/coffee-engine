#ifndef COFFEE_OBJECTS_MATERIALS
#define COFFEE_OBJECTS_MATERIALS

#include <coffee/objects/texture.hpp>
#include <coffee/utils/math.hpp>
#include <coffee/utils/non_copyable.hpp>

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
            Vec3 diffuseColor {};
            // Specular color component of mesh
            // RGB Formula if specular texture is provided: specularTexture.rgb * specularColor
            // RGB Formula if specular texture isn't provided: specularColor
            Vec3 specularColor {};
            // Ambient color component of mesh
            // RGB Formula: ambientColor.rgb * ambientColor.a * alreadyCalculatedDiffuseColor
            // Alpha component used as overall intensity (default value is 0.02, only set when ambient is not provided by model)
            Vec4 ambientColor {};
            // Shininess must be used as exponent for Blinn-Phong equation
            float shininessExponent = 1.0f;
            // Metallic factor must be used in PBR equations
            float metallicFactor = 0.0f;
            // Roughness factor must be used in PBR equations
            float roughnessFactor = 0.0f;
        } modifiers {};

        const Texture defaultTexture;
    private:
        std::array<Texture, 9> textures_;
        TextureType textureFlags_ = TextureType::None;
    };

}

#endif
