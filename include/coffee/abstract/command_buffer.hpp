#ifndef COFFEE_ABSTRACT_COMMAND_BUFFER
#define COFFEE_ABSTRACT_COMMAND_BUFFER

#include <coffee/abstract/buffer.hpp>
#include <coffee/abstract/descriptors.hpp>
#include <coffee/abstract/framebuffer.hpp>
#include <coffee/abstract/pipeline.hpp>
#include <coffee/abstract/render_pass.hpp>

namespace coffee {

    class AbstractTransferCB : NonMoveable {
    public:
        AbstractTransferCB() noexcept = default;
        virtual ~AbstractTransferCB() noexcept = default;

        virtual void layoutTransition(const Image& image, ResourceState newLayout) = 0;

        virtual void copyBufferToBuffer(
            const Buffer& srcBuffer,
            const Buffer& dstBuffer,
            const std::vector<BufferCopyRegion>& copyRegions
        ) = 0;
        virtual void copyImageToImage(const Image& srcImage, const Image& dstImage, const std::vector<ImageCopyRegion>& copyRegions) = 0;

        virtual void copyBufferToImage(
            const Buffer& srcBuffer,
            const Image& dstImage,
            const std::vector<BufferImageCopyRegion>& copyRegions
        ) = 0;
        virtual void copyImageToBuffer(
            const Image& srcImage,
            const Buffer& dstBuffer,
            const std::vector<BufferImageCopyRegion>& copyRegions
        ) = 0;
    };

    class AbstractGraphicsComputeCB : public virtual AbstractTransferCB {
    public:
        AbstractGraphicsComputeCB() noexcept = default;
        virtual ~AbstractGraphicsComputeCB() noexcept = default;

        virtual void bindPipeline(const Pipeline& pipeline) = 0;
        virtual void bindDescriptorSet(const DescriptorSet& set) = 0;
        virtual void bindDescriptorSets(const std::vector<DescriptorSet>& sets) = 0;

        virtual void pushConstants(ShaderStage shaderStages, const void* data, uint32_t size, uint32_t offset = 0U) = 0;

        template <typename T, std::enable_if_t<!std::is_pointer_v<T> && !std::is_null_pointer_v<T>, bool> = true>
        void pushConstants(ShaderStage shaderStages, const T& data, uint32_t offset = 0U) {
            pushConstants(shaderStages, &data, static_cast<uint32_t>(sizeof(T)), offset);
        }
    };

    class AbstractGraphicsCB : public AbstractGraphicsComputeCB {
    public:
        AbstractGraphicsCB() noexcept = default;
        virtual ~AbstractGraphicsCB() noexcept = default;

        virtual void beginRenderPass(
            const RenderPass& renderPass,
            const Framebuffer& framebuffer,
            const Extent2D& renderArea,
            const Offset2D& offset = {}
        ) = 0;
        virtual void endRenderPass() = 0;

        virtual void setViewport(const Extent2D& area, const Offset2D& offset = {}, float minDepth = 0.0f, float maxDepth = 1.0f) = 0;
        virtual void setScissor(const Extent2D& area, const Offset2D& offset = {}) = 0;
        virtual void setBlendColors(float red, float green, float blue, float alpha) = 0;

        virtual void bindVertexBuffer(const Buffer& vertexBuffer, size_t offset = 0ULL) = 0;
        virtual void bindIndexBuffer(const Buffer& indexBuffer, size_t offset = 0ULL) = 0;

        virtual void draw(uint32_t vertexCount, uint32_t instanceCount = 1U, uint32_t firstVertex = 0U, uint32_t firstInstance = 0U) = 0;
        virtual void drawIndexed(
            uint32_t indexCount,
            uint32_t instanceCount = 1U,
            uint32_t firstIndex = 0U,
            uint32_t vertexOffset = 0U,
            uint32_t firstInstance = 0U
        ) = 0;
        virtual void drawIndirect(const Buffer& drawBuffer, size_t offset = 0U, uint32_t drawCount = 1U, uint32_t stride = 0U) = 0;
        virtual void drawIndexedIndirect(const Buffer& drawBuffer, size_t offset = 0U, uint32_t drawCount = 1U, uint32_t stride = 0U) = 0;
    };

    class AbstractComputeCB : public AbstractGraphicsComputeCB {
    public:
        AbstractComputeCB() noexcept = default;
        virtual ~AbstractComputeCB() noexcept = default;

        virtual void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;
        virtual void dispatchIndirect(const Buffer& buffer, size_t offset = 0U) = 0;
    };

    using TransferCommandBuffer = std::unique_ptr<AbstractTransferCB>;
    using GraphicsCommandBuffer = std::unique_ptr<AbstractGraphicsCB>;
    using ComputeCommandBuffer = std::unique_ptr<AbstractComputeCB>;

} // namespace coffee

#endif