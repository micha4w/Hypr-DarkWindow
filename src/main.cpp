#include "WindowInverter.h"


inline HANDLE PHANDLE = nullptr;

inline WindowInverter g_WindowInverter;

inline CFunctionHook* g_SetConfigValueHook;
inline std::vector<SWindowRule> g_WindowRulesBuildup;


void hkConfigSetValueSafe(void* thisptr, const std::string& COMMAND, const std::string& VALUE)
{ 
    if (COMMAND == "plugin:dark_window:invert")
    {
        g_WindowRulesBuildup.push_back(ParseRule(VALUE));
        // return;
    }

    using OriginalFunc = void (*)(void*, const std::string&, const std::string&);
    ((OriginalFunc) g_SetConfigValueHook->m_pOriginal)(thisptr, COMMAND, VALUE);
}

APICALL EXPORT std::string PLUGIN_API_VERSION()
{
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle)
{
    PHANDLE = handle;

    g_WindowInverter.Init();

    static const auto METHODS = HyprlandAPI::findFunctionsByName(PHANDLE, "configSetValueSafe");
    g_SetConfigValueHook = HyprlandAPI::createFunctionHook(handle, METHODS[0].address, (void*)&hkConfigSetValueSafe);
    g_SetConfigValueHook->hook();

    HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "render",
        [&](void* self, std::any data) {
            eRenderStage renderStage = std::any_cast<eRenderStage>(data);

            if (renderStage == eRenderStage::RENDER_PRE_WINDOW)
                g_WindowInverter.OnRenderWindowPre();
            if (renderStage == eRenderStage::RENDER_POST_WINDOW)
                g_WindowInverter.OnRenderWindowPost();
        }
    );
    HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "configReloaded",
        [&](void* self, std::any data) {
            g_WindowInverter.SetRules(std::move(g_WindowRulesBuildup));
            g_WindowRulesBuildup = {};
        }
    );
    HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "closeWindow",
        [&](void* self, std::any data) { 
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
            [&](void* self, std::any data) {
                g_WindowInverter.InvertIfMatches(std::any_cast<CWindow*>(data));
            }
        );
    }

    g_pConfigManager->m_bForceReload = true;

    return {
        "Hypr-DarkWindow",
        "Allows you to set dark mode for only specific Windows",
        "micha4w",
        "1.0"
    };
}

APICALL EXPORT void PLUGIN_EXIT()
{
    g_WindowInverter.Unload();
}
