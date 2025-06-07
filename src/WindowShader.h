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
    void ReshadeWindows();
    void AddShader(std::string name, std::variant<std::string, std::string> idOrPath, std::string args);

    void OnRenderWindowPre(PHLWINDOW window);
    void OnRenderWindowPost();
    void OnWindowClose(PHLWINDOW window);

private:
    std::map<std::variant<std::string, std::string>, std::unique_ptr<ShaderHolder>> m_CompiledShaders;
    std::map<std::string, std::unique_ptr<ShaderConfig>> m_Shaders;

    std::map<PHLWINDOW, ShaderConfig*> m_ShadedWindows;
    std::map<PHLWINDOW, ShaderConfig*> m_ManuallyShadedWindows;

    std::optional<ShaderConfig*> m_ShadersSwapped;
};