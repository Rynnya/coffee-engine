#ifndef COFFEE_GRAPHICS_SUBMESH
#define COFFEE_GRAPHICS_SUBMESH

#include <coffee/graphics/aabb.hpp>
#include <coffee/graphics/buffer.hpp>
#include <coffee/graphics/materials.hpp>

namespace coffee {

namespace graphics {

    class SubMesh : NonCopyable {
    public:
        SubMesh(Materials&& mats, AABB&& aabb, uint32_t vertsOffset, uint32_t indsOffset, uint32_t vertsCount, uint32_t indsCount);
        ~SubMesh() noexcept = default;

        SubMesh(SubMesh&& other) noexcept = default;
        SubMesh& operator=(SubMesh&& other) noexcept = delete;

        Materials materials;
        const AABB aabb;
        const uint32_t verticesOffset;
        const uint32_t indicesOffset;
        const uint32_t verticesCount;
        const uint32_t indicesCount;

        friend class Mesh;
    };

} // namespace graphics

} // namespace coffee

#endif
