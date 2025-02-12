#pragma once

#include <vector>

#include "Helpers.h"

#include <hyprland/src/Compositor.hpp>


class WindowInverter
{
public:
    void Init();
    void Unload();

    void InvertIfMatches(PHLWINDOW window);
    void ToggleInvert(PHLWINDOW window);
    void Reload();

    void OnRenderWindowPre(PHLWINDOW window);
    void OnRenderWindowPost();
    void OnWindowClose(PHLWINDOW window);

private:
    std::vector<CWindowRule> m_InvertWindowRules;
    std::vector<PHLWINDOW> m_InvertedWindows;
    std::vector<PHLWINDOW> m_ManuallyInvertedWindows;

    ShaderHolder m_Shaders;
    bool m_ShadersSwapped = false;
};