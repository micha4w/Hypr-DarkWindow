#include <hyprland/src/render/pass/SurfacePassElement.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

#include "CustomShader.h"
#include "State.h"

static ShadedElement* getShaderForSurfacePass(CSurfacePassElement* element)
{
    if (element->m_data.pWindow)
        return g.Manager.GetShaderForElement(element->m_data.pWindow);
    if (element->m_data.pLS)
        return g.Manager.GetShaderForElement(element->m_data.pLS);

    return nullptr;
}

HOOK_FUNCTION(Desktop::View::, CWindow, opaque, bool, (Desktop::View::CWindow * thisptr))
{
    auto config = g.Manager.GetShaderForElement(thisptr->m_self.lock());
    if (config && config->ActiveShader->Transparency)
        // so Hyprland does not try to optimize away the drawing of the background
        return false;

    return original(thisptr);
}

HOOK_FUNCTION(Render::, CRenderPass, render, CRegion, (Render::CRenderPass * thisptr, const CRegion& damage_))
{
    for (auto& elData : thisptr->m_passElements)
    {
        if (auto s = dc<CSurfacePassElement*>(elData.element.get()))
        {
            auto config = getShaderForSurfacePass(s);
            if (config && config->ActiveShader->Transparency && s->m_data.alpha >= 1)
                // so the blur gets drawn, this needs to be set before render() executes
                s->m_data.alpha = 0.999f;
        }
    }

    return original(thisptr, damage_);
}

HOOK_FUNCTION(
    Render::,
    IElementRenderer,
    drawSurface,
    void,
    (void* thisptr, WP<CSurfacePassElement> element, const CRegion& damage)
)
{
    Hyprutils::Utils::CScopeGuard _state([&] {
        g.RenderState.ShaderConfig = nullptr;
        g.RenderState.Texture = nullptr;
    });
    g.RenderState.ShaderConfig = getShaderForSurfacePass(element.get());

    if (g.RenderState.ShaderConfig && g.RenderState.ShaderConfig->ActiveShader->Compiled->FailedCompilation)
        g.RenderState.ShaderConfig = nullptr;

    if (g.RenderState.ShaderConfig)
    {
        g.RenderState.Texture = element->m_data.texture;
        g.RenderState.Uniforms.MonitorScale = g_pHyprRenderer->renderData().pMonitor.lock()->m_scale;
        if (element->m_data.pWindow)
        {
            g.RenderState.Uniforms.WindowSize = element->m_data.pWindow->m_realSize->value();
            g.RenderState.Uniforms.WindowPos = element->m_data.pWindow->m_realPosition->value();
        }
        else
        {
            g.RenderState.Uniforms.WindowSize = element->m_data.pLS->m_realSize->value();
            g.RenderState.Uniforms.WindowPos = element->m_data.pLS->m_realPosition->value();
        }
    }

    original(thisptr, element, damage);
}

HOOK_FUNCTION(
    Render::GL::,
    CHyprOpenGLImpl,
    renderTextureInternal,
    void,
    (Render::GL::CHyprOpenGLImpl * thisptr,
     SP<Render::ITexture> tex,
     const CBox& box,
     const Render::GL::CHyprOpenGLImpl::STextureRenderData& data)
)
{
    // so the blurred background does not get shaded
    g.RenderState.Active = g.RenderState.ShaderConfig && g.RenderState.Texture == tex;
    Hyprutils::Utils::CScopeGuard _active([&] { g.RenderState.Active = false; });

    original(thisptr, tex, box, data);
}


HOOK_FUNCTION(
    Render::GL::,
    CHyprOpenGLImpl,
    getShaderVariant,
    WP<CShader>,
    (Render::GL::CHyprOpenGLImpl * thisptr, Render::ePreparedFragmentShader frag, Render::ShaderFeatureFlags features)
)
{
    if (!g.RenderState.Active || frag != Render::SH_FRAG_SURFACE)
        return original(thisptr, frag, features);

    auto shaders = g.RenderState.ShaderConfig->ActiveShader;
    try
    {
        auto shader = shaders->Compiled->GetOrCreateVariant(
            features,
            [&]
            {
                std::string fragSrc = Render::g_pShaderLoader->getVariantSource(frag, features);
                std::string modifiedFragSrc = shaders->Compiled->EditShader(fragSrc);

                auto newShader = makeShared<CShader>();
                newShader->createProgram(thisptr->m_shaders->TEXVERTSRC, modifiedFragSrc, true, true);
                return newShader;
            }
        );

        shader.SetUniforms(*g.RenderState.ShaderConfig, g.RenderState.Uniforms);
        return shader.Shader;
    }
    catch (const std::exception& ex)
    {
        g.NotifyError(std::string("Failed to apply custom shader: ") + ex.what());
        return original(thisptr, frag, features);
    }
}