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
        COFFEE_ASSERT(
            !renderPassActive, "Vulkan doesn't allow you to nest render passes. You must run endRenderPass() before beginRenderPass().");
        COFFEE_ASSERT(renderArea.width != 0 && renderArea.height != 0, "Invalid renderArea provided, both width and height must be more than 0.");
        COFFEE_ASSERT(
            renderArea.width > offset.x && renderArea.height > offset.y, "Both offset x and y must be less than renderArea width and height.");

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
        VulkanPipeline* pipelineImpl = static_cast<VulkanPipeline*>(pipeline.get());

        vkCmdBindPipeline(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineImpl->pipeline);

        layout = pipelineImpl->layout;
    }

    void VulkanCommandBuffer::bindDescriptorSet(const DescriptorSet& set) {
        COFFEE_ASSERT(layout != nullptr, "layout was nullptr. Did you forget to bindPipeline()?");

        constexpr auto bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        vkCmdBindDescriptorSets(commandBuffer, bindPoint, layout, 0U, 1U, &static_cast<VulkanDescriptorSet*>(set.get())->set, 0U, nullptr);
    }

    void VulkanCommandBuffer::bindDescriptorSets(const std::vector<DescriptorSet>& sets) {
        COFFEE_ASSERT(layout != nullptr, "layout was nullptr. Did you forget to bindPipeline()?");

        std::vector<VkDescriptorSet> implSets {};

        for (const auto& descriptorSet : sets) {
            COFFEE_ASSERT(descriptorSet != nullptr, "Invalid DescriptorSet provided.");
            implSets.push_back(static_cast<VulkanDescriptorSet*>(descriptorSet.get())->set);
        }

        constexpr auto bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        vkCmdBindDescriptorSets(commandBuffer, bindPoint, layout, 0U, static_cast<uint32_t>(implSets.size()), implSets.data(), 0U, nullptr);
    }

    //void VulkanCommandBuffer::clearAttachments(
    //    const Framebuffer& framebuffer,
    //    const ClearColorValue& colorValues,
    //    const std::optional<ClearDepthStencilValue>& depthValues,
    //    const Offset2D& offset, 
    //    const Extent2D& clearArea
    //) {
    //    COFFEE_ASSERT(renderPassActive, "No render pass is running. You must run beginRenderPass() before clearAttachments().");
    //    COFFEE_ASSERT(
    //        !hasDepthAttachment_ || (hasDepthAttachment_ && depthValues.has_value()),
    //        "Render pass have a depth attachment, but it wasn't provided.");

    //    std::vector<VkClearAttachment> clearValues {};
    //    clearValues.resize(1ULL + hasDepthAttachment_);

    //    clearValues[0].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    //    clearValues[0].colorAttachment = 0;
    //    clearValues[0].clearValue.color.float32[0] = colorValues.float32[0];
    //    clearValues[0].clearValue.color.float32[1] = colorValues.float32[1];
    //    clearValues[0].clearValue.color.float32[2] = colorValues.float32[2];
    //    clearValues[0].clearValue.color.float32[3] = colorValues.float32[3];

    //    if (hasDepthAttachment_) {
    //        clearValues[1].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    //        clearValues[1].clearValue.depthStencil.depth = depthValues->depth;
    //        clearValues[1].clearValue.depthStencil.stencil = depthValues->stencil;
    //    }

    //    std::vector<VkClearRect> rects {};
    //    rects.resize(1ULL + hasDepthAttachment_);

    //    for (auto& clearRect : rects) {
    //        clearRect.rect.offset.x = offset.x;
    //        clearRect.rect.offset.y = offset.y;
    //        clearRect.rect.extent.width = clearArea.width == 0 ? renderArea_.width : clearArea.width;
    //        clearRect.rect.extent.height = clearArea.height == 0 ? renderArea_.height : clearArea.height;

    //        clearRect.layerCount = 1;
    //    }

    //    vkCmdClearAttachments(
    //        commandBuffer, 
    //        static_cast<uint32_t>(clearValues.size()),
    //        clearValues.data(),
    //        static_cast<uint32_t>(rects.size()),
    //        rects.data());
    //}

    void VulkanCommandBuffer::setViewport(float width, float height, float x, float y, float minDepth, float maxDepth) {
        COFFEE_ASSERT(width > 0.0f, "Width must be more than 0.");
        COFFEE_ASSERT(height > 0.0f, "Height must be more than 0.");

        VkViewport viewport {}; 
        viewport.x = x;
        viewport.y = y;
        viewport.width = width;
        viewport.height = height;
        viewport.minDepth = minDepth;
        viewport.maxDepth = maxDepth;

        vkCmdSetViewport(commandBuffer, 0U, 1U, &viewport);
    }

    void VulkanCommandBuffer::setScissor(uint32_t width, uint32_t height, uint32_t x, uint32_t y) {
        COFFEE_ASSERT(
            std::numeric_limits<int32_t>::max() - width > x, "Addition of width and X will cause an signed integer overflow.");
        COFFEE_ASSERT(
            std::numeric_limits<int32_t>::max() - height > y, "Addition of height and Y will cause an signed integer overflow.");

        VkRect2D scissor {};
        scissor.offset.x = x;
        scissor.offset.y = y;
        scissor.extent.width = width;
        scissor.extent.height = height;

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