#include <coffee/events/application_event.hpp>

namespace coffee {

    PresentModeEvent::PresentModeEvent(PresentMode mode) noexcept : mode_ { mode } {}

    PresentMode coffee::PresentModeEvent::getMode() const noexcept {
        return mode_;
    }

} // namespace coffee