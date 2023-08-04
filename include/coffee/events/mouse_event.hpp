#ifndef COFFEE_EVENTS_MOUSE_EVENT
#define COFFEE_EVENTS_MOUSE_EVENT

#include <coffee/events/event.hpp>
#include <coffee/interfaces/keys.hpp>

namespace coffee {

class MouseMoveEvent {
public:
    MouseMoveEvent(float x, float y) noexcept;
    ~MouseMoveEvent() noexcept = default;

    const float x;
    const float y;
};

class MouseWheelEvent {
public:
    MouseWheelEvent(float x, float y) noexcept;
    ~MouseWheelEvent() noexcept = default;

    const float x;
    const float y;
};

class MouseClickEvent {
public:
    MouseClickEvent(State state, MouseButton button, uint32_t mods) noexcept;
    ~MouseClickEvent() noexcept = default;

    const State state;
    const MouseButton button;

    const bool control;
    const bool shift;
    const bool alt;
    const bool super;
    const bool capsLock;
    const bool numLock;
};

} // namespace coffee

#endif