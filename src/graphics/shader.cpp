#include <coffee/graphics/shader.hpp>

#include <coffee/utils/exceptions.hpp>
#include <coffee/utils/log.hpp>

namespace coffee {

    ShaderModule::ShaderModule(
        const GPUDevicePtr& device,
        const std::vector<uint8_t>& byteCode,
        ShaderStage stage,
        const std::string& entrypoint
    )
        : entrypoint { entrypoint.empty() ? "main" : entrypoint }
        , stage { stage }
        , device_ { device }
    {
        [[maybe_unused]] constexpr auto verifyStage = [](VkShaderStageFlagBits stageToCheck) -> bool {
            switch (stageToCheck) {
                case VK_SHADER_STAGE_VERTEX_BIT:
                case VK_SHADER_STAGE_GEOMETRY_BIT:
                case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
                case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
                case VK_SHADER_STAGE_FRAGMENT_BIT:
                case VK_SHADER_STAGE_COMPUTE_BIT:
                    return true;
                default:
                    return false;
            }
        };

        COFFEE_ASSERT(
            verifyStage(stage),
            "Invalid VkShaderStageFlagBits provided. It must be any of (VK_SHADER_STAGE_VERTEX_BIT, "
            "VK_SHADER_STAGE_GEOMETRY_BIT, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "
            "VK_SHADER_STAGE_FRAGMENT_BIT, VK_SHADER_STAGE_COMPUTE_BIT)."
        );

        VkShaderModuleCreateInfo createInfo { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        createInfo.codeSize = byteCode.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(byteCode.data());
        VkResult result = vkCreateShaderModule(device_->logicalDevice(), &createInfo, nullptr, &shader_);

        if (result != VK_SUCCESS) {
            COFFEE_ERROR("Failed to create shader module!");

            throw RegularVulkanException { result };
        }
    }

    ShaderModule::~ShaderModule() noexcept { vkDestroyShaderModule(device_->logicalDevice(), shader_, nullptr); }

    ShaderPtr ShaderModule::create(
        const GPUDevicePtr& device,
        const std::vector<uint8_t>& byteCode,
        ShaderStage stage,
        const std::string& entrypoint
    )
    {
        COFFEE_ASSERT(device != nullptr, "Invalid device provided.");
        COFFEE_ASSERT(!byteCode.empty(), "Empty byte code provided.");

        return std::shared_ptr<ShaderModule>(new ShaderModule { device, byteCode, stage, entrypoint });
    }

} // namespace coffee