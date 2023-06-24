#include <coffee/objects/model.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    Model::Model(std::vector<Mesh>&& meshes, graphics::BufferPtr&& verticesBuffer, graphics::BufferPtr&& indicesBuffer)
        : meshes { std::move(meshes) }
        , verticesBuffer { std::move(verticesBuffer) }
        , indicesBuffer { std::move(indicesBuffer) }
    {
        COFFEE_ASSERT(this->verticesBuffer != nullptr, "Invalid vertices buffer provided.");
    }

} // namespace coffee