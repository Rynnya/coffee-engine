#ifndef COFFEE_GRAPHICS_WINDOW
#define COFFEE_GRAPHICS_WINDOW

#include <coffee/events/key_event.hpp>
#include <coffee/events/mouse_event.hpp>
#include <coffee/events/window_event.hpp>
#include <coffee/graphics/cursor.hpp>
#include <coffee/graphics/swap_chain.hpp>
#include <coffee/interfaces/event_handler.hpp>
#include <coffee/interfaces/keys.hpp>

#include <coffee/types.hpp>
#include <coffee/utils/non_moveable.hpp>

#include <any>
#include <functional>
#include <mutex>

namespace coffee {

    namespace graphics {

        struct WindowSettings {
            // Leave as 0 to automatic selection
            VkExtent2D extent {};
            // FIFO - Available always, fallback if provided method isn't available
            // FIFO Relaxed - Automatically set as replacement for FIFO if supported by GPU
            // Mailbox - Applied if supported
            // Immediate - Applied if supported
            VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
            // Window will be hidden when created, so you can do actual things before presenting anything to user
            // Works only if windowed mode is used
            bool hiddenOnStart = true;
            // Defines if window should have borders (not recommended with fullscreen mode)
            bool borderless = false;
            // Defines if window should take full monitor resolution size (not recommended with borderless mode)
            bool fullscreen = false;
            // Allows cursor to have unlimited bounds, which is perfect solution for 3D
            bool cursorDisabled = false;
            // Defines if user input should be accelerated by an OS
            bool rawInput = true;
        };

        enum class CursorState : uint32_t {
            // Cursor visible and not bounded to window
            Visible = 0,
            // Cursor not visible, but still not bounded to window
            Hidden = 1,
            // Cursor not visible and bounded to window, meaning it can expand it positions up to double max
            Disabled = 2
        };

        enum class WindowState : uint32_t {
            // Normal window state
            Normal = 0,
            // Window is minimized, icon is shown in taskbar
            Iconified = 1,
            // Window is maximized, like, windowed fullscreen
            Maximized = 2
        };

        class Window;
        using WindowPtr = std::unique_ptr<Window>;

        class Window : NonMoveable {
        public:
            ~Window() noexcept;

            static WindowPtr create(const DevicePtr& device, WindowSettings settings, const std::string& windowName = "Coffee");

            inline const std::vector<ImagePtr>& getPresentImages() const noexcept { return swapChain_->getPresentImages(); }

            inline uint32_t getPresentIndex() const noexcept { return swapChain_->getPresentIndex(); }

            bool acquireNextImage();
            void sendCommandBuffer(CommandBuffer&& commandBuffer);
            void sendCommandBuffers(std::vector<CommandBuffer>&& commandBuffers);

            inline VkPresentModeKHR getPresentMode() const noexcept { return swapChain_->getPresentMode(); }

            void setPresentMode(VkPresentModeKHR newMode);

            const std::string& getWindowTitle() const noexcept;
            void setWindowTitle(const std::string& newTitle) const noexcept;

            bool isFocused() const noexcept;
            void focusWindow() const noexcept;

            WindowState getWindowState() const noexcept;
            void restoreWindow() const noexcept;
            void iconifyWindow() const noexcept;
            void maximizeWindow() const noexcept;

            bool isHidden() const noexcept;
            void hideWindow() const noexcept;
            void showWindow() const noexcept;

            bool isBorderless() const noexcept;
            void makeBorderless() const noexcept;
            void revertBorderless() const noexcept;

            bool isPassthrough() const noexcept;
            void enablePassthrough() const noexcept;
            void disablePassthrough() const noexcept;

            CursorState getCursorState() const noexcept;
            void showCursor() const noexcept;
            void hideCursor() const noexcept;
            void disableCursor() const noexcept;

            inline const CursorPtr& getCursor() const noexcept { return cursor_; }

            void setCursor(const CursorPtr& cursor) const noexcept;

            inline const Float2D& getMousePosition() const noexcept { return mousePosition_; }

            void setMousePosition(const Float2D& position) const noexcept;

            inline const VkOffset2D& getWindowPosition() const noexcept { return windowPosition_; }

            void setWindowPosition(const VkOffset2D& position) const noexcept;

            inline const VkExtent2D& getWindowSize() const noexcept { return windowSize_; }

            void setWindowSize(const VkExtent2D& size) const noexcept;

            inline const VkExtent2D& getFramebufferSize() const noexcept { return framebufferSize_; }

            static std::string getClipboard();
            static void setClipboard(const std::string& clipboard);

            bool isButtonPressed(Keys key) const noexcept;
            bool isButtonPressed(MouseButton mouseButton) const noexcept;

            void requestAttention() const noexcept;
            bool shouldClose() const noexcept;

            // clang-format off

            mutable Invokable<const Window&, const ResizeEvent&>            windowResizeEvent {};
            mutable Invokable<const Window&, const WindowEnterEvent&>       windowEnterEvent {};
            mutable Invokable<const Window&, const WindowPositionEvent&>    windowPositionEvent {};
            mutable Invokable<const Window&>                                windowCloseEvent {};
            mutable Invokable<const Window&, const WindowFocusEvent&>       windowFocusEvent {};
            mutable Invokable<const Window&, const MouseClickEvent&>        mouseClickEvent {};
            mutable Invokable<const Window&, const MouseMoveEvent&>         mouseMoveEvent {};
            mutable Invokable<const Window&, const MouseWheelEvent&>        mouseWheelEvent {};
            mutable Invokable<const Window&, const KeyEvent&>               keyEvent {};
            mutable Invokable<const Window&, char32_t>                      charEvent {};

            // clang-format on

            mutable std::any userData;

        private:
            Window(const DevicePtr& device, WindowSettings settings, const std::string& windowName);

            static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
            static void windowResizeCallback(GLFWwindow* window, int width, int height);
            static void windowEnterCallback(GLFWwindow* window, int entered);
            static void windowPositionCallback(GLFWwindow* window, int xpos, int ypos);
            static void windowCloseCallback(GLFWwindow* window);
            static void windowFocusCallback(GLFWwindow* window, int focused);

            static void mouseClickCallback(GLFWwindow* window, int button, int action, int mods);
            static void mousePositionCallback(GLFWwindow* window, double xpos, double ypos);
            static void mouseWheelCallback(GLFWwindow* window, double xoffset, double yoffset);

            static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
            static void charCallback(GLFWwindow* window, unsigned int codepoint);

            DevicePtr device_;

            GLFWwindow* windowHandle_ = nullptr;
            GLFWmonitor* monitorHandle_ = nullptr;
            VkSurfaceKHR surfaceHandle_ = VK_NULL_HANDLE;

            mutable std::string titleName_ {};
            mutable CursorPtr cursor_ = nullptr;

            mutable Float2D mousePosition_ {};
            mutable VkOffset2D windowPosition_ {};
            mutable VkExtent2D windowSize_ {};
            mutable VkExtent2D framebufferSize_ {};

            std::unique_ptr<SwapChain> swapChain_ = nullptr;
        };

    } // namespace graphics

} // namespace coffee

#endif