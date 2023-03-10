#ifndef COFFEE_OBJECTS_VERTEX
#define COFFEE_OBJECTS_VERTEX

#include <coffee/types.hpp>

#include <glm/glm.hpp>

namespace coffee {

    class Vertex {
    public:
        glm::vec3 position {};
        glm::u16vec3 normal {};
        glm::uint32 texCoords {};
        glm::u16vec3 tangent {};

        static inline std::vector<coffee::InputElement> getElementDescriptions() {
            std::vector<InputElement> elements {};

            elements.push_back({ "Position", 0U, 0U, coffee::Format::R32G32B32SFloat, offsetof(Vertex, position) });
            elements.push_back({ "Normal", 0U, 1U, coffee::Format::R16G16B16SFloat, offsetof(Vertex, normal) });
            elements.push_back({ "UV", 0U, 2U, coffee::Format::R16G16SFloat, offsetof(Vertex, texCoords) });
            elements.push_back({ "Tangent", 0U, 3U, coffee::Format::R16G16B16SFloat, offsetof(Vertex, tangent) });

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
