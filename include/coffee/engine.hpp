#ifndef COFFEE_ENGINE_CORE
#define COFFEE_ENGINE_CORE

#include <coffee/abstract/backend.hpp>
#include <coffee/abstract/keys.hpp>

#include <coffee/events/application_event.hpp>
#include <coffee/events/key_event.hpp>
#include <coffee/events/mouse_event.hpp>
#include <coffee/events/window_event.hpp>

#include <coffee/interfaces/deferred_requests.hpp>
#include <coffee/interfaces/thread_pool.hpp>

#include <coffee/types.hpp>

#include <chrono>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>

namespace coffee {

    class Engine : DeferredRequests {
    public:
        Engine(BackendSelect backend, WindowSettings settings = {}, const std::string& applicationName = "Coffee Engine");
        ~Engine();

        void pollEvents();
        bool acquireFrame();
        void sendCommandBuffer(CommandBuffer&& commandBuffer);
        void endFrame();
        void wait();

        BackendSelect getBackendSelect() const noexcept;
        Format getColorFormat() const noexcept;
        Format getDepthFormat() const noexcept;
        uint32_t getCurrentImageIndex() const noexcept;
        const std::vector<Image>& getPresentImages() const noexcept;

        Buffer createBuffer(const BufferConfiguration& configuration);
        Image createImage(const ImageConfiguration& configuration);
        Sampler createSampler(const SamplerConfiguration& configuration);

        Shader createShader(const std::string& fileName, ShaderStage stage, const std::string& entrypoint = "main");
        Shader createShader(const std::vector<uint8_t>& bytes, ShaderStage stage, const std::string& entrypoint = "main");

        DescriptorLayout createDescriptorLayout(const std::map<uint32_t, DescriptorBindingInfo>& bindings);
        DescriptorSet createDescriptorSet(const DescriptorWriter& writer);

        RenderPass createRenderPass(
            const RenderPassConfiguration& configuration);
        RenderPass createRenderPass(
            const RenderPass& parent, const RenderPassConfiguration& configuration);
        Pipeline createPipeline(
            const RenderPass& renderPass,
            const PushConstants& pushConstants,
            const std::vector<DescriptorLayout>& descriptorLayouts,
            const ShaderProgram& shaderProgram,
            const PipelineConfiguration& configuration);
        Framebuffer createFramebuffer(
            const RenderPass& renderPass,
            const FramebufferConfiguration& configuration);
        CommandBuffer createCommandBuffer();

        void copyBuffer(Buffer& dstBuffer, const Buffer& srcBuffer);
        void copyBufferToImage(Image& dstImage, const Buffer& srcBuffer);

        void changePresentMode(PresentMode newMode);
        void waitDeviceIdle();

        bool isWindowFocused() const noexcept;
        void hideWindow() const noexcept;
        void showWindow() const noexcept;

        std::string_view getClipboard() const noexcept;
        void setClipboard(const std::string& clipboard);

        uint32_t getWindowWidth() const noexcept;
        uint32_t getWindowHeight() const noexcept;

        uint32_t getFramebufferWidth() const noexcept;
        uint32_t getFramebufferHeight() const noexcept;

        float getMouseX() const noexcept;
        float getMouseY() const noexcept;

        bool isButtonPressed(Keys key) const noexcept;
        bool isButtonPressed(MouseButton mouseButton) const noexcept;

        CursorState getCursorState() const noexcept;
        void showCursor();
        void hideCursor();
        void disableCursor();
        void setCursorPosition(float x, float y) const noexcept;

        bool isTextMode() const noexcept;
        void enableTextMode();
        void disableTextMode();

        float getDeltaTime() const noexcept;
        float getFrameLimit() const noexcept;
        void setFrameLimit(float newFrameLimit) noexcept;

        bool shouldExit() const noexcept;

        void addPresentModeCallback(const std::string& name, const std::function<void(const PresentModeEvent)>& callback);

        void addWindowFocusCallback(const std::string& name, const std::function<void(const WindowFocusEvent&)>& callback);
        void addWindowResizeCallback(const std::string& name, const std::function<void(const ResizeEvent&)>& callback);
        void addWindowEnterCallback(const std::string& name, const std::function<void(const WindowEnterEvent&)>& callback);

        void addMouseClickCallback(const std::string& name, const std::function<void(const MouseClickEvent&)>& callback);
        void addMousePositionCallback(const std::string& name, const std::function<void(const MouseMoveEvent&)>& callback);
        void addMouseWheelCallback(const std::string& name, const std::function<void(const MouseWheelEvent&)>& callback);

        void addKeyCallback(const std::string& name, const std::function<void(const KeyEvent&)>& callback);
        void addCharCallback(const std::string& name, const std::function<void(char32_t)>& callback);

        void removePresentModeCallback(const std::string& name);

        void removeWindowFocusCallback(const std::string& name);
        void removeWindowResizeCallback(const std::string& name);
        void removeWindowEnterCallback(const std::string& name);

        void removeMouseClickCallback(const std::string& name);
        void removeMousePositionCallback(const std::string& name);
        void removeMouseWheelCallback(const std::string& name);

        void removeKeyCallback(const std::string& name);
        void removeCharCallback(const std::string& name);

    private:
        BackendSelect currentBackend_;
        std::unique_ptr<AbstractBackend> backendImpl_ = nullptr;

        float framerateLimit_ = 60.0f;
        float deltaTime_ = 0.0f;
        std::chrono::high_resolution_clock::time_point lastPollTime_ {};

        std::map<std::string, std::function<void(const WindowFocusEvent&)>> windowFocusCallbacks_ {};
        std::map<std::string, std::function<void(const ResizeEvent&)>> windowResizeCallbacks_ {};
        std::map<std::string, std::function<void(const WindowEnterEvent&)>> windowEnterCallbacks_ {};
        std::map<std::string, std::function<void(const MouseClickEvent&)>> mouseClickCallbacks_ {};
        std::map<std::string, std::function<void(const MouseMoveEvent&)>> mousePositionCallbacks_ {};
        std::map<std::string, std::function<void(const MouseWheelEvent&)>> mouseWheelCallbacks_ {};
        std::map<std::string, std::function<void(const KeyEvent&)>> keyCallbacks_ {};
        std::map<std::string, std::function<void(char32_t)>> charCallbacks_ {};
        std::map<std::string, std::function<void(const PresentModeEvent&)>> presentModeCallbacks_ {};

        struct PImpl;
        PImpl* pImpl_;

        friend struct Engine::PImpl;
    };

}

#endif