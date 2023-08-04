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
    using BufferPtr = std::shared_ptr<Buffer>;

    class Buffer : NonMoveable {
    public:
        ~Buffer() noexcept;

        static BufferPtr create(const DevicePtr& device, const BufferConfiguration& configuration);

        template <typename T, std::enable_if_t<std::is_pointer_v<T> && !std::is_null_pointer_v<T>, bool> = true>
        inline T map()
        {
            return reinterpret_cast<T>(map());
        }

        template <typename T, std::enable_if_t<!(std::is_pointer_v<T> || std::is_null_pointer_v<T>), bool> = true>
        inline T& map()
        {
            return *reinterpret_cast<T*>(map());
        }

        void* map();
        void unmap() noexcept;
        void flush(size_t size = VK_WHOLE_SIZE, size_t offset = 0U);
        void invalidate(size_t size = VK_WHOLE_SIZE, size_t offset = 0U);

        const VkDeviceSize instanceSize = 1U;
        const VkDeviceSize instanceCount = 1U;
        const VkBufferUsageFlags usageFlags = 0;
        const VkMemoryPropertyFlags memoryProperties = 0;

        inline const VkBuffer& buffer() const noexcept { return buffer_; }

        template <typename T, std::enable_if_t<std::is_pointer_v<T> && !std::is_null_pointer_v<T>, bool> = true>
        inline T memory() const noexcept
        {
            return reinterpret_cast<T>(memory());
        }

        template <typename T, std::enable_if_t<!(std::is_pointer_v<T> || std::is_null_pointer_v<T>), bool> = true>
        inline T& memory() const noexcept
        {
            return *reinterpret_cast<T*>(memory());
        }

        // WARNING: This pointer point to the beginning of buffer, so you must always apply offset to it
        // Map doesn't have such flaw because it does offset automatically
        inline void* memory() const noexcept
        {
            VmaAllocationInfo info {};
            vmaGetAllocationInfo(device_->allocator(), allocation_, &info);

            return info.pMappedData;
        }

        inline bool isHostVisible() const noexcept { return isHostVisible_; }

        inline bool isHostCoherent() const noexcept { return isHostCoherent_; }

    private:
        Buffer(const DevicePtr& device, const BufferConfiguration& configuration);

        DevicePtr device_;

        VmaAllocation allocation_ = VK_NULL_HANDLE;
        VkBuffer buffer_ = VK_NULL_HANDLE;

        bool isHostVisible_ = false;
        bool isHostCoherent_ = false;
    };

} // namespace graphics

} // namespace coffee

#endif