#ifndef COFFEE_EVENTS_WINDOW_EVENT
#define COFFEE_EVENTS_WINDOW_EVENT

#include <coffee/events/event.hpp>

namespace coffee {

    class ResizeEvent {
    public:
        ResizeEvent(uint32_t width, uint32_t height) noexcept;
        ~ResizeEvent() noexcept = default;

        const uint32_t width;
        const uint32_t height;
    };

    class WindowEnterEvent {
    public:
        WindowEnterEvent(bool entered) noexcept;
        ~WindowEnterEvent() noexcept = default;

        const bool entered;
    };

    class WindowPositionEvent {
    public:
        WindowPositionEvent(int32_t xpos, int32_t ypos) noexcept;

        const int32_t x;
        const int32_t y;
    };

    class WindowFocusEvent {
    public:
        WindowFocusEvent(bool lost) noexcept;
        ~WindowFocusEvent() noexcept = default;

        const bool lost;
    };

} // namespace coffee

#endif