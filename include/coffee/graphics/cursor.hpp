#ifndef COFFEE_GRAPHICS_CURSOR
#define COFFEE_GRAPHICS_CURSOR

#include <coffee/types.hpp>

// Required this order because implementation uses glfwCreateWindowSurface which not defined without volk ;-;

// clang-format off
#include <volk/volk.h>
#include <GLFW/glfw3.h>
// clang-format on

#include <vector>

namespace coffee { namespace graphics {

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

}} // namespace coffee::graphics

#endif