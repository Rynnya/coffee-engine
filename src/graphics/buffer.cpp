#include <coffee/graphics/buffer.hpp>

#include <coffee/graphics/command_buffer.hpp>
#include <coffee/utils/exceptions.hpp>
#include <coffee/utils/log.hpp>
#include <coffee/utils/math.hpp>
#include <coffee/utils/vk_utils.hpp>

#include <algorithm>

namespace coffee {

    Buffer::Buffer(const GPUDevicePtr& device, const BufferConfiguration& configuration)
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

    BufferPtr Buffer::create(const GPUDevicePtr& device, const BufferConfiguration& configuration)
    {
        COFFEE_ASSERT(device != nullptr, "Invalid device provided.");

        return std::unique_ptr<Buffer>(new Buffer { device, configuration });
    }

    //BufferPtr Buffer::create(
    //    const GPUDevicePtr& device,
    //    const FilesystemPtr& filesystem,
    //    const FSBufferConfiguration& configuration,
    //    const ThreadContext& ctx
    //)
    //{
    //    COFFEE_ASSERT(device != nullptr, "Invalid device provided.");

    //    if (configuration.path.empty()) {
    //        throw FilesystemException { FilesystemException::Type::FileNotFound, "Empty filepath provided!" };
    //    }

    //    // Getting metadata is way lighter than getting whole content instantly
    //    // We might failed to allocate buffer and waste a lot of time for nothing
    //    // This will also check if file exist
    //    Filesystem::Entry metadata = filesystem->getMetadata(configuration.path);

    //    if (metadata.uncompressedSize <= configuration.offset) {
    //        return nullptr;
    //    }

    //    size_t rawBytesSize = std::min(configuration.size, metadata.uncompressedSize - configuration.offset);
    //    uint32_t instanceSize = 2;

    //    while (rawBytesSize % instanceSize != 0) {
    //        instanceSize++;
    //    }

    //    BufferConfiguration bufferConfiguration {};
    //    bufferConfiguration.instanceSize = instanceSize;
    //    bufferConfiguration.instanceCount = static_cast<uint32_t>(rawBytesSize / instanceSize);
    //    bufferConfiguration.usageFlags = configuration.usageFlags;
    //    bufferConfiguration.memoryProperties = configuration.memoryProperties;
    //    bufferConfiguration.allocationFlags = configuration.allocationFlags;
    //    bufferConfiguration.allocationUsage = configuration.allocationUsage;
    //    bufferConfiguration.priority = configuration.priority;

    //    if (configuration.memoryProperties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
    //        // Buffer will be readable from host, we can write directly without staging buffer
    //        BufferPtr createdBuffer = std::unique_ptr<Buffer>(new Buffer { device, bufferConfiguration });

    //        std::vector<uint8_t> rawBytes = filesystem->getContent(configuration.path, ctx);
    //        std::memcpy(createdBuffer->map(), rawBytes.data() + configuration.offset, rawBytesSize);
    //        createdBuffer->flush();

    //        // Leave this memory as mapped so that end user can instantly use it for own things
    //        // Implementation will call unmap automatically so it doesn't a problem

    //        return createdBuffer;
    //    }
    //    else if (configuration.memoryProperties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
    //        // Buffer must be uploaded to GPU
    //        bufferConfiguration.usageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    //        bufferConfiguration.allocationFlags |=
    //            VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    //        BufferPtr createdBuffer = std::unique_ptr<Buffer>(new Buffer { device, bufferConfiguration });

    //        if (createdBuffer->isHostVisible()) {
    //            // ReBAR is available / Integrated GPU / Less than BAR memory size (typically 256 mb)
    //            std::vector<uint8_t> rawBytes = filesystem->getContent(configuration.path, ctx);
    //            std::memcpy(createdBuffer->map(), rawBytes.data() + configuration.offset, rawBytesSize);
    //            createdBuffer->flush();
    //            // Leave this memory as mapped so that end user can instantly use it for own things
    //            // Implementation will call unmap automatically so it doesn't a problem
    //        }
    //        else {
    //            // Doing stuff old good way - through staging buffer
    //            BufferConfiguration stagingBufferConfiguration {};
    //            stagingBufferConfiguration.instanceSize = instanceSize;
    //            stagingBufferConfiguration.instanceCount = static_cast<uint32_t>(rawBytesSize / instanceSize);
    //            stagingBufferConfiguration.usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    //            stagingBufferConfiguration.memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    //            stagingBufferConfiguration.allocationFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    //            BufferPtr stagingBuffer = std::unique_ptr<Buffer>(new Buffer { device, stagingBufferConfiguration });

    //            std::vector<uint8_t> rawBytes = filesystem->getContent(configuration.path, ctx);
    //            std::memcpy(stagingBuffer->map(), rawBytes.data() + configuration.offset, rawBytesSize);
    //            stagingBuffer->flush();
    //            stagingBuffer->unmap();

    //            device->singleTimeTransfer([&](const CommandBuffer& commandBuffer) {
    //                VkBufferCopy copyRegion {};
    //                copyRegion.size = rawBytesSize;
    //                vkCmdCopyBuffer(commandBuffer, stagingBuffer->buffer(), createdBuffer->buffer(), 1, &copyRegion);
    //            });
    //        }

    //        return createdBuffer;
    //    }
    //    // Failed to identify buffer type by memory properties, will try by usage flags
    //    else if (configuration.usageFlags & VK_BUFFER_USAGE_TRANSFER_SRC_BIT && configuration.usageFlags & VK_BUFFER_USAGE_TRANSFER_DST_BIT) {
    //        // User wanna create a "swap" buffer in either RAM or VRAM
    //        // The only proper way to identify this is by using VMA memory usage
    //        // So we need assert if user wanna create buffer with UNKNOWN or AUTO memory usage

    //        switch (bufferConfiguration.allocationUsage) {
    //            case VMA_MEMORY_USAGE_UNKNOWN:
    //            case VMA_MEMORY_USAGE_AUTO:
    //                throw RegularVulkanException { VK_ERROR_FORMAT_NOT_SUPPORTED };
    //            default: // Everything else is fine
    //                break;
    //        }

    //        BufferPtr createdBuffer = std::unique_ptr<Buffer>(new Buffer { device, bufferConfiguration });

    //        BufferConfiguration stagingBufferConfiguration {};
    //        stagingBufferConfiguration.instanceSize = instanceSize;
    //        stagingBufferConfiguration.instanceCount = static_cast<uint32_t>(rawBytesSize / instanceSize);
    //        stagingBufferConfiguration.usageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    //        stagingBufferConfiguration.memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    //        stagingBufferConfiguration.allocationFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    //        BufferPtr stagingBuffer = std::unique_ptr<Buffer>(new Buffer { device, stagingBufferConfiguration });

    //        std::vector<uint8_t> rawBytes = filesystem->getContent(configuration.path, ctx);
    //        std::memcpy(stagingBuffer->map(), rawBytes.data() + configuration.offset, rawBytesSize);
    //        stagingBuffer->flush();
    //        stagingBuffer->unmap();

    //        device->singleTimeTransfer([&](const CommandBuffer& commandBuffer) {
    //            VkBufferCopy copyRegion {};
    //            copyRegion.size = rawBytesSize;
    //            vkCmdCopyBuffer(commandBuffer, stagingBuffer->buffer(), createdBuffer->buffer(), 1, &copyRegion);
    //        });

    //        return createdBuffer;
    //    }

    //    // Everything else before is failed, we cannot issue this
    //    return nullptr;
    //}

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

} // namespace coffee