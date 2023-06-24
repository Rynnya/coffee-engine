#include <coffee/interfaces/loop_handler.hpp>

#include <GLFW/glfw3.h>

#include <thread>

#include <coffee/utils/log.hpp>

namespace coffee {

    void LoopHandler::pollEvents() { glfwPollEvents(); }

    void LoopHandler::pollEvents(double timeout) { glfwWaitEventsTimeout(timeout); }

    void LoopHandler::waitFramelimit() noexcept
    {
        float frameTime = std::chrono::duration<float>(std::chrono::high_resolution_clock::now() - lastPollTime_).count();
        float waitForSeconds = std::max(1.0f / framerateLimit_ - frameTime, 0.0f);

        if (waitForSeconds > 0.0f) {
            auto spinStart = std::chrono::high_resolution_clock::now();
            while ((std::chrono::high_resolution_clock::now() - spinStart).count() / 1e9 < waitForSeconds)
                ;
        }

        auto currentTime = std::chrono::high_resolution_clock::now();
        deltaTime_ = std::chrono::duration<float>(currentTime - lastPollTime_).count();
        lastPollTime_ = currentTime;
    }

    float LoopHandler::deltaTime() const noexcept { return deltaTime_; }

    float LoopHandler::framerateLimit() const noexcept { return framerateLimit_; }

    void LoopHandler::setFramerateLimit(float framerateLimit) noexcept { framerateLimit_ = framerateLimit; }

} // namespace coffee