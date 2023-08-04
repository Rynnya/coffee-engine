#ifndef COFFEE_GRAPHICS_RENDER_PASS
#define COFFEE_GRAPHICS_RENDER_PASS

#include <coffee/graphics/device.hpp>
#include <coffee/graphics/image.hpp>

namespace coffee {

namespace graphics {

    struct AttachmentConfiguration {
        VkAttachmentDescriptionFlags flags = 0;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
        VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ImagePtr resolveImage = nullptr;
    };

    struct RenderPassConfiguration {
        std::vector<AttachmentConfiguration> colorAttachments {};
        // VK_FORMAT_UNDEFINED means there's no depthStencilAttachment
        AttachmentConfiguration depthStencilAttachment {};
    };

    class RenderPass;
    using RenderPassPtr = std::unique_ptr<RenderPass>;

    class RenderPass {
    public:
        ~RenderPass() noexcept;

        static RenderPassPtr create(const DevicePtr& device, const RenderPassConfiguration& configuration);

        inline const VkRenderPass& renderPass() const noexcept { return renderPass_; }

    private:
        RenderPass(const DevicePtr& device, const RenderPassConfiguration& configuration);

        DevicePtr device_;

        VkRenderPass renderPass_ = VK_NULL_HANDLE;
    };

} // namespace graphics

} // namespace coffee

#endif