#ifndef COFFEE_EVENTS_APPLICATION_EVENT
#define COFFEE_EVENTS_APPLICATION_EVENT

#include <coffee/events/event.hpp>
#include <coffee/types.hpp>

namespace coffee {

    class PresentModeEvent {
    public:
        PresentModeEvent(PresentMode mode) noexcept;
        ~PresentModeEvent() noexcept = default;

        PresentMode getMode() const noexcept;

    private:
        const PresentMode mode_;
    };

} // namespace coffee

#endif