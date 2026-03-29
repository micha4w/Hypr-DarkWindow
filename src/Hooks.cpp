#include "State.h"

HOOK_FUNCTION(Desktop::View::, CWindow, opaque,
    bool, (Desktop::View::CWindow* thisptr))
{
    auto shader = g.Manager.GetShaderForWindow(thisptr->m_self.lock());
    if (shader && shader->Transparency)
        // so Hyprland does not try to optimize away the drawing of the background
        return false;

    return original(thisptr);
}

HOOK_FUNCTION(Render::, CRenderPass, render,
    CRegion, (Render::CRenderPass* thisptr, const CRegion& damage_))
{
    for (auto& elData : thisptr->m_passElements)
    {
        if (CSurfacePassElement* s = dynamic_cast<CSurfacePassElement*>(elData->element.get()))
        {
            if (s->m_data.pWindow)
            {
                auto shaders = g.Manager.GetShaderForWindow(s->m_data.pWindow);

                if (shaders)
                {
                    // bool success = SaveTextureAsBMP(*s->m_data.texture, "/home/micha4w/Code/Linux/Hypr-DarkWindow/todo/" + s->m_data.pWindow->m_class + ".bmp");
                    if (shaders->Transparency && s->m_data.alpha >= 1)
                        // so the blur gets drawn
                        s->m_data.alpha = 0.999f;
                }
            }
        }
    }

    return original(thisptr, damage_);
}

HOOK_FUNCTION(Render::GL::, CHyprOpenGLImpl, renderTextureWithBlurInternal,
    void, (Render::GL::CHyprOpenGLImpl* thisptr, SP<Render::ITexture> tex, const CBox& box, const Render::GL::CHyprOpenGLImpl::STextureRenderData& data))
{
    g.RenderState.BlurredBG = data.blurredBG;
    original(thisptr, tex, box, data);
    g.RenderState.BlurredBG.reset();
}

HOOK_FUNCTION(Render::GL::, CHyprOpenGLImpl, renderTextureInternal,
    void, (Render::GL::CHyprOpenGLImpl* thisptr, SP<Render::ITexture> tex, const CBox& box, const Render::GL::CHyprOpenGLImpl::STextureRenderData& data))
{
    // so the blurred background does not get shaded
    g.RenderState.Ignore = g.RenderState.BlurredBG == tex;
    original(thisptr, tex, box, data);
    g.RenderState.Ignore = true;
}


HOOK_FUNCTION(Render::GL::, CHyprOpenGLImpl, getShaderVariant,
    WP<CShader>, (Render::GL::CHyprOpenGLImpl* thisptr, Render::ePreparedFragmentShader frag, Render::ShaderFeatureFlags features))
{
    if (g.RenderState.Ignore || frag != Render::SH_FRAG_SURFACE)
        return original(thisptr, frag, features);

    auto window = g_pHyprRenderer->renderData().currentWindow.lock();
    if (!window)
        return original(thisptr, frag, features);

    auto shaders = g.Manager.GetShaderForWindow(window);
    if (!shaders)
        return original(thisptr, frag, features);

    try
    {
        auto& variants = thisptr->m_shaders->fragVariants[frag];
        auto shader = shaders->Compiled->GetOrCreateVariant(features, [&] {
            std::string fragSrc = Render::g_pShaderLoader->getVariantSource(frag, features);
            std::string modifiedFragSrc = shaders->Compiled->EditShader(fragSrc);

            auto newShader = makeShared<CShader>();
            newShader->createProgram(thisptr->m_shaders->TEXVERTSRC, modifiedFragSrc, true, true);
            return newShader;
        });

        shader.SetUniforms(shaders->Args, g_pHyprRenderer->renderData().pMonitor.lock(), window);
        return shader.Shader;
    }
    catch (const std::exception& ex)
    {
        shaders->Compiled->FailedCompilation = true;
        g.NotifyError(std::string("Failed to apply custom shader: ") + ex.what());
        return original(thisptr, frag, features);
    }
}

#ifdef WATCH_SHADERS
HOOK_FUNCTION(, CConfigWatcher, setWatchList,
    void, (void* thisptr, const std::vector<std::string>& paths))
{
    std::vector<std::string> newPaths = paths;
    newPaths.insert(newPaths.end(), additionalWatchPaths.begin(), additionalWatchPaths.end());

    original(thisptr, newPaths);
}
#endif
