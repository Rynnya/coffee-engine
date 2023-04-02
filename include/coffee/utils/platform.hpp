#ifndef COFFEE_UTILS_PLATFORM
#define COFFEE_UTILS_PLATFORM

#ifndef NOMINMAX
#    define NOMINMAX
#endif

#ifdef assert
#    undef assert
#endif

#ifdef NDEBUG
#    define COFFEE_RELEASE
#else
#    define COFFEE_DEBUG
#    define COFFEE_ASSERT_ENABLE
#endif

// https://stackoverflow.com/a/5920028
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#    define COFFEE_WINDOWS
#    if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
#        pragma comment(lib, "user32.lib")
#        pragma comment(lib, "gdi32.lib")
#        pragma comment(lib, "shell32.lib")
#    endif
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    ifdef _WIN64
// define something for Windows (64-bit only)
#    else
// define something for Windows (32-bit only)
#    endif
#elif __APPLE__
#    define COFFEE_APPLE
#    include <TargetConditionals.h>
#    if TARGET_IPHONE_SIMULATOR
// iOS, tvOS, or watchOS Simulator
#    elif TARGET_OS_MACCATALYST
// Mac's Catalyst (ports iOS API into Mac, like UIKit).
#    elif TARGET_OS_IPHONE
// iOS, tvOS, or watchOS device
#    elif TARGET_OS_MAC
// Other kinds of Apple platforms
#    else
#        error "Unknown Apple platform" // TODO: Make basic handling like unix system
#    endif
#elif __linux__
#    define COFFEE_LINUX
#elif __unix__ // all unices not caught above
#    define COFFEE_UNIX
#elif defined(_POSIX_VERSION)
#    define COFFEE_POSIX
#else
#    error "Unknown compiler"
#endif

#endif