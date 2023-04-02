#ifndef COFFEE_VK_SHADER
#define COFFEE_VK_SHADER

#include <coffee/abstract/shader.hpp>
#include <coffee/abstract/vulkan/vk_device.hpp>

namespace coffee {

    class VulkanShader : public AbstractShader {
    public:
        VulkanShader(VulkanDevice& device, const std::vector<uint8_t>& byteCode, ShaderStage stage, const std::string& entrypoint);
        ~VulkanShader() noexcept;

        VkShaderModule shader;

    private:
        VulkanDevice& device_;
    };

} // namespace coffee

#endif