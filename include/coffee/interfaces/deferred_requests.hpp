#ifndef COFFEE_INTERFACES_DEFERRED_REQUESTS
#define COFFEE_INTERFACES_DEFERRED_REQUESTS

#include <coffee/utils/non_moveable.hpp>

#include <functional>
#include <mutex>
#include <vector>

namespace coffee {

    // Interface that allows derived classes to use deferred requests system
    // Requests will wait until apply is called
    class DeferredRequests : NonMoveable {
    protected:
        DeferredRequests() noexcept = default;
        ~DeferredRequests() noexcept = default;

        void addRequest(const std::function<void()>& func);
        void addRequest(std::function<void()>&& func);

        void applyRequests();
        void clearRequests();

        size_t amountOfRequests() const noexcept;

    private:
        std::vector<std::function<void()>> requests_ {};
        mutable std::mutex mtx_ {};
    };

} // namespace coffee

#endif