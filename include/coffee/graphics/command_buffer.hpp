#ifndef COFFEE_GRAPHICS_COMMAND_BUFFER
#define COFFEE_GRAPHICS_COMMAND_BUFFER

#include <coffee/graphics/buffer.hpp>
#include <coffee/graphics/descriptors.hpp>
#include <coffee/graphics/device.hpp>
#include <coffee/graphics/framebuffer.hpp>
#include <coffee/graphics/image.hpp>
#include <coffee/graphics/pipeline.hpp>
#include <coffee/graphics/render_pass.hpp>
#include <coffee/graphics/sampler.hpp>

#include <coffee/objects/model.hpp>
#include <coffee/utils/log.hpp>
#include <coffee/utils/non_copyable.hpp>

namespace coffee {

    // This class provided as way to abstract Volk because it disables intellisence function dispatch
    // Most of functions will be inlined anyway, so there little to no performance difference
    // Because of this there also will be added some asserts which will be deleted on release
    // Sometimes this can provide a easier way to handle routine, e.g vkCmdSetViewport
    // Some functions like vkCmdPipelineBarrier won't be declared here as they pretty big and must be handled by user instead

    namespace graphics {

        class CommandBuffer : NonCopyable {
        private:
            static constexpr uint32_t kUIntMax = std::numeric_limits<uint32_t>::max();

        public:
            ~CommandBuffer() noexcept;

            static CommandBuffer createGraphics(const DevicePtr& device);
            static CommandBuffer createCompute(const DevicePtr& device);
            static CommandBuffer createTransfer(const DevicePtr& device);

            CommandBuffer(CommandBuffer&& other) noexcept;
            CommandBuffer& operator=(CommandBuffer&&) = delete;

            inline void copyBuffer(const BufferPtr& srcBuffer, const BufferPtr& dstBuffer, size_t regionCount, const VkBufferCopy* pRegions)
                const noexcept
            {
                COFFEE_ASSERT(srcBuffer != nullptr, "Invalid srcBuffer provided.");
                COFFEE_ASSERT(dstBuffer != nullptr, "Invalid dstBuffer provided.");
                COFFEE_ASSERT(srcBuffer != dstBuffer, "srcBuffer must be other than dstBuffer.");

                COFFEE_ASSERT(
                    (srcBuffer->usageFlags & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) != 0,
                    "srcBuffer must be created with VK_BUFFER_USAGE_TRANSFER_SRC_BIT flag."
                );
                COFFEE_ASSERT(
                    (dstBuffer->usageFlags & VK_BUFFER_USAGE_TRANSFER_DST_BIT) != 0,
                    "dstBuffer must be created with VK_BUFFER_USAGE_TRANSFER_DST_BIT flag."
                );

                COFFEE_ASSERT(regionCount > 0, "regionCount must be greater than 0.");
                COFFEE_ASSERT(regionCount <= kUIntMax, "regionCount must be less than {}.", kUIntMax);
                COFFEE_ASSERT(pRegions != nullptr, "pRegions must be a valid pointer.");

                vkCmdCopyBuffer(buffer_, srcBuffer->buffer(), dstBuffer->buffer(), static_cast<uint32_t>(regionCount), pRegions);
            }

            inline void copyImage(
                const ImagePtr& srcImage,
                VkImageLayout srcImageLayout,
                const ImagePtr& dstImage,
                VkImageLayout dstImageLayout,
                size_t regionCount,
                const VkImageCopy* pRegions
            ) const noexcept
            {
                COFFEE_ASSERT(srcImage != nullptr, "Invalid srcImage provided.");
                COFFEE_ASSERT(dstImage != nullptr, "Invalid dstImage provided.");
                COFFEE_ASSERT(srcImage != dstImage, "srcImage must be other than dstImage.");

                COFFEE_ASSERT(regionCount > 0, "regionCount must be greater than 0.");
                COFFEE_ASSERT(regionCount <= kUIntMax, "regionCount must be less than {}.", kUIntMax);
                COFFEE_ASSERT(pRegions != nullptr, "pRegions must be a valid pointer.");

                vkCmdCopyImage(
                    buffer_,
                    srcImage->image(),
                    srcImageLayout,
                    dstImage->image(),
                    dstImageLayout,
                    static_cast<uint32_t>(regionCount),
                    pRegions
                );
            }

            inline void copyBufferToImage(
                const BufferPtr& srcBuffer,
                const ImagePtr& dstImage,
                VkImageLayout dstImageLayout,
                size_t regionCount,
                const VkBufferImageCopy* pRegions
            ) const noexcept
            {
                COFFEE_ASSERT(srcBuffer != nullptr, "Invalid srcBuffer provided.");
                COFFEE_ASSERT(dstImage != nullptr, "Invalid dstImage provided.");

                COFFEE_ASSERT(
                    (srcBuffer->usageFlags & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) != 0,
                    "srcBuffer must be created with VK_BUFFER_USAGE_TRANSFER_SRC_BIT flag."
                );

                COFFEE_ASSERT(regionCount > 0, "regionCount must be greater than 0.");
                COFFEE_ASSERT(regionCount <= kUIntMax, "regionCount must be less than {}.", kUIntMax);
                COFFEE_ASSERT(pRegions != nullptr, "pRegions must be a valid pointer.");

                vkCmdCopyBufferToImage(
                    buffer_,
                    srcBuffer->buffer(),
                    dstImage->image(),
                    dstImageLayout,
                    static_cast<uint32_t>(regionCount),
                    pRegions
                );
            }

            inline void copyImageToBuffer(
                const ImagePtr& srcImage,
                VkImageLayout srcImageLayout,
                const BufferPtr& dstBuffer,
                size_t regionCount,
                const VkBufferImageCopy* pRegions
            ) const noexcept
            {
                COFFEE_ASSERT(srcImage != nullptr, "Invalid srcImage provided.");
                COFFEE_ASSERT(dstBuffer != nullptr, "Invalid dstBuffer provided.");

                COFFEE_ASSERT(
                    (dstBuffer->usageFlags & VK_BUFFER_USAGE_TRANSFER_DST_BIT) != 0,
                    "dstBuffer must be created with VK_BUFFER_USAGE_TRANSFER_DST_BIT flag."
                );

                COFFEE_ASSERT(regionCount > 0, "regionCount must be greater than 0.");
                COFFEE_ASSERT(regionCount <= kUIntMax, "regionCount must be less than {}.", kUIntMax);
                COFFEE_ASSERT(pRegions != nullptr, "pRegions must be a valid pointer.");

                vkCmdCopyImageToBuffer(
                    buffer_,
                    srcImage->image(),
                    srcImageLayout,
                    dstBuffer->buffer(),
                    static_cast<uint32_t>(regionCount),
                    pRegions
                );
            }

            inline void blitImage(
                const ImagePtr& srcImage,
                VkImageLayout srcImageLayout,
                const ImagePtr& dstImage,
                VkImageLayout dstImageLayout,
                size_t regionCount,
                const VkImageBlit* pRegions,
                VkFilter filter = VK_FILTER_LINEAR
            ) const noexcept
            {
                COFFEE_ASSERT(srcImage != nullptr, "Invalid srcImage provided.");
                COFFEE_ASSERT(dstImage != nullptr, "Invalid dstImage provided.");
                COFFEE_ASSERT(srcImage != dstImage, "srcImage must be other than dstImage.");

                COFFEE_ASSERT(regionCount > 0, "regionCount must be greater than 0.");
                COFFEE_ASSERT(regionCount <= kUIntMax, "regionCount must be less than {}.", kUIntMax);
                COFFEE_ASSERT(pRegions != nullptr, "pRegions must be a valid pointer.");

                vkCmdBlitImage(
                    buffer_,
                    srcImage->image(),
                    srcImageLayout,
                    dstImage->image(),
                    dstImageLayout,
                    static_cast<uint32_t>(regionCount),
                    pRegions,
                    filter
                );
            }

            inline void bindPipeline(VkPipelineBindPoint bindPoint, const PipelinePtr& pipeline) const noexcept
            {
                COFFEE_ASSERT(type != CommandBufferType::Transfer, "You cannot bind pipeline to transfer command buffer.");

                COFFEE_ASSERT(pipeline != nullptr, "Invalid pipeline provided.");

                vkCmdBindPipeline(buffer_, bindPoint, pipeline->pipeline());
            }

            inline void bindDescriptorSets(
                VkPipelineBindPoint bindPoint,
                const PipelinePtr& pipeline,
                const DescriptorSetPtr& firstDescriptor,
                size_t firstSet = 0U
            ) const noexcept
            {
                COFFEE_ASSERT(type != CommandBufferType::Transfer, "You cannot bind descriptor sets to transfer command buffer.");
                COFFEE_ASSERT(pipeline != nullptr, "Invalid pipeline provided.");

                COFFEE_ASSERT(firstDescriptor != nullptr, "firstDescriptor must be a valid pointer.");

                vkCmdBindDescriptorSets(buffer_, bindPoint, pipeline->layout(), firstSet, 1, &firstDescriptor->set(), 0, nullptr);
            }

            inline void bindDescriptorSets(
                VkPipelineBindPoint bindPoint,
                const PipelinePtr& pipeline,
                const DescriptorSetPtr& firstDescriptor,
                const DescriptorSetPtr& secondDescriptor,
                size_t firstSet = 0U
            ) const noexcept
            {
                COFFEE_ASSERT(type != CommandBufferType::Transfer, "You cannot bind descriptor sets to transfer command buffer.");
                COFFEE_ASSERT(pipeline != nullptr, "Invalid pipeline provided.");

                COFFEE_ASSERT(firstDescriptor != nullptr, "firstDescriptor must be a valid pointer.");
                COFFEE_ASSERT(secondDescriptor != nullptr, "secondDescriptor must be a valid pointer.");

                VkDescriptorSet sets[2] = { firstDescriptor->set(), secondDescriptor->set() };
                vkCmdBindDescriptorSets(buffer_, bindPoint, pipeline->layout(), firstSet, 2, sets, 0, nullptr);
            }

            inline void bindDescriptorSets(
                VkPipelineBindPoint bindPoint,
                const PipelinePtr& pipeline,
                const DescriptorSetPtr& firstDescriptor,
                const DescriptorSetPtr& secondDescriptor,
                const DescriptorSetPtr& thirdDescriptor,
                size_t firstSet = 0U
            ) const noexcept
            {
                COFFEE_ASSERT(type != CommandBufferType::Transfer, "You cannot bind descriptor sets to transfer command buffer.");
                COFFEE_ASSERT(pipeline != nullptr, "Invalid pipeline provided.");

                COFFEE_ASSERT(firstDescriptor != nullptr, "firstDescriptor must be a valid pointer.");
                COFFEE_ASSERT(secondDescriptor != nullptr, "secondDescriptor must be a valid pointer.");
                COFFEE_ASSERT(thirdDescriptor != nullptr, "thirdDescriptor must be a valid pointer.");

                VkDescriptorSet sets[3] = { firstDescriptor->set(), secondDescriptor->set(), thirdDescriptor->set() };
                vkCmdBindDescriptorSets(buffer_, bindPoint, pipeline->layout(), firstSet, 3, sets, 0, nullptr);
            }

            inline void bindDescriptorSets(
                VkPipelineBindPoint bindPoint,
                const PipelinePtr& pipeline,
                const DescriptorSetPtr& firstDescriptor,
                const DescriptorSetPtr& secondDescriptor,
                const DescriptorSetPtr& thirdDescriptor,
                const DescriptorSetPtr& fourthDescriptor
            ) const noexcept
            {
                COFFEE_ASSERT(type != CommandBufferType::Transfer, "You cannot bind descriptor sets to transfer command buffer.");
                COFFEE_ASSERT(pipeline != nullptr, "Invalid pipeline provided.");

                COFFEE_ASSERT(firstDescriptor != nullptr, "firstDescriptor must be a valid pointer.");
                COFFEE_ASSERT(secondDescriptor != nullptr, "secondDescriptor must be a valid pointer.");
                COFFEE_ASSERT(thirdDescriptor != nullptr, "thirdDescriptor must be a valid pointer.");
                COFFEE_ASSERT(fourthDescriptor != nullptr, "fourthDescriptor must be a valid pointer.");

                VkDescriptorSet sets[4] = { firstDescriptor->set(),
                                            secondDescriptor->set(),
                                            thirdDescriptor->set(),
                                            fourthDescriptor->set() };
                vkCmdBindDescriptorSets(buffer_, bindPoint, pipeline->layout(), 0, 4, sets, 0, nullptr);
            }

            inline void pushConstants(
                const PipelinePtr& pipeline,
                VkShaderStageFlags stageFlags,
                size_t size,
                const void* pValues,
                size_t offset = 0U
            ) const noexcept
            {
                COFFEE_ASSERT(type != CommandBufferType::Transfer, "You cannot push constants to transfer command buffer.");
                COFFEE_ASSERT(pipeline != nullptr, "Invalid pipeline provided.");

                COFFEE_ASSERT(size > 0, "size must be greater than 0.");
                COFFEE_ASSERT(size <= kUIntMax, "size must be less than {}.", kUIntMax);
                COFFEE_ASSERT(pValues != nullptr, "pValues must be a valid pointer.");
                COFFEE_ASSERT(offset <= kUIntMax, "offset must be less than {}.", kUIntMax);

                vkCmdPushConstants(
                    buffer_,
                    pipeline->layout(),
                    stageFlags,
                    static_cast<uint32_t>(offset),
                    static_cast<uint32_t>(size),
                    pValues
                );
            }

            inline void beginRenderPass(
                const RenderPassPtr& renderPass,
                const FramebufferPtr& framebuffer,
                const VkRect2D& renderArea,
                size_t clearValueCount,
                const VkClearValue* pClearValues
            ) const noexcept
            {
                COFFEE_ASSERT(type == CommandBufferType::Graphics, "CommandBufferType must be Graphics.");

                COFFEE_ASSERT(renderPass != nullptr, "Invalid renderPass provided.");
                COFFEE_ASSERT(framebuffer != nullptr, "Invalid framebuffer provided.");

                COFFEE_ASSERT(
                    renderArea.extent.width > 0 && renderArea.extent.height > 0,
                    "renderArea extent members must be greater than 0."
                );

                VkRenderPassBeginInfo renderPassBeginInfo { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
                renderPassBeginInfo.renderPass = renderPass->renderPass();
                renderPassBeginInfo.framebuffer = framebuffer->framebuffer();
                renderPassBeginInfo.renderArea = renderArea;
                renderPassBeginInfo.clearValueCount = clearValueCount;
                renderPassBeginInfo.pClearValues = pClearValues;
                vkCmdBeginRenderPass(buffer_, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
            }

            inline void endRenderPass() const noexcept
            {
                COFFEE_ASSERT(type == CommandBufferType::Graphics, "CommandBufferType must be Graphics.");

                vkCmdEndRenderPass(buffer_);
            }

            inline void setViewport(const VkViewport& viewport) const noexcept
            {
                COFFEE_ASSERT(type == CommandBufferType::Graphics, "CommandBufferType must be Graphics.");

                // Without extensions this function only allowed to accept one viewport at the time
                vkCmdSetViewport(buffer_, 0, 1, &viewport);
            }

            inline void setScissor(const VkRect2D& scissor) const noexcept
            {
                COFFEE_ASSERT(type == CommandBufferType::Graphics, "CommandBufferType must be Graphics.");

                // Without extensions this function only allowed to accept one scissor at the time
                vkCmdSetScissor(buffer_, 0, 1, &scissor);
            }

            inline void setBlendColors(const float blendConstants[4]) const noexcept
            {
                COFFEE_ASSERT(type == CommandBufferType::Graphics, "CommandBufferType must be Graphics.");

                COFFEE_ASSERT(blendConstants != nullptr, "Invalid blend constants provided.");

                vkCmdSetBlendConstants(buffer_, blendConstants);
            }

            inline void bindModel(const ModelPtr& model) const noexcept
            {
                COFFEE_ASSERT(type == CommandBufferType::Graphics, "CommandBufferType must be Graphics.");

                COFFEE_ASSERT(model != nullptr, "Invalid model provided.");

                VkBuffer buffers[] = { model->verticesBuffer->buffer() };
                VkDeviceSize offsets[] = { 0ULL };
                vkCmdBindVertexBuffers(buffer_, 0U, 1U, buffers, offsets);

                if (model->indicesBuffer != nullptr) {
                    vkCmdBindIndexBuffer(buffer_, model->indicesBuffer->buffer(), 0U, VK_INDEX_TYPE_UINT32);
                }
            }

            inline void bindVertexBuffers(
                size_t bindingCount,
                const VkBuffer* pBuffers,
                const VkDeviceSize* pOffsets,
                size_t firstBinding = 0U
            ) const noexcept
            {
                COFFEE_ASSERT(type == CommandBufferType::Graphics, "CommandBufferType must be Graphics.");

                COFFEE_ASSERT(bindingCount > 0, "bindingCount must be greater than 0.");
                COFFEE_ASSERT(bindingCount <= kUIntMax, "bindingCount must be less than {}.", kUIntMax);
                COFFEE_ASSERT(pBuffers != nullptr, "pBuffers must be a valid pointer.");
                COFFEE_ASSERT(pOffsets != nullptr, "pOffsets must be a valid pointer.");
                COFFEE_ASSERT(firstBinding <= kUIntMax, "firstBinding must be less than {}.", kUIntMax);

                vkCmdBindVertexBuffers(
                    buffer_,
                    static_cast<uint32_t>(firstBinding),
                    static_cast<uint32_t>(bindingCount),
                    pBuffers,
                    pOffsets
                );
            }

            inline void bindIndexBuffer(const BufferPtr& indexBuffer, VkDeviceSize offset = 0ULL) const noexcept
            {
                COFFEE_ASSERT(type == CommandBufferType::Graphics, "CommandBufferType must be Graphics.");

                COFFEE_ASSERT(indexBuffer != nullptr, "Invalid indexBuffer provided.");

                vkCmdBindIndexBuffer(buffer_, indexBuffer->buffer(), offset, VK_INDEX_TYPE_UINT32);
            }

            inline void draw(uint32_t vertexCount, uint32_t instanceCount = 1U, uint32_t firstVertex = 0U, uint32_t firstInstance = 0U)
                const noexcept
            {
                COFFEE_ASSERT(type == CommandBufferType::Graphics, "CommandBufferType must be Graphics.");

                vkCmdDraw(buffer_, vertexCount, instanceCount, firstVertex, firstInstance);
            }

            // Note that you must call bindModel() before using mesh drawing
            inline void drawMesh(const Mesh& mesh) const noexcept
            {
                COFFEE_ASSERT(type == CommandBufferType::Graphics, "CommandBufferType must be Graphics.");

                if (mesh.indicesCount == 0) {
                    vkCmdDraw(buffer_, mesh.verticesCount, 1U, mesh.verticesOffset, 0U);
                    return;
                }

                vkCmdDrawIndexed(buffer_, mesh.indicesCount, 1U, mesh.indicesOffset, mesh.verticesOffset, 0U);
            }

            // Note that you must call bindModel() before using model drawing
            inline void drawModel(const ModelPtr& model) const noexcept
            {
                COFFEE_ASSERT(type == CommandBufferType::Graphics, "CommandBufferType must be Graphics.");

                COFFEE_ASSERT(model != nullptr, "Invalid model provided.");

                if (model->indicesBuffer == nullptr) {
                    for (const auto& mesh : model->meshes) {
                        vkCmdDraw(buffer_, mesh.verticesCount, 1U, mesh.verticesOffset, 0U);
                    }
                }
                else {
                    for (const auto& mesh : model->meshes) {
                        vkCmdDrawIndexed(buffer_, mesh.indicesCount, 1U, mesh.indicesOffset, mesh.verticesOffset, 0U);
                    }
                }
            }

            inline void drawIndexed(
                uint32_t indexCount,
                uint32_t instanceCount = 1U,
                uint32_t firstIndex = 0U,
                uint32_t vertexOffset = 0U,
                uint32_t firstInstance = 0U
            ) const noexcept
            {
                COFFEE_ASSERT(type == CommandBufferType::Graphics, "CommandBufferType must be Graphics.");

                vkCmdDrawIndexed(buffer_, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
            }

            inline void drawIndirect(const BufferPtr& drawBuffer, VkDeviceSize offset = 0U, uint32_t drawCount = 1U, uint32_t stride = 0U)
                const noexcept
            {
                COFFEE_ASSERT(type == CommandBufferType::Graphics, "CommandBufferType must be Graphics.");

                COFFEE_ASSERT(drawBuffer != nullptr, "Invalid drawBuffer provided.");

                vkCmdDrawIndirect(buffer_, drawBuffer->buffer(), offset, drawCount, stride);
            }

            inline void drawIndexedIndirect(
                const BufferPtr& drawBuffer,
                VkDeviceSize offset = 0U,
                uint32_t drawCount = 1U,
                uint32_t stride = 0U
            ) const noexcept
            {
                COFFEE_ASSERT(type == CommandBufferType::Graphics, "CommandBufferType must be Graphics.");

                COFFEE_ASSERT(drawBuffer != nullptr, "Invalid drawBuffer provided.");

                vkCmdDrawIndexedIndirect(buffer_, drawBuffer->buffer(), offset, drawCount, stride);
            }

            inline void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) const noexcept
            {
                COFFEE_ASSERT(type == CommandBufferType::Compute, "CommandBufferType must be Compute.");

                vkCmdDispatch(buffer_, groupCountX, groupCountY, groupCountZ);
            }

            inline void dispatchIndirect(const BufferPtr& dispatchBuffer, VkDeviceSize offset = 0U) const noexcept
            {
                COFFEE_ASSERT(type == CommandBufferType::Compute, "CommandBufferType must be Compute.");

                COFFEE_ASSERT(dispatchBuffer != nullptr, "Invalid dispatchBuffer provided.");

                vkCmdDispatchIndirect(buffer_, dispatchBuffer->buffer(), offset);
            }

            // Provides strong pipeline synchronization between two scopes
            // WARNING: Must not be used in production as can cause huge overhead
            // Use this only as debugging tool to catch some nesty bugs
            inline void fullPipelineBarrier() const noexcept
            {
                constexpr VkAccessFlags allAccessFlags =
                    VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
                    VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT | VK_ACCESS_SHADER_READ_BIT |
                    VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                    VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT;

                VkMemoryBarrier memoryBarrier { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
                memoryBarrier.srcAccessMask = allAccessFlags;
                memoryBarrier.dstAccessMask = allAccessFlags;

                vkCmdPipelineBarrier(
                    buffer_,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    0,
                    1,
                    &memoryBarrier,
                    0,
                    nullptr,
                    0,
                    nullptr
                );
            };

            // Provided as way to add additional functionality if not present in this implementation
            // WARNING: Using this naked pointer will allow to forbidden usage of Vulkan, please look closely to Validation Errors
            inline operator VkCommandBuffer() const noexcept { return buffer_; }

            const CommandBufferType type;

        private:
            CommandBuffer(const DevicePtr& device, CommandBufferType type);

            DevicePtr device_;

            VkCommandPool pool_ = VK_NULL_HANDLE;
            VkCommandBuffer buffer_ = VK_NULL_HANDLE;

            friend class Device;
        };

    } // namespace graphics

} // namespace coffee

#endif