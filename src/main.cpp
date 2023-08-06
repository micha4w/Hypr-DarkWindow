#define WLR_USE_UNSTABLE

#include <cstring>

#include <hyprland/src/Compositor.hpp>

#include "HyprOverrides.h"
#include "CustomRules.h"


template<>
void std::swap(CShader& a, CShader& b)
{
    CShader c;
    std::memcpy(&c, &a, offsetof(CShader, output) + sizeof(CShader::output));
    std::memcpy(&a, &b, offsetof(CShader, output) + sizeof(CShader::output));
    std::memcpy(&b, &c, offsetof(CShader, output) + sizeof(CShader::output));

    // To stop destructor from yeeting the shader
    c.program = 0;
}


static HANDLE PHANDLE = nullptr;

APICALL EXPORT std::string PLUGIN_API_VERSION()
{
    return HYPRLAND_API_VERSION;
}

static ShaderHolder s_Shaders;
static bool s_ShadersSwapped = false;
static void onRenderStage(void* self, std::any data)
{
    eRenderStage renderStage = std::any_cast<eRenderStage>(data);

    if (renderStage == eRenderStage::RENDER_PRE_WINDOW)
    {
        if (g_pHyprOpenGL->m_pCurrentWindow && invertWindow(PHANDLE, *g_pHyprOpenGL->m_pCurrentWindow))
        {
            std::swap(s_Shaders.EXT, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shEXT);
            std::swap(s_Shaders.RGBA, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBA);
            std::swap(s_Shaders.RGBX, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBX);
            s_ShadersSwapped = true;
        }
    }
    else if (renderStage == eRenderStage::RENDER_POST_WINDOW)
    {
        if (s_ShadersSwapped)
        {
            std::swap(s_Shaders.EXT, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shEXT);
            std::swap(s_Shaders.RGBA, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBA);
            std::swap(s_Shaders.RGBX, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBX);
            s_ShadersSwapped = false;
        }
    }
}


APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle)
{
    PHANDLE = handle;

    RASSERT(eglMakeCurrent(wlr_egl_get_display(g_pCompositor->m_sWLREGL), EGL_NO_SURFACE, EGL_NO_SURFACE, wlr_egl_get_context(g_pCompositor->m_sWLREGL)),
        "Couldn't set current EGL!");
    initShaders(s_Shaders);
    RASSERT(eglMakeCurrent(wlr_egl_get_display(g_pCompositor->m_sWLREGL), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT),
        "Couldn't unset current EGL!");

    HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "render",
        [&](void* self, std::any data) { onRenderStage(self, data); });

    return {
        "Hypr-DarkWindow",
        "Allows you to set dark mode for only specific Windows",
        "micha4w",
        "1.0"
    };
}

APICALL EXPORT void PLUGIN_EXIT()
{
    if (s_ShadersSwapped)
    {
        std::swap(s_Shaders.EXT, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shEXT);
        std::swap(s_Shaders.RGBA, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBA);
        std::swap(s_Shaders.RGBX, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBX);
        s_ShadersSwapped = false;
    }

    RASSERT(eglMakeCurrent(wlr_egl_get_display(g_pCompositor->m_sWLREGL), EGL_NO_SURFACE, EGL_NO_SURFACE, wlr_egl_get_context(g_pCompositor->m_sWLREGL)),
        "Couldn't set current EGL!");

    s_Shaders.RGBA.destroy();
    s_Shaders.RGBX.destroy();
    s_Shaders.EXT.destroy();

    RASSERT(eglMakeCurrent(wlr_egl_get_display(g_pCompositor->m_sWLREGL), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT),
        "Couldn't unset current EGL!");
}
