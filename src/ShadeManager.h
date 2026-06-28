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

    void RecheckRules();

    template<class ElementT>
    void ApplyRuleShader(ElementT ele);
    template<class ElementT>
    void ApplyDispatchedShader(ElementT ele, const std::string& shader);
    template<class ElementT>
    void ForgetElement(ElementT ele);
    template<class ElementT>
    ShadedElement* GetShaderForElement(ElementT ele);


    void PreRenderMonitor(PHLMONITOR monitor);
    void MouseMove();

private:
    std::map<std::string, ShaderInstance> m_Shaders;

    std::tuple<std::map<PHLWINDOW, ShadedElement>, std::map<PHLLS, ShadedElement>> m_ShadedElements;

    template<class T>
    std::map<T, ShadedElement>& getShadedElements()
    {
        return std::get<std::map<T, ShadedElement>>(m_ShadedElements);
    }
};
