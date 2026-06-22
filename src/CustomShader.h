#pragma once

#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprland/src/render/Shader.hpp>
#include <map>
#include <string>

struct ShadedWindow;

enum IntroducesTransparency : bool
{
    No = false,
    Yes = true
};
using Uniforms = std::map<std::string, std::vector<float>>;

struct SpecialVariables
{
    enum SpecialUniforms : uint8_t
    {
        Time,
        FadeIn,
        FadeOut,
        CursorPos,
        WindowSize,
        MonitorScale,
        _Count
    };

    std::array<GLint, (size_t) SpecialUniforms::_Count> UniformLocations;

    static std::string EditSource(const std::string& originalSource, std::string pixelGetter);
    void PrimeUniforms(const SP<CShader>& shader);
    void SetUniforms(ShadedWindow& props, PHLMONITOR monitor, PHLWINDOW window);
};

struct ShaderVariant
{
    SP<CShader> Shader;

    std::map<std::string, GLint> UniformLocations;
    SpecialVariables Specials;

    void PrimeUniforms(const Uniforms& args);
    void SetUniforms(ShadedWindow& props, PHLMONITOR monitor, PHLWINDOW window) noexcept;
};

struct CompiledShaders
{
    std::string CustomSource;
    std::map<uint8_t, ShaderVariant> FragVariants;
    bool FailedCompilation = false;

    bool UsesTimeUniform = false;
    bool UsesMousePosUniform = false;

    std::vector<struct ShaderInstance*> Instances;

    ~CompiledShaders();

    std::string EditShader(const std::string& originalSource);
    void TestCompilation(const Uniforms& args);

    ShaderVariant& GetOrCreateVariant(uint8_t features, std::function<SP<CShader>()> create);
};

struct ShaderInstance
{
    std::string ID;

    SP<CompiledShaders> Compiled;
    Uniforms Args;

    IntroducesTransparency Transparency;
    float FadeInSpeed, FadeOutSpeed;
    float AnimationInterval;
};


struct ShadedWindow
{
    ShaderInstance* RuleShader = nullptr;
    ShaderInstance* DispatchShader = nullptr;

    ShaderInstance* ConfiguredShader = nullptr;  // the shader that the User wants
    ShaderInstance* FadingOutShader = nullptr;   // the shader that is fading out, if any
    ShaderInstance* ActiveShader = nullptr;      // one of ConfiguredShader or FadingOutShader

    Time::steady_tp StartTime;
    Time::steady_tp FadeStartTime;
    enum
    {
        FadeIn,
        None,
        FadeOut,
    } FadeState;
};