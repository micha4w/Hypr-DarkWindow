#include "Helpers.h"

#include <exception>
#include <unordered_set>

#include <hyprland/src/Compositor.hpp>
#include <hyprutils/string/String.hpp>


void ShaderHolder::Init()
{
    g_pHyprRenderer->makeEGLCurrent();

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
    RGBA.applyTint            = glGetUniformLocation(prog, "applyTint");
    RGBA.tint                 = glGetUniformLocation(prog, "tint");
    RGBA_Invert               = glGetUniformLocation(prog, "doInvert"); 

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
    RGBX.applyTint            = glGetUniformLocation(prog, "applyTint");
    RGBX.tint                 = glGetUniformLocation(prog, "tint");
    RGBX_Invert               = glGetUniformLocation(prog, "doInvert"); 

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
    EXT.applyTint            = glGetUniformLocation(prog, "applyTint");
    EXT.tint                 = glGetUniformLocation(prog, "tint");
    EXT_Invert               = glGetUniformLocation(prog, "doInvert"); 

    g_pHyprRenderer->unsetEGL();
}

void ShaderHolder::Destroy()
{
    g_pHyprRenderer->makeEGLCurrent();

    RGBA.destroy();
    RGBX.destroy();
    EXT.destroy();

    g_pHyprRenderer->unsetEGL();
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
        Debug::log(ERR, "Error compiling shader: {}", infoLog);
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
