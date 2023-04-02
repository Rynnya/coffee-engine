#include <coffee/abstract/vulkan/vk_pipeline.hpp>

#include <coffee/abstract/vulkan/vk_descriptors.hpp>
#include <coffee/abstract/vulkan/vk_render_pass.hpp>
#include <coffee/abstract/vulkan/vk_shader.hpp>

#include <coffee/utils/log.hpp>
#include <coffee/utils/math.hpp>
#include <coffee/utils/vk_utils.hpp>

#include <array>

namespace coffee {

    VulkanPipeline::VulkanPipeline(
        VulkanDevice& device,
        const RenderPass& renderPass,
        const std::vector<DescriptorLayout>& descriptorLayouts,
        const std::vector<Shader>& shaderPrograms,
        const PipelineConfiguration& configuration
    )
            : device_ { device } {
        VkPipelineLayoutCreateInfo createInfo { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

        std::vector<VkDescriptorSetLayout> setLayouts {};
        std::vector<VkPushConstantRange> pushConstantsRanges {};

        for (const auto& descriptorLayout : descriptorLayouts) {
            setLayouts.push_back(static_cast<VulkanDescriptorLayout*>(descriptorLayout.get())->layout);
        }

        for (const auto& range : configuration.ranges) {
            VkPushConstantRange rangeImpl {};

            uint32_t roundedOffset = Math::roundToMultiple(range.offset, 4ULL);
            uint32_t roundedSize = Math::roundToMultiple(range.size, 4ULL);

            rangeImpl.stageFlags = VkUtils::transformShaderStages(range.shaderStages);
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
            vkCreatePipelineLayout(device_.getLogicalDevice(), &createInfo, nullptr, &layout) != VK_SUCCESS,
            "Failed to create a pipeline layout!");

        std::vector<VkVertexInputBindingDescription> bindingDescriptions {};
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions {};

        for (const auto& inputBinding : configuration.inputBindings) {
            VkVertexInputBindingDescription bindingDescription {};

            bindingDescription.binding = inputBinding.binding;
            bindingDescription.stride = inputBinding.stride;
            bindingDescription.inputRate = VkUtils::transformInputRate(inputBinding.inputRate);

            for (const auto& element : inputBinding.elements) {
                VkVertexInputAttributeDescription attributeDescription {};

                attributeDescription.binding = inputBinding.binding;
                attributeDescription.location = element.location;
                attributeDescription.format = VkUtils::transformFormat(element.format);
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
        inputAssemblyInfo.topology = VkUtils::transformTopology(configuration.inputAssembly.topology);
        inputAssemblyInfo.primitiveRestartEnable = configuration.inputAssembly.primitiveRestartEnable ? VK_TRUE : VK_FALSE;

        VkPipelineRasterizationStateCreateInfo rasterizationInfo { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        rasterizationInfo.depthClampEnable = VK_FALSE;
        rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
        rasterizationInfo.polygonMode = VkUtils::transformPolygonMode(configuration.rasterizationInfo.fillMode);
        rasterizationInfo.frontFace = VkUtils::transformFrontFace(configuration.rasterizationInfo.frontFace);
        rasterizationInfo.depthBiasEnable = configuration.rasterizationInfo.depthBiasEnable ? VK_TRUE : VK_FALSE;
        rasterizationInfo.depthBiasConstantFactor = configuration.rasterizationInfo.depthBiasConstantFactor;
        rasterizationInfo.depthBiasClamp = configuration.rasterizationInfo.depthBiasClamp;
        rasterizationInfo.depthBiasSlopeFactor = configuration.rasterizationInfo.depthBiasSlopeFactor;
        rasterizationInfo.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampleInfo { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        multisampleInfo.rasterizationSamples =
            VkUtils::getUsableSampleCount(configuration.multisampleInfo.sampleCount, device_.getProperties());
        multisampleInfo.sampleShadingEnable = configuration.multisampleInfo.sampleRateShading ? VK_TRUE : VK_FALSE;
        multisampleInfo.minSampleShading = configuration.multisampleInfo.minSampleShading;
        multisampleInfo.pSampleMask = nullptr;
        multisampleInfo.alphaToCoverageEnable = configuration.multisampleInfo.alphaToCoverage ? VK_TRUE : VK_FALSE;
        multisampleInfo.alphaToOneEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachment {};
        colorBlendAttachment.blendEnable = configuration.colorBlendAttachment.blendEnable ? VK_TRUE : VK_FALSE;
        colorBlendAttachment.colorWriteMask = VkUtils::transformColorComponents(configuration.colorBlendAttachment.colorWriteMask);
        colorBlendAttachment.srcColorBlendFactor = VkUtils::transformBlendFactor(configuration.colorBlendAttachment.srcBlend);
        colorBlendAttachment.dstColorBlendFactor = VkUtils::transformBlendFactor(configuration.colorBlendAttachment.dstBlend);
        colorBlendAttachment.colorBlendOp = VkUtils::transformBlendOp(configuration.colorBlendAttachment.blendOp);
        colorBlendAttachment.srcAlphaBlendFactor = VkUtils::transformBlendFactor(configuration.colorBlendAttachment.alphaSrcBlend);
        colorBlendAttachment.dstAlphaBlendFactor = VkUtils::transformBlendFactor(configuration.colorBlendAttachment.alphaDstBlend);
        colorBlendAttachment.alphaBlendOp = VkUtils::transformBlendOp(configuration.colorBlendAttachment.alphaBlendOp);

        VkPipelineColorBlendStateCreateInfo colorBlendInfo { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        colorBlendInfo.logicOpEnable = configuration.colorBlendAttachment.logicOpEnable ? VK_TRUE : VK_FALSE;
        colorBlendInfo.logicOp = VkUtils::transformLogicOp(configuration.colorBlendAttachment.logicOp);
        colorBlendInfo.attachmentCount = 1;
        colorBlendInfo.pAttachments = &colorBlendAttachment;
        colorBlendInfo.blendConstants[0] = 0.0f;
        colorBlendInfo.blendConstants[1] = 0.0f;
        colorBlendInfo.blendConstants[2] = 0.0f;
        colorBlendInfo.blendConstants[3] = 0.0f;

        VkPipelineDepthStencilStateCreateInfo depthStencilInfo { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        depthStencilInfo.depthTestEnable = configuration.depthStencilInfo.depthTestEnable ? VK_TRUE : VK_FALSE;
        depthStencilInfo.depthWriteEnable = configuration.depthStencilInfo.depthWriteEnable ? VK_TRUE : VK_FALSE;
        depthStencilInfo.depthCompareOp = VkUtils::transformCompareOp(configuration.depthStencilInfo.depthCompareOp);
        depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
        depthStencilInfo.stencilTestEnable = configuration.depthStencilInfo.stencilTestEnable ? VK_TRUE : VK_FALSE;
        depthStencilInfo.front.failOp = VkUtils::transformStencilOp(configuration.depthStencilInfo.frontFace.failOp);
        depthStencilInfo.front.passOp = VkUtils::transformStencilOp(configuration.depthStencilInfo.frontFace.passOp);
        depthStencilInfo.front.depthFailOp = VkUtils::transformStencilOp(configuration.depthStencilInfo.frontFace.depthFailOp);
        depthStencilInfo.front.compareOp = VkUtils::transformCompareOp(configuration.depthStencilInfo.frontFace.compareOp);
        depthStencilInfo.front.compareMask = static_cast<uint32_t>(configuration.depthStencilInfo.stencilReadMask);
        depthStencilInfo.front.writeMask = static_cast<uint32_t>(configuration.depthStencilInfo.stencilWriteMask);
        depthStencilInfo.front.reference = configuration.depthStencilInfo.frontFace.reference;
        depthStencilInfo.back.failOp = VkUtils::transformStencilOp(configuration.depthStencilInfo.backFace.failOp);
        depthStencilInfo.back.passOp = VkUtils::transformStencilOp(configuration.depthStencilInfo.backFace.passOp);
        depthStencilInfo.back.depthFailOp = VkUtils::transformStencilOp(configuration.depthStencilInfo.backFace.depthFailOp);
        depthStencilInfo.back.compareOp = VkUtils::transformCompareOp(configuration.depthStencilInfo.backFace.compareOp);
        depthStencilInfo.back.compareMask = static_cast<uint32_t>(configuration.depthStencilInfo.stencilReadMask);
        depthStencilInfo.back.writeMask = static_cast<uint32_t>(configuration.depthStencilInfo.stencilWriteMask);
        depthStencilInfo.back.reference = configuration.depthStencilInfo.backFace.reference;
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

            shaderCreateInfo.stage = VkUtils::transformShaderStage(shader->getStage());
            shaderCreateInfo.module = static_cast<VulkanShader*>(shader.get())->shader;
            shaderCreateInfo.pName = shader->getEntrypointName().data();
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

        pipelineInfo.layout = layout;
        pipelineInfo.renderPass = static_cast<VulkanRenderPass*>(renderPass.get())->renderPass;
        pipelineInfo.subpass = 0;

        pipelineInfo.basePipelineIndex = -1;
        pipelineInfo.basePipelineHandle = nullptr;

        COFFEE_THROW_IF(
            vkCreateGraphicsPipelines(device_.getLogicalDevice(), nullptr, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS,
            "Failed to create graphics pipeline!");
    }

    VulkanPipeline::~VulkanPipeline() noexcept {
        vkDestroyPipeline(device_.getLogicalDevice(), pipeline, nullptr);
        vkDestroyPipelineLayout(device_.getLogicalDevice(), layout, nullptr);
    }

} // namespace coffee