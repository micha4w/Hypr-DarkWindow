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
        // (Would break unordered map, but those aren't in use anyway..)
        uint8_t c[sizeof(SShader)];
        std::memcpy(&c, &a, sizeof(SShader));
        std::memcpy(&a, &b, sizeof(SShader));
        std::memcpy(&b, &c, sizeof(SShader));
    }
}

struct ShaderHolder
{
    SShader CM;
    SShader RGBA;
    SShader RGBX;
    SShader EXT;

    void Init();
    void Destroy();
};
