#ifndef COFFEE_INTERFACES_EVENT_HANDLER
#define COFFEE_INTERFACES_EVENT_HANDLER

#include <functional>
#include <mutex>
#include <utility>
#include <vector>

namespace coffee {

    template <typename... Args>
    class Callback {
    public:
        template <typename T, class Fx>
        void bind_callback(T* obj, Fx&& func)
        {
            bound_ = [obj, function = std::move(func)](Args&&... args) { std::invoke(function, obj, std::forward<Args>(args)...); };
            hash_ = std::hash<T*> {}(obj) ^ bound_.target_type().hash_code();
        }

        template <class Fx>
        void bind_callback(Fx&& func)
        {
            bound_ = [function = std::move(func)](Args&&... args) { std::invoke(function, std::forward<Args>(args)...); };
            hash_ = bound_.target_type().hash_code();
        }

        template <typename T, class Fx>
        Callback(T* obj, Fx&& func)
        {
            bind_callback(obj, std::move(func));
        }

        template <class Fx>
        Callback(Fx&& func)
        {
            bind_callback(std::move(func));
        }

        bool operator==(const Callback<Args...>& cb) const noexcept
        {
            return hash_ == cb.hash_;
        }

        bool operator!=(const Callback<Args...>& cb) const noexcept
        {
            return hash_ != cb.hash_;
        }

        constexpr size_t hash_code() const noexcept
        {
            return hash_;
        }

        void invoke(Args&&... args) const
        {
            bound_(std::forward<Args>(args)...);
        }

    private:
        size_t hash_;
        std::function<void(Args...)> bound_;
    };

    template <typename... Args>
    class Invokable {
    public:
        void operator+=(const Callback<Args...>& cb)
        {
            std::scoped_lock<std::mutex> lock { mtx_ };

            if (std::find(callbacks_.begin(), callbacks_.end(), cb) == callbacks_.end()) {
                callbacks_.push_back(cb);
            }
        }

        void operator-=(const Callback<Args...>& cb)
        {
            std::scoped_lock<std::mutex> lock { mtx_ };

            auto it = callbacks_.begin();

            while (it != callbacks_.end()) {
                if (it->hash_code() == cb.hash_code()) {
                    it = callbacks_.erase(it);
                }
                else {
                    it++;
                }
            }
        }

        void operator()(Args&&... args) const
        {
            std::scoped_lock<std::mutex> lock { mtx_ };

            for (const Callback<Args...>& cb : callbacks_) {
                cb.invoke(std::forward<Args>(args)...);
            }
        }

        void clear()
        {
            std::scoped_lock<std::mutex> lock { mtx_ };

            callbacks_.clear();
        }

    private:
        mutable std::mutex mtx_;
        std::vector<Callback<Args...>> callbacks_;
    };

} // namespace coffee

#endif