#include <coffee/objects/model.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    Model::Model(std::vector<Mesh>&& meshes, BufferPtr&& verticesBuffer, BufferPtr&& indicesBuffer)
        : meshes { std::move(meshes) }
        , verticesBuffer_ { std::move(verticesBuffer) }
        , indicesBuffer_ { std::move(indicesBuffer) }
    {
        COFFEE_ASSERT(verticesBuffer_ != nullptr, "Invalid vertices buffer provided.");
    }

    void Model::bind(const CommandBuffer& commandBuffer) const {
        VkBuffer buffers[] = { verticesBuffer_->buffer() };
        VkDeviceSize offsets[] = { 0ULL };
        vkCmdBindVertexBuffers(commandBuffer, 0U, 1U, buffers, offsets);

        if (indicesBuffer_ != nullptr) {
            vkCmdBindIndexBuffer(commandBuffer, indicesBuffer_->buffer(), 0U, VK_INDEX_TYPE_UINT32);
        }
    }

    void Model::draw(const CommandBuffer& commandBuffer) const
    {
        if (indicesBuffer_ == nullptr) {
            for (const auto& mesh : meshes) {
                vkCmdDraw(commandBuffer, mesh.verticesCount, 1U, mesh.verticesOffset, 0U);
            }
        }
        else {
            for (const auto& mesh : meshes) {
                vkCmdDrawIndexed(commandBuffer, mesh.indicesCount, 1U, mesh.indicesOffset, mesh.verticesOffset, 0U);
            }
        }
    }

} // namespace coffee