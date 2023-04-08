#ifndef COFFEE_GRAPHICS_BUFFER
#define COFFEE_GRAPHICS_BUFFER

#include <coffee/graphics/device.hpp>
#include <coffee/utils/non_copyable.hpp>

#include <vk_mem_alloc.h>

namespace coffee {

    struct BufferConfiguration {
        uint32_t instanceSize = 1U;
        uint32_t instanceCount = 1U;
        VkBufferUsageFlags usageFlags = 0;
        VkMemoryPropertyFlags memoryProperties = 0;
        VmaAllocationCreateFlags allocationFlags = 0;
        float priority = 0.5f;
    };

    class BufferImpl : NonMoveable {
    public:
        BufferImpl(Device& device, const BufferConfiguration& configuration);
        ~BufferImpl() noexcept;

        void map() noexcept;
        void unmap() noexcept;
        void flush(size_t size = VK_WHOLE_SIZE, size_t offset = 0U);
        void invalidate(size_t size, size_t offset = 0U);

        const VkDeviceSize instanceSize = 1U;
        const VkDeviceSize instanceCount = 1U;
        const VkBufferUsageFlags usageFlags = 0;
        const VkMemoryPropertyFlags memoryProperties = 0;

        inline const VkBuffer& buffer() const noexcept
        {
            return buffer_;
        }

        inline void* memory() const noexcept
        {
            return mappedMemory_;
        }

        inline bool isHostVisible() const noexcept
        {
            return isHostVisible_;
        }

        inline bool isHostCoherent() const noexcept
        {
            return isHostCoherent_;
        }

    private:
        Device& device_;

        VmaAllocation allocation_ = VK_NULL_HANDLE;
        VkBuffer buffer_ = VK_NULL_HANDLE;
        void* mappedMemory_ = nullptr;

        bool isHostVisible_ = false;
        bool isHostCoherent_ = false;

        std::mutex allocationMutex_ {};
    };

    using Buffer = std::unique_ptr<BufferImpl>;

} // namespace coffee

#endif