#include <coffee/utils/log.hpp>

#include <fmt/chrono.h>

#ifdef COFFEE_WINDOWS
#   include <Windows.h>
#   include <debugapi.h>
#endif

#include <iomanip>
#include <iostream>

namespace coffee {

    namespace detail {

        static std::chrono::system_clock::time_point startupTime = std::chrono::system_clock::now();

        constexpr const char* getFileName(const char* path) {
            for (auto ptr = path; *ptr; ++ptr) {
                if (*ptr == '/' || *ptr == '\\') {
                    path = ptr + 1;
                }
            }

            return path;
        }

        constexpr std::string_view toString(MessageSeverity severity) {
            switch (severity) {
                case MessageSeverity::Info:
                    return "INFO";
                case MessageSeverity::Warning:
                    return "WARNING";
                case MessageSeverity::Error:
                    return "ERROR";
                default: 
                    return "UNKNOWN";
            }
        }

        std::string escape(const char* str) {
            std::string ret {};

            for (auto ptr = str; *ptr; ++ptr) {
                switch (*ptr) {
                    case '{':
                        ret += "{{";
                        break;
                    case '}':
                        ret += "}}";
                        break;
                    default:
                        ret += *ptr;
                        break;
                }
            }

            return ret;
        }

    }

    void mustBe(bool exprResult, const char* exprString, const char* filePath, const unsigned line, const std::string& message) {
        if (exprResult) {
            return;
        }

        fmt::print("ASSERTION FAILED: {} IN FILE {}:{}\n", detail::escape(exprString), detail::getFileName(filePath), line);
        fmt::print(" => {}", detail::escape(message.data()));
        ::abort();
    }

    void log(MessageSeverity severity, const char* filePath, const unsigned line, const std::string& message) {
        const auto now = std::chrono::system_clock::now();
        const auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - detail::startupTime).count();
        const auto formatted = fmt::format(
            "{:%Y-%m-%d %H:%M:%S} [{:>8d}s] | {:>24}:{:<4d} | {}: {}\n", 
            fmt::localtime(now), diff, detail::getFileName(filePath), line, detail::toString(severity), detail::escape(message.data())
        );

#       ifdef COFFEE_WINDOWS
            OutputDebugStringA(formatted.c_str());
#       endif

        fmt::print(formatted);
    }

}