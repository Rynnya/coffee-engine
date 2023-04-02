#include <coffee/abstract/vulkan/vk_command_buffer.hpp>

#include <coffee/abstract/vulkan/vk_buffer.hpp>
#include <coffee/abstract/vulkan/vk_descriptors.hpp>
#include <coffee/abstract/vulkan/vk_framebuffer.hpp>
#include <coffee/abstract/vulkan/vk_image.hpp>
#include <coffee/abstract/vulkan/vk_pipeline.hpp>
#include <coffee/abstract/vulkan/vk_render_pass.hpp>

#include <coffee/utils/log.hpp>
#include <coffee/utils/math.hpp>
#include <coffee/utils/vk_utils.hpp>

namespace coffee {

    VulkanTransferCB::VulkanTransferCB(VulkanDevice& device) : device { device } {
        pool = device.getUniqueCommandPool();

        VkCommandBufferAllocateInfo allocateInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        allocateInfo.commandPool = pool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1;

        COFFEE_THROW_IF(
            vkAllocateCommandBuffers(device.getLogicalDevice(), &allocateInfo, &commandBuffer) != VK_SUCCESS,
            "Failed to allocate command buffer!");

        VkCommandBufferBeginInfo beginInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        COFFEE_THROW_IF(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS, "Failed to begin command buffer recording!");
    }

    VulkanTransferCB::~VulkanTransferCB() noexcept {
        if (pool != nullptr) { // Implementation will take ownership of pool when submitting to
                               // queue, so pool must be checked
            vkFreeCommandBuffers(device.getLogicalDevice(), pool, 1U, &commandBuffer);
            device.returnCommandPool(pool);
        }
    }

    void VulkanTransferCB::layoutTransition(const Image& image, ResourceState newLayout) {
        [[maybe_unused]] constexpr auto verifyLayout = [](ResourceState layout) noexcept -> bool {
            switch (layout) {
                case ResourceState::VertexBuffer:
                case ResourceState::UniformBuffer:
                case ResourceState::IndexBuffer:
                case ResourceState::IndirectCommand:
                    return false;
                default:
                    return true;
            }
        };

        COFFEE_ASSERT(Math::hasSingleBit(static_cast<uint32_t>(newLayout)), "ResourceState 'newLayout' must have only one bit.");
        COFFEE_ASSERT(verifyLayout(newLayout), "Invalid 'newLayout' state.");

        VulkanImage* imageImpl = static_cast<VulkanImage*>(image.get());

        VkImageLayout oldLayoutImpl = VkUtils::transformResourceStateToLayout(imageImpl->getLayout());
        VkImageLayout newLayoutImpl = VkUtils::transformResourceStateToLayout(newLayout);

        VkAccessFlags srcAccessMask = VkUtils::imageLayoutToAccessFlags(oldLayoutImpl);
        VkAccessFlags dstAccessMask = VkUtils::imageLayoutToAccessFlags(newLayoutImpl);

        VkPipelineStageFlags srcStages = [&]() -> VkPipelineStageFlags {
            if (oldLayoutImpl == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
                return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            }

            if (srcAccessMask != 0) {
                return VkUtils::accessFlagsToPipelineStages(srcAccessMask);
            }

            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        }();

        VkPipelineStageFlags dstStages = [&]() -> VkPipelineStageFlags {
            if (newLayoutImpl == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
                return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            }

            if (dstAccessMask != 0) {
                return VkUtils::accessFlagsToPipelineStages(dstAccessMask);
            }

            return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        }();

        VkImageMemoryBarrier barrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        barrier.srcAccessMask = srcAccessMask;
        barrier.dstAccessMask = dstAccessMask;
        barrier.oldLayout = oldLayoutImpl;
        barrier.newLayout = newLayoutImpl;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = imageImpl->image;
        barrier.subresourceRange.aspectMask = imageImpl->aspectMask;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;

        vkCmdPipelineBarrier(commandBuffer, srcStages, dstStages, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        imageImpl->setNewLayout(newLayout);
    }

    void VulkanTransferCB::copyBufferToBuffer(
        const Buffer& srcBuffer,
        const Buffer& dstBuffer,
        const std::vector<BufferCopyRegion>& copyRegions
    ) {
        if (copyRegions.empty()) {
            COFFEE_WARNING("Empty array of copy regions was provided.");
            return;
        }

        COFFEE_ASSERT(srcBuffer != nullptr, "Invalid srcBuffer provided.");
        COFFEE_ASSERT(dstBuffer != nullptr, "Invalid dstBuffer provided.");

        COFFEE_ASSERT(
            (srcBuffer->getUsageFlags() & BufferUsage::TransferSource) == BufferUsage::TransferSource,
            "srcBuffer must have TransferSource flag set."
        );
        COFFEE_ASSERT(
            (dstBuffer->getUsageFlags() & BufferUsage::TransferDestination) == BufferUsage::TransferDestination,
            "dstBuffer must have TransferDestination flag set."
        );

        std::vector<VkBufferCopy> copyRegionsImpl {};

        for (const auto& copyRegion : copyRegions) {
            VkBufferCopy copyRegionImpl {};

            COFFEE_ASSERT(copyRegion.srcOffset < srcBuffer->getTotalSize(), "srcOffset must be less than srcBuffer size.");
            COFFEE_ASSERT(copyRegion.dstOffset < dstBuffer->getTotalSize(), "dstOffset must be less than dstBuffer size.");

            copyRegionImpl.srcOffset = copyRegion.srcOffset;
            copyRegionImpl.dstOffset = copyRegion.dstOffset;

            if (copyRegion.totalSize == std::numeric_limits<size_t>::max()) {
                copyRegionImpl.size =
                    std::min(srcBuffer->getTotalSize() - copyRegion.srcOffset, dstBuffer->getTotalSize() - copyRegion.dstOffset);
            }
            else {
                COFFEE_ASSERT(
                    copyRegion.totalSize <= (srcBuffer->getTotalSize() - copyRegion.srcOffset),
                    "totalSize must be less or equal to srcBuffer size - srcOffset"
                );
                COFFEE_ASSERT(
                    copyRegion.totalSize <= (dstBuffer->getTotalSize() - copyRegion.dstOffset),
                    "totalSize must be less or equal to dstBuffer size - dstOffset"
                );

                copyRegionImpl.size = copyRegion.totalSize;
            }

            copyRegionsImpl.push_back(std::move(copyRegionImpl));
        }

        VulkanBuffer* srcBufferImpl = static_cast<VulkanBuffer*>(srcBuffer.get());
        VulkanBuffer* dstBufferImpl = static_cast<VulkanBuffer*>(dstBuffer.get());

        vkCmdCopyBuffer(
            commandBuffer,
            srcBufferImpl->buffer,
            dstBufferImpl->buffer,
            static_cast<uint32_t>(copyRegionsImpl.size()),
            copyRegionsImpl.data()
        );
    }

    void VulkanTransferCB::copyImageToImage(const Image& srcImage, const Image& dstImage, const std::vector<ImageCopyRegion>& copyRegions) {
        if (copyRegions.empty()) {
            COFFEE_WARNING("Empty array of copy regions was provided.");
            return;
        }

        COFFEE_ASSERT(srcImage != nullptr, "Invalid srcImage provided.");
        COFFEE_ASSERT(dstImage != nullptr, "Invalid dstImage provided.");

        COFFEE_ASSERT(
            (srcImage->getUsageFlags() & ImageUsage::TransferSource) == ImageUsage::TransferSource,
            "srcImage must have TransferSource flag set."
        );
        COFFEE_ASSERT(
            (dstImage->getUsageFlags() & ImageUsage::TransferDestination) == ImageUsage::TransferDestination,
            "dstImage must have TransferDestination flag set."
        );
        COFFEE_ASSERT(
            srcImage->getLayout() == ResourceState::CopySource || srcImage->getLayout() == ResourceState::UnorderedAccess,
            "srcImage must have layout set to CopySource or UnorderedAccess (this can be achieved "
            "by usage of pipeline barriers)"
        );
        COFFEE_ASSERT(
            dstImage->getLayout() == ResourceState::CopyDestination || dstImage->getLayout() == ResourceState::UnorderedAccess,
            "srcImage must have layout set to CopyDestination or UnorderedAccess (this can be "
            "achieved by usage of pipeline barriers)"
        );

        std::vector<VkImageCopy> copyRegionsImpl {};

        for (const auto& copyRegion : copyRegions) {
            VkImageCopy copyRegionImpl {};

            COFFEE_ASSERT(copyRegion.srcAspects == copyRegion.dstAspects, "srcAspects must be same as dstAspects");

            COFFEE_ASSERT(
                copyRegion.extent.width <= srcImage->getWidth() && copyRegion.extent.width <= dstImage->getWidth(),
                "imageExtent.width must be less or equal to srcImage and dstImage width."
            );
            COFFEE_ASSERT(
                copyRegion.extent.height <= srcImage->getHeight() && copyRegion.extent.height <= dstImage->getHeight(),
                "imageExtent.height must be less or equal to srcImage and dstImage height."
            );
            COFFEE_ASSERT(
                copyRegion.extent.depth <= srcImage->getDepth() && copyRegion.extent.depth <= dstImage->getDepth(),
                "imageExtent.depth must be less or equal to srcImage and dstImage depth."
            );

            COFFEE_ASSERT(
                copyRegion.extent.width + copyRegion.srcOffset.x <= srcImage->getWidth(),
                "extent.width + srcOffset.x must be less or equal to srcImage width."
            );
            COFFEE_ASSERT(
                copyRegion.extent.height + copyRegion.srcOffset.y <= srcImage->getHeight(),
                "extent.height + srcOffset.y must be less or equal to srcImage height."
            );
            COFFEE_ASSERT(
                copyRegion.extent.depth + copyRegion.srcOffset.z <= srcImage->getDepth(),
                "extent.height + srcOffset.z must be less or equal to srcImage depth."
            );

            COFFEE_ASSERT(
                copyRegion.extent.width + copyRegion.dstOffset.x <= dstImage->getWidth(),
                "extent.width + dstOffset.x must be less or equal to dstImage width."
            );
            COFFEE_ASSERT(
                copyRegion.extent.height + copyRegion.dstOffset.y <= dstImage->getHeight(),
                "extent.height + dstOffset.y must be less or equal to dstImage height."
            );
            COFFEE_ASSERT(
                copyRegion.extent.depth + copyRegion.dstOffset.z <= dstImage->getDepth(),
                "extent.height + dstOffset.z must be less or equal to dstImage depth."
            );

            copyRegionImpl.srcSubresource.aspectMask = VkUtils::transformImageAspects(copyRegion.srcAspects);
            // TODO: Add support for mipmaps and layers
            copyRegionImpl.srcSubresource.mipLevel = 0;
            copyRegionImpl.srcSubresource.baseArrayLayer = 0;
            copyRegionImpl.srcSubresource.layerCount = 1;

            copyRegionImpl.srcOffset.x = copyRegion.srcOffset.x;
            copyRegionImpl.srcOffset.y = copyRegion.srcOffset.y;
            copyRegionImpl.srcOffset.z = copyRegion.srcOffset.z;

            copyRegionImpl.dstSubresource.aspectMask = VkUtils::transformImageAspects(copyRegion.dstAspects);
            // TODO: Add support for mipmaps and layers
            copyRegionImpl.dstSubresource.mipLevel = 0;
            copyRegionImpl.dstSubresource.baseArrayLayer = 0;
            copyRegionImpl.dstSubresource.layerCount = 1;

            copyRegionImpl.dstOffset.x = copyRegion.dstOffset.x;
            copyRegionImpl.dstOffset.y = copyRegion.dstOffset.y;
            copyRegionImpl.dstOffset.z = copyRegion.dstOffset.z;

            copyRegionImpl.extent.width = std::max(copyRegion.extent.width, 1U);
            copyRegionImpl.extent.height = std::max(copyRegion.extent.height, 1U);
            copyRegionImpl.extent.depth = std::max(copyRegion.extent.depth, 1U);

            copyRegionsImpl.push_back(std::move(copyRegionImpl));
        }

        VulkanImage* srcImageImpl = static_cast<VulkanImage*>(srcImage.get());
        VulkanImage* dstImageImpl = static_cast<VulkanImage*>(dstImage.get());

        vkCmdCopyImage(
            commandBuffer,
            srcImageImpl->image,
            VkUtils::transformResourceStateToLayout(srcImage->getLayout()),
            dstImageImpl->image,
            VkUtils::transformResourceStateToLayout(dstImage->getLayout()),
            static_cast<uint32_t>(copyRegionsImpl.size()),
            copyRegionsImpl.data()
        );
    }

    void VulkanTransferCB::copyBufferToImage(
        const Buffer& srcBuffer,
        const Image& dstImage,
        const std::vector<BufferImageCopyRegion>& copyRegions
    ) {
        if (copyRegions.empty()) {
            COFFEE_WARNING("Empty array of copy regions was provided.");
            return;
        }

        COFFEE_ASSERT(srcBuffer != nullptr, "Invalid srcBuffer provided.");
        COFFEE_ASSERT(dstImage != nullptr, "Invalid dstImage provided.");

        COFFEE_ASSERT(
            (srcBuffer->getUsageFlags() & BufferUsage::TransferSource) == BufferUsage::TransferSource,
            "srcBuffer must have TransferSource flag set."
        );
        COFFEE_ASSERT(
            (dstImage->getUsageFlags() & ImageUsage::TransferDestination) == ImageUsage::TransferDestination,
            "dstImage must have TransferDestination flag set."
        );
        COFFEE_ASSERT(
            dstImage->getLayout() == ResourceState::CopyDestination || dstImage->getLayout() == ResourceState::UnorderedAccess,
            "dstImage must have layout set to CopyDestination or UnorderedAccess (this can be "
            "achieved by usage of pipeline barriers)"
        );

        std::vector<VkBufferImageCopy> copyRegionsImpl {};

        for (const auto& copyRegion : copyRegions) {
            VkBufferImageCopy copyRegionImpl {};

            COFFEE_ASSERT(copyRegion.bufferOffset < srcBuffer->getTotalSize(), "bufferOffset must be less than srcBuffer size.");
            COFFEE_ASSERT(
                copyRegion.bufferRowLength == 0 || copyRegion.bufferRowLength <= copyRegion.imageExtent.width,
                "bufferRowLength must be 0 or less than imageExtent.width"
            );
            COFFEE_ASSERT(
                copyRegion.bufferImageHeight == 0 || copyRegion.bufferImageHeight <= copyRegion.imageExtent.height,
                "bufferImageHeight must be 0 or less than imageExtent.height"
            );

            copyRegionImpl.bufferOffset = Math::roundToMultiple(copyRegion.bufferOffset, 4ULL);
            copyRegionImpl.bufferRowLength = copyRegion.bufferRowLength;
            copyRegionImpl.bufferImageHeight = copyRegion.bufferImageHeight;

            COFFEE_ASSERT(
                copyRegion.imageExtent.width <= dstImage->getWidth(),
                "imageExtent.width must be less or equal to dstImage width."
            );
            COFFEE_ASSERT(
                copyRegion.imageExtent.height <= dstImage->getHeight(),
                "imageExtent.height must be less or equal to dstImage height."
            );
            COFFEE_ASSERT(
                copyRegion.imageExtent.depth <= dstImage->getDepth(),
                "imageExtent.depth must be less or equal to dstImage depth."
            );

            COFFEE_ASSERT(
                copyRegion.imageExtent.width + copyRegion.imageOffset.x <= dstImage->getWidth(),
                "imageExtent.width + imageOffset.x must be less or equal to dstImage width."
            );
            COFFEE_ASSERT(
                copyRegion.imageExtent.height + copyRegion.imageOffset.y <= dstImage->getHeight(),
                "imageExtent.height + imageOffset.y must be less or equal to dstImage height."
            );
            COFFEE_ASSERT(
                copyRegion.imageExtent.depth + copyRegion.imageOffset.z <= dstImage->getDepth(),
                "imageExtent.height + imageOffset.z must be less or equal to dstImage depth."
            );

            copyRegionImpl.imageExtent.width = std::max(copyRegion.imageExtent.width, 1U);
            copyRegionImpl.imageExtent.height = std::max(copyRegion.imageExtent.height, 1U);
            copyRegionImpl.imageExtent.depth = std::max(copyRegion.imageExtent.depth, 1U);

            copyRegionImpl.imageOffset.x = copyRegion.imageOffset.x;
            copyRegionImpl.imageOffset.y = copyRegion.imageOffset.y;
            copyRegionImpl.imageOffset.z = copyRegion.imageOffset.z;

            COFFEE_ASSERT(
                (dstImage->getAspects() & copyRegion.aspects) == copyRegion.aspects,
                "dstImage doesn't have aspects, required in copyRegion"
            );

            if ((copyRegion.aspects & ImageAspect::Color) == ImageAspect::Color) {
                COFFEE_ASSERT(
                    (copyRegion.aspects & ImageAspect::Depth) != ImageAspect::Depth &&
                        (copyRegion.aspects & ImageAspect::Stencil) != ImageAspect::Stencil,
                    "aspects must not have set both color and depth/stencil flags."
                );
            }

            copyRegionImpl.imageSubresource.aspectMask = VkUtils::transformImageAspects(copyRegion.aspects);
            // TODO: Add support for mipmaps and layers
            copyRegionImpl.imageSubresource.mipLevel = 0;
            copyRegionImpl.imageSubresource.baseArrayLayer = 0;
            copyRegionImpl.imageSubresource.layerCount = 1;

            copyRegionsImpl.push_back(std::move(copyRegionImpl));
        }

        VulkanBuffer* srcBufferImpl = static_cast<VulkanBuffer*>(srcBuffer.get());
        VulkanImage* dstImageImpl = static_cast<VulkanImage*>(dstImage.get());

        vkCmdCopyBufferToImage(
            commandBuffer,
            srcBufferImpl->buffer,
            dstImageImpl->image,
            VkUtils::transformResourceStateToLayout(dstImage->getLayout()),
            static_cast<uint32_t>(copyRegionsImpl.size()),
            copyRegionsImpl.data()
        );
    }

    void VulkanTransferCB::copyImageToBuffer(
        const Image& srcImage,
        const Buffer& dstBuffer,
        const std::vector<BufferImageCopyRegion>& copyRegions
    ) {
        if (copyRegions.empty()) {
            COFFEE_WARNING("Empty array of copy regions was provided.");
            return;
        }

        COFFEE_ASSERT(srcImage != nullptr, "Invalid srcImage provided.");
        COFFEE_ASSERT(dstBuffer != nullptr, "Invalid dstBuffer provided.");

        COFFEE_ASSERT(
            (srcImage->getUsageFlags() & ImageUsage::TransferSource) == ImageUsage::TransferSource,
            "srcImage must have TransferSource flag set."
        );
        COFFEE_ASSERT(
            (dstBuffer->getUsageFlags() & BufferUsage::TransferDestination) == BufferUsage::TransferDestination,
            "dstBuffer must have TransferDestination flag set."
        );
        COFFEE_ASSERT(
            srcImage->getLayout() == ResourceState::CopySource || srcImage->getLayout() == ResourceState::UnorderedAccess,
            "srcImage must have layout set to CopySource or UnorderedAccess (this can be achieved "
            "by usage of pipeline barriers)"
        );

        std::vector<VkBufferImageCopy> copyRegionsImpl {};

        for (const auto& copyRegion : copyRegions) {
            VkBufferImageCopy copyRegionImpl {};

            COFFEE_ASSERT(copyRegion.bufferOffset < dstBuffer->getTotalSize(), "bufferOffset must be less than dstBuffer size.");
            COFFEE_ASSERT(
                copyRegion.bufferRowLength == 0 || copyRegion.bufferRowLength <= copyRegion.imageExtent.width,
                "bufferRowLength must be 0 or less than imageExtent.width"
            );
            COFFEE_ASSERT(
                copyRegion.bufferImageHeight == 0 || copyRegion.bufferImageHeight <= copyRegion.imageExtent.height,
                "bufferImageHeight must be 0 or less than imageExtent.height"
            );

            copyRegionImpl.bufferOffset = Math::roundToMultiple(copyRegion.bufferOffset, 4ULL);
            copyRegionImpl.bufferRowLength = copyRegion.bufferRowLength;
            copyRegionImpl.bufferImageHeight = copyRegion.bufferImageHeight;

            COFFEE_ASSERT(
                copyRegion.imageExtent.width <= srcImage->getWidth(),
                "imageExtent.width must be less or equal to srcImage width."
            );
            COFFEE_ASSERT(
                copyRegion.imageExtent.height <= srcImage->getHeight(),
                "imageExtent.height must be less or equal to srcImage height."
            );
            COFFEE_ASSERT(
                copyRegion.imageExtent.depth <= srcImage->getDepth(),
                "imageExtent.depth must be less or equal to srcImage depth."
            );

            COFFEE_ASSERT(
                copyRegion.imageExtent.width + copyRegion.imageOffset.x <= srcImage->getWidth(),
                "imageExtent.width + imageOffset.x must be less or equal to srcImage width."
            );
            COFFEE_ASSERT(
                copyRegion.imageExtent.height + copyRegion.imageOffset.y <= srcImage->getHeight(),
                "imageExtent.height + imageOffset.y must be less or equal to srcImage height."
            );
            COFFEE_ASSERT(
                copyRegion.imageExtent.depth + copyRegion.imageOffset.z <= srcImage->getDepth(),
                "imageExtent.height + imageOffset.z must be less or equal to srcImage depth."
            );

            copyRegionImpl.imageExtent.width = std::max(copyRegion.imageExtent.width, 1U);
            copyRegionImpl.imageExtent.height = std::max(copyRegion.imageExtent.height, 1U);
            copyRegionImpl.imageExtent.depth = std::max(copyRegion.imageExtent.depth, 1U);

            copyRegionImpl.imageOffset.x = copyRegion.imageOffset.x;
            copyRegionImpl.imageOffset.y = copyRegion.imageOffset.y;
            copyRegionImpl.imageOffset.z = copyRegion.imageOffset.z;

            COFFEE_ASSERT(
                (srcImage->getAspects() & copyRegion.aspects) == copyRegion.aspects,
                "srcImage doesn't have aspects, required in copyRegion"
            );

            if ((copyRegion.aspects & ImageAspect::Color) == ImageAspect::Color) {
                COFFEE_ASSERT(
                    (copyRegion.aspects & ImageAspect::Depth) != ImageAspect::Depth &&
                        (copyRegion.aspects & ImageAspect::Stencil) != ImageAspect::Stencil,
                    "aspects must not have set both color and depth/stencil flags."
                );
            }

            copyRegionImpl.imageSubresource.aspectMask = VkUtils::transformImageAspects(copyRegion.aspects);
            // TODO: Add support for mipmaps and layers
            copyRegionImpl.imageSubresource.mipLevel = 0;
            copyRegionImpl.imageSubresource.baseArrayLayer = 0;
            copyRegionImpl.imageSubresource.layerCount = 1;

            copyRegionsImpl.push_back(std::move(copyRegionImpl));
        }

        VulkanImage* srcImageImpl = static_cast<VulkanImage*>(srcImage.get());
        VulkanBuffer* dstBufferImpl = static_cast<VulkanBuffer*>(dstBuffer.get());

        vkCmdCopyImageToBuffer(
            commandBuffer,
            srcImageImpl->image,
            VkUtils::transformResourceStateToLayout(srcImageImpl->getLayout()),
            dstBufferImpl->buffer,
            static_cast<uint32_t>(copyRegionsImpl.size()),
            copyRegionsImpl.data()
        );
    }

    VulkanGraphicsCB::VulkanGraphicsCB(VulkanDevice& device) : VulkanTransferCB { device } {}

    void VulkanGraphicsCB::beginRenderPass(
        const RenderPass& renderPass,
        const Framebuffer& framebuffer,
        const Extent2D& renderArea,
        const Offset2D& offset
    ) {
        [[maybe_unused]] constexpr auto verifyBounds =
            [](const Framebuffer& framebuffer, const Extent2D& renderArea, const Offset2D& offset) noexcept -> bool {
            uint32_t combinedWidth = renderArea.width + offset.x;
            uint32_t combinedHeight = renderArea.height + offset.y;

            return combinedWidth <= framebuffer->getWidth() && combinedHeight <= framebuffer->getHeight();
        };

        COFFEE_ASSERT(
            !renderPassActive,
            "Vulkan doesn't allow you to nest render passes. You must run endRenderPass() before "
            "beginRenderPass()."
        );
        COFFEE_ASSERT(renderPass != nullptr, "Invalid RenderPass provided.");
        COFFEE_ASSERT(framebuffer != nullptr, "Invalid Framebuffer provided.");
        COFFEE_ASSERT(
            renderArea.width != 0 && renderArea.height != 0,
            "Invalid renderArea provided, both width and height must be more than 0."
        );
        COFFEE_ASSERT(offset.x >= 0 && offset.y >= 0, "Invalid offset provided, both x and y must be more or equal to 0.");
        COFFEE_ASSERT(verifyBounds(framebuffer, renderArea, offset), "renderArea + offset must be less or equal to framebuffer sizes.");

        VulkanRenderPass* renderPassImpl = static_cast<VulkanRenderPass*>(renderPass.get());
        VulkanFramebuffer* framebufferImpl = static_cast<VulkanFramebuffer*>(framebuffer.get());

        VkRenderPassBeginInfo beginInfo { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        beginInfo.renderPass = renderPassImpl->renderPass;
        beginInfo.framebuffer = framebufferImpl->framebuffer;

        beginInfo.renderArea.offset.x = offset.x;
        beginInfo.renderArea.offset.y = offset.y;
        beginInfo.renderArea.extent.width = renderArea.width;
        beginInfo.renderArea.extent.height = renderArea.height;

        beginInfo.clearValueCount = renderPassImpl->useClearValues ? static_cast<uint32_t>(renderPassImpl->clearValues.size()) : 0U;
        beginInfo.pClearValues = renderPassImpl->useClearValues ? renderPassImpl->clearValues.data() : nullptr;

        vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

        this->renderPassActive = true;
        this->hasDepthAttachment = renderPassImpl->hasDepthAttachment;
        this->renderArea = renderArea;
    }

    void VulkanGraphicsCB::endRenderPass() {
        COFFEE_ASSERT(renderPassActive, "No render pass is running. You must run beginRenderPass() before endRenderPass().");

        vkCmdEndRenderPass(commandBuffer);

        renderPassActive = false;
        layout = nullptr;
    }

    void VulkanGraphicsCB::bindPipeline(const Pipeline& pipeline) {
        COFFEE_ASSERT(pipeline != nullptr, "Invalid Pipeline provided.");

        VulkanPipeline* pipelineImpl = static_cast<VulkanPipeline*>(pipeline.get());

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineImpl->pipeline);

        layout = pipelineImpl->layout;
    }

    void VulkanGraphicsCB::bindDescriptorSet(const DescriptorSet& set) {
        COFFEE_ASSERT(layout != nullptr, "layout was nullptr. Did you forget to bindPipeline()?");
        COFFEE_ASSERT(set != nullptr, "Invalid DescriptorSet provided.");
        // TODO: Add assert that checks if current pipeline layout matches provided descriptor set

        constexpr auto bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        vkCmdBindDescriptorSets(commandBuffer, bindPoint, layout, 0U, 1U, &static_cast<VulkanDescriptorSet*>(set.get())->set, 0U, nullptr);
    }

    void VulkanGraphicsCB::bindDescriptorSets(const std::vector<DescriptorSet>& sets) {
        COFFEE_ASSERT(layout != nullptr, "layout was nullptr. Did you forget to bindPipeline()?");
        // TODO: Add assert that checks if current pipeline layout matches provided descriptor sets

        std::vector<VkDescriptorSet> implSets {};

        for (const auto& descriptorSet : sets) {
            COFFEE_ASSERT(descriptorSet != nullptr, "Invalid DescriptorSet provided.");
            implSets.push_back(static_cast<VulkanDescriptorSet*>(descriptorSet.get())->set);
        }

        constexpr auto bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        vkCmdBindDescriptorSets(commandBuffer, bindPoint, layout, 0U, static_cast<uint32_t>(implSets.size()), implSets.data(), 0U, nullptr);
    }

    void VulkanGraphicsCB::bindVertexBuffer(const Buffer& vertexBuffer, size_t offset) {
        VkDeviceSize offsets[] = { static_cast<VkDeviceSize>(offset) };
        vkCmdBindVertexBuffers(commandBuffer, 0U, 1U, &static_cast<VulkanBuffer*>(vertexBuffer.get())->buffer, offsets);
    }

    void VulkanGraphicsCB::bindIndexBuffer(const Buffer& indexBuffer, size_t offset) {
        vkCmdBindIndexBuffer(commandBuffer, static_cast<VulkanBuffer*>(indexBuffer.get())->buffer, offset, VK_INDEX_TYPE_UINT32);
    }

    void VulkanGraphicsCB::pushConstants(ShaderStage shaderStages, const void* data, uint32_t size, uint32_t offset) {
        COFFEE_ASSERT(layout != nullptr, "layout was nullptr. Did you forget to bindPipeline()?");

        VkShaderStageFlags implShaderStages = VK_SHADER_STAGE_ALL_GRAPHICS;

        if ((shaderStages & ShaderStage::All) != ShaderStage::All) {
            implShaderStages = 0;

            if ((shaderStages & ShaderStage::Vertex) == ShaderStage::Vertex) {
                implShaderStages |= VK_SHADER_STAGE_VERTEX_BIT;
            }

            if ((shaderStages & ShaderStage::Geometry) == ShaderStage::Geometry) {
                implShaderStages |= VK_SHADER_STAGE_GEOMETRY_BIT;
            }

            if ((shaderStages & ShaderStage::TessellationControl) == ShaderStage::TessellationControl) {
                implShaderStages |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            }

            if ((shaderStages & ShaderStage::TessellationEvaluation) == ShaderStage::TessellationEvaluation) {
                implShaderStages |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            }

            if ((shaderStages & ShaderStage::Fragment) == ShaderStage::Fragment) {
                implShaderStages |= VK_SHADER_STAGE_FRAGMENT_BIT;
            }

            if ((shaderStages & ShaderStage::Compute) == ShaderStage::Compute) {
                implShaderStages |= VK_SHADER_STAGE_COMPUTE_BIT;
            }
        }

        vkCmdPushConstants(commandBuffer, layout, implShaderStages, offset, size, data);
    }

    void VulkanGraphicsCB::setViewport(const Extent2D& area, const Offset2D& offset, float minDepth, float maxDepth) {
        COFFEE_ASSERT(area.width > 0U, "Width must be more than 0.");
        COFFEE_ASSERT(area.height > 0U, "Height must be more than 0.");

        COFFEE_ASSERT(
            std::numeric_limits<int32_t>::max() - area.width > offset.x,
            "Addition of width and X will cause an signed integer overflow."
        );
        COFFEE_ASSERT(
            std::numeric_limits<int32_t>::max() - area.height > offset.y,
            "Addition of height and Y will cause an signed integer overflow."
        );

        VkViewport viewport {};
        viewport.x = offset.x;
        viewport.y = offset.y;
        viewport.width = area.width;
        viewport.height = area.height;
        viewport.minDepth = minDepth;
        viewport.maxDepth = maxDepth;

        vkCmdSetViewport(commandBuffer, 0U, 1U, &viewport);
    }

    void VulkanGraphicsCB::setScissor(const Extent2D& area, const Offset2D& offset) {
        COFFEE_ASSERT(area.width > 0U, "Width must be more than 0.");
        COFFEE_ASSERT(area.height > 0U, "Height must be more than 0.");

        COFFEE_ASSERT(
            std::numeric_limits<int32_t>::max() - area.width > offset.x,
            "Addition of width and X will cause an signed integer overflow."
        );
        COFFEE_ASSERT(
            std::numeric_limits<int32_t>::max() - area.height > offset.y,
            "Addition of height and Y will cause an signed integer overflow."
        );

        VkRect2D scissor {};
        scissor.offset.x = offset.x;
        scissor.offset.y = offset.y;
        scissor.extent.width = area.width;
        scissor.extent.height = area.height;

        vkCmdSetScissor(commandBuffer, 0U, 1U, &scissor);
    }

    void VulkanGraphicsCB::setBlendColors(float red, float green, float blue, float alpha) {
        const float blendConstants[4] = { red, green, blue, alpha };

        vkCmdSetBlendConstants(commandBuffer, blendConstants);
    }

    void VulkanGraphicsCB::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
        vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void VulkanGraphicsCB::drawIndexed(
        uint32_t indexCount,
        uint32_t instanceCount,
        uint32_t firstIndex,
        uint32_t vertexOffset,
        uint32_t firstInstance
    ) {
        vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    void VulkanGraphicsCB::drawIndirect(const Buffer& drawBuffer, size_t offset, uint32_t drawCount, uint32_t stride) {
        vkCmdDrawIndirect(commandBuffer, static_cast<VulkanBuffer*>(drawBuffer.get())->buffer, offset, drawCount, stride);
    }

    void VulkanGraphicsCB::drawIndexedIndirect(const Buffer& drawBuffer, size_t offset, uint32_t drawCount, uint32_t stride) {
        vkCmdDrawIndexedIndirect(commandBuffer, static_cast<VulkanBuffer*>(drawBuffer.get())->buffer, offset, drawCount, stride);
    }

    // void VulkanCommandBuffer::dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t
    // groupCountZ) {
    //     vkCmdDispatch(
    //         commandBuffer,
    //         groupCountX,
    //         groupCountY,
    //         groupCountZ);
    // }

    // void VulkanCommandBuffer::dispatchIndirect(const Buffer& dispatchBuffer, size_t offset) {
    //     vkCmdDispatchIndirect(
    //         commandBuffer,
    //         static_cast<VulkanBuffer*>(dispatchBuffer.get())->buffer,
    //         offset);
    // }

} // namespace coffee