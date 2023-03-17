#include <coffee/abstract/vulkan/vk_framebuffer.hpp>

#include <coffee/abstract/vulkan/vk_image.hpp>
#include <coffee/abstract/vulkan/vk_render_pass.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    VulkanFramebuffer::VulkanFramebuffer(
        VulkanDevice& device,
        const RenderPass& renderPass,
        const FramebufferConfiguration& configuration
    ) 
        : AbstractFramebuffer { configuration.extent.width, configuration.extent.height }
        , device_ { device }
    {
        std::vector<VkImageView> imageViewsImpl {};

        for (const auto& imageView : configuration.colorViews) {
            imageViewsImpl.push_back(static_cast<VulkanImage*>(imageView.get())->imageView);
        }

        if (configuration.depthStencilView != nullptr) {
            imageViewsImpl.push_back(static_cast<VulkanImage*>(configuration.depthStencilView.get())->imageView);
        }

        if (configuration.resolveView != nullptr) {
            imageViewsImpl.push_back(static_cast<VulkanImage*>(configuration.resolveView.get())->imageView);
        }

        VkFramebufferCreateInfo createInfo { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        createInfo.renderPass = static_cast<VulkanRenderPass*>(renderPass.get())->renderPass;
        createInfo.attachmentCount = static_cast<uint32_t>(imageViewsImpl.size());
        createInfo.pAttachments = imageViewsImpl.data();
        createInfo.width = getWidth();
        createInfo.height = getHeight();
        createInfo.layers = 1U;

        COFFEE_THROW_IF(
            vkCreateFramebuffer(device_.getLogicalDevice(), &createInfo, nullptr, &framebuffer) != VK_SUCCESS,
            "Failed to create framebuffer!");
    }

    VulkanFramebuffer::~VulkanFramebuffer() noexcept {
        vkDestroyFramebuffer(device_.getLogicalDevice(), framebuffer, nullptr);
    }

}