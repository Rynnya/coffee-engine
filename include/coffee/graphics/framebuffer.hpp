#ifndef COFFEE_GRAPHICS_FRAMEBUFFER
#define COFFEE_GRAPHICS_FRAMEBUFFER

#include <coffee/graphics/device.hpp>
#include <coffee/graphics/image.hpp>
#include <coffee/graphics/render_pass.hpp>

namespace coffee {

    struct FramebufferConfiguration {
        VkExtent2D extent {};
        uint32_t layers = 1U;
        std::vector<ImageView> colorViews {};
        ImageView depthStencilView = nullptr;
        ImageView resolveView = nullptr;
    };

    class FramebufferImpl {
    public:
        FramebufferImpl(Device& device, const RenderPass& renderPass, const FramebufferConfiguration& configuration);
        ~FramebufferImpl() noexcept;

        const uint32_t width;
        const uint32_t height;
        const uint32_t layers;

        inline const VkFramebuffer& framebuffer() const noexcept
        {
            return framebuffer_;
        }

    private:
        Device& device_;

        VkFramebuffer framebuffer_ = VK_NULL_HANDLE;
    };

    using Framebuffer = std::unique_ptr<FramebufferImpl>;

} // namespace coffee

#endif