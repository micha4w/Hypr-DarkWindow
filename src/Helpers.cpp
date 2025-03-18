#include "Helpers.h"

#include <exception>
#include <unordered_set>

#include <hyprland/src/Compositor.hpp>
#include <hyprutils/string/String.hpp>


void ShaderHolder::Init()
{
    g_pHyprRenderer->makeEGLCurrent();

    GLuint prog               = CreateProgram(TEXVERTSRC, TEXFRAGSRCCM_DARK);
    CM.program                = prog;
    CM.proj                   = glGetUniformLocation(prog, "proj");
    CM.tex                    = glGetUniformLocation(prog, "tex");
    CM.texType                = glGetUniformLocation(prog, "texType");
    CM.sourceTF               = glGetUniformLocation(prog, "sourceTF");
    CM.targetTF               = glGetUniformLocation(prog, "targetTF");
    CM.sourcePrimaries        = glGetUniformLocation(prog, "sourcePrimaries");
    CM.targetPrimaries        = glGetUniformLocation(prog, "targetPrimaries");
    CM.maxLuminance           = glGetUniformLocation(prog, "maxLuminance");
    CM.dstMaxLuminance        = glGetUniformLocation(prog, "dstMaxLuminance");
    CM.dstRefLuminance        = glGetUniformLocation(prog, "dstRefLuminance");
    CM.sdrSaturation          = glGetUniformLocation(prog, "sdrSaturation");
    CM.sdrBrightness          = glGetUniformLocation(prog, "sdrBrightnessMultiplier");
    CM.alphaMatte             = glGetUniformLocation(prog, "texMatte");
    CM.alpha                  = glGetUniformLocation(prog, "alpha");
    CM.texAttrib              = glGetAttribLocation(prog, "texcoord");
    CM.matteTexAttrib         = glGetAttribLocation(prog, "texcoordMatte");
    CM.posAttrib              = glGetAttribLocation(prog, "pos");
    CM.discardOpaque          = glGetUniformLocation(prog, "discardOpaque");
    CM.discardAlpha           = glGetUniformLocation(prog, "discardAlpha");
    CM.discardAlphaValue      = glGetUniformLocation(prog, "discardAlphaValue");
    CM.topLeft                = glGetUniformLocation(prog, "topLeft");
    CM.fullSize               = glGetUniformLocation(prog, "fullSize");
    CM.radius                 = glGetUniformLocation(prog, "radius");
    CM.roundingPower          = glGetUniformLocation(prog, "roundingPower");
    CM.applyTint              = glGetUniformLocation(prog, "applyTint");
    CM.tint                   = glGetUniformLocation(prog, "tint");
    CM.useAlphaMatte          = glGetUniformLocation(prog, "useAlphaMatte");

    prog                      = CreateProgram(TEXVERTSRC, TEXFRAGSRCRGBA_DARK);
    RGBA.program              = prog;
    RGBA.proj                 = glGetUniformLocation(prog, "proj");
    RGBA.tex                  = glGetUniformLocation(prog, "tex");
    RGBA.alphaMatte           = glGetUniformLocation(prog, "texMatte");
    RGBA.alpha                = glGetUniformLocation(prog, "alpha");
    RGBA.texAttrib            = glGetAttribLocation(prog, "texcoord");
    RGBA.matteTexAttrib       = glGetAttribLocation(prog, "texcoordMatte");
    RGBA.posAttrib            = glGetAttribLocation(prog, "pos");
    RGBA.discardOpaque        = glGetUniformLocation(prog, "discardOpaque");
    RGBA.discardAlpha         = glGetUniformLocation(prog, "discardAlpha");
    RGBA.discardAlphaValue    = glGetUniformLocation(prog, "discardAlphaValue");
    RGBA.topLeft              = glGetUniformLocation(prog, "topLeft");
    RGBA.fullSize             = glGetUniformLocation(prog, "fullSize");
    RGBA.radius               = glGetUniformLocation(prog, "radius");
    RGBA.roundingPower        = glGetUniformLocation(prog, "roundingPower");
    RGBA.applyTint            = glGetUniformLocation(prog, "applyTint");
    RGBA.tint                 = glGetUniformLocation(prog, "tint");
    RGBA.useAlphaMatte        = glGetUniformLocation(prog, "useAlphaMatte");

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
    RGBX.roundingPower        = glGetUniformLocation(prog, "roundingPower");
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
    EXT.applyTint            = glGetUniformLocation(prog, "applyTint");
    EXT.roundingPower        = glGetUniformLocation(prog, "roundingPower");
    EXT.tint                 = glGetUniformLocation(prog, "tint");

    g_pHyprRenderer->unsetEGL();
}

void ShaderHolder::Destroy()
{
    g_pHyprRenderer->makeEGLCurrent();

    CM.destroy();
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
        throw std::runtime_error(std::format("Error compiling shader \"{}\": {}", src, infoLog));
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
