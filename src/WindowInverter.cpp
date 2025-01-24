#include "WindowInverter.h"

#include <hyprutils/string/String.hpp>

#include "DecorationsWrapper.h"


void WindowInverter::OnRenderWindowPre()
{
    auto window = g_pHyprOpenGL->m_RenderData.currentWindow.lock();
    bool shouldInvert =
        (std::find(m_InvertedWindows.begin(), m_InvertedWindows.end(), window)
            != m_InvertedWindows.end()) ^
        (std::find(m_ManuallyInvertedWindows.begin(), m_ManuallyInvertedWindows.end(), window)
            != m_ManuallyInvertedWindows.end());

    if (shouldInvert)
    {
        if (m_IgnoreDecorations && *m_IgnoreDecorations)
        {
            for (auto& decoration : window->m_dWindowDecorations)
            {
                // Debug::log(LOG, "ADD: Window {:p}, Decoration {:p}", (void*)window.get(), (void*)decoration.get());
                decoration = std::move(UP<IHyprWindowDecoration>(
                    new DecorationsWrapper(*this, std::move(decoration), window)
                ));
            }
            m_DecorationsWrapped = true;
        }

        std::swap(m_Shaders.EXT, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shEXT);
        std::swap(m_Shaders.RGBA, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBA);
        std::swap(m_Shaders.RGBX, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBX);
        m_ShadersSwapped = true;

        SoftToggle(true);
    }
}

void WindowInverter::OnRenderWindowPost()
{
    if (m_ShadersSwapped)
    {
        if (m_DecorationsWrapped)
        {
            for (auto& decoration : g_pHyprOpenGL->m_RenderData.currentWindow.lock()->m_dWindowDecorations)
            {
                // Debug::log(LOG, "REMOVE: Window {:p}, Decoration {:p}", (void*)g_pHyprOpenGL->m_pCurrentWindow.get(), (void*)decoration.get());
                if (DecorationsWrapper* wrapper = dynamic_cast<DecorationsWrapper*>(decoration.get()))
                    decoration = std::move(wrapper->take());
            }
            m_DecorationsWrapped = false;
        }

        std::swap(m_Shaders.EXT, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shEXT);
        std::swap(m_Shaders.RGBA, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBA);
        std::swap(m_Shaders.RGBX, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBX);
        m_ShadersSwapped = false;
    }
}

void WindowInverter::OnWindowClose(PHLWINDOW window)
{
    static const auto remove = [](std::vector<PHLWINDOW>& windows, PHLWINDOW& window) {
        auto windowIt = std::find(windows.begin(), windows.end(), window);
        if (windowIt != windows.end())
        {
            std::swap(*windowIt, *(windows.end() - 1));
            windows.pop_back();
        }
    };

    remove(m_InvertedWindows, window);
    remove(m_ManuallyInvertedWindows, window);
}

void WindowInverter::Init(HANDLE pluginHandle)
{
    m_Shaders.Init();

    m_PluginHandle = pluginHandle;
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

    std::vector<SP<CWindowRule>> rules = g_pConfigManager->getMatchingRules(window);
    bool shouldInvert = std::any_of(rules.begin(), rules.end(), [](const SP<CWindowRule>& rule) {
        return rule->szRule == "plugin:invertwindow";
    });

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

void WindowInverter::SoftToggle(bool invert)
{
    if (m_ShadersSwapped)
    {
        const auto toggleInvert = [&](GLuint prog, GLint location) {
            glUseProgram(prog);
            glUniform1i(location, invert ? 1 : 0);
        };

        toggleInvert(g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shEXT.program, m_Shaders.EXT_Invert);
        toggleInvert(g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBA.program, m_Shaders.RGBA_Invert);
        toggleInvert(g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBX.program, m_Shaders.RGBX_Invert);
    }
}


void WindowInverter::Reload()
{
    m_InvertedWindows = {};

    for (const auto& window : g_pCompositor->m_vWindows)
        InvertIfMatches(window);

    if (m_IgnoreDecorations) {
        Hyprlang::CConfigValue* config = HyprlandAPI::getConfigValue(m_PluginHandle, "plugin:darkwindow:ignore_decorations");
        if (config && config->dataPtr()) {
            m_IgnoreDecorations = *((Hyprlang::INT*) config->dataPtr()) != 0;
        }
    }
}
