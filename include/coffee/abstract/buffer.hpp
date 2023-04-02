#ifndef COFFEE_ABSTRACT_BUFFER
#define COFFEE_ABSTRACT_BUFFER

#include <coffee/types.hpp>
#include <coffee/utils/non_moveable.hpp>

namespace coffee {

    class AbstractBuffer : NonMoveable {
    public:
        AbstractBuffer(uint32_t instanceCount, uint32_t instanceSize, BufferUsage usage, MemoryProperty props) noexcept;
        virtual ~AbstractBuffer() noexcept = default;

        template <typename T, std::enable_if_t<!std::is_pointer_v<T> && !std::is_null_pointer_v<T> && !std::is_array_v<T>, bool> = true>
        void write(const T& object, size_t offset = 0ULL) {
            write(&object, sizeof(object), offset);
        }

        virtual void write(const void* data, size_t size, size_t offset = 0ULL) = 0;
        // Flush must be called when CPU write new data into buffer, so GPU can see changes
        virtual void flush(size_t size = std::numeric_limits<size_t>::max(), size_t offset = 0ULL) = 0;
        // Invalidate must be called when GPU write new data into buffer, so CPU can see changes
        virtual void invalidate(size_t size = std::numeric_limits<size_t>::max(), size_t offset = 0ULL) = 0;
        // Resize will flush and invalidate all writes that was issued before calling it
        // It also will be discarded if new size is same or smaller than original
        virtual void resize(uint32_t instanceCount, uint32_t instanceSize) = 0;

        uint32_t getInstanceCount() const noexcept;
        uint32_t getInstanceSize() const noexcept;
        // This returns requested from user size
        size_t getTotalSize() const noexcept;
        // This returns actually allocated size
        size_t getRealSize() const noexcept;

        BufferUsage getUsageFlags() const noexcept;
        MemoryProperty getMemoryFlags() const noexcept;

    protected:
        uint32_t instanceCount_;
        uint32_t instanceSize_;
        size_t realSize_;

        BufferUsage usageFlags_;
        MemoryProperty memoryFlags_;
    };

    using Buffer = std::shared_ptr<AbstractBuffer>;

} // namespace coffee

#endif