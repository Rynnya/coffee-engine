#ifndef COFFEE_GRAPHICS_PIPELINE
#define COFFEE_GRAPHICS_PIPELINE

#include <coffee/graphics/descriptors.hpp>
#include <coffee/graphics/render_pass.hpp>
#include <coffee/graphics/shader.hpp>

#include <set>

namespace coffee {

    struct PushConstantRange {
        VkShaderStageFlags stages = 0;
        uint32_t size = 0U;
        uint32_t offset = 0U;
    };

    struct InputElement {
        // Defines a input slot inside input assembler
        uint32_t location = 0U;
        // Defines it's type and size
        VkFormat format = VK_FORMAT_UNDEFINED;
        // Offset over others objects (use offsetof() macro to calculate it properly)
        uint32_t offset = 0U;
    };

    struct InputBinding {
        // Binding number
        uint32_t binding = 0U;
        // Must be same as struct size (use sizeof() to calculate it properly)
        uint32_t stride = 0U;
        // Applies to every object inside 'elements'
        VkVertexInputRate inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        // Look at InputElement struct for better information
        std::vector<InputElement> elements {};
    };

    struct InputAssembly {
        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        // On every implementation only 32-bit indices allowed. Using 16-bit indices is undefined behaviour
        bool primitiveRestartEnable = false;
    };

    struct RasterizerationInfo {
        VkCullModeFlags cullMode = VK_CULL_MODE_NONE;
        VkPolygonMode fillMode = VK_POLYGON_MODE_FILL;
        VkFrontFace frontFace = VK_FRONT_FACE_CLOCKWISE;
        bool depthBiasEnable = false;
        float depthBiasConstantFactor = 0.0f;
        float depthBiasClamp = 0.0f;
        float depthBiasSlopeFactor = 0.0f;
    };

    struct MultisampleInfo {
        bool sampleRateShading = false;
        float minSampleShading = 1.0f;
        VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
        bool alphaToCoverage = false;
    };

    struct ColorBlendAttachment {
        bool logicOpEnable = false;
        VkLogicOp logicOp = VK_LOGIC_OP_COPY;
        bool blendEnable = false;
        VkBlendOp colorBlendOp = VK_BLEND_OP_ADD;
        VkBlendFactor srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        VkBlendFactor dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        VkBlendOp alphaBlendOp = VK_BLEND_OP_ADD;
        VkBlendFactor srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        VkBlendFactor dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        VkColorComponentFlags colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    };

    struct DepthStencilInfo {
        bool depthTestEnable = true;
        bool depthWriteEnable = true;
        VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        bool stencilTestEnable = false;
        VkStencilOpState frontFace {};
        VkStencilOpState backFace {};
    };

    struct PipelineConfiguration {
        std::vector<ShaderPtr> shaders {};
        std::vector<DescriptorLayoutPtr> layouts {};
        std::vector<InputBinding> inputBindings {};
        std::vector<PushConstantRange> constantRanges {};
        InputAssembly inputAssembly {};
        RasterizerationInfo rasterizationInfo {};
        MultisampleInfo multisampleInfo {};
        ColorBlendAttachment colorBlendAttachment {};
        DepthStencilInfo depthStencilInfo {};
    };

    class Pipeline;
    using PipelinePtr = std::shared_ptr<Pipeline>;

    class Pipeline {
    public:
        ~Pipeline() noexcept;

        static PipelinePtr create(const GPUDevicePtr& device, const RenderPassPtr& renderPass, const PipelineConfiguration& configuration);

        inline const VkPipelineLayout& layout() const noexcept
        {
            return layout_;
        }

        inline const VkPipeline& pipeline() const noexcept
        {
            return pipeline_;
        }

    private:
        Pipeline(const GPUDevicePtr& device, const RenderPassPtr& renderPass, const PipelineConfiguration& configuration);

        GPUDevicePtr device_;

        VkPipelineLayout layout_ = VK_NULL_HANDLE;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
    };

    

} // namespace coffee

#endif