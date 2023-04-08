#include <coffee/graphics/buffer.hpp>

#include <coffee/utils/log.hpp>
#include <coffee/utils/math.hpp>
#include <coffee/utils/vk_utils.hpp>

#include <algorithm>

namespace coffee {

    BufferImpl::BufferImpl(Device& device, const BufferConfiguration& configuration)
        : instanceSize { configuration.instanceSize }
        , instanceCount { configuration.instanceCount }
        , usageFlags { configuration.usageFlags }
        , memoryProperties { configuration.memoryProperties }
        , device_ { device }
    {
        COFFEE_ASSERT(instanceCount > 0, "Buffer cannot be allocated with size 0. (instanceCount)");
        COFFEE_ASSERT(instanceSize > 0, "Buffer cannot be allocated with size 0. (instanceSize)");

        VkBufferCreateInfo createInfo { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        createInfo.size = static_cast<VkDeviceSize>(configuration.instanceCount) * configuration.instanceSize;
        createInfo.usage = configuration.usageFlags;
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo vmaCreateInfo {};
        vmaCreateInfo.flags = configuration.allocationFlags;
        vmaCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        vmaCreateInfo.priority = std::clamp(configuration.priority, 0.0f, 1.0f);

        VmaAllocationInfo allocationInfo {};

        COFFEE_THROW_IF(
            vmaCreateBuffer(device_.allocator(), &createInfo, &vmaCreateInfo, &buffer_, &allocation_, &allocationInfo) != VK_SUCCESS, 
            "VMA failed to allocate and bind memory for buffer!");

        mappedMemory_ = allocationInfo.pMappedData;

        VkMemoryPropertyFlags memoryProperties {};
        vmaGetAllocationMemoryProperties(device_.allocator(), allocation_, &memoryProperties);

        isHostVisible_ = (memoryProperties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
        isHostCoherent_ = (memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;

        if (((configuration.memoryProperties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) && !isHostVisible_) {
            COFFEE_WARNING("Created buffer was requested with HOST_VISIBLE bit, but wasn't created as one. "
                "Please add VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT into allocationFlags.");
        }
    }

    BufferImpl::~BufferImpl() noexcept
    {
        vmaDestroyBuffer(device_.allocator(), buffer_, allocation_);
        buffer_ = VK_NULL_HANDLE;
        allocation_ = VK_NULL_HANDLE;
    }

    void BufferImpl::map() noexcept
    {
        if (!isHostVisible_) {
            return;
        }

        vmaMapMemory(device_.allocator(), allocation_, &mappedMemory_);
    }

    void BufferImpl::unmap() noexcept
    {
        if (!isHostVisible_) {
            return;
        }

        vmaUnmapMemory(device_.allocator(), allocation_);
    }

    void BufferImpl::flush(size_t size, size_t offset)
    {
        COFFEE_THROW_IF(vmaFlushAllocation(device_.allocator(), allocation_, offset, size) != VK_SUCCESS, "Failed to flush buffer!");
    }

    void BufferImpl::invalidate(size_t size, size_t offset)
    {
        COFFEE_THROW_IF(vmaInvalidateAllocation(device_.allocator(), allocation_, offset, size) != VK_SUCCESS, "Failed to invalidate buffer!");
    }

} // namespace coffee