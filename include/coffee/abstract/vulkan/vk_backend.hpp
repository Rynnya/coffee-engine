#ifndef COFFEE_VK_BACKEND
#define COFFEE_VK_BACKEND

#include <coffee/abstract/backend.hpp>

#include <coffee/abstract/vulkan/vk_device.hpp>
#include <coffee/abstract/vulkan/vk_swap_chain.hpp>

namespace coffee {

    class VulkanBackend : public AbstractBackend {
    public:
        VulkanBackend(void* windowHandle, uint32_t windowWidth, uint32_t windowHeight);
        ~VulkanBackend();

        bool acquireFrame() override;
        void sendCommandBuffer(CommandBuffer&& commandBuffer) override;
        void endFrame() override;

        void changeFramebufferSize(uint32_t width, uint32_t height) override;
        void changePresentMode(uint32_t width, uint32_t height, PresentMode newMode) override;

        std::unique_ptr<AbstractBuffer> createBuffer(const BufferConfiguration& configuration) override;
        std::shared_ptr<AbstractImage> createImage(const ImageConfiguration& configuration) override;
        std::shared_ptr<AbstractSampler> createSampler(const SamplerConfiguration& configuration) override;

        std::unique_ptr<AbstractShader> createShader(
            const std::string& fileName,
            ShaderStage stage,
            const std::string& entrypoint) override;
        std::unique_ptr<AbstractShader> createShader(
            const std::vector<uint8_t>& bytes,
            ShaderStage stage,
            const std::string& entrypoint) override;

        std::shared_ptr<AbstractDescriptorLayout> createDescriptorLayout(
            const std::map<uint32_t, DescriptorBindingInfo>& bindings) override;
        std::shared_ptr<AbstractDescriptorSet> createDescriptorSet(
            const DescriptorWriter& writer) override;

        std::unique_ptr<AbstractRenderPass> createRenderPass(
            const RenderPass& parent, const RenderPassConfiguration& configuration) override;
        std::unique_ptr<AbstractPipeline> createPipeline(
            const RenderPass& renderPass,
            const PushConstants& pushConstants,
            const std::vector<DescriptorLayout>& descriptorLayouts,
            const ShaderProgram& shaderProgram,
            const PipelineConfiguration& configuration) override;
        std::unique_ptr<AbstractFramebuffer> createFramebuffer(
            const RenderPass& renderPass,
            const FramebufferConfiguration& configuration) override;
        std::unique_ptr<AbstractCommandBuffer> createCommandBuffer() override;

        void copyBuffer(Buffer& dstBuffer, const Buffer& srcBuffer) override;
        void copyBufferToImage(Image& dstImage, const Buffer& srcBuffer) override;

        Format getColorFormat() const noexcept override;
        Format getDepthFormat() const noexcept override;
        uint32_t getCurrentImageIndex() const noexcept override;
        const std::vector<Image>& getPresentImages() const noexcept override;

        void waitDevice() override;

    private:
        void checkSupportedPresentModes() noexcept;

        VulkanDevice device_;
        std::unique_ptr<VulkanSwapChain> swapChain_;

        std::vector<std::vector<std::pair<VkCommandPool, VkCommandBuffer>>> poolsAndBuffers_ {};
        std::vector<CommandBuffer> commandBuffers_ {};

        uint32_t framebufferWidth_;
        uint32_t framebufferHeight_;
        uint32_t currentImage_ = 0U;

        bool mailboxSupported_ = false;
        bool immediateSupported_ = false;
    };

}

#endif