#include <coffee/graphics/framebuffer.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    FramebufferImpl::FramebufferImpl(Device& device, const RenderPass& renderPass, const FramebufferConfiguration& configuration)
        : width { configuration.extent.width }
        , height { configuration.extent.height }
        , layers { configuration.layers }
        , device_ { device }
    {
        COFFEE_ASSERT(renderPass != nullptr, "Invalid render pass provided.");
        COFFEE_ASSERT(
            !configuration.colorViews.empty() || configuration.depthStencilView != nullptr || configuration.resolveView != nullptr,
            "Framebuffer must have at least one attachment."
        );

        std::vector<VkImageView> imageViews {};

        for (const auto& imageView : configuration.colorViews) {
            if (imageView == nullptr) {
                continue;
            }

            imageViews.push_back(imageView->view());
        }

        if (configuration.depthStencilView != nullptr) {
            imageViews.push_back(configuration.depthStencilView->view());
        }

        if (configuration.resolveView != nullptr) {
            imageViews.push_back(configuration.resolveView->view());
        }

        VkFramebufferCreateInfo createInfo { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        createInfo.renderPass = renderPass->renderPass();
        createInfo.attachmentCount = static_cast<uint32_t>(imageViews.size());
        createInfo.pAttachments = imageViews.data();
        createInfo.width = width;
        createInfo.height = height;
        createInfo.layers = layers;

        COFFEE_THROW_IF(
            vkCreateFramebuffer(device_.logicalDevice(), &createInfo, nullptr, &framebuffer_) != VK_SUCCESS,
            "Failed to create framebuffer!");
    }

    FramebufferImpl::~FramebufferImpl() noexcept
    {
        vkDestroyFramebuffer(device_.logicalDevice(), framebuffer_, nullptr);
    }

} // namespace coffee