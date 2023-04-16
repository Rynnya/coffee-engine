#ifndef COFFEE_ENGINE_FACTORY
#define COFFEE_ENGINE_FACTORY

#include <coffee/graphics/buffer.hpp>
#include <coffee/graphics/command_buffer.hpp>
#include <coffee/graphics/cursor.hpp>
#include <coffee/graphics/descriptors.hpp>
#include <coffee/graphics/device.hpp>
#include <coffee/graphics/framebuffer.hpp>
#include <coffee/graphics/image.hpp>
#include <coffee/graphics/keys.hpp>
#include <coffee/graphics/monitor.hpp>
#include <coffee/graphics/pipeline.hpp>
#include <coffee/graphics/render_pass.hpp>
#include <coffee/graphics/sampler.hpp>
#include <coffee/graphics/swap_chain.hpp>
#include <coffee/graphics/window.hpp>

#include <coffee/events/key_event.hpp>
#include <coffee/events/mouse_event.hpp>
#include <coffee/events/window_event.hpp>

#include <coffee/interfaces/archive.hpp>

#include <coffee/objects/model.hpp>
#include <coffee/objects/texture.hpp>
#include <coffee/objects/vertex.hpp>

#include <coffee/types.hpp>

#include <chrono>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>

namespace coffee {

    class Engine {
    public:
        static void initialize();
        static void destroy() noexcept;

        static Monitor primaryMonitor() noexcept;
        static const std::vector<Monitor>& monitors() noexcept;

        static void pollEvents();
        static void waitEventsTimeout(double timeout);

        static void waitFramelimit();
        // This call must be used only for debugging and closing down the application
        // It will shread your performance if you use it in main loop
        static void waitDeviceIdle();

        static float deltaTime() noexcept;
        static float framerateLimit() noexcept;
        static void setFramerateLimit(float framerateLimit) noexcept;

        static Model importModel(const std::string& filename);
        static Model importModel(const std::string& modelFile, const Archive& materialsArchive);
        static Texture importTexture(const std::string& filename, TextureType type);

        static std::string_view getClipboard() noexcept;
        static void setClipboard(const std::string& clipboard) noexcept;

        static void addMonitorConnectedCallback(const std::string& name, const std::function<void(const MonitorImpl&)>& callback);
        static void removeMonitorConnectedCallback(const std::string& name);
        static void addMonitorDisconnectedCallback(const std::string& name, const std::function<void(const MonitorImpl&)>& callback);
        static void removeMonitorDisconnectedCallback(const std::string& name);

        static void sendCommandBuffer(CommandBuffer&& commandBuffer);
        static void sendCommandBuffers(std::vector<CommandBuffer>&& commandBuffers);
        static void submitPendingWork();

        class Factory {
        public:
            static VkDeviceSize swapChainImageCount() noexcept;
            static VkFormat surfaceColorFormat() noexcept;

            static Window createWindow(const WindowSettings& settings = {}, const std::string& windowName = "Coffee Engine");

            static Cursor createCursor(CursorType type) noexcept;
            static Cursor createCursorFromImage(
                const std::vector<uint8_t>& rawImage,
                uint32_t width,
                uint32_t height,
                CursorType type
            ) noexcept;

            static Buffer createBuffer(const BufferConfiguration& configuration);
            static Image createImage(const ImageConfiguration& configuration);
            static Sampler createSampler(const SamplerConfiguration& configuration);

            inline static ImageView createImageView(const Image& image, const ImageViewConfiguration& configuration)
            {
                return std::make_shared<ImageViewImpl>(image, configuration);
            }

            static Shader createShader(const std::string& fileName, VkShaderStageFlagBits stage, const std::string& entrypoint = "main");
            static Shader createShader(
                const std::vector<uint8_t>& bytes,
                VkShaderStageFlagBits stage,
                const std::string& entrypoint = "main"
            );

            static DescriptorLayout createDescriptorLayout(const std::map<uint32_t, DescriptorBindingInfo>& bindings);
            static DescriptorSet createDescriptorSet(const DescriptorWriter& writer);

            static RenderPass createRenderPass(const RenderPassConfiguration& configuration);
            static Pipeline createPipeline(
                const RenderPass& renderPass,
                const std::vector<DescriptorLayout>& descriptorLayouts,
                const std::vector<Shader>& shaderPrograms,
                const PipelineConfiguration& configuration
            );
            static Framebuffer createFramebuffer(const RenderPass& renderPass, const FramebufferConfiguration& configuration);
            static CommandBuffer createCommandBuffer();

        private:
            constexpr Factory() noexcept = default;
        };

        class SingleTimeAction {
        public:
            static bool isUnifiedTransferGraphicsQueue() noexcept;
            static uint32_t transferQueueFamilyIndex() noexcept;
            static uint32_t graphicsQueueFamilyIndex() noexcept;

            static ScopeExit runTransfer(std::function<void(const CommandBuffer&)>&& action);
            static ScopeExit runGraphics(std::function<void(const CommandBuffer&)>&& action);

            static ScopeExit copyBuffer(
                const Buffer& dstBuffer,
                const Buffer& srcBuffer,
                VkDeviceSize dstOffset = 0ULL,
                VkDeviceSize srcOffset = 0ULL
            )
            {
                if (dstBuffer == srcBuffer) {
                    return {};
                }

                return SingleTimeAction::runTransfer([&](const CommandBuffer& commandBuffer) {
                    VkBufferCopy copyRegion {};
                    copyRegion.srcOffset = srcOffset;
                    copyRegion.dstOffset = dstOffset;
                    copyRegion.size = srcBuffer->instanceCount * srcBuffer->instanceSize;
                    vkCmdCopyBuffer(commandBuffer, srcBuffer->buffer(), dstBuffer->buffer(), 1, &copyRegion);
                });
            }

            static std::vector<ScopeExit> copyBufferToImage(const Image& dstImage, const Buffer& srcBuffer)
            {
                std::vector<ScopeExit> queueCompletionTokens {};

                queueCompletionTokens.push_back(SingleTimeAction::runTransfer([&](const CommandBuffer& commandBuffer) {
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

                    if (isUnifiedTransferGraphicsQueue()) {
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

                if (!isUnifiedTransferGraphicsQueue()) {
                    queueCompletionTokens.push_back(SingleTimeAction::runGraphics([&](const CommandBuffer& commandBuffer) {
                        VkImageMemoryBarrier useBarrier { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
                        useBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                        useBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                        useBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                        useBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                        useBarrier.srcQueueFamilyIndex = transferQueueFamilyIndex();
                        useBarrier.dstQueueFamilyIndex = graphicsQueueFamilyIndex();
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

        private:
            constexpr SingleTimeAction() noexcept = default;
        };

    private:
        constexpr Engine() noexcept = default;

        static void createNullTexture();

        static Texture createTexture(
            const uint8_t* rawBytes,
            size_t bufferSize,
            VkFormat format,
            uint32_t width,
            uint32_t height,
            const std::string& filePath,
            TextureType type
        );
        static Buffer createVerticesBuffer(const Vertex* vertices, size_t amount);
        static Buffer createIndicesBuffer(const uint32_t* indices, size_t amount);

        static VkFormat typeToFormat(TextureType type) noexcept;
    };

    using Factory = Engine::Factory;
    using SingleTimeAction = Engine::SingleTimeAction;

} // namespace coffee

#endif