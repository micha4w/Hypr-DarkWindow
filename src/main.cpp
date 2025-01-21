#include "WindowInverter.h"

#include <dlfcn.h>

#include <hyprlang.hpp>

#include "DecorationsWrapper.h"


inline HANDLE PHANDLE = nullptr;

inline WindowInverter g_WindowInverter;
inline std::mutex g_InverterMutex;

inline std::vector<SP<HOOK_CALLBACK_FN>> g_Callbacks;
CFunctionHook* g_getDataForHook;

// TODO remove deprecated
static void addDeprecatedEventListeners();


void* hkGetDataFor(void* thisptr, IHyprWindowDecoration* pDecoration, PHLWINDOW pWindow) {
    if (DecorationsWrapper* wrapper = dynamic_cast<DecorationsWrapper*>(pDecoration))
    {
        // Debug::log(LOG, "IGNORE: Decoration {}", (void*)pDecoration);
        pDecoration = wrapper->get();
    }

    return ((decltype(&hkGetDataFor))g_getDataForHook->m_pOriginal)(thisptr, pDecoration, pWindow);
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle)
{
    PHANDLE = handle;

    {
        std::lock_guard<std::mutex> lock(g_InverterMutex);
        g_WindowInverter.Init(PHANDLE);
    }

    HyprlandAPI::addConfigValue(PHANDLE, "plugin:darkwindow:ignore_decorations", Hyprlang::CConfigValue(Hyprlang::INT{ 0 }));
    HyprlandAPI::reloadConfig();

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
        }
    ));

    g_Callbacks.push_back(HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "configReloaded",
        [&](void* self, SCallbackInfo&, std::any data) {
            std::lock_guard<std::mutex> lock(g_InverterMutex);
            g_WindowInverter.Reload();
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
        PHANDLE, "windowUpdateRules",
        [&](void* self, SCallbackInfo&, std::any data) {
            std::lock_guard<std::mutex> lock(g_InverterMutex);
            g_WindowInverter.InvertIfMatches(std::any_cast<PHLWINDOW>(data));
        }
    ));

    static const auto METHOD = ([&] {
        auto all = HyprlandAPI::findFunctionsByName(PHANDLE, "getDataFor");
        auto found = std::find_if(all.begin(), all.end(), [](const SFunctionMatch& line) {
            return line.demangled.starts_with("CDecorationPositioner::getDataFor");
        });
        if (found != all.end())
            return std::optional(*found);
        else
            return std::optional<SFunctionMatch>();
    })();
    if (METHOD)
    {
        g_getDataForHook = HyprlandAPI::createFunctionHook(handle, METHOD->address, (void*)&hkGetDataFor);
        g_getDataForHook->hook();
    }
    else
    {
        Debug::log(WARN, "[DarkWindow] Failed to hook CDecorationPositioner::getDataFor, cannot ignore Decorations");
        g_WindowInverter.NoIgnoreDecorations();
    }

    HyprlandAPI::addDispatcherV2(PHANDLE, "invertwindow", [&](std::string args) {
        std::lock_guard<std::mutex> lock(g_InverterMutex);
        g_WindowInverter.ToggleInvert(g_pCompositor->getWindowByRegex(args));
        return SDispatchResult{};
    });
    HyprlandAPI::addDispatcherV2(PHANDLE, "invertactivewindow", [&](std::string args) {
        std::lock_guard<std::mutex> lock(g_InverterMutex);
        g_WindowInverter.ToggleInvert(g_pCompositor->m_pLastWindow.lock());
        return SDispatchResult{};
    });

    return {
        "Hypr-DarkWindow",
        "Allows you to set dark mode for only specific Windows",
        "micha4w",
        "2.0.0"
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
            { 0xFF'00'00'00 },
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
