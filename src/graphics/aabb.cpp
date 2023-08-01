#include <coffee/graphics/aabb.hpp>

namespace coffee {

    namespace graphics {

        AABBPoints AABB::transform(const glm::mat4& modelMatrix) const noexcept
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

    } // namespace graphics

} // namespace coffee