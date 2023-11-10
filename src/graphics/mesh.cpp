#include <coffee/graphics/mesh.hpp>

#include <coffee/utils/log.hpp>

namespace coffee { namespace graphics {

    Mesh::Mesh(std::vector<SubMesh>&& subMeshes, BufferPtr&& verticesBuffer, BufferPtr&& indicesBuffer)
        : subMeshes { std::move(subMeshes) }
        , verticesBuffer { std::move(verticesBuffer) }
        , indicesBuffer { std::move(indicesBuffer) }
    {
        COFFEE_ASSERT(this->verticesBuffer != nullptr, "Invalid vertices buffer provided.");
    }

}} // namespace coffee::graphics