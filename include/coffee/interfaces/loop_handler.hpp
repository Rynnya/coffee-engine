#pragma once

#include <chrono>

namespace coffee {

    // Handles everything about basic game loop
    // Can be created multiple times, but properly will work only if one is used at the time
    class LoopHandler {
    public:
        LoopHandler() noexcept = default;
        ~LoopHandler() noexcept = default;

        LoopHandler(const LoopHandler&) noexcept = default;
        LoopHandler& operator=(const LoopHandler&) noexcept = default;
        LoopHandler(LoopHandler&&) noexcept = default;
        LoopHandler& operator=(LoopHandler&&) noexcept = default;

        // Process events that's already in queue and immediately returns
        // Exception might happen inside callbacks, not thread-safe
        void pollEvents();

        // Process events if they already in queue or wait up to 'timeout' and then returns
        // Exception might happen inside callbacks, not thread-safe
        void pollEvents(double timeout);

        // Waits until framelimit (or just skips it if already hit) and simply recalculates delta time
        // Exception-free, not thread-safe
        void waitFramelimit() noexcept;

        float deltaTime() const noexcept;
        float framerateLimit() const noexcept;

        // Not thread-safe
        void setFramerateLimit(float framerateLimit) noexcept;

    private:
        std::chrono::high_resolution_clock::time_point lastPollTime_ {};
        float deltaTime_ = 0.0f;
        float framerateLimit_ = 60.0f;
    };

} // namespace coffee