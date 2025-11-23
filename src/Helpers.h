#pragma once

#include <string>
#include <cstring>

#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>


template <typename... Args>
inline auto efmt(std::format_string<Args...> fmt, Args&&... args)
{
    return std::runtime_error(std::format(fmt, std::forward<Args>(args)...));
}

inline void notifyError(HANDLE handle, const std::string& err)
{
    std::string msg = std::string("[Hypr-DarkWindow] ") + err;
    Debug::log(ERR, msg);
    HyprlandAPI::addNotification(
        handle,
        msg,
        CHyprColor(0xFFFF0000),
        25'000
    );
}

inline auto findFunction(HANDLE handle, const std::string& className, const std::string& name)
{
    auto all = HyprlandAPI::findFunctionsByName(handle, name);
    auto found = std::find_if(all.begin(), all.end(), [&](const SFunctionMatch& line) {
        return line.demangled.starts_with(className + "::" + name);
    });
    return found != all.end() ? std::optional(*found) : std::optional<SFunctionMatch>();
};

namespace std
{
    inline void swap(SShader& a, SShader& b)
    {
        // memcpy because speed!
        uint8_t c[sizeof(SShader)];
        std::memcpy(&c, &a, sizeof(SShader));
        std::memcpy(&a, &b, sizeof(SShader));
        std::memcpy(&b, &c, sizeof(SShader));
    }
}

using Uniforms = std::map<std::string, std::vector<float>>;
struct ShaderDefinition
{
    ShaderDefinition(std::string id, std::string from, std::string path, const std::string& args, bool transparency);

    std::string ID;
    std::string From;
    std::string Path;
    Uniforms Args;
    bool Transparency;

    Uniforms ParseArgs(const std::string& args);
};

struct ShaderHolder
{
    std::map<std::string, std::array<GLint, 4>> UniformLocations;

    SShader CM;
    SShader RGBA;
    SShader RGBX;
    SShader EXT;

    ShaderHolder(const std::string& source);
    ~ShaderHolder();

    void PrimeUniforms(const Uniforms& args);
    void ApplyArgs(const Uniforms& args) noexcept;
};

enum class IntroducesTransparency { No = 0, Yes};

struct ShaderConfig {
    std::string ID;

    SP<ShaderHolder> CompiledShaders;
    Uniforms Args;
    IntroducesTransparency Transparent;
};