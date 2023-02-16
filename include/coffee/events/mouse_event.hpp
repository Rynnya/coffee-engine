#ifndef COFFEE_EVENTS_MOUSE_EVENT
#define COFFEE_EVENTS_MOUSE_EVENT

#include <coffee/abstract/keys.hpp>
#include <coffee/events/event.hpp>

namespace coffee {

    class MouseMoveEvent {
    public:
        MouseMoveEvent(float x, float y) noexcept;
        virtual ~MouseMoveEvent() noexcept = default;

        float getX() const noexcept;
        float getY() const noexcept;

    private:
        const float x_;
        const float y_;
    };

    class MouseWheelEvent {
    public:
        MouseWheelEvent(float x, float y) noexcept;
        virtual ~MouseWheelEvent() noexcept = default;

        float getX() const noexcept;
        float getY() const noexcept;

    private:
        const float x_;
        const float y_;
    };

    class MouseClickEvent {
    public:
        MouseClickEvent(State state, MouseButton button, uint32_t mods) noexcept;
        virtual ~MouseClickEvent() noexcept = default;

        State getState() const noexcept;
        MouseButton getButton() const noexcept;

        bool withControl() const noexcept;
        bool withShift() const noexcept;
        bool withAlt() const noexcept;
        bool withSuper() const noexcept;
        bool withCapsLock() const noexcept;
        bool withNumLock() const noexcept;

    private:
        const State state_;
        const MouseButton button_;

        const bool control_;
        const bool shift_;
        const bool alt_;
        const bool super_;
        const bool capsLock_;
        const bool numLock_;
    };

}

#endif