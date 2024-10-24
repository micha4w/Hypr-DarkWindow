#pragma once

#include <string>
#include <cstring>

#include <hyprland/src/render/OpenGL.hpp>

#include "TexturesDark.h"


namespace std
{
    inline void swap(CShader& a, CShader& b)
    {
        // memcpy because speed!
        // (Would break unordered map, but those aren't in use anyway..)
        uint8_t c[sizeof(CShader)];
        std::memcpy(&c, &a, sizeof(CShader));
        std::memcpy(&a, &b, sizeof(CShader));
        std::memcpy(&b, &c, sizeof(CShader));
    }
}

struct ShaderHolder
{
    CShader RGBA;
    GLint RGBA_Invert;
    CShader RGBX;
    GLint RGBX_Invert;
    CShader EXT;
    GLint EXT_Invert;

    void Init();
    void Destroy();

private:
    GLuint CompileShader(const GLuint& type, std::string src);
    GLuint CreateProgram(const std::string& vert, const std::string& frag);
};
