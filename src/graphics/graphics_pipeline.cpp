#include <coffee/graphics/graphics_pipeline.hpp>

#include <coffee/graphics/exceptions.hpp>
#include <coffee/utils/log.hpp>
#include <coffee/utils/math.hpp>
#include <coffee/utils/vk_utils.hpp>

#include <array>

namespace coffee {

    namespace graphics {

        GraphicsPipeline::GraphicsPipeline(const DevicePtr& device, const RenderPassPtr& renderPass, const GraphicsPipelineConfiguration& config)
            : device_ { device }
        {
            VkResult result = VK_SUCCESS;
            VkPipelineLayoutCreateInfo createInfo { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

            std::vector<VkDescriptorSetLayout> setLayouts {};

            for (const auto& descriptorLayout : config.layouts) {
                setLayouts.push_back(descriptorLayout->layout());
            }

            uint32_t pushConstantsSize = 0;
            std::array<VkPushConstantRange, 2> pushConstantsRanges {};

            if (config.vertexPushConstants.size > 0) {
                size_t alignedSize = Math::roundToMultiple(config.vertexPushConstants.size, 4ULL);
                size_t alignedOffset = Math::roundToMultiple(config.vertexPushConstants.offset, 4ULL);

                verifySize("size", config.vertexPushConstants.size, alignedSize);
                verifySize("offset", config.vertexPushConstants.offset, alignedOffset);

                if (alignedSize > 128 || alignedSize + alignedOffset > 128) {
                    COFFEE_WARNING(
                        "Specification only allow us to use up to 128 bytes of push constants, while you requested {} with offset {}. "
                        "It's generally not recommended to overpass this limit, as it might cause crash on some devices.", 
                        config.vertexPushConstants.size, config.vertexPushConstants.offset
                    );
                }

                auto& pushConstants = pushConstantsRanges[pushConstantsSize];
                pushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
                pushConstants.size = static_cast<uint32_t>(alignedSize);
                pushConstants.offset = static_cast<uint32_t>(alignedOffset);

                pushConstantsSize++;
            }

            if (config.fragmentPushConstants.size > 0) {
                size_t alignedSize = Math::roundToMultiple(config.fragmentPushConstants.size, 4ULL);
                size_t alignedOffset = Math::roundToMultiple(config.fragmentPushConstants.offset, 4ULL);

                verifySize("size", config.fragmentPushConstants.size, alignedSize);
                verifySize("offset", config.fragmentPushConstants.offset, alignedOffset);

                if (alignedSize > 128 || alignedSize + alignedOffset > 128) {
                    COFFEE_WARNING(
                        "Specification only allow us to use up to 128 bytes of push constants, while you requested {} with offset {}. "
                        "It's generally not recommended to overpass this limit, as it might cause crash on some devices.", 
                        config.fragmentPushConstants.size, config.fragmentPushConstants.offset
                    );
                }

                auto& pushConstants = pushConstantsRanges[pushConstantsSize];
                pushConstants.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
                pushConstants.size = static_cast<uint32_t>(alignedSize);
                pushConstants.offset = static_cast<uint32_t>(alignedOffset);

                pushConstantsSize++;
            }

            createInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
            createInfo.pSetLayouts = setLayouts.data();
            createInfo.pushConstantRangeCount = pushConstantsSize;
            createInfo.pPushConstantRanges = pushConstantsRanges.data();

            if ((result = vkCreatePipelineLayout(device_->logicalDevice(), &createInfo, nullptr, &layout_)) != VK_SUCCESS) {
                COFFEE_ERROR("Failed to create a pipeline layout!");

                throw RegularVulkanException { result };
            }

            std::vector<VkVertexInputBindingDescription> bindingDescriptions {};
            std::vector<VkVertexInputAttributeDescription> attributeDescriptions {};

            for (const auto& inputBinding : config.inputBindings) {
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
            inputAssemblyInfo.topology = config.inputAssembly.topology;
            inputAssemblyInfo.primitiveRestartEnable = config.inputAssembly.primitiveRestartEnable ? VK_TRUE : VK_FALSE;

            VkPipelineRasterizationStateCreateInfo rasterizationInfo { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
            rasterizationInfo.depthClampEnable = VK_FALSE;
            rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
            rasterizationInfo.polygonMode = config.rasterizationInfo.fillMode;
            rasterizationInfo.cullMode = config.rasterizationInfo.cullMode;
            rasterizationInfo.frontFace = config.rasterizationInfo.frontFace;
            rasterizationInfo.depthBiasEnable = config.rasterizationInfo.depthBiasEnable ? VK_TRUE : VK_FALSE;
            rasterizationInfo.depthBiasConstantFactor = config.rasterizationInfo.depthBiasConstantFactor;
            rasterizationInfo.depthBiasClamp = config.rasterizationInfo.depthBiasClamp;
            rasterizationInfo.depthBiasSlopeFactor = config.rasterizationInfo.depthBiasSlopeFactor;
            rasterizationInfo.lineWidth = 1.0f;

            VkPipelineMultisampleStateCreateInfo multisampleInfo { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
            multisampleInfo.rasterizationSamples = VkUtils::getUsableSampleCount(config.multisampleInfo.sampleCount, device_->properties());
            multisampleInfo.sampleShadingEnable = config.multisampleInfo.sampleRateShading ? VK_TRUE : VK_FALSE;
            multisampleInfo.minSampleShading = config.multisampleInfo.minSampleShading;
            multisampleInfo.pSampleMask = nullptr;
            multisampleInfo.alphaToCoverageEnable = config.multisampleInfo.alphaToCoverage ? VK_TRUE : VK_FALSE;
            multisampleInfo.alphaToOneEnable = VK_FALSE;

            VkPipelineColorBlendStateCreateInfo colorBlendInfo { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
            colorBlendInfo.logicOpEnable = config.colorBlendAttachment.logicOpEnable ? VK_TRUE : VK_FALSE;
            colorBlendInfo.logicOp = config.colorBlendAttachment.logicOp;

            VkPipelineColorBlendAttachmentState colorBlendAttachment {};
            colorBlendAttachment.blendEnable = config.colorBlendAttachment.blendEnable ? VK_TRUE : VK_FALSE;
            colorBlendAttachment.srcColorBlendFactor = config.colorBlendAttachment.srcColorBlendFactor;
            colorBlendAttachment.dstColorBlendFactor = config.colorBlendAttachment.dstColorBlendFactor;
            colorBlendAttachment.colorBlendOp = config.colorBlendAttachment.colorBlendOp;
            colorBlendAttachment.srcAlphaBlendFactor = config.colorBlendAttachment.srcAlphaBlendFactor;
            colorBlendAttachment.dstAlphaBlendFactor = config.colorBlendAttachment.dstAlphaBlendFactor;
            colorBlendAttachment.alphaBlendOp = config.colorBlendAttachment.alphaBlendOp;
            colorBlendAttachment.colorWriteMask = config.colorBlendAttachment.colorWriteMask;

            colorBlendInfo.attachmentCount = 1;
            colorBlendInfo.pAttachments = &colorBlendAttachment;
            colorBlendInfo.blendConstants[0] = 0.0f;
            colorBlendInfo.blendConstants[1] = 0.0f;
            colorBlendInfo.blendConstants[2] = 0.0f;
            colorBlendInfo.blendConstants[3] = 0.0f;

            VkPipelineDepthStencilStateCreateInfo depthStencilInfo { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
            depthStencilInfo.depthTestEnable = config.depthStencilInfo.depthTestEnable ? VK_TRUE : VK_FALSE;
            depthStencilInfo.depthWriteEnable = config.depthStencilInfo.depthWriteEnable ? VK_TRUE : VK_FALSE;
            depthStencilInfo.depthCompareOp = config.depthStencilInfo.depthCompareOp;
            depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
            depthStencilInfo.stencilTestEnable = config.depthStencilInfo.stencilTestEnable ? VK_TRUE : VK_FALSE;
            depthStencilInfo.front = config.depthStencilInfo.frontFace;
            depthStencilInfo.back = config.depthStencilInfo.backFace;
            depthStencilInfo.minDepthBounds = 0.0f;
            depthStencilInfo.maxDepthBounds = 1.0f;

            constexpr std::array<VkDynamicState, 3> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT,
                                                                      VK_DYNAMIC_STATE_SCISSOR,
                                                                      VK_DYNAMIC_STATE_BLEND_CONSTANTS };

            VkPipelineDynamicStateCreateInfo dynamicStateInfo { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
            dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
            dynamicStateInfo.pDynamicStates = dynamicStates.data();

            std::vector<VkPipelineShaderStageCreateInfo> shaderStages {};

            VkSpecializationInfo vertexSpecializationInfo {};
            VkSpecializationInfo fragmentSpecializationInfo {};
            VkSpecializationInfo computeSpecializationInfo {};
            std::unique_ptr<VkSpecializationMapEntry[]> vertexEntries = nullptr;
            std::unique_ptr<VkSpecializationMapEntry[]> fragmentEntries = nullptr;
            std::unique_ptr<VkSpecializationMapEntry[]> computeEntries = nullptr;
            std::unique_ptr<char[]> vertexData = nullptr;
            std::unique_ptr<char[]> fragmentData = nullptr;
            std::unique_ptr<char[]> computeData = nullptr;

            if (config.vertexShader != nullptr) {
                VkPipelineShaderStageCreateInfo shaderCreateInfo { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };

                if (!config.vertexSpecializationConstants.empty()) {
                    uint32_t totalDataSize = 0;
                    uint32_t dataOffset = 0;

                    for (const auto& constant : config.vertexSpecializationConstants) {
                        totalDataSize += static_cast<uint32_t>(constant.dataSize == sizeof(bool) ? sizeof(VkBool32) : constant.dataSize);
                    }

                    vertexEntries = std::make_unique<VkSpecializationMapEntry[]>(config.vertexSpecializationConstants.size());
                    vertexData = std::make_unique<char[]>(totalDataSize);

                    for (size_t index = 0; index < config.vertexSpecializationConstants.size(); index++) {
                        const auto& constant = config.vertexSpecializationConstants[index];
                        uint32_t constantSize = static_cast<uint32_t>(constant.dataSize == sizeof(bool) ? sizeof(VkBool32) : constant.dataSize);

                        vertexEntries[index].constantID = constant.constantID;
                        vertexEntries[index].offset = dataOffset;
                        vertexEntries[index].size = constantSize;

                        std::memcpy(vertexData.get() + dataOffset, &constant.rawData, constant.dataSize);
                        dataOffset += constantSize;
                    }

                    vertexSpecializationInfo.mapEntryCount = static_cast<uint32_t>(config.vertexSpecializationConstants.size());
                    vertexSpecializationInfo.pMapEntries = vertexEntries.get();
                    vertexSpecializationInfo.dataSize = totalDataSize;
                    vertexSpecializationInfo.pData = vertexData.get();
                }

                shaderCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
                shaderCreateInfo.module = config.vertexShader->shader();
                shaderCreateInfo.pName = config.vertexShader->entrypoint.data();
                shaderCreateInfo.pSpecializationInfo = &vertexSpecializationInfo;

                shaderStages.push_back(std::move(shaderCreateInfo));
            }

            if (config.fragmentShader != nullptr) {
                VkPipelineShaderStageCreateInfo shaderCreateInfo { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };

                if (!config.fragmentSpecializationConstants.empty()) {
                    uint32_t totalDataSize = 0;
                    uint32_t dataOffset = 0;

                    for (const auto& constant : config.fragmentSpecializationConstants) {
                        totalDataSize += static_cast<uint32_t>(constant.dataSize == sizeof(bool) ? sizeof(VkBool32) : constant.dataSize);
                    }

                    fragmentEntries = std::make_unique<VkSpecializationMapEntry[]>(config.fragmentSpecializationConstants.size());
                    fragmentData = std::make_unique<char[]>(totalDataSize);

                    for (size_t index = 0; index < config.fragmentSpecializationConstants.size(); index++) {
                        const auto& constant = config.fragmentSpecializationConstants[index];
                        uint32_t constantSize = static_cast<uint32_t>(constant.dataSize == sizeof(bool) ? sizeof(VkBool32) : constant.dataSize);

                        fragmentEntries[index].constantID = constant.constantID;
                        fragmentEntries[index].offset = dataOffset;
                        fragmentEntries[index].size = constantSize;

                        std::memcpy(fragmentData.get() + dataOffset, &constant.rawData, constant.dataSize);
                        dataOffset += constantSize;
                    }

                    fragmentSpecializationInfo.mapEntryCount = static_cast<uint32_t>(config.fragmentSpecializationConstants.size());
                    fragmentSpecializationInfo.pMapEntries = fragmentEntries.get();
                    fragmentSpecializationInfo.dataSize = totalDataSize;
                    fragmentSpecializationInfo.pData = fragmentData.get();
                }

                shaderCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                shaderCreateInfo.module = config.fragmentShader->shader();
                shaderCreateInfo.pName = config.fragmentShader->entrypoint.data();
                shaderCreateInfo.pSpecializationInfo = &fragmentSpecializationInfo;

                shaderStages.push_back(std::move(shaderCreateInfo));
            }

            COFFEE_ASSERT(!shaderStages.empty(), "Shaders wasn't provided, at least one shader must be provided.");

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
            pipelineInfo.basePipelineHandle = nullptr;
            pipelineInfo.basePipelineIndex = -1;

            if ((result = vkCreateGraphicsPipelines(device_->logicalDevice(), nullptr, 1, &pipelineInfo, nullptr, &pipeline_)) != VK_SUCCESS) {
                COFFEE_ERROR("Failed to create graphics pipeline!");

                throw RegularVulkanException { result };
            }
        }

        void GraphicsPipeline::verifySize(const char* name, uint32_t originalSize, uint32_t alignedSize)
        {
            if (originalSize != alignedSize) {
                COFFEE_WARNING(
                    "Provided {} with size {} isn't multiple of 4 and was rounded up to {}. "
                    "This might cause a strange behaviour in your shaders.",
                    name, originalSize, alignedSize
                );
            }
        }

        GraphicsPipeline::~GraphicsPipeline() noexcept
        {
            vkDestroyPipeline(device_->logicalDevice(), pipeline_, nullptr);
            vkDestroyPipelineLayout(device_->logicalDevice(), layout_, nullptr);
        }

        GraphicsPipelinePtr GraphicsPipeline::create(
            const DevicePtr& device,
            const RenderPassPtr& renderPass,
            const GraphicsPipelineConfiguration& configuration
        )
        {
            COFFEE_ASSERT(device != nullptr, "Invalid device provided.");

            [[maybe_unused]] constexpr auto verifyDescriptorLayouts = [](const std::vector<DescriptorLayoutPtr>& layouts) noexcept -> bool {
                bool result = true;

                for (const auto& layout : layouts) {
                    result &= (layout != nullptr);
                }

                return result;
            };

            COFFEE_ASSERT(renderPass != nullptr, "Invalid renderPass provided.");
            COFFEE_ASSERT(verifyDescriptorLayouts(configuration.layouts), "Invalid std::vector<DescriptorLayoutPtr> provided.");

            return std::shared_ptr<GraphicsPipeline>(new GraphicsPipeline { device, renderPass, configuration });
        }

    } // namespace graphics

} // namespace coffee