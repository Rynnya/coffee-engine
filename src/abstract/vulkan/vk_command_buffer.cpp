#include <coffee/abstract/vulkan/vk_command_buffer.hpp>

#include <coffee/abstract/vulkan/vk_buffer.hpp>
#include <coffee/abstract/vulkan/vk_image.hpp>
#include <coffee/abstract/vulkan/vk_descriptors.hpp>
#include <coffee/abstract/vulkan/vk_framebuffer.hpp>
#include <coffee/abstract/vulkan/vk_pipeline.hpp>
#include <coffee/abstract/vulkan/vk_render_pass.hpp>

#include <coffee/utils/log.hpp>
#include <coffee/utils/vk_utils.hpp>

namespace coffee {

    VulkanCommandBuffer::VulkanCommandBuffer(VulkanDevice& device) : device_ { device } {
        pool = device_.getUniqueCommandPool();

        VkCommandBufferAllocateInfo allocateInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        allocateInfo.commandPool = pool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1;

        COFFEE_THROW_IF(
            vkAllocateCommandBuffers(device_.getLogicalDevice(), &allocateInfo, &commandBuffer) != VK_SUCCESS,
            "Failed to allocate command buffer!");

        VkCommandBufferBeginInfo beginInfo { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        COFFEE_THROW_IF(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS, "Failed to begin command buffer recording!");
    }

    VulkanCommandBuffer::~VulkanCommandBuffer() {
        if (pool != nullptr) { // Implementation will take ownership of pool when submitting to queue, so pool must be checked
            vkFreeCommandBuffers(device_.getLogicalDevice(), pool, 1U, &commandBuffer);
            device_.returnCommandPool(pool);
        }
    }

    void VulkanCommandBuffer::beginRenderPass(
        const RenderPass& renderPass,
        const Framebuffer& framebuffer,
        const Extent2D& renderArea,
        const Offset2D& offset
    ) {
        [[ maybe_unused ]] constexpr auto verifyBounds = [](
            const Framebuffer& framebuffer,
            const Extent2D& renderArea,
            const Offset2D& offset
        ) noexcept -> bool {
            uint32_t combinedWidth = renderArea.width + offset.x;
            uint32_t combinedHeight = renderArea.height + offset.y;

            return combinedWidth <= framebuffer->getWidth() && combinedHeight <= framebuffer->getHeight();
        };

        COFFEE_ASSERT(!renderPassActive, "Vulkan doesn't allow you to nest render passes. You must run endRenderPass() before beginRenderPass().");
        COFFEE_ASSERT(renderPass != nullptr, "Invalid RenderPass provided.");
        COFFEE_ASSERT(framebuffer != nullptr, "Invalid Framebuffer provided.");
        COFFEE_ASSERT(renderArea.width != 0 && renderArea.height != 0, "Invalid renderArea provided, both width and height must be more than 0.");
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

        renderPassActive = true;
        hasDepthAttachment_ = renderPassImpl->hasDepthAttachment;
        renderArea_ = renderArea;
    }

    void VulkanCommandBuffer::endRenderPass() {
        COFFEE_ASSERT(renderPassActive, "No render pass is running. You must run beginRenderPass() before endRenderPass().");

        vkCmdEndRenderPass(commandBuffer);

        renderPassActive = false;
        layout = nullptr;
    }

    void VulkanCommandBuffer::bindPipeline(const Pipeline& pipeline) {
        COFFEE_ASSERT(pipeline != nullptr, "Invalid Pipeline provided.");

        VulkanPipeline* pipelineImpl = static_cast<VulkanPipeline*>(pipeline.get());

        vkCmdBindPipeline(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineImpl->pipeline);

        layout = pipelineImpl->layout;
    }

    void VulkanCommandBuffer::bindDescriptorSet(const DescriptorSet& set) {
        COFFEE_ASSERT(layout != nullptr, "layout was nullptr. Did you forget to bindPipeline()?");
        COFFEE_ASSERT(set != nullptr, "Invalid DescriptorSet provided.");
        // TODO: Add assert that checks if current pipeline layout matches provided descriptor set

        constexpr auto bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        vkCmdBindDescriptorSets(commandBuffer, bindPoint, layout, 0U, 1U, &static_cast<VulkanDescriptorSet*>(set.get())->set, 0U, nullptr);
    }

    void VulkanCommandBuffer::bindDescriptorSets(const std::vector<DescriptorSet>& sets) {
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

    void VulkanCommandBuffer::setViewport(const Extent2D& area, const Offset2D& offset, float minDepth, float maxDepth) {
        COFFEE_ASSERT(area.width > 0U, "Width must be more than 0.");
        COFFEE_ASSERT(area.height > 0U, "Height must be more than 0.");

        COFFEE_ASSERT(
            std::numeric_limits<int32_t>::max() - area.width > offset.x, "Addition of width and X will cause an signed integer overflow.");
        COFFEE_ASSERT(
            std::numeric_limits<int32_t>::max() - area.height > offset.y, "Addition of height and Y will cause an signed integer overflow.");

        VkViewport viewport {}; 
        viewport.x = offset.x;
        viewport.y = offset.y;
        viewport.width = area.width;
        viewport.height = area.height;
        viewport.minDepth = minDepth;
        viewport.maxDepth = maxDepth;

        vkCmdSetViewport(commandBuffer, 0U, 1U, &viewport);
    }

    void VulkanCommandBuffer::setScissor(const Extent2D& area, const Offset2D& offset) {
        COFFEE_ASSERT(area.width > 0U, "Width must be more than 0.");
        COFFEE_ASSERT(area.height > 0U, "Height must be more than 0.");

        COFFEE_ASSERT(
            std::numeric_limits<int32_t>::max() - area.width > offset.x, "Addition of width and X will cause an signed integer overflow.");
        COFFEE_ASSERT(
            std::numeric_limits<int32_t>::max() - area.height > offset.y, "Addition of height and Y will cause an signed integer overflow.");

        VkRect2D scissor {};
        scissor.offset.x = offset.x;
        scissor.offset.y = offset.y;
        scissor.extent.width = area.width;
        scissor.extent.height = area.height;

        vkCmdSetScissor(commandBuffer, 0U, 1U, &scissor);
    }

    void VulkanCommandBuffer::setBlendColors(float red, float green, float blue, float alpha) {
        const float blendConstants[4] = { red, green, blue, alpha };

        vkCmdSetBlendConstants(commandBuffer, blendConstants);
    }

    void VulkanCommandBuffer::pushConstants(ShaderStage shaderStages, const void* data, uint32_t size, uint32_t offset) {
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

    void VulkanCommandBuffer::bindVertexBuffer(const Buffer& vertexBuffer, size_t offset) {
        VkDeviceSize offsets[] = { static_cast<VkDeviceSize>(offset) };
        vkCmdBindVertexBuffers(commandBuffer, 0U, 1U, &static_cast<VulkanBuffer*>(vertexBuffer.get())->buffer, offsets);
    }

    void VulkanCommandBuffer::bindIndexBuffer(const Buffer& indexBuffer, size_t offset) {
        vkCmdBindIndexBuffer(
            commandBuffer,
            static_cast<VulkanBuffer*>(indexBuffer.get())->buffer,
            offset,
            VK_INDEX_TYPE_UINT32);
    }

    void VulkanCommandBuffer::draw(
        uint32_t vertexCount,
        uint32_t instanceCount,
        uint32_t firstVertex,
        uint32_t firstInstance
    ) {
        vkCmdDraw(
            commandBuffer,
            vertexCount,
            instanceCount,
            firstVertex,
            firstInstance);
    }

    void VulkanCommandBuffer::drawIndexed(
        uint32_t indexCount,
        uint32_t instanceCount,
        uint32_t firstIndex,
        uint32_t vertexOffset,
        uint32_t firstInstance
    ) {
        vkCmdDrawIndexed(
            commandBuffer,
            indexCount,
            instanceCount,
            firstIndex,
            vertexOffset,
            firstInstance);
    }

    void VulkanCommandBuffer::drawIndirect(const Buffer& drawBuffer, size_t offset, uint32_t drawCount, uint32_t stride) {
        vkCmdDrawIndirect(
            commandBuffer,
            static_cast<VulkanBuffer*>(drawBuffer.get())->buffer,
            offset,
            drawCount,
            stride);
    }

    void VulkanCommandBuffer::drawIndexedIndirect(const Buffer& drawBuffer, size_t offset, uint32_t drawCount, uint32_t stride) {
        vkCmdDrawIndexedIndirect(
            commandBuffer,
            static_cast<VulkanBuffer*>(drawBuffer.get())->buffer,
            offset,
            drawCount,
            stride);
    }

    void VulkanCommandBuffer::memoryBarrier(const MemoryBarrier& memoryBarrier) {

    }

    void VulkanCommandBuffer::bufferBarrier(const BufferBarrier& bufferBarrier) {

    }

    void VulkanCommandBuffer::imageBarrier(const ImageBarrier& imageBarrier) {
        VulkanImage* imageImpl = static_cast<VulkanImage*>(imageBarrier.image.get());

        VkImageLayout oldLayout = VkUtils::transformResourceStateToLayout(imageBarrier.oldLayout);
        VkImageLayout newLayout = VkUtils::transformResourceStateToLayout(imageBarrier.newLayout);

        VkAccessFlags srcAccessMask = VkUtils::imageLayoutToAccessFlags(oldLayout);
        VkAccessFlags dstAccessMask = VkUtils::transformResourceStateToAccess(imageBarrier.newLayout);

        VkPipelineStageFlags srcStages = [&]() -> VkPipelineStageFlags {
            if (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
                return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            }

            if (srcAccessMask != 0) {
                return VkUtils::accessFlagsToPipelineStages(srcAccessMask);
            }

            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        }();

        VkPipelineStageFlags dstStages = [&]() -> VkPipelineStageFlags {
            if (newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
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
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = imageImpl->image;
        barrier.subresourceRange.aspectMask = imageImpl->aspectMask;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;

        vkCmdPipelineBarrier(
            commandBuffer,
            srcStages,
            dstStages,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
    }

    void VulkanCommandBuffer::dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
        vkCmdDispatch(
            commandBuffer,
            groupCountX,
            groupCountY,
            groupCountZ);
    }

    void VulkanCommandBuffer::dispatchIndirect(const Buffer& dispatchBuffer, size_t offset) {
        vkCmdDispatchIndirect(
            commandBuffer,
            static_cast<VulkanBuffer*>(dispatchBuffer.get())->buffer,
            offset);
    }

}