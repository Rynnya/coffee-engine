#ifndef COFFEE_ABSTRACT_WINDOW
#define COFFEE_ABSTRACT_WINDOW

#include <coffee/abstract/cursor.hpp>
#include <coffee/abstract/swap_chain.hpp>
#include <coffee/events/application_event.hpp>
#include <coffee/events/key_event.hpp>
#include <coffee/events/mouse_event.hpp>
#include <coffee/events/window_event.hpp>
#include <coffee/types.hpp>
#include <coffee/utils/non_moveable.hpp>

#include <any>
#include <functional>
#include <mutex>

namespace coffee {

    class AbstractWindow;
    using Window = std::unique_ptr<AbstractWindow>;

    class AbstractWindow : NonMoveable {
    public:
        AbstractWindow(WindowSettings settings, const std::string& windowName);
        virtual ~AbstractWindow() noexcept;

        const std::vector<Image>& getPresentImages() const noexcept;
        uint32_t getCurrentImageIndex() const noexcept;

        Format getColorFormat() const noexcept;
        Format getDepthFormat() const noexcept;

        // Ideally, this must be only called one time per frame, to keep game loop active
        // But nobody stops you from using it up to presentImagesSize and then sending them into any order
        bool acquireNextImage();

        void sendCommandBuffer(GraphicsCommandBuffer&& commandBuffer);
        void sendCommandBuffers(std::vector<GraphicsCommandBuffer>&& commandBuffers);

        void changePresentMode(PresentMode newMode);

        const std::string& getWindowTitle() const noexcept;
        void setWindowTitle(const std::string& newTitle) const noexcept;

        bool isFocused() const noexcept;
        void focusWindow() const noexcept;

        bool isIconified() const noexcept;
        void hideWindow() const noexcept;
        void showWindow() const noexcept;

        bool isBorderless() const noexcept;
        void makeBorderless() const noexcept;
        void revertBorderless() const noexcept;

        bool isPassthrough() const noexcept;
        void enablePassthrough() const noexcept;
        void disablePassthrough() const noexcept;

        bool isTextMode() const noexcept;
        void enableTextMode() const noexcept;
        void disableTextMode() const noexcept;

        CursorState getCursorState() const noexcept;
        void showCursor() const noexcept;
        void hideCursor() const noexcept;
        void disableCursor() const noexcept;

        void setCursor(const Cursor& cursor) const noexcept;
        void resetCursor() const noexcept;

        Float2D getMousePosition() const noexcept;
        Offset2D getWindowPosition() const noexcept;
        Extent2D getWindowSize() const noexcept;
        Extent2D getFramebufferSize() const noexcept;
        void setMousePosition(const Float2D& position) const noexcept;
        void setWindowPosition(const Offset2D& position) const noexcept;
        void setWindowSize(const Extent2D& size) const noexcept;

        bool isButtonPressed(Keys key) const noexcept;
        bool isButtonPressed(MouseButton mouseButton) const noexcept;

        bool shouldClose() const noexcept;

        void addWindowResizeCallback(
            const std::string& name,
            const std::function<void(const AbstractWindow* const, const ResizeEvent&)>& callback
        );
        void removeWindowResizeCallback(const std::string& name);

        void addWindowEnterCallback(
            const std::string& name,
            const std::function<void(const AbstractWindow* const, const WindowEnterEvent&)>& callback
        );
        void removeWindowEnterCallback(const std::string& name);

        void addWindowPositionCallback(
            const std::string& name,
            const std::function<void(const AbstractWindow* const, const WindowPositionEvent&)>& callback
        );
        void removeWindowPositionCallback(const std::string& name);

        void addWindowCloseCallback(const std::string& name, const std::function<void(const AbstractWindow* const)>& callback);
        void removeWindowCloseCallback(const std::string& name);

        void addWindowFocusCallback(
            const std::string& name,
            const std::function<void(const AbstractWindow* const, const WindowFocusEvent&)>& callback
        );
        void removeWindowFocusCallback(const std::string& name);

        void addMouseClickCallback(
            const std::string& name,
            const std::function<void(const AbstractWindow* const, const MouseClickEvent&)>& callback
        );
        void removeMouseClickCallback(const std::string& name);

        void addMousePositionCallback(
            const std::string& name,
            const std::function<void(const AbstractWindow* const, const MouseMoveEvent&)>& callback
        );
        void removeMousePositionCallback(const std::string& name);

        void addMouseWheelCallback(
            const std::string& name,
            const std::function<void(const AbstractWindow* const, const MouseWheelEvent&)>& callback
        );
        void removeMouseWheelCallback(const std::string& name);

        void addKeyCallback(const std::string& name, const std::function<void(const AbstractWindow* const, const KeyEvent&)>& callback);
        void removeKeyCallback(const std::string& name);

        void addCharCallback(const std::string& name, const std::function<void(const AbstractWindow* const, char32_t)>& callback);
        void removeCharCallback(const std::string& name);

        void addPresentModeCallback(
            const std::string& name,
            const std::function<void(const AbstractWindow* const, const PresentModeEvent)>& callback
        );
        void removePresentModeCallback(const std::string& name);

        mutable std::any userData;

    protected:
        void* windowHandle = nullptr;
        std::unique_ptr<AbstractSwapChain> swapChain = nullptr;

    private:
        struct PImpl;
        PImpl* pImpl_;
    };

} // namespace coffee

#endif