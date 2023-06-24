#ifndef COFFEE_OBJECTS_MODEL
#define COFFEE_OBJECTS_MODEL

#include <coffee/objects/mesh.hpp>

namespace coffee {

    class Model : NonMoveable {
    public:
        Model(std::vector<Mesh>&& meshes, graphics::BufferPtr&& verticesBuffer, graphics::BufferPtr&& indicesBuffer);
        virtual ~Model() noexcept = default;

        const std::vector<Mesh> meshes;
        const graphics::BufferPtr verticesBuffer;
        const graphics::BufferPtr indicesBuffer;
    };

    using ModelPtr = std::shared_ptr<Model>;

} // namespace coffee

#endif