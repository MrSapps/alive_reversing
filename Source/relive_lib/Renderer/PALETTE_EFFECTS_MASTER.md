# Palette (CLUT) effects on `master`: reference

This documents how palettes work on the `master` branch, and every effect that changes them, in both
Abe's Oddysee (AO, `Source/AliveLibAO`) and Abe's Exoddus (AE, `Source/AliveLibAE`, plus
`Source/AliveLibCommon`).

All `file:line` references are to the master worktree (`build-master-src/`, commit `bcf93fac2`), relative to
`Source/`. "AE" means `AliveLibAE/`, "AO" means `AliveLibAO/`. Code snippets are quoted verbatim, cut down
where marked.

Contents
1. [Data model](#1-data-model)
2. [How the renderers use palettes](#2-how-the-renderers-use-palettes)
3. [Runtime palette effects](#3-runtime-palette-effects)
4. [Static and per-level palette swaps](#4-static-and-per-level-palette-swaps)
5. [Things that look like palette effects but are not](#5-things-that-look-like-palette-effects-but-are-not)
6. [Summary table](#6-summary-table)
7. [Bugs, oddities and open questions](#7-bugs-oddities-and-open-questions)

---

## 1. Data model

### 1.1 The PSX 16-bit CLUT entry

Every palette in the game data, and every palette the game code builds or edits, uses the PSX 15-bit
format. Each entry is a little-endian `u16`:

```
bit 15   14..10   9..5   4..0
 STP       B        G      R        (5 bits per channel)
```

- `0x0000` (all zero, STP clear) is the **transparent** texel. Both renderers skip it.
- `0x8000` (black with STP set) is an **opaque black**. If the prim is semi-transparent it is
  blended black instead.
- STP on a non-zero entry marks the texel as **semi-transparent**. It only has an effect when the prim's
  semi-trans code bit is set (see 2.3).

Building a colour: `Pal_Make_Colour_4834C0` (AE `VRam.cpp:408`; AO `Pal_Make_Colour_447950`,
AO `VRam.cpp:45`, forwards to it):

```cpp
return (bOpaque != 0 ? 0x8000 : 0) + ((u32) r >> 3) + 4 * ((g & 0xF8) + 32 * (b & 0xF8));
```

Despite its name, `bOpaque` sets the STP bit. The only user is Electrocute (3.1).

### 1.2 Palette depths

An animation's palette depth comes from frame 0 of the animation when it is initialised:

| Frame colour depth | Palette depth | Notes |
|---|---|---|
| 4 bpp | 16 | AE `Animation.cpp:988-996`, AO `Animation.cpp:817-821` |
| 8 bpp | 64 if the CLUT length word is 64, else 256 | AE reads the length as a `u32` (`Animation.cpp:1001`); AO reads it as a `u16` (`Animation.cpp:826`) |
| 16 bpp | 0 (no palette) | Only FG1 and camera data in practice |

A CLUT blob in an animation file is `[u32 entry count][entries...]`. That is why every upload passes
`pClut + 4` (AE `Animation.cpp:1044`, `:1077`; AO `Animation.cpp:866`, `:953`). Standalone `Resource_Palt`
resources (`'Palt'` = `0x746C6150`, AE `ResourceManager.hpp:27`, AO `ResourceManager.hpp:34`) use the same
layout and are loaded with `palOffset = 0`.

"8-bit" animations are really 6-bit. Type 3 compression stores 6-bit indices (`& 0x3F` at AE
`Compression.cpp:99-122`), and the direct renderer only converts 64 entries for 8-bit frames
(`PSX_Render_Poly_FT4_Direct_Impl<64>`, AE `PsxRender.cpp:1909-1917`). Entries above 63 of a 256-entry
palette are unreachable from type 3 frames.

### 1.3 The palette area in VRAM, and allocation

The emulated PSX VRAM is 1024×512 16-bit words (`sPsxVram_C1D160`, AE `Psx.cpp:41`).

| Region | Purpose | Source |
|---|---|---|
| (0,0)-(639,271) reserved | Frame buffer plus palette rows | AE `PsxDisplay.cpp:231` |
| (0,240)-(639,271): 32 rows | Palette area | `Pal_Area_Init_483080(0, 240, 640, 32)` at AE `PsxDisplay.cpp:232` and AO `VRam.cpp:12` (`Pal_Reset_4476C0`, which ignores its arguments) |
| (0,272)-(639,511) | Camera background (16×240 strips) | AE `ScreenManager.cpp:209-213`, AO `ScreenManager.cpp:126-130` |

The allocator is AE `VRam.cpp:259-381`, and AO forwards to it (AO `VRam.cpp:16-24`).
- There is one `s32` bitmask per palette row: `sPal_table_5C9164`.
- Each bit is a 16-entry slot, at `x = pal_xpos + 16*bit`.
- A 16-colour palette takes 1 bit (mask `1`), a 64-colour palette 4 bits (`0xF`), a 256-colour palette
  16 bits (`0xFFFF`). Any other count fails (`:305`).
- The result is `{x = 16*bitIdx, y = 240 + row, w = count}` (`VRam.cpp:339-344`).
- `Pal_free_483390` XORs the same mask back out (`:364-381`). The caller has to pass the right depth.

Because the table entries are 32-bit, only slots 0-31 (x 0..511) can actually be addressed, even though
the area is 640 wide (`pal_width = 640/4 = 160`, `:388`). `(1 << (pal_width + 1)) - 1` at `:271` is a
shift larger than 32 (UB). See section 7.

During demo recording or playback, a failed allocation does not fail. It returns `{0,0}` (the top-left of
the frame buffer, `VRam.cpp:351-359`, the "Fat pal alloc hax"). Palette writes then land in the visible
frame buffer area.

Renderer wrappers (`IRenderer.hpp:31-58`): `PalRecord{x, y, depth}`, `PalAlloc`, `PalSetData`, `PalFree`.
- Software (`Renderer/SoftwareRenderer.cpp:38-60`): `PalAlloc` is `Pal_Allocate_483110`. `PalSetData`
  loads a `{x, y, w=depth, h=1}` rect with `PSX_LoadImage16_4F5E20`. `PalFree` is `Pal_free_483390`.
- OpenGL (`OpenGLRenderer.cpp:865-897`): the same allocator. `PalSetData` calls `Upload(e16Bit, {x,y,depth,1})`,
  which only feeds the GL palette cache (2.5).
- DX9 (`DirectX9Renderer.cpp:109-120`): stubs. `PalAlloc` returns false.

`PalRecord::depth` is used as a plain width by partial writes: the Electrocute overwriter writes 8 entries
at `x + index` (3.1).

### 1.4 How a sprite selects a palette

The clut field of a prim (`tpage_clut_pad` of `mUv`, `Primitives_common.hpp:669-678`) is the PSX encoding:

```cpp
// AE Primitives.cpp:149
return (y << 6) | ((x >> 4) & 63);
```

Readers turn it back into a VRAM address: `vram + 32*((clut & 63) + (clut >> 6 << 6))` bytes, which is
`x = 16*(clut & 63)`, `y = clut >> 6` (AE `PsxRender.cpp:1676`, `:2442`, `:3575`, `:3764`).

The tpage (`PSX_getTPage_4F60E0`, AE `Primitives.cpp:144`) carries the texture mode (bits 7-8: 0 = 4-bit,
1 = 8-bit, 2 = 16-bit) and the **ABR blend mode** (bits 5-6).

Animations set both on every render:

```cpp
// AE Animation.cpp:497-501
Poly_Set_SemiTrans_4F8A60(&pPoly->mBase.header, field_4_flags.Get(AnimFlags::eBit15_bSemiTrans));
Poly_Set_Blending_4F8A20(&pPoly->mBase.header, field_4_flags.Get(AnimFlags::eBit16_bBlending));
SetRGB0(pPoly, field_8_r, field_9_g, field_A_b);
SetTPage(pPoly, static_cast<u16>(PSX_getTPage_4F60E0(textureMode, field_B_render_mode, field_84_vram_rect.x, field_84_vram_rect.y)));
SetClut(pPoly, static_cast<u16>(PSX_getClut_4F6350(field_8C_pal_vram_xy.field_0_x, field_8C_pal_vram_xy.field_2_y)));
```

AO is the same at AO `Animation.cpp:559-564`.

The prim code bits (`Primitives.cpp:92-116`):
- **bit 1 (0x02): semi-transparent.**
- **bit 0 (0x01): "Blending"** in this codebase's naming. This is the PSX *raw texture* bit: set means
  "do not modulate by the vertex colour".
- The software renderer also forces bit 0 on when the vertex colour is about 128
  (`(r & 248) == 128` for all three channels, AE `PsxRender.cpp:2167-2173`).

### 1.5 Who owns a CLUT

- **AE** `Animation::Init_40A030(..., u8 bOwnsPalData, ...)` (AE `Animation.cpp:898`). With
  `eBit17_bOwnPal` set (`:937-940`) it allocates a CLUT and uploads frame 0's CLUT (`:1031-1045`). It frees
  the CLUT at `:806-809` (`Animation_Pal_Free_40C4C0`).
- **AO** `Animation::Init_402D20` always allocates when `pal_depth > 0` (AO `Animation.cpp:853-868`) and
  always frees (`:652-655`).
- **Every caller on master passes `bOwnsPalData = 1`**, so every Animation has a private CLUT. No two
  animations share one through Init.
- Sharing happens only through hand-built prims that copy the owner's clut:
  - AE `AnimationUnknown` copies the parent's whole poly, clut included (AE `AnimationUnknown.cpp:34`).
  - AO HintFly sprites (`HintFly.cpp:1437-1453`).
  - AE/AO Blood (AE `Blood.cpp:124-128`, AO `Blood.cpp:121-125`).
  - AE/AO ZapLine (AE `:98`, AO `:117-120`).
  - AE Water (`Water.cpp:121-125`).

  Any edit to the owner's CLUT shows on these too.

The only ways palette data reaches an animation's CLUT after Init:
- `Animation::Load_Pal_40A530(ppData, palOffset)` (AE `Animation.cpp:1066-1079`) and
  `LoadPal_403090` (AO `Animation.cpp:943-955`). Both upload `field_90_pal_depth` entries from
  `(*ppData)[palOffset] + 4`, are a no-op for a null `ppData`, and are fatal for a depth other than
  16/64/256.
- Direct `Pal_Set` / `PalSetData` calls by effects (section 3).

`Set_Animation_Data` **never reloads the palette**. After switching an animation to a different resource,
the CLUT still holds the old resource's palette until someone calls `Load_Pal` with the new frame's
`field_0_clut_offset`. Several callers do exactly that (section 4.2).

### 1.6 Uploading and reading back palettes (Software renderer)

**Upload.** `Pal_Set_483510` (AE `VRam.cpp:413-420`; AO `Pal_Set_447990`, `VRam.cpp:36-43`) leads to
`PalSetData`, which leads to `PSX_LoadImage16_4F5E20` (AE `Psx.cpp:753-767`). That converts the PSX
entries to the PC VRAM format with `PSX_Pal_Conversion_4F98D0`:

```cpp
// AE PsxRender.cpp:1057-1063
pConverted[i] = ((pDataToConvert[i] >> 15) << sSemiTransShift_C215C0)   // STP -> bit 5
              | ((pDataToConvert[i] & 31) << sRedShift_C215C4)          // R   -> bits 11..15
              | (((pDataToConvert[i] >> 5) & 31) << sGreenShift_C1D180) // G   -> bits 6..10
              | (((pDataToConvert[i] >> 10) & 31) << sBlueShift_C19140);// B   -> bits 0..4
```

The shifts are fixed by `PSX_EMU_SetDispType_4F9960` (only type 2 is supported): R=11, G=6, B=0, STP=5
(AE `PsxRender.cpp:990-993`). So in the PC VRAM a CLUT entry is `RRRRR GGGGG S BBBBB`, an RGB565 word whose
green LSB is the STP flag. Every software rasteriser tests `& 0x20` for semi-transparency and `== 0` for
transparency.

**Read back.** `Pal_Copy_483560` (AE `VRam.cpp:399-406`; AO `Pal_Copy_4479D0`) calls
`PSX_StoreImage_4F5E90` (AE `Psx.cpp:703-751`), which converts back to PSX format. The round trip is
lossless (`(vram >> 5)` truncated to `u16` keeps only the STP bit):

```cpp
// AE Psx.cpp:743
*(pDstIter) = static_cast<u16>(shiftedRed | 32 * (shiftedGreen | 32 * (shiftedBlue | 32 * (shiftedTrans))));
```

`PSX_LoadImage_4F5FB0` (AE `Psx.cpp:662`) copies raw data with **no** conversion. AE Water uses it to poke
one CLUT entry (3.6).

**There is no palette cache.** Every software draw reads the CLUT straight from `sPsxVram` when the prim is
rasterised (AE `PsxRender.cpp:1676`, `:2442`, `:3575`, `:3764`).
- Prims are rasterised when the ordering table is drawn (`PSX_DrawOTag_4F6540`), after all objects' `VRender`.
- So what a sprite shows is the CLUT contents **at OT-draw time**: the last write of the frame wins.
- Effects that write palettes in `VRender` (Electrocute's overwriters, glowing eyes) or in `VUpdate`
  (InvisibleEffect, UXB, Mudokon) are all seen in the same frame.

---

## 2. How the renderers use palettes

The default renderer is **Software**. `RENDERER_OPENGL` defaults to `OFF` (`options.cmake:19`), and
`VGA.cpp:282-286` chooses OpenGL only when it is set. Software is the original PC PSX-emulation
rasteriser, so it is the reference for "faithful".

### 2.1 Software: colour modulation and the CLUT

Vertex RGB modulation goes through LUTs built in `PSX_EMU_SetDispType_4F9960` (AE `PsxRender.cpp:1021-1042`):

```cpp
// value = min(31, i * j / 16), where i = vertexColour >> 3 and j = 5-bit texel channel
stru_C1D1C0[i].field_0[j] = value;                      // plain 5-bit result
stru_C146C0.r[i][j] = value << redShift; ...            // pre-shifted for OR-ing
```

So `out = min(31, c * (rgb >> 3) / 16)`.
- 128 is the identity (`128 >> 3 = 16`).
- **127 is not the identity** (`127 >> 3 = 15`, so ×15/16). Many objects use 127.
- Saturation happens at 31.

ABR blend LUTs (`CalculateBlendingModesLUT`, AE `PsxRender.cpp:930-975`), with B = background and
F = foreground, per 5-bit channel:

| abr | Formula | LUT |
|---|---|---|
| 0 | `(B + F) / 2` | `value1 = (j + i) / 2` |
| 1 | `min(31, B + F)` | `value2` |
| 2 | `max(0, B - F)` | `value3 = j - i` |
| 3 | `min(31, B + F/4)` | `value4 = j + (i / 4)` |

They are applied by `Calc_Abr_Pixel` (AE `PsxRender.cpp:461-466`).

### 2.2 Software: animation frames (the direct FT4 path)

Animation frames compressed with type 3 (8-bit) or type 6 (4-bit) are **not** put into VRAM. `vRender`
attaches the compressed frame to the poly (`SetPrimExtraPointerHack`, AE `Animation.cpp:596-603`).
`PSX_Render_Convert_Polys_To_Internal_Format_4F7110` then calls the direct rasteriser
(AE `PsxRender.cpp:2179-2227`), which decompresses on the fly. That is
`PSX_Render_Poly_FT4_Direct_Impl<clut_size>` (AE `PsxRender.cpp:1671-1897`):

- `clut_size` is 16 for 4-bit and 64 for 8-bit (`:1899-1917`).
- **If the poly is modulated** (code bit 0 clear) it builds a local, **pre-tinted copy of the CLUT** for
  every draw (`:1706-1764`):
  ```cpp
  const u16 clut_pixel = lut_b[clut_entry & 0x1F] | lut_r[(clut_entry >> 11) & 0x1F] | lut_g[(clut_entry >> 6) & 0x1F];
  clut_local[i] = clut_pixel | (clut_entry & 0x20);   // STP kept
  ```
  - Entries equal to `0` stay 0 (transparent), and entries equal to `0x20` (STP-black) stay `0x20`.
  - In semi-trans mode with abr 0, a tinted entry that becomes exactly `0x20` is replaced with `0x861`
    (`:1728-1734`), a near-black that keeps STP.

  So **the per-object RGB tint (level tint, shadow zones, flicker) is applied to the CLUT entries**, not
  per pixel.
- **If the poly is raw** (bit 0 set) it uses the VRAM CLUT directly. It scans entries 1..63 to set two flags
  (`:1685-1705`):
  - opaque: `skipBlackPixels = 1` if any entry is 0;
  - semi-trans: `unknown = 0` if any entry lacks STP.
- **Writing pixels** (`:1841-1877`):
  - RLE "black" runs are skipped structurally, so index 0 never writes (`:1571-1591`, `:1656`).
  - Opaque: a texel whose CLUT value is 0 is skipped, unless `skipBlackPixels`. The name is backwards: when
    it is set, zero entries **are** written as black.
  - Semi-trans: every texel is written.
    - Texels whose CLUT entry has `0x20` (STP) are ABR-blended; the others are written opaque.
    - When `unknown` holds (abr 1, raw, every entry has STP) all texels are blended.
- 8-bit semi-trans only takes the semi-trans path when `k1_dword_55EF90` is set (always 1,
  AE `Game.cpp:70`; AE `PsxRender.cpp:1922`).

Frames that are uncompressed, or type 2/4/5 (and type 3/6 frames of 256-colour anims, `eBit25_bDecompressDone`),
are uploaded to the animation's VRAM rect (`Animation::UploadTexture`, AE `Animation.cpp:280-350`). They are
drawn by the generic textured-poly rasterisers below.

### 2.3 Software: generic textured prims (VRAM textures, fonts, sprites)

- **Polys:** `PSX_Render_Internal_Format_Polygon_4F7960` (AE `PsxRender.cpp:2564-2642`) picks a rasteriser
  from code bit 0 (raw) and bit 1 (semi-trans):
  - **Raw, opaque** (`_Blending_Opqaue_51CCA0`, `:174-306`): `if (clut_pixel) *pOut = clut_pixel;`.
    0 is transparent; STP is ignored.
  - **Raw, semi** (`_Blending_SemiTrans_51D2B0`, `:681-781`):
    - 8-bit: `0` is transparent, entries with `0x20` are ABR-blended, the others opaque (`:717-727`).
    - **4-bit and 16-bit: every non-zero texel is blended, whatever its STP** (`:743-745`, `:759-762`).
  - **Modulated, semi** (`_NoBlending_SemiTrans_51E890`, `:468-611`): modulate through
    `r_lut/g_lut/b_lut`, then blend only if `clut_pixel & 0x20` (`:529`, `:551`, `:587`).
  - **Modulated, opaque** (`_NoBlending_Opaque_51E140`): modulate; 0 is transparent.
- **SPRT** (`PSX_EMU_Render_SPRT_4bit/8bit/16bit`, AE `PsxRender.cpp:3571-3972`; `DrawOTag_Render_SPRT`
  `:3225-3260`):
  - Raw forces the colour to 128.
  - With RGB 128 + semi-trans, only the 8-bit SPRT checks STP (`:3839`). The 4-bit SPRT and every
    *tinted* semi-trans SPRT blend all non-zero texels.
  - The 16-bit SPRT skips 0 only when `sActiveTPage_578318 >= 0` (`:3955`).

These inconsistencies (PSX hardware always honours STP) are part of the PC renderer's behaviour, so a
faithful reimplementation of the PC game has to reproduce them per path.

### 2.4 Software: palette-less textures

- **Cameras.** 16-bit direct colour, uploaded as 16×240 strips at y=272 with no conversion (AE
  `ScreenManager.cpp:130-180`, AO `ScreenManager.cpp:61-95`, via `Upload(e8Bit, …)` →
  `PSX_LoadImage_4F5FB0`).
- **AE FG1.** A 1-bit mask per pixel. `PSX_Render_PolyFT4_16bit_517990` (AE `PsxRender.cpp:1933-2002`)
  copies the *camera* pixel from `y + 272` wherever the bit is set. There is no palette.
- **AO FG1.** 16-bit PSX texels uploaded with `Upload(e16Bit…)` (AO `FG1.cpp:137`), so they are converted by
  `PSX_LoadImage16`. Drawn raw and opaque (AO `FG1.cpp:153-169`): the 0 texel is transparent.

### 2.5 OpenGL renderer (off by default)

`Renderer/OpenGLRenderer.cpp` keeps its own palette cache and never reads `sPsxVram`:

- **Palette capture.** `Upload()` treats any `h == 1` rect as a palette (`:1458-1468`). Only `e16Bit`
  uploads are captured, which means only those from `PalSetData`.
  - `Renderer_LoadPalette` (`:250-280`) decodes into `PaletteCache::mPalData[256]` RGBA. Writing into an
    existing cache entry at an x offset is supported, so partial writes such as Electrocute's work.
  - Decode (`Renderer_DecodePalette`, `:222-234`):
    ```cpp
    dst[i].R = (((oldPixel >> 0) & 0x1F)) << 2;      // 0..124, i.e. half scale
    dst[i].G = (((oldPixel >> 5) & 0x1F)) << 2;
    dst[i].B = (((oldPixel >> 10) & 0x1F)) << 2;
    dst[i].A = ((oldPixel >> 15) & 0xffff) ? 127 : 255;   // STP -> alpha 127
    ...
    c.mPalData[0].A = 0;                              // index 0 always transparent (:259-262, :274-277)
    ```
  - The half-scale colour is compensated by FT4 vertex colour `rgb / 64.0f` (`:1299-1301`). 128 then gives
    2.0, so 124/255 × 2 ≈ 0.97.
  - SPRTs use vertex colour 1.0 (`:993-997`), so palettized SPRTs come out at about half brightness.
- **Re-upload on every draw.** `Renderer_BindPalette` (`:282-300`) calls `glTexImage2D` for the palette on
  every bind, with no dirty flag. That costs time but means a palette change is always seen.
- **Clut lookup.** `Renderer_ClutToPalette` (`:174-190`) uses `y = (tClut >> 6) & 0xff`. That masks
  palette rows 256-271 (rows 16-31 of the palette area) down to 0-15, so those palettes are never found.
- **Shader** (`GLShader.cpp:251-337`): the index texture is `GL_RED`.
  ```glsl
  vec4 PixelToPalette(float v) { return texture(m_Palette, vec2((v / m_PaletteDepth) * 256, 0.5f)); }
  vFrag = palColored * vec4(m_Color, 1.0f);
  ```
  - 8-bit VRAM textures are uploaded as bytes (`:1534-1536`).
  - 4-bit VRAM textures (fonts) are unpacked to one byte per texel (`Convert4bppTextureFont`, `:383-396`).
  - 8-bit animation frames are decompressed to bytes (`:456-461`).
  - **4-bit animation frames are uploaded packed, two texels per byte, at half width** (`:447-455`), with
    `m_PaletteDepth = 16 * 16` (the `mPalNormMulti` hack, `:452`, `:1368-1371`). The shader then indexes
    by `byte / 16`, which is the high nibble. Only the odd texel of each pair shows, stretched across two
    pixels.
- **Blending.** `Renderer_SetBlendMode` (`:314-337`) maps:
  - abr 0 → `SRC_ALPHA, ONE_MINUS_SRC_ALPHA` (STP entries, alpha 127, about 50 %);
  - abr 1 → `ONE, ONE`;
  - abr 2 → reverse subtract;
  - abr 3 → `CONSTANT_ALPHA(0.25), ONE`.

  `Renderer_ParseTPageBlendMode` is applied to *every* textured FT4 (`:1387`), whatever the prim's
  semi-trans bit.
- **FG1 hack** (`:1340-1356`): `// Hack, set palette texture to our background.` For FG1 polys it binds the
  **camera texture to the palette sampler** (`GL_TEXTURE1`). The shader's `m_FG1` path (`GLShader.cpp:298-305`)
  then outputs `background(bgCoord) * mask`, the same thing the software FG1 copy does.
- **AO FG1 / 16-bit uploads.** `ConvertAOFG1` (`:1431-1443`) gives alpha 0 to STP texels and 255 to all
  others, so black 0 is opaque. That is the opposite of the software rule (0 = transparent, STP = semi); see
  section 7.
- **No readback.** `PSX_StoreImage_4F5E90` reads `sPsxVram` (`Psx.cpp:710-713`: "This is actually download,
  but we have a copy of it stored in the pal cache"). `PalSetData` in GL mode never writes `sPsxVram`, so
  **`Pal_Copy` returns stale data in GL mode**. InvisibleEffect and Electrocute save and restore this
  wrong data.
- **AE Water's poke** via `PSX_LoadImage_4F5FB0` goes through `Upload(e8Bit, h=1)`, which the GL palette
  hook ignores (only e16 is captured), so the poke has no effect in GL.
- **Freeing.** `Vram_free_495A60` in GL builds also calls `PalFree({x, y, 0})` (AE `VRam.cpp:212-218`).
  That frees any palette or texture cached at that point.

### 2.6 DirectX9

`Renderer/DirectX9Renderer.cpp` is a stub. All palette functions are empty (`:109-120`, `:179-181`).

---

## 3. Runtime palette effects

These are the effects that edit CLUT entries frame by frame.

### 3.1 Electrocute (AE and AO)

Files: AE `Electrocute.cpp/.hpp`, AO `Electrocute.cpp/.hpp`.

#### Helper: `PalleteOverwriter`

It sweeps one colour across a CLUT, 8 entries at a time. The logic is identical in both games
(AE ctor `:15-41`, AO ctor `:17-43`).
- **Fields:** `palBuffer[8]` filled with one colour, `pal_x_index = 1`, `pal_w = 8`, `bFirstUpdate = 1`,
  `bDone = 0`, and `pal_colours_count = target field_90_pal_depth`.
- **Index 0 is never touched.**
- **Render** (AE `:117-119`, AO `:82-83`) writes the chunk while the overwriter is not done, using the
  "depth" field as a width ("TODO: FIX ME - abstraction break"):
  ```cpp
  const IRenderer::PalRecord palRec{ static_cast<s16>(field_20_pal_xy.field_0_x + field_C8_pal_x_index), field_20_pal_xy.field_2_y, field_CA_pal_w};
  IRenderer::GetRenderer()->PalSetData(palRec, reinterpret_cast<u8*>(&field_B8_palBuffer[0]));
  ```
- **Update** (AE `:64-93`, AO `:92-121`):
  ```cpp
  if (field_CC_bFirstUpdate || field_CE_bDone) { field_CC_bFirstUpdate = FALSE; }
  else if (field_C8_pal_x_index == field_24_pal_colours_count - 1) { field_CE_bDone = TRUE; }
  else {
      field_C8_pal_x_index += 8;
      if (field_C8_pal_x_index >= field_24_pal_colours_count - 1) field_C8_pal_x_index = field_24_pal_colours_count - 1;
      if (field_C8_pal_x_index + field_CA_pal_w >= field_24_pal_colours_count - 1) field_CA_pal_w = field_24_pal_colours_count - field_C8_pal_x_index;
  }
  ```

Write sequence:

| Depth | Writes (x, w) | Renders | Updates until done |
|---|---|---|---|
| 16 | (1,8) (9,7) (15,1) | 3 | 4 |
| 64 | (1,8) … (49,8) (57,7) (63,1) | 9 | 10 |
| 256 | (1,8) … (241,8) (249,7) (255,1) | 33 | 34 |

Colours (`Pal_Make_Colour`):

| Overwriter | Colour | PSX value | VRAM value |
|---|---|---|---|
| [0] | white `(255,255,255,1)` | 0xFFFF | 0xFFFF |
| [1] | blue `(64,64,255,1)` | 0xFD08 | 0x423F |
| [2] | black `(0,0,0,0)`, only when `bExtraOverwriter` | 0x0000 | 0x0000 = transparent |

#### AE state machine

AE `Electrocute::vUpdate_4E6240`, `:247-369`.

**Ctor** `ctor_4E5E80(pTarget, bExtraOverwriter, bKillTarget)` (`:163-202`):
- Saves the palette only for `eFlyingSlig_54`, `eGlukkon_67`, `eAbe_69` and `eSlig_125` (`:177-189`):
  `malloc(depth*2)` + `Pal_Copy_483560`.

**States:**
- **0** (`:258-278`):
  - Saves RGB. For Abe it is **hard-coded 127,127,127**; for others it is `field_D0..D4`.
  - Sets `e114_Bit11_Electrocuting`.
  - Sets the target's RGB to **255,255,255**. Through the CLUT tint (2.2) that roughly doubles every
    entry, saturating at 31.
- **1** (`:280-315`):
  - Creates overwriter [0], then [1] with `SetUpdateDelay(4)`, then [2] with `SetUpdateDelay(8)` if extra.
- **2** (`:318-350`): when the last overwriter is null or done:
  - `Pal_Set_483510(xy, depth, saved)` restores the saved palette (`:325`).
  - Kill or hide: `VTakeDamage` if `bKillTarget`, otherwise clear `eBit3_Render`.
  - Restores RGB and clears the flag.
- **Ticking the overwriters:** each update Electrocute calls `pPalOverwriter->VUpdate()` itself
  (`:361-367`).

**Delay bug (AE only).** The overwriters are built with `BaseGameObject_ctor_4DBFA0(FALSE, 0)` (`:17`), so
they are not in the update list, and `SetUpdateDelay` has no effect on them. All overwriters therefore
advance in lockstep. In each frame they write the same chunk in draw-list order, so **only the last
overwriter's colour is visible**:
- `bExtra = 1`: a black sweep, i.e. the sprite becomes transparent from index 1 upwards;
- otherwise a blue sweep.

**Timing:** state 2 acts on tick 5 / 11 / 35 (depth 16 / 64 / 256).

**`vStop_4E6150`** (`:371-405`), called on a screen or path change or when the target dies:
- Restores the palette if one was saved, then **always** damages the target.

**AE spawners:**

| Spawner | Call | Extra | Kill |
|---|---|---|---|
| `ElectricWall.cpp:204-213` | `ctor(obj, 1, 1)` | yes | yes |
| `Greeter.cpp:578-586` | `ctor(target, TRUE, TRUE)` | yes | yes |
| `Shrykull.cpp:203-209` | `ctor(obj, 0, 1)` | no | yes |
| `Teleporter.cpp:84-89` | `ctor(Abe, TRUE, FALSE)` | yes | no: Abe is hidden instead |

#### AO state machine

AO `VUpdate_48D5C0`, `:257-353`.
- **Ctor** `ctor_48D3A0(pTarget, bExtra)` has no kill flag. It saves the palette **only for Abe** (`:149-157`).
- **Overwriters** are in the update list (AO `BaseGameObject.cpp:40-45`), so their `field_8_update_delay`
  of 4 and 8 **works**:
  - [1] runs 4 ticks behind [0], and [2] 8 ticks behind.
  - That gives a leading white band, blue 32 entries behind, then black 64 entries behind. Each finished
    entry ends at the last overwriter's colour.
  - Render ignores the delay, so entries 1-8 show the delayed overwriters' colours from the first frame.
- **State 0** saves the target's real RGB and sets it to 255.
- **State 2**, when the last overwriter is done:
  - Abe: `VTakeDamage`, *then* `Pal_Set` restore (`:315-344`), then RGB restore.
  - Others: damage only. **Neither the palette nor RGB is restored.**
- **Spawners:** `ElectricWall.cpp:182-188` (extra) and `Shrykull.cpp:221-227`.
- **Timing:** state 2 fires on tick 9 / 15 / 39 without the extra overwriter, 13 / 19 / 43 with it.

#### Renderer interaction

- White and blue carry STP. They blend only if the target is drawn semi-trans; Abe is semi-trans with abr 0
  (AE `Abe.cpp:1106`).
- Black `0x0000` is transparent. In the direct path, once any entry from 1 up is 0, `skipBlackPixels`
  becomes 0 and those texels are skipped, so the sprite is "erased".

### 3.2 InvisibleEffect (AE only)

Files: AE `InvisibleEffect.cpp/.hpp`.

**Ctor** (`:14-53`):
- `Pal_Copy`s the target's current CLUT twice: `pPal1` is the original, `pPal2` the working copy.
- Saves the anim's SemiTrans and Blending flags and `field_B_render_mode`.

**States** (`vUpdate_45F4A0`, `:96-277`):
- **0** (`:112-116`): sets `render_mode = eBlend_1` every update for the effect's whole lifetime.
- **1** (`:117-138`):
  - `pPal2[8..depth-1] |= 0x8000`, setting STP on entries 8 and up. There is no upload here.
  - Sets `e114_Bit8_bInvisible`, clears `eBit16_bBlending` (so the sprite is modulated), sets
    `eBit15_bSemiTrans`, sets `eBlend_1`.
  - Delay 1, then state 2, or state 3 if `eIsInvisible_Bit3`.
- **2: fade out** (`:139-190`). For entries 8 to depth-1:
  ```cpp
  if (pal[idx] & 0x1F)   { v3 = true; pal[idx] -= 1;    }  // R
  if (pal[idx] & 0x3E0)  { v3 = true; pal[idx] -= 32;   }  // G
  if (pal[idx] & 0x7C00) { v3 = true; pal[idx] -= 1024; }  // B
  if (pal[idx] == 0x8000u) pal[idx] = 0;                   // STP-black -> transparent
  ```
  - If anything changed: `Pal_Set(pPal2)` (`:184`) and `SetUpdateDelay(1)`. That is one step every 2
    frames, at most 31 steps (about 62 frames).
  - When nothing changes, go to state 0.
  - Because the sprite is additive (abr 1) with STP on, it fades towards invisible.
  - **Entries 1-7 are never changed** and stay visible as additive colour.
- **3** (`:191-204`): logged "Unreachable", but it **is** reached by `InstantInvisibility_45FA00` on quickload
  and by the pause-menu cheat (`Abe.cpp:1367-1378`, `PauseMenu.cpp:830-836`).
  - It runs `pPal2[i] &= 0x8000` for i ≥ 8. The comment says "Clear transparent bit", but the code clears
    RGB and keeps STP: every entry becomes STP-black, which is additive black, i.e. invisible.
  - Then `Pal_Set`.
- **4: fade in** (`:205-246`). For entries **1** to depth-1, each channel steps +1 towards `pPal1`, using
  `(pal2 ^ pal1) & mask`.
  - `Pal_Set` (`:237`), `eBlend_1`, `SetUpdateDelay(5)`. That is one step every 6 frames, at most about 186
    frames.
  - Entries that reached 0 lost their STP bit, so they come back **opaque**.
- **5** (`:247-265`):
  - `Pal_Set(pPal1)` restores the exact original.
  - Restores the flags and render mode and clears `bInvisible`.
  - Spawns `PossessionFlicker(target, 16, 255, 128, 128)`, an RGB-only flicker.
- **6**: dies.

**Triggers** (AE Abe):
- Chant with the invisibility power (`Abe.cpp:7537-7566`) → `BecomeInvisible`.
- Timer expiry (`:1676-1684`) → `BecomeVisible`.
- Door enter, Fleech death, death (`:8030`, `:8424`, `:9117`) → `ClearInvisibility`.

### 3.3 Glowing eyes: `renderWithGlowingEyes` (AE Slig, FlyingSlig, CrawlingSlig, Glukkon)

AE `Slig.cpp:653-760`. AO Sligs do not have it.

It runs on every render while visible in the current camera.

1. `r,g,b` = the object's level tint `field_D0..D4` + `ShadowZones_Calculate_Colour_463CE0` at
   (x, bounding mid-y) (`:668-677`).
2. If `!e114_Bit7_Electrocuted` and `(r,g,b)` differs from the cached value (`:678-681`), it rebuilds from
   **frame 0's CLUT** of the current animation (`:687-690`):
   ```cpp
   u16 resultR = static_cast<s16>(auxPalValue * r) >> 7;  if (resultR > 31) resultR = 31;   // same for G, B
   s32 resultMixed = (src[i] & 0x8000) | ((resultR & 31) + 32 * (resultG & 31) + 32 * 32 * (resultB & 31));
   if (resultMixed <= 0 && src[i]) resultMixed = 1;    // a non-zero entry never becomes transparent
   pPalAlloc[i] = resultMixed;
   ...
   pPalAlloc[eyeColourIndices[i]] = src[eyeColourIndices[i]];   // eyes stay unlit
   Pal_Set_483510(anim.field_8C_pal_vram_xy, anim.field_90_pal_depth, pPalAlloc, palRect);
   ```
   - That is `c' = min(31, (c * light) >> 7)`, so 128 is the identity, and STP is kept.
   - The buffer is `s16[64]`; the upload length is `pal_depth`.
3. Sets the anim RGB to 127, so the light is baked into the CLUT (`:727-729`). Note that 127 still means
   ×15/16 in the software LUT (2.1).

| Object | Eye indices | Buffer |
|---|---|---|
| Slig (`Slig.cpp:761-766`) | 61, 62 | `field_178_pPalAlloc[64]` |
| FlyingSlig (`FlyingSlig.cpp:714-719`) | 60, 62 | `field_1F4` |
| CrawlingSlig (`CrawlingSlig.cpp:251-256`) | 60, 62 | `field_11C` |
| Glukkon (`Glukkon.cpp:331-345`) | 63 for StoryAslik/StoryPhleg, else 62 | `field_118` |

Notes:
- It only re-uploads **when the light colour changes**.
- An animation change does not trigger a rebuild, so a resource with a different CLUT keeps the old tinted
  palette until the light changes.
- `e114_Bit7_Electrocuted` is never set in AE, so during Electrocute (RGB 255) the CLUT is rebuilt brighter,
  and the overwriters write on top of it.

### 3.4 UXB red/green blink (AE and AO)

Files: AE `UXB.cpp`, AO `UXB.cpp`.

- The **blink anim** is `Bomb_RedGreenTick`.
  - It has its own CLUT, which starts as frame 0's ("red").
  - It is drawn semi-trans + raw with `eBlend_1` (AE `:43-52`, AO `:173-192`).
- **Green** is the standalone Palt `kGrenflshResID` (1006, from UXB.BND; AE `Factory.cpp:763-767`,
  AO `Factory.cpp:1290-1294`).
- **Red** is restored from the anim's own current frame CLUT:
  ```cpp
  // AE UXB.cpp:445-447 (AO :447-453)
  field_128_animation.Load_Pal_40A530(field_128_animation.field_20_ppBlock, pFrameHeader->field_0_clut_offset);
  ```
- **State machine** (AE `Update_4DF030` `:403-476`, AO `VUpdate_489380` `:409-492`):
  - `eDelay_0` → `eActive_1` after 10 frames.
  - `eActive_1` runs after 2 more frames:
    - If `red_blink_count != 0`: decrement it, and when it reaches 0 load **green** (AE `:439`, AO `:441`).
    - Otherwise load **red**, take the next pattern digit, and reset the count.
  - One blink is 12 frames.
  - Digit N gives N red blinks then one green blink. A digit 0 skips green.
- **Green also loads** on disarmed starts (AE `:188`, `:212`; AO `:70`, `:106`) and on AE save restore
  (`:627`).
- **Slapping while green** disarms with no palette change.

### 3.5 Mudokon emotion palettes (AE only)

`Mudokon::SetPal_4772D0`, AE `Mudokon.cpp:1282-1325`. The palettes come from MUDPAL.BND Palts held in
`field_10_resources_array` slots 13-17 (`:607-611`).

| Emotion | RGB | Palette |
|---|---|---|
| normal, blind | 63 | slot 16 (`kMudblindResID`) |
| normal | 87,103,67 | own current-frame CLUT (`:1297`) |
| angry / aggressive | 63 | slot 13 (`kMudangry`) |
| sad / suicidal | 63 | slot 14 (`kMudsad`) |
| happy / wired | 74 | slot 15 (`kMudwired`) |
| sick | 63 | slot 17 (`kMudsick`) |

When it is applied:
- Every update while speaking or punching (`:5806-5821`, `:6511-6551`), and back to normal on the last frame.
- Reset to normal in `vTakeDamage` (`:1542`, `:1577`, `:1609`, `:1681`, `:1707`, `:1743`, `:1761`, `:1773`,
  `:1798`, `:1841`).
- A blind Mud calls it once in the ctor (`:613-616`).

### 3.6 Water (AE only): single-entry poke

```cpp
// AE Water.cpp:138-146
rect.x = field_20_animation.field_8C_pal_vram_xy.field_0_x + 1; rect.w = 1; rect.h = 1; ...
const u8 zeroedData[4] = {};
PSX_LoadImage_4F5FB0(&rect, zeroedData); // TODO: FIX ME - won't work with other renderers
```

- This makes **CLUT entry 1** of the water anim `0x0000`, i.e. transparent.
- The write is raw VRAM: it bypasses `PalSetData` and the conversion (0 is 0 in both formats).
- The drops are FT4s using that CLUT, semi-trans + raw, with **abr 3** (`:105-125`).

### 3.7 LCDScreen palette swap (AE and AO)

- Each LCDScreen has two 16-entry CLUTs:
  - the font's own: `sLCDScreen_Palette` (AE `LCDScreen.cpp:16`, AO `:21`);
  - a second, allocated with `PalAlloc`/`PalSetData(sLCDScreen_Palette2)` (AE `:195-209`, AO `:231-246`).
- Each time a message ends, it **swaps the clut rect** (AE `:281-283`, AO `:379-381`):
  ```cpp
  auto palSwap = field_98_pal_rect;
  field_98_pal_rect = field_60_font.field_28_palette_rect;
  field_60_font.field_28_palette_rect = palSwap;
  ```
- No data is re-uploaded.
- The palettes differ at idx 7 (cyan `CE65` ↔ dark red `8405`) and idx 10 (AE: green `8E60` ↔ red `9413`;
  AO likewise, and both AO palettes have idx 8 = `E739`).
- The text "flicker" is **not** a palette effect: it is a per-glyph random vertex RGB of 127±40.

---

## 4. Static and per-level palette swaps

These are one-off `Load_Pal` / `PalSetData` calls with no per-frame arithmetic.

### 4.1 Per-level, object-variant and Stockyards palettes

| What | Where | Palette | Notes |
|---|---|---|---|
| AE `SetBaseAnimPaletteTint_425690` | `BaseAliveGameObject.cpp:440-454` | Palt by id, if resident | Sets RGB via `SetTint` (falls back to the eNone row), then `Load_Pal`. Callers: Slig (Palt 412, `Slig.cpp:599`), TimedMine (Palt 1005, `TimedMine.cpp:115`). **No AE factory loads Palt 412 or 1005, so the palette part is a no-op in AE.** |
| AO `SetBaseAnimPaletteTint_4187C0` | AO `BaseAnimatedWithPhysicsGameObject.cpp:446-470` | Palt by id | Returns early **without RGB or palette** if the level isn't in the table (no eNone fallback). Slig (`Slig.cpp:357`): StockYards/Return → RGB 127 + `E1PAL.BAN` (Palt 412, `Factory.cpp:795`). TimedMine (`TimedMine.cpp:83`): StockYards → RGB 60 + `E1BOMB.BAN` (Palt 1005, `Factory.cpp:704`). |
| AO Gibs | AO `Gibs.cpp:100-111`, `:193-196` | Stockyards only: `ABEE1PAL.BAN` (Palt 25) for Abe/Mud gibs, `DOGE1PAL.BAN` (Palt 576) for Slog gibs | Applied to the 7 parts, **not the head** (the base anim). |
| AE Gibs | AE `Gibs.cpp:121-126`, `:266-271` | `PalRec(mPalOverride)`, only `PalId::BlindMud` → MUDPAL.BND `kMudblindResID` (`AnimResources.cpp:7-9`) | Parts only, not the head. Used by `BlindMud_*_Gib` (`AnimResources.cpp:154-156`). |
| Rock | AE/AO `Rock.cpp:48-61` | Palt `kAberockResID` if resident, else its own frame CLUT | AE never loads that Palt. AO loads `EPUIROCK.BAN` in Stockyards (`Factory.cpp:470`). |
| AO RockSack | `RockSack.cpp:193-197` | Stockyards `E1BAG.BAN` (Palt `kP2c2bagAOResID`, `Factory.cpp:469`) | |
| AO BackgroundGlukkon | `BackgroundGlukkon.cpp:69-73` | Palt `pTlv->field_1A_pal_id`: red/green/blue/aqua 825-828 (`Factory.cpp:2651-2654`) | The suit colour is chosen by the TLV. |
| TestAnimation | AE `TestAnimation.cpp:39-56`, AO `:50-67` | `PalRec(mPalOverride)` | Dev tool. |

### 4.2 Reloading a frame's own CLUT after an animation switch

`Set_Animation_Data` doesn't reload palettes (1.5), so these callers reload the new resource's CLUT
themselves:

| Where | Why |
|---|---|
| AE `Abe.cpp:1103` (quickload) | Puts a clean palette back before invisibility is recreated. |
| AE `PauseMenu.cpp:1692` | Mud mood icon: normal, angry or happy resource. |
| AE `MainMenu.cpp:3201-3207` `Load_Anim_Pal_4D06A0` | Abe head / door anim resource changes (`:621`, `:643`, `:2480`, `:3360`, `:3772-3777`). |
| AO `MainMenu.cpp:837`, `:961` | Abe speak idle; door → AbeIntro. |
| AO `MainMenu.cpp:774`, `:3930` vs `:1704` | Button highlight uses Palt `kHighlitePalAOResID` (8003, HIGHLITE.PAL) on the main screen and its own CLUT on the GameSpeak page. The glow pulse itself is RGB 40..80, ±8 per frame (`:934-946`). AE loads Palt 8003 (`Factory.cpp:116`) but never uses it; the AE glow is RGB only (`MainMenu.cpp:3797-3810`). |
| Mudokon normal (3.5), UXB red (3.4), Rock fallback | |

### 4.3 Font and UI palettes (fixed 16-entry CLUTs)

- **Allocation.** Every `Font` allocates its own 16-entry CLUT and uploads a hard-coded 32-byte array
  (AE `Font.cpp:73-89`, AO `Font.cpp:296-312`). It frees it in the dtor (AE `:101-104`, AO `:496-500`).
- **Atlas.** A 4-bit VRAM texture uploaded with `Upload(e4Bit…)` (AE `Font.cpp:333-372`, AO `:257-282`).
  The font file's own palette is ignored, except for the debug fonts (`LoadFontTypeCustom`,
  AE `Font.cpp:400-417`).
- **Text colour** is vertex RGB, since the polys are modulated (`DrawString`, AE `Font.cpp:128-172`,
  AO `:396-457`).

The palette families:

| Palette | Arrays | STP | Drawn with | Effect |
|---|---|---|---|---|
| "LCD" family | AE `GasCountDown.cpp:12`, `BrewMachine.cpp:7`, `ColourfulMeter.cpp:9`, `LCDStatusBoard.cpp:16`, `LCDScreen.cpp:16/22`; AO `GasCountDown.cpp:17`, `LCDStatusBoard.cpp:13`, `LCDScreen.cpp:21/55` | every non-zero entry | semi + **abr 1** | Fully additive text |
| Pause menu grey ramp | AE `PauseMenu.cpp:31` (`pal_554474`, also used by `Text.cpp`); AO `PauseMenu.cpp:29` | entries 1-4 | semi + abr 0 | Edge pixels 50 % blended |
| Main menu grey ramp | AE `MainMenu.cpp:36`; AO `MainMenu.cpp:229` | entries 1-6 | semi + abr 0 (AO also draws an abr-3 layer, `:3971-4007`) | |
| Debug font | AE `DebugHelpers.cpp` `sDebugFont` palette | none | abr 0/1 | |

AE `DebugHelpers.cpp:2073` (`AnimRenderTest`) allocates with `Pal_Allocate_483110` directly and never
frees it. Its `kTestImagePal` is built with `RGB888toRGB565`, not BGR555.

---

## 5. Things that look like palette effects but are not

These are RGB, blend-mode or prim effects. On Software, the RGB changes still reach the colours as
pre-tinted CLUT entries (2.2).

- **Level tints and ShadowZones.**
  - `SetTint_425600` / `SetTint_418750` set `field_D0..D4` (AE) or `field_C0..C4` (AO).
  - `BaseAnimatedWithPhysicsGameObject` copies them, plus `ShadowZones_Calculate_Colour` (AE
    `ShadowZone.cpp:63-121`), into the anim RGB (AE `BaseAnimatedWithPhysicsGameObject.cpp:118-136`,
    AO `:145-154`).
  - These are vertex-colour modulations, not palette writes.
- **`PossessionFlicker`.** Alternates RGB and sets abr 1 while running, abr 0 after.
- **AE main menu highlight glow** and **AO main menu glow pulse**: RGB.
- **Font flicker**: RGB jitter.
- **UXB / TimedMine / Mine ticking flash animations.** TimedMine arming only switches to the `Bomb_Flash`
  animation and never changes the palette (AE `TimedMine.cpp:382-386`, AO `:369-380`).
- **Tile and gouraud effects:** `DeathFadeOut`, `CircularFade` (abr 2), `MainMenuTransition`,
  `AbilityRing`, `ScreenWave`, `MotionDetector` (abr 1/3, untextured), `ThrowableTotalIndicator`.
- **`Map` `free_all_anim_and_palts`** (AE `Map.cpp:768-772`, AO `:1800-1804`). Only frees the CPU-side
  `Resource_Palt` blocks; VRAM CLUTs are freed by the owners.
- **Blend modes used for semi-transparent (STP) texels**, which decide how STP palette entries look:
  - **abr 0:** most creatures (Abe, UXB main, MovingBomb, Grenade), Blood.
  - **abr 1:** UXB/TimedMine blink, explosions, particles, sparks, BirdPortal, ElectricWall,
    InvisibleEffect, PossessionFlicker, LCD fonts, HintFly, thick ZapLine.
  - **abr 2:** Shadow, CircularFade.
  - **abr 3:** DoorFlame, AO DoorLight, Water, thin ZapLine, some Particles.
  - BackgroundAnimation takes its mode from the TLV.
  - Animations default to abr 0 (AE `Animation.cpp:967`).

---

## 6. Summary table

| Effect | Object / file | Palette operation | What a faithful reimplementation must preserve |
|---|---|---|---|
| CLUT format | all | PSX BGR555 + STP; 0 = transparent | Keep 15-bit entries and the STP flag per entry. Treat `0x0000` as transparent and `0x8000` as opaque (or semi) black. |
| Palette load timing | Software renderer | CLUT read from VRAM at OT-draw time, no cache | The last palette write in a frame applies to every draw of that frame, including writes made in `VRender`. |
| Vertex tint | Animation, direct FT4 path | Tint baked into a local CLUT copy: `min(31, c*(rgb>>3)/16)` | 128 = identity, 127 = ×15/16; saturate at 31. Keep 0 and 0x20 as they are; `0x861` substitution for abr 0. STP is preserved after tinting. |
| Semi-trans | rasterisers | blend if prim semi and (usually) entry STP | Per-path rules from 2.2/2.3, including the PC paths that ignore STP (raw 4/16-bit semi polys, 4-bit and tinted SPRTs). |
| Electrocute AE | `AliveLibAE/Electrocute.cpp` | `Pal_Copy` save; 8-entry sweeps from index 1 every render; `Pal_Set` restore | Sweep cadence (+8 per update, (63,1) tail), overwriters in lockstep so only the last colour shows (blue, or black/transparent with extra), RGB 255 during the effect, Abe RGB restored to 127, restore only for Abe/Slig/FlyingSlig/Glukkon. |
| Electrocute AO | `AliveLibAO/Electrocute.cpp` | same, overwriters on update list with delays 4/8 | Staggered white/blue/black bands (32/64 entries apart), Abe-only restore, damage before restore. |
| Invisibility | `AliveLibAE/InvisibleEffect.cpp` | STP on entries ≥8, −1 per channel every 2 frames, STP-black→0; fade in +1 every 6 frames from entry 1; exact restore | abr 1 (additive) during the effect, entries 1-7 untouched on fade out, lost STP on entries that hit 0, instant variant (`&= 0x8000`), PossessionFlicker at the end. |
| Glowing eyes | `AliveLibAE/Slig.cpp:653` + FlyingSlig, CrawlingSlig, Glukkon | rebuild from frame-0 CLUT `(c*light)>>7` on light change; eyes unscaled | Rebuild only on a light change, non-zero entries never become 0, STP kept, anim RGB 127. |
| UXB blink | AE/AO `UXB.cpp` | Load Palt Grenflsh (green) / own frame CLUT (red) | Pattern digit timing (12-frame blink), initial red from Init, no palette change when disarming while green. |
| Mud emotions | AE `Mudokon.cpp:1282` | Load Palt angry/sad/wired/blind/sick or own CLUT | The RGB values per emotion; applied every frame while speaking or slapping. |
| Water | AE `Water.cpp:138` | Raw VRAM write: entry 1 = 0 | Entry 1 transparent; drops drawn with abr 3. |
| LCD screen | AE/AO `LCDScreen.cpp` | Swap between two CLUT rects per message | The two palettes and the swap on message change; additive (abr 1) text with 127±40 jitter. |
| Fonts / UI | AE/AO `Font.cpp` and users | Fixed 16-entry CLUT per Font instance | STP layout per palette (all-STP LCD palettes are additive; menu palettes blend only their edge entries). |
| Per-level / variant palettes | SetBaseAnimPaletteTint, Gibs, Rock, RockSack, BackgroundGlukkon | `Load_Pal` from a Palt | AO Stockyards palettes; head gib excluded; AO tint skipped outside the listed levels. |
| Resource switch reload | AE Abe quickload, AE PauseMenu, AE/AO MainMenu | `Load_Pal(frame clut_offset)` | A CLUT survives `Set_Animation_Data`; reload explicitly. |
| Camera / FG1 | ScreenManager, FG1, PsxRender | none (16-bit direct / mask copy) | AE FG1 copies camera pixels; AO FG1 0 = transparent. |
| OpenGL | `Renderer/OpenGLRenderer.cpp` | own palette cache, re-uploaded every draw | (Not reference.) See the bugs in section 7. |

---

## 7. Bugs, oddities and open questions

**Allocator**
1. `Pal_Allocate_Helper` (AE `VRam.cpp:267-296`) does `1 << (pal_width + 1)` with `pal_width = 160`, which is
   UB, and the `s32` row mask can only address 32 × 16 = 512 entries of the 640-wide area.

**Electrocute**

2. AE: the `SetUpdateDelay` on the overwriters is dead, so the intended white→blue→black bands only
   happen in AO.
3. AE with a target that doesn't own a palette (depth 0, not possible on master since every caller owns one):
   the overwriter would write at `x - 1`.
4. AE `vStop` damages the target even for the Teleporter (`bKillTarget = FALSE`).
5. AO leaves non-Abe targets with an overwritten CLUT and RGB 255; they die anyway.

**InvisibleEffect**

6. State 3 is described as "unreachable" and its comment contradicts the code; the fade out starts at entry 8
   but the fade in starts at entry 1.

**Glowing eyes**

7. The 64-entry buffer is uploaded with `pal_depth`; it only rebuilds when the light changes and always uses
   frame 0's CLUT; `e114_Bit7_Electrocuted` is never set.

**Resources that are loaded but never applied** (possibly missing reversed code)
8. AO Stockyards `TBMBPAL.BAN` (UXB, Palt 1037, `Factory.cpp:1285`), `LNDMNPAL.BAN` (Mine, Palt 1036,
   `:1228`) and `E1BAG.BAN` for MeatSack (`:1817`).
9. AE Palt 8003 (highlight), 412, 1005, Aberock and Grenade Palts are never loaded.

**Renderer behaviour**

10. The software renderer's STP handling is inconsistent between paths (2.3). A port that "fixes" it to PSX
    rules will look different from the PC game.
11. The 8-bit SPRT tinted semi path and the 4-bit SPRT blend every non-zero texel.

**OpenGL renderer**

12. `Pal_Copy` reads stale VRAM, which breaks Electrocute and InvisibleEffect saves.
13. The Water poke is ignored.
14. Clut y is masked with `& 0xff`, so palette rows 256-271 are not found.
15. 4-bit animations show half resolution (high nibble only).
16. SPRT palettes come out at half brightness.
17. Blending applies to every textured FT4 regardless of the semi-trans bit.
18. AO FG1 alpha comes from STP rather than `!= 0`.

**Unverified against game data**

19. Which font atlas indices are actually used, the actual colours of the UXB "red" (BOMBFLSH frame CLUT)
    and "green" (GRENFLSH) palettes, and whether AO FG1 data uses STP to mark transparency (which would
    explain the GL `ConvertAOFG1` rule).
