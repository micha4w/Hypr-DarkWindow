#pragma once

#include <string>
#include <format>

#include <hyprland/src/render/shaders/Textures.hpp>


inline static const std::string DARK_MODE_FUNC = R"glsl(
void invert(inout vec4 color) {
    // Invert Colors
    color.rgb = vec3(1.) - vec3(.88, .9, .92) * color.rgb;

    // Invert Hue
    color.rgb = dot(vec3(0.26312, 0.5283, 0.10488), color.rgb) * 2.0 - color.rgb;
}
)glsl";


inline const std::string TEXFRAGSRCCM_DARK = R"glsl(
precision highp float;
varying vec2 v_texcoord;
uniform sampler2D tex;

)glsl" + DARK_MODE_FUNC + R"glsl(

void main() {
    vec4 pixColor = texture2D(tex, v_texcoord);

    invert(pixColor);

    gl_FragColor = pixColor;
})glsl";

inline const std::string TEXFRAGSRCRGBA_DARK = R"glsl(
precision highp float;
varying vec2 v_texcoord; // is in 0-1
uniform sampler2D tex;
uniform float alpha;

uniform vec2 topLeft;
uniform vec2 fullSize;
uniform float radius;
uniform float roundingPower;

uniform int discardOpaque;
uniform int discardAlpha;
uniform float discardAlphaValue;

uniform int applyTint;
uniform vec3 tint;

)glsl" + DARK_MODE_FUNC + R"glsl(

void main() {

    vec4 pixColor = texture2D(tex, v_texcoord);

    if (discardOpaque == 1 && pixColor[3] * alpha == 1.0)
	    discard;

    if (discardAlpha == 1 && pixColor[3] <= discardAlphaValue)
        discard;

    if (applyTint == 1) {
	    pixColor[0] = pixColor[0] * tint[0];
	    pixColor[1] = pixColor[1] * tint[1];
	    pixColor[2] = pixColor[2] * tint[2];
    }

    invert(pixColor);

    if (radius > 0.0) {
    )glsl" +
    ROUNDED_SHADER_FUNC("pixColor") + R"glsl(
    }

    gl_FragColor = pixColor * alpha;
})glsl";

inline const std::string TEXFRAGSRCRGBX_DARK = R"glsl(
precision highp float;
varying vec2 v_texcoord;
uniform sampler2D tex;
uniform float alpha;

uniform vec2 topLeft;
uniform vec2 fullSize;
uniform float radius;
uniform float roundingPower;

uniform int discardOpaque;
uniform int discardAlpha;
uniform int discardAlphaValue;

uniform int applyTint;
uniform vec3 tint;

)glsl" + DARK_MODE_FUNC + R"glsl(

void main() {

    if (discardOpaque == 1 && alpha == 1.0)
	discard;

    vec4 pixColor = vec4(texture2D(tex, v_texcoord).rgb, 1.0);

    if (applyTint == 1) {
	pixColor[0] = pixColor[0] * tint[0];
	pixColor[1] = pixColor[1] * tint[1];
	pixColor[2] = pixColor[2] * tint[2];
    }

    invert(pixColor);

    if (radius > 0.0) {
    )glsl" +
    ROUNDED_SHADER_FUNC("pixColor") + R"glsl(
    }

    gl_FragColor = pixColor * alpha;
})glsl";

inline const std::string TEXFRAGSRCEXT_DARK = R"glsl(
#extension GL_OES_EGL_image_external : require

precision highp float;
varying vec2 v_texcoord;
uniform samplerExternalOES texture0;
uniform float alpha;

uniform vec2 topLeft;
uniform vec2 fullSize;
uniform float radius;
uniform float roundingPower;

uniform int discardOpaque;
uniform int discardAlpha;
uniform int discardAlphaValue;

uniform int applyTint;
uniform vec3 tint;

)glsl" + DARK_MODE_FUNC + R"glsl(

void main() {

    vec4 pixColor = texture2D(texture0, v_texcoord);

    if (discardOpaque == 1 && pixColor[3] * alpha == 1.0)
	discard;

    if (applyTint == 1) {
	pixColor[0] = pixColor[0] * tint[0];
	pixColor[1] = pixColor[1] * tint[1];
	pixColor[2] = pixColor[2] * tint[2];
    }

    invert(pixColor);

    if (radius > 0.0) {
    )glsl" +
    ROUNDED_SHADER_FUNC("pixColor") + R"glsl(
    }

    gl_FragColor = pixColor * alpha;
}
)glsl";