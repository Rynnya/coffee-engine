#ifndef COFFEE_OBJECTS_MESH
#define COFFEE_OBJECTS_MESH

#include <coffee/interfaces/drawable.hpp>
#include <coffee/objects/materials.hpp>

namespace coffee {

    class MeshImpl : public Drawable, NonMoveable {
    public:
        MeshImpl(Buffer&& verticesBuffer, Buffer&& indicesBuffer, Materials&& materials);
        virtual ~MeshImpl() noexcept = default;

        Materials materials;

        void draw(const CommandBuffer& commandBuffer) override;

    private:
        Buffer verticesBuffer_;
        Buffer indicesBuffer_;
    };

    using Mesh = std::unique_ptr<MeshImpl>;

}

#endif
