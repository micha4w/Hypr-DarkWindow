#include "CustomShader.h"

#include <exception>
#include <ranges>
#include <unordered_set>

#include "State.h"

std::string SpecialVariables::EditSource(const std::string& originalSource, std::string pixelGetter)
{
    static const auto replaceIdentifier = [](std::string& str, const std::string_view& from, const std::string_view& to)
    {
        size_t pos = 0;
        while ((pos = str.find(from, pos)) != std::string::npos)
        {
            static const auto isIdentifierChar = [](char c) { return std::isalnum(c) || c == '_'; };

            bool borderStart = pos == 0 || !isIdentifierChar(str[pos - 1]);
            bool borderEnd = pos + from.size() >= str.size() || !isIdentifierChar(str[pos + from.size()]);

            if (borderStart && borderEnd)
            {
                str.replace(pos, from.size(), to);
                pos += to.size();
            }
            else
            {
                pos += from.size();
            }
        }
    };

    replaceIdentifier(pixelGetter, "v_TexCoord", "texCoord");

    std::string source = R"glsl(
uniform float x_Time;
uniform float x_FadeIn;
uniform float x_FadeOut;
uniform vec2  x_CursorPos;
uniform vec2  x_WindowSize;
uniform float x_MonitorScale;

vec4 x_Texture(vec2 texCoord) {
    return )glsl" + pixelGetter +
                         R"glsl(;
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
    UniformLocations[(uint8_t) Time] = glGetUniformLocation(shader->program(), "x_Time");
    UniformLocations[(uint8_t) FadeIn] = glGetUniformLocation(shader->program(), "x_FadeIn");
    UniformLocations[(uint8_t) FadeOut] = glGetUniformLocation(shader->program(), "x_FadeOut");
    UniformLocations[(uint8_t) CursorPos] = glGetUniformLocation(shader->program(), "x_CursorPos");
    UniformLocations[(uint8_t) WindowSize] = glGetUniformLocation(shader->program(), "x_WindowSize");
    UniformLocations[(uint8_t) MonitorScale] = glGetUniformLocation(shader->program(), "x_MonitorScale");
}

void SpecialVariables::SetUniforms(ShadedElement& config, const UniformVariables& vars)
{
    GLint loc;
    if (loc = UniformLocations[(size_t) Time]; loc != -1)
    {
        float secondsPassed =
            std::chrono::duration_cast<std::chrono::milliseconds>(g.RenderState.Time - config.StartTime).count() / 1000.f;

        glUniform1f(loc, secondsPassed);
    }
    if (loc = UniformLocations[(size_t) FadeIn]; loc != -1)
    {
        float progress = 1.f;
        if (config.FadeState == ShadedElement::FadeIn)
        {
            auto msPassed =
                std::chrono::duration_cast<std::chrono::milliseconds>(g.RenderState.Time - config.FadeStartTime).count();

            progress = msPassed / (config.ActiveShader->FadeInSpeed * 100.f);
        }

        glUniform1f(loc, progress);
    }
    if (loc = UniformLocations[(size_t) FadeOut]; loc != -1)
    {
        float progress = 0.f;
        if (config.FadeState == ShadedElement::FadeOut)
        {
            auto ms_passed =
                std::chrono::duration_cast<std::chrono::milliseconds>(g.RenderState.Time - config.FadeStartTime).count();

            progress = ms_passed / (config.ActiveShader->FadeOutSpeed * 100.f);
        }

        glUniform1f(loc, progress);
    }

    if (loc = UniformLocations[(size_t) WindowSize]; loc != -1)
        glUniform2f(loc, vars.WindowSize.x, vars.WindowSize.y);

    if (loc = UniformLocations[(size_t) CursorPos]; loc != -1)
    {
        Vector2D pos = Pointer::mgr()->position() - vars.WindowPos;
        glUniform2f(loc, (float) pos.x, (float) pos.y);
    }
    if (loc = UniformLocations[(size_t) MonitorScale]; loc != -1)
        glUniform1f(loc, vars.MonitorScale);
}


void ShaderVariant::PrimeUniforms(const Uniforms& args)
{
    Specials.PrimeUniforms(Shader);

    for (auto& [name, _] : args)
    {
        if (UniformLocations.contains(name))
            continue;

        GLint loc = glGetUniformLocation(Shader->program(), name.c_str());
        if (loc == -1)
            throw g.Efmt("Shader failed to find the uniform: '{}'", name);
        UniformLocations[name] = loc;
    }
}

void ShaderVariant::SetUniforms(ShadedElement& config, const UniformVariables& vars) noexcept
{
    GLint prog;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);

    glUseProgram(Shader->program());

    Specials.SetUniforms(config, vars);

    for (auto& [name, values] : config.ActiveShader->Args)
    {
        GLint loc = UniformLocations[name];
        switch (values.size())
        {
        case 1:
            glUniform1f(loc, values[0]);
            break;
        case 2:
            glUniform2f(loc, values[0], values[1]);
            break;
        case 3:
            glUniform3f(loc, values[0], values[1], values[2]);
            break;
        case 4:
            glUniform4f(loc, values[0], values[1], values[2], values[3]);
            break;
        }
    }

    glUseProgram(prog);
}


CompiledShaders::~CompiledShaders()
{
    Render::GL::g_pHyprOpenGL->makeEGLCurrent();
    FragVariants.clear();
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
    // Simple dummy shader for a simple test
    vec4 pixColor = vec4(1.0);
    fragColor = pixColor;
}
    )glsl";

    std::string testFrag = EditShader(testFragSource);
    auto testShader = SP(new CShader());

    Render::GL::g_pHyprOpenGL->makeEGLCurrent();

    try
    {
        if (!testShader->createProgram(Render::GL::g_pHyprOpenGL->m_shaders->TEXVERTSRC, testFrag, true, true))
        {
            Log::logger->log(Log::ERR, "[Hypr-DarkWindow] Failed to compile this Shader: {}", testFrag);
            throw g.Efmt("Failed to compile Shader with Test Source, check logs for details");
        }

        ShaderVariant{ .Shader = testShader }.PrimeUniforms(args);
    }
    catch (...)
    {
        FailedCompilation = true;
        throw;
    }
}

ShaderVariant& CompiledShaders::GetOrCreateVariant(uint8_t features, std::function<SP<CShader>()> create)
{
    try
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

        if (variant.Specials.UniformLocations[(size_t) SpecialVariables::Time] != -1)
            UsesTimeUniform = true;

        if (variant.Specials.UniformLocations[(size_t) SpecialVariables::CursorPos] != -1)
            UsesMousePosUniform = true;

        return variant;
    }
    catch (...)
    {
        FailedCompilation = true;
        throw;
    }
}


void ShadedElement::Changed()
{
    ShaderInstance* prevConfigured = ConfiguredShader;
    ConfiguredShader = nullptr;

    if (RuleShader && DispatchShader)
    {
        if (RuleShader->ID != DispatchShader->ID)
            ConfiguredShader = DispatchShader;
    }
    else if (DispatchShader)
        ConfiguredShader = DispatchShader;
    else if (RuleShader)
        ConfiguredShader = RuleShader;

    if (prevConfigured == ConfiguredShader)
    {
        ActiveShader = FadingOutShader ? FadingOutShader : ConfiguredShader;
        return;
    }

    auto prevStartTime = FadeStartTime;
    auto prevFadeState = FadeState;
    if (!FadingOutShader)
    {
        auto now = Time::steadyNow();
        StartTime = now;
        FadeStartTime = now;
        FadeState = ShadedElement::None;

        if (prevConfigured && prevConfigured->FadeOutSpeed > 0.f)
        {
            FadingOutShader = prevConfigured;
            FadeState = ShadedElement::FadeOut;

            if (prevFadeState == ShadedElement::FadeIn)
            {
                auto ms_passed = std::chrono::duration_cast<std::chrono::milliseconds>(now - prevStartTime).count();
                auto progress = 1.f - ms_passed / (FadingOutShader->FadeInSpeed * 100.f);
                if (progress < 0.f)
                    progress = 0.f;

                FadeStartTime = now - std::chrono::milliseconds((long) (progress * FadingOutShader->FadeOutSpeed * 100.f));
            }
        }
        else if (ConfiguredShader && ConfiguredShader->FadeInSpeed > 0.f)
            FadeState = ShadedElement::FadeIn;
    }
    else if (FadingOutShader == ConfiguredShader)
    {
        auto now = Time::steadyNow();
        FadingOutShader = nullptr;
        FadeStartTime = now;
        FadeState = ShadedElement::None;

        if (prevFadeState == ShadedElement::FadeOut && ConfiguredShader->FadeInSpeed > 0.f)
        {
            auto ms_passed = std::chrono::duration_cast<std::chrono::milliseconds>(now - prevStartTime).count();
            auto progress = 1.f - ms_passed / (ConfiguredShader->FadeOutSpeed * 100.f);
            if (progress < 0.f)
                progress = 0.f;

            FadeStartTime = now - std::chrono::milliseconds((long) (progress * ConfiguredShader->FadeInSpeed * 100.f));
            FadeState = ShadedElement::FadeIn;
        }
    }

    ActiveShader = FadingOutShader ? FadingOutShader : ConfiguredShader;
}