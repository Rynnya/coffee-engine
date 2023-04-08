#include <coffee/graphics/pipeline.hpp>

#include <coffee/utils/log.hpp>
#include <coffee/utils/math.hpp>
#include <coffee/utils/vk_utils.hpp>

#include <array>

namespace coffee {

    PipelineImpl::PipelineImpl(
        Device& device,
        const RenderPass& renderPass,
        const std::vector<DescriptorLayout>& descriptorLayouts,
        const std::vector<Shader>& shaderPrograms,
        const PipelineConfiguration& configuration
    )
        : device_ { device }
    {
        VkPipelineLayoutCreateInfo createInfo { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

        std::vector<VkDescriptorSetLayout> setLayouts {};
        std::vector<VkPushConstantRange> pushConstantsRanges {};

        for (const auto& descriptorLayout : descriptorLayouts) {
            setLayouts.push_back(descriptorLayout->layout());
        }

        for (const auto& range : configuration.constantRanges) {
            VkPushConstantRange rangeImpl {};

            uint32_t roundedOffset = Math::roundToMultiple(range.offset, 4ULL);
            uint32_t roundedSize = Math::roundToMultiple(range.size, 4ULL);

            rangeImpl.stageFlags = range.stages;
            rangeImpl.offset = roundedOffset;
            rangeImpl.size = roundedSize;

            auto validateSize = [](const char* name, uint32_t originalSize, uint32_t roundedSize) {
                if (originalSize != roundedSize) {
                    COFFEE_WARNING(
                        "Provided {} {} isn't multiple of 4 and was rounded up to {}. This might cause a strange behaviour in your shaders.",
                        name, originalSize, roundedSize);
                }
            };

            validateSize("size", range.size, roundedSize);
            validateSize("offset", range.offset, roundedOffset);

            pushConstantsRanges.push_back(std::move(rangeImpl));
        }

        createInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
        createInfo.pSetLayouts = setLayouts.data();
        createInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantsRanges.size());
        createInfo.pPushConstantRanges = pushConstantsRanges.data();

        COFFEE_THROW_IF(
            vkCreatePipelineLayout(device_.logicalDevice(), &createInfo, nullptr, &layout_) != VK_SUCCESS,
            "Failed to create a pipeline layout!");

        std::vector<VkVertexInputBindingDescription> bindingDescriptions {};
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions {};

        for (const auto& inputBinding : configuration.inputBindings) {
            VkVertexInputBindingDescription bindingDescription {};

            bindingDescription.binding = inputBinding.binding;
            bindingDescription.stride = inputBinding.stride;
            bindingDescription.inputRate = inputBinding.inputRate;

            for (const auto& element : inputBinding.elements) {
                VkVertexInputAttributeDescription attributeDescription {};

                attributeDescription.binding = inputBinding.binding;
                attributeDescription.location = element.location;
                attributeDescription.format = element.format;
                attributeDescription.offset = element.offset;

                attributeDescriptions.push_back(std::move(attributeDescription));
            }

            bindingDescriptions.push_back(std::move(bindingDescription));
        }

        VkPipelineVertexInputStateCreateInfo vertexInfo { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        vertexInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
        vertexInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
        vertexInfo.pVertexBindingDescriptions = bindingDescriptions.data();

        VkPipelineViewportStateCreateInfo viewportInfo { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        viewportInfo.viewportCount = 1;
        viewportInfo.pViewports = nullptr;
        viewportInfo.scissorCount = 1;
        viewportInfo.pScissors = nullptr;

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        inputAssemblyInfo.topology = configuration.inputAssembly.topology;
        inputAssemblyInfo.primitiveRestartEnable = configuration.inputAssembly.primitiveRestartEnable ? VK_TRUE : VK_FALSE;

        VkPipelineRasterizationStateCreateInfo rasterizationInfo { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        rasterizationInfo.depthClampEnable = VK_FALSE;
        rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
        rasterizationInfo.polygonMode = configuration.rasterizationInfo.fillMode;
        rasterizationInfo.cullMode = configuration.rasterizationInfo.cullMode;
        rasterizationInfo.frontFace = configuration.rasterizationInfo.frontFace;
        rasterizationInfo.depthBiasEnable = configuration.rasterizationInfo.depthBiasEnable ? VK_TRUE : VK_FALSE;
        rasterizationInfo.depthBiasConstantFactor = configuration.rasterizationInfo.depthBiasConstantFactor;
        rasterizationInfo.depthBiasClamp = configuration.rasterizationInfo.depthBiasClamp;
        rasterizationInfo.depthBiasSlopeFactor = configuration.rasterizationInfo.depthBiasSlopeFactor;
        rasterizationInfo.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampleInfo { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        multisampleInfo.rasterizationSamples =
            VkUtils::getUsableSampleCount(configuration.multisampleInfo.sampleCount, device_.properties());
        multisampleInfo.sampleShadingEnable = configuration.multisampleInfo.sampleRateShading ? VK_TRUE : VK_FALSE;
        multisampleInfo.minSampleShading = configuration.multisampleInfo.minSampleShading;
        multisampleInfo.pSampleMask = nullptr;
        multisampleInfo.alphaToCoverageEnable = configuration.multisampleInfo.alphaToCoverage ? VK_TRUE : VK_FALSE;
        multisampleInfo.alphaToOneEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlendInfo { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        colorBlendInfo.logicOpEnable = configuration.colorBlendAttachment.logicOpEnable ? VK_TRUE : VK_FALSE;
        colorBlendInfo.logicOp = configuration.colorBlendAttachment.logicOp;

        VkPipelineColorBlendAttachmentState colorBlendAttachment {};
        colorBlendAttachment.blendEnable = configuration.colorBlendAttachment.blendEnable ? VK_TRUE : VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = configuration.colorBlendAttachment.srcColorBlendFactor;
        colorBlendAttachment.dstColorBlendFactor = configuration.colorBlendAttachment.dstColorBlendFactor;
        colorBlendAttachment.colorBlendOp = configuration.colorBlendAttachment.colorBlendOp;
        colorBlendAttachment.srcAlphaBlendFactor = configuration.colorBlendAttachment.srcAlphaBlendFactor;
        colorBlendAttachment.dstAlphaBlendFactor = configuration.colorBlendAttachment.dstAlphaBlendFactor;
        colorBlendAttachment.alphaBlendOp = configuration.colorBlendAttachment.alphaBlendOp;
        colorBlendAttachment.colorWriteMask = configuration.colorBlendAttachment.colorWriteMask;

        colorBlendInfo.attachmentCount = 1;
        colorBlendInfo.pAttachments = &colorBlendAttachment;
        colorBlendInfo.blendConstants[0] = 0.0f;
        colorBlendInfo.blendConstants[1] = 0.0f;
        colorBlendInfo.blendConstants[2] = 0.0f;
        colorBlendInfo.blendConstants[3] = 0.0f;

        VkPipelineDepthStencilStateCreateInfo depthStencilInfo { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        depthStencilInfo.depthTestEnable = configuration.depthStencilInfo.depthTestEnable ? VK_TRUE : VK_FALSE;
        depthStencilInfo.depthWriteEnable = configuration.depthStencilInfo.depthWriteEnable ? VK_TRUE : VK_FALSE;
        depthStencilInfo.depthCompareOp = configuration.depthStencilInfo.depthCompareOp;
        depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
        depthStencilInfo.stencilTestEnable = configuration.depthStencilInfo.stencilTestEnable ? VK_TRUE : VK_FALSE;
        depthStencilInfo.front = configuration.depthStencilInfo.frontFace;
        depthStencilInfo.back = configuration.depthStencilInfo.backFace;
        depthStencilInfo.minDepthBounds = 0.0f;
        depthStencilInfo.maxDepthBounds = 1.0f;

        constexpr std::array<VkDynamicState, 3> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT,
                                                                  VK_DYNAMIC_STATE_SCISSOR,
                                                                  VK_DYNAMIC_STATE_BLEND_CONSTANTS };

        VkPipelineDynamicStateCreateInfo dynamicStateInfo { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicStateInfo.pDynamicStates = dynamicStates.data();

        std::vector<VkPipelineShaderStageCreateInfo> shaderStages {};

        for (const auto& shader : shaderPrograms) {
            VkPipelineShaderStageCreateInfo shaderCreateInfo { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };

            shaderCreateInfo.stage = shader->stage;
            shaderCreateInfo.module = shader->shader();
            shaderCreateInfo.pName = shader->entrypoint.data();
            shaderCreateInfo.pSpecializationInfo = nullptr;

            shaderStages.push_back(std::move(shaderCreateInfo));
        }

        VkGraphicsPipelineCreateInfo pipelineInfo { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInfo;
        pipelineInfo.pViewportState = &viewportInfo;
        pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
        pipelineInfo.pRasterizationState = &rasterizationInfo;
        pipelineInfo.pMultisampleState = &multisampleInfo;
        pipelineInfo.pColorBlendState = &colorBlendInfo;
        pipelineInfo.pDepthStencilState = &depthStencilInfo;
        pipelineInfo.pDynamicState = &dynamicStateInfo;

        pipelineInfo.layout = layout_;
        pipelineInfo.renderPass = renderPass->renderPass();
        pipelineInfo.subpass = 0;

        pipelineInfo.basePipelineIndex = -1;
        pipelineInfo.basePipelineHandle = nullptr;

        COFFEE_THROW_IF(
            vkCreateGraphicsPipelines(device_.logicalDevice(), nullptr, 1, &pipelineInfo, nullptr, &pipeline_) != VK_SUCCESS,
            "Failed to create graphics pipeline!");
    }

    PipelineImpl::~PipelineImpl() noexcept
    {
        vkDestroyPipeline(device_.logicalDevice(), pipeline_, nullptr);
        vkDestroyPipelineLayout(device_.logicalDevice(), layout_, nullptr);
    }

} // namespace coffee