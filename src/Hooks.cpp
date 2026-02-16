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

HOOK_FUNCTION(, CRenderPass, render,
    CRegion, (CRenderPass* thisptr, const CRegion& damage_))
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

HOOK_FUNCTION(, CHyprOpenGLImpl, renderTextureInternal,
    void, (CHyprOpenGLImpl* thisptr, SP<CTexture> tex, const CBox& box, const CHyprOpenGLImpl::STextureRenderData& data))
{
    g.RenderState.Ignore = true;
    if (!(tex == thisptr->m_renderData.pCurrentMonData->blurFB.getTexture()
        || tex == thisptr->m_renderData.pCurrentMonData->mirrorFB.getTexture()
        || tex == thisptr->m_renderData.pCurrentMonData->mirrorSwapFB.getTexture()))
    {
        // so the blurred texture does not get shaded
        g.RenderState.Ignore = false;
    }
    original(thisptr, tex, box, data);
}


HOOK_FUNCTION(, CHyprOpenGLImpl, getSurfaceShader,
    WP<CShader>, (CHyprOpenGLImpl* thisptr, uint8_t features))
{
    auto window = thisptr->m_renderData.currentWindow.lock();
    if (g.RenderState.Ignore || !window)
        return original(thisptr, features);

    auto shader = g.Manager.GetOrCreateShaderForWindow(window, features, [&] (ShaderInstance* shaders) {
        auto existing_it = thisptr->m_shaders->fragVariants.find(features);
        SP<CShader> existingShader = existing_it != thisptr->m_shaders->fragVariants.end() ? existing_it->second : nullptr;

        g.RenderState.Shader = shaders;
        if (existingShader)
            thisptr->m_shaders->fragVariants.erase(existing_it);

        Hyprutils::Utils::CScopeGuard _([&] {
            g.RenderState.Shader = {};
            if (existingShader)
                thisptr->m_shaders->fragVariants[features] = std::move(existingShader);
            else
                thisptr->m_shaders->fragVariants.erase(features);
        });

        return original(thisptr, features).lock();
    });

    if (shader) return shader;
    return original(thisptr, features);
}

HOOK_FUNCTION(, CShader, createProgram,
    bool, (CShader* thisptr, const std::string& vert, const std::string& frag, bool dynamic, bool silent))
{
    if (!g.RenderState.Shader)
        return original(thisptr, vert, frag, dynamic, silent);

    std::string modifiedFrag = (*g.RenderState.Shader)->Compiled->EditShader(frag);
    bool success = original(thisptr, vert, modifiedFrag, dynamic, silent);
    if (!success)
        Log::logger->log(Log::ERR, "Failed fragment source: {}", modifiedFrag);

    return true;
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
