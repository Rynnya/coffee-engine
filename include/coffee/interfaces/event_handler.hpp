#ifndef COFFEE_INTERFACES_EVENT_HANDLER
#define COFFEE_INTERFACES_EVENT_HANDLER

#include <oneapi/tbb/queuing_mutex.h>

#include <functional>
#include <utility>
#include <vector>

namespace coffee {

    template <typename... Args>
    class Callback {
    public:
        template <typename T, typename Fx, std::enable_if_t<std::is_invocable_v<Fx, T, Args...>, bool> = true>
        Callback(T* obj, Fx&& func)
            : bound_ { [obj, function = std::move(func)](Args&&... args) { std::invoke(function, obj, std::forward<Args>(args)...); } }
            , hash_ { std::hash<T*> {}(obj) ^ bound_.target_type().hash_code() }
        {}

        template <typename Fx, std::enable_if_t<std::is_invocable_v<Fx, Args...>, bool> = true>
        Callback(Fx&& func)
            : bound_ { [function = std::move(func)](Args&&... args) { std::invoke(function, std::forward<Args>(args)...); } }
            , hash_ { bound_.target_type().hash_code() }
        {}

        Callback(const Callback<Args...>& other) : bound_ { other.bound_ }, hash_ { other.hash_ } {}

        Callback(Callback<Args...>&& other) : bound_ { std::move(other.bound_) }, hash_ { other.hash_ } {}

        Callback& operator=(const Callback<Args...>& other)
        {
            if (this == &other) {
                return *this;
            }

            bound_ = other.bound_;
            hash_ = other.hash_;

            return *this;
        }

        Callback& operator=(Callback<Args...>&& other)
        {
            if (this == &other) {
                return *this;
            }

            bound_ = std::move(other.bound_);
            hash_ = std::move(other.hash_);

            return *this;
        }

        bool operator==(const Callback<Args...>& cb) const noexcept { return hash_ == cb.hash_; }

        bool operator!=(const Callback<Args...>& cb) const noexcept { return hash_ != cb.hash_; }

        constexpr size_t hash_code() const noexcept { return hash_; }

        void invoke(Args&&... args) const { bound_(std::forward<Args>(args)...); }

    private:
        std::function<void(Args...)> bound_;
        size_t hash_;
    };

    template <typename... Args>
    class Invokable {
    public:
        void operator+=(const Callback<Args...>& cb)
        {
            tbb::queuing_mutex::scoped_lock lock { mtx_ };

            if (std::find(callbacks_.begin(), callbacks_.end(), cb) == callbacks_.end()) {
                callbacks_.push_back(cb);
            }
        }

        void operator+=(Callback<Args...>&& cb)
        {
            tbb::queuing_mutex::scoped_lock lock { mtx_ };

            if (std::find(callbacks_.begin(), callbacks_.end(), cb) == callbacks_.end()) {
                callbacks_.push_back(std::move(cb));
            }
        }

        void operator-=(const Callback<Args...>& cb)
        {
            tbb::queuing_mutex::scoped_lock lock { mtx_ };

            auto it = std::remove_if(callbacks_.begin(), callbacks_.end(), [&cb](auto& callback) { return callback.hash_code() == cb.hash_code(); });
            callbacks_.erase(it, callbacks_.end());
        }

        void operator-=(Callback<Args...>&& cb)
        {
            tbb::queuing_mutex::scoped_lock lock { mtx_ };

            auto it = std::remove_if(callbacks_.begin(), callbacks_.end(), [&cb](auto& callback) { return callback.hash_code() == cb.hash_code(); });
            callbacks_.erase(it, callbacks_.end());
        }

        void operator()(Args&&... args) const
        {
            tbb::queuing_mutex::scoped_lock lock { mtx_ };

            for (const Callback<Args...>& cb : callbacks_) {
                cb.invoke(std::forward<Args>(args)...);
            }
        }

        void clear()
        {
            tbb::queuing_mutex::scoped_lock lock { mtx_ };

            callbacks_.clear();
        }

    private:
        mutable tbb::queuing_mutex mtx_;
        std::vector<Callback<Args...>> callbacks_;
    };

} // namespace coffee

#endif