#include <coffee/objects/mesh.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    MeshImpl::MeshImpl(Buffer&& verticesBuffer, Buffer&& indicesBuffer, Materials&& materials)
        : materials { std::move(materials) }
        , verticesBuffer_ { std::move(verticesBuffer) }
        , indicesBuffer_ { std::move(indicesBuffer) }
    {
        COFFEE_ASSERT(verticesBuffer_ != nullptr, "Invalid vertices buffer provided.");

        if ((verticesBuffer_->memoryProperties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            COFFEE_WARNING(
                "Mesh {} was loaded without MemoryProperty::DeviceLocal flag, which might cause a performance issues.", fmt::ptr(verticesBuffer_));
        }
    }

    void MeshImpl::draw(const CommandBuffer& commandBuffer)
    {
        VkBuffer buffers[] = { verticesBuffer_->buffer() };
        VkDeviceSize offsets[] = { 0ULL };

        vkCmdBindVertexBuffers(commandBuffer, 0U, 1U, buffers, offsets);

        if (indicesBuffer_ == nullptr) {
            vkCmdDraw(commandBuffer, verticesBuffer_->instanceCount, 1U, 0U, 0U);
            return;
        }

        vkCmdBindIndexBuffer(commandBuffer, indicesBuffer_->buffer(), 0U, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, indicesBuffer_->instanceCount, 1U, 0U, 0U, 0U);
    }

} // namespace coffee