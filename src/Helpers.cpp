#include "Helpers.h"

#include <exception>

#include <hyprland/src/Compositor.hpp>


SWindowRule ParseRule(const std::string& value)
{
    SWindowRule rule;
    rule.v2 = true;
    rule.szRule = "invert";
    rule.szValue = value;

    const auto TITLEPOS = value.find("title:");
    const auto CLASSPOS = value.find("class:");
    const auto X11POS = value.find("xwayland:");
    const auto FLOATPOS = value.find("floating:");
    const auto FULLSCREENPOS = value.find("fullscreen:");
    const auto PINNEDPOS = value.find("pinned:");
    const auto WORKSPACEPOS = value.find("workspace:");

    if (TITLEPOS == std::string::npos && CLASSPOS == std::string::npos && X11POS == std::string::npos && FLOATPOS == std::string::npos && FULLSCREENPOS == std::string::npos &&
        PINNEDPOS == std::string::npos && WORKSPACEPOS == std::string::npos) {
        Debug::log(ERR, "Invalid rulev2 syntax: %s", value.c_str());
        return rule;
    }

    auto extract = [&](size_t pos) -> std::string {
        std::string result;
        result = value.substr(pos);

        size_t min = 999999;
        if (TITLEPOS > pos && TITLEPOS < min)
            min = TITLEPOS;
        if (CLASSPOS > pos && CLASSPOS < min)
            min = CLASSPOS;
        if (X11POS > pos && X11POS < min)
            min = X11POS;
        if (FLOATPOS > pos && FLOATPOS < min)
            min = FLOATPOS;
        if (FULLSCREENPOS > pos && FULLSCREENPOS < min)
            min = FULLSCREENPOS;
        if (PINNEDPOS > pos && PINNEDPOS < min)
            min = PINNEDPOS;
        if (WORKSPACEPOS > pos && WORKSPACEPOS < min)
            min = PINNEDPOS;

        result = result.substr(0, min - pos);

        result = removeBeginEndSpacesTabs(result);

        if (result.back() == ',')
            result.pop_back();

        return result;
    };

    if (CLASSPOS != std::string::npos)
        rule.szClass = extract(CLASSPOS + 6);

    if (TITLEPOS != std::string::npos)
        rule.szTitle = extract(TITLEPOS + 6);

    if (X11POS != std::string::npos)
        rule.bX11 = extract(X11POS + 9) == "1" ? 1 : 0;

    if (FLOATPOS != std::string::npos)
        rule.bFloating = extract(FLOATPOS + 9) == "1" ? 1 : 0;

    if (FULLSCREENPOS != std::string::npos)
        rule.bFullscreen = extract(FULLSCREENPOS + 11) == "1" ? 1 : 0;

    if (PINNEDPOS != std::string::npos)
        rule.bPinned = extract(PINNEDPOS + 7) == "1" ? 1 : 0;

    if (WORKSPACEPOS != std::string::npos)
        rule.szWorkspace = extract(WORKSPACEPOS + 10);

    return rule;
}

void ShaderHolder::Init()
{
    RASSERT(eglMakeCurrent(wlr_egl_get_display(g_pCompositor->m_sWLREGL), EGL_NO_SURFACE, EGL_NO_SURFACE, wlr_egl_get_context(g_pCompositor->m_sWLREGL)),
        "Couldn't set current EGL!");

    GLuint prog               = CreateProgram(TEXVERTSRC, TEXFRAGSRCRGBA_DARK);
    RGBA.program              = prog;
    RGBA.proj                 = glGetUniformLocation(prog, "proj");
    RGBA.tex                  = glGetUniformLocation(prog, "tex");
    RGBA.alpha                = glGetUniformLocation(prog, "alpha");
    RGBA.texAttrib            = glGetAttribLocation(prog, "texcoord");
    RGBA.posAttrib            = glGetAttribLocation(prog, "pos");
    RGBA.discardOpaque        = glGetUniformLocation(prog, "discardOpaque");
    RGBA.discardAlpha         = glGetUniformLocation(prog, "discardAlpha");
    RGBA.discardAlphaValue    = glGetUniformLocation(prog, "discardAlphaValue");
    RGBA.topLeft              = glGetUniformLocation(prog, "topLeft");
    RGBA.fullSize             = glGetUniformLocation(prog, "fullSize");
    RGBA.radius               = glGetUniformLocation(prog, "radius");
    RGBA.primitiveMultisample = glGetUniformLocation(prog, "primitiveMultisample");
    RGBA.applyTint            = glGetUniformLocation(prog, "applyTint");
    RGBA.tint                 = glGetUniformLocation(prog, "tint");

    prog                      = CreateProgram(TEXVERTSRC, TEXFRAGSRCRGBX_DARK);
    RGBX.program              = prog;
    RGBX.tex                  = glGetUniformLocation(prog, "tex");
    RGBX.proj                 = glGetUniformLocation(prog, "proj");
    RGBX.alpha                = glGetUniformLocation(prog, "alpha");
    RGBX.texAttrib            = glGetAttribLocation(prog, "texcoord");
    RGBX.posAttrib            = glGetAttribLocation(prog, "pos");
    RGBX.discardOpaque        = glGetUniformLocation(prog, "discardOpaque");
    RGBX.discardAlpha         = glGetUniformLocation(prog, "discardAlpha");
    RGBX.discardAlphaValue    = glGetUniformLocation(prog, "discardAlphaValue");
    RGBX.topLeft              = glGetUniformLocation(prog, "topLeft");
    RGBX.fullSize             = glGetUniformLocation(prog, "fullSize");
    RGBX.radius               = glGetUniformLocation(prog, "radius");
    RGBX.primitiveMultisample = glGetUniformLocation(prog, "primitiveMultisample");
    RGBX.applyTint            = glGetUniformLocation(prog, "applyTint");
    RGBX.tint                 = glGetUniformLocation(prog, "tint");

    prog                     = CreateProgram(TEXVERTSRC, TEXFRAGSRCEXT_DARK);
    EXT.program              = prog;
    EXT.tex                  = glGetUniformLocation(prog, "tex");
    EXT.proj                 = glGetUniformLocation(prog, "proj");
    EXT.alpha                = glGetUniformLocation(prog, "alpha");
    EXT.posAttrib            = glGetAttribLocation(prog, "pos");
    EXT.texAttrib            = glGetAttribLocation(prog, "texcoord");
    EXT.discardOpaque        = glGetUniformLocation(prog, "discardOpaque");
    EXT.discardAlpha         = glGetUniformLocation(prog, "discardAlpha");
    EXT.discardAlphaValue    = glGetUniformLocation(prog, "discardAlphaValue");
    EXT.topLeft              = glGetUniformLocation(prog, "topLeft");
    EXT.fullSize             = glGetUniformLocation(prog, "fullSize");
    EXT.radius               = glGetUniformLocation(prog, "radius");
    EXT.primitiveMultisample = glGetUniformLocation(prog, "primitiveMultisample");
    EXT.applyTint            = glGetUniformLocation(prog, "applyTint");
    EXT.tint                 = glGetUniformLocation(prog, "tint");

    RASSERT(eglMakeCurrent(wlr_egl_get_display(g_pCompositor->m_sWLREGL), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT),
        "Couldn't unset current EGL!");
}

void ShaderHolder::Destroy()
{
    RASSERT(eglMakeCurrent(wlr_egl_get_display(g_pCompositor->m_sWLREGL), EGL_NO_SURFACE, EGL_NO_SURFACE, wlr_egl_get_context(g_pCompositor->m_sWLREGL)),
        "Couldn't set current EGL!");

    RGBA.destroy();
    RGBX.destroy();
    EXT.destroy();

    RASSERT(eglMakeCurrent(wlr_egl_get_display(g_pCompositor->m_sWLREGL), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT),
        "Couldn't unset current EGL!");
}

GLuint ShaderHolder::CompileShader(const GLuint& type, std::string src)
{
    auto shader = glCreateShader(type);

    auto shaderSource = src.c_str();

    glShaderSource(shader, 1, (const GLchar**)&shaderSource, nullptr);
    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        Debug::log(ERR, "Error compiling shader: %s", infoLog);
        throw std::runtime_error(std::string("Error compiling shader: ") + infoLog);
    }

    return shader;
}

GLuint ShaderHolder::CreateProgram(const std::string& vert, const std::string& frag)
{
    auto vertCompiled = CompileShader(GL_VERTEX_SHADER, vert);
    auto fragCompiled = CompileShader(GL_FRAGMENT_SHADER, frag);

    auto prog = glCreateProgram();
    glAttachShader(prog, vertCompiled);
    glAttachShader(prog, fragCompiled);
    glLinkProgram(prog);

    glDetachShader(prog, vertCompiled);
    glDetachShader(prog, fragCompiled);
    glDeleteShader(vertCompiled);
    glDeleteShader(fragCompiled);

    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char infoLog[512];
        glGetShaderInfoLog(prog, 512, NULL, infoLog);
        Debug::log(ERR, "Error linking shader: %s", infoLog);
        throw std::runtime_error(std::string("Error linking shader: ") + infoLog);
    }

    return prog;
}
