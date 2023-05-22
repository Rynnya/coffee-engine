#ifndef COFFEE_OBJECTS_MODEL
#define COFFEE_OBJECTS_MODEL

#include <coffee/objects/mesh.hpp>

namespace coffee {

    class Model
        : public Drawable
        , NonMoveable {
    public:
        Model(std::vector<Mesh>&& meshes, BufferPtr&& verticesBuffer, BufferPtr&& indicesBuffer);
        virtual ~Model() noexcept = default;

        void bind(const CommandBuffer& commandBuffer) const override;
        void draw(const CommandBuffer& commandBuffer) const override;

        const std::vector<Mesh> meshes;

    private:
        BufferPtr verticesBuffer_;
        BufferPtr indicesBuffer_;
    };

    using ModelPtr = std::shared_ptr<Model>;

} // namespace coffee

#endif