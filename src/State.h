#pragma once

#include <any>
#include <hyprutils/string/ConstVarList.hpp>
#include <hyprutils/string/String.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#include <optional>
#include <sstream>
#include <vector>

#include "CustomShader.h"

// All hyprland includes are in this file so the private overwriting works correctly
#define private public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/legacy/ConfigManager.hpp>
#include <hyprland/src/config/lua/bindings/LuaBindingsInternal.hpp>
#include <hyprland/src/config/shared/actions/ConfigActions.hpp>
#include <hyprland/src/config/shared/inotify/ConfigWatcher.hpp>
#include <hyprland/src/config/values/types/StringValue.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprland/src/managers/PointerManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/pass/Pass.hpp>
#include <hyprland/src/render/pass/PassElement.hpp>
#include <hyprland/src/render/pass/SurfacePassElement.hpp>
#undef private

#include "LuaCallbacks.h"
#include "ShadeManager.h"


#define HOOK_FUNCTION(ns, className, methodName, retType, args)                                   \
    namespace _ns_##className_##methodName                                                        \
    {                                                                                             \
        retType hook args;                                                                        \
        retType(*original) args = nullptr;                                                        \
        auto register##className##methodName = []                                                 \
        {                                                                                         \
            g.Hooks.push_back({ #ns #className, #methodName, (void**) &original, (void*) hook }); \
            return true;                                                                          \
        }();                                                                                      \
    }                                                                                             \
    retType _ns_##className_##methodName::hook args

struct State
{
    using WindowRuleEffect = Desktop::Rule::CWindowRuleEffectContainer::storageType;
    using LayerRuleEffect = Desktop::Rule::CLayerRuleEffectContainer::storageType;

    struct UserShader
    {
        std::string Id;
        std::string From;
        std::string Path;
        std::string Source;
        std::string Args;
        bool IntroducesTransparency;
        std::optional<float> FadeInSpeed, FadeOutSpeed;
        std::optional<float> AnimationInterval;
    };

    // assume everything is single threaded

    HANDLE Handle = nullptr;
    ShadeManager Manager;
    SP<Config::Values::IValue> LoadShaders;
    WindowRuleEffect WindowRuleShade;
    LayerRuleEffect LayerRuleShade;
    std::vector<UserShader> UserShaders;

    bool InConfigLoad = false;

    std::vector<CHyprSignalListener> Listeners;

    WindowRuleEffect& GetRule(const PHLWINDOW& _)
    {
        return WindowRuleShade;
    }
    LayerRuleEffect& GetRule(const PHLLS& _)
    {
        return LayerRuleShade;
    }

    void Init(HANDLE handle)
    {
        Handle = handle;
        // check that header version aligns with running version
        const std::string CLIENT_HASH = __hyprland_api_get_client_hash();
        const std::string COMPOSITOR_HASH = __hyprland_api_get_hash();
        if (COMPOSITOR_HASH != CLIENT_HASH)
        {
            NotifyError("Failed to load, mismatched versions! (see logs)");
            throw Efmt(
                "[Hypr-DarkWindow] version mismatch, built against {}, running compositor {}", CLIENT_HASH, COMPOSITOR_HASH
            );
        }
    }

    struct
    {
        bool Active = false;
        WP<Render::ITexture> Texture;
        ShadedElement* ShaderConfig;
        Time::steady_tp Time;
        UniformVariables Uniforms;
    } RenderState;

    struct Hook
    {
        std::string className;
        std::string methodName;
        void** originalPtr;
        void* hookFunc;
        CFunctionHook* hypr;
    };
    std::vector<Hook> Hooks;

    void HookFunctions()
    {
        for (auto& hook : Hooks)
        {
            auto all = HyprlandAPI::findFunctionsByName(Handle, hook.methodName);
            auto found = std::find_if(
                all.begin(),
                all.end(),
                [&](const SFunctionMatch& line)
                { return line.demangled.starts_with(hook.className + "::" + hook.methodName + "("); }
            );

            if (found == all.end())
                throw Efmt("Failed to find {}::{}", hook.className, hook.methodName);

            hook.hypr = HyprlandAPI::createFunctionHook(Handle, found->address, hook.hookFunc);
            if (!hook.hypr->hook())
                throw Efmt("Failed to hook {}::{}", hook.className, hook.methodName);

            *hook.originalPtr = hook.hypr->m_original;
        }
    };

    inline static const char* USER_SHADER_CATEGORY =
        "plugin:darkwindow:shader";  // TODO: not currently used with the Lua config, clean up at some point
    inline static const char* LOAD_SHADERS_KEY = "plugin:darkwindow:load_shaders";

    void AddConfigValues()
    {
        // legacy config is only supported for backwards compatibility, no new features will be added
        if (auto legacy = Config::Legacy::mgr())
        {
            legacy->m_config->addSpecialCategory(
                USER_SHADER_CATEGORY,
                {
                    .key = "id",
                }
            );
            legacy->m_config->addSpecialConfigValue(USER_SHADER_CATEGORY, "from", "");
            legacy->m_config->addSpecialConfigValue(USER_SHADER_CATEGORY, "path", "");
            legacy->m_config->addSpecialConfigValue(USER_SHADER_CATEGORY, "args", "");
            legacy->m_config->addSpecialConfigValue(USER_SHADER_CATEGORY, "introduces_transparency", Hyprlang::INT{ 0 });
        }
        else
        {
            const auto registerLuaFn = [&](const std::string& name, lua_CFunction func)
            {
                if (!HyprlandAPI::addLuaFunction(Handle, "darkwindow", name, func))
                    throw Efmt("Failed to register Lua function hl.plugin.darwindow.{}", name);
            };
            registerLuaFn("load_shader", &LuaCallbacks::loadShader);
            registerLuaFn("dsp_shade", &LuaCallbacks::shade);
            registerLuaFn("build_rule_effect", &LuaCallbacks::buildRule);
            registerLuaFn("build_window_rule", &LuaCallbacks::buildRule);
        }

        LoadShaders = SP(new Config::Values::CStringValue(
            LOAD_SHADERS_KEY, "comma separated list of shaders to load, can be empty or \"all\"", "all"
        ));
        if (!HyprlandAPI::addConfigValueV2(Handle, LoadShaders))
            throw Efmt("Failed to add config value {}", LOAD_SHADERS_KEY);

        WindowRuleShade = Desktop::Rule::windowEffects()->registerEffect("darkwindow:shade");
        LayerRuleShade = Desktop::Rule::layerEffects()->registerEffect("darkwindow:shade");
    }

    Hyprutils::String::CConstVarList Config_LoadedShaders()
    {
        return Hyprutils::String::CConstVarList(((Config::Values::CStringValue*) LoadShaders.get())->value());
    }

    std::vector<UserShader> Config_UserShaders()
    {
        if (auto legacy = Config::Legacy::mgr())
        {
            std::vector<UserShader> shaders;

            auto ids = legacy->m_config->listKeysForSpecialCategory(USER_SHADER_CATEGORY);
            for (auto& id : std::set<std::string>(ids.begin(), ids.end()))
            {
                auto from = std::any_cast<Hyprlang::STRING>(
                    legacy->m_config->getSpecialConfigValue(USER_SHADER_CATEGORY, "from", id.c_str())
                );
                auto path = std::any_cast<Hyprlang::STRING>(
                    legacy->m_config->getSpecialConfigValue(USER_SHADER_CATEGORY, "path", id.c_str())
                );
                auto args = std::any_cast<Hyprlang::STRING>(
                    legacy->m_config->getSpecialConfigValue(USER_SHADER_CATEGORY, "args", id.c_str())
                );
                auto transparent = std::any_cast<Hyprlang::INT>(
                    legacy->m_config->getSpecialConfigValue(USER_SHADER_CATEGORY, "introduces_transparency", id.c_str())
                );

                shaders.push_back(
                    UserShader{
                        .Id = id,
                        .From = from,
                        .Path = path,
                        .Args = args,
                        .IntroducesTransparency = transparent > 0,
                    }
                );
            }

            return shaders;
        }
        else
            return UserShaders;
    }

    void RemoveConfigValues()
    {
        if (auto legacy = Config::Legacy::mgr())
        {
            legacy->m_config->removeSpecialConfigValue(USER_SHADER_CATEGORY, "from");
            legacy->m_config->removeSpecialConfigValue(USER_SHADER_CATEGORY, "path");
            legacy->m_config->removeSpecialConfigValue(USER_SHADER_CATEGORY, "args");
            legacy->m_config->removeSpecialConfigValue(USER_SHADER_CATEGORY, "introduces_transparency");
            legacy->m_config->removeSpecialCategory(USER_SHADER_CATEGORY);
        }

        Desktop::Rule::windowEffects()->unregisterEffect(WindowRuleShade);
        Desktop::Rule::layerEffects()->unregisterEffect(LayerRuleShade);
    }


    template<typename... Args>
    static std::runtime_error Efmt(std::format_string<Args...> fmt, Args&&... args)
    {
        return std::runtime_error(std::format(fmt, std::forward<Args>(args)...));
    }

    void NotifyError(const std::string& err)
    {
        std::string msg = std::string("[Hypr-DarkWindow] ") + err;
        Log::logger->log(Log::ERR, msg);
        HyprlandAPI::addNotification(Handle, msg, CHyprColor(0xFFFF0000), 25'000);
    }

    auto HandleError(auto f)
    {
        return [&](std::string args)
        {
            try
            {
                f(args);
                return SDispatchResult{};
            }
            catch (const std::exception& ex)
            {
                NotifyError(std::format("Exception in dispatcher: {}", ex.what()));
                return SDispatchResult{ .success = false, .error = ex.what() };
            }
        };
    };
};

inline State g;
