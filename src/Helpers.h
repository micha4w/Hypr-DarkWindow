#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <GL/gl.h>

#include <array>
#include <optional>
#include <utility>
#include <string_view>


// Saves a CTexture to a 24-bit BMP file.
// Notes:
// - This assumes the pixel data is available as 8-bit RGBA (4 bytes per pixel).
//   If CTexture::dataCopy() is empty, the function will attempt to read pixels
//   from the GL texture id (requires a current GL context).
// - alpha channel is discarded in the BMP output.
// - Returns true on success.
inline bool SaveTextureAsBMP(CTexture& tex, const std::string& filepath) {
    auto& dc = tex.dataCopy();
    const int width  = static_cast<int>(tex.m_size.x);
    const int height = static_cast<int>(tex.m_size.y);
    if (width <= 0 || height <= 0) return false;

    std::vector<uint8_t> pixels;
    uint8_t* srcData = nullptr;

    if (!dc.empty() && static_cast<int>(dc.size()) >= width * height * 4) {
        pixels = dc;
        srcData = pixels.data();
    } else {
        if (tex.m_texID == 0) return false;
        pixels.resize(static_cast<size_t>(width) * height * 4);

        bool success = false;

    #ifdef GL_TEXTURE_EXTERNAL_OES
        if (tex.m_target == GL_TEXTURE_EXTERNAL_OES || tex.m_type == TEXTURE_EXTERNAL) return false;
    #else
        if (tex.m_type == TEXTURE_EXTERNAL) return false;
    #endif

    #ifndef GL_ES
        tex.bind();
        glGetError();
        glGetTexImage(tex.m_target, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        GLenum err = glGetError();
        tex.unbind();
        if (err == GL_NO_ERROR) success = true;
    #endif

        if (!success) {
            GLint prevFBO = 0;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);

            GLuint fbo = 0;
            glGenFramebuffers(1, &fbo);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            Hyprutils::Utils::CScopeGuard _fbo([&] { glBindFramebuffer(GL_FRAMEBUFFER, prevFBO); glDeleteFramebuffers(1, &fbo); });

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex.m_target, tex.m_texID, 0);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
    #ifdef GL_READ_BUFFER
                glReadBuffer(GL_COLOR_ATTACHMENT0);
    #endif
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                success = (glGetError() == GL_NO_ERROR);
            }
        }

        if (!success) return false;
        srcData = pixels.data();
    }

    // ---- BMP V4 (alpha-aware) ----
    constexpr uint32_t FILE_HDR_SIZE = 14;
    constexpr uint32_t DIB_V4_SIZE   = 108; // BITMAPV4HEADER

    const uint32_t rowBytes   = static_cast<uint32_t>(width) * 4; // BGRA
    const uint32_t dataSize   = rowBytes * static_cast<uint32_t>(height);
    const uint32_t pixelOff   = FILE_HDR_SIZE + DIB_V4_SIZE;
    const uint32_t fileSize   = pixelOff + dataSize;

    std::ofstream f(filepath, std::ios::binary);
    if (!f) return false;

    auto w16 = [&](uint16_t v) { f.write(reinterpret_cast<const char*>(&v), sizeof(v)); };
    auto w32 = [&](uint32_t v) { f.write(reinterpret_cast<const char*>(&v), sizeof(v)); };
    auto wi32 = [&](int32_t v) { f.write(reinterpret_cast<const char*>(&v), sizeof(v)); };

    // BITMAPFILEHEADER
    w16(0x4D42);      // 'BM'
    w32(fileSize);
    w16(0);
    w16(0);
    w32(pixelOff);

    // BITMAPV4HEADER
    w32(DIB_V4_SIZE); // bV4Size
    wi32(width);      // bV4Width
    wi32(-height);    // bV4Height: negative => top-down (no manual flip needed)
    w16(1);           // bV4Planes
    w16(32);          // bV4BitCount
    w32(3);           // bV4V4Compression = BI_BITFIELDS
    w32(dataSize);    // bV4SizeImage
    wi32(3780);       // bV4XPelsPerMeter
    wi32(3780);       // bV4YPelsPerMeter
    w32(0);           // bV4ClrUsed
    w32(0);           // bV4ClrImportant

    // Masks (BGRA byte layout in file)
    w32(0x00FF0000);  // Red
    w32(0x0000FF00);  // Green
    w32(0x000000FF);  // Blue
    w32(0xFF000000);  // Alpha

    // bV4CSType = LCS_sRGB
    w32(0x73524742);  // 'sRGB'

    // CIEXYZTRIPLE endpoints (unused for sRGB): 9 * FXPT2DOT30 = 36 bytes
    for (int i = 0; i < 9; ++i) w32(0);

    // Gamma RGB
    w32(0);
    w32(0);
    w32(0);

    // Pixels: source is RGBA -> write BGRA.
    // With negative height we write rows in natural top->bottom order.
    for (int y = 0; y < height; ++y) {
        const uint8_t* row = srcData + (static_cast<size_t>(y) * width * 4);
        for (int x = 0; x < width; ++x) {
            const uint8_t r = row[4 * x + 0];
            const uint8_t g = row[4 * x + 1];
            const uint8_t b = row[4 * x + 2];
            const uint8_t a = row[4 * x + 3];
            f.put(static_cast<char>(b));
            f.put(static_cast<char>(g));
            f.put(static_cast<char>(r));
            f.put(static_cast<char>(a));
        }
    }

    f.close();
    return f.good();
}
