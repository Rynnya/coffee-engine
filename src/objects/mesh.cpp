#include <coffee/objects/mesh.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    Mesh::Mesh(Materials&& materials, uint32_t verticesOffset, uint32_t indicesOffset, uint32_t verticesCount, uint32_t indicesCount)
        : materials { std::move(materials) }
        , verticesOffset { verticesOffset }
        , indicesOffset { indicesOffset }
        , verticesCount { verticesCount }
        , indicesCount { indicesCount }
    {}

    void Mesh::draw(const CommandBuffer& commandBuffer) const {
        if (indicesCount == 0) {
            vkCmdDraw(commandBuffer, verticesCount, 1U, verticesOffset, 0U);
            return;
        }

        vkCmdDrawIndexed(commandBuffer, indicesCount, 1U, indicesOffset, verticesOffset, 0U);
    }

} // namespace coffee