#include <coffee/graphics/cursor.hpp>

#include <coffee/utils/log.hpp>

#include <GLFW/glfw3.h>

namespace coffee {

namespace graphics {

    int cursorTypeToGLFWtype(CursorType type)
    {
        switch (type) {
            default:
            case CursorType::Arrow:
                return GLFW_ARROW_CURSOR;
            case CursorType::TextInput:
                return GLFW_IBEAM_CURSOR;
            case CursorType::CrossHair:
                return GLFW_CROSSHAIR_CURSOR;
            case CursorType::Hand:
                return GLFW_HAND_CURSOR;
            case CursorType::ResizeEW:
                return GLFW_RESIZE_EW_CURSOR;
            case CursorType::ResizeNS:
                return GLFW_RESIZE_NS_CURSOR;
            case CursorType::ResizeNWSE:
                return GLFW_RESIZE_NWSE_CURSOR;
            case CursorType::ResizeNESW:
                return GLFW_RESIZE_NESW_CURSOR;
            case CursorType::ResizeAll:
                return GLFW_RESIZE_ALL_CURSOR;
            case CursorType::NotAllowed:
                return GLFW_NOT_ALLOWED_CURSOR;
        }
    }

    Cursor::Cursor(GLFWcursor* cursorHandle, CursorType type) : type { type }, cursor_ { cursorHandle }
    {
        COFFEE_ASSERT(cursor_ != nullptr, "Invalid cursor handle provided.");
    }

    Cursor::~Cursor() noexcept { glfwDestroyCursor(cursor_); }

    CursorPtr Cursor::create(CursorType type)
    {
        if (GLFWcursor* cursor = glfwCreateStandardCursor(cursorTypeToGLFWtype(type))) {
            return std::shared_ptr<Cursor>(new Cursor { cursor, type });
        }

        return nullptr;
    }

    CursorPtr Cursor::create(const std::vector<uint8_t>& rawImage, uint32_t width, uint32_t height, CursorType type)
    {
        constexpr size_t bytesPerPixel = 4;

        if (rawImage.size() < bytesPerPixel * width * height) {
            return nullptr;
        }

        GLFWimage image {};
        image.width = width;
        image.height = height;
        image.pixels = const_cast<uint8_t*>(rawImage.data()); // GLFW promises that they won't be modifying this data

        if (GLFWcursor* cursor = glfwCreateCursor(&image, 0, 0)) {
            return std::shared_ptr<Cursor>(new Cursor { cursor, type });
        }

        return nullptr;
    }

} // namespace graphics

} // namespace coffee