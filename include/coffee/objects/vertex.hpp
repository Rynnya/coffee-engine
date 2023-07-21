#ifndef COFFEE_OBJECTS_VERTEX
#define COFFEE_OBJECTS_VERTEX

#include <coffee/graphics/graphics_pipeline.hpp>

#include <glm/glm.hpp>
#include <volk/volk.h>

#include <vector>

namespace coffee {

    class Vertex {
    public:
        glm::vec3 position {};
        glm::u16vec3 normal {};
        glm::uint32 texCoords {};
        glm::u16vec3 tangent {};

        static inline std::vector<graphics::InputElement> getElementDescriptions()
        {
            std::vector<graphics::InputElement> elements {};

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
