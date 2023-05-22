#ifndef COFFEE_VK_RENDER_PASS
#define COFFEE_VK_RENDER_PASS

#include <coffee/graphics/device.hpp>
#include <coffee/graphics/image.hpp>

#include <optional>

namespace coffee {

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
        std::optional<AttachmentConfiguration> depthStencilAttachment = std::nullopt;
    };

    class RenderPass;
    using RenderPassPtr = std::unique_ptr<RenderPass>;

    class RenderPass {
    public:
        ~RenderPass() noexcept;

        static RenderPassPtr create(const GPUDevicePtr& device, const RenderPassConfiguration& configuration);

        inline const VkRenderPass& renderPass() const noexcept {
            return renderPass_;
        }

    private:
        RenderPass(const GPUDevicePtr& device, const RenderPassConfiguration& configuration);

        GPUDevicePtr device_;

        VkRenderPass renderPass_ = VK_NULL_HANDLE;
    };

} // namespace coffee

#endif