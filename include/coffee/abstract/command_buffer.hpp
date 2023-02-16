#ifndef COFFEE_ABSTRACT_COMMAND_BUFFER
#define COFFEE_ABSTRACT_COMMAND_BUFFER

#include <coffee/abstract/buffer.hpp>
#include <coffee/abstract/barriers.hpp>
#include <coffee/abstract/descriptors.hpp>
#include <coffee/abstract/framebuffer.hpp>
#include <coffee/abstract/pipeline.hpp>
#include <coffee/abstract/render_pass.hpp>

namespace coffee {

    class AbstractCommandBuffer : NonMoveable {
    public:
        AbstractCommandBuffer() noexcept = default;
        virtual ~AbstractCommandBuffer() noexcept = default;

        virtual void beginRenderPass(
            const RenderPass& renderPass,
            const Framebuffer& framebuffer,
            const Extent2D& renderArea,
            const Offset2D& offset = {}) = 0;
        virtual void endRenderPass() = 0;

        virtual void bindPipeline(const Pipeline& pipeline) = 0;
        virtual void bindDescriptorSets(const Pipeline& pipeline, const DescriptorSet& set) = 0;
        //virtual void clearAttachments(
        //    const Framebuffer& framebuffer,
        //    const ClearColorValue& colorValues,
        //    const std::optional<ClearDepthStencilValue>& depthValues = {},
        //    const Offset2D& offset = {},
        //    const Extent2D& clearArea = {}) = 0;

        virtual void setViewport(
            float width, float height, float x = 0.0f, float y = 0.0f, float minDepth = 0.0f, float maxDepth = 1.0f) = 0;
        virtual void setScissor(uint32_t width, uint32_t height, uint32_t x = 0U, uint32_t y = 0U) = 0;
        virtual void setBlendColors(float red, float green, float blue, float alpha) = 0;

        template <typename T, std::enable_if_t<!std::is_pointer_v<T> && !std::is_null_pointer_v<T>, bool> = true>
        void pushConstants(ShaderStage shaderStages, const T& data, uint32_t offset = 0U) {
            pushConstants(shaderStages, &data, static_cast<uint32_t>(sizeof(T)), offset);
        }

        virtual void pushConstants(ShaderStage shaderStages, const void* data, uint32_t size, uint32_t offset = 0U) = 0;
        virtual void bindVertexBuffer(const Buffer& vertexBuffer, size_t offset = 0ULL) = 0;
        virtual void bindIndexBuffer(const Buffer& indexBuffer, size_t offset = 0ULL) = 0;

        virtual void draw(
            uint32_t vertexCount,
            uint32_t instanceCount = 1U,
            uint32_t firstVertex = 0U,
            uint32_t firstInstance = 0U) = 0;
        virtual void drawIndexed(
            uint32_t indexCount, 
            uint32_t instanceCount = 1U,
            uint32_t firstIndex = 0U,
            uint32_t vertexOffset = 0U,
            uint32_t firstInstance = 0U) = 0;
        virtual void drawIndirect(
            const Buffer& drawBuffer,
            size_t offset = 0U,
            uint32_t drawCount = 1U,
            uint32_t stride = 0U) = 0;
        virtual void drawIndexedIndirect(
            const Buffer& drawBuffer,
            size_t offset = 0U,
            uint32_t drawCount = 1U,
            uint32_t stride = 0U) = 0;

        virtual void memoryBarrier(const MemoryBarrier& memoryBarrier) = 0;
        virtual void bufferBarrier(const BufferBarrier& bufferBarrier) = 0;
        virtual void imageBarrier(const ImageBarrier& imageBarrier) = 0;

        // TODO: Add query when i can finally understand what this thing do

        virtual void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;
        virtual void dispatchIndirect(const Buffer& dispatchBuffer, size_t offset) = 0;
    };

    using CommandBuffer = std::unique_ptr<AbstractCommandBuffer>;

}

#endif