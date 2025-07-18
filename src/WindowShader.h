#pragma once

#include <vector>

#include "Helpers.h"

#include <hyprland/src/Compositor.hpp>


class WindowShader
{
public:
    void Unload();

    void ShadeIfMatches(PHLWINDOW window);
    void ToggleShade(PHLWINDOW window, const std::string& shader);
    void ForgetWindow(PHLWINDOW window);
    void ReshadeWindows();

    void AddPredefinedShader(const std::string& name);
    ShaderConfig* AddShader(ShaderDefinition def);
    ShaderConfig* EnsureShader(const std::string& shader);

    void OnRenderWindowPre(PHLWINDOW window);
    void OnRenderWindowPost();

private:
    std::map<std::string, UP<ShaderConfig>> m_Shaders;

    std::map<PHLWINDOW, ShaderConfig*> m_RuleShadedWindows;
    std::map<PHLWINDOW, ShaderConfig*> m_DispatchShadedWindows;

    std::optional<ShaderConfig*> m_ShadersSwapped;
};