#include "WindowInverter.h"

#include <hyprland/src/SharedDefs.hpp>


inline HANDLE PHANDLE = nullptr;

inline WindowInverter g_WindowInverter;

inline CFunctionHook* g_SetConfigValueHook;
inline std::vector<SWindowRule> g_WindowRulesBuildup;


APICALL EXPORT std::string PLUGIN_API_VERSION()
{
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle)
{
    PHANDLE = handle;

    g_WindowInverter.Init();

    HyprlandAPI::addConfigKeyword(
        handle, "plugin:dark_window:invert",
        [&](const std::string& cmd, const std::string& val) {
            g_WindowRulesBuildup.push_back(ParseRule(val));
        }
    );

    HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "render",
        [&](void* self, SCallbackInfo&, std::any data) {
            eRenderStage renderStage = std::any_cast<eRenderStage>(data);

            if (renderStage == eRenderStage::RENDER_PRE_WINDOW)
                g_WindowInverter.OnRenderWindowPre();
            if (renderStage == eRenderStage::RENDER_POST_WINDOW)
                g_WindowInverter.OnRenderWindowPost();
        }
    );
    HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "configReloaded",
        [&](void* self, SCallbackInfo&, std::any data) {
            g_WindowInverter.SetRules(std::move(g_WindowRulesBuildup));
            g_WindowRulesBuildup = {};
        }
    );
    HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "closeWindow",
        [&](void* self, SCallbackInfo&, std::any data) { 
            g_WindowInverter.OnWindowClose(std::any_cast<CWindow*>(data));
        }
    );

    HyprlandAPI::addDispatcher(PHANDLE, "invertwindow", [&](std::string args) {
        g_WindowInverter.ToggleInvert(g_pCompositor->getWindowByRegex(args));
    });
    HyprlandAPI::addDispatcher(PHANDLE, "invertactivewindow", [&](std::string args) {
        g_WindowInverter.ToggleInvert(g_pCompositor->m_pLastWindow);
    });


    for (const auto& event : { "openWindow", "fullscreen", "changeFloatingMode", "windowtitle", "moveWindow" })
    {
        HyprlandAPI::registerCallbackDynamic(
            PHANDLE, event,
            [&](void* self, SCallbackInfo&, std::any data) {
                g_WindowInverter.InvertIfMatches(std::any_cast<CWindow*>(data));
            }
        );
    }

    g_pConfigManager->m_bForceReload = true;

    return {
        "hypr-darkwindow",
        "Allows you to set dark mode for only specific Windows",
        "micha4w",
        "1.0.0"
    };
}

APICALL EXPORT void PLUGIN_EXIT()
{
    g_WindowInverter.Unload();
}

