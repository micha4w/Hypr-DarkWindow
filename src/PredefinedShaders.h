#pragma once

#include "State.h"

struct WindowShader
{
    std::string Source;
    Uniforms DefaultArgs;
    IntroducesTransparency Transparency;
    float FadeInSpeed = .0f, FadeOutSpeed = .0f;
    float AnimationInterval = .0f;
};

inline static const std::map<std::string, WindowShader> WINDOW_SHADERS = {
    { "invert",
      { R"glsl(
        void windowShader(inout vec4 color) {
            // Invert Colors
            vec3 inverted = vec3(1.) - vec3(.88, .9, .92) * color.rgb / color.a;

            // Invert Hue
            inverted = dot(vec3(0.26312, 0.5283, 0.10488), inverted) * 2.0 - inverted;

            // Animate
            color.rgb = mix(color.rgb, inverted * color.a, x_FadeIn - x_FadeOut);
        }
    )glsl",
        {},
        {} } },
    // Example for a shader with default uniform values
    { "tint",
      { R"glsl(
        uniform vec3 tintColor;
        uniform float tintStrength;

        void windowShader(inout vec4 color) {
            color.rgb = mix(color.rgb, tintColor * color.a, tintStrength * (x_FadeIn - x_FadeOut));
        }
    )glsl",
        {
            { "tintColor", { 1, 0, 0 } },
            { "tintStrength", { 0.1 } },
        },
        {} } },
    // Original shader by ikz87
    // Applies opacity changes to pixels similar to one color
    { "chromakey",
      { R"glsl(
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

                float animatedOpacity = mix(1., targetOpacity, x_FadeIn - x_FadeOut);
                color *= animatedOpacity + (1.0 - animatedOpacity) * avg_error * amount / similarity;
            }
        }
    )glsl",
        {
            { "bkg", { 0, 0, 0 } },
            { "similarity", { 0.1 } },
            { "amount", { 1.4 } },
            { "targetOpacity", { 0.83 } },
        },
        IntroducesTransparency::Yes } },
};