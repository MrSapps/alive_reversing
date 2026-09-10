#pragma once

#include <string_view>
#include <unordered_map>
#include "Math.hpp"
#include "../relive_lib/Primitives.hpp"
#include "../relive_lib/ResourceManagerWrapper.hpp"
#include "Layer.hpp"

class OrderingTable;
class FontString;

struct File_Font final
{
    s16 mWidth;
    s16 mHeight;
    s16 mColorDepth;
    s16 mPaletteSize;
    u16 mPalette[32/2];
    u8 mPixelBuffer[1];
};
//ALIVE_ASSERT_SIZEOF(File_Font, 0x28);
// Can vary in size

struct Font_AtlasEntry final
{
    u8 x;
    u8 mY;
    u8 mWidth;
    u8 mHeight;
};

enum class GlyphSize : u8
{
    Normal,
    Small,
};

// A glyph that isn't looked up by codepoint - button/dpad prompts, and any other icon-like
// glyph a {MacroName} in a FontString can refer to. Shared between AO and AE so both games'
// control-byte macros can be translated onto the same vocabulary (see ControlByteToNamedGlyph
// in each game's Input.cpp) and so new text can be authored with e.g. "{Button_A}" directly.
enum class NamedGlyph : u8
{
    Button_A,
    Button_B,
    Button_C,
    Button_D,
    Button_E,
    Button_F,
    Button_G,
    Button_H,
    Dpad,
    Dpad_Up,
    Dpad_Down,
    Dpad_Left,
    Dpad_Right,
    // Meta entries: not real glyphs, just where FontContext::GetGlyphSpacing()/GetSpaceWidth()
    // pull their per-atlas values from (this used to be array indices 0 and 1).
    GlyphSpacingMeta,
    SpaceWidthMeta,
};

// Key into a font atlas map. Replaces looking glyphs up by their rendered UTF-8 text (which
// can't tell two same-looking glyphs of different sizes apart, and needs a std::string alloc
// + hash per glyph per draw) with a single dense integer: either a unicode codepoint (optionally
// tagged with a size variant), or a NamedGlyph.
class GlyphId final
{
public:
    GlyphId() = default;
    static GlyphId FromCodepoint(char32_t codepoint, GlyphSize size = GlyphSize::Normal);
    static GlyphId FromNamed(NamedGlyph namedGlyph, GlyphSize size = GlyphSize::Normal);

    // Decodes the first UTF-8 codepoint of utf8Text and builds a GlyphId from it. Convenience
    // for building codepoint keys straight from UTF-8 string literals (avoids having to hand
    // transcribe accented characters to char32_t literals).
    static GlyphId FromUtf8(std::string_view utf8Text, GlyphSize size = GlyphSize::Normal);

    bool operator==(const GlyphId& other) const { return mValue == other.mValue; }

    u32 mValue = 0;
};

struct GlyphIdHash final
{
    size_t operator()(const GlyphId& id) const
    {
        return std::hash<u32>{}(id.mValue);
    }
};

class FontContext final
{
public:
    std::unordered_map<GlyphId, Font_AtlasEntry, GlyphIdHash>* mAtlas = nullptr;
    FontResource mFntResource;

    void LoadFontType(FontType resourceID, ResourceManagerWrapper& resMan);

    // this is used for both button prompt width and space character width
    u8 GetGlyphSpacing();
    u8 GetSpaceWidth();
};


class AliveFont final
{
public:
    AliveFont();
    void Load(s32 maxCharLength, const PalResource& pal, FontContext* fontContext);
    ~AliveFont();
	
    s32 MeasureTextWidth(const char_type* text);
    s32 MeasureTextWidth(const FontString& text);
    s32 MeasureCharacterWidth(char_type character);
    s32 MeasureLeadingGlyphWidth(const char_type* text, size_t& outByteLength);
    s32 MeasureScaledTextWidth(const char_type* text, FP scale);

    s32 DrawString(OrderingTable& ot, const char_type* text, s32 x, s16 y, relive::TBlendModes blendMode, s32 bSemiTrans, s32 disableBlending, Layer layer, u8 r, u8 g, u8 b, s32 polyOffset, FP scale, s32 maxRenderWidth, s16 colorRandomRange);
    s32 DrawString(OrderingTable& ot, const FontString& text, s32 x, s16 y, relive::TBlendModes blendMode, s32 bSemiTrans, s32 disableBlending, Layer layer, u8 r, u8 g, u8 b, s32 polyOffset, FP scale, s32 maxRenderWidth, s16 colorRandomRange);

    const char_type* SliceText(const char_type* text, s32 left, FP scale, s32 right);

public:
    Poly_FT4* mFntPolyArray = nullptr;
    s32 mPolyCount = 0;
    FontContext* mFontContext = nullptr;
    bool mLoaded = false;
};

extern bool gDisableFontFlicker;
extern bool gFontDrawScreenSpace;
