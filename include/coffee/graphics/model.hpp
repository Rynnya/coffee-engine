#ifndef COFFEE_GRAPHICS_MODEL
#define COFFEE_GRAPHICS_MODEL

#include <coffee/graphics/mesh.hpp>

namespace coffee {

    namespace graphics {

        class Model : NonMoveable {
        public:
            Model(std::vector<Mesh>&& meshes, BufferPtr&& verticesBuffer, BufferPtr&& indicesBuffer);
            virtual ~Model() noexcept = default;

            const std::vector<Mesh> meshes;
            const BufferPtr verticesBuffer;
            const BufferPtr indicesBuffer;
        };

        using ModelPtr = std::shared_ptr<Model>;

    } // namespace graphics

} // namespace coffee

#endif