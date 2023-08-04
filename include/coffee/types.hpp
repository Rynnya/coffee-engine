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

    class SpecializationConstant {
    public:
        inline SpecializationConstant(uint32_t id, bool value)
            : constantID { id }
            , dataSize { sizeof(bool) }
            , rawData { static_cast<uint64_t>(value) }
        {}

        inline SpecializationConstant(uint32_t id, int8_t value)
            : constantID { id }
            , dataSize { sizeof(int8_t) }
            , rawData { static_cast<uint64_t>(value) }
        {}

        inline SpecializationConstant(uint32_t id, uint8_t value)
            : constantID { id }
            , dataSize { sizeof(uint8_t) }
            , rawData { static_cast<uint64_t>(value) }
        {}

        inline SpecializationConstant(uint32_t id, int16_t value)
            : constantID { id }
            , dataSize { sizeof(int16_t) }
            , rawData { static_cast<uint64_t>(value) }
        {}

        inline SpecializationConstant(uint32_t id, uint16_t value)
            : constantID { id }
            , dataSize { sizeof(uint16_t) }
            , rawData { static_cast<uint64_t>(value) }
        {}

        inline SpecializationConstant(uint32_t id, int32_t value)
            : constantID { id }
            , dataSize { sizeof(int32_t) }
            , rawData { static_cast<uint64_t>(value) }
        {}

        inline SpecializationConstant(uint32_t id, uint32_t value)
            : constantID { id }
            , dataSize { sizeof(uint32_t) }
            , rawData { static_cast<uint64_t>(value) }
        {}

        inline SpecializationConstant(uint32_t id, int64_t value)
            : constantID { id }
            , dataSize { sizeof(int64_t) }
            , rawData { static_cast<uint64_t>(value) }
        {}

        inline SpecializationConstant(uint32_t id, uint64_t value)
            : constantID { id }
            , dataSize { sizeof(uint64_t) }
            , rawData { static_cast<uint64_t>(value) }
        {}

        inline SpecializationConstant(uint32_t id, float value)
            : constantID { id }
            , dataSize { sizeof(float) }
            , rawData { static_cast<uint64_t>(*reinterpret_cast<uint32_t*>(&value)) }
        {}

        inline SpecializationConstant(uint32_t id, double value)
            : constantID { id }
            , dataSize { sizeof(double) }
            , rawData { *reinterpret_cast<uint64_t*>(&value) }
        {}

    private:
        uint32_t constantID = 0;
        uint32_t dataSize = 0;
        uint64_t rawData = 0;

        friend class GraphicsPipeline;
        friend class ComputePipeline;
    };

    struct PushConstants {
        size_t size = 0ULL;
        size_t offset = 0ULL;
    };

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
