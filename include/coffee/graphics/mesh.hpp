#ifndef COFFEE_GRAPHICS_MESH
#define COFFEE_GRAPHICS_MESH

#include <coffee/graphics/aabb.hpp>
#include <coffee/graphics/buffer.hpp>
#include <coffee/graphics/materials.hpp>

namespace coffee {

    namespace graphics {

        class Mesh : NonCopyable {
        public:
            Mesh(Materials&& mats, AABB&& aabb, uint32_t vertsOffset, uint32_t indsOffset, uint32_t vertsCount, uint32_t indsCount);
            virtual ~Mesh() noexcept = default;

            Mesh(Mesh&& other) noexcept = default;
            Mesh& operator=(Mesh&& other) noexcept = delete;

            Materials materials;
            const AABB aabb;
            const uint32_t verticesOffset;
            const uint32_t indicesOffset;
            const uint32_t verticesCount;
            const uint32_t indicesCount;

            friend class Model;
        };

    } // namespace graphics

} // namespace coffee

#endif
