#ifndef COFFEE_VK_COMMAND_BUFFER
#define COFFEE_VK_COMMAND_BUFFER

#include <coffee/abstract/vulkan/vk_device.hpp>
#include <coffee/abstract/command_buffer.hpp>

namespace coffee {

    class VulkanCommandBuffer : public AbstractCommandBuffer {
    public:
        VulkanCommandBuffer(VulkanDevice& device);
        ~VulkanCommandBuffer();

        void beginRenderPass(
            const RenderPass& renderPass,
            const Framebuffer& framebuffer,
            const Extent2D& renderArea,
            const Offset2D& offset) override;
        void endRenderPass() override;

        void bindPipeline(const Pipeline& pipeline) override;
        void bindDescriptorSet(const DescriptorSet& set) override;
        void bindDescriptorSets(const std::vector<DescriptorSet>& sets) override;

        void setViewport(const Extent2D& area, const Offset2D& offset, float minDepth, float maxDepth) override;
        void setScissor(const Extent2D& area, const Offset2D& offset) override;
        void setBlendColors(float red, float green, float blue, float alpha) override;

        void pushConstants(ShaderStage shaderStages, const void* data, uint32_t size, uint32_t offset) override;
        void bindVertexBuffer(const Buffer& vertexBuffer, size_t offset) override;
        void bindIndexBuffer(const Buffer& indexBuffer, size_t offset) override;

        void draw(
            uint32_t vertexCount,
            uint32_t instanceCount,
            uint32_t firstVertex,
            uint32_t firstInstance) override;
        void drawIndexed(
            uint32_t indexCount,
            uint32_t instanceCount,
            uint32_t firstIndex,
            uint32_t vertexOffset,
            uint32_t firstInstance) override;
        void drawIndirect(
            const Buffer& drawBuffer,
            size_t offset,
            uint32_t drawCount,
            uint32_t stride) override;
        void drawIndexedIndirect(
            const Buffer& drawBuffer,
            size_t offset,
            uint32_t drawCount,
            uint32_t stride) override;

        void memoryBarrier(const MemoryBarrier& memoryBarrier) override;
        void bufferBarrier(const BufferBarrier& bufferBarrier) override;
        void imageBarrier(const ImageBarrier& imageBarrier) override;

        void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;
        void dispatchIndirect(const Buffer& dispatchBuffer, size_t offset) override;

        VkCommandPool pool;
        VkCommandBuffer commandBuffer;

        bool renderPassActive = false;
        bool hasDepthAttachment_ = false;
        Extent2D renderArea_ { 0, 0 };

        VkPipelineLayout layout = nullptr;

    private:
        VulkanDevice& device_;
    };

}

#endif