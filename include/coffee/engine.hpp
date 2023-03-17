#ifndef COFFEE_ENGINE_FACTORY
#define COFFEE_ENGINE_FACTORY

#include <coffee/abstract/backend.hpp>
#include <coffee/abstract/keys.hpp>
#include <coffee/abstract/monitor.hpp>
#include <coffee/abstract/window.hpp>

#include <coffee/events/application_event.hpp>
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
        static void initialize(BackendAPI backend);
        static void destroy() noexcept;

        static BackendAPI getBackendType() noexcept;
        static Monitor getPrimaryMonitor() noexcept;
        static const std::vector<Monitor>& getMonitors() noexcept;

        static void pollEvents();
        static void wait();

        static float getDeltaTime() noexcept;
        static float getFramerateLimit() noexcept;
        static void setFramerateLimit(float framerateLimit) noexcept;

        static Model importModel(const std::string& filename);
        static Model importModel(const std::string& modelFile, const Archive& materialsArchive);
        static Texture importTexture(const std::string& filename, TextureType type);

        static void copyBuffer(Buffer& dstBuffer, const Buffer& srcBuffer);
        static void copyBufferToImage(Image& dstImage, const Buffer& srcBuffer);
        static void waitDeviceIdle();

        static std::string_view getClipboard() noexcept;
        static void setClipboard(const std::string& clipboard) noexcept;

        static void addMonitorConnectedCallback(const std::string& name, const std::function<void(const MonitorImpl&)>& callback);
        static void removeMonitorConnectedCallback(const std::string& name);
        static void addMonitorDisconnectedCallback(const std::string& name, const std::function<void(const MonitorImpl&)>& callback);
        static void removeMonitorDisconnectedCallback(const std::string& name);

        class Factory {
        public:
            static Window createWindow(WindowSettings settings = {}, const std::string& windowName = "Coffee Engine");

            static Buffer createBuffer(const BufferConfiguration& configuration);
            static Image createImage(const ImageConfiguration& configuration);
            static Sampler createSampler(const SamplerConfiguration& configuration);

            static Shader createShader(const std::string& fileName, ShaderStage stage, const std::string& entrypoint = "main");
            static Shader createShader(const std::vector<uint8_t>& bytes, ShaderStage stage, const std::string& entrypoint = "main");

            static DescriptorLayout createDescriptorLayout(const std::map<uint32_t, DescriptorBindingInfo>& bindings);
            static DescriptorSet createDescriptorSet(const DescriptorWriter& writer);

            static RenderPass createRenderPass(
                const RenderPassConfiguration& configuration);
            static Pipeline createPipeline(
                const RenderPass& renderPass,
                const std::vector<DescriptorLayout>& descriptorLayouts,
                const std::vector<Shader>& shaderPrograms,
                const PipelineConfiguration& configuration);
            static Framebuffer createFramebuffer(
                const RenderPass& renderPass,
                const FramebufferConfiguration& configuration);
            static CommandBuffer createCommandBuffer();

        private:
            constexpr Factory() noexcept = default;
        };

    private:
        constexpr Engine() noexcept = default;

        static void createNullTexture();

        static Texture createTexture(
            const uint8_t* rawBytes,
            size_t bufferSize,
            Format format,
            uint32_t width,
            uint32_t height,
            const std::string& filePath,
            TextureType type);
        static Buffer createVerticesBuffer(const std::vector<Vertex>& vertices);
        static Buffer createIndicesBuffer(const std::vector<uint32_t>& indices);

        static Format typeToFormat(TextureType type) noexcept;
    };

    using Factory = Engine::Factory;

}

#endif