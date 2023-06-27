#ifndef COFFEE_GRAPHICS_BUFFER
#define COFFEE_GRAPHICS_BUFFER

#include <coffee/graphics/device.hpp>
#include <coffee/utils/non_copyable.hpp>

namespace coffee {

    namespace graphics {

        struct BufferConfiguration {
            uint32_t instanceSize = 1U;
            uint32_t instanceCount = 1U;
            VkBufferUsageFlags usageFlags = 0;
            VkMemoryPropertyFlags memoryProperties = 0;
            VmaAllocationCreateFlags allocationFlags = 0;
            VmaMemoryUsage allocationUsage = VMA_MEMORY_USAGE_AUTO;
            float priority = 0.5f;
        };

        class Buffer;
        using BufferPtr = std::unique_ptr<Buffer>;

        class Buffer : NonMoveable {
        public:
            ~Buffer() noexcept;

            static BufferPtr create(const DevicePtr& device, const BufferConfiguration& configuration);

            template <typename T>
            T& map()
            {
                return *reinterpret_cast<T*>(map());
            }

            void* map();
            void unmap() noexcept;
            void flush(size_t size = VK_WHOLE_SIZE, size_t offset = 0U);
            void invalidate(size_t size, size_t offset = 0U);

            const VkDeviceSize instanceSize = 1U;
            const VkDeviceSize instanceCount = 1U;
            const VkBufferUsageFlags usageFlags = 0;
            const VkMemoryPropertyFlags memoryProperties = 0;

            inline const VkBuffer& buffer() const noexcept { return buffer_; }

            // WARNING: This pointer point to the beginning of buffer, so you must always apply offset to it
            // Map doesn't have such flaw because it does offset automatically
            inline void* memory() const noexcept { return mappedMemory_; }

            inline bool isHostVisible() const noexcept { return isHostVisible_; }

            inline bool isHostCoherent() const noexcept { return isHostCoherent_; }

        private:
            Buffer(const DevicePtr& device, const BufferConfiguration& configuration);

            DevicePtr device_;

            VmaAllocation allocation_ = VK_NULL_HANDLE;
            VkBuffer buffer_ = VK_NULL_HANDLE;
            void* mappedMemory_ = nullptr;
            std::atomic_uint16_t mapCount_ = 0;

            bool isHostVisible_ = false;
            bool isHostCoherent_ = false;
        };

    } // namespace graphics

} // namespace coffee

#endif