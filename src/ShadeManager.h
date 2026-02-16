#pragma once

#include "CustomShader.h"

class ShadeManager
{
public:
    void RecheckWindowRules();

    void ApplyWindowRuleShader(PHLWINDOW window);
    void ApplyDispatchedShader(PHLWINDOW window, const std::string& shader);
    void ForgetWindow(PHLWINDOW window);

    void LoadPredefinedShader(const std::string& name);
    ShaderInstance* AddShader(ShaderDefinition def);
    ShaderInstance* EnsureShader(const std::string& shader);

    ShaderInstance* GetShaderForWindow(PHLWINDOW window);
    SP<CShader> GetOrCreateShaderForWindow(PHLWINDOW window, uint8_t features, std::function<SP<CShader>(ShaderInstance*)> create);

private:
    std::map<std::string, UP<ShaderInstance>> m_Shaders;

    struct ShadeWindow {
        ShaderInstance* RuleShader;
        ShaderInstance* DispatchShader;

        ShaderInstance* ActiveShader;
    };

    std::map<PHLWINDOW, ShadeWindow> m_Windows;

    void WindowShaderChanged(decltype(m_Windows)::iterator it);
};
