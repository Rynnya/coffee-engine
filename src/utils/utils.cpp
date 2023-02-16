#include <coffee/utils/utils.hpp>

#include <coffee/utils/log.hpp>

#include <fstream>

namespace coffee {

    std::vector<uint8_t> Utils::readFile(const std::string& fileName) {
        std::ifstream file { fileName, std::ios::ate | std::ios::binary };
        COFFEE_THROW_IF(!file.is_open(), "Failed to open file: '{}'!", fileName);

        size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<uint8_t> buffer {};
        buffer.resize(fileSize);

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
        file.close();

        return buffer;
    }

}