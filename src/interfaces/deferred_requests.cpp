#include <coffee/interfaces/deferred_requests.hpp>

namespace coffee {

void DeferredRequests::addRequest(const std::function<void()>& func)
{
    tbb::queuing_mutex::scoped_lock lock { mtx_ };

    requests_.push_back(func);
}

void DeferredRequests::addRequest(std::function<void()>&& func)
{
    tbb::queuing_mutex::scoped_lock lock { mtx_ };

    requests_.push_back(std::move(func));
}

void DeferredRequests::applyRequests()
{
    tbb::queuing_mutex::scoped_lock lock { mtx_ };

    for (auto& request : requests_) {
        request();
    }

    requests_.clear();
}

void DeferredRequests::clearRequests()
{
    tbb::queuing_mutex::scoped_lock lock { mtx_ };

    requests_.clear();
}

size_t DeferredRequests::amountOfRequests() const noexcept
{
    tbb::queuing_mutex::scoped_lock lock { mtx_ };

    return requests_.size();
}

} // namespace coffee