#include <coffee/abstract/backend.hpp>

namespace coffee {

    AbstractBackend::AbstractBackend(BackendAPI backend) noexcept : backend_ { backend } {}

    BackendAPI AbstractBackend::getBackendType() const noexcept {
        return backend_;
    }

} // namespace coffee