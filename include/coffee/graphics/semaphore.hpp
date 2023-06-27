#ifndef COFFEE_GRAPHICS_SEMAPHORE
#define COFFEE_GRAPHICS_SEMAPHORE

#include <coffee/types.hpp>

namespace coffee {

    namespace graphics {

        class Semaphore;
        using SemaphorePtr = std::shared_ptr<Semaphore>;

        class Semaphore {
        public:
            ~Semaphore() noexcept;

            static SemaphorePtr create(const DevicePtr& device);

            inline VkSemaphore semaphore() const noexcept { return semaphore_; }

        private:
            Semaphore(const DevicePtr& device);

            DevicePtr device_;

            VkSemaphore semaphore_ = VK_NULL_HANDLE;
        };

    } // namespace graphics

} // namespace coffee

#endif
