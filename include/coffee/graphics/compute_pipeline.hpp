#ifndef COFFEE_GRAPHICS_COMPUTE_PIPELINE
#define COFFEE_GRAPHICS_COMPUTE_PIPELINE

#include <coffee/graphics/descriptors.hpp>
#include <coffee/graphics/shader.hpp>

namespace coffee { namespace graphics {

    struct ComputePipelineConfiguration {
        ShaderPtr shader = nullptr;
        PushConstants pushConstants {};
        std::vector<SpecializationConstant> specializationConstants {};
        std::vector<DescriptorLayoutPtr> layouts {};
    };

    class ComputePipeline;
    using ComputePipelinePtr = std::shared_ptr<ComputePipeline>;

    class ComputePipeline {
    public:
        ~ComputePipeline() noexcept;

        static ComputePipelinePtr create(const DevicePtr& device, const ComputePipelineConfiguration& configuration);

        inline const VkPipelineLayout& layout() const noexcept { return layout_; }

        inline const VkPipeline& pipeline() const noexcept { return pipeline_; }

    private:
        ComputePipeline(const DevicePtr& device, const ComputePipelineConfiguration& configuration);

        void verifySize(const char* name, uint32_t originalSize, uint32_t alignedSize);

        DevicePtr device_;

        VkPipelineLayout layout_ = VK_NULL_HANDLE;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
    };

}} // namespace coffee::graphics

#endif
