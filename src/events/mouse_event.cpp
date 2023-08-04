#include <coffee/events/mouse_event.hpp>

#include <GLFW/glfw3.h>

namespace coffee {

MouseMoveEvent::MouseMoveEvent(float x, float y) noexcept : x { x }, y { y } {}

MouseWheelEvent::MouseWheelEvent(float x, float y) noexcept : x { x }, y { y } {}

MouseClickEvent::MouseClickEvent(State state, MouseButton button, uint32_t mods) noexcept
    : state { state }
    , button { button }
    , control { static_cast<bool>(mods & GLFW_MOD_CONTROL) }
    , shift { static_cast<bool>(mods & GLFW_MOD_SHIFT) }
    , alt { static_cast<bool>(mods & GLFW_MOD_ALT) }
    , super { static_cast<bool>(mods & GLFW_MOD_SUPER) }
    , capsLock { static_cast<bool>(mods & GLFW_MOD_CAPS_LOCK) }
    , numLock { static_cast<bool>(mods & GLFW_MOD_NUM_LOCK) }
{}

} // namespace coffee