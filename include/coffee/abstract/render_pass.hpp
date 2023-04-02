#ifndef COFFEE_ABSTRACT_RENDER_PASS
#define COFFEE_ABSTRACT_RENDER_PASS

#include <coffee/utils/non_moveable.hpp>

#include <memory>

namespace coffee {

    class AbstractRenderPass : NonMoveable {
    public:
        AbstractRenderPass() noexcept = default;
        virtual ~AbstractRenderPass() noexcept = default;
    };

    using RenderPass = std::unique_ptr<AbstractRenderPass>;

} // namespace coffee

#endif