#include <coffee/graphics/submesh.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    namespace graphics {

        SubMesh::SubMesh(Materials&& mats, AABB&& aabb, uint32_t vertsOffset, uint32_t indsOffset, uint32_t vertsCount, uint32_t indsCount)
            : materials { std::move(mats) }
            , aabb { std::move(aabb) }
            , verticesOffset { vertsOffset }
            , indicesOffset { indsOffset }
            , verticesCount { vertsCount }
            , indicesCount { indsCount }
        {}

    } // namespace graphics

} // namespace coffee