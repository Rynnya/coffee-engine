#include <coffee/abstract/vulkan/vk_backend.hpp>

#include <coffee/abstract/vulkan/vk_buffer.hpp>
#include <coffee/abstract/vulkan/vk_command_buffer.hpp>
#include <coffee/abstract/vulkan/vk_descriptors.hpp>
#include <coffee/abstract/vulkan/vk_pipeline.hpp>
#include <coffee/abstract/vulkan/vk_render_pass.hpp>
#include <coffee/abstract/vulkan/vk_sampler.hpp>
#include <coffee/abstract/vulkan/vk_shader.hpp>
#include <coffee/abstract/vulkan/vk_window.hpp>

#include <coffee/utils/log.hpp>
#include <coffee/utils/utils.hpp>
#include <coffee/utils/vk_utils.hpp>

namespace coffee {

    VulkanBackend::VulkanBackend() noexcept : AbstractBackend { BackendAPI::Vulkan } {}

    Window VulkanBackend::createWindow(WindowSettings settings, const std::string& windowName) {
        return std::make_unique<VulkanWindow>(device_, settings, windowName);
    }

    std::shared_ptr<AbstractBuffer> VulkanBackend::createBuffer(const BufferConfiguration& configuration) {
        return std::make_shared<VulkanBuffer>(device_, configuration);
    }

    std::shared_ptr<AbstractImage> VulkanBackend::createImage(const ImageConfiguration& configuration) {
        return std::make_shared<VulkanImage>(device_, configuration);
    }

    std::shared_ptr<AbstractSampler> VulkanBackend::createSampler(const SamplerConfiguration& configuration) {
        return std::make_shared<VulkanSampler>(device_, configuration);
    }

    std::unique_ptr<AbstractShader> VulkanBackend::createShader(
        const std::string& fileName,
        ShaderStage stage,
        const std::string& entrypoint
    ) {
        return std::make_unique<VulkanShader>(device_, Utils::readFile(fileName), stage, entrypoint);
    }

    std::unique_ptr<AbstractShader> VulkanBackend::createShader(
        const std::vector<uint8_t>& bytes,
        ShaderStage stage,
        const std::string& entrypoint
    ) {
        return std::make_unique<VulkanShader>(device_, bytes, stage, entrypoint);
    }

    std::shared_ptr<AbstractDescriptorLayout> VulkanBackend::createDescriptorLayout(
        const std::map<uint32_t, DescriptorBindingInfo>& bindings
    ) {
        return std::make_shared<VulkanDescriptorLayout>(device_, bindings);
    }

    std::shared_ptr<AbstractDescriptorSet> VulkanBackend::createDescriptorSet(const DescriptorWriter& writer) {
        return std::make_shared<VulkanDescriptorSet>(device_, writer);
    }

    std::unique_ptr<AbstractRenderPass> VulkanBackend::createRenderPass(
        const RenderPass& parent,
        const RenderPassConfiguration& configuration
    ) {
        return std::make_unique<VulkanRenderPass>(device_, configuration, parent);
    }

    std::unique_ptr<AbstractPipeline> VulkanBackend::createPipeline(
        const RenderPass& renderPass,
        const std::vector<DescriptorLayout>& descriptorLayouts,
        const std::vector<Shader>& shaderPrograms,
        const PipelineConfiguration& configuration
    ) {
        return std::make_unique<VulkanPipeline>(device_, renderPass, descriptorLayouts, shaderPrograms, configuration);
    }

    std::unique_ptr<AbstractFramebuffer> VulkanBackend::createFramebuffer(
        const RenderPass& renderPass, 
        const FramebufferConfiguration& configuration
    ) {
        return std::make_unique<VulkanFramebuffer>(device_, renderPass, configuration);
    }

    std::unique_ptr<AbstractCommandBuffer> VulkanBackend::createCommandBuffer() {
        return std::make_unique<VulkanCommandBuffer>(device_);
    }

    void VulkanBackend::copyBuffer(Buffer& dstBuffer, const Buffer& srcBuffer) {
        device_.copyBuffer(srcBuffer, dstBuffer);
    }

    void VulkanBackend::copyBufferToImage(Image& dstImage, const Buffer& srcBuffer) {
        device_.copyBufferToImage(srcBuffer, dstImage);
    }

    void VulkanBackend::waitDevice() {
        vkDeviceWaitIdle(device_.getLogicalDevice());
    }

}