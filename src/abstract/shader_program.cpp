#include <coffee/abstract/shader_program.hpp>

#include <coffee/utils/log.hpp>

namespace coffee {

    ShaderProgram::ShaderProgram(ShaderProgram&& other) noexcept
        : shaders_ { std::exchange(other.shaders_, {}) }
    {}

    ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        shaders_ = std::exchange(other.shaders_, {});

        return *this;
    }

    ShaderProgram& ShaderProgram::addVertexShader(Shader&& vertexShader) {
        shaders_.emplace(ShaderStage::Vertex, std::move(vertexShader));
        return *this;
    }

    ShaderProgram& ShaderProgram::addFragmentShader(Shader&& fragmentShader) {
        shaders_.emplace(ShaderStage::Fragment, std::move(fragmentShader));
        return *this;
    }

}