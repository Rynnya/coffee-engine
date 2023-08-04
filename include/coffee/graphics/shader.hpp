#ifndef COFFEE_GRAPHICS_SHADER
#define COFFEE_GRAPHICS_SHADER

#include <coffee/graphics/device.hpp>

namespace coffee {

namespace graphics {

    class ShaderModule;
    using ShaderPtr = std::shared_ptr<ShaderModule>;

    class ShaderModule : NonMoveable {
    public:
        ~ShaderModule() noexcept;

        static ShaderPtr create(const DevicePtr& device, const std::vector<uint8_t>& byteCode, const std::string& entrypoint = "main");

        const std::string entrypoint;

        inline const VkShaderModule& shader() const noexcept { return shader_; }

    private:
        ShaderModule(const DevicePtr& device, const std::vector<uint8_t>& byteCode, const std::string& entrypoint);

        DevicePtr device_;

        VkShaderModule shader_ = VK_NULL_HANDLE;
    };

} // namespace graphics

} // namespace coffee

#endif