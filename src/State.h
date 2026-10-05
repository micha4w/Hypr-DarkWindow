#pragma once

#include <dlfcn.h>

#include <any>
#include <cstring>
#include <hyprutils/string/ConstVarList.hpp>
#include <hyprutils/string/String.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>
#include <optional>
#include <sstream>
#include <type_traits>
#include <vector>

#include "CustomShader.h"

// All hyprland includes are in this file so the private overwriting works correctly
#define private   public
#define protected public
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/lua/bindings/LuaBindingsInternal.hpp>
#include <hyprland/src/config/shared/actions/ConfigActions.hpp>
#include <hyprland/src/config/shared/inotify/ConfigWatcher.hpp>
#include <hyprland/src/config/values/types/StringValue.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/state/ViewState.hpp>
#include <hyprland/src/desktop/view/window/WindowPresentation.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/pointer/PointerManager.hpp>
#include <hyprland/src/render/ElementRenderer.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/pass/Pass.hpp>
#include <hyprland/src/render/pass/PassElement.hpp>
#include <hyprland/src/render/pass/SurfacePassElement.hpp>
#undef protected
#undef private

#include "LuaCallbacks.h"
#include "ShadeManager.h"


#define HOOK_FUNCTION(ns, className, methodName, retType, args)                                     \
    namespace _ns_##className_##methodName                                                          \
    {                                                                                               \
        retType hook args;                                                                          \
        retType(*trampoline) args = nullptr;                                                        \
        auto _init = []<class R, class T, class... A>(R (*)(T*, A...))                              \
        {                                                                                           \
            using M = std::conditional_t<std::is_const_v<T>, R (T::*)(A...) const, R (T::*)(A...)>; \
            auto pmf = static_cast<M>(&ns className::methodName);                                   \
                                                                                                    \
            void* original;                                                                         \
            memcpy(&original, &pmf, sizeof(original));                                              \
                                                                                                    \
            g.Hooks.push_back(                                                                      \
                {                                                                                   \
                    .name = #ns #className "::" #methodName,                                        \
                    .original = original,                                                           \
                    .trampolinePtr = (void**) &trampoline,                                          \
                    .hookFunc = (void*) hook,                                                       \
                }                                                                                   \
            );                                                                                      \
            return true;                                                                            \
        }(hook);                                                                                    \
    }                                                                                               \
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
                "Hypr-DarkWindow", "version mismatch, built against {}, running compositor {}", CLIENT_HASH, COMPOSITOR_HASH
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
        std::string name;
        void* original;
        void** trampolinePtr;
        void* hookFunc;
        CFunctionHook* hypr;
    };
    std::vector<Hook> Hooks;

    void HookFunctions()
    {
        for (auto& hook : Hooks)
        {
            std::string name = hook.name;
            Dl_info info;
            if (dladdr(hook.original, &info) && info.dli_sname)
                name += " (" + std::string(info.dli_sname) + ")";

            Log::logger->log(Log::INFO, "Hypr-DarkWindow", "Hooking {} at {}", name, hook.original);

            hook.hypr = HyprlandAPI::createFunctionHook(Handle, hook.original, hook.hookFunc);
            if (!hook.hypr->hook())
                throw Efmt("Failed to hook {}", name);

            *hook.trampolinePtr = hook.hypr->m_original;
        }
    };

    inline static const char* LOAD_SHADERS_KEY = "plugin:darkwindow:load_shaders";

    void AddConfigValues()
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

    void RemoveConfigValues()
    {
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
        Log::logger->log(Log::ERR, "Hypr-DarkWindow", err);
        HyprlandAPI::addNotification(Handle, "[Hypr-DarkWindow] " + err, CHyprColor(0xFFFF0000), 25'000);
    }

    auto HandleError(auto f)
    {
        return [this, f](std::string args)
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
