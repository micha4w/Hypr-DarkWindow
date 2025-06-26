#include "Helpers.h"

#include <exception>
#include <unordered_set>

#include <hyprland/src/Compositor.hpp>

#include "OpenGL.cpp.h"
#include "src/render/Shader.hpp"


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

    const auto TEXFRAGSRCCM           = invert(processShader("CM.frag", includes));
    const auto TEXFRAGSRCRGBA         = invert(processShader("rgba.frag", includes));
    const auto TEXFRAGSRCRGBX         = invert(processShader("rgbx.frag", includes));
    const auto TEXFRAGSRCEXT          = invert(processShader("ext.frag", includes));

    CM.program = createProgram(TEXVERTSRC, TEXFRAGSRCCM, true, true);
    if (CM.program) {
        getCMShaderUniforms(CM);
        getRoundingShaderUniforms(CM);

        CM.uniformLocations[SHADER_PROJ]                = glGetUniformLocation(CM.program, "proj");
        CM.uniformLocations[SHADER_TEX]                 = glGetUniformLocation(CM.program, "tex");
        CM.uniformLocations[SHADER_TEX_TYPE]            = glGetUniformLocation(CM.program, "texType");
        CM.uniformLocations[SHADER_ALPHA_MATTE]         = glGetUniformLocation(CM.program, "texMatte");
        CM.uniformLocations[SHADER_ALPHA]               = glGetUniformLocation(CM.program, "alpha");
        CM.uniformLocations[SHADER_TEX_ATTRIB]          = glGetAttribLocation(CM.program, "texcoord");
        CM.uniformLocations[SHADER_MATTE_TEX_ATTRIB]    = glGetAttribLocation(CM.program, "texcoordMatte");
        CM.uniformLocations[SHADER_POS_ATTRIB]          = glGetAttribLocation(CM.program, "pos");
        CM.uniformLocations[SHADER_DISCARD_OPAQUE]      = glGetUniformLocation(CM.program, "discardOpaque");
        CM.uniformLocations[SHADER_DISCARD_ALPHA]       = glGetUniformLocation(CM.program, "discardAlpha");
        CM.uniformLocations[SHADER_DISCARD_ALPHA_VALUE] = glGetUniformLocation(CM.program, "discardAlphaValue");
        CM.uniformLocations[SHADER_APPLY_TINT]          = glGetUniformLocation(CM.program, "applyTint");
        CM.uniformLocations[SHADER_TINT]                = glGetUniformLocation(CM.program, "tint");
        CM.uniformLocations[SHADER_USE_ALPHA_MATTE]     = glGetUniformLocation(CM.program, "useAlphaMatte");
        CM.createVao();
    } else {
        if (g_pHyprOpenGL->m_shaders->m_shCM.program) throw std::runtime_error("Failed to create Shader: CM.frag, check hyprland logs");
    }

    RGBA.program = createProgram(TEXVERTSRC, TEXFRAGSRCRGBA, true, true);
    if (!RGBA.program) throw std::runtime_error("Failed to create Shader: rgba.frag, check hyprland logs");
    getRoundingShaderUniforms(RGBA);
    RGBA.uniformLocations[SHADER_PROJ]                = glGetUniformLocation(RGBA.program, "proj");
    RGBA.uniformLocations[SHADER_TEX]                 = glGetUniformLocation(RGBA.program, "tex");
    RGBA.uniformLocations[SHADER_ALPHA_MATTE]         = glGetUniformLocation(RGBA.program, "texMatte");
    RGBA.uniformLocations[SHADER_ALPHA]               = glGetUniformLocation(RGBA.program, "alpha");
    RGBA.uniformLocations[SHADER_TEX_ATTRIB]          = glGetAttribLocation(RGBA.program, "texcoord");
    RGBA.uniformLocations[SHADER_MATTE_TEX_ATTRIB]    = glGetAttribLocation(RGBA.program, "texcoordMatte");
    RGBA.uniformLocations[SHADER_POS_ATTRIB]          = glGetAttribLocation(RGBA.program, "pos");
    RGBA.uniformLocations[SHADER_DISCARD_OPAQUE]      = glGetUniformLocation(RGBA.program, "discardOpaque");
    RGBA.uniformLocations[SHADER_DISCARD_ALPHA]       = glGetUniformLocation(RGBA.program, "discardAlpha");
    RGBA.uniformLocations[SHADER_DISCARD_ALPHA_VALUE] = glGetUniformLocation(RGBA.program, "discardAlphaValue");
    RGBA.uniformLocations[SHADER_APPLY_TINT]          = glGetUniformLocation(RGBA.program, "applyTint");
    RGBA.uniformLocations[SHADER_TINT]                = glGetUniformLocation(RGBA.program, "tint");
    RGBA.uniformLocations[SHADER_USE_ALPHA_MATTE]     = glGetUniformLocation(RGBA.program, "useAlphaMatte");
    RGBA.createVao();

    RGBX.program = createProgram(TEXVERTSRC, TEXFRAGSRCRGBX, true, true);
    if (!RGBX.program) throw std::runtime_error("Failed to create Shader: rgbx.frag, check hyprland logs");
    getRoundingShaderUniforms(RGBX);
    RGBX.uniformLocations[SHADER_TEX]                 = glGetUniformLocation(RGBX.program, "tex");
    RGBX.uniformLocations[SHADER_PROJ]                = glGetUniformLocation(RGBX.program, "proj");
    RGBX.uniformLocations[SHADER_ALPHA]               = glGetUniformLocation(RGBX.program, "alpha");
    RGBX.uniformLocations[SHADER_TEX_ATTRIB]          = glGetAttribLocation(RGBX.program, "texcoord");
    RGBX.uniformLocations[SHADER_POS_ATTRIB]          = glGetAttribLocation(RGBX.program, "pos");
    RGBX.uniformLocations[SHADER_DISCARD_OPAQUE]      = glGetUniformLocation(RGBX.program, "discardOpaque");
    RGBX.uniformLocations[SHADER_DISCARD_ALPHA]       = glGetUniformLocation(RGBX.program, "discardAlpha");
    RGBX.uniformLocations[SHADER_DISCARD_ALPHA_VALUE] = glGetUniformLocation(RGBX.program, "discardAlphaValue");
    RGBX.uniformLocations[SHADER_APPLY_TINT]          = glGetUniformLocation(RGBX.program, "applyTint");
    RGBX.uniformLocations[SHADER_TINT]                = glGetUniformLocation(RGBX.program, "tint");
    RGBX.createVao();

    EXT.program = createProgram(TEXVERTSRC, TEXFRAGSRCEXT, true, true);
    if (!EXT.program) throw std::runtime_error("Failed to create Shader: ext.frag, check hyprland logs");
    getRoundingShaderUniforms(EXT);
    EXT.uniformLocations[SHADER_TEX]                 = glGetUniformLocation(EXT.program, "tex");
    EXT.uniformLocations[SHADER_PROJ]                = glGetUniformLocation(EXT.program, "proj");
    EXT.uniformLocations[SHADER_ALPHA]               = glGetUniformLocation(EXT.program, "alpha");
    EXT.uniformLocations[SHADER_POS_ATTRIB]          = glGetAttribLocation(EXT.program, "pos");
    EXT.uniformLocations[SHADER_TEX_ATTRIB]          = glGetAttribLocation(EXT.program, "texcoord");
    EXT.uniformLocations[SHADER_DISCARD_OPAQUE]      = glGetUniformLocation(EXT.program, "discardOpaque");
    EXT.uniformLocations[SHADER_DISCARD_ALPHA]       = glGetUniformLocation(EXT.program, "discardAlpha");
    EXT.uniformLocations[SHADER_DISCARD_ALPHA_VALUE] = glGetUniformLocation(EXT.program, "discardAlphaValue");
    EXT.uniformLocations[SHADER_APPLY_TINT]          = glGetUniformLocation(EXT.program, "applyTint");
    EXT.uniformLocations[SHADER_TINT]                = glGetUniformLocation(EXT.program, "tint");
    EXT.createVao();

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
