#ifndef COFFEE_EVENTS_KEY_EVENT
#define COFFEE_EVENTS_KEY_EVENT

#include <coffee/events/event.hpp>
#include <coffee/graphics/keys.hpp>

namespace coffee {

    class KeyEvent {
    public:
        KeyEvent(State state, Keys key, uint32_t scancode, uint32_t mods) noexcept;

        const State state;
        const Keys key;
        const uint32_t scancode;

        const bool control;
        const bool shift;
        const bool alt;
        const bool super;
        const bool capsLock;
        const bool numLock;
    };

} // namespace coffee

#endif