#ifndef COFFEE_GRAPHICS_PIPELINE
#define COFFEE_GRAPHICS_PIPELINE

#include <coffee/graphics/descriptors.hpp>
#include <coffee/graphics/render_pass.hpp>
#include <coffee/graphics/shader.hpp>

#include <set>

namespace coffee {

    namespace graphics {

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

        class SpecializationConstant {
        public:
            inline SpecializationConstant(uint32_t id, bool value)
                : constantID { id }
                , dataSize { sizeof(bool) }
                , rawData { static_cast<uint64_t>(value) }
            {}

            inline SpecializationConstant(uint32_t id, int8_t value)
                : constantID { id }
                , dataSize { sizeof(int8_t) }
                , rawData { static_cast<uint64_t>(value) }
            {}

            inline SpecializationConstant(uint32_t id, uint8_t value)
                : constantID { id }
                , dataSize { sizeof(uint8_t) }
                , rawData { static_cast<uint64_t>(value) }
            {}

            inline SpecializationConstant(uint32_t id, int16_t value)
                : constantID { id }
                , dataSize { sizeof(int16_t) }
                , rawData { static_cast<uint64_t>(value) }
            {}

            inline SpecializationConstant(uint32_t id, uint16_t value)
                : constantID { id }
                , dataSize { sizeof(uint16_t) }
                , rawData { static_cast<uint64_t>(value) }
            {}

            inline SpecializationConstant(uint32_t id, int32_t value)
                : constantID { id }
                , dataSize { sizeof(int32_t) }
                , rawData { static_cast<uint64_t>(value) }
            {}

            inline SpecializationConstant(uint32_t id, uint32_t value)
                : constantID { id }
                , dataSize { sizeof(uint32_t) }
                , rawData { static_cast<uint64_t>(value) }
            {}

            inline SpecializationConstant(uint32_t id, int64_t value)
                : constantID { id }
                , dataSize { sizeof(int64_t) }
                , rawData { static_cast<uint64_t>(value) }
            {}

            inline SpecializationConstant(uint32_t id, uint64_t value)
                : constantID { id }
                , dataSize { sizeof(uint64_t) }
                , rawData { static_cast<uint64_t>(value) }
            {}

            inline SpecializationConstant(uint32_t id, float value)
                : constantID { id }
                , dataSize { sizeof(float) }
                , rawData { static_cast<uint64_t>(*reinterpret_cast<uint32_t*>(&value)) }
            {}

            inline SpecializationConstant(uint32_t id, double value)
                : constantID { id }
                , dataSize { sizeof(double) }
                , rawData { *reinterpret_cast<uint64_t*>(&value) }
            {}

        private:
            uint32_t constantID = 0;
            uint32_t dataSize = 0;
            uint64_t rawData = 0;

            friend class Pipeline;
        };

        struct PipelineConfiguration {
            std::optional<ShaderPtr> vertexShader = std::nullopt;
            std::optional<ShaderPtr> fragmentShader = std::nullopt;
            std::optional<ShaderPtr> computeShader = std::nullopt;
            std::vector<DescriptorLayoutPtr> layouts {};
            std::vector<InputBinding> inputBindings {};
            std::vector<PushConstantRange> constantRanges {};
            InputAssembly inputAssembly {};
            RasterizerationInfo rasterizationInfo {};
            MultisampleInfo multisampleInfo {};
            ColorBlendAttachment colorBlendAttachment {};
            DepthStencilInfo depthStencilInfo {};
            std::vector<SpecializationConstant> vertexSpecializationConstants {};
            std::vector<SpecializationConstant> fragmentSpecializationConstants {};
            std::vector<SpecializationConstant> computeSpecializationConstants {};
        };

        class Pipeline;
        using PipelinePtr = std::shared_ptr<Pipeline>;

        class Pipeline {
        public:
            ~Pipeline() noexcept;

            static PipelinePtr create(const DevicePtr& device, const RenderPassPtr& renderPass, const PipelineConfiguration& configuration);

            inline const VkPipelineLayout& layout() const noexcept { return layout_; }

            inline const VkPipeline& pipeline() const noexcept { return pipeline_; }

        private:
            Pipeline(const DevicePtr& device, const RenderPassPtr& renderPass, const PipelineConfiguration& configuration);

            DevicePtr device_;

            VkPipelineLayout layout_ = VK_NULL_HANDLE;
            VkPipeline pipeline_ = VK_NULL_HANDLE;
        };

    } // namespace graphics

} // namespace coffee

#endif