#ifndef COFFEE_EVENTS_MAIN_EVENT
#define COFFEE_EVENTS_MAIN_EVENT

#include <cstdint>

namespace coffee {

    // State enum for keys (keyboard or mouse)
    enum class State : uint8_t { Unknown, Press, Repeat, Release };

    State glfwToCoffeeState(int state);

} // namespace coffee

#endif