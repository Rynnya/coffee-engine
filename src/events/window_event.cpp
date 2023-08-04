#include <coffee/events/window_event.hpp>

namespace coffee {

ResizeEvent::ResizeEvent(uint32_t width, uint32_t height) noexcept : width { width }, height { height } {}

WindowEnterEvent::WindowEnterEvent(bool entered) noexcept : entered { entered } {}

WindowPositionEvent::WindowPositionEvent(int32_t xpos, int32_t ypos) noexcept : x { xpos }, y { ypos } {}

WindowFocusEvent::WindowFocusEvent(bool lost) noexcept : lost { lost } {}

} // namespace coffee