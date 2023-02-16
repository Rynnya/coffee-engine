#include <coffee/abstract/push_constant.hpp>

#include <coffee/utils/math.hpp>

namespace coffee {

    PushConstantRange::~PushConstantRange() {
        if (size_ > 0) {
            parent_.ranges_.push_back({ stages_, size_ });
        }
    }

    PushConstantRange& PushConstantRange::addTypelessObject(size_t size) noexcept {
        size_ += Math::roundToMultiple(size, 4ULL);
        return *this;
    }

    PushConstantRange::PushConstantRange(PushConstants& parent, ShaderStage stages) noexcept
        : parent_ { parent }
        , stages_ { stages }
        , size_ { 0ULL }
    {}

    PushConstants::PushConstants(const PushConstants& other) noexcept
        : ranges_ { other.ranges_ }
    {}

    PushConstants::PushConstants(PushConstants&& other) noexcept
        : ranges_ { std::exchange(other.ranges_, {}) }
    {}

    PushConstants& PushConstants::operator=(const PushConstants& other) noexcept {
        ranges_ = other.ranges_;
        return *this;
    }

    PushConstants& PushConstants::operator=(PushConstants&& other) noexcept {
        ranges_ = std::exchange(other.ranges_, {});
        return *this;
    }

    PushConstantRange PushConstants::addRange(ShaderStage stages) {
        return PushConstantRange { *this, stages };
    }

}