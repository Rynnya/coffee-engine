#ifndef COFFEE_TYPES
#define COFFEE_TYPES

#include <volk/volk.h>

#include <cstdint>
#include <memory>

namespace coffee {

    namespace graphics {

        class Device;
        using DevicePtr = std::shared_ptr<Device>;

        class CommandBuffer;

        using ShaderStage = VkShaderStageFlagBits;

        enum class CommandBufferType : uint32_t { Transfer = 0, Compute = 1, Graphics = 2 };

    } // namespace graphics

    struct Float2D {
        float x = 0.0f;
        float y = 0.0f;
    };

    enum class TextureType : uint32_t {
        None = 0,
        Diffuse = 1 << 0,
        Specular = 1 << 1,
        Normals = 1 << 2,
        Emissive = 1 << 3,
        Roughness = 1 << 4,
        Metallic = 1 << 5,
        AmbientOcclusion = 1 << 6
    };

} // namespace coffee

#endif
