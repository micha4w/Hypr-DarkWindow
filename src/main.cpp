#include "WindowInverter.h"

#include <dlfcn.h>

#include <hyprland/src/SharedDefs.hpp>
#include <hyprlang.hpp>


inline HANDLE PHANDLE = nullptr;

inline WindowInverter g_WindowInverter;
inline std::mutex g_InverterMutex;

inline std::vector<SP<HOOK_CALLBACK_FN>> g_Callbacks;

// TODO remove deprecated
static void addDeprecatedEventListeners();



APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle)
{
    PHANDLE = handle;

    {
        std::lock_guard<std::mutex> lock(g_InverterMutex);
        g_WindowInverter.Init();
        g_pConfigManager->m_bForceReload = true;
    }

    g_Callbacks = {};

    addDeprecatedEventListeners();

    g_Callbacks.push_back(HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "render",
        [&](void* self, SCallbackInfo&, std::any data) {
        std::lock_guard<std::mutex> lock(g_InverterMutex);
        eRenderStage renderStage = std::any_cast<eRenderStage>(data);

        if (renderStage == eRenderStage::RENDER_PRE_WINDOW)
            g_WindowInverter.OnRenderWindowPre();
        if (renderStage == eRenderStage::RENDER_POST_WINDOW)
            g_WindowInverter.OnRenderWindowPost();
    }));

    g_Callbacks.push_back(HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "configReloaded",
        [&](void* self, SCallbackInfo&, std::any data) {
        std::lock_guard<std::mutex> lock(g_InverterMutex);
        g_WindowInverter.Reload();
    }));
    g_Callbacks.push_back(HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "closeWindow",
        [&](void* self, SCallbackInfo&, std::any data) {
        std::lock_guard<std::mutex> lock(g_InverterMutex);
        g_WindowInverter.OnWindowClose(std::any_cast<PHLWINDOW>(data));
    }));
    g_Callbacks.push_back(HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "windowUpdateRules",
        [&](void* self, SCallbackInfo&, std::any data) {
        std::lock_guard<std::mutex> lock(g_InverterMutex);
        g_WindowInverter.InvertIfMatches(std::any_cast<PHLWINDOW>(data));
    }));



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

// TODO remove deprecated
inline static bool g_DidNotify = false;
Hyprlang::CParseResult onInvertKeyword(const char* COMMAND, const char* VALUE)
{
    if (!g_DidNotify) {
        g_DidNotify = true;
        HyprlandAPI::addNotification(
            PHANDLE,
            "[Hypr-DarkWindow] The darkwindow_invert keyword was removed in favor of windowrulev2s please check the GitHub for more info.",
            CColor(0xFF'00'00'00),
            10000
        );
    }
    return {};
}

// TODO remove deprecated
static void addDeprecatedEventListeners()
{
    HyprlandAPI::addConfigKeyword(
        PHANDLE, "darkwindow_invert",
        onInvertKeyword,
        { .allowFlags = false }
    );
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