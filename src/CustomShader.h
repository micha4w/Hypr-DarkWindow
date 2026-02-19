#pragma once

#include <string>
#include <map>

#include <hyprland/src/render/Shader.hpp>


enum IntroducesTransparency : bool { No = false, Yes = true };
using Uniforms = std::map<std::string, std::vector<float>>;


struct ShaderVariant
{
    SP<CShader> Shader;

    std::map<std::string, GLint> UniformLocations;
    GLint TimeLocation;

    void PrimeUniforms(const Uniforms& args);
    void SetUniforms(const Uniforms& args, PHLMONITOR monitor, PHLWINDOW window) noexcept;
};

struct CompiledShaders
{
    std::string CustomSource;
    std::map<uint8_t, ShaderVariant> FragVariants;
    bool FailedCompilation = false;

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