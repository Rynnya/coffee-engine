#ifndef COFFEE_INTERFACES_SCOPE_EXIT
#define COFFEE_INTERFACES_SCOPE_EXIT

#include <type_traits>
#include <utility>

namespace coffee {

    // Type-erasing function wrapper which have support for move-only lambda
    class ScopeExit {
    public:
        ScopeExit() noexcept = default;

        template <typename Fx, std::enable_if_t<std::is_invocable_v<Fx>, bool> = true>
        ScopeExit(Fx&& function)
        {
            action_ = new auto([func = std::move(function)]() { func(); });
            caller_ = [](void* buffer) { (*reinterpret_cast<Fx*>(buffer))(); };
            destructor_ = [](void* buffer) noexcept { delete reinterpret_cast<Fx*>(buffer); };
        }

        ~ScopeExit() noexcept
        {
            try {
                consume();
            }
            catch (...) {
                release();
            }
        }

        ScopeExit(ScopeExit&& other)
        {
            consume();

            action_ = std::exchange(other.action_, nullptr);
            caller_ = std::exchange(other.caller_, nullptr);
            destructor_ = std::exchange(other.destructor_, nullptr);
        }

        ScopeExit& operator=(ScopeExit&& other)
        {
            if (this == &other) {
                return *this;
            }

            consume();

            action_ = std::exchange(other.action_, nullptr);
            caller_ = std::exchange(other.caller_, nullptr);
            destructor_ = std::exchange(other.destructor_, nullptr);

            return *this;
        }

        void consume()
        {
            if (action_ != nullptr) {
                caller_(action_);

                cleanup();
            }
        }

        void release() noexcept
        {
            if (action_ != nullptr) {
                cleanup();
            }
        }

    private:
        void cleanup() noexcept
        {
            destructor_(action_);

            action_ = nullptr;
            caller_ = nullptr;
            destructor_ = nullptr;
        }

        void* action_ = nullptr;
        std::add_pointer_t<void(void*)> caller_ = nullptr;
        std::add_pointer_t<void(void*)> destructor_ = nullptr;
    };

} // namespace coffee

#endif