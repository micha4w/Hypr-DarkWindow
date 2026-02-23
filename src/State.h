#pragma once

#include <vector>

#include <hyprutils/string/ConstVarList.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#include <hyprutils/string/String.hpp>

// All hyprland includes are in this file so the private overwriting works correctly
#include <hyprland/src/render/pass/PassElement.hpp>
#include <hyprland/src/helpers/time/Time.hpp>

#define private public
#include <hyprland/src/render/pass/SurfacePassElement.hpp>
#include <hyprland/src/render/pass/Pass.hpp>
#undef private

#define m_failedPluginConfigValues    m_failedPluginConfigValues; friend struct State;
#include <hyprland/src/config/ConfigManager.hpp>
#undef m_failedPluginConfigValues

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/event/EventBus.hpp>

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/managers/PointerManager.hpp>
#include <hyprland/src/helpers/time/Time.hpp>

#include <hyprland/src/render/Renderer.hpp>

#include "ShadeManager.h"


#define HOOK_FUNCTION(ns, className, methodName, retType, args)                                  \
    namespace _ns_##className_##methodName {                                                     \
        retType hook args;                                                                       \
        retType (*original) args = nullptr;                                                      \
        auto register##className##methodName = [] {                                              \
            g.Hooks.push_back({ #ns#className, #methodName, (void**) &original, (void*)hook });  \
            return true;                                                                         \
        }();                                                                                     \
    }                                                                                            \
    retType _ns_##className_##methodName::hook args

struct State {
    using WindowRuleEffect = Desktop::Rule::CWindowRuleEffectContainer::storageType;

    // assume everything is single threaded

    HANDLE Handle = nullptr;
    ShadeManager Manager;
    WindowRuleEffect RuleShade;

    std::vector<CHyprSignalListener> Listeners;

#ifdef WATCH_SHADERS
    std::vector<std::string> additionalWatchPaths;
#endif

    void Init(HANDLE handle)
    {
        Handle = handle;
        // check that header version aligns with running version
        const std::string CLIENT_HASH = __hyprland_api_get_client_hash();
        const std::string COMPOSITOR_HASH = __hyprland_api_get_hash();
        if (COMPOSITOR_HASH != CLIENT_HASH)
        {
            NotifyError("Failed to load, mismatched versions! (see logs)");
            throw Efmt("[Hypr-DarkWindow] version mismatch, built against {}, running compositor {}", CLIENT_HASH, COMPOSITOR_HASH);
        }
    }

    struct {
        bool Ignore = true;
        std::optional<ShaderInstance*> Shader;
    } RenderState;

    struct Hook {
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
            auto found = std::find_if(all.begin(), all.end(), [&](const SFunctionMatch& line) {
                return line.demangled.starts_with(hook.className + "::" + hook.methodName + "(");
            });

            if (found == all.end())
                throw Efmt("Failed to find {}::{}", hook.className, hook.methodName);

            hook.hypr = HyprlandAPI::createFunctionHook(Handle, found->address, hook.hookFunc);
            if (!hook.hypr->hook())
                throw Efmt("Failed to hook {}::{}", hook.className, hook.methodName);

            *hook.originalPtr = hook.hypr->m_original;
        }
    };

    inline static const char* USER_SHADER_CATEGORY = "plugin:darkwindow:shader";
    inline static const char* LOAD_SHADERS_KEY = "plugin:darkwindow:load_shaders";

    struct UserShader {
        std::string Id;
        Hyprlang::STRING From;
        Hyprlang::STRING Path;
        Hyprlang::STRING Args;
        bool IntroducesTransparency;
    };

    void AddConfigValues()
    {
        g_pConfigManager->m_config->addSpecialCategory(USER_SHADER_CATEGORY, { .key = "id", });
        g_pConfigManager->m_config->addSpecialConfigValue(USER_SHADER_CATEGORY, "from", "");
        g_pConfigManager->m_config->addSpecialConfigValue(USER_SHADER_CATEGORY, "path", "");
        g_pConfigManager->m_config->addSpecialConfigValue(USER_SHADER_CATEGORY, "args", "");
        g_pConfigManager->m_config->addSpecialConfigValue(USER_SHADER_CATEGORY, "introduces_transparency", Hyprlang::INT{ 0 });

        HyprlandAPI::addConfigValue(Handle, LOAD_SHADERS_KEY, Hyprlang::STRING("all"));

        RuleShade = Desktop::Rule::windowEffects()->registerEffect("darkwindow:shade");
    }

    Hyprutils::String::CConstVarList Config_LoadedShaders()
    {
        return Hyprutils::String::CConstVarList(
            (Hyprlang::STRING)HyprlandAPI::getConfigValue(Handle, LOAD_SHADERS_KEY)->dataPtr()
        );
    }

    std::vector<UserShader> Config_UserShaders()
    {
        std::vector<UserShader> shaders;

        auto ids = g_pConfigManager->m_config->listKeysForSpecialCategory(USER_SHADER_CATEGORY);
        for (auto& id : std::set<std::string>(ids.begin(), ids.end()))
        {
            auto from = std::any_cast<Hyprlang::STRING>(
                g_pConfigManager->m_config->getSpecialConfigValue(USER_SHADER_CATEGORY, "from", id.c_str()));
            auto path = std::any_cast<Hyprlang::STRING>(
                g_pConfigManager->m_config->getSpecialConfigValue(USER_SHADER_CATEGORY, "path", id.c_str()));
            auto args = std::any_cast<Hyprlang::STRING>(
                g_pConfigManager->m_config->getSpecialConfigValue(USER_SHADER_CATEGORY, "args", id.c_str()));
            auto transparent = std::any_cast<Hyprlang::INT>(
                g_pConfigManager->m_config->getSpecialConfigValue(USER_SHADER_CATEGORY, "introduces_transparency", id.c_str()));

            shaders.push_back(UserShader{
                .Id = id,
                .From = from,
                .Path = path,
                .Args = args,
                .IntroducesTransparency = transparent > 0,
            });
        }

        return shaders;
    }

    void RemoveConfigValues() {
        g_pConfigManager->m_config->removeSpecialConfigValue(USER_SHADER_CATEGORY, "from");
        g_pConfigManager->m_config->removeSpecialConfigValue(USER_SHADER_CATEGORY, "path");
        g_pConfigManager->m_config->removeSpecialConfigValue(USER_SHADER_CATEGORY, "args");
        g_pConfigManager->m_config->removeSpecialConfigValue(USER_SHADER_CATEGORY, "introduces_transparency");
        g_pConfigManager->m_config->removeSpecialCategory(USER_SHADER_CATEGORY);

        Desktop::Rule::windowEffects()->unregisterEffect(RuleShade);
    }


    template <typename... Args>
    static std::runtime_error Efmt(std::format_string<Args...> fmt, Args&&... args)
    {
        return std::runtime_error(std::format(fmt, std::forward<Args>(args)...));
    }

    void NotifyError(const std::string& err)
    {
        std::string msg = std::string("[Hypr-DarkWindow] ") + err;
        Log::logger->log(Log::ERR, msg);
        HyprlandAPI::addNotification(
            Handle,
            msg,
            CHyprColor(0xFFFF0000),
            25'000
        );
    }

    auto HandleError(auto f) {
        return [&](std::string args) {
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