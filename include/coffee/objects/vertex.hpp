#ifndef COFFEE_OBJECTS_VERTEX
#define COFFEE_OBJECTS_VERTEX

#include <coffee/types.hpp>

#include <glm/glm.hpp>

namespace coffee {

    class Vertex {
    public:
        glm::vec3 position {};
        glm::vec3 normal {};
        glm::vec2 texCoords {};
        glm::vec3 tangent {};

        static inline std::vector<coffee::InputElement> getElementDescriptions() {
            std::vector<InputElement> elements {};

            elements.push_back({ "", 0U, 0U, coffee::Format::R32G32B32SFloat, offsetof(Vertex, position) });
            elements.push_back({ "", 0U, 1U, coffee::Format::R32G32B32SFloat, offsetof(Vertex, normal) });
            elements.push_back({ "", 0U, 2U, coffee::Format::R32G32SFloat, offsetof(Vertex, texCoords) });
            elements.push_back({ "", 0U, 3U, coffee::Format::R32G32B32SFloat, offsetof(Vertex, tangent) });

            return elements;
        }

        bool operator==(const Vertex& other);
        bool operator!=(const Vertex& other);
    };

}

namespace std {

    template <>
    struct hash<coffee::Vertex> {
        size_t operator()(const coffee::Vertex& vertex) const;
    };

}

#endif
