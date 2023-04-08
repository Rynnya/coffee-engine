#ifndef COFFEE_ABSTRACT_CURSOR
#define COFFEE_ABSTRACT_CURSOR

#include <memory>

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

    class CursorImpl {
    public:
        CursorImpl(void* nativeHandle, CursorType type);
        ~CursorImpl() noexcept;

        CursorType getType() const noexcept;

    private:
        void* nativeHandle_ = nullptr;
        CursorType type_ = CursorType::Arrow;

        friend class WindowImpl;
    };

    using Cursor = std::shared_ptr<CursorImpl>;

} // namespace coffee

#endif