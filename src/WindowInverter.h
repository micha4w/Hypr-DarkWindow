#pragma once

#include <vector>
#include <hyprland/src/Compositor.hpp>

#include "Helpers.h"


class WindowInverter
{
public:
    void Init();
    void Unload();

    void InvertIfMatches(CWindow* window);
    void ToggleInvert(CWindow* window);
    void SetRules(std::vector<SWindowRule>&& rules);
    void Reload();

    void OnRenderWindowPre();
    void OnRenderWindowPost();
    void OnWindowClose(CWindow* window);

private:
    std::vector<SWindowRule> m_InvertWindowRules;
    std::vector<CWindow*> m_InvertedWindows;
    std::vector<CWindow*> m_ManuallyInvertedWindows;

    ShaderHolder m_Shaders;
    bool m_ShadersSwapped = false;
};