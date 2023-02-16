#ifndef COFFEE_ABSTRACT_BACKEND
#define COFFEE_ABSTRACT_BACKEND

#include <coffee/abstract/buffer.hpp>
#include <coffee/abstract/command_buffer.hpp>
#include <coffee/abstract/descriptors.hpp>
#include <coffee/abstract/framebuffer.hpp>
#include <coffee/abstract/image.hpp>
#include <coffee/abstract/pipeline.hpp>
#include <coffee/abstract/push_constant.hpp>
#include <coffee/abstract/render_pass.hpp>
#include <coffee/abstract/sampler.hpp>
#include <coffee/abstract/shader.hpp>
#include <coffee/abstract/shader_program.hpp>

namespace coffee {

    class AbstractBackend : NonMoveable {
    public:
        AbstractBackend() noexcept = default;
        virtual ~AbstractBackend() noexcept = default;

        virtual bool acquireFrame() = 0;
        virtual void sendCommandBuffer(CommandBuffer&& commandBuffer) = 0;
        virtual void endFrame() = 0;

        virtual void changeFramebufferSize(uint32_t width, uint32_t height) = 0;
        virtual void changePresentMode(uint32_t width, uint32_t height, PresentMode newMode) = 0;

        virtual std::unique_ptr<AbstractBuffer> createBuffer(const BufferConfiguration& configuration) = 0;
        virtual std::shared_ptr<AbstractImage> createImage(const ImageConfiguration& configuration) = 0;
        virtual std::shared_ptr<AbstractSampler> createSampler(const SamplerConfiguration& configuration) = 0;

        virtual Shader createShader(
            const std::string& fileName,
            ShaderStage stage,
            const std::string& entrypoint) = 0;
        virtual Shader createShader(
            const std::vector<uint8_t>& bytes,
            ShaderStage stage,
            const std::string& entrypoint) = 0;

        virtual DescriptorLayout createDescriptorLayout(
            const std::map<uint32_t, DescriptorBindingInfo>& bindings) = 0;
        virtual DescriptorSet createDescriptorSet(
            const DescriptorWriter& writer) = 0;

        virtual RenderPass createRenderPass(
            const RenderPass& parent,
            const RenderPassConfiguration& configuration) = 0;
        virtual Pipeline createPipeline(
            const RenderPass& renderPass,
            const PushConstants& pushConstants,
            const std::vector<DescriptorLayout>& descriptorLayouts,
            const ShaderProgram& shaderProgram,
            const PipelineConfiguration& configuration) = 0;
        virtual Framebuffer createFramebuffer(
            const RenderPass& renderPass,
            const FramebufferConfiguration& configuration) = 0;
        virtual CommandBuffer createCommandBuffer() = 0;

        virtual void copyBuffer(Buffer& dstBuffer, const Buffer& srcBuffer) = 0;
        virtual void copyBufferToImage(Image& dstImage, const Buffer& srcBuffer) = 0;

        virtual Format getColorFormat() const noexcept = 0;
        virtual Format getDepthFormat() const noexcept = 0;
        virtual uint32_t getCurrentImageIndex() const noexcept = 0;
        virtual const std::vector<Image>& getPresentImages() const noexcept = 0;

        virtual void waitDevice() = 0;
    };

}

#endif