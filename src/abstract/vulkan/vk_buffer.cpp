#include <coffee/abstract/vulkan/vk_buffer.hpp>

#include <coffee/utils/log.hpp>
#include <coffee/utils/math.hpp>
#include <coffee/utils/vk_utils.hpp>

namespace coffee {

    VulkanBuffer::VulkanBuffer(VulkanDevice& device, const BufferConfiguration& configuration)
        : AbstractBuffer { configuration.instanceCount, configuration.instanceSize, configuration.usage, configuration.properties }
        , device_ { device }
    {
        instanceCount_ = configuration.instanceCount;
        instanceSize_ = configuration.instanceSize;
        alwaysKeepMapped_ = (static_cast<size_t>(configuration.instanceCount) * configuration.instanceSize) <= alwaysMappedThreshold;

        COFFEE_ASSERT(instanceCount_ > 0, "Buffer cannot be allocated with size 0. (instanceCount)");
        COFFEE_ASSERT(instanceSize_ > 0, "Buffer cannot be allocated with size 0. (instanceSize)");

        usageFlags_ = configuration.usage;
        memoryFlags_ = configuration.properties;

        VkBufferCreateInfo createInfo { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        createInfo.size = static_cast<size_t>(instanceSize_) * instanceCount_;
        createInfo.usage = VkUtils::transformBufferFlags(usageFlags_);
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        COFFEE_THROW_IF(
            vkCreateBuffer(device_.getLogicalDevice(), &createInfo, nullptr, &buffer) != VK_SUCCESS, "Failed to create buffer!");

        VkMemoryRequirements memoryRequirements {};
        vkGetBufferMemoryRequirements(device_.getLogicalDevice(), buffer, &memoryRequirements);

        offsetAlignment = [](const VkPhysicalDeviceLimits& limits, BufferUsage usage) {
            if ((usage & BufferUsage::Uniform) == BufferUsage::Uniform) {
                return Math::roundToMultiple(limits.minUniformBufferOffsetAlignment, limits.nonCoherentAtomSize);
            }

            if ((usage & BufferUsage::Storage) == BufferUsage::Storage) {
                return Math::roundToMultiple(limits.minStorageBufferOffsetAlignment, limits.nonCoherentAtomSize);
            }

            return limits.nonCoherentAtomSize;
        }(device_.getProperties().limits, usageFlags_);

        VkMemoryAllocateInfo allocateInfo { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        allocateInfo.allocationSize = memoryRequirements.size;
        allocateInfo.memoryTypeIndex = VkUtils::findMemoryType(
            device_.getPhysicalDevice(),
            memoryRequirements.memoryTypeBits, 
            VkUtils::transformMemoryFlags(memoryFlags_));

        COFFEE_THROW_IF(
            vkAllocateMemory(device_.getLogicalDevice(), &allocateInfo, nullptr, &memory) != VK_SUCCESS, 
            "Failed to allocate memory for buffer!");

        COFFEE_THROW_IF(
            vkBindBufferMemory(device_.getLogicalDevice(), buffer, memory, 0U) != VK_SUCCESS, "Failed to bind buffer to memory!");

        if (((memoryFlags_ & MemoryProperty::HostVisible) == MemoryProperty::HostVisible) && alwaysKeepMapped_) {
            COFFEE_THROW_IF(
                vkMapMemory(device_.getLogicalDevice(), memory, 0, memoryRequirements.size, 0, &mappedMemory_) != VK_SUCCESS,
                "Failed to map memory!");
        }

        realSize_ = memoryRequirements.size;
    }

    VulkanBuffer::~VulkanBuffer() {
        if (mappedMemory_ != nullptr) {
            vkUnmapMemory(device_.getLogicalDevice(), memory);
        }

        vkDestroyBuffer(device_.getLogicalDevice(), buffer, nullptr);
        vkFreeMemory(device_.getLogicalDevice(), memory, nullptr);
    }

    void VulkanBuffer::write(const void* data, size_t size, size_t offset) {
        COFFEE_ASSERT(
            (memoryFlags_ & MemoryProperty::HostVisible) == MemoryProperty::HostVisible,
            "Calling write() while buffer isn't host visible is forbidden.");
        COFFEE_ASSERT(data != nullptr, "data was nullptr.");
        COFFEE_ASSERT(size < std::numeric_limits<size_t>::max() - offset, "Combination of size and offset will cause an arithmetic overflow.");
        COFFEE_ASSERT(offset + size <= getTotalSize(), "Combination of size and offset will cause an buffer overflow.");

        std::scoped_lock<std::mutex> lock { allocationMutex_ };

        if (alwaysKeepMapped_) {
            std::memcpy(reinterpret_cast<uint8_t*>(mappedMemory_) + offset, data, size);
            return;
        }

        if (this->map(size, offset) == VK_SUCCESS) {
            std::memcpy(mappedMemory_, data, size);

            mappedSize_ = size;
            mappedOffset_ = offset;

            return;
        }

        size_t splitMultiplier = 2;

        while (this->map(size / splitMultiplier, offset) != VK_SUCCESS) {
            splitMultiplier *= 2;

            // After that much of divisions it's more likely that we just run out of memory
            // So it will be better to just throw and don't care
            COFFEE_THROW_IF(
                splitMultiplier >= 128 || splitMultiplier >= size,
                "Failed to find good chunk size for map! This more likely means that there not enough memory!");
        }

        // This was successfully mapped region, so write to it immediately
        size_t chunkSize = size / splitMultiplier;
        size_t chunkOffset = chunkSize;
        std::memcpy(mappedMemory_, data, chunkSize);

        while (chunkOffset < size) {
            COFFEE_THROW_IF(
                this->map(chunkSize, offset + chunkOffset) != VK_SUCCESS, 
                "Failed to map memory through chunking! This more likely means that there not enough memory!");

            std::memcpy(mappedMemory_, data, chunkSize);
            chunkOffset += chunkSize;
        }

        mappedSize_ = chunkSize;
        mappedOffset_ = offset + chunkOffset - chunkSize;
    }

    void VulkanBuffer::flush(size_t size, size_t offset) {
        COFFEE_ASSERT(mappedMemory_ != nullptr, "Forbidden call to flush() when memory isn't mapped. Do map() before calling this.");

        alignOffset(size, offset);

        VkMappedMemoryRange mappedRange { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
        mappedRange.memory = memory;
        mappedRange.offset = offset;
        mappedRange.size = size;

        COFFEE_THROW_IF(
            vkFlushMappedMemoryRanges(device_.getLogicalDevice(), 1, &mappedRange) != VK_SUCCESS, "Failed to flush memory range!");
    }

    void VulkanBuffer::invalidate(size_t size, size_t offset) {
        COFFEE_ASSERT(mappedMemory_ != nullptr, "Forbidden call to invalidate() when memory isn't mapped. Do map() before calling this.");

        alignOffset(size, offset);

        VkMappedMemoryRange mappedRange { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
        mappedRange.memory = memory;
        mappedRange.offset = offset;
        mappedRange.size = size;

        COFFEE_THROW_IF(
            vkInvalidateMappedMemoryRanges(device_.getLogicalDevice(), 1, &mappedRange) != VK_SUCCESS, 
            "Failed to invalidate memory range!");
    }

    void VulkanBuffer::resize(uint32_t instanceCount, uint32_t instanceSize) {
        COFFEE_ASSERT(mappedMemory_ == nullptr, "Forbidden call to resize() when memory mapped. Do unmap() before calling this.");

        if (static_cast<size_t>(instanceSize_) * instanceCount_ >= static_cast<size_t>(instanceSize) * instanceCount) {
            return;
        }

        // TODO: Implement
    }

    VkResult VulkanBuffer::map(size_t size, size_t offset) {
        if (mappedSize_ == size && mappedOffset_ == offset) {
            return VK_SUCCESS;
        }

        // TODO: Implement subrange support, so memory won't be reallocated when it's already in range

        if (mappedMemory_ != nullptr) {
            vkUnmapMemory(device_.getLogicalDevice(), memory);
        }

        return vkMapMemory(device_.getLogicalDevice(), memory, offset, size, 0, &mappedMemory_);
    }

    void VulkanBuffer::alignOffset(size_t& size, size_t& offset) {
        if (offset < mappedOffset_) {
            offset = mappedOffset_;
        }

        size_t alignedOffset = Math::roundToMultiple(offset, offsetAlignment);

        if (alignedOffset != offset) {
            alignedOffset -= offsetAlignment;

            if (size != VK_WHOLE_SIZE) {
                size += offsetAlignment;
            }
        }
    }

}