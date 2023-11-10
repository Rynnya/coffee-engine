#include <coffee/graphics/shader.hpp>

#include <coffee/graphics/exceptions.hpp>
#include <coffee/utils/log.hpp>

namespace coffee { namespace graphics {

    ShaderModule::ShaderModule(const DevicePtr& device, const std::vector<uint8_t>& byteCode, const std::string& entrypoint)
        : entrypoint { entrypoint.empty() ? "main" : entrypoint }
        , device_ { device }
    {
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

    ShaderPtr ShaderModule::create(const DevicePtr& device, const std::vector<uint8_t>& byteCode, const std::string& entrypoint)
    {
        COFFEE_ASSERT(device != nullptr, "Invalid device provided.");
        COFFEE_ASSERT(!byteCode.empty(), "Empty byte code provided.");

        return std::shared_ptr<ShaderModule>(new ShaderModule { device, byteCode, entrypoint });
    }

}} // namespace coffee::graphics
