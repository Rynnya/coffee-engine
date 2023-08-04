#ifndef COFFEE_GRAPHICS_AABB
#define COFFEE_GRAPHICS_AABB

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace coffee {

namespace graphics {

    class AABBPoints {
    public:
        static constexpr size_t kAmountOfPoints = 8;

        inline glm::vec4& operator[](size_t index) noexcept { return points[index]; }

        inline const glm::vec4& operator[](size_t index) const noexcept { return points[index]; }

        // w component is always 1.0f
        glm::vec4 points[kAmountOfPoints] {};
    };

    class AABB {
    public:
        AABB() noexcept = default;

        AABB(const AABB&) noexcept = default;
        AABB& operator=(const AABB&) noexcept = default;
        AABB(AABB&&) noexcept = default;
        AABB& operator=(AABB&&) noexcept = default;

        AABBPoints transform(const glm::mat4& meshMatrix) const noexcept;

        glm::vec4 min {};
        glm::vec4 max {};
    };

} // namespace graphics

} // namespace coffee

#endif
