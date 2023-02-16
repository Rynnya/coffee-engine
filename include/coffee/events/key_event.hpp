#ifndef COFFEE_EVENTS_KEY_EVENT
#define COFFEE_EVENTS_KEY_EVENT

#include <coffee/abstract/keys.hpp>
#include <coffee/events/event.hpp>

namespace coffee {

    class KeyEvent {
    public:
        KeyEvent(State state, Keys key, uint32_t scancode, uint32_t mods) noexcept;

        State getState() const noexcept;
        Keys getKey() const noexcept;
        uint32_t getScancode() const noexcept;

        bool withControl() const noexcept;
        bool withShift() const noexcept;
        bool withAlt() const noexcept;
        bool withSuper() const noexcept;
        bool withCapsLock() const noexcept;
        bool withNumLock() const noexcept;

    private:
        const State state_;
        const Keys key_;
        const uint32_t scancode_;

        const bool control_;
        const bool shift_;
        const bool alt_;
        const bool super_;
        const bool capsLock_;
        const bool numLock_;
    };

}

#endif