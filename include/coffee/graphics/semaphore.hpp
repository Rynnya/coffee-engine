#ifndef COFFEE_GRAPHICS_SEMAPHORE
#define COFFEE_GRAPHICS_SEMAPHORE

#include <coffee/types.hpp>
#include <coffee/utils/non_moveable.hpp>

namespace coffee {

    namespace graphics {

        class Semaphore;
        using SemaphorePtr = std::shared_ptr<Semaphore>;

        class Semaphore : NonMoveable {
        public:
            ~Semaphore() noexcept;

            static SemaphorePtr create(const DevicePtr& device);

            inline const VkSemaphore& semaphore() const noexcept { return semaphore_; }

        private:
            Semaphore(const DevicePtr& device);

            DevicePtr device_;

            VkSemaphore semaphore_ = VK_NULL_HANDLE;
        };

    } // namespace graphics

} // namespace coffee

#endif
