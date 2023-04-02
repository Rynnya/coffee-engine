#ifndef COFFEE_ABSTRACT_BACKEND
#define COFFEE_ABSTRACT_BACKEND

#include <coffee/abstract/buffer.hpp>
#include <coffee/abstract/command_buffer.hpp>
#include <coffee/abstract/descriptors.hpp>
#include <coffee/abstract/framebuffer.hpp>
#include <coffee/abstract/image.hpp>
#include <coffee/abstract/pipeline.hpp>
#include <coffee/abstract/render_pass.hpp>
#include <coffee/abstract/sampler.hpp>
#include <coffee/abstract/shader.hpp>
#include <coffee/abstract/window.hpp>

#include <set>

namespace coffee {

    class AbstractBackend : NonMoveable {
    public:
        AbstractBackend(BackendAPI backend) noexcept;
        virtual ~AbstractBackend() noexcept = default;

        BackendAPI getBackendType() const noexcept;
        virtual size_t getSwapChainImageCount() const noexcept = 0;
        virtual Format getSurfaceColorFormat() const noexcept = 0;
        virtual Format getOptimalDepthFormat() const noexcept = 0;

        virtual Window createWindow(WindowSettings settings, const std::string& windowName) = 0;

        virtual Buffer createBuffer(const BufferConfiguration& configuration) = 0;
        virtual Image createImage(const ImageConfiguration& configuration) = 0;
        virtual Sampler createSampler(const SamplerConfiguration& configuration) = 0;

        virtual Shader createShader(const std::string& fileName, ShaderStage stage, const std::string& entrypoint) = 0;
        virtual Shader createShader(const std::vector<uint8_t>& bytes, ShaderStage stage, const std::string& entrypoint) = 0;

        virtual DescriptorLayout createDescriptorLayout(const std::map<uint32_t, DescriptorBindingInfo>& bindings) = 0;
        virtual DescriptorSet createDescriptorSet(const DescriptorWriter& writer) = 0;

        virtual RenderPass createRenderPass(const RenderPass& parent, const RenderPassConfiguration& configuration) = 0;
        virtual Pipeline createPipeline(
            const RenderPass& renderPass,
            const std::vector<DescriptorLayout>& descriptorLayouts,
            const std::vector<Shader>& shaderPrograms,
            const PipelineConfiguration& configuration
        ) = 0;
        virtual Framebuffer createFramebuffer(const RenderPass& renderPass, const FramebufferConfiguration& configuration) = 0;
        virtual GraphicsCommandBuffer createCommandBuffer() = 0;

        virtual void copyBuffer(Buffer& dstBuffer, const Buffer& srcBuffer) = 0;
        virtual void copyBufferToImage(Image& dstImage, const Buffer& srcBuffer) = 0;

        virtual void sendCommandBuffers(std::vector<GraphicsCommandBuffer>&& commandBuffers) = 0;
        virtual void submitPendingWork() = 0;

        virtual void waitDevice() = 0;

    private:
        BackendAPI backend_;
    };

} // namespace coffee

#endif