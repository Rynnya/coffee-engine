#ifndef COFFEE_INTERFACES_SCOPE_EXIT
#define COFFEE_INTERFACES_SCOPE_EXIT

#include <coffee/utils/non_copyable.hpp>

#include <functional>

namespace coffee {

    class ScopeExit : NonCopyable {
    public:
        ScopeExit() noexcept : released_ { true }, action_ { nullptr } {}

        template <typename Fx, std::enable_if_t<std::is_invocable_v<Fx>, bool> = true>
        ScopeExit(Fx&& function) : action_ { [func = std::move(function)]() { func(); } }
        {}

        ~ScopeExit() noexcept
        {
            if (!released_) {
                try {
                    action_();
                }
                catch (...) {
                    // Destructor must be noexcept
                }
            }
        }

        ScopeExit(ScopeExit&& other) noexcept
            : released_ { std::exchange(other.released_, true) }
            , action_ { std::exchange(other.action_, nullptr) }
        {}

        ScopeExit& operator=(ScopeExit&& other) noexcept
        {
            if (this == &other) {
                return *this;
            }

            released_ = std::exchange(other.released_, true);
            action_ = std::exchange(other.action_, nullptr);

            return *this;
        }

        void consume()
        {
            if (!released_) {
                action_();
                action_ = nullptr;
                released_ = true;
            }
        }

        void release() noexcept
        {
            action_ = nullptr;
            released_ = true;
        }

    private:
        bool released_ = false;
        std::function<void()> action_;
    };

} // namespace coffee

#endif