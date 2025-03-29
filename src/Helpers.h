#pragma once

#include <string>
#include <cstring>

#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>


namespace std
{
    inline void swap(CShader& a, CShader& b)
    {
        // memcpy because speed!
        // (Would break unordered map, but those aren't in use anyway..)
        uint8_t c[sizeof(CShader)];
        std::memcpy(&c, &a, sizeof(CShader));
        std::memcpy(&a, &b, sizeof(CShader));
        std::memcpy(&b, &c, sizeof(CShader));
    }
}

struct ShaderHolder
{
    CShader CM;
    CShader RGBA;
    CShader RGBX;
    CShader EXT;

    void Init();
    void Destroy();
};
