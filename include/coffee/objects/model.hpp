#ifndef COFFEE_OBJECTS_MODEL
#define COFFEE_OBJECTS_MODEL

#include <coffee/objects/mesh.hpp>

namespace coffee {

    class ModelImpl
            : public Drawable
            , NonMoveable {
    public:
        ModelImpl(std::vector<Mesh>&& meshes);
        virtual ~ModelImpl() noexcept = default;

        const std::vector<Mesh> meshes;

        void draw(const GraphicsCommandBuffer& commandBuffer) override;
    };

    using Model = std::shared_ptr<ModelImpl>;

} // namespace coffee

#endif