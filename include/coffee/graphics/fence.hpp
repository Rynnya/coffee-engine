#ifndef COFFEE_GRAPHICS_FENCE
#define COFFEE_GRAPHICS_FENCE

#include <coffee/types.hpp>
#include <coffee/utils/non_moveable.hpp>

namespace coffee {

namespace graphics {

    class Fence;
    using FencePtr = std::unique_ptr<Fence>;

    class Fence : NonMoveable {
    public:
        ~Fence() noexcept;

        static FencePtr create(const DevicePtr& device, bool signaled = false);

        VkResult status() noexcept;
        void wait(uint64_t timeout = std::numeric_limits<uint64_t>::max()) noexcept;
        void reset() noexcept;

        inline const VkFence& fence() const noexcept { return fence_; }

    private:
        Fence(const DevicePtr& device, bool signaled);

        DevicePtr device_;

        VkFence fence_ = VK_NULL_HANDLE;
    };

} // namespace graphics

} // namespace coffee

#endif
