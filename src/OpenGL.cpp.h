// This file contains functions that were yoinked from hyprland/src/render/OpenGL.cpp
#pragma once

#include <hyprland/src/render/OpenGL.hpp>
#include <hyprland/src/render/shaders/Shaders.hpp>
#include <hyprland/src/helpers/fs/FsUtils.hpp>

#include <hyprutils/string/String.hpp>
#include <hyprutils/path/Path.hpp>

const std::vector<const char*> ASSET_PATHS = {
#ifdef DATAROOTDIR
    DATAROOTDIR,
#endif
    "/usr/share",
    "/usr/local/share",
};

static void logShaderError(const GLuint& shader, bool program, bool silent = false) {
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

    Debug::log(ERR, "Failed to link shader: {}", FULLERROR);

    if (!silent)
        g_pConfigManager->addParseError(FULLERROR);
}

static GLuint compileShader(const GLuint& type, std::string src, bool dynamic, bool silent) {
    auto shader = glCreateShader(type);

    auto shaderSource = src.c_str();

    glShaderSource(shader, 1, (const GLchar**)&shaderSource, nullptr);
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

static GLuint createProgram(const std::string& vert, const std::string& frag, bool dynamic, bool silent) {
    auto vertCompiled = compileShader(GL_VERTEX_SHADER, vert, dynamic, silent);
    if (dynamic) {
        if (vertCompiled == 0)
            return 0;
    } else
        RASSERT(vertCompiled, "Compiling shader failed. VERTEX nullptr! Shader source:\n\n{}", vert);

    auto fragCompiled = compileShader(GL_FRAGMENT_SHADER, frag, dynamic, silent);
    if (dynamic) {
        if (fragCompiled == 0)
            return 0;
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
            return 0;
        }
    } else {
        if (ok != GL_TRUE)
            logShaderError(prog, true);
        RASSERT(ok != GL_FALSE, "createProgram() failed! GL_LINK_STATUS not OK!");
    }

    return prog;
}

static std::string loadShader(const std::string& filename) {
    const auto home = Hyprutils::Path::getHome();
    if (home.has_value()) {
        const auto src = NFsUtils::readFileAsString(home.value() + "/hypr/shaders/" + filename);
        if (src.has_value())
            return src.value();
    }
    for (auto& e : ASSET_PATHS) {
        const auto src = NFsUtils::readFileAsString(std::string{e} + "/hypr/shaders/" + filename);
        if (src.has_value())
            return src.value();
    }
    if (SHADERS.contains(filename))
        return SHADERS.at(filename);
    throw std::runtime_error(std::format("Couldn't load shader {}", filename));
}

static void loadShaderInclude(const std::string& filename, std::map<std::string, std::string>& includes) {
    includes.insert({filename, loadShader(filename)});
}

static void processShaderIncludes(std::string& source, const std::map<std::string, std::string>& includes) {
    for (auto it = includes.begin(); it != includes.end(); ++it) {
        Hyprutils::String::replaceInString(source, "#include \"" + it->first + "\"", it->second);
    }
}

static std::string processShader(const std::string& filename, const std::map<std::string, std::string>& includes) {
    auto source = loadShader(filename);
    processShaderIncludes(source, includes);
    return source;
}

// shader has #include "CM.glsl"
static void getCMShaderUniforms(CShader& shader) {
    shader.skipCM          = glGetUniformLocation(shader.program, "skipCM");
    shader.sourceTF        = glGetUniformLocation(shader.program, "sourceTF");
    shader.targetTF        = glGetUniformLocation(shader.program, "targetTF");
    shader.sourcePrimaries = glGetUniformLocation(shader.program, "sourcePrimaries");
    shader.targetPrimaries = glGetUniformLocation(shader.program, "targetPrimaries");
    shader.maxLuminance    = glGetUniformLocation(shader.program, "maxLuminance");
    shader.dstMaxLuminance = glGetUniformLocation(shader.program, "dstMaxLuminance");
    shader.dstRefLuminance = glGetUniformLocation(shader.program, "dstRefLuminance");
    shader.sdrSaturation   = glGetUniformLocation(shader.program, "sdrSaturation");
    shader.sdrBrightness   = glGetUniformLocation(shader.program, "sdrBrightnessMultiplier");
}

// shader has #include "rounding.glsl"
static void getRoundingShaderUniforms(CShader& shader) {
    shader.topLeft       = glGetUniformLocation(shader.program, "topLeft");
    shader.fullSize      = glGetUniformLocation(shader.program, "fullSize");
    shader.radius        = glGetUniformLocation(shader.program, "radius");
    shader.roundingPower = glGetUniformLocation(shader.program, "roundingPower");
}