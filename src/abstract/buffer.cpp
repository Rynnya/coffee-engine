#include <coffee/abstract/buffer.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    AbstractBuffer::AbstractBuffer(uint32_t instanceCount, uint32_t instanceSize, BufferUsage usage, MemoryProperty props) noexcept
            : instanceCount_ { instanceCount }
            , instanceSize_ { instanceSize }
            , realSize_ { 0 }
            , usageFlags_ { usage }
            , memoryFlags_ { props } {}

    uint32_t AbstractBuffer::getInstanceCount() const noexcept {
        return instanceCount_;
    }

    uint32_t AbstractBuffer::getInstanceSize() const noexcept {
        return instanceSize_;
    }

    size_t AbstractBuffer::getTotalSize() const noexcept {
        return static_cast<size_t>(instanceSize_) * instanceCount_;
    }

    size_t AbstractBuffer::getRealSize() const noexcept {
        return realSize_;
    }

    BufferUsage AbstractBuffer::getUsageFlags() const noexcept {
        return usageFlags_;
    }

    MemoryProperty AbstractBuffer::getMemoryFlags() const noexcept {
        return memoryFlags_;
    }

} // namespace coffee