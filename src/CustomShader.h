#pragma once

#include <string>
#include <map>

#include <hyprland/src/render/Shader.hpp>


enum IntroducesTransparency : bool { No = false, Yes = true };
using Uniforms = std::map<std::string, std::vector<float>>;

struct ShaderDefinition
{
    std::string ID;

    // At least one of these must be set
    std::string From;
    std::string Path;
    std::string Source;

    Uniforms Args;
    IntroducesTransparency Transparency;

    static Uniforms ParseArgs(const std::string& args);
};

struct CompiledShaders
{
    struct CustomShader
    {
        std::map<std::string, GLint> UniformLocations;
        SP<CShader> Shader;
        bool FailedUniforms = false;

        void PrimeUniforms(const Uniforms& args);
        void SetUniforms(const Uniforms& args) noexcept;
    };

    std::string CustomSource;
    std::map<uint8_t, CustomShader> FragVariants;
    bool FailedCompilation = false;

    ~CompiledShaders();

    std::string EditShader(const std::string& originalSource);
    void TestCompilation(const Uniforms& args);
};

struct ShaderInstance
{
    std::string ID;

    SP<CompiledShaders> Compiled;
    Uniforms Args;
    IntroducesTransparency Transparency;
};