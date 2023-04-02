#include <coffee/abstract/vulkan/vk_shader.hpp>

#include <coffee/utils/log.hpp>
#include <coffee/utils/utils.hpp>

#include <stdexcept>

namespace coffee {

    VulkanShader::VulkanShader(VulkanDevice& device, const std::vector<uint8_t>& byteCode, ShaderStage stage, const std::string& entrypoint)
            : AbstractShader { stage, entrypoint }
            , device_ { device } {
        VkShaderModuleCreateInfo createInfo { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        createInfo.codeSize = byteCode.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(byteCode.data());

        COFFEE_THROW_IF(
            vkCreateShaderModule(device_.getLogicalDevice(), &createInfo, nullptr, &shader) != VK_SUCCESS,
            "Failed to create shader module!");
    }

    VulkanShader::~VulkanShader() noexcept {
        vkDestroyShaderModule(device_.getLogicalDevice(), shader, nullptr);
    }

} // namespace coffee