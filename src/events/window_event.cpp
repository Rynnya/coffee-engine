#include <coffee/events/window_event.hpp>

namespace coffee {

    ResizeEvent::ResizeEvent(uint32_t width, uint32_t height) noexcept : width_ { width }, height_ { height } {}

    uint32_t ResizeEvent::getWidth() const noexcept {
        return width_;
    }

    uint32_t ResizeEvent::getHeight() const noexcept {
        return height_;
    }

    WindowFocusEvent::WindowFocusEvent(bool lost) noexcept : lost_ { lost } {}

    bool WindowFocusEvent::isLostFocus() const noexcept {
        return lost_;
    }

    bool WindowFocusEvent::isFocusGained() const noexcept {
        return !lost_;
    }

    WindowEnterEvent::WindowEnterEvent(bool entered) noexcept : entered_ { entered } {}

    bool WindowEnterEvent::entered() const noexcept {
        return entered_;
    }

    bool WindowEnterEvent::exited() const noexcept {
        return !entered_;
    }

}