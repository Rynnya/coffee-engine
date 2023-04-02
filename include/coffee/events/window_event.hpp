#ifndef COFFEE_EVENTS_WINDOW_EVENT
#define COFFEE_EVENTS_WINDOW_EVENT

#include <coffee/events/event.hpp>

namespace coffee {

    class ResizeEvent {
    public:
        ResizeEvent(uint32_t width, uint32_t height) noexcept;
        ~ResizeEvent() noexcept = default;

        uint32_t getWidth() const noexcept;
        uint32_t getHeight() const noexcept;

    private:
        const uint32_t width_;
        const uint32_t height_;
    };

    class WindowEnterEvent {
    public:
        WindowEnterEvent(bool entered) noexcept;
        ~WindowEnterEvent() noexcept = default;

        bool entered() const noexcept;
        bool exited() const noexcept;

    private:
        const bool entered_;
    };

    class WindowPositionEvent {
    public:
        WindowPositionEvent(int32_t xpos, int32_t ypos) noexcept;

        int32_t getXPosition() const noexcept;
        int32_t getYPosition() const noexcept;

    private:
        const int32_t xPosition_;
        const int32_t yPosition_;
    };

    class WindowFocusEvent {
    public:
        WindowFocusEvent(bool lost) noexcept;
        ~WindowFocusEvent() noexcept = default;

        bool isLostFocus() const noexcept;
        bool isFocusGained() const noexcept;

    private:
        const bool lost_;
    };

} // namespace coffee

#endif