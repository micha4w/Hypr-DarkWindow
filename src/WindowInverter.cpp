#include "WindowInverter.h"

#include <hyprutils/string/String.hpp>


void WindowInverter::OnRenderWindowPre()
{
    bool shouldInvert =
        (std::find(m_InvertedWindows.begin(), m_InvertedWindows.end(), g_pHyprOpenGL->m_pCurrentWindow.lock())
            != m_InvertedWindows.end()) ^
        (std::find(m_ManuallyInvertedWindows.begin(), m_ManuallyInvertedWindows.end(), g_pHyprOpenGL->m_pCurrentWindow.lock())
            != m_ManuallyInvertedWindows.end());

    if (shouldInvert)
    {
        std::swap(m_Shaders.EXT, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shEXT);
        std::swap(m_Shaders.RGBA, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBA);
        std::swap(m_Shaders.RGBX, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBX);
        m_ShadersSwapped = true;
    }
}

void WindowInverter::OnRenderWindowPost()
{
    if (m_ShadersSwapped)
    {
        std::swap(m_Shaders.EXT, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shEXT);
        std::swap(m_Shaders.RGBA, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBA);
        std::swap(m_Shaders.RGBX, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBX);
        m_ShadersSwapped = false;
    }
}

void WindowInverter::OnWindowClose(PHLWINDOW window)
{
    auto windowIt = std::find(m_InvertedWindows.begin(), m_InvertedWindows.end(), window);
    if (windowIt != m_InvertedWindows.end())
    {
        std::swap(*windowIt, *(m_InvertedWindows.end() - 1));
        m_InvertedWindows.pop_back();
    }

    windowIt = std::find(m_ManuallyInvertedWindows.begin(), m_ManuallyInvertedWindows.end(), window);
    if (windowIt != m_ManuallyInvertedWindows.end())
    {
        std::swap(*windowIt, *(m_ManuallyInvertedWindows.end() - 1));
        m_ManuallyInvertedWindows.pop_back();
    }
}


void WindowInverter::SetRules(std::vector<SWindowRule>&& rules)
{
    m_InvertWindowRules = rules;
    Reload();
}


void WindowInverter::Init()
{
    m_Shaders.Init();
}

void WindowInverter::Unload()
{
    if (m_ShadersSwapped)
    {
        std::swap(m_Shaders.EXT, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shEXT);
        std::swap(m_Shaders.RGBA, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBA);
        std::swap(m_Shaders.RGBX, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBX);
        m_ShadersSwapped = false;
    }

    m_Shaders.Destroy();
}

void WindowInverter::InvertIfMatches(PHLWINDOW window)
{
    // for some reason, some events (currently `activeWindow`) sometimes pass a null pointer
    if (!window) return;

    std::string title = window->m_szTitle;
    std::string appidclass = window->m_szClass;

    bool shouldInvert = false;
    for (const auto& rule : m_InvertWindowRules)
    {
        try {
            if (!rule.szTag.empty() && !window->m_tags.isTagged(rule.szTag))
                continue;

            if (rule.szClass != "") {
                std::regex RULECHECK(rule.szClass);

                if (!std::regex_search(appidclass, RULECHECK))
                    continue;
            }

            if (rule.szTitle != "") {
                std::regex RULECHECK(rule.szTitle);

                if (!std::regex_search(title, RULECHECK))
                    continue;
            }

            if (rule.szInitialTitle != "") {
                std::regex RULECHECK(rule.szInitialTitle);

                if (!std::regex_search(window->m_szInitialTitle, RULECHECK))
                    continue;
            }

            if (rule.szInitialClass != "") {
                std::regex RULECHECK(rule.szInitialClass);

                if (!std::regex_search(window->m_szInitialClass, RULECHECK))
                    continue;
            }

            if (rule.bX11 != -1) {
                if (window->m_bIsX11 != rule.bX11)
                    continue;
            }

            if (rule.bFloating != -1) {
                if (window->m_bIsFloating != rule.bFloating)
                    continue;
            }

            if (rule.bFullscreen != -1) {
                if (window->m_bIsFullscreen != rule.bFullscreen)
                    continue;
            }

            if (rule.bPinned != -1) {
                if (window->m_bPinned != rule.bPinned)
                    continue;
            }

            if (rule.bFocus != -1) {
                if (rule.bFocus != (g_pCompositor->m_pLastWindow.lock() == window))
                    continue;
            }

            if (!rule.szOnWorkspace.empty()) {
                const auto PWORKSPACE = window->m_pWorkspace;
                if (!PWORKSPACE || !PWORKSPACE->matchesStaticSelector(rule.szOnWorkspace))
                    continue;
            }

            if (!rule.szWorkspace.empty()) {
                const auto PWORKSPACE = window->m_pWorkspace;

                if (!PWORKSPACE)
                    continue;

                if (rule.szWorkspace.starts_with("name:")) {
                    if (PWORKSPACE->m_szName != rule.szWorkspace.substr(5))
                        continue;
                }
                else {
                    // number
                    if (!Hyprutils::String::isNumber(rule.szWorkspace))
                        throw std::runtime_error("szWorkspace not name: or number");

                    const int64_t ID = std::stoll(rule.szWorkspace);

                    if (PWORKSPACE->m_iID != ID)
                        continue;
                }
            }
        }
        catch (std::exception& e) {
            Debug::log(ERR, "Regex error at {} ({})", rule.szValue, e.what());
            continue;
        }

        shouldInvert = true;
        break;
    }

    auto windowIt = std::find(m_InvertedWindows.begin(), m_InvertedWindows.end(), window);
    if (shouldInvert != (windowIt != m_InvertedWindows.end()))
    {
        if (shouldInvert)
            m_InvertedWindows.push_back(window);
        else
        {
            std::swap(*windowIt, *(m_InvertedWindows.end() - 1));
            m_InvertedWindows.pop_back();
        }

        g_pHyprRenderer->damageWindow(window);
    }
}


void WindowInverter::ToggleInvert(PHLWINDOW window)
{
    if (!window)
        return;

    auto windowIt = std::find(m_ManuallyInvertedWindows.begin(), m_ManuallyInvertedWindows.end(), window);
    if (windowIt == m_ManuallyInvertedWindows.end())
        m_ManuallyInvertedWindows.push_back(window);
    else
    {
        std::swap(*windowIt, *(m_ManuallyInvertedWindows.end() - 1));
        m_ManuallyInvertedWindows.pop_back();
    }

    g_pHyprRenderer->damageWindow(window);
}


void WindowInverter::Reload()
{
    m_InvertedWindows = {};

    for (const auto& window : g_pCompositor->m_vWindows)
        InvertIfMatches(window);
}
