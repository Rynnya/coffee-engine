#ifndef COFFEE_ABSTRACT_PUSH_CONSTANT
#define COFFEE_ABSTRACT_PUSH_CONSTANT

#include <coffee/utils/non_moveable.hpp>
#include <coffee/types.hpp>

namespace coffee {

    class PushConstants;

    class PushConstantRange : NonMoveable {
    public:
        ~PushConstantRange();

        PushConstantRange(const PushConstantRange&) noexcept = delete;
        PushConstantRange(PushConstantRange&&) noexcept = delete;
        PushConstantRange& operator=(const PushConstantRange&) noexcept = delete;
        PushConstantRange& operator=(PushConstantRange&&) noexcept = delete;

        PushConstantRange& addTypelessObject(size_t size) noexcept;

        template <typename T>
        PushConstantRange& addObject() noexcept {
            return this->addTypelessObject(sizeof(T));
        }

    private:
        PushConstantRange(PushConstants& parent, ShaderStage stages) noexcept;

        PushConstants& parent_;
        ShaderStage stages_;
        size_t size_;

        friend class PushConstants;
    };

    class PushConstants {
    public:

        struct Range {
            ShaderStage stage = ShaderStage::None;
            size_t size = 0ULL;
        };

        PushConstants() noexcept = default;
        ~PushConstants() noexcept = default;

        PushConstants(const PushConstants&) noexcept;
        PushConstants(PushConstants&&) noexcept;
        PushConstants& operator=(const PushConstants&) noexcept;
        PushConstants& operator=(PushConstants&&) noexcept;

        PushConstantRange addRange(ShaderStage stages);

    private:
        std::vector<Range> ranges_ {};

        friend class AbstractPipeline;
        friend class PushConstantRange;
    };

}

#endif