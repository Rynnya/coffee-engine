#ifndef COFFEE_ABSTRACT_PIPELINE
#define COFFEE_ABSTRACT_PIPELINE

#include <coffee/utils/non_moveable.hpp>

#include <memory>

namespace coffee {

    class AbstractPipeline : NonMoveable {
    public:
        AbstractPipeline() noexcept = default;
        virtual ~AbstractPipeline() noexcept = default;
    };

    using Pipeline = std::unique_ptr<AbstractPipeline>;

} // namespace coffee

#endif