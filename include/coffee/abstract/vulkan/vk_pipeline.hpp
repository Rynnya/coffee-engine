#ifndef COFFEE_VK_PIPELINE
#define COFFEE_VK_PIPELINE

#include <coffee/abstract/vulkan/vk_device.hpp>
#include <coffee/abstract/descriptors.hpp>
#include <coffee/abstract/pipeline.hpp>
#include <coffee/abstract/push_constant.hpp>
#include <coffee/abstract/render_pass.hpp>
#include <coffee/abstract/shader_program.hpp>

namespace coffee {

    class VulkanPipeline : public AbstractPipeline {
    public:
        VulkanPipeline(
            VulkanDevice& device,
            const RenderPass& renderPass,
            const PushConstants& pushConstants,
            const std::vector<DescriptorLayout>& descriptorLayouts,
            const ShaderProgram& shaderProgram,
            const PipelineConfiguration& configuration);
        ~VulkanPipeline() noexcept;

        VkPipelineLayout layout;
        VkPipeline pipeline;

    private:
        void createPipelineLayout(
            const PushConstants& pushConstants,
            const std::vector<DescriptorLayout>& descriptorLayouts);
        void createPipeline(
            const RenderPass& renderPass,
            const ShaderProgram& shaderProgram,
            const PipelineConfiguration& configuration);

        VulkanDevice& device_;
    };

}

#endif