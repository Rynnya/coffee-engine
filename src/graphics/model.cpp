#include <coffee/graphics/model.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    namespace graphics {

        Model::Model(std::vector<Mesh>&& meshes, BufferPtr&& verticesBuffer, BufferPtr&& indicesBuffer)
            : meshes { std::move(meshes) }
            , verticesBuffer { std::move(verticesBuffer) }
            , indicesBuffer { std::move(indicesBuffer) }
        {
            COFFEE_ASSERT(this->verticesBuffer != nullptr, "Invalid vertices buffer provided.");
        }

    } // namespace graphics

} // namespace coffee