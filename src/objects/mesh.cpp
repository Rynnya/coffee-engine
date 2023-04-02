#include <coffee/objects/mesh.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    MeshImpl::MeshImpl(Buffer&& verticesBuffer, Buffer&& indicesBuffer, Materials&& materials)
            : materials { std::move(materials) }
            , verticesBuffer_ { std::move(verticesBuffer) }
            , indicesBuffer_ { std::move(indicesBuffer) } {
        COFFEE_ASSERT(verticesBuffer_ != nullptr, "Invalid vertices buffer provided.");

        if ((verticesBuffer_->getMemoryFlags() & MemoryProperty::DeviceLocal) != MemoryProperty::DeviceLocal) {
            COFFEE_WARNING(
                "Mesh {} was loaded without MemoryProperty::DeviceLocal flag, which might cause a performance issues.", fmt::ptr(verticesBuffer_));
        }
    }

    void MeshImpl::draw(const GraphicsCommandBuffer& commandBuffer) {
        commandBuffer->bindVertexBuffer(verticesBuffer_);

        if (indicesBuffer_ == nullptr) {
            commandBuffer->draw(verticesBuffer_->getInstanceCount());
            return;
        }

        commandBuffer->bindIndexBuffer(indicesBuffer_);
        commandBuffer->drawIndexed(indicesBuffer_->getInstanceCount());
    }

} // namespace coffee