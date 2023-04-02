#ifndef COFFEE_VK_FRAMEBUFFER
#define COFFEE_VK_FRAMEBUFFER

#include <coffee/abstract/framebuffer.hpp>
#include <coffee/abstract/render_pass.hpp>
#include <coffee/abstract/vulkan/vk_device.hpp>
#include <coffee/types.hpp>

namespace coffee {

    class VulkanFramebuffer : public AbstractFramebuffer {
    public:
        VulkanFramebuffer(VulkanDevice& device, const RenderPass& renderPass, const FramebufferConfiguration& configuration);
        ~VulkanFramebuffer() noexcept;

        VkFramebuffer framebuffer;

    private:
        VulkanDevice& device_;
    };

} // namespace coffee

#endif