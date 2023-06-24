#include <coffee/objects/mesh.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    Mesh::Mesh(Materials&& mats, AABB&& aabb, uint32_t vertsOffset, uint32_t indsOffset, uint32_t vertsCount, uint32_t indsCount)
        : materials { std::move(mats) }
        , aabb { std::move(aabb) }
        , verticesOffset { vertsOffset }
        , indicesOffset { indsOffset }
        , verticesCount { vertsCount }
        , indicesCount { indsCount }
    {}

} // namespace coffee