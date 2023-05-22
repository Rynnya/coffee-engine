#pragma once

#include <coffee/graphics/buffer.hpp>
#include <coffee/graphics/command_buffer.hpp>
#include <coffee/interfaces/scope_exit.hpp>

namespace coffee {

    class SingleTime {
    public:
        static ScopeExit copyBuffer(
            const GPUDevicePtr& device,
            const BufferPtr& dstBuffer,
            const BufferPtr& srcBuffer,
            VkDeviceSize dstOffset = 0ULL,
            VkDeviceSize srcOffset = 0ULL
        )
        {
            if (dstBuffer == srcBuffer) {
                return {};
            }

            return device->singleTimeTransfer([&](const CommandBuffer& commandBuffer) {
                VkBufferCopy copyRegion {};
                copyRegion.srcOffset = srcOffset;
                copyRegion.dstOffset = dstOffset;
                copyRegion.size =
                    std::min(dstBuffer->instanceCount * dstBuffer->instanceSize, srcBuffer->instanceCount * srcBuffer->instanceSize);
                vkCmdCopyBuffer(commandBuffer, srcBuffer->buffer(), dstBuffer->buffer(), 1, &copyRegion);
            });
        }

        static std::vector<ScopeExit> copyBufferToImage(const GPUDevicePtr& device, const ImagePtr& dstImage, const BufferPtr& srcBuffer)
        {
            std::vector<ScopeExit> queueCompletionTokens {};

            queueCompletionTokens.push_back(device->singleTimeTransfer([&](const CommandBuffer& commandBuffer) {
                VkImageMemoryBarrier copyBarrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
                copyBarrier.srcAccessMask = 0;
                copyBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                copyBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                copyBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                copyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                copyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                copyBarrier.image = dstImage->image();
                copyBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyBarrier.subresourceRange.levelCount = 1;
                copyBarrier.subresourceRange.layerCount = 1;
                vkCmdPipelineBarrier(
                    commandBuffer,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0,
                    0,
                    nullptr,
                    0,
                    nullptr,
                    1,
                    &copyBarrier
                );

                VkBufferImageCopy copyRegion {};
                copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.imageSubresource.layerCount = 1;
                copyRegion.imageExtent.width = dstImage->extent.width;
                copyRegion.imageExtent.height = dstImage->extent.height;
                copyRegion.imageExtent.depth = dstImage->extent.depth;
                commandBuffer.copyBufferToImage(srcBuffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

                if (device->isUnifiedTransferGraphicsQueue()) {
                    VkImageMemoryBarrier useBarrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
                    useBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    useBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    useBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                    useBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    useBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    useBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    useBarrier.image = dstImage->image();
                    useBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    useBarrier.subresourceRange.levelCount = 1;
                    useBarrier.subresourceRange.layerCount = 1;
                    vkCmdPipelineBarrier(
                        commandBuffer,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        0,
                        0,
                        nullptr,
                        0,
                        nullptr,
                        1,
                        &useBarrier
                    );
                }
            }));

            if (!device->isUnifiedTransferGraphicsQueue()) {
                queueCompletionTokens.push_back(device->singleTimeGraphics([&](const CommandBuffer& commandBuffer) {
                    VkImageMemoryBarrier useBarrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
                    useBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    useBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    useBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                    useBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    useBarrier.srcQueueFamilyIndex = device->transferQueueFamilyIndex();
                    useBarrier.dstQueueFamilyIndex = device->graphicsQueueFamilyIndex();
                    useBarrier.image = dstImage->image();
                    useBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    useBarrier.subresourceRange.levelCount = 1;
                    useBarrier.subresourceRange.layerCount = 1;
                    vkCmdPipelineBarrier(
                        commandBuffer,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        0,
                        0,
                        nullptr,
                        0,
                        nullptr,
                        1,
                        &useBarrier
                    );
                }));
            }

            return queueCompletionTokens;
        }
    };

} // namespace coffee