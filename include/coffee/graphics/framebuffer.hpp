#ifndef COFFEE_GRAPHICS_FRAMEBUFFER
#define COFFEE_GRAPHICS_FRAMEBUFFER

#include <coffee/graphics/device.hpp>
#include <coffee/graphics/image.hpp>
#include <coffee/graphics/render_pass.hpp>

namespace coffee {

    struct FramebufferConfiguration {
        VkExtent2D extent {};
        uint32_t layers = 1U;
        std::vector<ImageViewPtr> colorViews {};
        ImageViewPtr depthStencilView = nullptr;
        ImageViewPtr resolveView = nullptr;
    };

    class Framebuffer;
    using FramebufferPtr = std::unique_ptr<Framebuffer>;

    class Framebuffer {
    public:
        ~Framebuffer() noexcept;

        static FramebufferPtr create(
            const GPUDevicePtr& device,
            const RenderPassPtr& renderPass,
            const FramebufferConfiguration& configuration
        );

        const uint32_t width;
        const uint32_t height;
        const uint32_t layers;

        inline const VkFramebuffer& framebuffer() const noexcept
        {
            return framebuffer_;
        }

    private:
        Framebuffer(const GPUDevicePtr& device, const RenderPassPtr& renderPass, const FramebufferConfiguration& configuration);

        GPUDevicePtr device_;

        VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
    };

    

} // namespace coffee

#endif