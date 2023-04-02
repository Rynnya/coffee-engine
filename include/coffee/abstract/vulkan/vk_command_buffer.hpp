#ifndef COFFEE_VK_COMMAND_BUFFER
#define COFFEE_VK_COMMAND_BUFFER

#include <coffee/abstract/command_buffer.hpp>
#include <coffee/abstract/vulkan/vk_device.hpp>

namespace coffee {

    class VulkanTransferCB : public virtual AbstractTransferCB {
    public:
        VulkanTransferCB(VulkanDevice& device);
        virtual ~VulkanTransferCB() noexcept;

        void layoutTransition(const Image& image, ResourceState newLayout) override;

        void copyBufferToBuffer(const Buffer& srcBuffer, const Buffer& dstBuffer, const std::vector<BufferCopyRegion>& copyRegions)
            override;
        void copyImageToImage(const Image& srcImage, const Image& dstImage, const std::vector<ImageCopyRegion>& copyRegions) override;

        void copyBufferToImage(const Buffer& srcBuffer, const Image& dstImage, const std::vector<BufferImageCopyRegion>& copyRegions)
            override;
        void copyImageToBuffer(const Image& srcImage, const Buffer& dstBuffer, const std::vector<BufferImageCopyRegion>& copyRegions)
            override;

        VkCommandPool pool;
        VkCommandBuffer commandBuffer;

        VulkanDevice& device;
    };

    class VulkanGraphicsCB
            : public VulkanTransferCB
            , public AbstractGraphicsCB {
    public:
        VulkanGraphicsCB(VulkanDevice& device);
        ~VulkanGraphicsCB() noexcept = default;

        void beginRenderPass(
            const RenderPass& renderPass,
            const Framebuffer& framebuffer,
            const Extent2D& renderArea,
            const Offset2D& offset
        ) override;
        void endRenderPass() override;

        void bindPipeline(const Pipeline& pipeline) override;
        void bindDescriptorSet(const DescriptorSet& set) override;
        void bindDescriptorSets(const std::vector<DescriptorSet>& sets) override;

        void bindVertexBuffer(const Buffer& vertexBuffer, size_t offset) override;
        void bindIndexBuffer(const Buffer& indexBuffer, size_t offset) override;

        void pushConstants(ShaderStage shaderStages, const void* data, uint32_t size, uint32_t offset = 0U) override;

        template <typename T, std::enable_if_t<!std::is_pointer_v<T> && !std::is_null_pointer_v<T>, bool> = true>
        void pushConstants(ShaderStage shaderStages, const T& data, uint32_t offset = 0U) {
            pushConstants(shaderStages, &data, static_cast<uint32_t>(sizeof(T)), offset);
        }

        void setViewport(const Extent2D& area, const Offset2D& offset, float minDepth, float maxDepth) override;
        void setScissor(const Extent2D& area, const Offset2D& offset) override;
        void setBlendColors(float red, float green, float blue, float alpha) override;

        void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override;
        void drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance)
            override;
        void drawIndirect(const Buffer& drawBuffer, size_t offset, uint32_t drawCount, uint32_t stride) override;
        void drawIndexedIndirect(const Buffer& drawBuffer, size_t offset, uint32_t drawCount, uint32_t stride) override;

        bool renderPassActive = false;
        bool hasDepthAttachment = false;
        Extent2D renderArea { 0, 0 };

        VkPipelineLayout layout = nullptr;
    };

} // namespace coffee

#endif