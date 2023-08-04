#ifndef COFFEE_UTILS_NON_MOVEABLE
#define COFFEE_UTILS_NON_MOVEABLE

namespace coffee {

// Defines that this class cannot be copied nor moved
class NonMoveable {
protected:
    NonMoveable() noexcept = default;
    ~NonMoveable() noexcept = default;

    NonMoveable(const NonMoveable&) noexcept = delete;
    NonMoveable& operator=(const NonMoveable&) noexcept = delete;

    NonMoveable(NonMoveable&&) noexcept = delete;
    NonMoveable& operator=(NonMoveable&&) noexcept = delete;
};

} // namespace coffee

#endif