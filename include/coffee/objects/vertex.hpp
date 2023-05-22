#ifndef COFFEE_OBJECTS_VERTEX
#define COFFEE_OBJECTS_VERTEX

#include <coffee/graphics/pipeline.hpp>

#include <glm/glm.hpp>
#include <volk/volk.h>

#include <vector>

namespace coffee {

    // A default vertex implementation for Coffee Engine
    // You can replace this with your own implementation

    class Vertex {
    public:
        glm::vec3 position {};
        glm::u16vec3 normal {};
        glm::uint32 texCoords {};
        glm::u16vec3 tangent {};

        static inline std::vector<InputElement> getElementDescriptions()
        {
            std::vector<InputElement> elements {};

            elements.push_back({ 0U, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position) });
            elements.push_back({ 1U, VK_FORMAT_R16G16B16_SNORM, offsetof(Vertex, normal) });
            elements.push_back({ 2U, VK_FORMAT_R16G16_SFLOAT, offsetof(Vertex, texCoords) });
            elements.push_back({ 3U, VK_FORMAT_R16G16B16_SNORM, offsetof(Vertex, tangent) });

            return elements;
        }

        bool operator==(const Vertex& other);
        bool operator!=(const Vertex& other);
    };

} // namespace coffee

namespace std {

    template <>
    struct hash<coffee::Vertex> {
        size_t operator()(const coffee::Vertex& vertex) const;
    };

} // namespace std

#endif
