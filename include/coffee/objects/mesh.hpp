#ifndef COFFEE_OBJECTS_MESH
#define COFFEE_OBJECTS_MESH

#include <coffee/graphics/buffer.hpp>
#include <coffee/objects/aabb.hpp>
#include <coffee/objects/materials.hpp>

namespace coffee {

    class Mesh {
    public:
        Mesh(Materials&& mats, AABB&& aabb, uint32_t vertsOffset, uint32_t indsOffset, uint32_t vertsCount, uint32_t indsCount);
        virtual ~Mesh() noexcept = default;

        Mesh(const Mesh&) = default;
        Mesh& operator=(const Mesh&) = default;
        Mesh(Mesh&&) = default;
        Mesh& operator=(Mesh&&) = default;

        Materials materials;
        const AABB aabb;
        const uint32_t verticesOffset;
        const uint32_t indicesOffset;
        const uint32_t verticesCount;
        const uint32_t indicesCount;

        friend class Model;
    };

} // namespace coffee

#endif
