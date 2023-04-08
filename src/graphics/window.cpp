#include <coffee/graphics/window.hpp>

#include <coffee/utils/log.hpp>
#include <coffee/utils/platform.hpp>

#include <atomic>
#include <functional>
#include <mutex>
#include <unordered_map>

namespace coffee {

    static std::mutex creationMutex {};

    WindowImpl::WindowImpl(Device& device, WindowSettings settings, const std::string& windowName) : device_ { device }
    {
        std::string safeWindowName { windowName };

        if (safeWindowName.empty()) {
            safeWindowName = "Coffee Window";
        }

        {
            std::scoped_lock<std::mutex> lock { creationMutex };

            glfwDefaultWindowHints();
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

            monitorHandle_ = glfwGetPrimaryMonitor();
            COFFEE_THROW_IF(monitorHandle_ == nullptr, "Failed to get primary monitor handle!");

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
                COFFEE_THROW_IF(videoMode == nullptr, "Failed to retreave main video mode of primary monitor!");

                settings.extent.width =
                    static_cast<uint32_t>(settings.fullscreen ? videoMode->width : videoMode->width - videoMode->width / 2);
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
            COFFEE_THROW_IF(windowHandle_ == nullptr, "Failed to create new GLFW window!");
        }

        titleName_ = safeWindowName;

        int width {}, height {};
        glfwGetWindowSize(windowHandle_, &width, &height);
        windowWidth_ = static_cast<uint32_t>(width);
        windowHeight_ = static_cast<uint32_t>(height);

        glfwGetFramebufferSize(windowHandle_, &width, &height);
        framebufferWidth_ = static_cast<uint32_t>(width);
        framebufferHeight_ = static_cast<uint32_t>(height);

        windowFocused_ = static_cast<bool>(glfwGetWindowAttrib(windowHandle_, GLFW_FOCUSED));
        windowIconified_ = static_cast<bool>(glfwGetWindowAttrib(windowHandle_, GLFW_ICONIFIED));

        // Hack: This required to forbid usage of possible extent of (0, 0)
        glfwSetWindowSizeLimits(windowHandle_, 1, 1, GLFW_DONT_CARE, GLFW_DONT_CARE);

        glfwSetInputMode(windowHandle_, GLFW_STICKY_KEYS, GLFW_TRUE);
        if (settings.cursorDisabled) {
            glfwSetInputMode(windowHandle_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }

        if (glfwRawMouseMotionSupported() && settings.rawInput) {
            glfwSetInputMode(windowHandle_, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            rawInputEnabled_ = true;
        }

        // Window related callbacks
        glfwSetFramebufferSizeCallback(windowHandle_, framebufferResizeCallback);
        glfwSetWindowSizeCallback(windowHandle_, resizeCallback);
        glfwSetCursorEnterCallback(windowHandle_, windowEnterCallback);
        glfwSetWindowPosCallback(windowHandle_, windowPositionCallback);
        glfwSetWindowCloseCallback(windowHandle_, windowCloseCallback);
        glfwSetWindowFocusCallback(windowHandle_, focusCallback);

        // Mouse related callbacks
        glfwSetMouseButtonCallback(windowHandle_, mouseClickCallback);
        glfwSetCursorPosCallback(windowHandle_, mousePositionCallback);
        glfwSetScrollCallback(windowHandle_, mouseWheelCallback);

        // Buttons related callbacks
        glfwSetKeyCallback(windowHandle_, keyCallback);
        glfwSetCharCallback(windowHandle_, charCallback);

        glfwSetWindowUserPointer(windowHandle_, this);

        COFFEE_THROW_IF(
            glfwCreateWindowSurface(device_.instance(), windowHandle_, nullptr, &surfaceHandle_) != VK_SUCCESS,
            "Failed to create window surface!");

        const VkExtent2D extent = { static_cast<uint32_t>(framebufferWidth_), static_cast<uint32_t>(framebufferHeight_) };
        swapChain = std::make_unique<SwapChain>(device_, surfaceHandle_, extent, settings.presentMode);
    }

    WindowImpl::~WindowImpl() noexcept
    {
        swapChain->waitIdle();

        swapChain = nullptr;
        vkDestroySurfaceKHR(device_.instance(), surfaceHandle_, nullptr);

        glfwDestroyWindow(windowHandle_);
    }

    const std::vector<Image>& WindowImpl::presentImages() const noexcept
    {
        return swapChain->presentImages();
    }

    uint32_t WindowImpl::currentImageIndex() const noexcept
    {
        return swapChain->currentFrame();
    }

    bool WindowImpl::acquireNextImage()
    {
        return swapChain->acquireNextImage();
    }

    void WindowImpl::sendCommandBuffer(CommandBuffer&& commandBuffer)
    {
        std::vector<CommandBuffer> commandBuffers {};
        commandBuffers.push_back(std::move(commandBuffer));
        swapChain->submitCommandBuffers(std::move(commandBuffers));
    }

    void WindowImpl::sendCommandBuffers(std::vector<CommandBuffer>&& commandBuffers)
    {
        swapChain->submitCommandBuffers(std::move(commandBuffers));
    }

    void WindowImpl::changePresentMode(VkPresentModeKHR newMode)
    {
        while (framebufferWidth_ == 0 || framebufferHeight_ == 0) {
            glfwWaitEvents();
        }

        swapChain->waitIdle();
        swapChain->recreate(framebufferWidth_, framebufferHeight_, newMode);

        presentModeEvent(*this, std::forward<VkPresentModeKHR>(newMode));
    }

    const std::string& WindowImpl::getWindowTitle() const noexcept
    {
        return titleName_;
    }

    void WindowImpl::setWindowTitle(const std::string& newTitle) const noexcept
    {
        if (newTitle.empty()) {
            return;
        }

        glfwSetWindowTitle(windowHandle_, newTitle.data());
        titleName_ = newTitle;
    }

    bool WindowImpl::isFocused() const noexcept
    {
        return windowFocused_;
    }

    void WindowImpl::focusWindow() const noexcept
    {
        glfwFocusWindow(windowHandle_);
    }

    bool WindowImpl::isIconified() const noexcept
    {
        return windowIconified_;
    }

    void WindowImpl::hideWindow() const noexcept
    {
        glfwHideWindow(windowHandle_);
    }

    void WindowImpl::showWindow() const noexcept
    {
        glfwShowWindow(windowHandle_);
    }

    bool WindowImpl::isBorderless() const noexcept
    {
        return !static_cast<bool>(glfwGetWindowAttrib(windowHandle_, GLFW_DECORATED));
    }

    void WindowImpl::makeBorderless() const noexcept
    {
        glfwSetWindowAttrib(windowHandle_, GLFW_DECORATED, GLFW_FALSE);
    }

    void WindowImpl::revertBorderless() const noexcept
    {
        glfwSetWindowAttrib(windowHandle_, GLFW_DECORATED, GLFW_TRUE);
    }

    bool WindowImpl::isPassthrough() const noexcept
    {
        return static_cast<bool>(glfwGetWindowAttrib(windowHandle_, GLFW_MOUSE_PASSTHROUGH));
    }

    void WindowImpl::enablePassthrough() const noexcept
    {
        glfwSetWindowAttrib(windowHandle_, GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);
    }

    void WindowImpl::disablePassthrough() const noexcept
    {
        glfwSetWindowAttrib(windowHandle_, GLFW_MOUSE_PASSTHROUGH, GLFW_FALSE);
    }

    bool WindowImpl::isTextMode() const noexcept
    {
        return textModeEnabled_;
    }

    void WindowImpl::enableTextMode() const noexcept
    {
        textModeEnabled_ = true;
    }

    void WindowImpl::disableTextMode() const noexcept
    {
        textModeEnabled_ = false;
    }

    CursorState WindowImpl::cursorState() const noexcept
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

    void WindowImpl::showCursor() const noexcept
    {
        glfwSetInputMode(windowHandle_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    void WindowImpl::hideCursor() const noexcept
    {
        glfwSetInputMode(windowHandle_, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    }

    void WindowImpl::disableCursor() const noexcept
    {
        glfwSetInputMode(windowHandle_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    void WindowImpl::setCursor(const Cursor& cursor) const noexcept
    {
        if (cursor != nullptr) {
            glfwSetCursor(windowHandle_, reinterpret_cast<GLFWcursor*>(cursor->nativeHandle_));
        }
    }

    void WindowImpl::resetCursor() const noexcept
    {
        glfwSetCursor(windowHandle_, nullptr);
    }

    Float2D WindowImpl::mousePosition() const noexcept
    {
        return { static_cast<float>(xMousePosition_), static_cast<float>(yMousePosition_) };
    }

    VkOffset2D WindowImpl::windowPosition() const noexcept
    {
        return { xWindowPosition_, yWindowPosition_ };
    }

    VkExtent2D WindowImpl::windowSize() const noexcept
    {
        return { windowWidth_, windowHeight_ };
    }

    VkExtent2D WindowImpl::framebufferSize() const noexcept
    {
        return { framebufferWidth_, framebufferHeight_ };
    }

    void WindowImpl::setMousePosition(const Float2D& position) const noexcept
    {
        glfwSetCursorPos(windowHandle_, static_cast<double>(position.x), static_cast<double>(position.y));
    }

    void WindowImpl::setWindowPosition(const VkOffset2D& position) const noexcept
    {
        glfwSetWindowPos(windowHandle_, position.x, position.y);
    }

    void WindowImpl::setWindowSize(const VkExtent2D& size) const noexcept
    {
        glfwSetWindowSize(windowHandle_, size.width, size.height);
        windowWidth_ = framebufferWidth_ = size.width;
        windowWidth_ = framebufferHeight_ = size.height;
    }

    bool WindowImpl::isButtonPressed(Keys key) const noexcept
    {
        return glfwGetKey(windowHandle_, *key) == GLFW_PRESS;
    }

    bool WindowImpl::isButtonPressed(MouseButton mouseButton) const noexcept
    {
        return glfwGetMouseButton(windowHandle_, *mouseButton) == GLFW_PRESS;
    }

    bool WindowImpl::shouldClose() const noexcept
    {
        return static_cast<bool>(glfwWindowShouldClose(windowHandle_));
    }

    void WindowImpl::framebufferResizeCallback(GLFWwindow* window, int width, int height)
    {
        WindowImpl* windowPtr = static_cast<WindowImpl*>(glfwGetWindowUserPointer(window));

        // Discard all callbacks when window is minimized
        if (width == 0 || height == 0) {
            windowPtr->windowIconified_ = true;
            return;
        }

        // !!! Always recreate swap chain, even if previous size is same as new one, otherwise it
        // will freeze image
        windowPtr->framebufferWidth_ = static_cast<uint32_t>(width);
        windowPtr->framebufferHeight_ = static_cast<uint32_t>(height);
        windowPtr->windowIconified_ = false;

        std::unique_ptr<SwapChain>& swapChain = windowPtr->swapChain;
        swapChain->waitIdle();
        swapChain->recreate(windowPtr->framebufferWidth_, windowPtr->framebufferHeight_, swapChain->presentMode());

        const ResizeEvent resizeEvent { windowPtr->framebufferWidth_, windowPtr->framebufferHeight_ };
        windowPtr->windowResizeEvent(*windowPtr, resizeEvent);
    }

    void WindowImpl::resizeCallback(GLFWwindow* window, int width, int height)
    {
        // This callback most likely will be called with framebufferResizeCallback, so we don't do
        // any callbacks or actions here
        WindowImpl* windowPtr = static_cast<WindowImpl*>(glfwGetWindowUserPointer(window));
        windowPtr->windowWidth_ = static_cast<uint32_t>(width);
        windowPtr->windowHeight_ = static_cast<uint32_t>(height);
    }

    void WindowImpl::windowEnterCallback(GLFWwindow* window, int entered)
    {
        WindowImpl* windowPtr = static_cast<WindowImpl*>(glfwGetWindowUserPointer(window));

        const WindowEnterEvent enterEvent { static_cast<bool>(entered) };
        windowPtr->windowEnterEvent(*windowPtr, enterEvent);
    }

    void WindowImpl::windowPositionCallback(GLFWwindow* window, int xpos, int ypos)
    {
        WindowImpl* windowPtr = static_cast<WindowImpl*>(glfwGetWindowUserPointer(window));

        windowPtr->xWindowPosition_ = xpos;
        windowPtr->yWindowPosition_ = ypos;

        const WindowPositionEvent positionEvent { xpos, ypos };
        windowPtr->windowPositionEvent(*windowPtr, positionEvent);
    }

    void WindowImpl::windowCloseCallback(GLFWwindow* window)
    {
        WindowImpl* windowPtr = static_cast<WindowImpl*>(glfwGetWindowUserPointer(window));

        windowPtr->windowCloseEvent(*windowPtr);
    }

    void WindowImpl::focusCallback(GLFWwindow* window, int focused)
    {
        WindowImpl* windowPtr = static_cast<WindowImpl*>(glfwGetWindowUserPointer(window));
        windowPtr->windowFocused_ = static_cast<bool>(focused);

        const WindowFocusEvent focusEvent { static_cast<bool>(focused) };
        windowPtr->windowFocusEvent(*windowPtr, focusEvent);
    }

    void WindowImpl::mouseClickCallback(GLFWwindow* window, int button, int action, int mods)
    {
        WindowImpl* windowPtr = static_cast<WindowImpl*>(glfwGetWindowUserPointer(window));

        const MouseClickEvent mouseClickEvent { glfwToCoffeeState(action), static_cast<MouseButton>(button), static_cast<uint32_t>(mods) };
        windowPtr->mouseClickEvent(*windowPtr, mouseClickEvent);
    }

    void WindowImpl::mousePositionCallback(GLFWwindow* window, double xpos, double ypos)
    {
        WindowImpl* windowPtr = static_cast<WindowImpl*>(glfwGetWindowUserPointer(window));

        const MouseMoveEvent moveEvent { static_cast<float>(xpos), static_cast<float>(ypos) };
        windowPtr->mouseMoveEvent(*windowPtr, moveEvent);

        windowPtr->xMousePosition_ = xpos;
        windowPtr->yMousePosition_ = ypos;
    }

    void WindowImpl::mouseWheelCallback(GLFWwindow* window, double xoffset, double yoffset)
    {
        WindowImpl* windowPtr = static_cast<WindowImpl*>(glfwGetWindowUserPointer(window));

        const MouseWheelEvent wheelEvent { static_cast<float>(xoffset), static_cast<float>(yoffset) };
        windowPtr->mouseWheelEvent(*windowPtr, wheelEvent);
    }

    void WindowImpl::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        WindowImpl* windowPtr = static_cast<WindowImpl*>(glfwGetWindowUserPointer(window));

        auto state = glfwToCoffeeState(action);
        const KeyEvent keyEvent { state, static_cast<Keys>(key), static_cast<uint32_t>(scancode), static_cast<uint32_t>(mods) };

        windowPtr->keyEvent(*windowPtr, keyEvent);
    }

    void WindowImpl::charCallback(GLFWwindow* window, unsigned int codepoint)
    {
        WindowImpl* windowPtr = static_cast<WindowImpl*>(glfwGetWindowUserPointer(window));

        if (windowPtr->isTextMode()) {
            windowPtr->charEvent(*windowPtr, static_cast<char32_t>(codepoint));
        }
    }

} // namespace coffee