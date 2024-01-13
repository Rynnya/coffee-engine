#include <coffee/graphics/window.hpp>

#include <coffee/graphics/exceptions.hpp>
#include <coffee/utils/log.hpp>
#include <coffee/utils/platform.hpp>

#include <oneapi/tbb/queuing_mutex.h>

#include <atomic>
#include <functional>
#include <unordered_map>

namespace coffee {

    static tbb::queuing_mutex creationMutex {};

    namespace graphics {

        Window::Window(const DevicePtr& device, WindowSettings settings, const std::string& windowName) : device_ { device }
        {
            std::string safeWindowName { windowName };

            if (safeWindowName.empty()) {
                safeWindowName = "Coffee Window";
            }

            {
                tbb::queuing_mutex::scoped_lock lock { creationMutex };

                glfwDefaultWindowHints();
                glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
                monitorHandle_ = glfwGetPrimaryMonitor();
                //
                if (monitorHandle_ == nullptr) {
                    COFFEE_ERROR("Failed to get primary monitor handle!");

                    throw GLFWException { "Failed to get primary monitor handle!" };
                }

                if (settings.fullscreen && settings.borderless) {
                    COFFEE_WARNING(
                    "It's generally not recommended to use Fullscreen and Borderless mode at the same time. "
                    "If you have any issues - try to disable one of those!");
                }

                // Window cannot be hidden when using fullscreen mode
                // Otherwise it will cause a window to be (0, 0)
                if (settings.hiddenOnStart && !settings.fullscreen) {
                    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
                }

                if (!settings.fullscreen) {
                    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
                }

                if (settings.borderless) {
                    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
                }

                if (settings.extent.width == 0 || settings.extent.height == 0) {
                    const GLFWvidmode* videoMode = glfwGetVideoMode(monitorHandle_);

                    if (videoMode == nullptr) {
                        COFFEE_ERROR("Failed to retreave main video mode of primary monitor!");

                        throw GLFWException { "Failed to retreave main video mode of primary monitor!" };
                    }

                    settings.extent.width = static_cast<uint32_t>(settings.fullscreen ? videoMode->width : videoMode->width - videoMode->width / 2);
                    settings.extent.height =
                        static_cast<uint32_t>(settings.fullscreen ? videoMode->height : videoMode->height - videoMode->height / 2);
                }

                windowHandle_ = glfwCreateWindow(
                    settings.extent.width,
                    settings.extent.height,
                    safeWindowName.c_str(),
                    settings.fullscreen ? monitorHandle_ : nullptr,
                    nullptr
                );

                if (windowHandle_ == nullptr) {
                    COFFEE_ERROR("Failed to create new GLFW window!");

                    throw GLFWException { "Failed to create new GLFW window!" };
                }
            }

            titleName_ = safeWindowName;

            int width {}, height {};
            glfwGetWindowSize(windowHandle_, &width, &height);
            windowSize_ = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

            glfwGetFramebufferSize(windowHandle_, &width, &height);
            framebufferSize_ = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

            // Hack: This required to forbid usage of possible extent of (0, 0)
            glfwSetWindowSizeLimits(windowHandle_, 1, 1, GLFW_DONT_CARE, GLFW_DONT_CARE);

            glfwSetInputMode(windowHandle_, GLFW_STICKY_KEYS, GLFW_TRUE);
            if (settings.cursorDisabled) {
                glfwSetInputMode(windowHandle_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }

            if (glfwRawMouseMotionSupported() && settings.rawInput) {
                glfwSetInputMode(windowHandle_, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            }

            // Window related callbacks
            glfwSetFramebufferSizeCallback(windowHandle_, framebufferResizeCallback);
            glfwSetWindowSizeCallback(windowHandle_, windowResizeCallback);
            glfwSetCursorEnterCallback(windowHandle_, windowEnterCallback);
            glfwSetWindowPosCallback(windowHandle_, windowPositionCallback);
            glfwSetWindowCloseCallback(windowHandle_, windowCloseCallback);
            glfwSetWindowFocusCallback(windowHandle_, windowFocusCallback);

            // Mouse related callbacks
            glfwSetMouseButtonCallback(windowHandle_, mouseClickCallback);
            glfwSetCursorPosCallback(windowHandle_, mousePositionCallback);
            glfwSetScrollCallback(windowHandle_, mouseWheelCallback);

            // Buttons related callbacks
            glfwSetKeyCallback(windowHandle_, keyCallback);
            glfwSetCharCallback(windowHandle_, charCallback);

            glfwSetWindowUserPointer(windowHandle_, this);
            VkResult result = glfwCreateWindowSurface(device_->instance(), windowHandle_, nullptr, &surfaceHandle_);

            if (result != VK_SUCCESS) {
                COFFEE_ERROR("Failed to create window surface!");

                throw RegularVulkanException { result };
            }

            swapChain_ = std::make_unique<SwapChain>(device_, surfaceHandle_, framebufferSize_, settings.presentMode);
        }

        Window::~Window() noexcept
        {
            swapChain_ = nullptr;

            vkDestroySurfaceKHR(device_->instance(), surfaceHandle_, nullptr);

            glfwDestroyWindow(windowHandle_);
        }

        WindowPtr Window::create(const DevicePtr& device, WindowSettings settings, const std::string& windowName)
        {
            COFFEE_ASSERT(device != nullptr, "Invalid device provided.");

            return std::unique_ptr<Window>(new Window { device, settings, windowName });
        }

        bool Window::acquireNextImage()
        {
            bool result = swapChain_->acquireNextImage();

            if (!result) {
                swapChain_->recreate(framebufferSize_, swapChain_->getPresentMode());

                const ResizeEvent resizeEvent { framebufferSize_.width, framebufferSize_.height };
                windowResizeEvent(*this, resizeEvent);
            }

            return result;
        }

        void Window::sendCommandBuffer(CommandBuffer&& commandBuffer)
        {
            std::vector<CommandBuffer> commandBuffers {};
            commandBuffers.push_back(std::move(commandBuffer));

            swapChain_->submit(std::move(commandBuffers));
        }

        void Window::sendCommandBuffers(std::vector<CommandBuffer>&& commandBuffers) { swapChain_->submit(std::move(commandBuffers)); }

        void Window::setPresentMode(VkPresentModeKHR newMode)
        {
            if (swapChain_->getPresentMode() == newMode) {
                return;
            }

            while (framebufferSize_.width == 0 || framebufferSize_.height == 0) {
                glfwWaitEvents();
            }

            swapChain_->recreate(framebufferSize_, newMode);
        }

        const std::string& Window::getWindowTitle() const noexcept { return titleName_; }

        void Window::setWindowTitle(const std::string& newTitle) const noexcept
        {
            if (newTitle.empty()) {
                return;
            }

            glfwSetWindowTitle(windowHandle_, newTitle.data());
            titleName_ = newTitle;
        }

        bool Window::isFocused() const noexcept { return glfwGetWindowAttrib(windowHandle_, GLFW_FOCUSED) == GLFW_TRUE; }

        void Window::focusWindow() const noexcept { glfwFocusWindow(windowHandle_); }

        WindowState Window::getWindowState() const noexcept
        {
            if (glfwGetWindowAttrib(windowHandle_, GLFW_ICONIFIED) == GLFW_TRUE) {
                return WindowState::Iconified;
            }

            if (glfwGetWindowAttrib(windowHandle_, GLFW_MAXIMIZED) == GLFW_TRUE) {
                return WindowState::Maximized;
            }

            return WindowState::Normal;
        }

        void Window::restoreWindow() const noexcept { glfwRestoreWindow(windowHandle_); }

        void Window::iconifyWindow() const noexcept { glfwIconifyWindow(windowHandle_); }

        void Window::maximizeWindow() const noexcept { glfwMaximizeWindow(windowHandle_); }

        bool Window::isHidden() const noexcept { return glfwGetWindowAttrib(windowHandle_, GLFW_VISIBLE) == GLFW_FALSE; }

        void Window::hideWindow() const noexcept { glfwHideWindow(windowHandle_); }

        void Window::showWindow() const noexcept { glfwShowWindow(windowHandle_); }

        bool Window::isBorderless() const noexcept { return glfwGetWindowAttrib(windowHandle_, GLFW_DECORATED) == GLFW_FALSE; }

        void Window::makeBorderless() const noexcept { glfwSetWindowAttrib(windowHandle_, GLFW_DECORATED, GLFW_FALSE); }

        void Window::revertBorderless() const noexcept { glfwSetWindowAttrib(windowHandle_, GLFW_DECORATED, GLFW_TRUE); }

        bool Window::isPassthrough() const noexcept { return glfwGetWindowAttrib(windowHandle_, GLFW_MOUSE_PASSTHROUGH) == GLFW_TRUE; }

        void Window::enablePassthrough() const noexcept { glfwSetWindowAttrib(windowHandle_, GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE); }

        void Window::disablePassthrough() const noexcept { glfwSetWindowAttrib(windowHandle_, GLFW_MOUSE_PASSTHROUGH, GLFW_FALSE); }

        CursorState Window::getCursorState() const noexcept
        {
            switch (glfwGetInputMode(windowHandle_, GLFW_CURSOR)) {
                default:
                case GLFW_CURSOR_NORMAL:
                    return CursorState::Visible;
                case GLFW_CURSOR_HIDDEN:
                    return CursorState::Hidden;
                case GLFW_CURSOR_DISABLED:
                    return CursorState::Disabled;
            }
        }

        void Window::showCursor() const noexcept { glfwSetInputMode(windowHandle_, GLFW_CURSOR, GLFW_CURSOR_NORMAL); }

        void Window::hideCursor() const noexcept { glfwSetInputMode(windowHandle_, GLFW_CURSOR, GLFW_CURSOR_HIDDEN); }

        void Window::disableCursor() const noexcept { glfwSetInputMode(windowHandle_, GLFW_CURSOR, GLFW_CURSOR_DISABLED); }

        void Window::setCursor(const CursorPtr& cursor) const noexcept
        {
            glfwSetCursor(windowHandle_, cursor != nullptr ? cursor->cursor_ : nullptr);
            cursor_ = cursor;
        }

        void Window::setMousePosition(const Float2D& position) const noexcept
        {
            glfwSetCursorPos(windowHandle_, static_cast<double>(position.x), static_cast<double>(position.y));
        }

        void Window::setWindowPosition(const VkOffset2D& position) const noexcept { glfwSetWindowPos(windowHandle_, position.x, position.y); }

        void Window::setWindowSize(const VkExtent2D& size) const noexcept
        {
            glfwSetWindowSize(windowHandle_, size.width, size.height);
            windowSize_ = framebufferSize_ = size;
        }

        std::string Window::getClipboard() { return glfwGetClipboardString(nullptr); }

        void Window::setClipboard(const std::string& clipboard) { glfwSetClipboardString(nullptr, clipboard.data()); }

        bool Window::isButtonPressed(Keys key) const noexcept { return glfwGetKey(windowHandle_, *key) == GLFW_PRESS; }

        bool Window::isButtonPressed(MouseButton mouseButton) const noexcept { return glfwGetMouseButton(windowHandle_, *mouseButton) == GLFW_PRESS; }

        void Window::requestAttention() const noexcept { glfwRequestWindowAttention(windowHandle_); }

        bool Window::shouldClose() const noexcept { return glfwWindowShouldClose(windowHandle_) == GLFW_TRUE; }

        void Window::framebufferResizeCallback(GLFWwindow* window, int width, int height)
        {
            Window* windowPtr = static_cast<Window*>(glfwGetWindowUserPointer(window));

            // Discard all callbacks when window is minimized
            if (width == 0 || height == 0) {
                return;
            }

            // !!! Always recreate swap chain, even if previous size is same as new one, otherwise it
            // will freeze image
            windowPtr->framebufferSize_ = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

            auto& swapChain = windowPtr->swapChain_;
            swapChain->recreate(windowPtr->framebufferSize_, swapChain->getPresentMode());

            const ResizeEvent resizeEvent { windowPtr->framebufferSize_.width, windowPtr->framebufferSize_.height };
            windowPtr->windowResizeEvent(*windowPtr, resizeEvent);
        }

        void Window::windowResizeCallback(GLFWwindow* window, int width, int height)
        {
            // This callback most likely will be called with framebufferResizeCallback, so we don't do
            // any callbacks or actions here
            Window* windowPtr = static_cast<Window*>(glfwGetWindowUserPointer(window));

            windowPtr->windowSize_ = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
        }

        void Window::windowEnterCallback(GLFWwindow* window, int entered)
        {
            Window* windowPtr = static_cast<Window*>(glfwGetWindowUserPointer(window));

            const WindowEnterEvent enterEvent { static_cast<bool>(entered) };
            windowPtr->windowEnterEvent(*windowPtr, enterEvent);
        }

        void Window::windowPositionCallback(GLFWwindow* window, int xpos, int ypos)
        {
            Window* windowPtr = static_cast<Window*>(glfwGetWindowUserPointer(window));

            windowPtr->windowPosition_ = { xpos, ypos };

            const WindowPositionEvent positionEvent { xpos, ypos };
            windowPtr->windowPositionEvent(*windowPtr, positionEvent);
        }

        void Window::windowCloseCallback(GLFWwindow* window)
        {
            Window* windowPtr = static_cast<Window*>(glfwGetWindowUserPointer(window));

            windowPtr->windowCloseEvent(*windowPtr);
        }

        void Window::windowFocusCallback(GLFWwindow* window, int focused)
        {
            Window* windowPtr = static_cast<Window*>(glfwGetWindowUserPointer(window));

            const WindowFocusEvent focusEvent { static_cast<bool>(focused) };
            windowPtr->windowFocusEvent(*windowPtr, focusEvent);
        }

        void Window::mouseClickCallback(GLFWwindow* window, int button, int action, int mods)
        {
            Window* windowPtr = static_cast<Window*>(glfwGetWindowUserPointer(window));

            const MouseClickEvent mouseClickEvent { glfwToCoffeeState(action), static_cast<MouseButton>(button), static_cast<uint32_t>(mods) };
            windowPtr->mouseClickEvent(*windowPtr, mouseClickEvent);
        }

        void Window::mousePositionCallback(GLFWwindow* window, double xpos, double ypos)
        {
            Window* windowPtr = static_cast<Window*>(glfwGetWindowUserPointer(window));

            const MouseMoveEvent moveEvent { static_cast<float>(xpos), static_cast<float>(ypos) };
            windowPtr->mouseMoveEvent(*windowPtr, moveEvent);

            windowPtr->mousePosition_ = { static_cast<float>(xpos), static_cast<float>(ypos) };
        }

        void Window::mouseWheelCallback(GLFWwindow* window, double xoffset, double yoffset)
        {
            Window* windowPtr = static_cast<Window*>(glfwGetWindowUserPointer(window));

            const MouseWheelEvent wheelEvent { static_cast<float>(xoffset), static_cast<float>(yoffset) };
            windowPtr->mouseWheelEvent(*windowPtr, wheelEvent);
        }

        void Window::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
        {
            Window* windowPtr = static_cast<Window*>(glfwGetWindowUserPointer(window));

            auto state = glfwToCoffeeState(action);
            const KeyEvent keyEvent { state, static_cast<Keys>(key), static_cast<uint32_t>(scancode), static_cast<uint32_t>(mods) };

            windowPtr->keyEvent(*windowPtr, keyEvent);
        }

        void Window::charCallback(GLFWwindow* window, unsigned int codepoint)
        {
            Window* windowPtr = static_cast<Window*>(glfwGetWindowUserPointer(window));
            windowPtr->charEvent(*windowPtr, static_cast<char32_t>(codepoint));
        }

    } // namespace graphics

} // namespace coffee