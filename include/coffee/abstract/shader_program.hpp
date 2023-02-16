#ifndef COFFEE_ABSTRACT_SHADER_PROGRAM
#define COFFEE_ABSTRACT_SHADER_PROGRAM

#include <coffee/abstract/shader.hpp>
#include <coffee/utils/non_copyable.hpp>

#include <map>

namespace coffee {

    class ShaderProgram : NonCopyable {
    public:
        ShaderProgram() noexcept = default;
        ~ShaderProgram() noexcept = default;

        ShaderProgram(ShaderProgram&&) noexcept;
        ShaderProgram& operator=(ShaderProgram&&) noexcept;

        ShaderProgram& addVertexShader(Shader&& vertexShader);
        ShaderProgram& addFragmentShader(Shader&& fragmentShader);

    private:
        std::map<ShaderStage, Shader, std::less<ShaderStage>> shaders_ {};

        friend class AbstractPipeline;
    };

}

#endif