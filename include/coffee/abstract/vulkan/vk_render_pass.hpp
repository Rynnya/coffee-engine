#ifndef COFFEE_VK_RENDER_PASS
#define COFFEE_VK_RENDER_PASS

#include <coffee/abstract/vulkan/vk_device.hpp>
#include <coffee/abstract/render_pass.hpp>

namespace coffee {

    class VulkanRenderPass : public AbstractRenderPass {
    public:
        VulkanRenderPass(VulkanDevice& device, const RenderPassConfiguration& configuration, const RenderPass& parentRenderPass);
        ~VulkanRenderPass();

        VkRenderPass renderPass;

        std::vector<VkClearValue> clearValues {};
        bool useClearValues = false;

        bool hasDepthAttachment = false;

    private:
        VulkanDevice& device_;
    };

}

#endif