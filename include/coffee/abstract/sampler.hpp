#ifndef COFFEE_ABSTRACT_SAMPLER
#define COFFEE_ABSTRACT_SAMPLER

#include <coffee/utils/non_moveable.hpp>

#include <memory>

namespace coffee {

    class AbstractSampler : NonMoveable {
    public:
        AbstractSampler() noexcept = default;
        virtual ~AbstractSampler() noexcept = default;
    };

    using Sampler = std::shared_ptr<AbstractSampler>;

} // namespace coffee

#endif