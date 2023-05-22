#ifndef COFFEE_TYPES
#define COFFEE_TYPES

#include <cstdint>
#include <memory>

namespace coffee {

    class GPUDevice;
    using GPUDevicePtr = std::shared_ptr<GPUDevice>;

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

    enum class CommandBufferType : uint32_t { Transfer = 0, Graphics = 1, Compute = 2 };

} // namespace coffee

#endif
