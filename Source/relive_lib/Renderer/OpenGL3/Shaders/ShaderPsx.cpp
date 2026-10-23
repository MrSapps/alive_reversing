#include "../../../../relive_lib/Types.hpp"
#include "../GLShader.hpp"


const char_type* gShader_PsxVSH = R"(
#version 140
#extension GL_ARB_explicit_attrib_location : enable

layout (location = 0) in vec2 vsPos;
layout (location = 1) in vec3 vsShadeColor;
layout (location = 2) in vec2 vsUV;
layout (location = 3) in uvec4 vsFlags;
layout (location = 4) in uvec2 vsTexIndexing;

out vec3  fsShadeColor;
out vec2  fsUV;
flat out uvec4 fsFlags;
flat out uvec2 fsTexIndexing;

uniform vec2 vsViewportSize;


void main()
{
    gl_Position.x = ((vsPos.x / vsViewportSize.x) * 2) - 1;
    gl_Position.y = (1 - ((vsPos.y / vsViewportSize.y) * 2));
    gl_Position.z = 0.0;
    gl_Position.w = 1.0;

    fsUV = vsUV;
    fsShadeColor = vsShadeColor;
    fsFlags = vsFlags;
    fsTexIndexing = vsTexIndexing;
}
)";

const char_type* gShader_PsxFSH = R"(
#version 140

in vec3 fsShadeColor;
in vec2 fsUV;
flat in uvec4 fsFlags;
flat in uvec2 fsTexIndexing;

out vec4 outColor;

// The sprite effects, one row each, see OpenGLRenderer.cpp's SpriteFxTexels
uniform sampler2D texFx;
uniform sampler2D texGas;
uniform sampler2D texCamera;

uniform sampler2D texSpriteSheets[12];

uniform sampler2D texFramebuffer;

uniform bool bDrawingFramebuffer;

const int BLEND_MODE_HALF_DST_ADD_HALF_SRC = 0;
const int BLEND_MODE_ONE_DST_ADD_ONE_SRC   = 1;
const int BLEND_MODE_ONE_DST_SUB_ONE_SRC   = 2;
const int BLEND_MODE_ONE_DST_ADD_QRT_SRC   = 3;

const int DRAW_FLAT        = 0;
const int DRAW_DEFAULT_FT4 = 1;
const int DRAW_CAM         = 2;
const int DRAW_FG1         = 3;
const int DRAW_GAS         = 4;
const int DRAW_SCREEN_WAVE = 5;
const int DRAW_DEFAULT_FT4_MASKED = 6;

const vec2 frameSize = vec2(640.0, 240.0);


bool dither()
{
    return mod(gl_FragCoord.x + mod(gl_FragCoord.y, 2.0), 2.0) > 0.0;
}

vec3 handle_shading(in vec3 texelT)
{
    bool isShaded = int(fsFlags.z) > 0;
    vec3 texelP = texelT;

    if (isShaded)
    {
        texelP = clamp((texelT * (fsShadeColor / 255.0)) / 0.5f, 0.0f, 1.0f);
    }

    return texelP;
}

// A texel's final colour. The blend func is (ONE, SRC_ALPHA), so the output is
// src.rgb + dst * src.a: outColor.rgb is what to add and outColor.a how much of the framebuffer
// is left. Subtractive blending (blend mode 2) has its own blend func, src.rgb is taken away from
// the framebuffer by src.a.
// alpha is how much of the texel is there, 0 to 1. stp is the PSX semi-transparency bit: the
// texel is only blended if the primitive is semi-transparent as well, or it's drawn over the
// framebuffer like any other (with alpha, so the soft edge of a true colour sprite blends in)
vec4 handle_final_color(in vec3 rgb, in float alpha, in bool stp, in bool doShading)
{
    int blendMode = int(fsFlags.w);
    bool isSemiTrans = int(fsFlags.y) > 0;

    if (alpha == 0.0)
    {
        // Nothing is drawn, the framebuffer is left as it is
        return vec4(0.0, 0.0, 0.0, 1.0);
    }

    if (doShading)
    {
        rgb = handle_shading(rgb);
    }

    if (isSemiTrans && stp)
    {
        switch (blendMode)
        {
            case BLEND_MODE_HALF_DST_ADD_HALF_SRC:
                return vec4(rgb * 0.5 * alpha, 1.0 - 0.5 * alpha);

            case BLEND_MODE_ONE_DST_ADD_ONE_SRC:
                return vec4(rgb * alpha, 1.0);

            case BLEND_MODE_ONE_DST_SUB_ONE_SRC:
                return vec4(rgb, alpha);

            case BLEND_MODE_ONE_DST_ADD_QRT_SRC:
                return vec4(rgb * 0.25 * alpha, 1.0);
        }
    }

    if (blendMode == BLEND_MODE_ONE_DST_SUB_ONE_SRC)
    {
        // Not blended, in a batch that subtracts: leave it
        return vec4(rgb, 0.0);
    }

    return vec4(rgb * alpha, 1.0 - alpha);
}

void draw_flat()
{
    outColor.rgb = fsShadeColor / 255.0;

    outColor = handle_final_color(outColor.rgb, 1.0, true, false);
}

// A texel of the sprite sheet in texture unit fsTexIndexing.y
vec4 fetch_sheet(ivec2 iUV)
{
    switch (fsTexIndexing.y)
    {
        case 0u:
            return texelFetch(texSpriteSheets[0], iUV, 0);

        case 1u:
            return texelFetch(texSpriteSheets[1], iUV, 0);

        case 2u:
            return texelFetch(texSpriteSheets[2], iUV, 0);

        case 3u:
            return texelFetch(texSpriteSheets[3], iUV, 0);

        case 4u:
            return texelFetch(texSpriteSheets[4], iUV, 0);

        case 5u:
            return texelFetch(texSpriteSheets[5], iUV, 0);

        case 6u:
            return texelFetch(texSpriteSheets[6], iUV, 0);

        case 7u:
            return texelFetch(texSpriteSheets[7], iUV, 0);

        case 8u:
            return texelFetch(texSpriteSheets[8], iUV, 0);

        case 9u:
            return texelFetch(texSpriteSheets[9], iUV, 0);

        case 10u:
            return texelFetch(texSpriteSheets[10], iUV, 0);

        case 11u:
            return texelFetch(texSpriteSheets[11], iUV, 0);

    }
    return vec4(0.0);
}

int sheet_height()
{
    switch (fsTexIndexing.y)
    {
        case 0u:
            return textureSize(texSpriteSheets[0], 0).y;

        case 1u:
            return textureSize(texSpriteSheets[1], 0).y;

        case 2u:
            return textureSize(texSpriteSheets[2], 0).y;

        case 3u:
            return textureSize(texSpriteSheets[3], 0).y;

        case 4u:
            return textureSize(texSpriteSheets[4], 0).y;

        case 5u:
            return textureSize(texSpriteSheets[5], 0).y;

        case 6u:
            return textureSize(texSpriteSheets[6], 0).y;

        case 7u:
            return textureSize(texSpriteSheets[7], 0).y;

        case 8u:
            return textureSize(texSpriteSheets[8], 0).y;

        case 9u:
            return textureSize(texSpriteSheets[9], 0).y;

        case 10u:
            return textureSize(texSpriteSheets[10], 0).y;

        case 11u:
            return textureSize(texSpriteSheets[11], 0).y;

    }
    return 0;
}

vec4 sample_texture()
{
    return fetch_sheet(ivec2(fsUV));
}

vec4 fx_texel(int x)
{
    return texelFetch(texFx, ivec2(x, int(fsTexIndexing.x)), 0);
}

// Where a brightness comes among the sprite's colours, 0 (darkest) to 1: the brightnesses at 32
// evenly spaced ranks are in the table after the sweeps
float luma_at(int k)
{
    return fx_texel(8 + k / 4)[k % 4];
}

float luma_rank(float luma)
{
    int k = 0;
    while (k < 30 && luma > luma_at(k + 1))
    {
        k++;
    }

    float lo = luma_at(k);
    float hi = luma_at(k + 1);
    float f = hi > lo ? clamp((luma - lo) / (hi - lo), 0.0, 1.0) : (luma > hi ? 1.0 : 0.0);
    return (float(k) + f) / 31.0;
}

// A texel of a sprite or a font glyph, with the sprite's effects on it (SpriteFx), if any. A sheet
// with a mask (SpriteMask.hpp) has it under the sheet in the same texture, the sheet's height again.
void draw_default_ft4(bool masked)
{
    vec4 texel = sample_texture();
    vec3 rgb = texel.rgb;
    float alpha = texel.a;

    vec4 maskTexel = masked ? fetch_sheet(ivec2(fsUV) + ivec2(0, sheet_height() / 2)) : vec4(0.0);
    bool stp = maskTexel.r < 0.5;
    bool emissive = maskTexel.g > 0.5;
    bool keepVisible = maskTexel.b > 0.5;

    if (fsTexIndexing.x != 0u)
    {
        // How bright the colour is among the sprite's, before the effects change it
        bool anySweep = fx_texel(4).w > 0.0 || fx_texel(5).w > 0.0 || fx_texel(6).w > 0.0;
        float rank = anySweep ? luma_rank(dot(rgb, vec3(0.299, 0.587, 0.114))) : 0.0;

        vec4 inRgb = vec4(rgb, 1.0);
        rgb = clamp(vec3(dot(fx_texel(0), inRgb), dot(fx_texel(1), inRgb), dot(fx_texel(2), inRgb)), 0.0, 1.0);

        // The invisibility leaves some of the colours alone
        if (!keepVisible)
        {
            vec4 subtractAndBlend = fx_texel(3);
            rgb = clamp(rgb - subtractAndBlend.x, 0.0, 1.0);
            if (subtractAndBlend.y > 0.5)
            {
                stp = true;
            }
        }

        vec4 sweepKinds = fx_texel(7);
        for (int i = 0; i < 3; i++)
        {
            vec4 sweep = fx_texel(4 + i);
            if (alpha > 0.0 && sweep.w > 0.0 && rank <= sweep.w + 0.001)
            {
                float kind = sweepKinds[i];
                if (kind > 1.5)
                {
                    // The transparent colour: erased
                    alpha = 0.0;
                }
                else
                {
                    rgb = sweep.rgb;
                    alpha = 1.0;
                    stp = kind > 0.5;
                }
            }
        }
    }

    outColor = handle_final_color(rgb, alpha, stp, !emissive);
}

void draw_cam()
{
    vec2 scaledUV = fsUV / frameSize;

    outColor = texture(texCamera, scaledUV);

    outColor = vec4(outColor.rgb, 0.0);
}

void draw_fg1()
{
    vec2 scaledUV = fsUV / frameSize;
    vec4 mask = sample_texture();

    outColor = vec4(texture(texCamera, scaledUV).rgb, 0.0);

    if (mask.rgb == vec3(0.0))
    {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}

void draw_gas()
{
    vec2 scaledUV = fsUV / frameSize;
    vec4 texelGas = texture(texGas, scaledUV);

    if (dither())
    {
        outColor = texelGas * 0.5;
    }
    else
    {
        outColor = vec4(vec3(0.0), 1.0);
    }
}

// The screen wave moves pieces of the frame. Black (anything that would be black in 16 bit colour)
// and anything from outside the screen stays where it is.
void draw_screen_wave()
{
    vec2 source = fsUV;
    if (any(lessThan(source, vec2(0.0))) || any(greaterThanEqual(source, frameSize)))
    {
        discard;
    }

    // The framebuffer's rows count from the bottom
    vec4 texel = texture(texFramebuffer, vec2(source.x, frameSize.y - source.y) / frameSize);
    if (all(lessThan(floor(texel.rgb * 255.0 + 0.5), vec3(8.0, 4.0, 8.0))))
    {
        discard;
    }

    outColor = vec4(texel.rgb, 0.0);
}

void draw_framebuffer()
{
    if (int(fsFlags.x) == DRAW_SCREEN_WAVE)
    {
        draw_screen_wave();
        return;
    }

    // The copy of the whole frame the framebuffer effects start with
    vec2 scaledUV = fsUV / frameSize;

    outColor = texture(texFramebuffer, scaledUV);

    outColor = vec4(outColor.rgb, 0.0);
}

void main()
{
    if (bDrawingFramebuffer)
    {
        draw_framebuffer();
    }
    else
    {
        int drawMode = int(fsFlags.x);

        switch (drawMode)
        {
            case DRAW_FLAT:
                draw_flat();
                break;

            case DRAW_DEFAULT_FT4:
                draw_default_ft4(false);
                break;

            case DRAW_DEFAULT_FT4_MASKED:
                draw_default_ft4(true);
                break;

            case DRAW_CAM:
                draw_cam();
                break;

            case DRAW_FG1:
                draw_fg1();
                break;

            case DRAW_GAS:
                draw_gas();
                break;
        }
    }
}
)";
