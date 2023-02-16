#ifndef COFFEE_UTILS_MAIN_UTILS
#define COFFEE_UTILS_MAIN_UTILS

#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace coffee {

    class Utils {
    public:
        // Opens file and reads it entirely into std::vector<char>
        // Can throw std::runtime_error if file failed to open
        static std::vector<uint8_t> readFile(const std::string& fileName);

        // From: https://stackoverflow.com/a/57595105
        template <typename T, typename... Rest>
        static void hashCombine(std::size_t& seed, const T& v, const Rest&... rest) {
            seed ^= std::hash<T> {} (v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            (hashCombine(seed, rest), ...);
        };

    };

}

#endif