#ifndef COFFEE_GRAPHICS_SINGLE_TIME
#define COFFEE_GRAPHICS_SINGLE_TIME

#include <coffee/graphics/buffer.hpp>
#include <coffee/graphics/command_buffer.hpp>
#include <coffee/interfaces/resource_guard.hpp>

namespace coffee {

    namespace graphics {

        class SingleTime {
        public:
            static ScopeExit copyBuffer(
                const DevicePtr& device,
                const BufferPtr& dstBuffer,
                const BufferPtr& srcBuffer,
                VkDeviceSize dstOffset = 0ULL,
                VkDeviceSize srcOffset = 0ULL
            )
            {
                if (dstBuffer == srcBuffer) {
                    return {};
                }

                CommandBuffer commandBuffer = CommandBuffer::createTransfer(device);

                VkBufferCopy copyRegion {};
                copyRegion.srcOffset = srcOffset;
                copyRegion.dstOffset = dstOffset;
                copyRegion.size =
                    std::min(dstBuffer->instanceCount * dstBuffer->instanceSize, srcBuffer->instanceCount * srcBuffer->instanceSize);
                vkCmdCopyBuffer(commandBuffer, srcBuffer->buffer(), dstBuffer->buffer(), 1, &copyRegion);

                return device->singleTimeTransfer(std::move(commandBuffer));
            }

            static ScopeExit copyBufferToImage(const DevicePtr& device, const ImagePtr& dstImage, const BufferPtr& srcBuffer)
            {
                static constexpr auto topOfPipeStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                static constexpr auto transferStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                static constexpr auto fragmentStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

                VkImageMemoryBarrier barrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
                VkBufferImageCopy copyRegion {};

                CommandBuffer transferCommandBuffer = CommandBuffer::createTransfer(device);

                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = dstImage->image();
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.layerCount = 1;
                vkCmdPipelineBarrier(transferCommandBuffer, topOfPipeStage, transferStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

                copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.imageSubresource.layerCount = 1;
                copyRegion.imageExtent.width = dstImage->extent.width;
                copyRegion.imageExtent.height = dstImage->extent.height;
                copyRegion.imageExtent.depth = dstImage->extent.depth;
                transferCommandBuffer.copyBufferToImage(srcBuffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = dstImage->image();
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.layerCount = 1;
                VkPipelineStageFlagBits useStage = fragmentStage;

                if (!device->isUnifiedGraphicsTransferQueue()) {
                    barrier.dstAccessMask = 0;
                    barrier.srcQueueFamilyIndex = device->transferQueueFamilyIndex();
                    barrier.dstQueueFamilyIndex = device->graphicsQueueFamilyIndex();
                    useStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                }

                vkCmdPipelineBarrier(transferCommandBuffer, transferStage, useStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
                auto transferScope = device->singleTimeTransfer(std::move(transferCommandBuffer));

                if (!device->isUnifiedGraphicsTransferQueue()) {
                    CommandBuffer ownershipCommandBuffer = CommandBuffer::createGraphics(device);

                    barrier.srcAccessMask = 0;
                    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    barrier.srcQueueFamilyIndex = device->transferQueueFamilyIndex();
                    barrier.dstQueueFamilyIndex = device->graphicsQueueFamilyIndex();
                    barrier.image = dstImage->image();
                    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    barrier.subresourceRange.levelCount = 1;
                    barrier.subresourceRange.layerCount = 1;

                    vkCmdPipelineBarrier(ownershipCommandBuffer, topOfPipeStage, fragmentStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
                    return ScopeExit::combine(std::move(transferScope), device->singleTimeGraphics(std::move(ownershipCommandBuffer)));
                }

                return transferScope;
            }
        };

    } // namespace graphics

} // namespace coffee

#endif