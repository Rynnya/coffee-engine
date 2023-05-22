#include <coffee/utils/utils.hpp>

#include <coffee/utils/exceptions.hpp>
#include <coffee/utils/log.hpp>

#include <filesystem>
#include <fstream>

namespace coffee {

    std::vector<uint8_t> utils::readFile(const std::string& fileName)
    {
        std::error_code ec;
        uintmax_t fileSize = std::filesystem::file_size(fileName, ec);

        if (ec) {
            throw FilesystemException { FilesystemException::Type::ImplementationFailure,
                                        fmt::format("Failed to get file size, with the following reason: {}", ec.message()) };
        }

        std::ifstream file { fileName, std::ios::in | std::ios::binary };

        if (!file.is_open()) {
            throw FilesystemException { FilesystemException::Type::ImplementationFailure, "Failed to open file for reading!" };
        }

        std::vector<uint8_t> buffer {};
        buffer.resize(fileSize);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

        return buffer;
    }

} // namespace coffee