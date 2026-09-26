#pragma once

#include "../../relive_lib/Animation.hpp"
#include "../../relive_lib/Font.hpp"
#include "../../relive_lib/Layer.hpp"
#include <memory>
#include <string>
#include <vector>

class OrderingTable;
class ResourceManagerWrapper;
class BaseMap;
class TestResources;

// Draws the HUD and scene labels with the debug font that's built into the engine
class TextDrawer final
{
public:
    explicit TextDrawer(ResourceManagerWrapper& resMan);

    // Every frame, before anything is drawn: the font's polygons are shared by the whole frame
    void StartFrame()
    {
        mPolyOffset = 0;
    }

    struct Style final
    {
        u8 r = 127;
        u8 g = 127;
        u8 b = 127;
        Layer layer = Layer::eLayer_Text_42;
        bool semiTrans = false;
        relive::TBlendModes blendMode = relive::TBlendModes::eBlend_0;
        FP scale = FP_FromInteger(1);
        // Each character's colour is off by up to this much, at random, as the LCD screens flicker
        s16 colourRandomRange = 0;
    };

    // x and y in screen pixels
    void Draw(OrderingTable& ot, s32 x, s32 y, const char* text, const Style& style);
    void Draw(OrderingTable& ot, s32 x, s32 y, const char* text)
    {
        Draw(ot, x, y, text, Style());
    }

    s32 Width(const char* text);

    static constexpr s32 kLineHeight = 9;

private:
    FontContext mFontContext;
    AliveFont mFont;
    s32 mPolyOffset = 0;
};

// What scenes build their frames from
struct SceneContext final
{
    ResourceManagerWrapper& mResMan;
    BaseMap& mMap;
    const TestResources& mRes;
    TextDrawer& mText;

    // Draws a camera image as the background, with the real ScreenManager
    void DrawCamera(OrderingTable& ot, const CamResource& cam);

    // For scenes with no camera. Like the game, every scene draws over the whole screen:
    // the renderers don't clear it between frames.
    void DrawBackground(OrderingTable& ot, u8 r, u8 g, u8 b);

    Poly_G4 mBackground;
};

// One screen of the render test. Each one exercises one part of the renderer, and says on
// screen what a correct renderer draws, so that a person can check it.
//
// A new scene object is made each time the scene is shown. Enter() makes whatever it draws;
// the runner destroys any game objects it made once it's done with the scene.
class Scene
{
public:
    virtual ~Scene() = default;

    // Shown at the top of the screen
    virtual const char* Title() const = 0;

    // Shown at the bottom: what a correct renderer draws. Lines are split with \n.
    virtual const char* Expected() const = 0;

    virtual void Enter(SceneContext& /*ctx*/)
    {
    }

    // Once per frame, after the game objects have been updated. frame counts from 0 when the
    // scene starts, so scenes play out the same way every time.
    virtual void Update(SceneContext& /*ctx*/, u32 /*frame*/)
    {
    }

    // Adds the scene's own primitives. Game objects the scene made draw themselves.
    virtual void Render(SceneContext& ctx, OrderingTable& ot) = 0;

    // Looks the same every frame, so it's checked for drawing exactly the same twice in a row
    virtual bool IsStatic() const
    {
        return false;
    }

    // Measures performance with a lot of drawing, so it's timed for longer
    virtual bool IsStress() const
    {
        return false;
    }

    // The frame that's captured to compare against other renderers and baselines
    virtual u32 CaptureFrame() const
    {
        return 40;
    }
};

// An Animation drawn straight from a scene, with no game object behind it. Positions are
// the centre of the sprite, in screen pixels.
class SceneSprite final
{
public:
    SceneSprite(const AnimResource& res, s32 x, s32 y);
    ~SceneSprite();

    SceneSprite(const SceneSprite&) = delete;
    SceneSprite& operator=(const SceneSprite&) = delete;

    void Render(OrderingTable& ot);

    Animation mAnim;
    s32 mX = 0;
    s32 mY = 0;
};

using SceneFactory = std::unique_ptr<Scene> (*)();

// Every scene, in the order they're shown. The first is the reference frame, which --auto
// draws again after each of the others to check that nothing leaked into it.
std::vector<SceneFactory> AllScenes();

// For file names and --scene: lower case, words joined by underscores
std::string SceneSlug(const Scene& scene);
