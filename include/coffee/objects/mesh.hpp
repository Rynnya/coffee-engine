#ifndef COFFEE_OBJECTS_MESH
#define COFFEE_OBJECTS_MESH

#include <coffee/graphics/buffer.hpp>
#include <coffee/interfaces/drawable.hpp>
#include <coffee/objects/materials.hpp>

namespace coffee {

    class Mesh : public Drawable {
    public:
        friend class Model;

        Mesh(Materials&& materials, uint32_t verticesOffset, uint32_t indicesOffset, uint32_t verticesCount, uint32_t indicesCount);
        virtual ~Mesh() noexcept = default;

        Mesh(const Mesh&) = default;
        Mesh& operator=(const Mesh&) = default;
        Mesh(Mesh&&) = default;
        Mesh& operator=(Mesh&&) = default;

        Materials materials;
        const uint32_t verticesOffset;
        const uint32_t indicesOffset;
        const uint32_t verticesCount;
        const uint32_t indicesCount;

        void draw(const CommandBuffer& commandBuffer) const override;
    };

} // namespace coffee

#endif
