#ifndef COFFEE_UTILS_MAIN_UTILS
#define COFFEE_UTILS_MAIN_UTILS

#include <coffee/utils/log.hpp>

#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace coffee {

    namespace utils {

        namespace detail {

            template <typename T>
            class MovableIL {
            public:
                MovableIL(T&& object) noexcept(std::is_nothrow_move_constructible_v<T>) : object_ { std::move(object) } {}

                constexpr operator T() const&& noexcept(std::is_nothrow_move_constructible_v<T>) { return std::move(object_); }

            private:
                mutable T object_;
            };

        } // namespace detail

        // Opens file and reads it entirely into std::vector<uint8_t>
        extern std::vector<uint8_t> readFile(const std::string& fileName);
        // Same as above, but returns allocated memory and size of that memory
        extern size_t readFile(const std::string& fileName, uint8_t*& memoryPointer);

        // Wrapper that allow compiler to properly move initializer list
        // Must be used for move-only objects that wanna be created through initializer list
        template <typename Base, typename Desirable>
        static Desirable moveList(std::initializer_list<detail::MovableIL<Base>> initializerList)
        {
            static_assert(
                std::is_constructible_v<Desirable, decltype(initializerList.begin()), decltype(initializerList.end())>,
                "Desirable list must be constructable from initializer list begin and end iterators."
            );

            return { std::make_move_iterator(initializerList.begin()), std::make_move_iterator(initializerList.end()) };
        }

        // From: https://stackoverflow.com/a/57595105
        template <typename T, typename... Rest>
        static void hashCombine(std::size_t& seed, const T& v, const Rest&... rest)
        {
            seed ^= std::hash<T> {}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            (hashCombine(seed, rest), ...);
        }

        // Stream-like wrapper for std::vector<uint8_t> or raw pointer
        // Note that this will only take a view of buffer, not actual buffer copy
        // There's only one exception to this rule, but always assume that it's just a view, not actual copy
        // Also be aware that this class doesn't verify out of bounds accesses in release mode
        // ALWAYS CHECK FOR SIZE BECAUSE 0 SIZE IS ALLOWED
        template <size_t BufferSize>
        class ReadOnlyStream {
        public:
            inline ReadOnlyStream(std::vector<uint8_t>& stream) : ptr_ { stream.data() }, size_ { stream.size() }, isOwner_ { false } {}

            inline ReadOnlyStream(uint8_t* stream, size_t size, bool isOwner) : ptr_ { stream }, size_ { size }, isOwner_ { isOwner } {}

            inline ReadOnlyStream(const uint8_t* stream, size_t size) : ptr_ { stream }, size_ { size }, isOwner_ { false } {}

            inline ~ReadOnlyStream() noexcept
            {
                if (isOwner_) {
                    // isOwner_ flag is only set when non-const memory pointer is provided
                    delete const_cast<uint8_t*>(ptr_);
                }
            }

            template <typename T>
            inline T read()
            {
                static_assert(!std::is_pointer_v<T> && !std::is_null_pointer_v<T>, "Don't use pointers, just use readBuffer.");
                static_assert(BufferSize >= sizeof(T), "Insufficient buffer, please increase it's template size.");

                COFFEE_ASSERT(offset_ + sizeof(T) <= size_, "Invalid read call: Out of bounds access.");

                std::memcpy(underlyingBuffer_, ptr_ + offset_, sizeof(T));
                offset_ += sizeof(T);

                return *reinterpret_cast<T*>(underlyingBuffer_);
            }

            template <typename T, size_t Amount>
            inline T* readBuffer()
            {
                static_assert(!std::is_pointer_v<T> && !std::is_null_pointer_v<T>, "Don't use pointers");
                static_assert(BufferSize >= Amount * sizeof(T), "Insufficient buffer, please increase it's template size.");

                COFFEE_ASSERT(offset_ + Amount * sizeof(T) <= size_, "Invalid readBuffer call: Out of bounds access.");

                std::memcpy(underlyingBuffer_, ptr_ + offset_, Amount * sizeof(T));
                offset_ += Amount * sizeof(T);

                return reinterpret_cast<T*>(underlyingBuffer_);
            }

            template <typename T>
            inline void readDirectly(T* dstMemory, size_t amount = 1)
            {
                static_assert(!std::is_pointer_v<T> && !std::is_null_pointer_v<T>, "Don't use pointers");

                COFFEE_ASSERT(offset_ + amount * sizeof(T) <= size_, "Invalid readDirectly call: Out of bounds access.");

                std::memcpy(reinterpret_cast<uint8_t*>(dstMemory), ptr_ + offset_, amount * sizeof(T));
                offset_ += amount * sizeof(T);
            }

            template <typename T>
            inline void readDirectlyWithOffset(T* dstMemory, size_t offset, size_t amount = 1)
            {
                static_assert(!std::is_pointer_v<T> && !std::is_null_pointer_v<T>, "Don't use pointers");

                COFFEE_ASSERT(offset + amount * sizeof(T) <= size_, "Invalid readDirectly call: Out of bounds access.");

                std::memcpy(reinterpret_cast<uint8_t*>(dstMemory), ptr_ + offset, amount * sizeof(T));
            }

            // Simply sets offset to += amountOfBytes, doesn't check if it will overflow in release mode
            inline void skip(size_t amountOfBytes) noexcept
            {
                COFFEE_ASSERT(
                    std::numeric_limits<size_t>::max() - offset_ >= amountOfBytes && offset_ + amountOfBytes > size_,
                    "Invalid skip call: Overflow."
                );

                offset_ += amountOfBytes;
            }

            // Simply sets offset to -= amountOfBytes, doesn't check if it will overflow in release mode
            inline void reverse(size_t amountOfBytes) noexcept
            {
                COFFEE_ASSERT(amountOfBytes <= offset_, "Invalid reverse call: Underflow.");
                offset_ -= amountOfBytes;
            }

            inline bool eof() noexcept { return size_ == offset_; }

            inline size_t size() const noexcept { return size_; }

            inline size_t offset() const noexcept { return offset_; }

        private:
            const uint8_t* ptr_;
            size_t size_;
            bool isOwner_ = false;
            size_t offset_ = 0;
            uint8_t underlyingBuffer_[BufferSize] {};
        };

        constexpr inline size_t kReaderStreamSize = 16;
        using ReaderStream = ReadOnlyStream<kReaderStreamSize>;

        namespace fnv1a {

            constexpr uint64_t offsetBasis = 14695981039346656037ULL;
            constexpr uint64_t primeNumber = 1099511628211ULL;

            // Do not use this for regular hashing, only for constant time hashing, instead use XXH3 at runtime
            static constexpr uint64_t digest(const char* str)
            {
                uint64_t hash = offsetBasis;

                for (size_t i = 0; str[i] != '\0'; i++) {
                    hash = primeNumber * (hash ^ static_cast<unsigned char>(str[i]));
                }

                return hash;
            }

            // Do not use this for regular hashing, only for constant time hashing, instead use XXH3 at runtime
            static constexpr uint64_t digest(const std::string& str) { return digest(str.data()); }

        } // namespace fnv1a

    } // namespace utils

} // namespace coffee

#endif