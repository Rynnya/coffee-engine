#ifndef COFFEE_INTERFACES_EXCEPTIONS
#define COFFEE_INTERFACES_EXCEPTIONS

#include <stdexcept>
#include <string>

namespace coffee {

    class FilesystemException : public std::runtime_error {
    public:
        const enum class Type {
            ImplementationFailure = 0,
            FileNotFound = 1,
            InvalidFileType = 2,
            InvalidFilesystemSignature = 3,
            BadFilesystemAccess = 4,
            DecompressionFailure = 5
        } type;

        inline FilesystemException(Type type, const std::string& message) noexcept : std::runtime_error { message }, type { type } {}
    };

    class AssetException : public std::runtime_error {
    public:
        const enum class Type { ImplementationFailure = 0, TypeMismatch = 1, NotInCache = 2, InvalidFilesystem = 3, InvalidRequest = 4 } type;

        inline AssetException(Type type, const std::string& message) noexcept : std::runtime_error { message }, type { type } {}
    };

} // namespace coffee

#endif
