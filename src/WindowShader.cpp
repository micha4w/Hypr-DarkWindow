#include "WindowShader.h"

#include <hyprutils/string/String.hpp>


void WindowShader::OnRenderWindowPre(PHLWINDOW window)
{
    m_ShadersSwapped.reset();

    auto shader = m_ShadedWindows.find(window);
    auto manualShader = m_ManuallyShadedWindows.find(window);
    if (shader != m_ShadedWindows.end() && manualShader != m_ManuallyShadedWindows.end())
    {
        if (shader->second->ID != manualShader->second->ID)
            m_ShadersSwapped = manualShader->second;
    }
    else if (manualShader != m_ManuallyShadedWindows.end())
        m_ShadersSwapped = manualShader->second;
    else if (shader != m_ShadedWindows.end())
        m_ShadersSwapped = shader->second;

    if (m_ShadersSwapped) {
        (*m_ShadersSwapped)->CompiledShaders->ApplyArgs((*m_ShadersSwapped)->Args);

        std::swap((*m_ShadersSwapped)->CompiledShaders->EXT, g_pHyprOpenGL->m_shaders->m_shEXT);
        std::swap((*m_ShadersSwapped)->CompiledShaders->RGBA, g_pHyprOpenGL->m_shaders->m_shRGBA);
        std::swap((*m_ShadersSwapped)->CompiledShaders->RGBX, g_pHyprOpenGL->m_shaders->m_shRGBX);
        std::swap((*m_ShadersSwapped)->CompiledShaders->CM, g_pHyprOpenGL->m_shaders->m_shCM);
    }
}

void WindowShader::OnRenderWindowPost()
{
    if (m_ShadersSwapped)
    {
        std::swap((*m_ShadersSwapped)->CompiledShaders->EXT, g_pHyprOpenGL->m_shaders->m_shEXT);
        std::swap((*m_ShadersSwapped)->CompiledShaders->RGBA, g_pHyprOpenGL->m_shaders->m_shRGBA);
        std::swap((*m_ShadersSwapped)->CompiledShaders->RGBX, g_pHyprOpenGL->m_shaders->m_shRGBX);
        std::swap((*m_ShadersSwapped)->CompiledShaders->CM, g_pHyprOpenGL->m_shaders->m_shCM);
        m_ShadersSwapped.reset();
    }
}

void WindowShader::OnWindowClose(PHLWINDOW window)
{
    m_ShadedWindows.erase(window);
    m_ManuallyShadedWindows.erase(window);
}

void WindowShader::Unload()
{
    OnRenderWindowPost();

    m_Shaders.clear();
    m_CompiledShaders.clear();
}

void WindowShader::ShadeIfMatches(PHLWINDOW window)
{
    // for some reason, some events (currently `activeWindow`) sometimes pass a null pointer
    if (!window) return;

    std::vector<SP<CWindowRule>> rules = g_pConfigManager->getMatchingRules(window);
    std::optional<std::string> shader;
    for (const SP<CWindowRule>& rule : rules)
    {
        if (rule->m_rule == "plugin:invertwindow")
        {
            shader = "invert";
            break;
        }

        if (rule->m_rule.starts_with("plugin:shadewindow:"))
        {
            shader = rule->m_rule.substr(std::string("plugin:shadewindow:").size());
            break;
        }
    }

    auto windowIt = m_ShadedWindows.find(window);
    std::optional<std::string> currentShader;
    if (windowIt != m_ShadedWindows.end()) currentShader = windowIt->second->ID;

    if (shader != currentShader)
    {
        if (shader)
            m_ShadedWindows[window] = m_Shaders.at(*shader).get();
        else
            m_ShadedWindows.erase(window);

        g_pHyprRenderer->damageWindow(window);
    }
}


void WindowShader::ToggleShade(PHLWINDOW window, const std::string& shader)
{
    if (!window)
        return;
    
    auto windowIt = m_ManuallyShadedWindows.find(window);
    std::optional<std::string> currentShader;
    if (windowIt != m_ManuallyShadedWindows.end()) currentShader = windowIt->second->ID;

    if (!currentShader || shader != *currentShader)
        m_ManuallyShadedWindows[window] = m_Shaders.at(shader).get();
    else
        m_ManuallyShadedWindows.erase(window);

    g_pHyprRenderer->damageWindow(window);
}

void WindowShader::AddShader(std::string id, std::variant<std::string, std::string> nameOrPath, std::string args)
{
    if (m_Shaders.contains(id)) return;

    if (!m_CompiledShaders.contains(nameOrPath))
        m_CompiledShaders[nameOrPath] = std::make_unique<ShaderHolder>(nameOrPath);

    auto compiled = m_CompiledShaders[nameOrPath].get();

    auto parsedArgs = parseArgs(args);
    compiled->LoadArgs(parsedArgs);
    m_Shaders[id] = std::make_unique<ShaderConfig>(id, compiled, parsedArgs);
}


void WindowShader::ReshadeWindows()
{
    m_ShadedWindows = {};

    for (const auto& window : g_pCompositor->m_windows)
        ShadeIfMatches(window);
}