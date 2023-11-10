#include <coffee/graphics/buffer.hpp>

#include <coffee/graphics/command_buffer.hpp>
#include <coffee/graphics/exceptions.hpp>
#include <coffee/utils/log.hpp>
#include <coffee/utils/math.hpp>
#include <coffee/utils/vk_utils.hpp>

#include <algorithm>

namespace coffee { namespace graphics {

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

        std::vector<uint32_t> queueFamilyIndices {};
        queueFamilyIndices.push_back(device->graphicsQueueFamilyIndex());
        queueFamilyIndices.push_back(device->computeQueueFamilyIndex());
        queueFamilyIndices.push_back(device->transferQueueFamilyIndex());

        std::sort(queueFamilyIndices.begin(), queueFamilyIndices.end());
        queueFamilyIndices.erase(std::unique(queueFamilyIndices.begin(), queueFamilyIndices.end()), queueFamilyIndices.end());

        if (queueFamilyIndices.size() > 1) {
            createInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size());
            createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
        }

        VmaAllocationCreateInfo vmaCreateInfo {};
        vmaCreateInfo.flags = configuration.allocationFlags;
        vmaCreateInfo.usage = configuration.allocationUsage;
        vmaCreateInfo.priority = std::clamp(configuration.priority, 0.0f, 1.0f);
        VkResult result = vmaCreateBuffer(device_->allocator(), &createInfo, &vmaCreateInfo, &buffer_, &allocation_, nullptr);

        if (result != VK_SUCCESS) {
            COFFEE_ERROR("VMA failed to allocate buffer, requested size {}, with {} usage flags!",
                createInfo.size, static_cast<uint64_t>(createInfo.usage));

            throw RegularVulkanException { result };
        }

        VkMemoryPropertyFlags memoryProperties {};
        vmaGetAllocationMemoryProperties(device_->allocator(), allocation_, &memoryProperties);

        isHostVisible_ = (memoryProperties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
        isHostCoherent_ = (memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;

        bool isUserRequestedHostVisible = (configuration.memoryProperties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
        bool isUserRequestedHostCoherent = (configuration.memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;

        if (isUserRequestedHostVisible && !isHostVisible_) {
            COFFEE_WARNING(
                    "Buffer was requested with HOST_VISIBLE bit, but wasn't set during creation. This might lead to unexpected behaviour. "
                    "Please add VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT or VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT "
                    "into allocationFlags if buffer must be mapped on CPU."
                );
        }

        if (isUserRequestedHostCoherent && !isHostVisible_) {
            COFFEE_WARNING(
                    "Buffer was requested with HOST_COHERENT bit, but HOST_VISIBLE bit wasn't set during creation. This might lead to "
                    "unexpected behaviour. Please add VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT or "
                    "VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT into allocationFlags if buffer must be mapped on CPU."
                );
        }
    }

    Buffer::~Buffer() noexcept { vmaDestroyBuffer(device_->allocator(), buffer_, allocation_); }

    BufferPtr Buffer::create(const DevicePtr& device, const BufferConfiguration& configuration)
    {
        COFFEE_ASSERT(device != nullptr, "Invalid device provided.");

        return std::shared_ptr<Buffer>(new Buffer { device, configuration });
    }

    void* Buffer::map()
    {
        void* mappedRegion = nullptr;
        VkResult result = vmaMapMemory(device_->allocator(), allocation_, &mappedRegion);

        if (result != VK_SUCCESS) {
            COFFEE_ERROR("VMA failed to map buffer!");

            throw RegularVulkanException { result };
        }

        return mappedRegion;
    }

    void Buffer::unmap() noexcept { vmaUnmapMemory(device_->allocator(), allocation_); }

    void Buffer::flush(size_t size, size_t offset)
    {
        VkResult result = vmaFlushAllocation(device_->allocator(), allocation_, offset, size);

        if (result != VK_SUCCESS) {
            COFFEE_ERROR("Failed to flush buffer, requested size {} and offset {}!", size, offset);

            throw RegularVulkanException { result };
        }
    }

    void Buffer::invalidate(size_t size, size_t offset)
    {
        VkResult result = vmaInvalidateAllocation(device_->allocator(), allocation_, offset, size);

        if (result != VK_SUCCESS) {
            COFFEE_ERROR("Failed to invalidate buffer, requested size {} and offset {}!", size, offset);

            throw RegularVulkanException { result };
        }
    }

}} // namespace coffee::graphics
