#include <coffee/events/key_event.hpp>

#include <GLFW/glfw3.h>

namespace coffee {

    KeyEvent::KeyEvent(State state, Keys key, uint32_t scancode, uint32_t mods) noexcept
        : state { state }
        , key { key }
        , scancode { scancode }
        , control { static_cast<bool>(mods & GLFW_MOD_CONTROL) }
        , shift { static_cast<bool>(mods & GLFW_MOD_SHIFT) }
        , alt { static_cast<bool>(mods & GLFW_MOD_ALT) }
        , super { static_cast<bool>(mods & GLFW_MOD_SUPER) }
        , capsLock { static_cast<bool>(mods & GLFW_MOD_CAPS_LOCK) }
        , numLock { static_cast<bool>(mods & GLFW_MOD_NUM_LOCK) }
    {}

} // namespace coffee