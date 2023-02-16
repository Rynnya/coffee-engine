#include <coffee/events/mouse_event.hpp>

#include <GLFW/glfw3.h>

namespace coffee {

    MouseMoveEvent::MouseMoveEvent(float x, float y) noexcept : x_ { x }, y_ { y } {}

    float MouseMoveEvent::getX() const noexcept {
        return x_;
    }

    float MouseMoveEvent::getY() const noexcept {
        return y_;
    }

    MouseWheelEvent::MouseWheelEvent(float x, float y) noexcept : x_ { x }, y_ { y } {}

    float MouseWheelEvent::getX() const noexcept {
        return x_;
    }

    float MouseWheelEvent::getY() const noexcept {
        return y_;
    }

    MouseClickEvent::MouseClickEvent(State state, MouseButton button, uint32_t mods) noexcept
        : state_ { state }
        , button_ { button }
        , control_ { static_cast<bool>(mods & GLFW_MOD_SHIFT) }
        , shift_ { static_cast<bool>(mods & GLFW_MOD_CONTROL) }
        , alt_ { static_cast<bool>(mods & GLFW_MOD_ALT) }
        , super_ { static_cast<bool>(mods & GLFW_MOD_SUPER) }
        , capsLock_ { static_cast<bool>(mods & GLFW_MOD_CAPS_LOCK) }
        , numLock_ { static_cast<bool>(mods & GLFW_MOD_NUM_LOCK) }
    {}

    State MouseClickEvent::getState() const noexcept {
        return state_;
    }

    MouseButton MouseClickEvent::getButton() const noexcept {
        return button_;
    }

    bool MouseClickEvent::withControl() const noexcept {
        return control_;
    }

    bool MouseClickEvent::withShift() const noexcept {
        return shift_;
    }

    bool MouseClickEvent::withAlt() const noexcept {
        return alt_;
    }

    bool MouseClickEvent::withSuper() const noexcept {
        return super_;
    }

    bool MouseClickEvent::withCapsLock() const noexcept {
        return capsLock_;
    }

    bool MouseClickEvent::withNumLock() const noexcept {
        return numLock_;
    }

}