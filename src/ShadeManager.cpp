#include "ShadeManager.h"

#include <fstream>

#include "State.h"
#include "PredefinedShaders.h"


Uniforms ShaderDefinition::ParseArgs(const std::string& args)
{
    Uniforms out;
    std::stringstream ss(args);

    ss >> std::ws;
    while (!ss.eof())
    {
        std::string name;
        std::getline(ss, name, '=');
        name = Hyprutils::String::trim(name);
        for (auto [i, c] : std::views::enumerate(name))
        {
            bool first = c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '_';
            bool other = c >= '0' && c <= '9';
            bool special = c == '[' || c == ']' || c == '.';

            if (!(first || ((other || special) && i != 0)))
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

                if (ss.fail()) break;
                values.push_back(next);

                ss >> std::ws;
                if (ss.peek() == ',')
                {
                    ss.get();
                    ss >> std::ws;
                }
                if (ss.peek() == ']') {
                    ss.get();
                    break;
                }

                if (ss.eof()) throw g.Efmt("expected ']' not found");
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
        if (ss.fail()) throw g.Efmt("expected a float");
        ss >> std::ws;

        out[name] = values;
    }

    return out;
}

ShaderInstance* ShadeManager::AddShader(ShaderDefinition def)
{
    auto found = m_Shaders.find(def.ID);
    if (found != m_Shaders.end()) return &found->second;

    ShaderInstance shader{ .ID = def.ID };

    if (def.Source != "")
        shader.Compiled = SP(new CompiledShaders{ .CustomSource = def.Source });
    else try
    {
        if (def.From != "")
        {
            auto from = m_Shaders.find(def.From);
            if (from == m_Shaders.end())
                throw g.Efmt("Unknown .from shader {}", def.From);

            shader.Args = from->second.Args;
            shader.Compiled = from->second.Compiled;
            shader.Transparency = from->second.Transparency;
        }

        if (def.Path != "")
        {
            std::ifstream file(def.Path);
            std::string source = std::string(std::istreambuf_iterator(file), {});

            shader.Compiled = SP(new CompiledShaders{ .CustomSource = source });
            shader.Compiled->TestCompilation(def.Args);
        }

        if (!shader.Compiled)
            throw g.Efmt("Either .from or .path has to be set");
    }
    catch (const std::exception& ex)
    {
        throw g.Efmt("Failed to load shader {}: {}", def.ID, ex.what());
    }

    def.Args.merge(shader.Args);
    shader.Args = std::move(def.Args);
    shader.Transparency = IntroducesTransparency{ shader.Transparency || def.Transparency };

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
    if (space == std::string::npos)
    {
        auto found = m_Shaders.find(shader);
        if (found == m_Shaders.end())
            throw g.Efmt("Unable to find shader {}", shader);

        return &found->second;
    }
    else
    {
        auto from = Hyprutils::String::trim(shader.substr(0, space));
        auto args = shader.substr(space + 1);
        return AddShader(ShaderDefinition{
            .ID = shader,
            .From = from,
            .Args = ShaderDefinition::ParseArgs(args),
            });
    }
}

void ShadeManager::LoadPredefinedShader(const std::string& name)
{
    static const auto add = [](ShadeManager* self, const auto& source) {
        auto& [id, options] = source;
        self->AddShader(ShaderDefinition{
            .ID = id,
            .Source = options.Source,
            .Args = options.DefaultArgs,
            .Transparency = options.Transparency,
            });
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
    for (const auto& window : g_pCompositor->m_windows)
        ApplyWindowRuleShader(window);
}

void ShadeManager::ApplyWindowRuleShader(PHLWINDOW window)
{
    // for some reason, some events (currently `activeWindow`) sometimes pass a null pointer
    if (!window) return;

    std::string shader;
    auto& props = window->m_ruleApplicator->m_otherProps.props;

    if (auto it = props.find(g.RuleShade); it != props.end())
        shader = it->second->effect;

    auto it = m_Windows.find(window);
    if (it != m_Windows.end())
    {
        if (it->second.RuleShader && it->second.RuleShader->ID == shader)
            it->second.RuleShader = nullptr;
        else
            it->second.RuleShader = EnsureShader(shader);
    }
    else
        it = m_Windows.emplace(window, ShadedWindow{ .RuleShader = EnsureShader(shader) }).first;

    windowShaderChanged(it);
}


void ShadeManager::ApplyDispatchedShader(PHLWINDOW window, const std::string& shader)
{
    if (!window) return;

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

ShaderInstance* ShadeManager::GetShaderForWindow(PHLWINDOW window)
{
    auto it = m_Windows.find(window);
    return it != m_Windows.end() ? it->second.ActiveShader : nullptr;
}


void ShadeManager::windowShaderChanged(decltype(m_Windows)::iterator it) {
    auto& s = it->second;

    s.ActiveShader = nullptr;
    if (s.RuleShader && s.DispatchShader)
    {
        if (s.RuleShader->ID != s.DispatchShader->ID)
            s.ActiveShader = s.DispatchShader;
    }
    else if (s.DispatchShader)
        s.ActiveShader = s.DispatchShader;
    else if (s.RuleShader)
        s.ActiveShader = s.RuleShader;

    g_pHyprRenderer->damageWindow(it->first);

    if (!s.ActiveShader)
        m_Windows.erase(it);
}

