#include "Helpers.h"

#include <exception>
#include <unordered_set>
#include <ranges>

#include <hyprland/src/Compositor.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

#include "OpenGL.cpp.h"


static const std::map<std::string, std::string> WINDOW_SHADER_SOURCES = {
    { "invert", R"glsl(
        void windowShader(inout vec4 color) {
            // Invert Colors
            color.rgb = vec3(1.) - vec3(.88, .9, .92) * color.rgb;

            // Invert Hue
            color.rgb = dot(vec3(0.26312, 0.5283, 0.10488), color.rgb) * 2.0 - color.rgb;
        }
    )glsl" },
    { "tint", R"glsl(
        uniform vec3 tintColor;
        uniform float tintFactor;
        
        void windowShader(inout vec4 color) {
            color.rgb = color.rgb * (1.0 - tintFactor) + tintColor * tintFactor;
        }
    )glsl" },
};

static std::string invert(std::string source, const std::string& windowShader) {
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

    source += windowShader + R"glsl(

void main() {
    main_uninverted();

    windowShader()glsl" + outVar + ");\n}";


    return source;
}

std::map<std::string, std::vector<float>> parseArgs(const std::string& args)
{
    std::map<std::string, std::vector<float>> out;
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
                throw std::runtime_error("invalid shader uniform name '" + name + "'");
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

                if (ss.eof()) throw std::runtime_error("expected ']' not found");
            }

            if (values.size() < 1 || values.size() > 4)
                throw std::runtime_error("only support from 1 to 4 values");
        }
        else
        {
            float next;
            ss >> next;
            values.push_back(next);
        }
        if (ss.fail()) throw std::runtime_error("expected a float");
        ss >> std::ws;

        out[name] = values;
    }

    return out;
}

void ShaderHolder::LoadArgs(std::map<std::string, std::vector<float>> args)
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
            if (loc == -1) throw std::runtime_error("Shader failed to find the uniform: " + name);
            locs[i] = loc;
        }

        UniformLocations[name] = locs;
    }
}

void ShaderHolder::ApplyArgs(std::map<std::string, std::vector<float>> args) noexcept
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


ShaderHolder::ShaderHolder(std::variant<std::string, std::string> nameOrPath)
    : NameOrPath(nameOrPath)
{
    std::string windowShader;
    if (NameOrPath.index() == 1)
    {
        std::string& path = std::get<1>(NameOrPath);
        Debug::log(INFO, "Loading shader at path: {}", path);

        std::ifstream file(path);
        windowShader = std::string(std::istreambuf_iterator(file), {});
    }
    else
    {
        std::string& name = std::get<0>(NameOrPath);
        Debug::log(INFO, "Loading shader with name: {}", name);

        auto inlineSource = WINDOW_SHADER_SOURCES.find(name);
        if (inlineSource == WINDOW_SHADER_SOURCES.end()) throw std::runtime_error("Shader name not found");

        windowShader = inlineSource->second;
    }

    g_pHyprRenderer->makeEGLCurrent();
    Hyprutils::Utils::CScopeGuard _egl([&]{ g_pHyprRenderer->unsetEGL(); });

    std::map<std::string, std::string> includes;
    loadShaderInclude("rounding.glsl", includes);
    loadShaderInclude("CM.glsl", includes);

    const auto TEXVERTSRC             = g_pHyprOpenGL->m_shaders->TEXVERTSRC;
    const auto TEXVERTSRC300          = g_pHyprOpenGL->m_shaders->TEXVERTSRC300;

    const auto TEXFRAGSRCCM           = invert(processShader("CM.frag", includes), windowShader);
    const auto TEXFRAGSRCRGBA         = invert(processShader("rgba.frag", includes), windowShader);
    const auto TEXFRAGSRCRGBX         = invert(processShader("rgbx.frag", includes), windowShader);
    const auto TEXFRAGSRCEXT          = invert(processShader("ext.frag", includes), windowShader);

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