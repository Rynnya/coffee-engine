#ifndef COFFEE_INTERFACES_PROPERTIES
#define COFFEE_INTERFACES_PROPERTIES

#include <type_traits>

namespace coffee {

namespace detail {

    template <typename T, typename = void>
    struct IsGetterConst : std::false_type {};

    template <typename T>
    struct IsGetterConst<T, std::void_t<decltype(std::declval<const T&>().get())>> : std::true_type {};

} // namespace detail

// Property class that allow for using like in C#
// ControlClass must be provided with 'get' (and, optionally, 'set') method in order for this class to be working
// This limitation applied for better runtime performance
// Using std::function or lightweight function would create too much runtime overhead
// BEWARE: Compile time is gonna be really really huge
template <typename T, typename ControlClass>
class PropertyImpl {
private: // Members must be placed here so decltype can capture them in expressions
    static_assert(std::is_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*>, "'get' must return proper type.");
    static_assert(detail::IsGetterConst<ControlClass>::value, "'get' must be declared as const method and return const reference.");

    ControlClass controller_;

public:
    inline explicit PropertyImpl() noexcept(std::is_nothrow_default_constructible_v<ControlClass>) {};

    // For some reason clang goes crazy here, so I will just format it manually
    // clang-format off

        template <typename ...Args, std::enable_if_t<std::is_constructible_v<ControlClass, Args...>, bool> = true>
        inline explicit PropertyImpl(Args&&... args) noexcept(std::is_nothrow_constructible_v<ControlClass, Args...>)
            : controller_ { std::forward<Args>(args)... }
        {}

        inline ~PropertyImpl() noexcept = default;

        inline explicit PropertyImpl(const PropertyImpl<T, ControlClass>& other) noexcept(std::is_nothrow_copy_constructible_v<ControlClass>)
            : controller_ { other.controller_ }
        {
            static_assert(std::is_copy_constructible_v<ControlClass>, "ControlClass must be copy-constructible.");
        }

        inline explicit PropertyImpl(PropertyImpl<T, ControlClass>&& other) noexcept(std::is_nothrow_move_constructible_v<ControlClass>)
            : controller_ { std::move(other.controller_) }
        {
            static_assert(std::is_move_constructible_v<ControlClass>, "ControlClass must be move-constructible.");
        }

        inline PropertyImpl& operator=(const PropertyImpl<T, ControlClass>& other)
            noexcept(std::is_nothrow_copy_assignable_v<ControlClass>)
        {
            static_assert(std::is_copy_assignable_v<ControlClass>, "ControlClass must be copy-assignable.");

            if (this == &other) {
                return *this;
            }

            controller_ = other.controller_;
            return *this;
        }

        inline PropertyImpl& operator=(PropertyImpl<T, ControlClass>&& other)
            noexcept(std::is_nothrow_move_assignable_v<ControlClass>)
        {
            static_assert(std::is_move_assignable_v<ControlClass>, "ControlClass must be move-assignable.");

            if (this == &other) {
                return *this;
            }

            controller_ = std::move(other.controller_);
            return *this;
        }

        inline operator const T&() const noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*>)
        {
            return controller_.get();
        }

        inline const T* operator->() const noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*>)
        {
            return &controller_.get();
        }

        template <typename O>
        inline decltype(auto) operator=(const O& object)
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::set), ControlClass*, T>)
        {
            static_assert(std::is_invocable_r_v<const T&, decltype(&ControlClass::set), ControlClass*, T>, "'set' must return const ref.");
            return controller_.set(object);
        }

        template <typename O>
        inline decltype(auto) operator=(O&& object)
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::set), ControlClass*, T>)
        {
            static_assert(std::is_invocable_r_v<const T&, decltype(&ControlClass::set), ControlClass*, T>, "'set' must return const ref.");
            return controller_.set(std::move(object));
        }

        template <typename O>
        inline decltype(auto) operator==(const O& object)
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*>)
        {
            return controller_.get() == object;
        }

        template <typename O>
        inline decltype(auto) operator!=(const O& object)
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*>)
        {
            return controller_.get() != object;
        }

        template <typename O>
        inline decltype(auto) operator>(const O& object) 
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*>)
        {
            return controller_.get() > object;
        }

        template <typename O>
        inline decltype(auto) operator<(const O& object) 
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*>)
        {
            return controller_.get() < object;
        }

        template <typename O>
        inline decltype(auto) operator>=(const O& object) 
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*>)
        {
            return controller_.get() >= object;
        }

        template <typename O>
        inline decltype(auto) operator<=(const O& object)
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*>)
        {
            return controller_.get() <= object;
        }

        template <typename O>
        inline decltype(auto) operator+(const O& object)
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*>)
        {
            return controller_.get() + object;
        }

        template <typename O>
        inline decltype(auto) operator-(const O& object)
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*>)
        {
            return controller_.get() - object;
        }

        template <typename O>
        inline decltype(auto) operator*(const O& object)
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*>)
        {
            return controller_.get() * object;
        }

        template <typename O>
        inline decltype(auto) operator/(const O& object)
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*>)
        {
            return controller_.get() / object;
        }

        template <typename O>
        inline decltype(auto) operator%(const O& object)
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*>)
        {
            return controller_.get() % object;
        }

        template <typename O>
        inline decltype(auto) operator^(const O& object)
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*>)
        {
            return controller_.get() ^ object;
        }

        template <typename O>
        inline decltype(auto) operator&(const O& object)
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*>)
        {
            return controller_.get() & object;
        }

        template <typename O>
        inline decltype(auto) operator|(const O& object)
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*>)
        {
            return controller_.get() | object;
        }

        template <typename O>
        inline decltype(auto) operator>>(const O& object)
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*>)
        {
            return controller_.get() >> object;
        }

        template <typename O>
        inline decltype(auto) operator<<(const O& object)
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*>)
        {
            return controller_.get() << object;
        }

        template <typename O>
        inline decltype(auto) operator+=(const O& object) noexcept(
            std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*> &&
            std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::set), ControlClass*, T>)
        {
            static_assert(std::is_invocable_r_v<const T&, decltype(&ControlClass::set), ControlClass*, T>, "'set' must return const ref.");
            return controller_.set(controller_.get() + object);
        }

        template <typename O>
        inline decltype(auto) operator-=(const O& object) noexcept(
            std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*> &&
            std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::set), ControlClass*, T>)
        {
            static_assert(std::is_invocable_r_v<const T&, decltype(&ControlClass::set), ControlClass*, T>, "'set' must return const ref.");
            return controller_.set(controller_.get() - object);
        }

        template <typename O>
        inline decltype(auto) operator*=(const O& object)
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*> &&
                     std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::set), ControlClass*, T>)
        {
            static_assert(std::is_invocable_r_v<const T&, decltype(&ControlClass::set), ControlClass*, T>, "'set' must return const ref.");
            return controller_.set(controller_.get() * object);
        }

        template <typename O>
        inline decltype(auto) operator/=(const O& object)
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*> &&
                     std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::set), ControlClass*, T>)
        {
            static_assert(std::is_invocable_r_v<const T&, decltype(&ControlClass::set), ControlClass*, T>, "'set' must return const ref.");
            return controller_.set(controller_.get() / object);
        }

        template <typename O>
        inline decltype(auto) operator%=(const O& object)
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*> &&
                     std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::set), ControlClass*, T>)
        {
            static_assert(std::is_invocable_r_v<const T&, decltype(&ControlClass::set), ControlClass*, T>, "'set' must return const ref.");
            return controller_.set(controller_.get() % object);
        }

        template <typename O>
        inline decltype(auto) operator^=(const O& object)
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*> &&
                     std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::set), ControlClass*, T>)
        {
            static_assert(std::is_invocable_r_v<const T&, decltype(&ControlClass::set), ControlClass*, T>, "'set' must return const ref.");
            return controller_.set(controller_.get() ^ object);
        }

        template <typename O>
        inline decltype(auto) operator&=(const O& object)
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*> &&
                     std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::set), ControlClass*, T>)
        {
            static_assert(std::is_invocable_r_v<const T&, decltype(&ControlClass::set), ControlClass*, T>, "'set' must return const ref.");
            return controller_.set(controller_.get() & object);
        }

        template <typename O>
        inline decltype(auto) operator|=(const O& object)
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*> &&
                     std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::set), ControlClass*, T>)
        {
            static_assert(std::is_invocable_r_v<const T&, decltype(&ControlClass::set), ControlClass*, T>, "'set' must return const ref.");
            return controller_.set(controller_.get() | object);
        }

        template <typename O>
        inline decltype(auto) operator<<=(const O& object)
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*> &&
                     std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::set), ControlClass*, T>)
        {
            static_assert(std::is_invocable_r_v<const T&, decltype(&ControlClass::set), ControlClass*, T>, "'set' must return const ref.");
            return controller_.set(controller_.get() << object);
        }

        template <typename O>
        inline decltype(auto) operator>>=(const O& object)
            noexcept(std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::get), ControlClass*> &&
                     std::is_nothrow_invocable_r_v<const T&, decltype(&ControlClass::set), ControlClass*, T>)
        {
            static_assert(std::is_invocable_r_v<const T&, decltype(&ControlClass::set), ControlClass*, T>, "'set' must return const ref.");
            return controller_.set(controller_.get() >> object);
        }

    // clang-format on
};

// Simple class example for Property
template <typename T>
class BasicControl {
private:
    T object_ {};

public:
    BasicControl() noexcept = default;

    const T& get() noexcept { return object_; }

    const T& set(const T& object) noexcept(std::is_nothrow_copy_assignable_v<T>) { return (object_ = object); }
};

template <typename T>
// Look at PropertyImpl for implementation details
using Property = PropertyImpl<T, BasicControl<T>>;

} // namespace coffee

#endif
