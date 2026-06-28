#include "ShadeManager.h"

#include <fstream>

#include "CustomShader.h"
#include "PredefinedShaders.h"
#include "State.h"

static bool shouldRender(PHLLS ls, PHLMONITOR monitor)
{
    return true;
}

static bool shouldRender(PHLWINDOW window, PHLMONITOR monitor)
{
    return g_pHyprRenderer->shouldRenderWindow(window, monitor);
}

static void damageElement(PHLLS ls)
{
    if (auto box = ls->logicalBox())
        g_pHyprRenderer->damageBox(*box, false);
}

static void damageElement(PHLWINDOW window)
{
    g_pHyprRenderer->damageWindow(window);
}

ShaderDefinition ShaderDefinition::Parse(const std::string& shader)
{
    ShaderDefinition out;
    out.ID = shader;

    std::stringstream ss(shader);

    ss >> std::ws;
    ss >> out.From;
    ss >> std::ws;

    while (!ss.eof())
    {
        std::string name;
        std::getline(ss, name, '=');
        if (ss.eof())
            throw g.Efmt("expected '=' not found");

        name = Hyprutils::String::trim(name);
        for (auto [i, c] : std::views::enumerate(name))
        {
            bool everywhere = c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '_' || c == '.';
            bool arrays = c == '[' || c == ']';
            bool not_first = arrays || (c >= '0' && c <= '9');

            if (!(everywhere || (not_first && i != 0)))
                throw g.Efmt("invalid shader uniform name '{}'", name);
        }
        ss >> std::ws;

        std::vector<float> values;
        if (ss.peek() == '[')
        {
            ss.get();
            while (1)
            {
                std::string rem(ss.str().substr(ss.tellg()));

                float next;
                ss >> next;

                if (ss.fail())
                    break;
                values.push_back(next);

                ss >> std::ws;
                if (ss.peek() == ',')
                {
                    ss.get();
                    ss >> std::ws;
                }
                if (ss.peek() == ']')
                {
                    ss.get();
                    break;
                }

                if (ss.eof())
                    throw g.Efmt("expected ']' not found");
            }

            if (values.size() < 1 || values.size() > 4)
                throw g.Efmt("only support from 1 to 4 values");
        }
        else
        {
            float next;
            ss >> next;
            values.push_back(next);
        }
        if (ss.fail())
            throw g.Efmt("expected a float");
        ss >> std::ws;

        if (name.starts_with('.'))
        {
            if (values.size() != 1)
                throw g.Efmt("shader properties must be a single float value");

            if (name == ".fade_in_speed")
                out.FadeInSpeed = values[0];
            else if (name == ".fade_out_speed")
                out.FadeOutSpeed = values[0];
            else if (name == ".animation_interval")
                out.AnimationInterval = values[0];
            else
                throw g.Efmt("unknown special shader uniform '{}'", name);
        }
        else
            out.Args[name] = values;
    }

    return out;
}

ShaderInstance* ShadeManager::AddShader(ShaderDefinition&& def)
{
    auto found = m_Shaders.find(def.ID);
    if (found != m_Shaders.end())
        return &found->second;

    ShaderInstance shader{ .ID = def.ID };

    try
    {
        if (def.From != "")
        {
            auto from = m_Shaders.find(def.From);
            if (from == m_Shaders.end())
                throw g.Efmt("Unknown .from shader {}", def.From);

            shader = from->second;
            shader.ID = def.ID;
        }

        if (def.Path != "")
        {
            std::ifstream file(def.Path);
            std::string source = "// Your custom User Shader Code:\n";
            source += std::string(std::istreambuf_iterator(file), {});

            shader.Compiled = SP(new CompiledShaders{ .CustomSource = source });
        }

        if (def.Source != "")
            shader.Compiled = SP(new CompiledShaders{ .CustomSource = def.Source });

        if (!shader.Compiled)
            throw g.Efmt("One of .from, .path, or .source has to be set");
    }
    catch (const std::exception& ex)
    {
        throw g.Efmt("Failed to load shader {}: {}", def.ID, ex.what());
    }

    def.Args.merge(shader.Args);
    shader.Args = std::move(def.Args);
    if (def.Transparency)
        shader.Transparency = IntroducesTransparency{ def.Transparency };
    if (def.FadeInSpeed)
        shader.FadeInSpeed = *def.FadeInSpeed;
    if (def.FadeOutSpeed)
        shader.FadeOutSpeed = *def.FadeOutSpeed;
    if (def.AnimationInterval)
        shader.AnimationInterval = *def.AnimationInterval;

    try
    {
        shader.Compiled->TestCompilation(shader.Args);

        for (auto& [_, variant] : shader.Compiled->FragVariants)
            variant.PrimeUniforms(shader.Args);
    }
    catch (const std::exception& ex)
    {
        g.NotifyError("Failed to create shader " + def.ID + ": " + ex.what());
    }

    auto* ptr = &(m_Shaders[def.ID] = std::move(shader));
    ptr->Compiled->Instances.push_back(ptr);
    return ptr;
}

ShaderInstance* ShadeManager::EnsureShader(const std::string& shader)
{
    if (shader == "")
        return nullptr;

    size_t space = shader.find(" ");
    ShaderInstance* inst;

    if (space == std::string::npos)
    {
        auto found = m_Shaders.find(shader);
        if (found == m_Shaders.end())
            throw g.Efmt("Unable to find shader {}", shader);

        inst = &found->second;
    }
    else
        inst = AddShader(ShaderDefinition::Parse(shader));

    return inst;
}

void ShadeManager::LoadPredefinedShader(const std::string& name)
{
    static const auto add = [](ShadeManager* self, const auto& source)
    {
        auto& [id, options] = source;
        self->AddShader(
            ShaderDefinition{
                .ID = id,
                .Source = options.Source,
                .Args = options.DefaultArgs,
                .Transparency = options.Transparency,
                .FadeInSpeed = options.FadeInSpeed,
                .FadeOutSpeed = options.FadeOutSpeed,
                .AnimationInterval = options.AnimationInterval,
            }
        );
    };

    if (name == "all")
    {
        for (const auto& source : WINDOW_SHADERS)
            add(this, source);
    }
    else
    {
        auto source = WINDOW_SHADERS.find(name);
        if (source == WINDOW_SHADERS.end())
            throw g.Efmt("Predefined shader with name {} not found", name);

        add(this, *source);
    }
}

void ShadeManager::SetupFailedCompilationShader()
{
    if (m_FailedCompilation.ActiveShader)
        return;

    auto it = m_Shaders.find("compilation_failed");
    if (it != m_Shaders.end())
        m_FailedCompilation.ActiveShader = &it->second;
}


void ShadeManager::RecheckRules()
{
    for (const auto& window : Desktop::viewState()->windows())
        ApplyRuleShader(window);

    for (const auto& ls : Desktop::viewState()->layers())
        ApplyRuleShader(ls);
}

template<class ElementT>
void ShadeManager::ApplyRuleShader(ElementT ele)
{
    if (!ele)
        return;

    std::optional<std::string> newShader, currentShader;
    auto& props = ele->m_ruleApplicator->m_otherProps.props;

    if (auto it = props.find(g.GetRule(ele)); it != props.end())
        newShader = it->second->effect;

    auto& eles = getShadedElements<ElementT>();

    auto it = eles.find(ele);
    if (it != eles.end() && it->second.RuleShader)
        currentShader = it->second.RuleShader->ID;

    if (newShader != currentShader)
    {
        if (it == eles.end() && newShader)
            it = eles.emplace(ele, ShadedElement{ .RuleShader = EnsureShader(*newShader) }).first;
        else
            it->second.RuleShader = newShader ? EnsureShader(*newShader) : nullptr;

        it->second.Changed();
        if (!it->second.ActiveShader)
            eles.erase(it);

        damageElement(ele);
    }
}
template void ShadeManager::ApplyRuleShader(PHLLS ele);
template void ShadeManager::ApplyRuleShader(PHLWINDOW ele);

template<class ElementT>
void ShadeManager::ApplyDispatchedShader(ElementT ele, const std::string& shader)
{
    if (!ele)
        return;

    auto& eles = getShadedElements<ElementT>();

    auto it = eles.find(ele);
    if (it != eles.end())
    {
        if (it->second.DispatchShader && it->second.DispatchShader->ID == shader)
            it->second.DispatchShader = nullptr;
        else
            it->second.DispatchShader = EnsureShader(shader);
    }
    else
        it = eles.emplace(ele, ShadedElement{ .DispatchShader = EnsureShader(shader) }).first;

    it->second.Changed();
    if (!it->second.ActiveShader)
        eles.erase(it);

    damageElement(ele);
}
template void ShadeManager::ApplyDispatchedShader(PHLLS ele, const std::string& shader);
template void ShadeManager::ApplyDispatchedShader(PHLWINDOW ele, const std::string& shader);

template<class ElementT>
void ShadeManager::ForgetElement(ElementT ele)
{
    getShadedElements<ElementT>().erase(ele);
}
template void ShadeManager::ForgetElement(PHLLS ele);
template void ShadeManager::ForgetElement(PHLWINDOW ele);

template<class ElementT>
ShadedElement* ShadeManager::GetShaderForElement(ElementT ele)
{
    auto& eles = getShadedElements<ElementT>();
    auto it = eles.find(ele);
    if (it == eles.end() || !it->second.ActiveShader)
        return nullptr;

    if (it->second.ActiveShader->Compiled->FailedCompilation)
        return m_FailedCompilation.ActiveShader ? &m_FailedCompilation : nullptr;

    return &it->second;
}
template ShadedElement* ShadeManager::GetShaderForElement(PHLLS ele);
template ShadedElement* ShadeManager::GetShaderForElement(PHLWINDOW ele);

void ShadeManager::PreRenderMonitor(PHLMONITOR monitor)
{
    g.RenderState.Time = Time::steadyNow();

    auto updateElements = [&](auto& eles)
    {
        for (auto it = eles.begin(); it != eles.end();)
        {
            auto& [ele, config] = *it;

            bool needsDamage = false, remove = false;
            if (config.ActiveShader->Compiled->UsesTimeUniform)
            {
                float secondsPassed =
                    std::chrono::duration_cast<std::chrono::milliseconds>(g.RenderState.Time - config.StartTime).count() /
                    1000.f;
                float loopDur =
                    config.ActiveShader->AnimationInterval > 0.f ? config.ActiveShader->AnimationInterval : 86'400.f;
                if (secondsPassed > loopDur)
                {
                    double loops = std::floor(secondsPassed / loopDur);
                    config.StartTime += std::chrono::milliseconds((long) (loopDur * loops * 1000.f));
                }

                needsDamage = true;
            }

            switch (config.FadeState)
            {
            case ShadedElement::FadeIn:
                needsDamage = true;

                if (g.RenderState.Time >
                    config.FadeStartTime + std::chrono::milliseconds((long) (config.ActiveShader->FadeInSpeed * 100.f)))
                    config.FadeState = ShadedElement::None;

                break;
            case ShadedElement::None:
                break;
            case ShadedElement::FadeOut:
                needsDamage = true;

                if (g.RenderState.Time >
                    config.FadeStartTime + std::chrono::milliseconds((long) (config.ActiveShader->FadeOutSpeed * 100.f)))
                {
                    config.ActiveShader = config.ConfiguredShader;
                    if (!config.ActiveShader)
                    {
                        remove = true;
                        break;
                    }

                    config.FadingOutShader = nullptr;
                    config.StartTime = g.RenderState.Time;
                    config.FadeStartTime = g.RenderState.Time;
                    config.FadeState =
                        config.ConfiguredShader->FadeInSpeed > 0.f ? ShadedElement::FadeIn : ShadedElement::None;
                }
                break;
            }

            if (needsDamage && shouldRender(ele, monitor))
            {
                if (auto box = ele->logicalBox())
                {
                    CBox damageBox = box->copy().translate(-monitor->m_position).scale(monitor->m_scale).round();
                    monitor->addDamage(*box);
                }
            }

            if (remove)
                it = eles.erase(it);
            else
                ++it;
        }
    };

    updateElements(getShadedElements<PHLWINDOW>());
    updateElements(getShadedElements<PHLLS>());
}

void ShadeManager::MouseMove()
{
    auto updateElements = [&](auto& eles)
    {
        for (auto& [ele, config] : eles)
        {
            if (config.ActiveShader->Compiled->UsesMousePosUniform)
                damageElement(ele);
        }
    };

    updateElements(getShadedElements<PHLWINDOW>());
    updateElements(getShadedElements<PHLLS>());
}
