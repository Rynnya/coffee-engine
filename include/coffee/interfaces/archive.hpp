#ifndef COFFEE_INTERFACES_ARCHIVE
#define COFFEE_INTERFACES_ARCHIVE

#include <coffee/objects/texture.hpp>

#include <string>
#include <vector>

namespace coffee {

    // A coffee unique archive that contains texture or regular files
    // This archive utilizes zstd as compression algorithm and simply writes file after file, with single header

    class Archive {
    public:
        struct ArchiveEntry {
            std::string filename;
            size_t position;
            size_t compressedSize;
        };

        Archive();
        Archive(const std::string& filepath);
        ~Archive() noexcept;

        void openFile(const std::string& filepath);
        void closeFile() noexcept;
        bool isEmpty() const noexcept;
        const std::string& getPath() const noexcept;

        std::vector<uint8_t> loadRawFile(const std::string& filepath) const noexcept;

    private:
        struct PImpl;
        PImpl* pImpl_;
    };

}

#endif
