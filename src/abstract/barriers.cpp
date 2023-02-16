#include <coffee/abstract/barriers.hpp>

#include <coffee/utils/log.hpp>
#include <coffee/utils/math.hpp>

namespace coffee {

    ImageBarrier::ImageBarrier(const Image& image, ResourceState oldLayout, ResourceState newLayout) noexcept
        : image { image }
        , oldLayout { oldLayout }
        , newLayout { newLayout }
    {
        COFFEE_ASSERT(Math::hasSingleBit(static_cast<uint32_t>(oldLayout)), "ResourceState 'oldLayout' must have only one bit.");
        COFFEE_ASSERT(Math::hasSingleBit(static_cast<uint32_t>(newLayout)), "ResourceState 'newLayout' must have only one bit.");

        [[ maybe_unused ]] constexpr auto verifyLayout = [](ResourceState layout) noexcept -> bool {
            switch (layout) {
                case ResourceState::VertexBuffer:
                case ResourceState::UniformBuffer:
                case ResourceState::IndexBuffer:
                case ResourceState::IndirectCommand:
                    return false;
                default:
                    return true;
            }

            return true;
        };

        COFFEE_ASSERT(verifyLayout(oldLayout), "Invalid 'oldLayout' state.");
        COFFEE_ASSERT(verifyLayout(newLayout), "Invalid 'newLayout' state.");
    }

}