#ifndef COFFEE_GRAPHICS_MESH
#define COFFEE_GRAPHICS_MESH

#include <coffee/graphics/submesh.hpp>

namespace coffee { namespace graphics {

    class Mesh : NonMoveable {
    public:
        Mesh(std::vector<SubMesh>&& subMeshes, BufferPtr&& verticesBuffer, BufferPtr&& indicesBuffer);
        ~Mesh() noexcept = default;

        const std::vector<SubMesh> subMeshes;
        const BufferPtr verticesBuffer;
        const BufferPtr indicesBuffer;
    };

    using MeshPtr = std::shared_ptr<Mesh>;

}} // namespace coffee::graphics

#endif