#include "CustomShader.h"

#include <exception>
#include <unordered_set>
#include <ranges>

#include "State.h"

std::string SpecialVariables::EditSource(const std::string& originalSource, std::string pixelGetter)
{
    static const auto replaceIdentifier = [](std::string& str, const std::string_view& from, const std::string_view& to) {
        size_t pos = 0;
        while ((pos = str.find(from, pos)) != std::string::npos) {
            static const auto isIdentifierChar = [](char c) { return std::isalnum(c) || c == '_'; };

            bool borderStart = pos == 0 || !isIdentifierChar(str[pos - 1]);
            bool borderEnd = pos + from.size() >= str.size() || !isIdentifierChar(str[pos + from.size()]);

            if (borderStart && borderEnd) {
                str.replace(pos, from.size(), to);
                pos += to.size();
            }
            else {
                pos += from.size();
            }
        }
    };

    replaceIdentifier(pixelGetter, "v_TexCoord", "texCoord");

    std::string source = R"glsl(
uniform float x_Time;
uniform vec2  x_WindowSize;
uniform vec2  x_CursorPos;
uniform float x_MonitorScale;

vec4 x_Texture(vec2 texCoord) {
    return )glsl" + pixelGetter + R"glsl(;
}

vec4 x_TextureOffset(vec2 pixelOffset) {
    return x_Texture(x_TexCoord - pixelOffset / x_WindowSize);
}
        )glsl" + originalSource;

    replaceIdentifier(source, "x_PixelPos", "(x_TexCoord * x_WindowSize)");

    // So we can provide API stability even if Hyprland changes its variable names
    replaceIdentifier(source, "x_Tex", "tex");
    replaceIdentifier(source, "x_TexCoord", "v_texcoord");

    return source;
}

void SpecialVariables::PrimeUniforms(const SP<CShader>& shader)
{
    UniformLocations[(uint8_t)Time] = glGetUniformLocation(shader->program(), "x_Time");
    UniformLocations[(uint8_t)WindowSize] = glGetUniformLocation(shader->program(), "x_WindowSize");
    UniformLocations[(uint8_t)CursorPos] = glGetUniformLocation(shader->program(), "x_CursorPos");
    UniformLocations[(uint8_t)MonitorScale] = glGetUniformLocation(shader->program(), "x_MonitorScale");
}

void SpecialVariables::SetUniforms(PHLMONITOR monitor, PHLWINDOW window)
{
    GLint loc;
    if (loc = UniformLocations[(size_t)Time]; loc != -1)
    {
        static auto startTime = Time::steadyNow();
        float seconds = std::chrono::duration_cast<std::chrono::duration<float>>(
            Time::steadyNow() - startTime
        ).count();
        glUniform1f(loc, seconds);
    }
    if (loc = UniformLocations[(size_t)WindowSize]; loc != -1)
    {
        auto size = window->m_realSize->value();
        glUniform2f(loc, size.x, size.y);
    }
    if (loc = UniformLocations[(size_t)CursorPos]; loc != -1)
    {
        Vector2D pos = g_pPointerManager->position() - window->m_realPosition->value();
        glUniform2f(loc, (float)pos.x, (float)pos.y);
    }
    if (loc = UniformLocations[(size_t)MonitorScale]; loc != -1)
        glUniform1f(loc, monitor->m_scale);
}


void ShaderVariant::PrimeUniforms(const Uniforms& args)
{
    Specials.PrimeUniforms(Shader);

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

    Specials.SetUniforms(monitor, window);

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

    std::string pixelGetter = source.substr(pixColorDef + 15, defEnd - (pixColorDef + 15));
    std::string customSource = SpecialVariables::EditSource(CustomSource, pixelGetter);

    size_t main = source.find("void main(");
    if (main == std::string::npos)
        throw g.Efmt("Frag source does not contain an occurence of 'void main('");

    source.insert(main, customSource + "\n\n");
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

    if (variant.Specials.UniformLocations[(size_t)SpecialVariables::Time] != -1)
        NeedsConstantDamage = true;

    if (variant.Specials.UniformLocations[(size_t)SpecialVariables::CursorPos] != -1)
        NeedsMouseMoveDamage = true;

    return variant;
}