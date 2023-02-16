#ifndef COFFEE_ABSTRACT_BARRIERS
#define COFFEE_ABSTRACT_BARRIERS

#include <coffee/abstract/buffer.hpp>
#include <coffee/abstract/image.hpp>

namespace coffee {

    class MemoryBarrier {
    public:
        MemoryBarrier() noexcept = default;

        // TODO: Implement or redesign system so it will fit D3D12 and Vulkan transition
    };

    class BufferBarrier {
    public:
        BufferBarrier() noexcept = default;

        // TODO: Implement or redesign system so it will fit D3D12 and Vulkan transition
    };

    class ImageBarrier {
    public:
        ImageBarrier(const Image& image, ResourceState oldLayout, ResourceState newLayout) noexcept;

        const Image image;
        const ResourceState oldLayout;
        const ResourceState newLayout;
    };

}

#endif