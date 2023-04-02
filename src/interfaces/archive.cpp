#include <coffee/interfaces/archive.hpp>

#include <coffee/utils/log.hpp>

#include <filesystem>
#include <fstream>
#include <zstddeclib.c>

namespace coffee {

    constexpr uint8_t archiveMagic[4] = { 0xD2, 0x8A, 0x3C, 0xB7 };

    struct Archive::PImpl {
        PImpl() : decompressorContext { ZSTD_createDCtx() } {}

        ~PImpl() {
            ZSTD_freeDCtx(decompressorContext);
        }

        void openFile(const std::string& filepath) {
            std::ifstream newInputStream { filepath, std::ios::binary | std::ios::in };
            COFFEE_THROW_IF(!newInputStream.is_open(), "Failed to open stream to archive '{}'!", filepath);

            char fileMagic[4] {};
            newInputStream.read(fileMagic, 4);

            COFFEE_THROW_IF(std::memcmp(fileMagic, archiveMagic, 4) != 0, "Invalid archive magic!");

            inputStream = std::move(newInputStream);
            files.clear();

            uint32_t amountOfFiles = 0;
            inputStream.read(reinterpret_cast<char*>(&amountOfFiles), sizeof(amountOfFiles));

            for (uint32_t i = 0; i < amountOfFiles; i++) {
                ArchiveEntry entry {};

                uint8_t filenameSize {};
                inputStream.read(reinterpret_cast<char*>(&filenameSize), sizeof(filenameSize));

                entry.filename.resize(filenameSize);
                inputStream.read(entry.filename.data(), filenameSize);

                inputStream.read(reinterpret_cast<char*>(&entry.position), sizeof(entry.position));
                inputStream.read(reinterpret_cast<char*>(&entry.compressedSize), sizeof(entry.compressedSize));

                files.push_back(std::move(entry));
            }

            std::filesystem::path absolute = std::filesystem::absolute(filepath);
            pathToArchive = absolute.string();
        }

        void closeFile() noexcept {
            inputStream.close();
            pathToArchive.clear();
            files.clear();
        }

        std::ifstream inputStream;
        std::string pathToArchive;
        std::vector<ArchiveEntry> files;
        ZSTD_DCtx* decompressorContext;
    };

    Archive::Archive() : pImpl_ { new Archive::PImpl() } {}

    Archive::Archive(const std::string& filepath) : pImpl_ { new Archive::PImpl() } {
        pImpl_->openFile(filepath);
    }

    Archive::~Archive() noexcept {
        delete pImpl_;
    }

    void Archive::openFile(const std::string& filepath) {
        pImpl_->openFile(filepath);
    }

    void Archive::closeFile() noexcept {
        pImpl_->closeFile();
    }

    bool Archive::isEmpty() const noexcept {
        return pImpl_->files.size() == 0 || !(pImpl_->inputStream.is_open() && pImpl_->inputStream.good());
    }

    const std::string& Archive::getPath() const noexcept {
        return pImpl_->pathToArchive;
    }

    std::vector<uint8_t> Archive::loadRawFile(const std::string& filepath) const noexcept {
        if (!pImpl_->inputStream.is_open()) {
            return {};
        }

        auto it = std::find_if(pImpl_->files.begin(), pImpl_->files.end(), [&filepath](const ArchiveEntry& entry) {
            return filepath == entry.filename;
        });

        if (it == pImpl_->files.end()) {
            return {};
        }

        std::unique_ptr<uint8_t[]> compressedData = std::make_unique<uint8_t[]>(it->compressedSize);
        pImpl_->inputStream.seekg(it->position);
        pImpl_->inputStream.read(reinterpret_cast<char*>(compressedData.get()), it->compressedSize);
        size_t decompressedSize = ZSTD_getFrameContentSize(compressedData.get(), it->compressedSize);

        if (decompressedSize == 0 || decompressedSize == ZSTD_CONTENTSIZE_ERROR || decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN) {
            return {};
        }

        std::vector<uint8_t> decompressedBytes {};
        decompressedBytes.resize(decompressedSize);
        size_t errorCode = ZSTD_decompressDCtx(
            pImpl_->decompressorContext,
            decompressedBytes.data(),
            decompressedBytes.size(),
            compressedData.get(),
            it->compressedSize
        );

        if (ZSTD_getErrorCode(errorCode) != ZSTD_error_no_error) {
            return {};
        }

        return decompressedBytes;
    }

} // namespace coffee