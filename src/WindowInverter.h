#pragma once

#include <vector>
#include <re2/re2.h>
#include <hyprland/src/Compositor.hpp>

#include "Helpers.h"


class WindowInverter
{
public:
    void Init(HANDLE pluginHandle);
    void Unload();

    void InvertIfMatches(PHLWINDOW window);
    void ToggleInvert(PHLWINDOW window);
    void SoftToggle(bool invert);
    void Reload();

    void OnRenderWindowPre();
    void OnRenderWindowPost();
    void OnWindowClose(PHLWINDOW window);

    void NoIgnoreDecorations()
    {
        m_IgnoreDecorations = {};
    }

private:
    HANDLE m_PluginHandle;

    std::vector<CWindowRule> m_InvertWindowRules;
    std::vector<PHLWINDOW> m_InvertedWindows;
    std::vector<PHLWINDOW> m_ManuallyInvertedWindows;

    std::optional<bool> m_IgnoreDecorations = true;
    bool m_DecorationsWrapped = false;   

    ShaderHolder m_Shaders;
    bool m_ShadersSwapped = false;
};