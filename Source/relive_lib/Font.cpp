#include "stdafx.h"
#include "Font.hpp"
#include "FontString.hpp"
#include "../relive_lib/Function.hpp"
#include "../relive_lib/FixedPoint.hpp"
#include "../relive_lib/FatalError.hpp"
#include "../relive_lib/GameType.hpp"
#include "../relive_lib/Primitives.hpp"
#include "../relive_lib/PsxDisplay.hpp"
#include "../relive_lib/BaseMap.hpp"
#include "../relive_lib/data_conversion/AnimationConverter.hpp"
#include "FontResources.hpp"

bool gDisableFontFlicker = false;
bool gFontDrawScreenSpace = false;

namespace
{
    constexpr u32 kGlyphIdNamedFlag = 0x8000'0000u;
    constexpr u32 kGlyphIdSmallFlag = 0x4000'0000u;

    // If text[i] starts a "{Name}" macro or a "{{" escape, describes how many raw bytes it spans;
    // mRawByteLength is left at 0 for anything else (unknown/unterminated macro included), telling
    // the caller to fall back to decoding text[i] as an ordinary character. Mirrors the exact rule
    // FontString::Parse segments text with, kept in one place because both SliceText and
    // AliveFont::MeasureLeadingGlyphWidth need to recognise it directly off raw text - callers that
    // step through a string one rendered glyph at a time rather than through a pre-parsed FontString.
    struct MacroSpan
    {
        size_t mRawByteLength = 0;
        bool mIsEscape = false; // "{{" -> literal '{'; mNamedGlyph is unused in this case
        NamedGlyph mNamedGlyph = NamedGlyph::Button_A;
    };

    MacroSpan TryDecodeMacroAt(const char_type* text, size_t i)
    {
        MacroSpan span;
        if (text[i] != '{')
        {
            return span;
        }

        if (text[i + 1] == '{')
        {
            span.mRawByteLength = 2;
            span.mIsEscape = true;
            return span;
        }

        const char_type* closeBrace = strchr(text + i + 1, '}');
        if (!closeBrace)
        {
            return span;
        }

        const std::string_view macroName(text + i + 1, static_cast<size_t>(closeBrace - (text + i + 1)));
        NamedGlyph namedGlyph = NamedGlyph::Button_A;
        if (!NamedGlyphFromMacroName(macroName, namedGlyph))
        {
            return span;
        }

        span.mRawByteLength = static_cast<size_t>(closeBrace + 1 - (text + i));
        span.mNamedGlyph = namedGlyph;
        return span;
    }
} // namespace

GlyphId GlyphId::FromCodepoint(char32_t codepoint, GlyphSize size)
{
    GlyphId id;
    id.mValue = static_cast<u32>(codepoint) | (size == GlyphSize::Small ? kGlyphIdSmallFlag : 0u);
    return id;
}

GlyphId GlyphId::FromNamed(NamedGlyph namedGlyph, GlyphSize size)
{
    GlyphId id;
    id.mValue = kGlyphIdNamedFlag | static_cast<u32>(namedGlyph) | (size == GlyphSize::Small ? kGlyphIdSmallFlag : 0u);
    return id;
}

// Decodes a single UTF-8 codepoint starting at text[i], advancing i past it. Never reads past a
// null terminator, even on truncated/malformed input.
static char32_t DecodeUtf8Codepoint(const char* text, size_t& i)
{
    const unsigned char leadByte = static_cast<unsigned char>(text[i]);

    char32_t codepoint = leadByte;
    size_t continuationBytes = 0;

    if (leadByte < 0x80)
    {
        continuationBytes = 0;
    }
    else if ((leadByte & 0xE0) == 0xC0)
    {
        codepoint = leadByte & 0x1F;
        continuationBytes = 1;
    }
    else if ((leadByte & 0xF0) == 0xE0)
    {
        codepoint = leadByte & 0x0F;
        continuationBytes = 2;
    }
    else if ((leadByte & 0xF8) == 0xF0)
    {
        codepoint = leadByte & 0x07;
        continuationBytes = 3;
    }

    size_t j = i + 1;
    for (size_t k = 0; k < continuationBytes && text[j] != '\0'; ++k, ++j)
    {
        const unsigned char continuationByte = static_cast<unsigned char>(text[j]);
        if ((continuationByte & 0xC0) != 0x80)
        {
            break;
        }
        codepoint = (codepoint << 6) | (continuationByte & 0x3F);
    }

    i = j;
    return codepoint;
}

GlyphId GlyphId::FromUtf8(std::string_view utf8Text, GlyphSize size)
{
    size_t i = 0;
    const char32_t codepoint = utf8Text.empty() ? 0 : DecodeUtf8Codepoint(utf8Text.data(), i);
    return FromCodepoint(codepoint, size);
}

u8 FontContext::GetGlyphSpacing()
{
    const auto it = mAtlas->find(GlyphId::FromNamed(NamedGlyph::GlyphSpacingMeta));
    return it != mAtlas->end() ? it->second.mWidth : 0;
}

u8 FontContext::GetSpaceWidth()
{
    const auto it = mAtlas->find(GlyphId::FromNamed(NamedGlyph::SpaceWidthMeta));
    return it != mAtlas->end() ? it->second.mWidth : 0;
}

AliveFont::AliveFont()
{
}

void AliveFont::Load(s32 maxCharLength, const PalResource& pal, FontContext* fontContext)
{
    if (mLoaded)
    {
        ALIVE_FATAL("AliveFont mFntPolyArray leaked");
    }

    mFontContext = fontContext;
    mFontContext->mFntResource.mCurPal = pal.mPal;
    mPolyCount = maxCharLength;
    mFntPolyArray = relive_new Poly_FT4[maxCharLength];
    mLoaded = true;
}

AliveFont::~AliveFont()
{
    relive_delete[] mFntPolyArray;
}

static s32 WorldSpaceToScreenSpace(s32 x)
{
    return GetGameType() == GameType::eAo ? PsxToPCX(x, 11) : static_cast<s32>(x / 0.575);
}

static s32 ScreenSpaceToWorldSpace(s32 x)
{
    return GetGameType() == GameType::eAo ? PCToPsxX(x, 20) : static_cast<s32>(x * 0.575);
}

s32 AliveFont::DrawString(OrderingTable& ot, const char_type* text, s32 x, s16 y, relive::TBlendModes blendMode, s32 bSemiTrans, s32 disableBlending, Layer layer, u8 r, u8 g, u8 b, s32 polyOffset, FP scale, s32 maxRenderWidth, s16 colorRandomRange)
{
    return DrawString(ot, FontString(text), x, y, blendMode, bSemiTrans, disableBlending, layer, r, g, b, polyOffset, scale, maxRenderWidth, colorRandomRange);
}

s32 AliveFont::DrawString(OrderingTable& ot, const FontString& text, s32 x, s16 y, relive::TBlendModes blendMode, s32 bSemiTrans, s32 disableBlending, Layer layer, u8 r, u8 g, u8 b, s32 polyOffset, FP scale, s32 maxRenderWidth, s16 colorRandomRange)
{
    if (!gFontDrawScreenSpace)
    {
        x = WorldSpaceToScreenSpace(x);
    }

    s32 characterRenderCount = 0;
    const s32 maxRenderX = WorldSpaceToScreenSpace(maxRenderWidth);
    s16 offsetX = static_cast<s16>(x);
    auto poly = &mFntPolyArray[polyOffset];

    bool stop = false;
    for (const FontString::Segment& segment : text.Segments())
    {
        if (stop)
        {
            break;
        }

        if (segment.mIsNamedGlyph)
        {
            if (offsetX >= maxRenderX)
            {
                break;
            }

            const auto it = mFontContext->mAtlas->find(GlyphId::FromNamed(segment.mNamedGlyph));
            if (it == mFontContext->mAtlas->end())
            {
                LOG_INFO("unsupported named glyph: %d", static_cast<s32>(segment.mNamedGlyph));
                break;
            }

            const Font_AtlasEntry& atlasEntry = it->second;
            const s8 charWidth = atlasEntry.mWidth;
            const auto charHeight = atlasEntry.mHeight;
            const s8 texture_u = static_cast<s8>(atlasEntry.x);
            const s8 texture_v = static_cast<s8>(atlasEntry.mY);
            const s16 widthScaled = static_cast<s16>(charWidth * FP_GetDouble(scale));
            const s16 heightScaled = static_cast<s16>(charHeight * FP_GetDouble(scale));

            poly->SetSemiTransparent(bSemiTrans);
            poly->SetShadeTex(disableBlending);
            poly->SetRGB0(
                static_cast<u8>(r + Math_RandomRange(-colorRandomRange, colorRandomRange)),
                static_cast<u8>(g + Math_RandomRange(-colorRandomRange, colorRandomRange)),
                static_cast<u8>(b + Math_RandomRange(-colorRandomRange, colorRandomRange)));
            poly->SetXY0(offsetX, y);
            poly->SetUV0(texture_u, texture_v);
            poly->SetXY1(offsetX + widthScaled, y);
            poly->SetUV1(texture_u + charWidth, texture_v);
            poly->SetXY2(offsetX, y + heightScaled);
            poly->SetUV2(texture_u, texture_v + charHeight);
            poly->SetXY3(offsetX + widthScaled, y + heightScaled);
            poly->SetUV3(texture_u + charWidth, texture_v + charHeight);
            poly->SetBlendMode(blendMode);
            poly->mFont = mFontContext;
            ot.Add(layer, poly);
            ++characterRenderCount;
            offsetX += widthScaled + FP_GetExponent(FP_FromInteger(mFontContext->GetGlyphSpacing()) * scale);
            poly++;
            continue;
        }

        const char* literal = segment.mLiteralUtf8.c_str();
        size_t i = 0;
        const size_t len = segment.mLiteralUtf8.size();
        while (i < len)
        {
            if (offsetX >= maxRenderX)
            {
                stop = true;
                break;
            }

            const size_t glyphStart = i;
            const char32_t codepoint = DecodeUtf8Codepoint(literal, i);
            if (codepoint == U' ')
            {
                if (GetGameType() == GameType::eAo)
                {
                    offsetX += mFontContext->GetGlyphSpacing();
                }

                offsetX += mFontContext->GetSpaceWidth();
                continue;
            }

            const auto it = mFontContext->mAtlas->find(GlyphId::FromCodepoint(codepoint));
            if (it == mFontContext->mAtlas->end())
            {
                LOG_INFO("unsupported glyph: %.*s", static_cast<int>(i - glyphStart), literal + glyphStart);
                stop = true;
                break;
            }

            const Font_AtlasEntry& atlasEntry = it->second;
            const s8 charWidth = atlasEntry.mWidth;
            const auto charHeight = atlasEntry.mHeight;

            const s8 texture_u = static_cast<s8>(atlasEntry.x);
            const s8 texture_v = static_cast<s8>(atlasEntry.mY);

            const s16 widthScaled = static_cast<s16>(charWidth * FP_GetDouble(scale));
            const s16 heightScaled = static_cast<s16>(charHeight * FP_GetDouble(scale));

            poly->SetSemiTransparent(bSemiTrans);
            poly->SetShadeTex(disableBlending);

            poly->SetRGB0(
                static_cast<u8>(r + Math_RandomRange(-colorRandomRange, colorRandomRange)),
                static_cast<u8>(g + Math_RandomRange(-colorRandomRange, colorRandomRange)),
                static_cast<u8>(b + Math_RandomRange(-colorRandomRange, colorRandomRange)));

            // P0
            poly->SetXY0(offsetX, y);
            poly->SetUV0(texture_u, texture_v);

            // P1
            poly->SetXY1(offsetX + widthScaled, y);
            poly->SetUV1(texture_u + charWidth, texture_v);

            // P2
            poly->SetXY2(offsetX, y + heightScaled);
            poly->SetUV2(texture_u, texture_v + charHeight);

            // P3
            poly->SetXY3(offsetX + widthScaled, y + heightScaled);
            poly->SetUV3(texture_u + charWidth, texture_v + charHeight);

            poly->SetBlendMode(blendMode);

            poly->mFont = mFontContext;

            ot.Add(layer, poly);

            ++characterRenderCount;

            offsetX += widthScaled + FP_GetExponent(FP_FromInteger(mFontContext->GetGlyphSpacing()) * scale);

            poly++;
        }
    }

    return polyOffset + characterRenderCount;
}

s32 AliveFont::MeasureTextWidth(const char_type* text)
{
    return MeasureTextWidth(FontString(text));
}

s32 AliveFont::MeasureTextWidth(const FontString& text)
{
    s32 result = 0;

    for (const FontString::Segment& segment : text.Segments())
    {
        if (segment.mIsNamedGlyph)
        {
            const auto it = mFontContext->mAtlas->find(GlyphId::FromNamed(segment.mNamedGlyph));
            if (it != mFontContext->mAtlas->end())
            {
                result += mFontContext->GetGlyphSpacing();
                result += it->second.mWidth;
            }
            continue;
        }

        const char* literal = segment.mLiteralUtf8.c_str();
        size_t i = 0;
        const size_t len = segment.mLiteralUtf8.size();
        while (i < len)
        {
            const char32_t codepoint = DecodeUtf8Codepoint(literal, i);

            // space or control char (button prompt)
            if (codepoint == U' ')
            {
                result += mFontContext->GetSpaceWidth();
                continue;
            }

            const auto it = mFontContext->mAtlas->find(GlyphId::FromCodepoint(codepoint));
            if (it != mFontContext->mAtlas->end())
            {
                result += mFontContext->GetGlyphSpacing();
                result += it->second.mWidth;
            }
        }
    }

    if (!gFontDrawScreenSpace)
    {
        if (GetGameType() == GameType::eAo)
        {
            result -= mFontContext->GetGlyphSpacing();
        }
        result = ScreenSpaceToWorldSpace(result);
    }

    return result;
}

// Measures the width of a string with scale applied.
s32 AliveFont::MeasureScaledTextWidth(const char_type* text, FP scale)
{
    const FP width = FP_FromInteger(MeasureTextWidth(text));
    return FP_GetExponent((width * scale) + FP_FromDouble(0.5));
}

// Measures the width of a single character.
s32 AliveFont::MeasureCharacterWidth(char_type character)
{
    s32 result = 0;

    if (character <= 32)
    {
        // space or control char (button prompt)
        if (character < (GetGameType() == GameType::eAo ? 8 : 7) || character > 31)
        {
            return mFontContext->GetSpaceWidth();
        }
    }

    const auto it = mFontContext->mAtlas->find(GlyphId::FromCodepoint(static_cast<u8>(character)));
    result = (it != mFontContext->mAtlas->end()) ? it->second.mWidth : mFontContext->GetSpaceWidth();

    if (!gFontDrawScreenSpace)
    {
        // for some reason AO used the same calc as AE here?
        result = static_cast<s32>(result * 0.575); // Convert screen space to world space.
    }

    return result;
}

// Measures the width of the single glyph unit at the start of text, and reports via outByteLength
// how many raw UTF-8 bytes it occupies - 1 for a plain/control byte (matching MeasureCharacterWidth
// exactly), more for a multi-byte codepoint or a whole "{Name}" macro. For anything but the plain
// single-byte case, this can't be answered by looking at text[0] alone, so callers that need to
// step through text one rendered glyph at a time (LCDScreen's scrolling ticker) should use this
// instead of assuming one raw byte is always one glyph.
s32 AliveFont::MeasureLeadingGlyphWidth(const char_type* text, size_t& outByteLength)
{
    const MacroSpan macro = TryDecodeMacroAt(text, 0);
    if (macro.mRawByteLength != 0 && !macro.mIsEscape)
    {
        outByteLength = macro.mRawByteLength;

        const auto it = mFontContext->mAtlas->find(GlyphId::FromNamed(macro.mNamedGlyph));
        const s32 width = (it != mFontContext->mAtlas->end()) ? it->second.mWidth : mFontContext->GetSpaceWidth();
        return gFontDrawScreenSpace ? width : static_cast<s32>(width * 0.575);
    }

    if (macro.mIsEscape)
    {
        // "{{" -> a single literal '{' glyph, spanning 2 raw bytes.
        outByteLength = macro.mRawByteLength;
        return MeasureCharacterWidth('{');
    }

    if (static_cast<unsigned char>(text[0]) >= 0x80)
    {
        size_t i = 0;
        const char32_t codepoint = DecodeUtf8Codepoint(text, i);
        outByteLength = i;

        const auto it = mFontContext->mAtlas->find(GlyphId::FromCodepoint(codepoint));
        const s32 width = (it != mFontContext->mAtlas->end()) ? it->second.mWidth : mFontContext->GetSpaceWidth();
        return gFontDrawScreenSpace ? width : static_cast<s32>(width * 0.575);
    }

    outByteLength = 1;
    return MeasureCharacterWidth(text[0]);
}

// Returns a pointer into text at the character where rendering would be cut off by the given
// left/right region - i.e. the first character that would NOT be shown. Callers (LCDScreen's
// scrolling marquee) compare this pointer across frames to detect when a new glyph has scrolled
// into view, and check whether it's a space to decide whether to play a tick sound.
//
// A {MacroName} named glyph counts as one glyph here (via TryDecodeMacroAt), not several
// unsupported literal characters decoded byte-by-byte, matching how DrawString/MeasureTextWidth
// already treat it through FontString.
const char_type* AliveFont::SliceText(const char_type* text, s32 left, FP scale, s32 right)
{
    s32 xOff = 0;
    s32 rightWorldSpace;
    if (GetGameType() == GameType::eAo)
    {
        rightWorldSpace = PsxToPCX(left, 11);
    }
    else
    {
        rightWorldSpace = static_cast<s32>(right * 0.575);
    }

    if (gFontDrawScreenSpace)
    {
        xOff = left;
    }
    else
    {
        xOff = WorldSpaceToScreenSpace(left);
    }

    size_t i = 0;
    while (text[i] != '\0')
    {
        if (xOff >= rightWorldSpace)
        {
            break;
        }

        const MacroSpan macro = TryDecodeMacroAt(text, i);
        if (macro.mRawByteLength != 0 && !macro.mIsEscape)
        {
            const auto it = mFontContext->mAtlas->find(GlyphId::FromNamed(macro.mNamedGlyph));
            if (it == mFontContext->mAtlas->end())
            {
                LOG_INFO("unsupported named glyph: %d", static_cast<s32>(macro.mNamedGlyph));
                return text + i;
            }

            xOff += static_cast<s32>(it->second.mWidth * FP_GetDouble(scale)) + mFontContext->GetGlyphSpacing();
            i += macro.mRawByteLength;
            continue;
        }

        const size_t glyphStart = i;
        char32_t codepoint;
        if (macro.mIsEscape)
        {
            codepoint = U'{';
            i += macro.mRawByteLength;
        }
        else
        {
            codepoint = DecodeUtf8Codepoint(text, i);
        }

        if (codepoint == U' ')
        {
            xOff += mFontContext->GetSpaceWidth();
            continue;
        }

        const auto it = mFontContext->mAtlas->find(GlyphId::FromCodepoint(codepoint));
        if (it != mFontContext->mAtlas->end())
        {
            xOff += static_cast<s32>(it->second.mWidth * FP_GetDouble(scale)) + mFontContext->GetGlyphSpacing();
        }
        else
        {
            LOG_INFO("unsupported glyph: %.*s", static_cast<int>(i - glyphStart), text + glyphStart);
            return text + glyphStart;
        }
    }

    return text + i;
}

void FontContext::LoadFontType(FontType resourceID, ResourceManagerWrapper& resMan)
{
    if (resourceID == FontType::Debug)
    {
        mFntResource = resMan.LoadFont(FontType::LcdFont);
        mAtlas = &sLcdFontAtlas;

        // TODO:
        //mAtlas = sDebugFontAtlas;
        return;

        mFntResource.mId = resourceID;
        mFntResource.mPngPtr = std::make_shared<PngData>();
        mFntResource.mPngPtr->mPal = std::make_shared<AnimationPal>();

        auto fontFile = reinterpret_cast<File_Font*>(sDebugFont);
        for (s32 i = 0; i < fontFile->mPaletteSize; i++)
        {
            mFntResource.mPngPtr->mPal->mPal[i] = RGBConversion::RGBA555ToRGBA888Components(fontFile->mPalette[i]);
        }
    
        std::vector<u8> newData(fontFile->mWidth * fontFile->mHeight); // TODO *2 was out of bounds?
    
        // Expand 4bit to 8bit
        std::size_t src = 0;
        std::size_t dst = 0;
        while (dst < newData.size())
        {
            newData[dst++] = (fontFile->mPixelBuffer[src] & 0xF);
            newData[dst++] = ((fontFile->mPixelBuffer[src++] & 0xF0) >> 4);
        }
        mFntResource.mPngPtr->mPixels = newData;

        mFntResource.mPngPtr->mWidth = fontFile->mWidth;
        mFntResource.mPngPtr->mHeight = fontFile->mHeight;
        mFntResource.mPngPtr->mPixels.resize(fontFile->mWidth * fontFile->mHeight);
    
        mFntResource.mCurPal = mFntResource.mPngPtr->mPal;
        return;
    }

    FontResource fontRes = resMan.LoadFont(resourceID);
    mFntResource = fontRes;

    // TODO: Will get moved to a json file in FontResource
    switch (resourceID)
    {
        case FontType::PauseMenu:
            mAtlas = &sPauseMenuFontAtlas;
            break;
        case FontType::LcdFont:
            mAtlas = &sLcdFontAtlas;
            break;
        default:
            ALIVE_FATAL("Unknown font resource ID !!!");
            break;
    }
}
