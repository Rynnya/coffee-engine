#include <coffee/interfaces/filesystem.hpp>

#include <coffee/interfaces/exceptions.hpp>
#include <coffee/utils/log.hpp>
#include <coffee/utils/math.hpp>
#include <coffee/utils/utils.hpp>

#include <fstream>

// This one must be defined here because ZSTD uses it internally
#include <xxh3/xxhash.h>
#include <zstd/zstddeclib.c>

namespace coffee {

    namespace detail {

        constexpr uint8_t filesystemMagic[4] = { 0xD2, 0x8A, 0x3C, 0xB7 };

        template <typename T>
        [[nodiscard]] inline T read(const mio::basic_mmap_source<uint8_t>& fileHandle, size_t& offset)
        {
            T result {};
            std::memcpy(&result, fileHandle.data() + offset, sizeof(result));

            if (!Math::isSystemLittleEndian()) {
                Math::byteSwap(result);
            }

            offset += sizeof(result);
            return result;
        }

        template <typename T>
        inline void readIntoBuffer(const mio::basic_mmap_source<uint8_t>& fileHandle, T* buffer, size_t size, size_t offset)
        {
            std::memcpy(buffer, fileHandle.data() + offset, size * sizeof(T));
        }

        Filesystem::FileType extensionToFileType(const std::string& extension)
        {
            switch (utils::fnv1a::digest(extension.data())) {
                case utils::fnv1a::digest(".spv"):
                    return Filesystem::FileType::Shader;
                case utils::fnv1a::digest(".cfa"):
                    return Filesystem::FileType::Model;
                case utils::fnv1a::digest(".img"):
                    return Filesystem::FileType::RawImage;
                case utils::fnv1a::digest(".basis"):
                case utils::fnv1a::digest(".ktx2"):
                    return Filesystem::FileType::BasisImage;
                case utils::fnv1a::digest(".wav"):
                case utils::fnv1a::digest(".wave"):
                    return Filesystem::FileType::WAV;
                case utils::fnv1a::digest(".ogg"):
                    return Filesystem::FileType::OGG;
            }

            return Filesystem::FileType::RawBytes;
        }

    } // namespace detail

    Filesystem::Filesystem(const std::string& path) : basePath { path } {};

    FilesystemPtr Filesystem::create(const std::string& path)
    {
        std::error_code ec {};
        std::filesystem::file_status status = std::filesystem::status(path, ec);

        if (ec) {
            throw FilesystemException {
                FilesystemException::Type::ImplementationFailure,
                fmt::format("Implementation failed to get status of path '{}' with following message: {}!", path, ec.message())
            };
        }

        switch (status.type()) {
            case std::filesystem::file_type::directory: {
                return std::shared_ptr<NativeFilesystem> { new NativeFilesystem { path } };
            }
            case std::filesystem::file_type::regular: {
                return std::shared_ptr<VirtualFilesystem> { new VirtualFilesystem { path } };
            }
            case std::filesystem::file_type::not_found: {
                throw FilesystemException { FilesystemException::Type::FileNotFound,
                                            fmt::format("File or directory doesn't exist in path '{}'!", path) };
            }
            default: {
                throw FilesystemException { FilesystemException::Type::InvalidFileType, "Filesystem can only open regular files and directories!" };
            }
        }
    }

    NativeFilesystem::NativeFilesystem(const std::string& path) : Filesystem { path } {}

    bool NativeFilesystem::contains(const std::string& path) const noexcept
    {
        std::error_code ec {};
        auto status = std::filesystem::status(path, ec);

        return !ec && status.type() == std::filesystem::file_type::regular;
    }

    Filesystem::Entry NativeFilesystem::getMetadata(const std::string& path) const
    {
        std::filesystem::path fullPath = std::filesystem::path(basePath) / path;

        std::error_code ec {};
        uintmax_t fileSize = std::filesystem::file_size(fullPath, ec);

        if (ec) {
            throw FilesystemException {
                FilesystemException::Type::ImplementationFailure,
                fmt::format("Implementation failed to get file size of file '{}' with following message: {}!", path, ec.message())
            };
        }

        Filesystem::Entry result {};
        result.type = detail::extensionToFileType(fullPath.extension().string());
        result.filename = fullPath.filename().string();
        result.compressed = false;
        result.uncompressedSize = fileSize;
        result.compressedSize = 0;

        return result;
    }

    std::vector<uint8_t> NativeFilesystem::getContent(const std::string& path) const
    {
        std::filesystem::path fullPath = std::filesystem::path(basePath) / path;

        return utils::readFile(fullPath.string());
    }

    utils::ReaderStream NativeFilesystem::getStream(const std::string& path) const
    {
        std::filesystem::path fullPath = std::filesystem::path(basePath) / path;
        uint8_t* pointer = nullptr;
        size_t size = utils::readFile(fullPath.string(), pointer);

        return { pointer, size, true };
    }

    VirtualFilesystem::VirtualFilesystem(const std::string& path) : Filesystem { path }
    {
        if (!std::filesystem::exists(path)) {
            throw FilesystemException { FilesystemException::Type::FileNotFound, fmt::format("Failed to open stream to archive '{}'!", path) };
        }

        std::error_code ec {};
        archiveFile_.map(path, ec);

        if (ec) {
            throw FilesystemException {
                FilesystemException::Type::ImplementationFailure,
                fmt::format("Implementation failed to create mapped region for '{}' with following message: {}!", path, ec.message())
            };
        }

        if (archiveFile_.size() <= 8) {
            throw FilesystemException { FilesystemException::Type::InvalidFilesystemSignature, "Provided filesystem doesn't have header!" };
        }

        if (std::memcmp(archiveFile_.data(), detail::filesystemMagic, sizeof(detail::filesystemMagic)) != 0) {
            throw FilesystemException { FilesystemException::Type::InvalidFilesystemSignature, "Invalid filesystem magic!" };
        }

        size_t offset = sizeof(detail::filesystemMagic);
        uint32_t amountOfFiles = detail::read<uint32_t>(archiveFile_, offset);

        for (uint32_t i = 0; i < amountOfFiles; i++) {
            InternalEntry entry {};

            static_assert(sizeof(entry.fileType) == sizeof(uint8_t), "Your compiler doesn't support enums with specific size.");
            entry.fileType = static_cast<FileType>(detail::read<std::underlying_type_t<FileType>>(archiveFile_, offset));

            entry.filepathSize = detail::read<uint8_t>(archiveFile_, offset);
            entry.filepath = new char[entry.filepathSize];

            detail::readIntoBuffer(archiveFile_, entry.filepath, entry.filepathSize, offset);
            offset += entry.filepathSize;

            entry.uncompressedSize = detail::read<size_t>(archiveFile_, offset);
            entry.compressedSize = detail::read<size_t>(archiveFile_, offset);
            entry.position = detail::read<size_t>(archiveFile_, offset);
            entry.hash = detail::read<uint64_t>(archiveFile_, offset);

            entries_.emplace(entry.hash, std::move(entry));
        }
    }

    VirtualFilesystem::~VirtualFilesystem() noexcept
    {
        for (auto& [_, entry] : entries_) {
            delete[] entry.filepath;
        }
    }

    bool VirtualFilesystem::contains(const std::string& path) const noexcept
    {
        return entries_.find(XXH3_64bits(path.data(), path.size())) != entries_.end();
    }

    Filesystem::Entry VirtualFilesystem::getMetadata(const std::string& path) const
    {
        auto it = entries_.find(XXH3_64bits(path.data(), path.size()));

        if (it == entries_.end()) {
            throw FilesystemException { FilesystemException::Type::FileNotFound, fmt::format("File '{}' doesn't exist!", path) };
        }

        Entry result {};
        auto& entry = it->second;

        result.type = entry.fileType;
        result.filename = { entry.filepath, entry.filepathSize };
        result.compressed = entry.compressedSize != 0;
        result.uncompressedSize = entry.uncompressedSize;
        result.compressedSize = entry.compressedSize;

        return result;
    }

    std::vector<uint8_t> VirtualFilesystem::getContent(const std::string& path) const
    {
        auto it = entries_.find(XXH3_64bits(path.data(), path.size()));

        if (it == entries_.end()) {
            throw FilesystemException { FilesystemException::Type::FileNotFound, fmt::format("File '{}' doesn't exist!", path) };
        }

        auto& entry = it->second;

        // Some files didn't have compression at all (or they have internal for this type compression)
        // In this case just read whole file into vector and return
        if (entry.compressedSize == 0) {
            std::vector<uint8_t> rawData {};

            rawData.resize(entry.uncompressedSize);
            detail::readIntoBuffer(archiveFile_, rawData.data(), rawData.size(), entry.position);

            return rawData;
        }

        // I don't trust vector::resize as it might allocate more than required
        // Using old C-style array on other hand will allocate exact amount of bytes (unless aligned)
        std::unique_ptr<uint8_t[]> compressedData = std::make_unique<uint8_t[]>(entry.compressedSize);
        detail::readIntoBuffer(archiveFile_, compressedData.get(), entry.compressedSize, entry.position);

        ZSTD_frameHeader frameHeader {};
        size_t errorCode = ZSTD_getFrameHeader(&frameHeader, compressedData.get(), entry.compressedSize);

        if (ZSTD_isError(errorCode)) {
            const char* description = ZSTD_getErrorName(errorCode);
            COFFEE_ERROR("ZSTD frame header returned error: {}!", description);

            throw FilesystemException { FilesystemException::Type::DecompressionFailure,
                                        fmt::format("ZSTD frame header returned error: {}!", description) };
        }

        size_t decompressedSize = frameHeader.frameContentSize;

        // Empty files allowed too, but not very useful
        if (decompressedSize == 0) {
            return {};
        }

        if (decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN) {
            throw FilesystemException { FilesystemException::Type::DecompressionFailure, "Failed to gather compressed size of frame!" };
        }

        std::vector<uint8_t> decompressedBytes {};
        decompressedBytes.resize(entry.uncompressedSize);

        errorCode = ZSTD_decompress(decompressedBytes.data(), decompressedBytes.size(), compressedData.get(), entry.compressedSize);

        if (ZSTD_isError(errorCode)) {
            const char* description = ZSTD_getErrorName(errorCode);
            COFFEE_ERROR("ZSTD decompression returned error: {}!", description);

            throw FilesystemException { FilesystemException::Type::DecompressionFailure,
                                        fmt::format("ZSTD decompression returned error: {}!", description) };
        }

        return decompressedBytes;
    }

    utils::ReaderStream VirtualFilesystem::getStream(const std::string& path) const
    {
        auto it = entries_.find(XXH3_64bits(path.data(), path.size()));

        if (it == entries_.end()) {
            throw FilesystemException { FilesystemException::Type::FileNotFound, fmt::format("File '{}' doesn't exist!", path) };
        }

        auto& entry = it->second;

        // Some files didn't have compression at all (or they have internal for this type compression)
        // In this case just return raw pointer into buffer
        if (entry.compressedSize == 0) {
            std::vector<uint8_t> rawData {};

            rawData.resize(entry.uncompressedSize);
            detail::readIntoBuffer(archiveFile_, rawData.data(), rawData.size(), entry.position);

            return { archiveFile_.data(), entry.uncompressedSize };
        }

        // Sadly, because interface must be identical for both Native and Virtual filesystems, we must handle compressed types too
        // Which literally destroys whole reason Streams for compressed files
        // But, calling this function for non-streamable file is literally pointless
        // And every streamable file is uncompressed by default

        // I don't trust vector::resize as it might allocate more than required
        // Using old C-style array on other hand will allocate exact amount of bytes (unless aligned)
        std::unique_ptr<uint8_t[]> compressedData = std::make_unique<uint8_t[]>(entry.compressedSize);
        detail::readIntoBuffer(archiveFile_, compressedData.get(), entry.compressedSize, entry.position);

        ZSTD_frameHeader frameHeader {};
        size_t errorCode = ZSTD_getFrameHeader(&frameHeader, compressedData.get(), entry.compressedSize);

        if (ZSTD_isError(errorCode)) {
            const char* description = ZSTD_getErrorName(errorCode);
            COFFEE_ERROR("ZSTD frame header returned error: {}!", description);

            throw FilesystemException { FilesystemException::Type::DecompressionFailure,
                                        fmt::format("ZSTD frame header returned error: {}!", description) };
        }

        size_t decompressedSize = frameHeader.frameContentSize;

        // Empty files allowed too, but not very useful
        if (decompressedSize == 0) {
            return { nullptr, 0, false };
        }

        if (decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN) {
            throw FilesystemException { FilesystemException::Type::DecompressionFailure, "Failed to gather compressed size of frame!" };
        }

        uint8_t* decompressedBytes = new uint8_t[entry.uncompressedSize];
        errorCode = ZSTD_decompress(decompressedBytes, entry.uncompressedSize, compressedData.get(), entry.compressedSize);

        if (ZSTD_isError(errorCode)) {
            const char* description = ZSTD_getErrorName(errorCode);
            COFFEE_ERROR("ZSTD decompression returned error: {}!", description);

            throw FilesystemException { FilesystemException::Type::DecompressionFailure,
                                        fmt::format("ZSTD decompression returned error: {}!", description) };
        }

        return { decompressedBytes, entry.uncompressedSize, true };
    }

} // namespace coffee