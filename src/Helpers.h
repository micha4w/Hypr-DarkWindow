#pragma once

#include <string>
#include <cstring>

#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>


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

std::map<std::string, std::vector<float>> parseArgs(const std::string& args);

struct ShaderHolder
{
    std::variant<std::string, std::string> NameOrPath;
    std::map<std::string, std::array<GLint, 4>> UniformLocations;

    SShader CM;
    SShader RGBA;
    SShader RGBX;
    SShader EXT;

    ShaderHolder(std::variant<std::string, std::string> nameOrPath);
    ~ShaderHolder();
    ShaderHolder(const ShaderHolder&) = delete;

    void LoadArgs(std::map<std::string, std::vector<float>> args);
    void ApplyArgs(std::map<std::string, std::vector<float>> args) noexcept;
};


struct ShaderConfig {
    std::string ID;

    ShaderHolder* CompiledShaders;
    std::map<std::string, std::vector<float>> Args;
};