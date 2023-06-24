#ifndef COFFEE_OBJECTS_AABB
#define COFFEE_OBJECTS_AABB

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace coffee {

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
        glm::vec4 min {};
        glm::vec4 max {};

        AABBPoints transform(const glm::mat4& modelMatrix) const noexcept
        {
            AABBPoints aabbPoints {};

            aabbPoints[0] = glm::vec4 { min.x, min.y, min.z, 1.0f };
            aabbPoints[1] = glm::vec4 { max.x, min.y, min.z, 1.0f };
            aabbPoints[2] = glm::vec4 { min.x, max.y, min.z, 1.0f };
            aabbPoints[3] = glm::vec4 { max.x, max.y, min.z, 1.0f };
            aabbPoints[4] = glm::vec4 { min.x, min.y, max.z, 1.0f };
            aabbPoints[5] = glm::vec4 { max.x, min.y, max.z, 1.0f };
            aabbPoints[6] = glm::vec4 { min.x, max.y, max.z, 1.0f };
            aabbPoints[7] = glm::vec4 { max.x, max.y, max.z, 1.0f };

            for (size_t i = 0; i < AABBPoints::kAmountOfPoints; i++) {
                aabbPoints[i] = modelMatrix * aabbPoints[i];
            }

            return aabbPoints;
        }
    };

} // namespace coffee

#endif
