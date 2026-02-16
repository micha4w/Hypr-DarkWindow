#include "CustomShader.h"

#include <exception>
#include <unordered_set>
#include <ranges>

#include "State.h"


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
            bool first = c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c == '_';
            bool other = c >= '0' && c <= '9';
            bool special = c == '[' || c == ']' || c == '.';

            if (!(first || ((other || special) && i != 0)))
                throw g.Efmt("invalid shader uniform name '{}'", name);
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

                if (ss.eof()) throw g.Efmt("expected ']' not found");
            }

            if (values.size() < 1 || values.size() > 4)
                throw g.Efmt("only support from 1 to 4 values");
        }
        else
        {
            float next;
            ss >> next;
            values.push_back(next);
        }
        if (ss.fail()) throw g.Efmt("expected a float");
        ss >> std::ws;

        out[name] = values;
    }

    return out;
}


std::string CompiledShaders::EditShader(const std::string& originalSource)
{
    std::string source = originalSource;

    size_t main = source.find("void main(");
    if (main == std::string::npos)
        throw g.Efmt("Frag source does not contain an occurence of 'void main('");

    source.insert(main, CustomSource + "\n\n");

    size_t pixColorDef = source.find("vec4 pixColor =");
    if (pixColorDef == std::string::npos)
        throw g.Efmt("Frag source does not contain an occurence of 'vec4 pixColor ='");
    
    size_t defEnd = source.find(';', pixColorDef);
    if (defEnd == std::string::npos)
        throw g.Efmt("Frag source does not contain a ';' after 'vec4 pixColor ='");

    source.insert(defEnd + 1, "\n    windowShader(pixColor);");

    return source;
}

void logShaderError(const GLuint& shader, bool program, bool silent = false) {
    GLint maxLength = 0;
    if (program)
        glGetProgramiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
    else
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

    std::vector<GLchar> errorLog(maxLength);
    if (program)
        glGetProgramInfoLog(shader, maxLength, &maxLength, errorLog.data());
    else
        glGetShaderInfoLog(shader, maxLength, &maxLength, errorLog.data());
    std::string errorStr(errorLog.begin(), errorLog.end());

    const auto  FULLERROR = (program ? "Screen shader parser: Error linking program:" : "Screen shader parser: Error compiling shader: ") + errorStr;

    Log::logger->log(Log::ERR, "Failed to link shader: {}", FULLERROR);

    if (!silent)
        g_pConfigManager->addParseError(FULLERROR);
}

GLuint compileShader(const GLuint& type, std::string src, bool dynamic, bool silent) {
    auto shader = glCreateShader(type);

    auto shaderSource = src.c_str();

    glShaderSource(shader, 1, &shaderSource, nullptr);
    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);

    if (dynamic) {
        if (ok == GL_FALSE) {
            logShaderError(shader, false, silent);
            return 0;
        }
    } else {
        if (ok != GL_TRUE)
            logShaderError(shader, false);
        RASSERT(ok != GL_FALSE, "compileShader() failed! GL_COMPILE_STATUS not OK!");
    }

    return shader;
}

bool createProgram(const std::string& vert, const std::string& frag, bool dynamic, bool silent) {
    auto vertCompiled = compileShader(GL_VERTEX_SHADER, vert, dynamic, silent);
    if (dynamic) {
        if (vertCompiled == 0)
            return false;
    } else
        RASSERT(vertCompiled, "Compiling shader failed. VERTEX nullptr! Shader source:\n\n{}", vert);

    auto fragCompiled = compileShader(GL_FRAGMENT_SHADER, frag, dynamic, silent);
    if (dynamic) {
        if (fragCompiled == 0)
            return false;
    } else
        RASSERT(fragCompiled, "Compiling shader failed. FRAGMENT nullptr! Shader source:\n\n{}", frag);

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
    if (dynamic) {
        if (ok == GL_FALSE) {
            logShaderError(prog, true, silent);
            return false;
        }
    } else {
        if (ok != GL_TRUE)
            logShaderError(prog, true);
        RASSERT(ok != GL_FALSE, "createProgram() failed! GL_LINK_STATUS not OK!");
    }

    return true;
}

void CompiledShaders::TestCompilation(const Uniforms& args)
{
    static const std::string testFragSource = R"glsl(
#version 300 es
precision highp float;

layout(location = 0) out vec4 fragColor;
void main() {
    vec4 pixColor = vec4(1.0);
    fragColor = pixColor;
}
    )glsl";
    std::string testFrag = EditShader(testFragSource);
    auto testShader = SP(new CShader());

    g_pHyprRenderer->makeEGLCurrent();
    Hyprutils::Utils::CScopeGuard _egl([&] { g_pHyprRenderer->unsetEGL(); });

    if (!testShader->createProgram(g_pHyprOpenGL->m_shaders->TEXVERTSRC, testFrag, true, true))
        throw g.Efmt("Failed to compile");

    CompiledShaders::CustomShader{ .Shader = testShader }.PrimeUniforms(args);
}

void CompiledShaders::CustomShader::PrimeUniforms(const Uniforms& args)
{
    for (auto& [name, _] : args)
    {
        if (UniformLocations.contains(name)) continue;

        GLint loc = glGetUniformLocation(Shader->program(), name.c_str());
        if (loc == -1) throw g.Efmt("Shader failed to find the uniform: {}", name);
        UniformLocations[name] = loc;
    }
}

void CompiledShaders::CustomShader::SetUniforms(const Uniforms& args) noexcept
{
    GLint prog;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);

    glUseProgram(Shader->program());
    for (auto& [name, values] : args)
    {
        GLint loc = UniformLocations[name];
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

    glUseProgram(prog);
}


CompiledShaders::~CompiledShaders()
{
    g_pHyprRenderer->makeEGLCurrent();
    FragVariants.clear();
    g_pHyprRenderer->unsetEGL();
}
