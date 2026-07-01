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
    { "compilation_failed",
      { R"glsl(
        void windowShader(inout vec4 color) {
            const float antiAliasing = 2.;
            const float circleRadius = 110.;
            const float circleThickness = 18.;
            const float dotRadius = 17.;
            const float dotOffset = 57.;
            const float squareOffset = -25.;
            const float squareHeight = 100.;
            const float squareTopWidth = 35.;
            const float squareBottomWidth = 22.;

            vec2 pos = x_PixelPos - x_WindowSize * .5;
            float dist = length(pos);
            float dotDist = length(pos - vec2(0., dotOffset));

            float circle = smoothstep(circleThickness * .5 + antiAliasing, circleThickness * .5, abs(dist - circleRadius));
            float dot = smoothstep(dotRadius + antiAliasing, dotRadius, dotDist);

            float squareY = (pos.y - squareOffset) / squareHeight + 0.5;
            float width = mix(squareTopWidth, squareBottomWidth, squareY);
            float square = float(squareY > 0. && squareY < 1.) * smoothstep(width * .5 + antiAliasing, width * .5, abs(pos.x));

            float ratio = dot + circle + square;
            color.rgb = mix(vec3(.3, .05, .05), vec3(.7, 0., 0.), ratio);
            color.a = 1.;
        }
    )glsl",
        {},
        {} } },
};