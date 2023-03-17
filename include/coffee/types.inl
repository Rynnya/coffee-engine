// DO NOT INCLUDE THIS FILE DIRECTLY
// Instead include types.hpp, it has a header guard and also a lot more than this file

namespace coffee {

    constexpr ShaderStage operator|(ShaderStage this_, ShaderStage other_) {
        return static_cast<ShaderStage>(static_cast<uint32_t>(this_) | static_cast<uint32_t>(other_));
    }

    constexpr ShaderStage operator&(ShaderStage this_, ShaderStage other_) {
        return static_cast<ShaderStage>(static_cast<uint32_t>(this_) & static_cast<uint32_t>(other_));
    }

    constexpr bool operator>(ShaderStage this_, ShaderStage other_) {
        return static_cast<uint32_t>(this_) > static_cast<uint32_t>(other_);
    }

    constexpr bool operator<(ShaderStage this_, ShaderStage other_) {
        return static_cast<uint32_t>(this_) < static_cast<uint32_t>(other_);
    }

    constexpr BufferUsage operator|(BufferUsage this_, BufferUsage other_) {
        return static_cast<BufferUsage>(static_cast<uint32_t>(this_) | static_cast<uint32_t>(other_));
    }

    constexpr BufferUsage operator&(BufferUsage this_, BufferUsage other_) {
        return static_cast<BufferUsage>(static_cast<uint32_t>(this_) & static_cast<uint32_t>(other_));
    }

    constexpr MemoryProperty operator|(MemoryProperty this_, MemoryProperty other_) {
        return static_cast<MemoryProperty>(static_cast<uint32_t>(this_) | static_cast<uint32_t>(other_));
    }

    constexpr MemoryProperty operator&(MemoryProperty this_, MemoryProperty other_) {
        return static_cast<MemoryProperty>(static_cast<uint32_t>(this_) & static_cast<uint32_t>(other_));
    }

    constexpr ImageUsage operator|(ImageUsage this_, ImageUsage other_) {
        return static_cast<ImageUsage>(static_cast<uint32_t>(this_) | static_cast<uint32_t>(other_));
    }

    constexpr ImageUsage operator&(ImageUsage this_, ImageUsage other_) {
        return static_cast<ImageUsage>(static_cast<uint32_t>(this_) & static_cast<uint32_t>(other_));
    }

    constexpr ImageAspect operator|(ImageAspect this_, ImageAspect other_) {
        return static_cast<ImageAspect>(static_cast<uint32_t>(this_) | static_cast<uint32_t>(other_));
    }

    constexpr ImageAspect operator&(ImageAspect this_, ImageAspect other_) {
        return static_cast<ImageAspect>(static_cast<uint32_t>(this_) & static_cast<uint32_t>(other_));
    }

    constexpr ColorComponent operator|(ColorComponent this_, ColorComponent other_) {
        return static_cast<ColorComponent>(static_cast<uint32_t>(this_) | static_cast<uint32_t>(other_));
    }

    constexpr ColorComponent operator&(ColorComponent this_, ColorComponent other_) {
        return static_cast<ColorComponent>(static_cast<uint32_t>(this_) & static_cast<uint32_t>(other_));
    }

    constexpr ResourceState operator|(ResourceState this_, ResourceState other_) {
        return static_cast<ResourceState>(static_cast<uint32_t>(this_) | static_cast<uint32_t>(other_));
    }

    constexpr ResourceState operator&(ResourceState this_, ResourceState other_) {
        return static_cast<ResourceState>(static_cast<uint32_t>(this_) & static_cast<uint32_t>(other_));
    }

    constexpr Offset2D Offset2D::operator+(const Offset2D& other) {
        return { this->x + other.x, this->y + other.y };
    }

    constexpr Offset2D Offset2D::operator-(const Offset2D& other) {
        return { this->x - other.x, this->y - other.y };
    }

    constexpr Offset2D& Offset2D::operator+=(const Offset2D& other) {
        this->x += other.x;
        this->y += other.y;

        return *this;
    }

    constexpr Offset2D& Offset2D::operator-=(const Offset2D& other) {
        this->x -= other.x;
        this->y -= other.y;

        return *this;
    }

    constexpr Extent2D Extent2D::operator+(const Extent2D& other) const noexcept {
        return { this->width + other.width, this->height + other.height };
    }

    constexpr Extent2D Extent2D::operator-(const Extent2D& other) const noexcept {
        return { this->width - other.width, this->height - other.height };
    }

    constexpr Extent2D Extent2D::operator+(const Offset2D& other) const noexcept {
        return { this->width + other.x, this->height + other.y };
    }

    constexpr Extent2D Extent2D::operator-(const Offset2D& other) const noexcept {
        return { this->width - other.x, this->height - other.y };
    }

    constexpr Extent2D Extent2D::operator+(const Float2D& other) const noexcept {
        return { this->width + static_cast<uint32_t>(other.x), this->height + static_cast<uint32_t>(other.y) };
    }

    constexpr Extent2D Extent2D::operator-(const Float2D& other) const noexcept {
        return { this->width - static_cast<uint32_t>(other.x), this->height - static_cast<uint32_t>(other.y) };
    }

    constexpr Extent2D& Extent2D::operator+=(const Extent2D& other) noexcept {
        this->width += other.width;
        this->height += other.height;

        return *this;
    }

    constexpr Extent2D& Extent2D::operator-=(const Extent2D& other) noexcept {
        this->width -= other.width;
        this->height -= other.height;

        return *this;
    }

    constexpr Extent2D& Extent2D::operator+=(const Offset2D& other) noexcept {
        this->width += other.x;
        this->height += other.y;

        return *this;
    }

    constexpr Extent2D& Extent2D::operator-=(const Offset2D& other) noexcept {
        this->width -= other.x;
        this->height -= other.y;

        return *this;
    }

    constexpr Extent2D& Extent2D::operator+=(const Float2D& other) noexcept {
        this->width += static_cast<uint32_t>(other.x);
        this->height += static_cast<uint32_t>(other.y);

        return *this;
    }

    constexpr Extent2D& Extent2D::operator-=(const Float2D& other) noexcept {
        this->width -= static_cast<uint32_t>(other.x);
        this->height -= static_cast<uint32_t>(other.y);

        return *this;
    }

    constexpr Extent2D::operator Float2D() const noexcept {
        return Float2D { static_cast<float>(this->width), static_cast<float>(this->height) };
    }

    constexpr Extent2D::operator Extent3D() const noexcept {
        return Extent3D { this->width, this->height, 1U };
    }

    constexpr Float2D Float2D::operator+(const Float2D& other) const noexcept {
        return { this->x + other.x, this->y + other.y };
    }

    constexpr Float2D Float2D::operator-(const Float2D& other) const noexcept {
        return { this->x - other.x, this->y - other.y };
    }

    constexpr Float2D Float2D::operator+(const Offset2D& other) const noexcept {
        return { this->x + other.x, this->y + other.y };
    }

    constexpr Float2D Float2D::operator-(const Offset2D& other) const noexcept {
        return { this->x - other.x, this->y - other.y };
    }

    constexpr Float2D Float2D::operator+(const Extent2D& other) const noexcept {
        return { this->x + other.width, this->y + other.height };
    }

    constexpr Float2D Float2D::operator-(const Extent2D& other) const noexcept {
        return { this->x - other.width, this->y - other.height };
    }

    constexpr Float2D& Float2D::operator+=(const Float2D& other) noexcept {
        this->x += other.x;
        this->y += other.y;

        return *this;
    }

    constexpr Float2D& Float2D::operator-=(const Float2D& other) noexcept {
        this->x -= other.x;
        this->y -= other.y;

        return *this;
    }

    constexpr Float2D& Float2D::operator+=(const Offset2D& other) noexcept {
        this->x += other.x;
        this->y += other.y;

        return *this;
    }

    constexpr Float2D& Float2D::operator-=(const Offset2D& other) noexcept {
        this->x -= other.x;
        this->y -= other.y;

        return *this;
    }

    constexpr Float2D& Float2D::operator+=(const Extent2D& other) noexcept {
        this->x += other.width;
        this->y += other.height;

        return *this;
    }

    constexpr Float2D& Float2D::operator-=(const Extent2D& other) noexcept {
        this->x -= other.width;
        this->y -= other.height;

        return *this;
    }

    constexpr Float2D::operator Extent2D() const noexcept {
        return Extent2D{ static_cast<uint32_t>(this->x), static_cast<uint32_t>(this->y) };
    }

}
