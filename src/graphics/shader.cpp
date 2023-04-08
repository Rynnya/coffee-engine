#include <coffee/graphics/shader.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    ShaderImpl::ShaderImpl(Device& device, const std::vector<uint8_t>& byteCode, VkShaderStageFlagBits stage, const std::string& entrypoint)
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
            "Invalid VkShaderStageFlagBits provided. It must be single input, and must be any of (VK_SHADER_STAGE_VERTEX_BIT, "
            "VK_SHADER_STAGE_GEOMETRY_BIT, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, "
            "VK_SHADER_STAGE_FRAGMENT_BIT, VK_SHADER_STAGE_COMPUTE_BIT)."
        );

        VkShaderModuleCreateInfo createInfo { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        createInfo.codeSize = byteCode.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(byteCode.data());

        COFFEE_THROW_IF(
            vkCreateShaderModule(device_.logicalDevice(), &createInfo, nullptr, &shader_) != VK_SUCCESS,
            "Failed to create shader module!");
    }

    ShaderImpl::~ShaderImpl() noexcept
    {
        vkDestroyShaderModule(device_.logicalDevice(), shader_, nullptr);
    }

} // namespace coffee