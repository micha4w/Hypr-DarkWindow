#include "ShadeManager.h"

#include <fstream>

#include "State.h"


ShaderInstance* ShadeManager::GetShaderForWindow(PHLWINDOW window)
{
    auto it = m_Windows.find(window);
    return it != m_Windows.end() ? it->second.ActiveShader : nullptr;
}

SP<CShader> ShadeManager::GetOrCreateShaderForWindow(PHLWINDOW window, uint8_t features, std::function<SP<CShader>(ShaderInstance*)> create)
{
    auto shaders = g.Manager.GetShaderForWindow(window);
    if (!shaders || shaders->Compiled->FailedCompilation)
        return nullptr;

    auto compiled_it = shaders->Compiled->FragVariants.find(features);
    try
    {
        if (compiled_it == shaders->Compiled->FragVariants.end())
        {
            auto newShader = create(shaders);

            if (newShader->program() == 0)
                throw g.Efmt("Failed to compile shader variant, check logs for details");

            compiled_it = shaders->Compiled->FragVariants.emplace(features, CompiledShaders::CustomShader{ Shader: newShader }).first;
            compiled_it->second.PrimeUniforms(shaders->Args);
        }

        compiled_it->second.SetUniforms(shaders->Args);
    }
    catch (const std::exception& ex)
    {
        shaders->Compiled->FailedCompilation = true;
        g.NotifyError(std::string("Failed to apply custom shader: ") + ex.what());
        return nullptr;
    }

    return compiled_it->second.Shader;
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
        it = m_Windows.emplace(window, ShadeWindow{ .RuleShader = EnsureShader(shader) }).first;

    WindowShaderChanged(it);
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
        it = m_Windows.emplace(window, ShadeWindow{ .DispatchShader = EnsureShader(shader) }).first;

    // window->m_activeInactiveAlpha->setValue(0.5f);

    WindowShaderChanged(it);
}

void ShadeManager::ForgetWindow(PHLWINDOW window)
{
    m_Windows.erase(window);
}

void ShadeManager::RecheckWindowRules()
{
    for (const auto& window : g_pCompositor->m_windows)
        ApplyWindowRuleShader(window);
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
        for (const auto& source : g.WindowShaders)
            add(this, source);
    }
    else
    {
        auto source = g.WindowShaders.find(name);
        if (source == g.WindowShaders.end())
            throw g.Efmt("Predefined shader with name {} not found", name);

        add(this, *source);
    }
}

ShaderInstance* ShadeManager::AddShader(ShaderDefinition def)
{
    auto found = m_Shaders.find(def.ID);
    if (found != m_Shaders.end()) return found->second.get();

    UP<ShaderInstance> shader(new ShaderInstance{ .ID = def.ID });

    if (def.Source != "")
        shader->Compiled = SP(new CompiledShaders{ .CustomSource = def.Source });
    else try
    {
        if (def.From != "")
        {
            if (!m_Shaders.contains(def.From))
                throw g.Efmt("Unknown .from shader", def.ID);

            auto& from = m_Shaders[def.From];
            shader->Args = from->Args;
            shader->Compiled = from->Compiled;
            shader->Transparency = from->Transparency;
        }

        if (def.Path != "")
        {
            std::ifstream file(def.Path);
            std::string source = std::string(std::istreambuf_iterator(file), {});

            shader->Compiled = SP(new CompiledShaders{ .CustomSource = source });
            shader->Compiled->TestCompilation(def.Args);
        }

        if (!shader->Compiled)
            throw g.Efmt("Either .from or .path has to be set");
    }
    catch (const std::exception& ex)
    {
        throw g.Efmt("Failed to load shader {}: {}", def.ID, ex.what());
    }

    def.Args.merge(shader->Args);
    def.Args.swap(shader->Args);
    shader->Transparency = IntroducesTransparency{ shader->Transparency || def.Transparency };

    auto ret = shader.get();
    m_Shaders[def.ID] = std::move(shader);
    return ret;
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

        return found->second.get();
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

    void ShadeManager::WindowShaderChanged(decltype(m_Windows)::iterator it) {
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