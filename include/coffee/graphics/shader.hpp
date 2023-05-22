#ifndef COFFEE_GRAPHICS_SHADER
#define COFFEE_GRAPHICS_SHADER

#include <coffee/graphics/device.hpp>

namespace coffee {

    using ShaderStage = VkShaderStageFlagBits;

    class ShaderModule;
    using ShaderPtr = std::shared_ptr<ShaderModule>;

    class ShaderModule : NonMoveable {
    public:
        ~ShaderModule() noexcept;

        static ShaderPtr create(
            const GPUDevicePtr& device,
            const std::vector<uint8_t>& byteCode,
            ShaderStage stage,
            const std::string& entrypoint = "main"
        );

        const ShaderStage stage;
        const std::string entrypoint;

        inline const VkShaderModule& shader() const noexcept { return shader_; }

    private:
        ShaderModule(const GPUDevicePtr& device, const std::vector<uint8_t>& byteCode, ShaderStage stage, const std::string& entrypoint);

        GPUDevicePtr device_;

        VkShaderModule shader_ = VK_NULL_HANDLE;
    };

} // namespace coffee

#endif