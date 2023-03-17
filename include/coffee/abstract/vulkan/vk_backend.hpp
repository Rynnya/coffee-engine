#ifndef COFFEE_VK_BACKEND
#define COFFEE_VK_BACKEND

#include <coffee/abstract/backend.hpp>

#include <coffee/abstract/vulkan/vk_device.hpp>
#include <coffee/abstract/vulkan/vk_swap_chain.hpp>

namespace coffee {

    class VulkanBackend : public AbstractBackend {
    public:
        VulkanBackend() noexcept;
        ~VulkanBackend() noexcept = default;

        Window createWindow(WindowSettings settings, const std::string& windowName) override;

        Buffer createBuffer(const BufferConfiguration& configuration) override;
        Image createImage(const ImageConfiguration& configuration) override;
        Sampler createSampler(const SamplerConfiguration& configuration) override;

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
            const std::vector<DescriptorLayout>& descriptorLayouts,
            const std::vector<Shader>& shaderPrograms,
            const PipelineConfiguration& configuration) override;
        std::unique_ptr<AbstractFramebuffer> createFramebuffer(
            const RenderPass& renderPass,
            const FramebufferConfiguration& configuration) override;
        std::unique_ptr<AbstractCommandBuffer> createCommandBuffer() override;

        void copyBuffer(Buffer& dstBuffer, const Buffer& srcBuffer) override;
        void copyBufferToImage(Image& dstImage, const Buffer& srcBuffer) override;

        void waitDevice() override;

        VulkanDevice device_ {};
    };

}

#endif