#ifndef COFFEE_UTILS_LOG
#define COFFEE_UTILS_LOG

#include <coffee/utils/platform.hpp>

#include <fmt/format.h>

namespace coffee {

    enum class MessageSeverity : uint32_t { Info = 1 << 0, Warning = 1 << 1, Error = 1 << 2, Critical = 1 << 3 };

    void mustBe(bool exprResult, const char* exprString, const char* filePath, const unsigned line, const std::string& message);
    void log(MessageSeverity severity, const char* filePath, const unsigned line, const std::string& message);

} // namespace coffee

// clang-format off

#ifdef COFFEE_ASSERT_ENABLE
#   define COFFEE_ASSERT(expr, formatting, ...) \
        coffee::mustBe(expr, #expr, __FILE__, __LINE__, fmt::format(formatting, ##__VA_ARGS__))
#else
#   define COFFEE_ASSERT(expr, fmt, ...) \
        static_cast<void>(nullptr)
#endif

#ifdef COFFEE_DEBUG
#   define COFFEE_INFO(formatting, ...) \
        coffee::log(coffee::MessageSeverity::Info, __FILE__, __LINE__, fmt::format(formatting, ##__VA_ARGS__))
#   define COFFEE_WARNING(formatting, ...) \
        coffee::log(coffee::MessageSeverity::Warning, __FILE__, __LINE__, fmt::format(formatting, ##__VA_ARGS__))
#   define COFFEE_ERROR(formatting, ...) \
        coffee::log(coffee::MessageSeverity::Error, __FILE__, __LINE__, fmt::format(formatting, ##__VA_ARGS__))
#   define COFFEE_FATAL(formatting, ...) \
        coffee::log(coffee::MessageSeverity::Critical, __FILE__, __LINE__, fmt::format(formatting, ##__VA_ARGS__))
#else
#   define COFFEE_INFO(fmt, ...) \
        static_cast<void>(nullptr)
#   define COFFEE_WARNING(fmt, ...) \
        static_cast<void>(nullptr)
#   define COFFEE_ERROR(fmt, ...) \
        static_cast<void>(nullptr)
#   define COFFEE_FATAL(fmt, ...) \
        static_cast<void>(nullptr)
#endif

// clang-format on

#endif