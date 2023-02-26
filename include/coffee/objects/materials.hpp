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

        struct Modifiers {
            float shininess = 0.0f;
            Vec3 diffuseColor {};
            Vec3 specularColor {};
            // Ambient color component of mesh
            // RGB Formula: (colorRed * ambientRed, colorGreen * ambientGreen, colorBlue * ambientBlue)
            // Alpha component used as intensity and must be used as RGB Formula * Intensity
            Vec4 ambientColor {};
        } modifiers {};

        const Texture defaultTexture;
    private:
        std::array<Texture, 9> textures_;
    };

}

#endif
