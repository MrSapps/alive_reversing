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

uniform sampler2D texPalette;
uniform sampler2D texGas;
uniform sampler2D texCamera;

uniform sampler2D texSpriteSheets[12];
uniform vec2 fsSpriteSheetSize[12];

const int BLEND_MODE_HALF_DST_ADD_HALF_SRC = 0;
const int BLEND_MODE_ONE_DST_ADD_ONE_SRC   = 1;
const int BLEND_MODE_ONE_DST_SUB_ONE_SRC   = 2;
const int BLEND_MODE_ONE_DST_ADD_QRT_SRC   = 3;

const int DRAW_FLAT        = 0;
const int DRAW_DEFAULT_FT4 = 1;
const int DRAW_CAM         = 2;
const int DRAW_FG1         = 3;
const int DRAW_GAS         = 4;

const vec2 frameSize = vec2(640.0, 240.0);


vec4 PixelToPalette(float v)
{
    return texture(texPalette, vec2(v, fsTexIndexing.x / 255.0));
}

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

vec4 handle_final_color(in vec4 src, in bool doShading)
{
    int blendMode = int(fsFlags.w);
    bool isSemiTrans = int(fsFlags.y) > 0;
    vec4 ret = src;

    if (src == vec4(0.0))
    {
        return vec4(0.0, 0.0, 0.0, 1.0);
    }

    if (doShading)
    {
        ret.rgb = handle_shading(src.rgb);
    }

    if (isSemiTrans && src.a == 1.0)
    {
        switch (blendMode)
        {
            case BLEND_MODE_HALF_DST_ADD_HALF_SRC:
                ret = vec4(ret.rgb * 0.5, 0.5);
                break;

            case BLEND_MODE_ONE_DST_ADD_ONE_SRC:
            case BLEND_MODE_ONE_DST_SUB_ONE_SRC:
                ret = vec4(ret.rgb, 1.0);
                break;

            case BLEND_MODE_ONE_DST_ADD_QRT_SRC:
                ret = vec4(ret.rgb * 0.25, 1.0);
                break;
        }
    }
    else
    {
        ret = vec4(ret.rgb, 0.0);
    }

    return ret;
}

void draw_flat()
{
    outColor.rgb = fsShadeColor / 255.0;

    outColor = handle_final_color(vec4(outColor.rgb, 1.0), false);
}

vec4 sample_texture()
{
    vec2 scaledUV = vec2(0.0);
    vec4 texelSprite = vec4(0.0);
    switch (fsTexIndexing.y)
    {
        case 0u:
            scaledUV = fsUV / fsSpriteSheetSize[0];
            texelSprite = texture(texSpriteSheets[0], scaledUV);
            break;

        case 1u:
            scaledUV = fsUV / fsSpriteSheetSize[1];
            texelSprite = texture(texSpriteSheets[1], scaledUV);
            break;

        case 2u:
            scaledUV = fsUV / fsSpriteSheetSize[2];
            texelSprite = texture(texSpriteSheets[2], scaledUV);
            break;

        case 3u:
            scaledUV = fsUV / fsSpriteSheetSize[3];
            texelSprite = texture(texSpriteSheets[3], scaledUV);
            break;

        case 4u:
            scaledUV = fsUV / fsSpriteSheetSize[4];
            texelSprite = texture(texSpriteSheets[4], scaledUV);
            break;

        case 5u:
            scaledUV = fsUV / fsSpriteSheetSize[5];
            texelSprite = texture(texSpriteSheets[5], scaledUV);
            break;

        case 6u:
            scaledUV = fsUV / fsSpriteSheetSize[6];
            texelSprite = texture(texSpriteSheets[6], scaledUV);
            break;

        case 7u:
            scaledUV = fsUV / fsSpriteSheetSize[7];
            texelSprite = texture(texSpriteSheets[7], scaledUV);
            break;

        case 8u:
            scaledUV = fsUV / fsSpriteSheetSize[8];
            texelSprite = texture(texSpriteSheets[8], scaledUV);
            break;

        case 9u:
            scaledUV = fsUV / fsSpriteSheetSize[9];
            texelSprite = texture(texSpriteSheets[9], scaledUV);
            break;

        case 10u:
            scaledUV = fsUV / fsSpriteSheetSize[10];
            texelSprite = texture(texSpriteSheets[10], scaledUV);
            break;

        case 11u:
            scaledUV = fsUV / fsSpriteSheetSize[11];
            texelSprite = texture(texSpriteSheets[11], scaledUV);
            break;
    }
    return texelSprite;
}

void draw_default_ft4()
{
    vec4 texelPal = PixelToPalette(sample_texture().r);
    outColor = handle_final_color(texelPal, true);
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

#define resolution vec2(640.0, 240.0)

//Stolen from https://www.shadertoy.com/view/4ljfRD
#define pi 3.14159265
float drawLine(vec2 p1, vec2 p2, vec2 uv, float a)
{
    float r = 0.;
    float one_px = 1. / resolution.x; //not really one px
    
    // get dist between points
    float d = distance(p1, p2);
    
    // get dist between current pixel and p1
    float duv = distance(p1, uv);

    //if point is on line, according to dist, it should match current uv 
    r = 1.-floor(1.-(a*one_px)+distance (mix(p1, p2, clamp(duv/d, 0., 1.)),  uv));
        
    return r;
}

float drawThinnerLine(vec2 p1, vec2 p2, vec2 uv, float a)
{
    float r = 0.;
    float one_px = 1. / resolution.x / 2; //not really one px
    
    // get dist between points
    float d = distance(p1, p2);
    
    // get dist between current pixel and p1
    float duv = distance(p1, uv);

    //if point is on line, according to dist, it should match current uv 
    r = 1.-floor(1.-(a*one_px)+distance (mix(p1, p2, clamp(duv/d, 0., 1.)),  uv));
        
    return r;
}

void drawDiamond(vec2 pos, vec4 colour)
{
    float one_pxX = 1. / resolution.x;
    float one_pxY = 1. / resolution.y;

    vec2 posPX = vec2(pos.x * one_pxX, pos.y * one_pxY);
    float halfWidth = one_pxX * 13;
    float halfHeight = one_pxX * 16;

    vec2 left = vec2(posPX.x, posPX.y + halfHeight);
    vec2 bottom = vec2(posPX.x + halfWidth, posPX.y);
    vec2 right = vec2(posPX.x + halfWidth + halfWidth, posPX.y + halfHeight);
    vec2 top = vec2(posPX.x + halfWidth, posPX.y + halfHeight + halfHeight);

    vec2 onePxOffsetX = vec2(one_pxX, 0);
    vec2 onePxOffsetY = vec2(0, one_pxX);

    bool isPixelOnAnyLine = false;
    if(drawLine(top + onePxOffsetY, left, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
    {
        isPixelOnAnyLine = true;
    }
    if(drawLine(top - onePxOffsetX + onePxOffsetY + onePxOffsetY + onePxOffsetY, right, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
    {
        isPixelOnAnyLine = true;
    }
    if(drawLine(right - onePxOffsetX - onePxOffsetX, bottom - onePxOffsetX, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
    {
        isPixelOnAnyLine = true;
    }
    if(drawLine(bottom + onePxOffsetX, left + onePxOffsetY + onePxOffsetX, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
    {
        isPixelOnAnyLine = true;
    }
    if(isPixelOnAnyLine)
    {
        outColor = vec4(colour.x, colour.y, colour.z, colour.w);
    }
}

void drawNumber(int whichNumber, vec2 pos, vec4 colour)
{
    float one_pxX = 1. / resolution.x;
    float one_pxY = 1. / resolution.y;

    vec2 halfPxOffsetX = vec2(one_pxX / 2, 0);
    vec2 halfPxOffsetY = vec2(0, one_pxY / 2);

    vec2 posPX = vec2(pos.x * one_pxX, pos.y * one_pxY) + halfPxOffsetX - halfPxOffsetY;
    float width = one_pxX * 10;
    float height = one_pxY * 9;

    vec2 topLeft = vec2(posPX.x, posPX.y + height - one_pxY);
    vec2 topRight = vec2(posPX.x + width, posPX.y + height - one_pxY);

    vec2 middleLeft = vec2(posPX.x, posPX.y + height / 2) - halfPxOffsetY;
    vec2 middleRight = vec2(posPX.x + width, posPX.y + height / 2) - halfPxOffsetY;

    vec2 bottomLeft = vec2(posPX.x, posPX.y);
    vec2 bottomRight = vec2(posPX.x + width, posPX.y);

    vec2 topMiddle = vec2(posPX.x + width / 2, posPX.y + height - one_pxY);
    vec2 bottomMiddle = vec2(posPX.x + width / 2, posPX.y);

    bool isPixelOnAnyLine = false;
    switch (whichNumber) {
    case 0:
        if(drawLine(topLeft, topRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(bottomLeft, bottomRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }

        if(drawLine(topLeft, bottomLeft, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(topRight, bottomRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        break;
    case 1:
        if(drawLine(topMiddle, bottomMiddle, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        break;
    case 2:
        if(drawLine(topLeft, topRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(middleLeft, middleRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(bottomLeft, bottomRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }

        if(drawLine(topRight, middleRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(middleLeft, bottomLeft, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        break;
    case 3:
        if(drawLine(topLeft, topRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(middleLeft + halfPxOffsetX * 4, middleRight - halfPxOffsetX * 4, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(bottomLeft, bottomRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }

        if(drawLine(topRight, middleRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(middleRight, bottomRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        break;
    case 4:
        if(drawLine(topLeft, middleLeft, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(middleLeft, middleRight - halfPxOffsetX * 4, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(topRight, bottomRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        break;
    case 5:
        if(drawLine(topLeft, topRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(middleLeft, middleRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(bottomLeft, bottomRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }

        if(drawLine(topLeft, middleLeft, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(middleRight, bottomRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        break;
    case 6:
        if(drawLine(topLeft, topRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(middleLeft + halfPxOffsetX * 4, middleRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(bottomLeft, bottomRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }

        if(drawLine(topLeft, bottomLeft, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(middleRight, bottomRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        break;
    case 7:
        if(drawLine(topLeft, topRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawThinnerLine(topRight + halfPxOffsetX + halfPxOffsetY * -1, bottomMiddle, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        break;
    case 8:
        if(drawLine(topLeft, topRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(middleLeft + halfPxOffsetX * 4, middleRight - halfPxOffsetX * 4, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(bottomLeft, bottomRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }

        if(drawLine(topLeft, bottomLeft, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(topRight, bottomRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        break;
    case 9:
        if(drawLine(topLeft, middleLeft, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(middleLeft, middleRight - halfPxOffsetX * 4, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(topRight, bottomRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(topLeft, topRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        break;
    case -1: //Infinity Sign
        vec2 middleLeftLeft = middleLeft - vec2(width / 2, 0) - halfPxOffsetX * 4;
        vec2 middleRightRight = middleRight + vec2(width /2, 0) + halfPxOffsetX * 4;
        vec2 absoluteMiddle = middleLeft + vec2(width / 2, 0);

        vec2 roundTopLeft = middleLeftLeft + vec2((width / 2) / 3, (height / 2) * 2 / 3) - halfPxOffsetY;
        vec2 roundBottomLeft = middleLeftLeft + vec2((width / 2) / 3, -(height / 2) * 2 / 3) + halfPxOffsetY;

        vec2 roundTopRight = middleRightRight - vec2((width / 2) / 3, -(height / 2) * 2 / 3) - halfPxOffsetY + halfPxOffsetX;
        vec2 roundBottomRight = middleRightRight - vec2((width / 2) / 3, (height / 2) * 2 / 3) + halfPxOffsetY + halfPxOffsetX;

        topLeft = topLeft - halfPxOffsetY * 2;
        bottomLeft = bottomLeft + halfPxOffsetY * 2;

        topRight = topRight - halfPxOffsetY * 2 + halfPxOffsetX * 4;
        bottomRight = bottomRight + halfPxOffsetY * 2 + halfPxOffsetX * 4;
        if(drawLine(topLeft, roundTopLeft, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(roundTopLeft, middleLeftLeft, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }

        if(drawLine(middleLeftLeft, roundBottomLeft, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(roundBottomLeft, bottomLeft - halfPxOffsetX * 1, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }

        if(drawLine(topLeft - halfPxOffsetX * 2, absoluteMiddle + halfPxOffsetX * 2, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(bottomLeft - halfPxOffsetX * 2, absoluteMiddle + halfPxOffsetX * 2, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(topRight - halfPxOffsetX, absoluteMiddle + halfPxOffsetX * 1, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(bottomRight - halfPxOffsetX, absoluteMiddle + halfPxOffsetX * 1, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }

        if(drawLine(topRight, roundTopRight + halfPxOffsetX * 2, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(roundTopRight, middleRightRight + halfPxOffsetX * 2, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(middleRightRight + halfPxOffsetX * 2, roundBottomRight, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        if(drawLine(roundBottomRight, bottomRight + halfPxOffsetX * 2, gl_FragCoord.xy / resolution.xy, 1.) > 0.0)
        {
            isPixelOnAnyLine = true;
        }
        break;
    default:
        break;
    }

    if(isPixelOnAnyLine)
    {
        outColor = vec4(colour.x, colour.y, colour.z, colour.w);
    }
}

void main()
{
    int drawMode = int(fsFlags.x);

    switch (drawMode)
    {
        case DRAW_FLAT:
            draw_flat();
            break;

        case DRAW_DEFAULT_FT4:
            draw_default_ft4();
            break;

        case DRAW_CAM:
            draw_cam();
            int offset = 20;
            drawDiamond(vec2(222,100), vec4(64.0/256.0, 88.0/256.0, 16.0/256.0, 1));
            drawNumber(1, vec2(256,100), vec4(64.0/256.0, 88.0/256.0, 16.0/256.0, 1));
            drawNumber(2, vec2(256 + offset * 1,100), vec4(64.0/256.0, 88.0/256.0, 16.0/256.0, 1));
            drawNumber(3, vec2(256 + offset * 2,100), vec4(64.0/256.0, 88.0/256.0, 16.0/256.0, 1));
            drawNumber(4, vec2(256 + offset * 3,100), vec4(64.0/256.0, 88.0/256.0, 16.0/256.0, 1));
            drawNumber(5, vec2(256 + offset * 4,100), vec4(64.0/256.0, 88.0/256.0, 16.0/256.0, 1));
            drawNumber(6, vec2(256 + offset * 5,100), vec4(64.0/256.0, 88.0/256.0, 16.0/256.0, 1));
            drawNumber(7, vec2(256 + offset * 6,100), vec4(64.0/256.0, 88.0/256.0, 16.0/256.0, 1));
            drawNumber(8, vec2(256 + offset * 7,100), vec4(64.0/256.0, 88.0/256.0, 16.0/256.0, 1));
            drawNumber(9, vec2(256 + offset * 8,100), vec4(64.0/256.0, 88.0/256.0, 16.0/256.0, 1));
            drawNumber(0, vec2(256 + offset * 9,100), vec4(64.0/256.0, 88.0/256.0, 16.0/256.0, 1));
            drawNumber(-1,vec2(256 + offset * 10 + 10,100), vec4(64.0/256.0, 88.0/256.0, 16.0/256.0, 1));
            break;

        case DRAW_FG1:
            draw_fg1();
            break;

        case DRAW_GAS:
            draw_gas();
            break;
    }
}
)";
