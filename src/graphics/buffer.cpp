#include <coffee/graphics/buffer.hpp>

#include <coffee/graphics/command_buffer.hpp>
#include <coffee/graphics/exceptions.hpp>
#include <coffee/utils/log.hpp>
#include <coffee/utils/math.hpp>
#include <coffee/utils/vk_utils.hpp>

#include <algorithm>

namespace coffee {

    namespace graphics {

        Buffer::Buffer(const DevicePtr& device, const BufferConfiguration& configuration)
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
            vmaCreateInfo.usage = configuration.allocationUsage;
            vmaCreateInfo.priority = std::clamp(configuration.priority, 0.0f, 1.0f);

            VmaAllocationInfo allocationInfo {};
            VkResult result = vmaCreateBuffer(device_->allocator(), &createInfo, &vmaCreateInfo, &buffer_, &allocation_, &allocationInfo);

            if (result != VK_SUCCESS) {
                COFFEE_ERROR("VMA failed to allocate buffer, requested size {}, with {} usage flags!", 
                createInfo.size, format::bufferUsageFlags(createInfo.usage));

                throw RegularVulkanException { result };
            }

            // This can be useful when always mapped flag applied
            mappedMemory_ = allocationInfo.pMappedData;

            VkMemoryPropertyFlags memoryProperties {};
            vmaGetAllocationMemoryProperties(device_->allocator(), allocation_, &memoryProperties);

            isHostVisible_ = (memoryProperties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
            isHostCoherent_ = (memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;

            if (((configuration.memoryProperties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) && !isHostVisible_) {
                COFFEE_WARNING("Created buffer was requested with HOST_VISIBLE bit, but wasn't created as one. "
                "Please add VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT into allocationFlags.");
            }
        }

        Buffer::~Buffer() noexcept
        {
            uint16_t remainingMappings = mapCount_.load();

            while (remainingMappings-- > 0) {
                vmaUnmapMemory(device_->allocator(), allocation_);
            }

            vmaDestroyBuffer(device_->allocator(), buffer_, allocation_);
            buffer_ = VK_NULL_HANDLE;
            allocation_ = VK_NULL_HANDLE;
        }

        BufferPtr Buffer::create(const DevicePtr& device, const BufferConfiguration& configuration)
        {
            COFFEE_ASSERT(device != nullptr, "Invalid device provided.");

            return std::unique_ptr<Buffer>(new Buffer { device, configuration });
        }

        void* Buffer::map()
        {
            void* mappedRegion = nullptr;
            VkResult result = vmaMapMemory(device_->allocator(), allocation_, &mappedRegion);

            if (result != VK_SUCCESS) {
                COFFEE_ERROR("VMA failed to map buffer!");

                throw RegularVulkanException { result };
            }

            // Update mapped memory as it can be changed because of defragmentation
            VmaAllocationInfo allocationInfo {};
            vmaGetAllocationInfo(device_->allocator(), allocation_, &allocationInfo);
            mappedMemory_ = allocationInfo.pMappedData;

            mapCount_.fetch_add(1, std::memory_order_relaxed);
            return mappedRegion;
        }

        void Buffer::unmap() noexcept
        {
            if (mapCount_.load(std::memory_order_acquire) == 0) {
                return;
            }

            if (mapCount_.fetch_sub(1, std::memory_order_acq_rel) == 0) {
                mappedMemory_ = nullptr;
            }

            vmaUnmapMemory(device_->allocator(), allocation_);
        }

        void Buffer::flush(size_t size, size_t offset)
        {
            VkResult result = vmaFlushAllocation(device_->allocator(), allocation_, offset, size);

            if (result != VK_SUCCESS) {
                COFFEE_ERROR("VMA failed to flush buffer, requested size {} and offset {}!", size, offset);

                throw RegularVulkanException { result };
            }
        }

        void Buffer::invalidate(size_t size, size_t offset)
        {
            VkResult result = vmaInvalidateAllocation(device_->allocator(), allocation_, offset, size);

            if (result != VK_SUCCESS) {
                COFFEE_ERROR("VMA failed to invalidate buffer, requested size {} and offset {}!", size, offset);

                throw RegularVulkanException { result };
            }
        }

    } // namespace graphics

} // namespace coffee