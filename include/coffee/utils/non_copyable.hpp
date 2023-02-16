#ifndef COFFEE_UTILS_NON_COPYABLE
#define COFFEE_UTILS_NON_COPYABLE

namespace coffee {

    // Defines that this class cannot be copied but can be moved
    class NonCopyable {
    protected:
        NonCopyable() noexcept = default;
        ~NonCopyable() noexcept = default;

        NonCopyable(const NonCopyable&) noexcept = delete;
        NonCopyable& operator=(const NonCopyable&) noexcept = delete;

        NonCopyable(NonCopyable&&) noexcept = default;
        NonCopyable& operator=(NonCopyable&&) noexcept = default;
    };

}

#endif