#include <coffee/graphics/compute_pipeline.hpp>

#include <coffee/graphics/exceptions.hpp>
#include <coffee/utils/log.hpp>
#include <coffee/utils/math.hpp>
#include <coffee/utils/vk_utils.hpp>

namespace coffee {

namespace graphics {

    ComputePipeline::ComputePipeline(const DevicePtr& device, const ComputePipelineConfiguration& configuration) : device_ { device }
    {
        VkResult result = VK_SUCCESS;
        VkPipelineLayoutCreateInfo createInfo { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

        std::vector<VkDescriptorSetLayout> setLayouts {};

        for (const auto& descriptorLayout : configuration.layouts) {
            setLayouts.push_back(descriptorLayout->layout());
        }

        bool withPushConstants = false;
        VkPushConstantRange pushConstantRange {};

        if (configuration.pushConstants.size > 0) {
            size_t alignedSize = Math::roundToMultiple(configuration.pushConstants.size, 4ULL);
            size_t alignedOffset = Math::roundToMultiple(configuration.pushConstants.offset, 4ULL);

            verifySize("size", configuration.pushConstants.size, alignedSize);
            verifySize("offset", configuration.pushConstants.offset, alignedOffset);

            if (alignedSize > 128 || alignedSize + alignedOffset > 128) {
                COFFEE_WARNING(
                        "Specification only allow us to use up to 128 bytes of push constants, while you requested {} with offset {}. "
                        "It's generally not recommended to overpass this limit, as it might cause crash on some devices.", 
                        configuration.pushConstants.size, configuration.pushConstants.offset
                    );
            }

            pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            pushConstantRange.size = static_cast<uint32_t>(alignedSize);
            pushConstantRange.offset = static_cast<uint32_t>(alignedOffset);

            withPushConstants = true;
        }

        createInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
        createInfo.pSetLayouts = setLayouts.data();
        createInfo.pushConstantRangeCount = static_cast<uint32_t>(withPushConstants);
        createInfo.pPushConstantRanges = &pushConstantRange;

        if ((result = vkCreatePipelineLayout(device_->logicalDevice(), &createInfo, nullptr, &layout_)) != VK_SUCCESS) {
            COFFEE_ERROR("Failed to create a pipeline layout!");

            throw RegularVulkanException { result };
        }

        VkComputePipelineCreateInfo pipelineInfo { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
        VkSpecializationInfo specializationInfo {};
        std::unique_ptr<VkSpecializationMapEntry[]> specializationEntries = nullptr;
        std::unique_ptr<char[]> specializationData = nullptr;

        if (!configuration.specializationConstants.empty()) {
            uint32_t totalDataSize = 0;
            uint32_t dataOffset = 0;

            for (const auto& constant : configuration.specializationConstants) {
                totalDataSize += static_cast<uint32_t>(constant.dataSize == sizeof(bool) ? sizeof(VkBool32) : constant.dataSize);
            }

            specializationEntries = std::make_unique<VkSpecializationMapEntry[]>(configuration.specializationConstants.size());
            specializationData = std::make_unique<char[]>(totalDataSize);

            for (size_t index = 0; index < configuration.specializationConstants.size(); index++) {
                const auto& constant = configuration.specializationConstants[index];
                uint32_t constantSize = static_cast<uint32_t>(constant.dataSize == sizeof(bool) ? sizeof(VkBool32) : constant.dataSize);

                specializationEntries[index].constantID = constant.constantID;
                specializationEntries[index].offset = dataOffset;
                specializationEntries[index].size = constantSize;

                std::memcpy(specializationData.get() + dataOffset, &constant.rawData, constant.dataSize);
                dataOffset += constantSize;
            }

            specializationInfo.mapEntryCount = static_cast<uint32_t>(configuration.specializationConstants.size());
            specializationInfo.pMapEntries = specializationEntries.get();
            specializationInfo.dataSize = totalDataSize;
            specializationInfo.pData = specializationData.get();
        }

        pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipelineInfo.stage.module = configuration.shader->shader();
        pipelineInfo.stage.pName = configuration.shader->entrypoint.data();
        pipelineInfo.stage.pSpecializationInfo = &specializationInfo;

        pipelineInfo.layout = layout_;
        pipelineInfo.basePipelineHandle = nullptr;
        pipelineInfo.basePipelineIndex = -1;

        if ((result = vkCreateComputePipelines(device_->logicalDevice(), nullptr, 1, &pipelineInfo, nullptr, &pipeline_)) != VK_SUCCESS) {
            COFFEE_ERROR("Failed to create compute pipeline!");

            throw RegularVulkanException { result };
        }
    }

    ComputePipeline::~ComputePipeline() noexcept
    {
        vkDestroyPipeline(device_->logicalDevice(), pipeline_, nullptr);
        vkDestroyPipelineLayout(device_->logicalDevice(), layout_, nullptr);
    }

    ComputePipelinePtr ComputePipeline::create(const DevicePtr& device, const ComputePipelineConfiguration& configuration)
    {
        COFFEE_ASSERT(device != nullptr, "Invalid device provided.");
        COFFEE_ASSERT(configuration.shader != nullptr, "Invalid compute shader provided.");

        [[maybe_unused]] constexpr auto verifyDescriptorLayouts = [](const std::vector<DescriptorLayoutPtr>& layouts) noexcept -> bool {
            bool result = true;

            for (const auto& layout : layouts) {
                result &= (layout != nullptr);
            }

            return result;
        };

        COFFEE_ASSERT(verifyDescriptorLayouts(configuration.layouts), "Invalid std::vector<DescriptorLayoutPtr> provided.");

        return std::shared_ptr<ComputePipeline>(new ComputePipeline { device, configuration });
    }

    void ComputePipeline::verifySize(const char* name, uint32_t originalSize, uint32_t alignedSize)
    {
        if (originalSize != alignedSize) {
            COFFEE_WARNING(
                    "Provided {} with size {} isn't multiple of 4 and was rounded up to {}. "
                    "This might cause a strange behaviour in your shaders.",
                    name, originalSize, alignedSize
                );
        }
    }

} // namespace graphics

} // namespace coffee