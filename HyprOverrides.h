#pragma once

#include <string>
#include <iostream>

#include <hyprland/src/render/OpenGL.hpp>

#include "TexturesDark.h"


inline GLuint compileShader(const GLuint& type, std::string src, bool dynamic = false) {
    auto shader = glCreateShader(type);

    auto shaderSource = src.c_str();

    glShaderSource(shader, 1, (const GLchar**)&shaderSource, nullptr);
    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cout << "[ERROR] Shader compilation failed\n" << infoLog << std::endl;
    }

    if (dynamic) {
        if (ok == GL_FALSE)
            return 0;
    } else {
        RASSERT(ok != GL_FALSE, "compileShader() failed! GL_COMPILE_STATUS not OK!");
    }

    return shader;
}

inline GLuint createProgram(const std::string& vert, const std::string& frag, bool dynamic = false) {
    auto vertCompiled = compileShader(GL_VERTEX_SHADER, vert, dynamic);
    if (dynamic) {
        if (vertCompiled == 0)
            return 0;
    } else {
        RASSERT(vertCompiled, "Compiling shader failed. VERTEX NULL! Shader source:\n\n%s", vert.c_str());
    }

    auto fragCompiled = compileShader(GL_FRAGMENT_SHADER, frag, dynamic);
    if (dynamic) {
        if (fragCompiled == 0)
            return 0;
    } else {
        RASSERT(fragCompiled, "Compiling shader failed. FRAGMENT NULL! Shader source:\n\n%s", frag.c_str());
    }

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
        std::cout << "[ERROR] Shader linking failed\n" << infoLog << std::endl;
    }
    if (dynamic) {
        if (ok == GL_FALSE)
            return 0;
    } else {
        RASSERT(ok != GL_FALSE, "createProgram() failed! GL_LINK_STATUS not OK!");
    }

    return prog;
}

struct ShaderHolder {
    CShader RGBA;
    CShader RGBX;
    CShader EXT;
};

inline void initShaders(ShaderHolder& shaders) {
    GLuint prog                       = createProgram(TEXVERTSRC, TEXFRAGSRCRGBA);
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

    prog                              = createProgram(TEXVERTSRC, TEXFRAGSRCRGBX);
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

    prog                             = createProgram(TEXVERTSRC, TEXFRAGSRCEXT);
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