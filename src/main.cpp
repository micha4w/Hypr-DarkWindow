#include <hyprland/src/render/pass/PassElement.hpp>

#define private public
#include <hyprland/src/render/pass/SurfacePassElement.hpp>
#undef private

#include "WindowInverter.h"

#include <dlfcn.h>

#include <hyprlang.hpp>


inline HANDLE PHANDLE = nullptr;

inline WindowInverter g_WindowInverter;
inline std::mutex g_InverterMutex;

inline std::vector<SP<HOOK_CALLBACK_FN>> g_Callbacks;
CFunctionHook* g_surfacePassDraw;

// TODO check out transformers

void hkSurfacePassDraw(CSurfacePassElement* thisptr, const CRegion& damage) {
    {
        std::lock_guard<std::mutex> lock(g_InverterMutex);
        g_WindowInverter.OnRenderWindowPre(thisptr->data.pWindow);
    }

    ((decltype(&hkSurfacePassDraw))g_surfacePassDraw->m_original)(thisptr, damage);

    {
        std::lock_guard<std::mutex> lock(g_InverterMutex);
        g_WindowInverter.OnRenderWindowPost();
    }
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle)
{
    PHANDLE = handle;

    {
        std::lock_guard<std::mutex> lock(g_InverterMutex);
        g_WindowInverter.Init();
    }

    g_Callbacks = {};
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

    const auto findFunction = [&](const std::string& className, const std::string& name) {
        auto all = HyprlandAPI::findFunctionsByName(PHANDLE, name);
        auto found = std::find_if(all.begin(), all.end(), [&](const SFunctionMatch& line) {
            return line.demangled.starts_with(className + "::" + name);
        });
        if (found != all.end())
            return std::optional(*found);
        else
            return std::optional<SFunctionMatch>();
    };

    static const auto pDraw = findFunction("CSurfacePassElement", "draw");
    if (!pDraw) throw std::runtime_error("Failed to find CSurfacePassElement::draw");
    g_surfacePassDraw = HyprlandAPI::createFunctionHook(handle, pDraw->address, (void*)&hkSurfacePassDraw);
    g_surfacePassDraw->hook();

    HyprlandAPI::addDispatcherV2(PHANDLE, "invertwindow", [&](std::string args) {
        std::lock_guard<std::mutex> lock(g_InverterMutex);
        g_WindowInverter.ToggleInvert(g_pCompositor->getWindowByRegex(args));
        return SDispatchResult{};
    });
    HyprlandAPI::addDispatcherV2(PHANDLE, "invertactivewindow", [&](std::string args) {
        std::lock_guard<std::mutex> lock(g_InverterMutex);
        g_WindowInverter.ToggleInvert(g_pCompositor->m_lastWindow.lock());
        return SDispatchResult{};
    });

    return {
        "Hypr-DarkWindow",
        "Allows you to set dark mode for only specific Windows",
        "micha4w",
        "3.0.1"
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
