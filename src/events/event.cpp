#include <coffee/events/event.hpp>

#include <GLFW/glfw3.h>

#include <atomic>

namespace coffee {

    State glfwToCoffeeState(int state) {
        switch (state) {
            case GLFW_PRESS:
                return State::Press;
            case GLFW_RELEASE:
                return State::Release;
            case GLFW_REPEAT:
                return State::Repeat;
            default:
                return State::Unknown;
        }
    }

} // namespace coffee