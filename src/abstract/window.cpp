#include <coffee/abstract/window.hpp>

#include <coffee/utils/log.hpp>
#include <coffee/utils/platform.hpp>

#include <GLFW/glfw3.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <unordered_map>

namespace coffee {

    static std::mutex creationMutex {};

    template <typename T>
    class Callbacks {
    public:
        inline void add(const std::string& name, const std::function<void(const coffee::AbstractWindow* const, T)>& callback) {
            if (callback == nullptr) {
                COFFEE_ERROR("Callback with name '{}' was discarded because it was nullptr", name);
                return;
            }

            std::scoped_lock<std::mutex> lock { mutex };

            if (map.find(name) != map.end()) {
                COFFEE_WARNING("Callback with name '{}' was replaced", name);
            }

            map[name] = callback;
        }

        inline void remove(const std::string& name) {
            std::scoped_lock<std::mutex> lock { mutex };
            map.erase(name);
        }

        inline void iterate(const coffee::AbstractWindow* const window, T object) {
            std::scoped_lock<std::mutex> lock { mutex };

            for (const auto& [name, callback] : map) {
                callback(window, object);
            }
        }

        std::mutex mutex {};
        std::unordered_map<std::string, std::function<void(const coffee::AbstractWindow* const, T)>> map {};
    };

    template <>
    class Callbacks<void> {
    public:
        inline void add(const std::string& name, const std::function<void(const coffee::AbstractWindow* const)>& callback) {
            if (callback == nullptr) {
                COFFEE_ERROR("Callback with name '{}' was discarded because it was nullptr", name);
                return;
            }

            std::scoped_lock<std::mutex> lock { mutex };

            if (map.find(name) != map.end()) {
                COFFEE_WARNING("Callback with name '{}' was replaced", name);
            }

            map[name] = callback;
        }

        inline void remove(const std::string& name) {
            std::scoped_lock<std::mutex> lock { mutex };
            map.erase(name);
        }

        inline void iterate(const coffee::AbstractWindow* const window) {
            std::scoped_lock<std::mutex> lock { mutex };

            for (const auto& [name, callback] : map) {
                callback(window);
            }
        }

        std::mutex mutex {};
        std::unordered_map<std::string, std::function<void(const coffee::AbstractWindow* const)>> map {};
    };

    struct AbstractWindow::PImpl {
        ~PImpl() {
            glfwDestroyWindow(windowHandle);
        }

        coffee::AbstractWindow* parentHandle = nullptr;
        GLFWmonitor* monitorHandle = nullptr;
        GLFWwindow* windowHandle = nullptr;

        uint32_t windowWidth = 0;
        uint32_t windowHeight = 0;
        uint32_t framebufferWidth = 0;
        uint32_t framebufferHeight = 0;

        std::string titleName {};
        bool rawInputEnabled = false;
        bool windowFocused = false;
        bool windowIconified = false;
        bool textModeEnabled = false;

        double xMousePosition = 0.0;
        double yMousePosition = 0.0;
        int32_t xWindowPosition = 0;
        int32_t yWindowPosition = 0;

        // Window related callbacks
        Callbacks<const ResizeEvent&> resizeCallbacks {};
        Callbacks<const WindowEnterEvent&> enterCallbacks {};
        Callbacks<const WindowPositionEvent&> positionCallbacks {};
        Callbacks<void> closeCallbacks {};
        Callbacks<const WindowFocusEvent&> focusCallbacks {};

        // Mouse related callbacks
        Callbacks<const MouseClickEvent&> mouseClickCallbacks {};
        Callbacks<const MouseMoveEvent&> mousePositionCallbacks {};
        Callbacks<const MouseWheelEvent&> mouseWheelCallbacks {};

        // Buttons related callbacks
        Callbacks<const KeyEvent&> keyCallbacks {};
        Callbacks<char32_t> charCallbacks {};

        // Non-GLFW related callbacks
        Callbacks<const PresentModeEvent&> presentModeCallbacks {};

        static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
        static void resizeCallback(GLFWwindow* window, int width, int height);
        static void windowEnterCallback(GLFWwindow* window, int entered);
        static void windowPositionCallback(GLFWwindow* window, int xpos, int ypos);
        static void windowCloseCallback(GLFWwindow* window);
        static void focusCallback(GLFWwindow* window, int focused);

        static void mouseClickCallback(GLFWwindow* window, int button, int action, int mods);
        static void mousePositionCallback(GLFWwindow* window, double xpos, double ypos);
        static void mouseWheelCallback(GLFWwindow* window, double xoffset, double yoffset);

        static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void charCallback(GLFWwindow* window, unsigned int codepoint);
    };

    void AbstractWindow::PImpl::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        AbstractWindow::PImpl* pImpl = static_cast<AbstractWindow::PImpl*>(glfwGetWindowUserPointer(window));

        // Discard all callbacks when window is minimized
        if (width == 0 || height == 0) {
            pImpl->windowIconified = true;
            return;
        }

        // !!! Always recreate swap chain, even if previous size is same as new one, otherwise it
        // will freeze image
        pImpl->framebufferWidth = static_cast<uint32_t>(width);
        pImpl->framebufferHeight = static_cast<uint32_t>(height);
        pImpl->windowIconified = false;

        std::unique_ptr<AbstractSwapChain>& swapChain = pImpl->parentHandle->swapChain;
        swapChain->waitIdle();
        swapChain->recreate(pImpl->framebufferWidth, pImpl->framebufferHeight, swapChain->getPresentMode());

        const ResizeEvent resizeEvent { pImpl->framebufferWidth, pImpl->framebufferHeight };
        pImpl->resizeCallbacks.iterate(pImpl->parentHandle, resizeEvent);
    }

    void AbstractWindow::PImpl::resizeCallback(GLFWwindow* window, int width, int height) {
        // This callback most likely will be called with framebufferResizeCallback, so we don't do
        // any callbacks or actions here
        AbstractWindow::PImpl* pImpl = static_cast<AbstractWindow::PImpl*>(glfwGetWindowUserPointer(window));
        pImpl->windowWidth = static_cast<uint32_t>(width);
        pImpl->windowHeight = static_cast<uint32_t>(height);
    }

    void AbstractWindow::PImpl::windowEnterCallback(GLFWwindow* window, int entered) {
        AbstractWindow::PImpl* pImpl = static_cast<AbstractWindow::PImpl*>(glfwGetWindowUserPointer(window));

        const WindowEnterEvent enterEvent { static_cast<bool>(entered) };
        pImpl->enterCallbacks.iterate(pImpl->parentHandle, enterEvent);
    }

    void AbstractWindow::PImpl::windowPositionCallback(GLFWwindow* window, int xpos, int ypos) {
        AbstractWindow::PImpl* pImpl = static_cast<AbstractWindow::PImpl*>(glfwGetWindowUserPointer(window));

        pImpl->xWindowPosition = xpos;
        pImpl->yWindowPosition = ypos;

        const WindowPositionEvent positionEvent { xpos, ypos };
        pImpl->positionCallbacks.iterate(pImpl->parentHandle, positionEvent);
    }

    void AbstractWindow::PImpl::windowCloseCallback(GLFWwindow* window) {
        AbstractWindow::PImpl* pImpl = static_cast<AbstractWindow::PImpl*>(glfwGetWindowUserPointer(window));

        pImpl->closeCallbacks.iterate(pImpl->parentHandle);
    }

    void AbstractWindow::PImpl::focusCallback(GLFWwindow* window, int focused) {
        AbstractWindow::PImpl* pImpl = static_cast<AbstractWindow::PImpl*>(glfwGetWindowUserPointer(window));
        pImpl->windowFocused = static_cast<bool>(focused);

        const WindowFocusEvent focusEvent { static_cast<bool>(focused) };
        pImpl->focusCallbacks.iterate(pImpl->parentHandle, focusEvent);
    }

    void AbstractWindow::PImpl::mouseClickCallback(GLFWwindow* window, int button, int action, int mods) {
        AbstractWindow::PImpl* pImpl = static_cast<AbstractWindow::PImpl*>(glfwGetWindowUserPointer(window));

        const MouseClickEvent mouseClickEvent { glfwToCoffeeState(action), static_cast<MouseButton>(button), static_cast<uint32_t>(mods) };
        pImpl->mouseClickCallbacks.iterate(pImpl->parentHandle, mouseClickEvent);
    }

    void AbstractWindow::PImpl::mousePositionCallback(GLFWwindow* window, double xpos, double ypos) {
        AbstractWindow::PImpl* pImpl = static_cast<AbstractWindow::PImpl*>(glfwGetWindowUserPointer(window));

        const MouseMoveEvent moveEvent { static_cast<float>(xpos), static_cast<float>(ypos) };
        pImpl->mousePositionCallbacks.iterate(pImpl->parentHandle, moveEvent);

        pImpl->xMousePosition = xpos;
        pImpl->yMousePosition = ypos;
    }

    void AbstractWindow::PImpl::mouseWheelCallback(GLFWwindow* window, double xoffset, double yoffset) {
        AbstractWindow::PImpl* pImpl = static_cast<AbstractWindow::PImpl*>(glfwGetWindowUserPointer(window));

        const MouseWheelEvent wheelEvent { static_cast<float>(xoffset), static_cast<float>(yoffset) };
        pImpl->mouseWheelCallbacks.iterate(pImpl->parentHandle, wheelEvent);
    }

    void AbstractWindow::PImpl::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        AbstractWindow::PImpl* pImpl = static_cast<AbstractWindow::PImpl*>(glfwGetWindowUserPointer(window));

        const KeyEvent keyEvent { glfwToCoffeeState(action),
                                  static_cast<Keys>(key),
                                  static_cast<uint32_t>(scancode),
                                  static_cast<uint32_t>(mods) };
        pImpl->keyCallbacks.iterate(pImpl->parentHandle, keyEvent);
    }

    void AbstractWindow::PImpl::charCallback(GLFWwindow* window, unsigned int codepoint) {
        AbstractWindow::PImpl* pImpl = static_cast<AbstractWindow::PImpl*>(glfwGetWindowUserPointer(window));

        if (pImpl->textModeEnabled) {
            pImpl->charCallbacks.iterate(pImpl->parentHandle, codepoint);
        }
    }

    AbstractWindow::AbstractWindow(WindowSettings settings, const std::string& windowName) : pImpl_ { new AbstractWindow::PImpl() } {
        pImpl_->parentHandle = this;

        std::string safeWindowName { windowName };

        if (safeWindowName.empty()) {
            safeWindowName = "Coffee Window";
        }

        {
            std::scoped_lock<std::mutex> lock { creationMutex };

            glfwDefaultWindowHints();
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

            pImpl_->monitorHandle = glfwGetPrimaryMonitor();
            COFFEE_THROW_IF(pImpl_->monitorHandle == nullptr, "Failed to get primary monitor handle!");

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
                const GLFWvidmode* videoMode = glfwGetVideoMode(pImpl_->monitorHandle);
                COFFEE_THROW_IF(videoMode == nullptr, "Failed to retreave main video mode of primary monitor!");

                settings.extent.width =
                    static_cast<uint32_t>(settings.fullscreen ? videoMode->width : videoMode->width - videoMode->width / 2);
                settings.extent.height =
                    static_cast<uint32_t>(settings.fullscreen ? videoMode->height : videoMode->height - videoMode->height / 2);
            }

            pImpl_->windowHandle = glfwCreateWindow(
                settings.extent.width,
                settings.extent.height,
                safeWindowName.c_str(),
                settings.fullscreen ? pImpl_->monitorHandle : nullptr,
                nullptr
            );
            COFFEE_THROW_IF(pImpl_->windowHandle == nullptr, "Failed to create new GLFW window!");
        }

        glfwSetInputMode(pImpl_->windowHandle, GLFW_STICKY_KEYS, GLFW_TRUE);
        if (settings.cursorDisabled) {
            glfwSetInputMode(pImpl_->windowHandle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }

        if (glfwRawMouseMotionSupported() && settings.rawInput) {
            glfwSetInputMode(pImpl_->windowHandle, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            pImpl_->rawInputEnabled = true;
        }

        pImpl_->titleName = safeWindowName;

        int width {}, height {};
        glfwGetWindowSize(pImpl_->windowHandle, &width, &height);
        pImpl_->windowWidth = static_cast<uint32_t>(width);
        pImpl_->windowHeight = static_cast<uint32_t>(height);

        glfwGetFramebufferSize(pImpl_->windowHandle, &width, &height);
        pImpl_->framebufferWidth = static_cast<uint32_t>(width);
        pImpl_->framebufferHeight = static_cast<uint32_t>(height);

        pImpl_->windowFocused = static_cast<bool>(glfwGetWindowAttrib(pImpl_->windowHandle, GLFW_FOCUSED));
        pImpl_->windowIconified = static_cast<bool>(glfwGetWindowAttrib(pImpl_->windowHandle, GLFW_ICONIFIED));

        // Hack: This required to forbid usage of possible extent of (0, 0)
        glfwSetWindowSizeLimits(pImpl_->windowHandle, 1, 1, GLFW_DONT_CARE, GLFW_DONT_CARE);

        // Window related callbacks
        glfwSetFramebufferSizeCallback(pImpl_->windowHandle, pImpl_->framebufferResizeCallback);
        glfwSetWindowSizeCallback(pImpl_->windowHandle, pImpl_->resizeCallback);
        glfwSetCursorEnterCallback(pImpl_->windowHandle, pImpl_->windowEnterCallback);
        glfwSetWindowPosCallback(pImpl_->windowHandle, pImpl_->windowPositionCallback);
        glfwSetWindowCloseCallback(pImpl_->windowHandle, pImpl_->windowCloseCallback);
        glfwSetWindowFocusCallback(pImpl_->windowHandle, pImpl_->focusCallback);

        // Mouse related callbacks
        glfwSetMouseButtonCallback(pImpl_->windowHandle, pImpl_->mouseClickCallback);
        glfwSetCursorPosCallback(pImpl_->windowHandle, pImpl_->mousePositionCallback);
        glfwSetScrollCallback(pImpl_->windowHandle, pImpl_->mouseWheelCallback);

        // Buttons related callbacks
        glfwSetKeyCallback(pImpl_->windowHandle, pImpl_->keyCallback);
        glfwSetCharCallback(pImpl_->windowHandle, pImpl_->charCallback);

        glfwSetWindowUserPointer(pImpl_->windowHandle, pImpl_);

        this->windowHandle = pImpl_->windowHandle;
    }

    AbstractWindow::~AbstractWindow() noexcept {
        delete pImpl_;
    }

    const std::vector<Image>& AbstractWindow::getPresentImages() const noexcept {
        return swapChain->getPresentImages();
    }

    uint32_t AbstractWindow::getCurrentImageIndex() const noexcept {
        return swapChain->getImageIndex();
    }

    Format AbstractWindow::getColorFormat() const noexcept {
        return swapChain->getColorFormat();
    }

    Format AbstractWindow::getDepthFormat() const noexcept {
        return swapChain->getDepthFormat();
    }

    bool AbstractWindow::acquireNextImage() {
        return swapChain->acquireNextImage();
    }

    void AbstractWindow::sendCommandBuffer(GraphicsCommandBuffer&& commandBuffer) {
        std::vector<GraphicsCommandBuffer> commandBuffers {};
        commandBuffers.push_back(std::move(commandBuffer));
        return swapChain->submitCommandBuffers(std::move(commandBuffers));
    }

    void AbstractWindow::sendCommandBuffers(std::vector<GraphicsCommandBuffer>&& commandBuffers) {
        return swapChain->submitCommandBuffers(std::move(commandBuffers));
    }

    void AbstractWindow::changePresentMode(PresentMode newMode) {
        while (pImpl_->framebufferWidth == 0 || pImpl_->framebufferHeight == 0) {
            glfwWaitEvents();
        }

        swapChain->waitIdle();
        swapChain->recreate(pImpl_->framebufferWidth, pImpl_->framebufferHeight, newMode);

        coffee::PresentModeEvent presentModeEvent { newMode };
        pImpl_->presentModeCallbacks.iterate(this, presentModeEvent);
    }

    const std::string& AbstractWindow::getWindowTitle() const noexcept {
        return pImpl_->titleName;
    }

    void AbstractWindow::setWindowTitle(const std::string& newTitle) const noexcept {
        if (newTitle.empty()) {
            return;
        }

        glfwSetWindowTitle(pImpl_->windowHandle, newTitle.data());
        pImpl_->titleName = newTitle;
    }

    bool AbstractWindow::isFocused() const noexcept {
        return pImpl_->windowFocused;
    }

    void AbstractWindow::focusWindow() const noexcept {
        glfwFocusWindow(pImpl_->windowHandle);
    }

    bool AbstractWindow::isIconified() const noexcept {
        return pImpl_->windowIconified;
    }

    void AbstractWindow::hideWindow() const noexcept {
        glfwHideWindow(pImpl_->windowHandle);
    }

    void AbstractWindow::showWindow() const noexcept {
        glfwShowWindow(pImpl_->windowHandle);
    }

    bool AbstractWindow::isBorderless() const noexcept {
        return !static_cast<bool>(glfwGetWindowAttrib(pImpl_->windowHandle, GLFW_DECORATED));
    }

    void AbstractWindow::makeBorderless() const noexcept {
        glfwSetWindowAttrib(pImpl_->windowHandle, GLFW_DECORATED, GLFW_FALSE);
    }

    void AbstractWindow::revertBorderless() const noexcept {
        glfwSetWindowAttrib(pImpl_->windowHandle, GLFW_DECORATED, GLFW_TRUE);
    }

    bool AbstractWindow::isPassthrough() const noexcept {
        return static_cast<bool>(glfwGetWindowAttrib(pImpl_->windowHandle, GLFW_MOUSE_PASSTHROUGH));
    }

    void AbstractWindow::enablePassthrough() const noexcept {
        glfwSetWindowAttrib(pImpl_->windowHandle, GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);
    }

    void AbstractWindow::disablePassthrough() const noexcept {
        glfwSetWindowAttrib(pImpl_->windowHandle, GLFW_MOUSE_PASSTHROUGH, GLFW_FALSE);
    }

    bool AbstractWindow::isTextMode() const noexcept {
        return pImpl_->textModeEnabled;
    }

    void AbstractWindow::enableTextMode() const noexcept {
        pImpl_->textModeEnabled = true;
    }

    void AbstractWindow::disableTextMode() const noexcept {
        pImpl_->textModeEnabled = false;
    }

    CursorState AbstractWindow::getCursorState() const noexcept {
        switch (glfwGetInputMode(pImpl_->windowHandle, GLFW_CURSOR)) {
            default:
            case GLFW_CURSOR_NORMAL:
                return CursorState::Visible;
            case GLFW_CURSOR_HIDDEN:
                return CursorState::Hidden;
            case GLFW_CURSOR_DISABLED:
                return CursorState::Disabled;
        }
    }

    void AbstractWindow::showCursor() const noexcept {
        glfwSetInputMode(pImpl_->windowHandle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    void AbstractWindow::hideCursor() const noexcept {
        glfwSetInputMode(pImpl_->windowHandle, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    }

    void AbstractWindow::disableCursor() const noexcept {
        glfwSetInputMode(pImpl_->windowHandle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    void AbstractWindow::setCursor(const Cursor& cursor) const noexcept {
        if (cursor != nullptr) {
            glfwSetCursor(pImpl_->windowHandle, reinterpret_cast<GLFWcursor*>(cursor->nativeHandle_));
        }
    }

    void AbstractWindow::resetCursor() const noexcept {
        glfwSetCursor(pImpl_->windowHandle, nullptr);
    }

    Float2D AbstractWindow::getMousePosition() const noexcept {
        return { static_cast<float>(pImpl_->xMousePosition), static_cast<float>(pImpl_->yMousePosition) };
    }

    Offset2D AbstractWindow::getWindowPosition() const noexcept {
        return { pImpl_->xWindowPosition, pImpl_->yWindowPosition };
    }

    Extent2D AbstractWindow::getWindowSize() const noexcept {
        return { pImpl_->windowWidth, pImpl_->windowHeight };
    }

    Extent2D AbstractWindow::getFramebufferSize() const noexcept {
        return { pImpl_->framebufferWidth, pImpl_->framebufferHeight };
    }

    void AbstractWindow::setMousePosition(const Float2D& position) const noexcept {
        glfwSetCursorPos(pImpl_->windowHandle, static_cast<double>(position.x), static_cast<double>(position.y));
    }

    void AbstractWindow::setWindowPosition(const Offset2D& position) const noexcept {
        glfwSetWindowPos(pImpl_->windowHandle, position.x, position.y);
    }

    void AbstractWindow::setWindowSize(const Extent2D& size) const noexcept {
        glfwSetWindowSize(pImpl_->windowHandle, size.width, size.height);
        pImpl_->windowWidth = pImpl_->framebufferWidth = size.width;
        pImpl_->windowWidth = pImpl_->framebufferHeight = size.height;
    }

    bool AbstractWindow::isButtonPressed(Keys key) const noexcept {
        return glfwGetKey(pImpl_->windowHandle, *key) == GLFW_PRESS;
    }

    bool AbstractWindow::isButtonPressed(MouseButton mouseButton) const noexcept {
        return glfwGetMouseButton(pImpl_->windowHandle, *mouseButton) == GLFW_PRESS;
    }

    bool AbstractWindow::shouldClose() const noexcept {
        return static_cast<bool>(glfwWindowShouldClose(pImpl_->windowHandle));
    }

    // Callbacks

    void AbstractWindow::addWindowResizeCallback(
        const std::string& name,
        const std::function<void(const AbstractWindow* const, const ResizeEvent&)>& callback
    ) {
        pImpl_->resizeCallbacks.add(name, callback);
    }

    void AbstractWindow::removeWindowResizeCallback(const std::string& name) {
        pImpl_->resizeCallbacks.remove(name);
    }

    void AbstractWindow::addWindowEnterCallback(
        const std::string& name,
        const std::function<void(const AbstractWindow* const, const WindowEnterEvent&)>& callback
    ) {
        pImpl_->enterCallbacks.add(name, callback);
    }

    void AbstractWindow::removeWindowEnterCallback(const std::string& name) {
        pImpl_->enterCallbacks.remove(name);
    }

    void AbstractWindow::addWindowPositionCallback(
        const std::string& name,
        const std::function<void(const AbstractWindow* const, const WindowPositionEvent&)>& callback
    ) {
        pImpl_->positionCallbacks.add(name, callback);
    }

    void AbstractWindow::removeWindowPositionCallback(const std::string& name) {
        pImpl_->positionCallbacks.remove(name);
    }

    void AbstractWindow::addWindowCloseCallback(const std::string& name, const std::function<void(const AbstractWindow* const)>& callback) {
        pImpl_->closeCallbacks.add(name, callback);
    }

    void AbstractWindow::removeWindowCloseCallback(const std::string& name) {
        pImpl_->closeCallbacks.remove(name);
    }

    void AbstractWindow::addWindowFocusCallback(
        const std::string& name,
        const std::function<void(const AbstractWindow* const, const WindowFocusEvent&)>& callback
    ) {
        pImpl_->focusCallbacks.add(name, callback);
    }

    void AbstractWindow::removeWindowFocusCallback(const std::string& name) {
        pImpl_->focusCallbacks.remove(name);
    }

    void AbstractWindow::addMouseClickCallback(
        const std::string& name,
        const std::function<void(const AbstractWindow* const, const MouseClickEvent&)>& callback
    ) {
        pImpl_->mouseClickCallbacks.add(name, callback);
    }

    void AbstractWindow::removeMouseClickCallback(const std::string& name) {
        pImpl_->mouseClickCallbacks.remove(name);
    }

    void AbstractWindow::addMousePositionCallback(
        const std::string& name,
        const std::function<void(const AbstractWindow* const, const MouseMoveEvent&)>& callback
    ) {
        pImpl_->mousePositionCallbacks.add(name, callback);
    }

    void AbstractWindow::removeMousePositionCallback(const std::string& name) {
        pImpl_->mousePositionCallbacks.remove(name);
    }

    void AbstractWindow::addMouseWheelCallback(
        const std::string& name,
        const std::function<void(const AbstractWindow* const, const MouseWheelEvent&)>& callback
    ) {
        pImpl_->mouseWheelCallbacks.add(name, callback);
    }

    void AbstractWindow::removeMouseWheelCallback(const std::string& name) {
        pImpl_->mouseWheelCallbacks.remove(name);
    }

    void AbstractWindow::addKeyCallback(
        const std::string& name,
        const std::function<void(const AbstractWindow* const, const KeyEvent&)>& callback
    ) {
        pImpl_->keyCallbacks.add(name, callback);
    }

    void AbstractWindow::removeKeyCallback(const std::string& name) {
        pImpl_->keyCallbacks.remove(name);
    }

    void AbstractWindow::addCharCallback(
        const std::string& name,
        const std::function<void(const AbstractWindow* const, char32_t)>& callback
    ) {
        pImpl_->charCallbacks.add(name, callback);
    }

    void AbstractWindow::removeCharCallback(const std::string& name) {
        pImpl_->charCallbacks.remove(name);
    }

    void AbstractWindow::addPresentModeCallback(
        const std::string& name,
        const std::function<void(const AbstractWindow* const, const PresentModeEvent)>& callback
    ) {
        pImpl_->presentModeCallbacks.add(name, callback);
    }

    void AbstractWindow::removePresentModeCallback(const std::string& name) {
        pImpl_->presentModeCallbacks.remove(name);
    }

} // namespace coffee