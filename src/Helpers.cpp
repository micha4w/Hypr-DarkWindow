#include "Helpers.h"

#include <exception>
#include <unordered_set>

#include <hyprland/src/Compositor.hpp>

#include "OpenGL.cpp.h"


static std::string invert(std::string source) {
    size_t out = source.find("layout(location = 0) out vec4 ");
    std::string outVar;
    if (out != std::string::npos) {
        // es 300
        size_t endOut = source.find(";", out);
        outVar = std::string(&source[out + sizeof("layout(location = 0) out vec4 ") - 1], &source[endOut]);
        outVar = Hyprutils::String::trim(outVar);
    } else {
        // es 200
        if (!source.contains("gl_FragColor")) {
            Debug::log(ERR, "Failed to invert GLSL Code, no gl_FragColor:\n{}", source);
            throw std::runtime_error("Frag source does not contain a usage of 'gl_FragColor'");
        }

        outVar = "gl_FragColor";
    }

    if (!source.contains("void main(")) {
        Debug::log(ERR, "Failed to invert GLSL Code, no main function: {}", source);
        throw std::runtime_error("Frag source does not contain an occurence of 'void main('");
    }

    Hyprutils::String::replaceInString(source, "void main(", "void main_uninverted(");

    source += std::format(R"glsl(
void main() {{
    main_uninverted();

    // Invert Colors
    {0}.rgb = vec3(1.) - vec3(.88, .9, .92) * {0}.rgb;

    // Invert Hue
    {0}.rgb = dot(vec3(0.26312, 0.5283, 0.10488), {0}.rgb) * 2.0 - {0}.rgb;
}}
    )glsl", outVar);

    return source;
}

void ShaderHolder::Init()
{
    g_pHyprRenderer->makeEGLCurrent();

    std::map<std::string, std::string> includes;
    loadShaderInclude("rounding.glsl", includes);
    loadShaderInclude("CM.glsl", includes);

    const auto TEXVERTSRC             = g_pHyprOpenGL->m_shaders->TEXVERTSRC;
    const auto TEXVERTSRC300          = g_pHyprOpenGL->m_shaders->TEXVERTSRC300;

    const auto TEXFRAGSRCCM           = invert(processShader("CM.frag", includes));
    const auto TEXFRAGSRCRGBA         = invert(processShader("rgba.frag", includes));
    const auto TEXFRAGSRCRGBX         = invert(processShader("rgbx.frag", includes));
    const auto TEXFRAGSRCEXT          = invert(processShader("ext.frag", includes));

    CM.program = createProgram(TEXVERTSRC300, TEXFRAGSRCCM, true, true);
    if (CM.program) {
        getCMShaderUniforms(CM);
        getRoundingShaderUniforms(CM);
        CM.proj              = glGetUniformLocation(CM.program, "proj");
        CM.tex               = glGetUniformLocation(CM.program, "tex");
        CM.texType           = glGetUniformLocation(CM.program, "texType");
        CM.alphaMatte        = glGetUniformLocation(CM.program, "texMatte");
        CM.alpha             = glGetUniformLocation(CM.program, "alpha");
        CM.texAttrib         = glGetAttribLocation(CM.program, "texcoord");
        CM.matteTexAttrib    = glGetAttribLocation(CM.program, "texcoordMatte");
        CM.posAttrib         = glGetAttribLocation(CM.program, "pos");
        CM.discardOpaque     = glGetUniformLocation(CM.program, "discardOpaque");
        CM.discardAlpha      = glGetUniformLocation(CM.program, "discardAlpha");
        CM.discardAlphaValue = glGetUniformLocation(CM.program, "discardAlphaValue");
        CM.applyTint         = glGetUniformLocation(CM.program, "applyTint");
        CM.tint              = glGetUniformLocation(CM.program, "tint");
        CM.useAlphaMatte     = glGetUniformLocation(CM.program, "useAlphaMatte");
    } else {
        if (g_pHyprOpenGL->m_shaders->m_shCM.program) throw std::runtime_error("Failed to create Shader: CM.frag, check hyprland logs");
    }

    RGBA.program = createProgram(TEXVERTSRC, TEXFRAGSRCRGBA, true, true);
    if (!RGBA.program) throw std::runtime_error("Failed to create Shader: rgba.frag, check hyprland logs");
    getRoundingShaderUniforms(RGBA);
    RGBA.proj              = glGetUniformLocation(RGBA.program, "proj");
    RGBA.tex               = glGetUniformLocation(RGBA.program, "tex");
    RGBA.alphaMatte        = glGetUniformLocation(RGBA.program, "texMatte");
    RGBA.alpha             = glGetUniformLocation(RGBA.program, "alpha");
    RGBA.texAttrib         = glGetAttribLocation(RGBA.program, "texcoord");
    RGBA.matteTexAttrib    = glGetAttribLocation(RGBA.program, "texcoordMatte");
    RGBA.posAttrib         = glGetAttribLocation(RGBA.program, "pos");
    RGBA.discardOpaque     = glGetUniformLocation(RGBA.program, "discardOpaque");
    RGBA.discardAlpha      = glGetUniformLocation(RGBA.program, "discardAlpha");
    RGBA.discardAlphaValue = glGetUniformLocation(RGBA.program, "discardAlphaValue");
    RGBA.applyTint         = glGetUniformLocation(RGBA.program, "applyTint");
    RGBA.tint              = glGetUniformLocation(RGBA.program, "tint");
    RGBA.useAlphaMatte     = glGetUniformLocation(RGBA.program, "useAlphaMatte");


    RGBX.program = createProgram(TEXVERTSRC, TEXFRAGSRCRGBX, true, true);
    if (!RGBX.program) throw std::runtime_error("Failed to create Shader: rgbx.frag, check hyprland logs");
    getRoundingShaderUniforms(RGBX);
    RGBX.tex               = glGetUniformLocation(RGBX.program, "tex");
    RGBX.proj              = glGetUniformLocation(RGBX.program, "proj");
    RGBX.alpha             = glGetUniformLocation(RGBX.program, "alpha");
    RGBX.texAttrib         = glGetAttribLocation(RGBX.program, "texcoord");
    RGBX.posAttrib         = glGetAttribLocation(RGBX.program, "pos");
    RGBX.discardOpaque     = glGetUniformLocation(RGBX.program, "discardOpaque");
    RGBX.discardAlpha      = glGetUniformLocation(RGBX.program, "discardAlpha");
    RGBX.discardAlphaValue = glGetUniformLocation(RGBX.program, "discardAlphaValue");
    RGBX.applyTint         = glGetUniformLocation(RGBX.program, "applyTint");
    RGBX.tint              = glGetUniformLocation(RGBX.program, "tint");

    EXT.program = createProgram(TEXVERTSRC, TEXFRAGSRCEXT, true, true);
    if (!EXT.program) throw std::runtime_error("Failed to create Shader: ext.frag, check hyprland logs");
    getRoundingShaderUniforms(EXT);
    EXT.tex               = glGetUniformLocation(EXT.program, "tex");
    EXT.proj              = glGetUniformLocation(EXT.program, "proj");
    EXT.alpha             = glGetUniformLocation(EXT.program, "alpha");
    EXT.posAttrib         = glGetAttribLocation(EXT.program, "pos");
    EXT.texAttrib         = glGetAttribLocation(EXT.program, "texcoord");
    EXT.discardOpaque     = glGetUniformLocation(EXT.program, "discardOpaque");
    EXT.discardAlpha      = glGetUniformLocation(EXT.program, "discardAlpha");
    EXT.discardAlphaValue = glGetUniformLocation(EXT.program, "discardAlphaValue");
    EXT.applyTint         = glGetUniformLocation(EXT.program, "applyTint");
    EXT.tint              = glGetUniformLocation(EXT.program, "tint");

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