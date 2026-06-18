#pragma once

#include "CustomShader.h"

// TODO: unify this with usershader once legacy is gone
struct ShaderDefinition
{
    std::string ID;

    // At least one of these must be set
    std::string From;
    std::string Path;
    std::string Source;

    Uniforms Args;
    IntroducesTransparency Transparency;
    std::optional<float> FadeInSpeed, FadeOutSpeed;
    std::optional<float> AnimationInterval;

    static ShaderDefinition Parse(const std::string& shader);
};

class ShadeManager
{
public:
    ShaderInstance* AddShader(ShaderDefinition&& def);
    ShaderInstance* EnsureShader(const std::string& shader);
    void LoadPredefinedShader(const std::string& name);

    void RecheckWindowRules();
    void ApplyWindowRuleShader(PHLWINDOW window);
    void ApplyDispatchedShader(PHLWINDOW window, const std::string& shader);
    void ForgetWindow(PHLWINDOW window);
    ShadedWindow* GetShaderForWindow(PHLWINDOW window);

    void PreRenderMonitor(PHLMONITOR monitor);
    void MouseMove();

private:
    std::map<std::string, ShaderInstance> m_Shaders;

    std::map<PHLWINDOW, ShadedWindow> m_Windows;

    void windowShaderChanged(decltype(m_Windows)::iterator it);
};
