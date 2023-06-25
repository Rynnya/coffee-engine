#ifndef COFFEE_INTERFACES_RESOURCE_GUARD
#define COFFEE_INTERFACES_RESOURCE_GUARD

#include <coffee/utils/non_copyable.hpp>

#include <new>
#include <type_traits>
#include <utility>

namespace coffee {

    // Type-erasing function wrapper which have support for move-only lambda
    // Sadly, it's allocates memory for each instance, but it doesn't care about function types
    class ScopeExit : NonCopyable {
    public:
        ScopeExit() noexcept = default;

        template <typename Fx, std::enable_if_t<std::is_invocable_v<Fx>, bool> = true>
        ScopeExit(Fx&& function)
        {
            auto* lambda = new auto([func = std::move(function)]() mutable { func(); });

            action_ = lambda;
            caller_ = [](void* buffer) { (*reinterpret_cast<decltype(lambda)>(buffer))(); };
            destructor_ = [](void* buffer) noexcept { delete reinterpret_cast<decltype(lambda)>(buffer); };
        }

        ~ScopeExit() noexcept { consume(std::nothrow); }

        ScopeExit(ScopeExit&& other) noexcept
        {
            consume(std::nothrow);

            action_ = std::exchange(other.action_, nullptr);
            caller_ = std::exchange(other.caller_, nullptr);
            destructor_ = std::exchange(other.destructor_, nullptr);
        }

        ScopeExit& operator=(ScopeExit&& other) noexcept
        {
            if (this == &other) {
                return *this;
            }

            consume(std::nothrow);

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

        void consume(std::nothrow_t) noexcept
        {
            try {
                consume();
            }
            catch (...) {
                release();
            }
        }

        void release() noexcept
        {
            if (action_ != nullptr) {
                cleanup();
            }
        }

        bool alive() const noexcept { return action_ != nullptr; }

        static ScopeExit combine(ScopeExit&& first, ScopeExit&& second)
        {
            return ScopeExit([firstScope = std::move(first), secondScope = std::move(second)]() mutable {
                firstScope.consume();
                secondScope.consume();
            });
        }

        static ScopeExit combine(ScopeExit&& first, ScopeExit&& second, ScopeExit&& third)
        {
            return ScopeExit([firstScope = std::move(first), secondScope = std::move(second), thirdScope = std::move(third)]() mutable {
                firstScope.consume();
                secondScope.consume();
                thirdScope.consume();
            });
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

    // Optional-like scope container that allow for safe retreavement of value from GPU
    template <typename T>
    class ResourceGuard : NonCopyable {
    public:
        static_assert(std::is_default_constructible_v<T> && std::is_copy_constructible_v<T>, "T must be default and copy constructible.");

        ResourceGuard(T&& result, ScopeExit&& guard) : result_ { std::move(result) }, guard_ { std::move(guard) } {}

        ResourceGuard(const T& result, ScopeExit&& guard) : result_ { result }, guard_ { std::move(guard) } {}

        ResourceGuard(T result) : result_ { result } {}

        ResourceGuard(ResourceGuard&& other) noexcept : result_ { std::move(other.result_) }, guard_ { std::move(other.guard_) } {}

        ResourceGuard& operator=(ResourceGuard&& other) noexcept
        {
            if (this == &other) {
                return *this;
            }

            result_ = std::exchange(other.result_, {});
            guard_ = std::exchange(other.guard_, {});

            return *this;
        }

        ~ResourceGuard() noexcept { guard_.consume(std::nothrow); }

        // Extract is doing waiting only once, further calls will return value without waiting, not thread-safe
        T extract()
        {
            guard_.consume();
            return result_;
        }

        // Works same as extract, but doesn't wait for ScopeExit to be done
        // This can be sometimes useful (for example, when creating image views while device is copying to this image)
        // WARNING: This might be dangerous, use with care
        T unsafe() { return result_; }

    private:
        T result_;
        ScopeExit guard_ {};
    };

    // Specialization for void without resource
    // Works exactly like ScopeExit
    template <>
    class ResourceGuard<void> {
    public:
        ResourceGuard(ScopeExit guard) : guard_ { std::move(guard) } {}

        ~ResourceGuard() noexcept
        {
            try {
                guard_.consume();
            }
            catch (...) {
                guard_.release();
            }
        }

        void extract() { guard_.consume(); }

        // No-op as this specialization doesn't return any object
        void unsafe() {};

    private:
        ScopeExit guard_;
    };

} // namespace coffee

#endif