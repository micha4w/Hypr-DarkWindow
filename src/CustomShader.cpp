#include "CustomShader.h"

#include <exception>
#include <unordered_set>
#include <ranges>

#include "State.h"


void ShaderVariant::PrimeUniforms(const Uniforms& args)
{
    TimeLocation = glGetUniformLocation(Shader->program(), "time");

    for (auto& [name, _] : args) {
        if (UniformLocations.contains(name))
            continue;

        GLint loc = glGetUniformLocation(Shader->program(), name.c_str());
        if (loc == -1)
            throw g.Efmt("Shader failed to find the uniform: {}", name);
        UniformLocations[name] = loc;
    }
}

void ShaderVariant::SetUniforms(const Uniforms& args, PHLMONITOR monitor, PHLWINDOW window) noexcept
{
    GLint prog;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);

    glUseProgram(Shader->program());

    if (TimeLocation != -1)
    {
        static auto startTime = std::chrono::steady_clock::now();
        float currentTime = std::chrono::duration_cast<std::chrono::duration<float>>(
            std::chrono::steady_clock::now() - startTime
        ).count();

        glUniform1f(TimeLocation, currentTime);
    }

    for (auto& [name, values] : args) {
        GLint loc = UniformLocations[name];
        switch (values.size()) {
        case 1: glUniform1f(loc, values[0]); break;
        case 2: glUniform2f(loc, values[0], values[1]); break;
        case 3: glUniform3f(loc, values[0], values[1], values[2]); break;
        case 4: glUniform4f(loc, values[0], values[1], values[2], values[3]); break;
        }
    }

    glUseProgram(prog);
}


CompiledShaders::~CompiledShaders()
{
    g_pHyprRenderer->makeEGLCurrent();
    FragVariants.clear();
    g_pHyprRenderer->unsetEGL();
}

std::string CompiledShaders::EditShader(const std::string& originalSource)
{
    std::string source = originalSource;

    size_t pixColorDef = source.find("vec4 pixColor =");
    if (pixColorDef == std::string::npos)
        throw g.Efmt("Frag source does not contain an occurence of 'vec4 pixColor ='");

    size_t defEnd = source.find(';', pixColorDef);
    if (defEnd == std::string::npos)
        throw g.Efmt("Frag source does not contain a ';' after 'vec4 pixColor ='");

    source.insert(defEnd + 1, "\n    windowShader(pixColor);");

    size_t main = source.find("void main(");
    if (main == std::string::npos)
        throw g.Efmt("Frag source does not contain an occurence of 'void main('");

    source.insert(main, CustomSource + "\n\n");
    return source;
}

void CompiledShaders::TestCompilation(const Uniforms& args)
{
    static const std::string testFragSource = R"glsl(
#version 300 es
precision highp float;

in vec2 v_texcoord;
uniform sampler2D tex;
uniform float alpha;

layout(location = 0) out vec4 fragColor;
void main() {
    vec4 pixColor = vec4(1.0);
    fragColor = pixColor;
}
    )glsl";

    std::string testFrag = EditShader(testFragSource);
    auto testShader = SP(new CShader());

    g_pHyprRenderer->makeEGLCurrent();
    Hyprutils::Utils::CScopeGuard _egl([&] { g_pHyprRenderer->unsetEGL(); });

    if (!testShader->createProgram(g_pHyprOpenGL->m_shaders->TEXVERTSRC, testFrag, true, true)) {
        Log::logger->log(Log::ERR, "Failed to compile this Shader: {}", testFrag);
        throw g.Efmt("Failed to compile Shader with Test Source, check logs for details");
    }

    ShaderVariant{ .Shader = testShader }.PrimeUniforms(args);
}

ShaderVariant& CompiledShaders::GetOrCreateVariant(uint8_t features, std::function<SP<CShader>()> create)
{
    auto compiled_it = FragVariants.find(features);
    if (compiled_it != FragVariants.end())
        return compiled_it->second;

    auto newShader = create();
    if (newShader->program() == 0)
        throw g.Efmt("Failed to compile shader variant, check logs for details");

    auto& variant = FragVariants[features] = ShaderVariant{ .Shader = newShader };

    for (auto* instance : Instances)
        variant.PrimeUniforms(instance->Args);

    return variant;
}