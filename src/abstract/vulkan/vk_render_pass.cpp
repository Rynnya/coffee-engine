#include <coffee/abstract/vulkan/vk_render_pass.hpp>

#include <coffee/utils/log.hpp>
#include <coffee/utils/vk_utils.hpp>

#include <array>

namespace coffee {

    VulkanRenderPass::VulkanRenderPass(
        VulkanDevice& device, 
        const RenderPassConfiguration& configuration,
        const RenderPass& parentRenderPass
    )
        : device_ { device }
    {
        VkRenderPassCreateInfo renderPassInfo { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        VkAttachmentReference depthAttachmentRef {};
        std::vector<VkAttachmentReference> colorAttachmentRefs {};
        std::vector<VkAttachmentDescription> attachments {};
        std::vector<VkAttachmentReference> resolveAttachmentRefs {};
        std::vector<VkAttachmentDescription> resolveAttachments {};
        std::array<VkSubpassDependency, 2> subpassDependencies {};

        VkSubpassDescription subpass {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        for (size_t i = 0; i < configuration.colorAttachments.size(); i++) {
            VkSampleCountFlagBits sampleCountImpl = 
                VkUtils::getUsableSampleCount(configuration.colorAttachments[i].sampleCount, device_.getProperties());
            bool resolveRequired = sampleCountImpl != VK_SAMPLE_COUNT_1_BIT;
            bool resolveInPlace = resolveRequired && (configuration.colorAttachments[i].resolveImage != nullptr);
            auto& clearValue = clearValues.emplace_back();

            if (configuration.colorAttachments[i].loadOp == AttachmentLoadOp::Clear) {
                COFFEE_ASSERT(
                    std::get_if<std::monostate>(&configuration.colorAttachments[i].clearValue) == nullptr,
                    "Color attachment require clear value, but it wasn't provided.");

                std::visit([&clearValue](auto&& colorClearValue) {
                    using T = std::decay_t<decltype(colorClearValue)>;

                    if constexpr (std::is_same_v<T, std::array<float, 4>>) {
                        clearValue.color.float32[0] = colorClearValue[0];
                        clearValue.color.float32[1] = colorClearValue[1];
                        clearValue.color.float32[2] = colorClearValue[2];
                        clearValue.color.float32[3] = colorClearValue[3];
                    }
                    else if constexpr (std::is_same_v<T, std::array<int32_t, 4>>) {
                        clearValue.color.int32[0] = colorClearValue[0];
                        clearValue.color.int32[1] = colorClearValue[1];
                        clearValue.color.int32[2] = colorClearValue[2];
                        clearValue.color.int32[3] = colorClearValue[3];
                    }
                    else if constexpr (std::is_same_v<T, std::array<uint32_t, 4>>) {
                        clearValue.color.uint32[0] = colorClearValue[0];
                        clearValue.color.uint32[1] = colorClearValue[1];
                        clearValue.color.uint32[2] = colorClearValue[2];
                        clearValue.color.uint32[3] = colorClearValue[3];
                    }
                }, configuration.colorAttachments[i].clearValue);

                useClearValues |= true;
            }

            VkAttachmentDescription colorAttachment {};
            colorAttachment.format = VkUtils::transformFormat(configuration.colorAttachments[i].format);
            colorAttachment.samples = sampleCountImpl;
            colorAttachment.loadOp = VkUtils::transformAttachmentLoadOp(configuration.colorAttachments[i].loadOp);
            colorAttachment.storeOp = VkUtils::transformAttachmentStoreOp(configuration.colorAttachments[i].storeOp);
            colorAttachment.stencilLoadOp = VkUtils::transformAttachmentLoadOp(configuration.colorAttachments[i].stencilLoadOp);
            colorAttachment.stencilStoreOp = VkUtils::transformAttachmentStoreOp(configuration.colorAttachments[i].stencilStoreOp);
            colorAttachment.initialLayout = VkUtils::transformResourceStateToLayout(configuration.colorAttachments[i].initialState);
            colorAttachment.finalLayout = resolveInPlace
                ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                : VkUtils::transformResourceStateToLayout(configuration.colorAttachments[i].finalState);
            attachments.push_back(std::move(colorAttachment));

            auto& colorRef = colorAttachmentRefs.emplace_back();
            colorRef.attachment = attachments.size() - 1;
            colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            if (resolveInPlace) {
                VkAttachmentDescription resolveAttachment {};
                resolveAttachment.format = colorAttachment.format;
                resolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
                resolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                resolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                resolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                resolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                resolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                resolveAttachment.finalLayout = VkUtils::transformResourceStateToLayout(configuration.colorAttachments[i].finalState);
                resolveAttachments.push_back(std::move(resolveAttachment));

                auto& resolveRef = resolveAttachmentRefs.emplace_back();
                resolveRef.attachment = configuration.colorAttachments.size() + resolveAttachments.size() - 1;
                resolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            }
            else {
                auto& resolveRef = resolveAttachmentRefs.emplace_back();
                resolveRef.attachment = VK_ATTACHMENT_UNUSED;
            }
        }

        subpass.colorAttachmentCount = colorAttachmentRefs.size();
        subpass.pColorAttachments = colorAttachmentRefs.data();
        subpass.pResolveAttachments = resolveAttachmentRefs.data();

        const coffee::AttachmentConfiguration* depthStencilAttachment =
            std::get_if<coffee::AttachmentConfiguration>(&configuration.depthStencilAttachment);

        if (depthStencilAttachment) {
            auto& clearValue = clearValues.emplace_back();

            if (depthStencilAttachment->loadOp == AttachmentLoadOp::Clear) {
                const coffee::ClearDepthStencilValue* depthStencilClearValue =
                    std::get_if<coffee::ClearDepthStencilValue>(&depthStencilAttachment->depthStencilClearValue);

                COFFEE_ASSERT(depthStencilClearValue != nullptr, "Depth attachment require clear values, but they wasn't provided.");

                clearValue.depthStencil.depth = depthStencilClearValue->depth;
                clearValue.depthStencil.stencil = depthStencilClearValue->stencil;

                useClearValues |= true;
            }

            VkAttachmentDescription depthAttachment {};
            depthAttachment.format = VkUtils::transformFormat(depthStencilAttachment->format);
            depthAttachment.samples =
                VkUtils::getUsableSampleCount(depthStencilAttachment->sampleCount, device_.getProperties());
            depthAttachment.loadOp = VkUtils::transformAttachmentLoadOp(depthStencilAttachment->loadOp);
            depthAttachment.storeOp = VkUtils::transformAttachmentStoreOp(depthStencilAttachment->storeOp);
            depthAttachment.stencilLoadOp = VkUtils::transformAttachmentLoadOp(depthStencilAttachment->stencilLoadOp);
            depthAttachment.stencilStoreOp = VkUtils::transformAttachmentStoreOp(depthStencilAttachment->stencilStoreOp);
            depthAttachment.initialLayout = VkUtils::transformResourceStateToLayout(depthStencilAttachment->initialState);
            depthAttachment.finalLayout = VkUtils::transformResourceStateToLayout(depthStencilAttachment->finalState);
            attachments.push_back(std::move(depthAttachment));

            depthAttachmentRef.attachment = attachments.size() - 1;
            depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            subpass.pDepthStencilAttachment = &depthAttachmentRef;

            hasDepthAttachment = true;
        }

        if (configuration.automaticBarriersRequired) {
            subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
            subpassDependencies[0].dstSubpass = 0;
            subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            subpassDependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            subpassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

            subpassDependencies[1].srcSubpass = 0;
            subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
            subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            subpassDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            subpassDependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            subpassDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

            if (depthStencilAttachment) {
                subpassDependencies[0].srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                subpassDependencies[0].dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            }

            renderPassInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
            renderPassInfo.pDependencies = subpassDependencies.data();
        }

        attachments.insert(attachments.end(), resolveAttachments.begin(), resolveAttachments.end());
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        COFFEE_ASSERT(!attachments.empty(), "No attachments was provided to render pass.");

        COFFEE_THROW_IF(
            vkCreateRenderPass(device_.getLogicalDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS,
            "Failed to create render pass!");
    }

    VulkanRenderPass::~VulkanRenderPass() {
        vkDestroyRenderPass(device_.getLogicalDevice(), renderPass, nullptr);
    }

}


