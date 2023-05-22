#ifndef COFFEE_ABSTRACT_CURSOR
#define COFFEE_ABSTRACT_CURSOR

#include <coffee/types.hpp>

#include <volk/volk.h>
#include <GLFW/glfw3.h>

#include <vector>

namespace coffee {

    enum class CursorType {
        Arrow = 0,
        TextInput = 1,
        CrossHair = 2,
        Hand = 3,
        ResizeEW = 4,
        ResizeNS = 5,
        ResizeNWSE = 6,
        ResizeNESW = 7,
        ResizeAll = 8,
        NotAllowed = 9
    };

    class Cursor;
    using CursorPtr = std::shared_ptr<Cursor>;

    class Cursor {
    public:
        ~Cursor() noexcept;

        static CursorPtr create(CursorType type);
        static CursorPtr create(const std::vector<uint8_t>& rawImage, uint32_t width, uint32_t height, CursorType type);

        const CursorType type = CursorType::Arrow;

    private:
        Cursor(GLFWcursor* cursorHandle, CursorType type);

        GLFWcursor* cursor_ = nullptr;

        friend class Window;
    };

} // namespace coffee

#endif