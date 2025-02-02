#include "WindowInverter.h"

#include <dlfcn.h>

#include <hyprlang.hpp>

#include "DecorationsWrapper.h"


inline HANDLE PHANDLE = nullptr;

inline WindowInverter g_WindowInverter;
inline std::mutex g_InverterMutex;

inline std::vector<SP<HOOK_CALLBACK_FN>> g_Callbacks;
CFunctionHook* g_getDataForHook, * g_renderTexture, * g_renderTextureWithBlur;

// TODO check out transformers

void hkRenderTexture(void* thisptr, SP<CTexture> tex, CBox* pBox, float alpha, int round, float roundingPower, bool discardActive, bool allowCustomUV) {
    {
        std::lock_guard<std::mutex> lock(g_InverterMutex);
        g_WindowInverter.OnRenderWindowPre();
    }

    ((decltype(&hkRenderTexture))g_renderTexture->m_pOriginal)(thisptr, tex, pBox, alpha, round, roundingPower, discardActive, allowCustomUV);

    {
        std::lock_guard<std::mutex> lock(g_InverterMutex);
        g_WindowInverter.OnRenderWindowPost();
    }
}

void hkRenderTextureWithBlur(void* thisptr, SP<CTexture> tex, CBox* pBox, float a, SP<CWLSurfaceResource> pSurface, int round, float roundingPower, bool blockBlurOptimization, float blurA, float overallA) {
        {
            std::lock_guard<std::mutex> lock(g_InverterMutex);
            g_WindowInverter.OnRenderWindowPre();
        }

        ((decltype(&hkRenderTextureWithBlur))g_renderTextureWithBlur->m_pOriginal)(thisptr, tex, pBox, a, pSurface, round, roundingPower, blockBlurOptimization, blurA, overallA);

        {
            std::lock_guard<std::mutex> lock(g_InverterMutex);
            g_WindowInverter.OnRenderWindowPost();
        }
}

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

    static const auto pRenderTexture = findFunction("CHyprOpenGLImpl", "renderTexture");
    if (!pRenderTexture) throw std::runtime_error("Failed to find CHyprOpenGLImpl::renderTexture");
    g_renderTexture = HyprlandAPI::createFunctionHook(handle, pRenderTexture->address, (void*)&hkRenderTexture);
    g_renderTexture->hook();

    static const auto pRenderTextureWithBlur = findFunction("CHyprOpenGLImpl", "renderTextureWithBlur");
    if (!pRenderTextureWithBlur) throw std::runtime_error("Failed to find CHyprOpenGLImpl::renderTextureWithBlur");
    g_renderTextureWithBlur = HyprlandAPI::createFunctionHook(handle, pRenderTextureWithBlur->address, (void*)&hkRenderTextureWithBlur);
    g_renderTextureWithBlur->hook();

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
        "3.0.0"
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
