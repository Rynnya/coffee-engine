#ifndef COFFEE_INTERFACES_FILESYSTEM
#define COFFEE_INTERFACES_FILESYSTEM

#include <coffee/utils/non_moveable.hpp>
#include <coffee/utils/utils.hpp>

#include <mio/mio.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

// Stolen directly from ZSTD single-file implementation
typedef struct ZSTD_DCtx_s ZSTD_DCtx;
// Stolen directly fron XXH3 single-file implementation
typedef uint64_t XXH64_hash_t;

namespace coffee {

    class Filesystem;
    using FilesystemPtr = std::shared_ptr<Filesystem>;

    class Filesystem : NonMoveable {
    public:
        // Virtual filesystem support some internal types as mandatory
        // This required because of type checking inside and for better error handling
        enum class FileType : uint8_t {
            // Applied to any file that isn't listed here
            RawBytes = 0,
            // .spv
            Shader = 1,
            // .cfa
            Model = 2,
            // .img
            RawImage = 3,
            // .basis, .ktx2
            BasisImage = 4,
            // .wav, .wave
            WAV = 5,
            // .ogg
            OGG = 6,
        };

        struct Entry {
            FileType type = FileType::RawBytes;
            std::string filename {};
            // This means only filesystem ZSTD compression, which isn't always applied
            // Reason for this is because some other formats uses internal for them compression
            // Which will do all work for us already, and ZSTD will just waste runtime resources instead
            bool compressed = false;
            size_t uncompressedSize = 0;
            size_t compressedSize = 0;
        };

        Filesystem(const std::string& path);
        virtual ~Filesystem() noexcept = default;

        static FilesystemPtr create(const std::string& path);

        virtual bool contains(const std::string& path) const noexcept = 0;

        virtual Filesystem::Entry getMetadata(const std::string& path) const = 0;
        virtual std::vector<uint8_t> getContent(const std::string& path) const = 0;
        virtual utils::ReaderStream getStream(const std::string& path) const = 0;

        const std::string basePath;
    };

    class NativeFilesystem : public Filesystem {
    public:
        ~NativeFilesystem() noexcept = default;

        bool contains(const std::string& path) const noexcept override;

        Filesystem::Entry getMetadata(const std::string& path) const override;
        std::vector<uint8_t> getContent(const std::string& path) const override;
        utils::ReaderStream getStream(const std::string& path) const override;

    private:
        NativeFilesystem(const std::string& path);

        friend class Filesystem;
    };

    class VirtualFilesystem : public Filesystem {
    public:
        ~VirtualFilesystem() noexcept;

        bool contains(const std::string& path) const noexcept override;

        Filesystem::Entry getMetadata(const std::string& path) const override;
        std::vector<uint8_t> getContent(const std::string& path) const override;
        utils::ReaderStream getStream(const std::string& path) const override;

    private:
        struct InternalEntry {
            FileType fileType = FileType::RawBytes;
            uint8_t filepathSize = 0;
            char* filepath = nullptr;
            size_t uncompressedSize = 0;
            size_t compressedSize = 0;
            size_t position = 0;
            uint64_t hash = 0;
        };

        VirtualFilesystem(const std::string& path);

        mio::basic_mmap_source<uint8_t> archiveFile_ {};
        std::unordered_map<XXH64_hash_t, InternalEntry> entries_ {};

        friend class Filesystem;
    };

} // namespace coffee

#endif
