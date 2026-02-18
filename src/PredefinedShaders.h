#pragma once

#include "State.h"

struct WindowShader {
    std::string Source;
    Uniforms DefaultArgs;
    IntroducesTransparency Transparency;
};

inline static const std::map<std::string, WindowShader> WINDOW_SHADERS = {
    { "invert", { R"glsl(
        void windowShader(inout vec4 color) {
            // remove premultiplied alpha
            color.rgb /= color.a;

            // Invert Colors
            color.rgb = vec3(1.) - vec3(.88, .9, .92) * color.rgb;

            // Invert Hue
            color.rgb = dot(vec3(0.26312, 0.5283, 0.10488), color.rgb) * 2.0 - color.rgb;

            // remultiply alpha
            color.rgb *= color.a;
        }
    )glsl", {}, {} } },
    // Example for a shader with default uniform values
    { "tint", { R"glsl(
        uniform vec3 tintColor;
        uniform float tintStrength;

        void windowShader(inout vec4 color) {
            // remove premultiplied alpha
            color.rgb /= color.a;

            // tint color
            color.rgb = color.rgb * (1.0 - tintStrength) + tintColor * tintStrength;

            // remultiply alpha
            color.rgb *= color.a;
        }
    )glsl", {
        { "tintColor", { 1, 0, 0 } },
        { "tintStrength", { 0.1 } },
    }, {} } },
    // Original shader by ikz87
    // Applies opacity changes to pixels similar to one color
    { "chromakey", { R"glsl(
        uniform vec3 bkg;
        uniform float similarity; // How many similar colors should be affected.
        uniform float amount; // How much similar colors should be changed.
        uniform float targetOpacity; // Target opacity for similar colors.

        void windowShader(inout vec4 color) {
            if (color.r >= bkg.r - similarity && color.r <= bkg.r + similarity &&
                    color.g >= bkg.g - similarity && color.g <= bkg.g + similarity &&
                    color.b >= bkg.b - similarity && color.b <= bkg.b + similarity) {
                vec3 error = vec3(abs(bkg.r - color.r), abs(bkg.g - color.g), abs(bkg.b - color.b));
                float avg_error = (error.r + error.g + error.b) / 3.0;

                color *= targetOpacity + (1.0 - targetOpacity) * avg_error * amount / similarity;
            }
        }
    )glsl", {
        { "bkg", { 0, 0, 0 } },
        { "similarity", { 0.1 } },
        { "amount", { 1.4 } },
        { "targetOpacity", { 0.83 } },
    }, IntroducesTransparency::Yes } },
};