#include <coffee/interfaces/deferred_requests.hpp>

namespace coffee {

    void DeferredRequests::addRequest(const std::function<void()>& func) {
        std::scoped_lock<std::mutex> lock { mtx_ };

        requests_.push_back(func);
    }

    void DeferredRequests::addRequest(std::function<void()>&& func) {
        std::scoped_lock<std::mutex> lock { mtx_ };

        requests_.push_back(std::move(func));
    }

    void DeferredRequests::applyRequests() {
        std::scoped_lock<std::mutex> lock { mtx_ };

        for (auto& request : requests_) {
            request();
        }

        requests_.clear();
    }

    void DeferredRequests::clearRequests() {
        std::scoped_lock<std::mutex> lock { mtx_ };

        requests_.clear();
    }

    size_t DeferredRequests::amountOfRequests() const noexcept {
        std::scoped_lock<std::mutex> lock { mtx_ };

        return requests_.size();
    }

}