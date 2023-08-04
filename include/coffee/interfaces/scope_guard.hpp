#ifndef COFFEE_INTERFACES_SCOPE_GUARD
#define COFFEE_INTERFACES_SCOPE_GUARD

#include <coffee/utils/non_moveable.hpp>

namespace coffee {

// Minimalistic implementation of scope guard, without memory allocations
template <typename Fx>
class ScopeGuard : NonMoveable {
public:
    ScopeGuard(Fx&& destructor) : destructor_ { std::move(destructor) } {}

    ~ScopeGuard() noexcept
    {
        if (active_) {
            try {
                destructor_();
            }
            catch (...) {
                // Destructors must not throw exceptions
            }
        }
    }

    void release() noexcept { active_ = false; }

private:
    Fx destructor_ {};
    bool active_ = true;
};

} // namespace coffee

#endif