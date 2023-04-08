#ifndef COFFEE_GRAPHICS_SHADER
#define COFFEE_GRAPHICS_SHADER

#include <coffee/graphics/device.hpp>

namespace coffee {

    class ShaderImpl : NonMoveable {
    public:
        ShaderImpl(Device& device, const std::vector<uint8_t>& byteCode, VkShaderStageFlagBits stage, const std::string& entrypoint);
        ~ShaderImpl() noexcept;

        const VkShaderStageFlagBits stage;
        const std::string entrypoint;

        inline const VkShaderModule& shader() const noexcept
        {
            return shader_;
        }

    private:
        Device& device_;

        VkShaderModule shader_ = VK_NULL_HANDLE;
    };

    using Shader = std::unique_ptr<ShaderImpl>;

} // namespace coffee

#endif