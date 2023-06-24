#include <coffee/graphics/render_pass.hpp>

#include <coffee/graphics/exceptions.hpp>
#include <coffee/utils/log.hpp>
#include <coffee/utils/vk_utils.hpp>

#include <array>

namespace coffee {

    namespace graphics {

        RenderPass::RenderPass(const DevicePtr& device, const RenderPassConfiguration& configuration) : device_ { device }
        {
            VkRenderPassCreateInfo renderPassInfo { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };

            std::vector<VkAttachmentReference> colorAttachmentRefs {};
            VkAttachmentReference depthAttachmentRef {};
            std::vector<VkAttachmentReference> resolveAttachmentRefs {};

            std::vector<VkAttachmentDescription> attachments {};
            std::vector<VkAttachmentDescription> resolveAttachments {};

            VkSubpassDescription subpass {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

            for (const auto& colorAttachment : configuration.colorAttachments) {
                VkSampleCountFlagBits sampleCountImpl = VkUtils::getUsableSampleCount(colorAttachment.samples, device_->properties());
                bool resolveRequired = sampleCountImpl != VK_SAMPLE_COUNT_1_BIT;
                bool resolveInPlace = resolveRequired && (colorAttachment.resolveImage != nullptr);

                VkAttachmentDescription colorAttachmentDescription {};
                colorAttachmentDescription.flags = colorAttachment.flags;
                colorAttachmentDescription.format = colorAttachment.format;
                colorAttachmentDescription.samples = colorAttachment.samples;
                colorAttachmentDescription.loadOp = colorAttachment.loadOp;
                colorAttachmentDescription.storeOp = colorAttachment.storeOp;
                colorAttachmentDescription.stencilLoadOp = colorAttachment.stencilLoadOp;
                colorAttachmentDescription.stencilStoreOp = colorAttachment.stencilStoreOp;
                colorAttachmentDescription.initialLayout = colorAttachment.initialLayout;
                colorAttachmentDescription.finalLayout = colorAttachment.finalLayout;
                attachments.push_back(colorAttachmentDescription);

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
                    resolveAttachment.finalLayout = colorAttachment.finalLayout;
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

            if (configuration.depthStencilAttachment.has_value()) {
                const AttachmentConfiguration& depthStencilAttachment = configuration.depthStencilAttachment.value();

                VkAttachmentDescription depthAttachmentDescription {};
                depthAttachmentDescription.flags = depthStencilAttachment.flags;
                depthAttachmentDescription.format = depthStencilAttachment.format;
                depthAttachmentDescription.samples = depthStencilAttachment.samples;
                depthAttachmentDescription.loadOp = depthStencilAttachment.loadOp;
                depthAttachmentDescription.storeOp = depthStencilAttachment.storeOp;
                depthAttachmentDescription.stencilLoadOp = depthStencilAttachment.stencilLoadOp;
                depthAttachmentDescription.stencilStoreOp = depthStencilAttachment.stencilStoreOp;
                depthAttachmentDescription.initialLayout = depthStencilAttachment.initialLayout;
                depthAttachmentDescription.finalLayout = depthStencilAttachment.finalLayout;
                attachments.push_back(depthAttachmentDescription);

                depthAttachmentRef.attachment = attachments.size() - 1;
                depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                subpass.pDepthStencilAttachment = &depthAttachmentRef;
            }

            // This is still suboptimal variant for render passes, but at least it tried to do only what attachments wanna do
            // As example, depth-only will only wait for early-late tests while color-only will wait for fragment shader and output

            std::array<VkSubpassDependency, 2> subpassDependencies {};
            subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
            subpassDependencies[0].dstSubpass = 0;
            subpassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            subpassDependencies[1].srcSubpass = 0;
            subpassDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
            subpassDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

            if (!configuration.colorAttachments.empty()) {
                subpassDependencies[0].srcStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                subpassDependencies[0].dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                subpassDependencies[0].srcAccessMask |= VK_ACCESS_SHADER_READ_BIT;
                subpassDependencies[0].dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                subpassDependencies[1].srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                subpassDependencies[1].dstStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                subpassDependencies[1].srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                subpassDependencies[1].dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
            }

            if (configuration.depthStencilAttachment.has_value()) {
                subpassDependencies[0].srcStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                subpassDependencies[0].dstStageMask |=
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                subpassDependencies[0].srcAccessMask |= VK_ACCESS_SHADER_READ_BIT;
                subpassDependencies[0].dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

                subpassDependencies[1].srcStageMask |=
                    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                subpassDependencies[1].dstStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                subpassDependencies[1].srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                subpassDependencies[1].dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
            }

            renderPassInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
            renderPassInfo.pDependencies = subpassDependencies.data();

            attachments.insert(attachments.end(), resolveAttachments.begin(), resolveAttachments.end());
            renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            renderPassInfo.pAttachments = attachments.data();
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;

            COFFEE_ASSERT(!attachments.empty(), "No attachments was provided to render pass.");
            VkResult result = vkCreateRenderPass(device_->logicalDevice(), &renderPassInfo, nullptr, &renderPass_);

            if (result != VK_SUCCESS) {
                COFFEE_ERROR("Failed to create render pass!");

                throw RegularVulkanException { result };
            }
        }

        RenderPass::~RenderPass() { vkDestroyRenderPass(device_->logicalDevice(), renderPass_, nullptr); }

        RenderPassPtr RenderPass::create(const DevicePtr& device, const RenderPassConfiguration& configuration)
        {
            COFFEE_ASSERT(device != nullptr, "Invalid device provided.");

            return std::unique_ptr<RenderPass>(new RenderPass { device, configuration });
        }

    } // namespace graphics

} // namespace coffee
