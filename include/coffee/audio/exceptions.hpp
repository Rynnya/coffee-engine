#ifndef COFFEE_AUDIO_EXCEPTIONS
#define COFFEE_AUDIO_EXCEPTIONS

#include <stdexcept>
#include <string>

namespace coffee {

    class AudioException : public std::runtime_error {
    public:
        const enum class Type { DeviceCreationFailure = 0, ContextFailure = 1, OutOfMemory = 2, EmptySource = 3 } type;

        inline AudioException(Type type, const std::string& message) noexcept : std::runtime_error { message }, type { type } {}
    };

} // namespace coffee

#endif
