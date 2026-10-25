# Sprites, alpha and palette effects

The original games drew every sprite through a PSX palette (CLUT), and a lot of effects worked by
changing the palette. See `PALETTE_EFFECTS_MASTER.md` for exactly how that worked on `master`.
Sprites here are **true colour**: an 8 bit RGBA PNG per animation (and per font), no palette. This
file says how each thing the palette did is done now.

## The sprite sheet: RGBA, and its mask

The data conversion (`AnimationConverter`) makes each animation's sheet, and each pixel of it from its
palette entry. The sheets are then packed into pages, see "Sprite pages" below. The
**alpha is how much of the pixel is there**: 0 is transparent (the PSX's `0x0000` entry), and
anything between is a soft edge. In a semi-transparent sprite **alpha 255 is solid**: only a pixel
below 255 blends, and it blends by its alpha (so an HD particle can fall off smoothly and still
add up like the PSX's blend modes do). The conversion writes a palette entry with the STP bit as
`RGBConversion::kBlendAlpha` (254) and one without it as 255, so the original's pixels blend as
they did, to within 1/255.

What else the PSX kept in the palette entry goes in an optional **mask** image next to
the sheet (`name.png` has `name_mask.png`, the same size, `SpriteMask.hpp`). Its colour channels
are per pixel:

| channel | meaning |
|---|---|
| R | **palette rank**, 1 to 255: where the pixel's palette entry was in the sprite's palette (its index scaled so the last entry is 255), 0 if unknown. The electrocution goes in this order |
| G | **glow**, 0 to 255: how much the sprite's tint doesn't change the pixel (the Sligs' and Glukkons' eyes are 255, an HD sheet can fall off towards the edge) |
| B | **keep visible**: the invisibility effect leaves it alone (palette entries 1 to 7, which the original's fade never touched) |

A zero is the default, and a sheet with no mask file has zero everywhere. So a custom or HD sheet
is plain true colour: every pixel below alpha 255 blends where the sprite is semi-transparent,
takes the tint and is faded by invisibility. The conversion writes a mask when some
pixel of the sheet has glow or keep visible. Any PNG is loaded as RGBA
(`PNGFile::LoadSprite`), including indexed ones.

What a texel adds to the framebuffer (`handle_final_color` in `ShaderPsx.cpp`, mirrored by
`Sdl3Texture::ConvertTexel`), with `a` the alpha and `c` the colour after the tint:

| draw | result |
|---|---|
| not semi-transparent, or a solid pixel | `c*a + dst*(1-a)` |
| blend mode 0 | `0.5*c*a + dst*(1 - 0.5*a)` |
| blend mode 1 | `dst + c*a` |
| blend mode 2 | `dst - c*a` |
| blend mode 3 | `dst + 0.25*c*a` |

With `a` = 1 these are the PSX's. A converted sheet stores no palette.

In OpenGL the mask is under the sheet in the same texture (twice as tall), and the sprites of
such a sheet are drawn with `PsxDrawMode::DefaultFT4Masked`. SDL3 keeps it next to the sheet's
pixels and uses it as it bakes the sheet.

## Sprite pages

Each animation has a **sheet**: `animations/<group>/<name>.png` (and `_mask.png`) with
`<name>.json` saying where its frames are in it, as only big as its frames need. The data conversion
writes them (`SpritePageWriter::SaveSheet`), and they are what mods and the Blender add-on write
too. They are the **source**: nothing packs them away or changes them.

`SpritePageWriter::Repack` makes the pages from them: it packs the frames of each group (the
animations of a BAN file: `animations/<group>/`, which a level loads together) into
`animations/pages/<group>_<n>.png` and `_mask.png`. `SpriteAtlasPacker` works out where the
frames go: individual frames, in free rectangles (MaxRects) with a 1 pixel gap, so a page has few
holes (the sheets themselves are laid out like the original game's textures and have plenty). All
the frames of an animation are on one page, frames that are the same part of the sheet are written
once, a group stays together on as few pages as it can, small groups share one.

- An animation on a page has `<name>.packed.json` next to its json, with `"sheet":
  "animations/pages/abe_0"` in its attributes and its frames' positions on that page. The loader
  uses it instead of the json when it is in the same data directory as the json it found, so a
  mod's own sheet of the animation is not affected by the base game's pages. The animations on a
  page share one `PngData` (and so one texture in the renderers):
  `ResourceManager::LoadSheetPage`. An animation without a packed json is used from its
  sheet: a mod or HD pack's animation, and a sheet that doesn't fit on a page.

- The converted sheets have no `luma`: the sweeps use the palette rank in their masks. A sheet of
  its own without R has the brightness ranks worked out as it loads.
- The **page size depends on the GPU**: a page has its mask under it in the same texture, so
  it can be half the biggest texture at most, and it is never more than 2048
  (`DataVersions::SpritePageSizeFor`, 512 at least). `data_version.json` has `sprite_page_size`,
  and when the GPU wants another size (`IRenderer::MaxTextureSize()`: another GPU, driver or
  renderer) the pages are made again from the sheets (`DataVersions::RepackPages`), which doesn't
  need the game's files. The same happens when a sheet is newer than the oldest page
  (`SpritePageWriter::PagesStale`), so a sheet the Blender add-on exported gets packed at the next
  start. `relive -convert` has no GPU, so it uses 2048.
- OpenGL uploads a page once and draws every animation on it from that texture (so they batch).
  SDL3 draws each sprite from its own small texture, made from just the part of the page it
  draws (a power of two in size, so the texture coordinates are exact) with the effects on it,
  and keeps the ones it used lately: `Sdl3Texture::GetTextureForSprite`.
- OpenGL keeps the GL textures of sheets that aren't used any more and uploads the next one of the
  same size into one (`GLTexturePool`): allocating them is most of the cost of making a texture.

## SpriteFx: the palette effects

`SpriteFx.hpp`. Every `AnimResource` has one (`Animation::Fx()`), the renderers apply it as they
draw: OpenGL looks it up in a table (`texFx`, one row per distinct effect, `PrepareFx`), SDL3 bakes
the sheet with it (`MakeVariant`) and keeps the few variants that are in use.

| member | what it takes the place of |
|---|---|
| `mMatrix` | another palette for the sprite (`Animation::LoadPal(PalResource)`), see below |
| `mSweeps` | the electrocution's `PalleteOverwriter`: a colour replaces the sprite from darkest to brightest |
| `mSubtract`, `mForceBlend` | the invisibility fading the palette entries' channels (and setting their STP bit), except the pixels the mask keeps visible |
| mask G | the glowing eyes: the original left those palette entries out when it lit the palette |

`ReloadPal()` takes it all off. `Set_Animation_Data` keeps it, like the palette stayed.

### Palettes from files (Mudokon moods, the blue rock, UXB's green, background Glukkons...)

A palette is now the change of colours from the sprite's own palette to that one: a 3x4 matrix
(`out = M * (r, g, b, 1)`), made by `PaletteFit` from the two palettes by least squares while
converting (48 bytes, `blind_mud.pal` etc). The sprites of a kind share one palette layout, so one
matrix serves them all. Which sprite's palette each is fitted to is in `PaletteFit.cpp`. The
conversion logs the average error per palette: 5 to 12 out of 255 for the Mudokon's. Palettes that
aren't a tint of the sprite's are approximate.

### Sweeps (electrocution)

`PalleteOverwriter` keeps the original's timing exactly: 8 palette entries a step from entry 1,
over the sprite's real palette depth (the animation's json has `palette_depth`: 16, 64 or 256).
Its progress is how far through the entries it is. A pixel is covered when its **rank** is at most
that. For a converted sprite the rank is the pixel's palette entry, which the conversion writes in
the mask's R channel, so the sweep goes over the sprite exactly as the original's did. A pixel with
no R (a mod's or an HD sheet, which has no palette) goes by brightness instead:
`SpriteFx::ComputeLuma` works out the brightness at 32 evenly spaced ranks of the sheet's colours
(worked out as the sheet loads, or from `luma` in the json if it has one), so the sweep goes at an
even pace whatever the sprite looks like. A modder can paint R to choose the order. Everything else is as `master`
describes: AE's overwriters all go at once (only the last colour is seen), AO's are on the update
list so the delays work and the white, blue and black bands trail each other, the restore and
damage order, which targets are restored.

### Invisibility

Each step takes 8 (one 5 bit step) from every channel, a step every 2 frames out and 6 in, 31 at
most, with the sprite added to the picture (blend mode 1) and every pixel blendable. The original
left the palette's first 7 entries alone, which the mask's B channel does: those pixels keep their
colour and aren't blended. `InstantInvisibility` goes straight to the last step.

## Fonts

A font is a 4 bit sheet drawn in different palettes (the LCD's colours, the menus' greys), so the
data conversion makes a sheet for each palette: `lcd_font.png`, `ledfont_red.png` ... A font's
`PalResource` holds that sheet, and `FontContext::UseFontSheet` switches to it. The debug font is
made in code.

## What differs from master

- Glowing eyes: the original baked the light into the palette and left the eyes out. The light is
  the sprite's tint now and the eyes are glowing pixels in the mask, which looks the same.
- The sweeps' order is by brightness, see above. Mudokon moods and the like are fitted, not exact.
- Invisibility fades all colours, not all but the 7 first entries.
- Water's entry 1 (transparent) is done in the data conversion.

## Looking at it

`relive_render_test` has two scenes for this: "True colour alpha" (alpha 0..255, and the mask flags, in every
blend mode) and "Palette effects" (electrocution as AE and AO do it, invisibility, side by side
and playing out); "Palettes" is the matrix swaps. In the game, `-automation` has `electrocute
[extra]` and `invisible out|in|now`.
