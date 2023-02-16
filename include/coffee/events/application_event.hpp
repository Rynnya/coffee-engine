#ifndef COFFEE_EVENTS_APPLICATION_EVENT
#define COFFEE_EVENTS_APPLICATION_EVENT

#include <coffee/types.hpp>
#include <coffee/events/event.hpp>

namespace coffee {

    class PresentModeEvent {
    public:
        PresentModeEvent(PresentMode mode) noexcept;
        ~PresentModeEvent() noexcept = default;

        PresentMode getMode() const noexcept;

    private:
        const PresentMode mode_;
    };

}

#endif