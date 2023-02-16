#include <coffee/abstract/vulkan/vk_buffer.hpp>

#include <coffee/utils/log.hpp>
#include <coffee/utils/math.hpp>
#include <coffee/utils/vk_utils.hpp>

namespace coffee {

    VulkanBuffer::VulkanBuffer(VulkanDevice& device, const BufferConfiguration& configuration)
        : AbstractBuffer { 
            configuration.usage,
            configuration.properties,
            configuration.instanceCount,
            configuration.instanceSize,
            calculateAlignment(device, configuration.alignment, configuration.instanceSize, configuration.properties)
        } , device_ { device }
    {
        VkBufferCreateInfo createInfo { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        createInfo.size = static_cast<VkDeviceSize>(alignment_) * configuration.instanceCount;
        createInfo.usage = VkUtils::transformBufferFlags(usageFlags_);
        createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        COFFEE_THROW_IF(
            vkCreateBuffer(device_.getLogicalDevice(), &createInfo, nullptr, &buffer) != VK_SUCCESS, "Failed to create buffer!");

        VkMemoryRequirements memoryRequirements {};
        vkGetBufferMemoryRequirements(device_.getLogicalDevice(), buffer, &memoryRequirements);

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
    }

    VulkanBuffer::~VulkanBuffer() {
        if (mappedMemory != nullptr) {
            vkUnmapMemory(device_.getLogicalDevice(), memory);
        }

        vkDestroyBuffer(device_.getLogicalDevice(), buffer, nullptr);
        vkFreeMemory(device_.getLogicalDevice(), memory, nullptr);
    }

    void* VulkanBuffer::map(size_t size, size_t offset) {
        std::scoped_lock<std::mutex> lock { allocationMutex_ };

        if (mappedMemory == nullptr) {
            // TODO: This might crash because of very huge buffer that cannot be allocated on RAM
            // So we must actually handle this case and make a allocation that will be an part of whole buffer
            // Then, when next map comes in, we check if it worth to remap, if so - we remap another part of memory
            // And we cannot handle same architecture that DX12 can - multiple allocations
            // Might be a good idea to handle just write() and read() functions instead of direct access
            vkMapMemory(device_.getLogicalDevice(), memory, 0, std::numeric_limits<uint64_t>::max(), 0, &mappedMemory);
        }

        return static_cast<char*>(mappedMemory) + offset;
    }

    void VulkanBuffer::flush(size_t size, size_t offset) {
        COFFEE_ASSERT(mappedMemory != nullptr, "Forbidden call to flush() when memory isn't mapped. Do map() before calling this.");

        VkMappedMemoryRange mappedRange { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
        mappedRange.memory = memory;
        mappedRange.offset = offset;
        mappedRange.size = size;

        COFFEE_THROW_IF(
            vkFlushMappedMemoryRanges(device_.getLogicalDevice(), 1, &mappedRange) != VK_SUCCESS, "Failed to flush memory range!");
    }

    void VulkanBuffer::invalidate(size_t size, size_t offset) {
        COFFEE_ASSERT(mappedMemory != nullptr, "Forbidden call to invalidate() when memory isn't mapped. Do map() before calling this.");

        VkMappedMemoryRange mappedRange { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
        mappedRange.memory = memory;
        mappedRange.offset = offset;
        mappedRange.size = size;

        COFFEE_THROW_IF(
            vkInvalidateMappedMemoryRanges(device_.getLogicalDevice(), 1, &mappedRange) != VK_SUCCESS, 
            "Failed to invalidate memory range!");
    }

    void VulkanBuffer::resize(uint32_t instanceCount, uint32_t instanceSize) {
        COFFEE_ASSERT(mappedMemory == nullptr, "Forbidden call to resize() when memory mapped. Do unmap() before calling this.");

        if (alignment_ * instanceCount >= static_cast<size_t>(instanceSize) * instanceCount) {
            return;
        }

        // TODO: Implement
    }

    void VulkanBuffer::unmap() {
        COFFEE_ASSERT(mappedMemory != nullptr, "Forbidden call to unmap() when memory isn't mapped. Do map() before calling this.");

        std::scoped_lock<std::mutex> lock { allocationMutex_ };

        vkUnmapMemory(device_.getLogicalDevice(), memory);
        mappedMemory = nullptr;
    }

    size_t VulkanBuffer::calculateAlignment(
        VulkanDevice& device,
        size_t requestedAlignment,
        size_t requiredInstanceSize,
        MemoryProperty properties
    ) noexcept {
        // nonCoherentAtomSize is always power of 2 because Vulkan API requires this
        size_t properAlignment = Math::roundToPowerOf2(requestedAlignment);

        #pragma warning(suppress: 26813) // Suppresses useless warning about using & instead of ==, but we need == here
        if ((properties & (MemoryProperty::HostVisible | MemoryProperty::HostCoherent)) == MemoryProperty::HostVisible) {
            // Both numbers are power of 2, so we only need the biggest one to make a proper alignment
            const VkDeviceSize biggest =
                std::max(static_cast<VkDeviceSize>(properAlignment), device.getProperties().limits.nonCoherentAtomSize);
            return (requiredInstanceSize + biggest - 1) & ~(biggest - 1);
        }

        if (properAlignment > 0) {
            // Memory flags contains coherent bit, so nonCoherentAtomSize alignment isn't required
            return (requiredInstanceSize + properAlignment - 1) & ~(properAlignment - 1);
        }

        // No alignment at all
        return requiredInstanceSize;
    }

}