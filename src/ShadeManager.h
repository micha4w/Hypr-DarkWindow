#pragma once

#include "CustomShader.h"

struct ShaderDefinition
{
    std::string ID;

    // At least one of these must be set
    std::string From;
    std::string Path;
    std::string Source;

    Uniforms Args;
    IntroducesTransparency Transparency;

    static Uniforms ParseArgs(const std::string& args);
};

class ShadeManager
{
public:
    ShaderInstance* AddShader(ShaderDefinition def);
    ShaderInstance* EnsureShader(const std::string& shader);
    void LoadPredefinedShader(const std::string& name);

    void RecheckWindowRules();
    void ApplyWindowRuleShader(PHLWINDOW window);
    void ApplyDispatchedShader(PHLWINDOW window, const std::string& shader);
    void ForgetWindow(PHLWINDOW window);
    ShaderInstance* GetShaderForWindow(PHLWINDOW window);

    void PreRenderMonitor(PHLMONITOR monitor);

private:
    std::map<std::string, ShaderInstance> m_Shaders;

    struct ShadedWindow {
        ShaderInstance* RuleShader;
        ShaderInstance* DispatchShader;

        ShaderInstance* ActiveShader;
    };

    std::map<PHLWINDOW, ShadedWindow> m_Windows;

    void windowShaderChanged(decltype(m_Windows)::iterator it);
};
