#include <coffee/events/key_event.hpp>

#include <GLFW/glfw3.h>

namespace coffee {

    KeyEvent::KeyEvent(State state, Keys key, uint32_t scancode, uint32_t mods) noexcept
            : state_ { state }
            , key_ { key }
            , scancode_ { scancode }
            , control_ { static_cast<bool>(mods & GLFW_MOD_SHIFT) }
            , shift_ { static_cast<bool>(mods & GLFW_MOD_CONTROL) }
            , alt_ { static_cast<bool>(mods & GLFW_MOD_ALT) }
            , super_ { static_cast<bool>(mods & GLFW_MOD_SUPER) }
            , capsLock_ { static_cast<bool>(mods & GLFW_MOD_CAPS_LOCK) }
            , numLock_ { static_cast<bool>(mods & GLFW_MOD_NUM_LOCK) } {}

    State KeyEvent::getState() const noexcept {
        return state_;
    }

    Keys KeyEvent::getKey() const noexcept {
        return key_;
    }

    uint32_t KeyEvent::getScancode() const noexcept {
        return scancode_;
    }

    bool KeyEvent::withControl() const noexcept {
        return control_;
    }

    bool KeyEvent::withShift() const noexcept {
        return shift_;
    }

    bool KeyEvent::withAlt() const noexcept {
        return alt_;
    }

    bool KeyEvent::withSuper() const noexcept {
        return super_;
    }

    bool KeyEvent::withCapsLock() const noexcept {
        return capsLock_;
    }

    bool KeyEvent::withNumLock() const noexcept {
        return numLock_;
    }

} // namespace coffee