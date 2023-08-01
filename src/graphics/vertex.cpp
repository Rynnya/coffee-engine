#include <coffee/graphics/vertex.hpp>

namespace coffee {

    namespace graphics {

        bool Vertex::operator==(const Vertex& other)
        {
            return position == other.position && normal == other.normal && texCoords == other.texCoords && tangent == other.tangent;
        }

        bool Vertex::operator!=(const Vertex& other) { return !operator==(other); }

    } // namespace graphics

} // namespace coffee

namespace std {

    size_t hash<coffee::graphics::Vertex>::operator()(const coffee::graphics::Vertex& vertex) const
    {
        size_t positionSeed = 0;
        size_t normalSeed = 0;
        size_t texCoordsSeed = 0;
        size_t tangentSeed = 0;

        std::hash<float> hasher {};
        auto hashCombine = [](size_t& seed, size_t hash) {
            hash += 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= hash;
        };

        hashCombine(positionSeed, hasher(vertex.position.x));
        hashCombine(positionSeed, hasher(vertex.position.y));
        hashCombine(positionSeed, hasher(vertex.position.z));

        hashCombine(normalSeed, hasher(vertex.normal.x));
        hashCombine(normalSeed, hasher(vertex.normal.y));
        hashCombine(normalSeed, hasher(vertex.normal.z));

        hashCombine(texCoordsSeed, hasher(vertex.texCoords));

        hashCombine(tangentSeed, hasher(vertex.tangent.x));
        hashCombine(tangentSeed, hasher(vertex.tangent.y));
        hashCombine(tangentSeed, hasher(vertex.tangent.z));

        size_t finalSeed = positionSeed;
        finalSeed ^= (normalSeed << 1);
        finalSeed >>= 1;
        finalSeed ^= (texCoordsSeed << 1);
        finalSeed >>= 1;
        finalSeed ^= (tangentSeed << 1);
        finalSeed >>= 1;

        return finalSeed;
    }

} // namespace std