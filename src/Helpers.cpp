#include "Helpers.h"

#include <exception>
#include <unordered_set>
#include <ranges>

#include <hyprland/src/Compositor.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

#include "OpenGL.cpp.h"
#include "src/render/Shader.hpp"


static std::string editShader(std::string source, const std::string& windowShader) {
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
            Debug::log(ERR, "Failed to edit GLSL Code, no gl_FragColor:\n{}", source);
            throw efmt("Frag source does not contain a usage of 'gl_FragColor'");
        }

        outVar = "gl_FragColor";
    }

    if (!source.contains("void main(")) {
        Debug::log(ERR, "Failed to edit GLSL Code, no main function: {}", source);
        throw efmt("Frag source does not contain an occurence of 'void main('");
    }

    Hyprutils::String::replaceInString(source, "void main(", "void main_unshaded(");

    source += windowShader + R"glsl(

void main() {
    main_unshaded();

    windowShader()glsl" + outVar + ");\n}";


    return source;
}

ShaderDefinition::ShaderDefinition(std::string id, std::string from, std::string path, const std::string& args)
    : ID(id), From(from), Path(path)
{
    try
    {
        Args = ParseArgs(args);
    }
    catch (const std::exception& ex)
    {
        throw efmt("Failed to parse arguments of shader '{}': {}", args, ex.what());
    }
}

Uniforms ShaderDefinition::ParseArgs(const std::string& args)
{
    Uniforms out;
    std::stringstream ss(args);

    ss >> std::ws;
    while (!ss.eof())
    {
        std::string name;
        std::getline(ss, name, '=');
        name = Hyprutils::String::trim(name);
        for (auto [i, c] : std::views::enumerate(name))
        {
            bool first = c>='a' && c<='z' || c>='A' && c<='Z' || c=='_';
            bool other = c>='0' && c<='9';

            if (!(first || other && i != 0))
                throw efmt("invalid shader uniform name '{}'", name);
        }

        ss >> std::ws;
        
        std::vector<float> values;
        if (ss.peek() == '[')
        {
            ss.get();
            while (1)
            {
                std::string rem(ss.str().substr(ss.tellg()));

                float next;
                ss >> next;

                if (ss.fail()) break;
                values.push_back(next);

                ss >> std::ws;
                if (ss.peek() == ',')
                {
                    ss.get();
                    ss >> std::ws;
                }
                if (ss.peek() == ']') {
                    ss.get();
                    break;
                }

                if (ss.eof()) throw efmt("expected ']' not found");
            }

            if (values.size() < 1 || values.size() > 4)
                throw efmt("only support from 1 to 4 values");
        }
        else
        {
            float next;
            ss >> next;
            values.push_back(next);
        }
        if (ss.fail()) throw efmt("expected a float");
        ss >> std::ws;

        out[name] = values;
    }

    return out;
}

void ShaderHolder::PrimeUniforms(const Uniforms& args)
{
    g_pHyprRenderer->makeEGLCurrent();
    Hyprutils::Utils::CScopeGuard _egl([&]{ g_pHyprRenderer->unsetEGL(); });

    for (auto& [name, _] : args)
    {
        if (UniformLocations.contains(name)) continue;

        SShader* shaders[4] = { &CM, &RGBA, &RGBX, &EXT };
        std::array<GLint, 4> locs;
        for (int i = 0; i < 4; i++)
        {
            if (!shaders[i]->program) continue;

            GLint loc = glGetUniformLocation(shaders[i]->program, name.c_str());
            if (loc == -1) throw efmt("Shader failed to find the uniform: {}", name);
            locs[i] = loc;
        }

        UniformLocations[name] = locs;
    }
}

void ShaderHolder::ApplyArgs(const Uniforms& args) noexcept
{
    SShader* shaders[4] = { &CM, &RGBA, &RGBX, &EXT };
    for (int i = 0; i < 4; i++)
    {
        if (!shaders[i]->program) continue;

        glUseProgram(shaders[i]->program);
        for (auto& [name, values] : args)
        {
            GLint loc = UniformLocations[name][i];
            switch (values.size())
            {
                case 1:
                    glUniform1f(loc, values[0]);
                    break;
                case 2:
                    glUniform2f(loc, values[0], values[1]);
                    break;
                case 3:
                    glUniform3f(loc, values[0], values[1], values[2]);
                    break;
                case 4:
                    glUniform4f(loc, values[0], values[1], values[2], values[3]);
                    break;
            }
        }
    }
}


ShaderHolder::ShaderHolder(const std::string& source)
{
    g_pHyprRenderer->makeEGLCurrent();
    Hyprutils::Utils::CScopeGuard _egl([&]{ g_pHyprRenderer->unsetEGL(); });

    std::map<std::string, std::string> includes;
    loadShaderInclude("rounding.glsl", includes);
    loadShaderInclude("CM.glsl", includes);

    const auto& TEXVERTSRC             = g_pHyprOpenGL->m_shaders->TEXVERTSRC;

    const auto TEXFRAGSRCCM           = editShader(processShader("CM.frag", includes), source);
    const auto TEXFRAGSRCRGBA         = editShader(processShader("rgba.frag", includes), source);
    const auto TEXFRAGSRCRGBX         = editShader(processShader("rgbx.frag", includes), source);
    const auto TEXFRAGSRCEXT          = editShader(processShader("ext.frag", includes), source);

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
        if (g_pHyprOpenGL->m_shaders->m_shCM.program)
            throw efmt("Failed to create Shader: CM.frag, check hyprland logs");
    }

    RGBA.program = createProgram(TEXVERTSRC, TEXFRAGSRCRGBA, true, true);
    if (!RGBA.program) throw efmt("Failed to create Shader: rgba.frag, check hyprland logs");
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
    if (!RGBX.program) throw efmt("Failed to create Shader: rgbx.frag, check hyprland logs");
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
    if (!EXT.program) throw efmt("Failed to create Shader: ext.frag, check hyprland logs");
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
}

ShaderHolder::~ShaderHolder()
{
    g_pHyprRenderer->makeEGLCurrent();

    CM.destroy();
    RGBA.destroy();
    RGBX.destroy();
    EXT.destroy();

    g_pHyprRenderer->unsetEGL();
}
