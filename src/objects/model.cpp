#include <coffee/objects/model.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    ModelImpl::ModelImpl(std::vector<Mesh>&& meshes)
        : meshes { std::move(meshes) }
    {}

    void ModelImpl::draw(const CommandBuffer& commandBuffer) {
        for (const auto& mesh : meshes) {
            mesh->draw(commandBuffer);
        }
    }

}