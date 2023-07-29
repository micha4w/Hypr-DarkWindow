#define WLR_USE_UNSTABLE

#include <hyprland/src/Compositor.hpp>

#include "HyprOverrides.h"

template<>
void std::swap(CShader& a, CShader& b) {
    std::swap(a.program, b.program);
    std::swap(a.proj, b.proj);
    std::swap(a.color, b.color);
    std::swap(a.tex, b.tex);
    std::swap(a.alpha, b.alpha);
    std::swap(a.posAttrib, b.posAttrib);
    std::swap(a.texAttrib, b.texAttrib);
    std::swap(a.discardOpaque, b.discardOpaque);
    std::swap(a.discardAlpha, b.discardAlpha);
    std::swap(a.discardAlphaValue, b.discardAlphaValue);
    std::swap(a.topLeft, b.topLeft);
    std::swap(a.bottomRight, b.bottomRight);
    std::swap(a.fullSize, b.fullSize);
    std::swap(a.fullSizeUntransformed, b.fullSizeUntransformed);
    std::swap(a.radius, b.radius);
    std::swap(a.primitiveMultisample, b.primitiveMultisample);
    std::swap(a.thick, b.thick);
    std::swap(a.halfpixel, b.halfpixel);
    std::swap(a.range, b.range);
    std::swap(a.shadowPower, b.shadowPower);
    std::swap(a.applyTint, b.applyTint);
    std::swap(a.tint, b.tint);
    std::swap(a.gradient, b.gradient);
    std::swap(a.gradientLength, b.gradientLength);
    std::swap(a.angle, b.angle);
    std::swap(a.time, b.time);
    std::swap(a.distort, b.distort);
    std::swap(a.output, b.output);
}

template<typename Tag, typename Tag::type M>
struct Rob {
    friend typename Tag::type get(Tag) {
        return M;
    }
};

struct ScreenShaderAccessor {
    using type = CShader CHyprOpenGLImpl::*;
    friend type get(ScreenShaderAccessor);
};

struct ApplyShaderAccessor {
    using type = bool CHyprOpenGLImpl::*;
    friend type get(ApplyShaderAccessor);
};

template struct Rob<ScreenShaderAccessor, &CHyprOpenGLImpl::m_sFinalScreenShader>;
template struct Rob<ApplyShaderAccessor, &CHyprOpenGLImpl::m_bApplyFinalShader>;


inline HANDLE PHANDLE = nullptr;

APICALL EXPORT std::string PLUGIN_API_VERSION()
{
    return HYPRLAND_API_VERSION;
}

static inline ShaderHolder s_Shaders;
static inline bool s_ShadersSwapped = false;
static void onRenderStage(void* self, std::any data) {

    eRenderStage renderStage = std::any_cast<eRenderStage>(data);

    if (renderStage == eRenderStage::RENDER_PRE_WINDOW) {
        CWindow* drawnWindow = g_pHyprOpenGL->m_pCurrentWindow;
        if (drawnWindow->m_szInitialClass == "Alacritty") {
            if (!s_Shaders.RGBA.program)
                // Shader initialization needs to be ran in the correct OpenGL context
                initShaders(s_Shaders);

            std::swap(s_Shaders.EXT, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shEXT);
            std::swap(s_Shaders.RGBA, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBA);
            std::swap(s_Shaders.RGBX, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBX);
            s_ShadersSwapped = true;
        }
    }
    else if (renderStage == eRenderStage::RENDER_POST_WINDOW) {
        if (s_ShadersSwapped) {
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

    HyprlandAPI::registerCallbackDynamic(
        PHANDLE, "render",
        [&](void* self, std::any data) { onRenderStage(self, data); });

    return { "Hypr-DarkWindow",
             "Allows you to set dark mode for only specific Windows", "micha4w",
             "1.0" };
}

APICALL EXPORT void PLUGIN_EXIT() {
    if (s_ShadersSwapped) {
        std::swap(s_Shaders.EXT, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shEXT);
        std::swap(s_Shaders.RGBA, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBA);
        std::swap(s_Shaders.RGBX, g_pHyprOpenGL->m_RenderData.pCurrentMonData->m_shRGBX);
        s_ShadersSwapped = false;
    }

    s_Shaders.RGBA.destroy();
    s_Shaders.RGBX.destroy();
    s_Shaders.EXT.destroy();
}
