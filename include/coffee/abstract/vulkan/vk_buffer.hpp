#ifndef COFFEE_VK_BUFFER
#define COFFEE_VK_BUFFER

#include <coffee/abstract/buffer.hpp>
#include <coffee/abstract/vulkan/vk_device.hpp>

#include <coffee/types.hpp>
#include <coffee/utils/non_copyable.hpp>

namespace coffee {

    class VulkanBuffer : public AbstractBuffer {
    public:
        VulkanBuffer(VulkanDevice& device, const BufferConfiguration& configuration);
        ~VulkanBuffer();

        void write(const void* data, size_t size, size_t offset) override;
        void flush(size_t size, size_t offset) override;
        void invalidate(size_t size, size_t offset) override;
        void resize(uint32_t instanceCount, uint32_t instanceSize) override;

        VkBuffer buffer = nullptr;
        VkDeviceMemory memory = nullptr;

        VkDeviceSize offsetAlignment = 0;

    private:
        VkResult map(size_t size, size_t offset);
        void alignOffset(size_t& size, size_t& offset);

        static constexpr size_t alwaysMappedThreshold = 32 * 1024 * 1024;
        bool alwaysKeepMapped_ = false;

        void* mappedMemory_ = nullptr;
        size_t mappedSize_ = 0;
        size_t mappedOffset_ = 0;

        VulkanDevice& device_;
        std::mutex allocationMutex_ {};
    };

} // namespace coffee

#endif