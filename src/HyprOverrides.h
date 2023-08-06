#pragma once

#include <string>
#include <iostream>
#include <exception>

#include <hyprland/src/render/OpenGL.hpp>

#include "TexturesDark.h"


inline GLuint compileShader(const GLuint& type, std::string src) {
    auto shader = glCreateShader(type);

    auto shaderSource = src.c_str();

    glShaderSource(shader, 1, (const GLchar**)&shaderSource, nullptr);
    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "[ERROR] Shader Compiling failed\n" << infoLog << std::endl;
        throw std::runtime_error(std::string("Shader compilation failed: ") + infoLog);
    }

    return shader;
}

inline GLuint createProgram(const std::string& vert, const std::string& frag) {
    auto vertCompiled = compileShader(GL_VERTEX_SHADER, vert);
    auto fragCompiled = compileShader(GL_FRAGMENT_SHADER, frag);

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
        std::cerr << "[ERROR] Shader linking failed\n" << infoLog << std::endl;
        std::runtime_error(std::string("Shader linking failed: ") + infoLog);
    }

    return prog;
}

struct ShaderHolder {
    CShader RGBA;
    CShader RGBX;
    CShader EXT;
};

inline void initShaders(ShaderHolder& shaders) {
    GLuint prog                       = createProgram(TEXVERTSRC, TEXFRAGSRCRGBA_DARK);
    shaders.RGBA.program              = prog;
    shaders.RGBA.proj                 = glGetUniformLocation(prog, "proj");
    shaders.RGBA.tex                  = glGetUniformLocation(prog, "tex");
    shaders.RGBA.alpha                = glGetUniformLocation(prog, "alpha");
    shaders.RGBA.texAttrib            = glGetAttribLocation(prog, "texcoord");
    shaders.RGBA.posAttrib            = glGetAttribLocation(prog, "pos");
    shaders.RGBA.discardOpaque        = glGetUniformLocation(prog, "discardOpaque");
    shaders.RGBA.discardAlpha         = glGetUniformLocation(prog, "discardAlpha");
    shaders.RGBA.discardAlphaValue    = glGetUniformLocation(prog, "discardAlphaValue");
    shaders.RGBA.topLeft              = glGetUniformLocation(prog, "topLeft");
    shaders.RGBA.fullSize             = glGetUniformLocation(prog, "fullSize");
    shaders.RGBA.radius               = glGetUniformLocation(prog, "radius");
    shaders.RGBA.primitiveMultisample = glGetUniformLocation(prog, "primitiveMultisample");
    shaders.RGBA.applyTint            = glGetUniformLocation(prog, "applyTint");
    shaders.RGBA.tint                 = glGetUniformLocation(prog, "tint");

    prog                              = createProgram(TEXVERTSRC, TEXFRAGSRCRGBX_DARK);
    shaders.RGBX.program              = prog;
    shaders.RGBX.tex                  = glGetUniformLocation(prog, "tex");
    shaders.RGBX.proj                 = glGetUniformLocation(prog, "proj");
    shaders.RGBX.alpha                = glGetUniformLocation(prog, "alpha");
    shaders.RGBX.texAttrib            = glGetAttribLocation(prog, "texcoord");
    shaders.RGBX.posAttrib            = glGetAttribLocation(prog, "pos");
    shaders.RGBX.discardOpaque        = glGetUniformLocation(prog, "discardOpaque");
    shaders.RGBX.discardAlpha         = glGetUniformLocation(prog, "discardAlpha");
    shaders.RGBX.discardAlphaValue    = glGetUniformLocation(prog, "discardAlphaValue");
    shaders.RGBX.topLeft              = glGetUniformLocation(prog, "topLeft");
    shaders.RGBX.fullSize             = glGetUniformLocation(prog, "fullSize");
    shaders.RGBX.radius               = glGetUniformLocation(prog, "radius");
    shaders.RGBX.primitiveMultisample = glGetUniformLocation(prog, "primitiveMultisample");
    shaders.RGBX.applyTint            = glGetUniformLocation(prog, "applyTint");
    shaders.RGBX.tint                 = glGetUniformLocation(prog, "tint");

    prog                             = createProgram(TEXVERTSRC, TEXFRAGSRCEXT_DARK);
    shaders.EXT.program              = prog;
    shaders.EXT.tex                  = glGetUniformLocation(prog, "tex");
    shaders.EXT.proj                 = glGetUniformLocation(prog, "proj");
    shaders.EXT.alpha                = glGetUniformLocation(prog, "alpha");
    shaders.EXT.posAttrib            = glGetAttribLocation(prog, "pos");
    shaders.EXT.texAttrib            = glGetAttribLocation(prog, "texcoord");
    shaders.EXT.discardOpaque        = glGetUniformLocation(prog, "discardOpaque");
    shaders.EXT.discardAlpha         = glGetUniformLocation(prog, "discardAlpha");
    shaders.EXT.discardAlphaValue    = glGetUniformLocation(prog, "discardAlphaValue");
    shaders.EXT.topLeft              = glGetUniformLocation(prog, "topLeft");
    shaders.EXT.fullSize             = glGetUniformLocation(prog, "fullSize");
    shaders.EXT.radius               = glGetUniformLocation(prog, "radius");
    shaders.EXT.primitiveMultisample = glGetUniformLocation(prog, "primitiveMultisample");
    shaders.EXT.applyTint            = glGetUniformLocation(prog, "applyTint");
    shaders.EXT.tint                 = glGetUniformLocation(prog, "tint");
}