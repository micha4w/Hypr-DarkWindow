#pragma once

#include <string>
#include <map>

#include <hyprland/src/render/Shader.hpp>


enum IntroducesTransparency : bool { No = false, Yes = true };
using Uniforms = std::map<std::string, std::vector<float>>;

struct SpecialVariables {
    enum SpecialUniforms : uint8_t
    {
        Time,
        WindowSize,
        CursorPos,
        MonitorScale,
        _Count
    };

    std::array<GLint, (size_t)SpecialUniforms::_Count> UniformLocations;

    static std::string EditSource(const std::string& originalSource, std::string pixelGetter);
    void PrimeUniforms(const SP<CShader>& shader);
    void SetUniforms(PHLMONITOR monitor, PHLWINDOW window);
};

struct ShaderVariant
{
    SP<CShader> Shader;

    std::map<std::string, GLint> UniformLocations;
    SpecialVariables Specials;

    void PrimeUniforms(const Uniforms& args);
    void SetUniforms(const Uniforms& args, PHLMONITOR monitor, PHLWINDOW window) noexcept;
};

struct CompiledShaders
{
    std::string CustomSource;
    std::map<uint8_t, ShaderVariant> FragVariants;
    bool FailedCompilation = false;

    bool NeedsConstantDamage = false;
    bool NeedsMouseMoveDamage = false;

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
};