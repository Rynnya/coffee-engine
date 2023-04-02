#include <coffee/abstract/cursor.hpp>

#include <coffee/utils/log.hpp>

#include <GLFW/glfw3.h>

namespace coffee {

    CursorImpl::CursorImpl(void* nativeHandle, CursorType type) : nativeHandle_ { nativeHandle }, type_ { type } {
        COFFEE_ASSERT(nativeHandle_ != nullptr, "Invalid cursor handle provided.");
    }

    CursorImpl::~CursorImpl() noexcept {
        glfwDestroyCursor(reinterpret_cast<GLFWcursor*>(nativeHandle_));
    }

    CursorType CursorImpl::getType() const noexcept {
        return type_;
    }

} // namespace coffee