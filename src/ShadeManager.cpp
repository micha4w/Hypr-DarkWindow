#include "ShadeManager.h"

#include <fstream>

#include "CustomShader.h"
#include "PredefinedShaders.h"
#include "State.h"


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
            shader.Compiled->TestCompilation(def.Args);
        }

        if (def.Source != "")
        {
            shader.Compiled = SP(new CompiledShaders{ .CustomSource = def.Source });
            shader.Compiled->TestCompilation(def.Args);
        }

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

    for (auto& [_, variant] : shader.Compiled->FragVariants)
        variant.PrimeUniforms(shader.Args);

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
    {
        inst = AddShader(ShaderDefinition::Parse(shader));
    }

    if (inst->Compiled->FailedCompilation)
        throw g.Efmt("Shader {} failed to compile in a previous iteration, skipping", shader);

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


void ShadeManager::RecheckWindowRules()
{
    for (const auto& window : Desktop::viewState()->windows())
        ApplyWindowRuleShader(window);
}

void ShadeManager::ApplyWindowRuleShader(PHLWINDOW window)
{
    // for some reason, some events (currently `activeWindow`) sometimes pass a null pointer
    if (!window)
        return;

    std::optional<std::string> newShader, currentShader;
    auto& props = window->m_ruleApplicator->m_otherProps.props;

    if (auto it = props.find(g.RuleShade); it != props.end())
        newShader = it->second->effect;

    auto it = m_Windows.find(window);
    if (it != m_Windows.end() && it->second.RuleShader)
        currentShader = it->second.RuleShader->ID;

    if (newShader != currentShader)
    {
        if (it == m_Windows.end() && newShader)
            it = m_Windows.emplace(window, ShadedWindow{ .RuleShader = EnsureShader(*newShader) }).first;
        else
            it->second.RuleShader = newShader ? EnsureShader(*newShader) : nullptr;

        windowShaderChanged(it);
    }
}


void ShadeManager::ApplyDispatchedShader(PHLWINDOW window, const std::string& shader)
{
    if (!window)
        return;

    auto it = m_Windows.find(window);
    if (it != m_Windows.end())
    {
        if (it->second.DispatchShader && it->second.DispatchShader->ID == shader)
            it->second.DispatchShader = nullptr;
        else
            it->second.DispatchShader = EnsureShader(shader);
    }
    else
        it = m_Windows.emplace(window, ShadedWindow{ .DispatchShader = EnsureShader(shader) }).first;

    windowShaderChanged(it);
}

void ShadeManager::ForgetWindow(PHLWINDOW window)
{
    m_Windows.erase(window);
}

ShadedWindow* ShadeManager::GetShaderForWindow(PHLWINDOW window)
{
    auto it = m_Windows.find(window);
    return it != m_Windows.end() && it->second.ActiveShader ? &it->second : nullptr;
}


void ShadeManager::PreRenderMonitor(PHLMONITOR monitor)
{
    g.RenderState.Time = Time::steadyNow();

    for (auto it = m_Windows.begin(); it != m_Windows.end();)
    {
        auto& [window, config] = *it;

        bool needsDamage = false, remove = false;
        if (config.ActiveShader->Compiled->UsesTimeUniform)
        {
            float secondsPassed =
                std::chrono::duration_cast<std::chrono::milliseconds>(g.RenderState.Time - config.StartTime).count() /
                1000.f;
            float loopDur = config.ActiveShader->AnimationInterval > 0.f ? config.ActiveShader->AnimationInterval : 86'400.f;
            if (secondsPassed > loopDur)
            {
                double loops = std::floor(secondsPassed / loopDur);
                config.StartTime += std::chrono::milliseconds((long) (loopDur * loops * 1000.f));
            }

            if (g_pHyprRenderer->shouldRenderWindow(window, monitor))
                needsDamage = true;
        }

        switch (config.FadeState)
        {
        case ShadedWindow::FadeIn:
            needsDamage = true;

            if (g.RenderState.Time >
                config.FadeStartTime + std::chrono::milliseconds((long) (config.ActiveShader->FadeInSpeed * 100.f)))
                config.FadeState = ShadedWindow::None;

            break;
        case ShadedWindow::None:
            break;
        case ShadedWindow::FadeOut:
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
                config.FadeState = config.ConfiguredShader->FadeInSpeed > 0.f ? ShadedWindow::FadeIn : ShadedWindow::None;
            }
            break;
        }

        if (needsDamage)
        {
            // Code stolen from g_pHyprRenderer->damageWindow
            auto windowBox = window->getFullWindowBoundingBox();
            CBox fixedDamageBox = { windowBox.pos() - monitor->m_position, windowBox.size() };
            fixedDamageBox.scale(monitor->m_scale);
            monitor->addDamage(fixedDamageBox);
        }

        if (remove)
            it = m_Windows.erase(it);
        else
            ++it;
    }
}

void ShadeManager::MouseMove()
{
    for (auto& [window, config] : m_Windows)
    {
        if (config.ActiveShader->Compiled->UsesMousePosUniform)
            g_pHyprRenderer->damageWindow(window);
    }
}


void ShadeManager::windowShaderChanged(decltype(m_Windows)::iterator it)
{
    auto& [window, s] = *it;

    ShaderInstance* prevConfigured = s.ConfiguredShader;
    s.ConfiguredShader = nullptr;
    if (s.RuleShader && s.DispatchShader)
    {
        if (s.RuleShader->ID != s.DispatchShader->ID)
            s.ConfiguredShader = s.DispatchShader;
    }
    else if (s.DispatchShader)
        s.ConfiguredShader = s.DispatchShader;
    else if (s.RuleShader)
        s.ConfiguredShader = s.RuleShader;

    if (prevConfigured == s.ConfiguredShader)
    {
        s.ActiveShader = s.FadingOutShader ? s.FadingOutShader : s.ConfiguredShader;
        return;
    }

    auto prevStartTime = s.FadeStartTime;
    auto prevFadeState = s.FadeState;
    if (!s.FadingOutShader)
    {
        auto now = Time::steadyNow();
        s.StartTime = now;
        s.FadeStartTime = now;
        s.FadeState = ShadedWindow::None;

        if (prevConfigured && prevConfigured->FadeOutSpeed > 0.f)
        {
            s.FadingOutShader = prevConfigured;
            s.FadeState = ShadedWindow::FadeOut;

            if (prevFadeState == ShadedWindow::FadeIn)
            {
                auto ms_passed = std::chrono::duration_cast<std::chrono::milliseconds>(now - prevStartTime).count();
                auto progress = 1.f - ms_passed / (s.FadingOutShader->FadeInSpeed * 100.f);
                if (progress < 0.f)
                    progress = 0.f;

                s.FadeStartTime =
                    now - std::chrono::milliseconds((long) (progress * s.FadingOutShader->FadeOutSpeed * 100.f));
            }
        }
        else if (s.ConfiguredShader && s.ConfiguredShader->FadeInSpeed > 0.f)
            s.FadeState = ShadedWindow::FadeIn;
    }
    else if (s.FadingOutShader == s.ConfiguredShader)
    {
        auto now = Time::steadyNow();
        s.FadingOutShader = nullptr;
        s.FadeStartTime = now;
        s.FadeState = ShadedWindow::None;

        if (prevFadeState == ShadedWindow::FadeOut && s.ConfiguredShader->FadeInSpeed > 0.f)
        {
            auto ms_passed = std::chrono::duration_cast<std::chrono::milliseconds>(now - prevStartTime).count();
            auto progress = 1.f - ms_passed / (s.ConfiguredShader->FadeOutSpeed * 100.f);
            if (progress < 0.f)
                progress = 0.f;

            s.FadeStartTime = now - std::chrono::milliseconds((long) (progress * s.ConfiguredShader->FadeInSpeed * 100.f));
            s.FadeState = ShadedWindow::FadeIn;
        }
    }

    s.ActiveShader = s.FadingOutShader ? s.FadingOutShader : s.ConfiguredShader;

    g_pHyprRenderer->damageWindow(window);

    if (!s.ActiveShader)
        m_Windows.erase(it);
}
