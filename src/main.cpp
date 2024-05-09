#include "WindowInverter.h"

#include <hyprland/src/SharedDefs.hpp>
#include <hyprlang.hpp>


inline HANDLE PHANDLE = nullptr;

inline WindowInverter g_WindowInverter;
inline std::mutex g_InverterMutex;

inline CFunctionHook* g_SetConfigValueHook;
inline std::vector<SWindowRule> g_WindowRulesBuildup;
inline std::vector<SP<HOOK_CALLBACK_FN>> g_Callbacks;
inline PHLWINDOWREF g_LastActiveWindow;


Hyprlang::CParseResult onInvertKeyword(const char* COMMAND, const char* VALUE)
{
    Hyprlang::CParseResult res;
    try
    {
        g_WindowRulesBuildup.push_back(ParseRule(VALUE));
    }
    catch (const std::exception& ex)
    {
        res.setError(ex.what());
    }
    return res;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle)
{
    PHANDLE = handle;

    {
        std::lock_guard<std::mutex> lock(g_InverterMutex);
        g_WindowInverter.Init();
        g_pConfigManager->m_bForceReload = true;
    }

    using PCONFIGHANDLERFUNC = Hyprlang::CParseResult(*)(const char* COMMAND, const char* VALUE);

    g_Callbacks = {};
    HyprlandAPI::addConfigKeyword(
        handle, "darkwindow_invert",
        onInvertKeyword,
        { .allowFlags = false }
    );

    g_Callbacks.push_back(HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "render",
        [&](void* self, SCallbackInfo&, std::any data) {
            std::lock_guard<std::mutex> lock(g_InverterMutex);
            eRenderStage renderStage = std::any_cast<eRenderStage>(data);

            if (renderStage == eRenderStage::RENDER_PRE_WINDOW)
                g_WindowInverter.OnRenderWindowPre();
            if (renderStage == eRenderStage::RENDER_POST_WINDOW)
                g_WindowInverter.OnRenderWindowPost();
        }
    ));
    g_Callbacks.push_back(HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "configReloaded",
        [&](void* self, SCallbackInfo&, std::any data) {
            std::lock_guard<std::mutex> lock(g_InverterMutex);
            g_WindowInverter.SetRules(std::move(g_WindowRulesBuildup));
            g_WindowRulesBuildup = {};
        }
    ));
    g_Callbacks.push_back(HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "closeWindow",
        [&](void* self, SCallbackInfo&, std::any data) {
            std::lock_guard<std::mutex> lock(g_InverterMutex);
            g_WindowInverter.OnWindowClose(std::any_cast<PHLWINDOW>(data));
        }
    ));

    g_Callbacks.push_back(HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "moveWindow",
        [&](void* self, SCallbackInfo&, std::any data) {
            std::lock_guard<std::mutex> lock(g_InverterMutex);
            g_WindowInverter.InvertIfMatches(std::any_cast<PHLWINDOW>(std::any_cast<std::vector<std::any>>(data)[0]));
        }
    ));

    g_LastActiveWindow = g_pCompositor->m_pLastWindow;
    g_Callbacks.push_back(HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "activeWindow",
        [&](void* self, SCallbackInfo&, std::any data) {
            std::lock_guard<std::mutex> lock(g_InverterMutex);
            if (!g_LastActiveWindow.expired())
                g_WindowInverter.InvertIfMatches(g_LastActiveWindow.lock());

            g_WindowInverter.InvertIfMatches(std::any_cast<PHLWINDOW>(data));
            g_LastActiveWindow = g_pCompositor->m_pLastWindow;
        }
    ));

    for (const auto& event : { "openWindow", "fullscreen", "changeFloatingMode", "windowtitle" })
    {
        g_Callbacks.push_back(HyprlandAPI::registerCallbackDynamic(
            PHANDLE, event,
            [&](void* self, SCallbackInfo&, std::any data) {
                std::lock_guard<std::mutex> lock(g_InverterMutex);
                g_WindowInverter.InvertIfMatches(std::any_cast<PHLWINDOW>(data));
            }
        ));
    }


    HyprlandAPI::addDispatcher(PHANDLE, "invertwindow", [&](std::string args) {
        std::lock_guard<std::mutex> lock(g_InverterMutex);
        g_WindowInverter.ToggleInvert(g_pCompositor->getWindowByRegex(args));
    });
    HyprlandAPI::addDispatcher(PHANDLE, "invertactivewindow", [&](std::string args) {
        std::lock_guard<std::mutex> lock(g_InverterMutex);
        g_WindowInverter.ToggleInvert(g_pCompositor->m_pLastWindow.lock());
    });

    return {
        "hypr-darkwindow",
        "Allows you to set dark mode for only specific Windows",
        "micha4w",
        "1.0.0"
    };
}

APICALL EXPORT void PLUGIN_EXIT()
{
    std::lock_guard<std::mutex> lock(g_InverterMutex);
    g_Callbacks = {};
    g_WindowInverter.Unload();
}

APICALL EXPORT std::string PLUGIN_API_VERSION()
{
    return HYPRLAND_API_VERSION;
}