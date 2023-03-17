#ifndef COFFEE_VK_PIPELINE
#define COFFEE_VK_PIPELINE

#include <coffee/abstract/vulkan/vk_device.hpp>
#include <coffee/abstract/descriptors.hpp>
#include <coffee/abstract/pipeline.hpp>
#include <coffee/abstract/render_pass.hpp>
#include <coffee/abstract/shader.hpp>

#include <set>

namespace coffee {

    class VulkanPipeline : public AbstractPipeline {
    public:
        VulkanPipeline(
            VulkanDevice& device,
            const RenderPass& renderPass,
            const std::vector<DescriptorLayout>& descriptorLayouts,
            const std::vector<Shader>& shaderPrograms,
            const PipelineConfiguration& configuration);
        ~VulkanPipeline() noexcept;

        VkPipelineLayout layout;
        VkPipeline pipeline;

    private:
        VulkanDevice& device_;
    };

}

#endif