# Bot: design and implementation plan

A bot that plays AE and AO on its own, headless, for coverage and regression. It is given a run
profile (save every Mudokon, kill every Mudokon, something in between, with or without dying) and
has to work out the rest: where to go, which lever opens what, how to get past a Slig, when to
chant. It plans with GOAP (goal-oriented action planning, as F.E.A.R. does) and, unlike F.E.A.R.,
it can test a plan step for real before committing to it, because a quicksave restore is exact.

Premise: quicksave/restore is deterministic (the determinism work on this branch). Everything below
leans on that.

## Where it stands (2026-11-11)

The sections after this one are the design as first written. This one is what is built, what was measured and
what is left, and wins where they differ.

### Built and tested

| part | file | what it does |
|------|------|--------------|
| Game view | `GameView` | The only code that knows the two games' classes: Abe's state (both games' motions mapped to one small `Move` set), the path's lines and TLVs as a `NavInput`, the entities as a `World` (Mudokons with mood and brain state, Sligs, lifts, switches), every path of the level as a `PathSummary`. |
| Pilot | `Pilot` | Turns a `Prim` (wait, move to a cell by walk/run/sneak/roll, face, press-and-let-play-out, walk off an edge, running jump, ride a lift) into pad buttons a frame at a time, by watching what Abe does. |
| Nav graph | `NavGraph` | A cell of floor per grid position. Edges: walk, roll (low ceilings, walls that stop short of the floor), fall (not if deadly), hop (over mines and gaps, and to a ledge four cells off), running jump (to a floor four cells off or a ledge five off), hoist up/down, door, well, lift. `Condition` says which switch an edge waits for (slam door, electric wall, a door that starts shut). Knows AO's gaps between cameras and both games' grids. `LearnEdge` corrects an edge by where it really led. |
| Regions | `NavGraph::Regions` | Cells Abe can move between freely both ways. Planning is over regions, not cells. |
| Level graph | `LevelGraph` | Which path leads to which, and how many paths each is from the way out of the level. An exit is only taken if it leads nearer the end, or (when the ending wants Mudokons) once into a path off the way (a secret area) that is worth it: `Domain::WeighSideTrip` builds that path's graph ahead of time and counts the Mudokons Abe could get to from where he would come in, how many of them have a bird portal in reach, and whether there's a way back. `save_all` and `average` go for Mudokons that can be rescued, `kill_all` for any that can be reached, `any` never goes. |
| Planner | `Goap` | Facts, actions with needs and effects, cheapest plan by forward search with the actions that can't help the goal left out. Deterministic. Says what nothing gives when there's no plan. |
| Domain | `Domain` | Builds the planning problem of a path: cross to another region (needs the edge's switch), use a lever/ring/wheel/hand stone, rescue a Mudokon through a portal in its region (sets its rescue switch), kill one (`kill_all` only), switch logic, wheels that must turn together (needs a Mudokon per wheel but one), leave by an exit or a level loader. `Frontier` says what blocks, `Reachable` which switches are worth trying when there's no plan to the exit. |
| Tasks | `Task`, `Tasks.cpp` | `GoTo` (replans the route after every move, bans edges that go nowhere, sneaks past sleeping Sligs, waits in cover for an awake one to look away, leads Mudokons by ways they can follow), `Use`, `Recruit` (hello, sorry or a slap by mood, follow me), `Rescue`, `WorkTogether`, `Kill`, `Exit`. |
| Checkpoints | `Checkpoint` | A quicksave the bot owns, taken every ~90 frames when Abe stands still and nothing threatens (no Slig on the screen, no laughing Mudokon in tow). On a death: back to the newest, try again after a (seeded) delay; six deaths from one and it goes back to the one before. Old ones are thinned out, not dropped. The game's own quicksave is left alone. |
| Bot | `Bot` | Plan, act, replan. Profiles, the committed input log (rewound tries aren't in it), the report, movie skipping, the reflex that slaps a wired Mudokon tagging along. |
| Profiles | `Profile` | `save_all`, `kill_all`, `average`, `any`, with `seed=<n>` and `human`. |

Engine changes it needed: `PsxDisplay::mSkipDraw` (automation `draw 0`: frames run but nothing is drawn or
waited for), the movie skip check reads the automation buttons and comes before the audio preroll wait,
`Mudokon::IsFollowingAbe()`, `Emotion()`, `BrainState()`, `BrainSubState()`.

Tests: `relive_lib_tests --gtest_filter='Bot*'` (rng, profiles, planner, nav graph with every edge kind, regions
and domain, level graph, pilot against a stand-in Abe). With game data, ctest `bot_goto_ae` and `bot_goto_ao`
(routes with a hoist, a well, hops over UXBs, AO's camera gaps, a door into the background) and `bot_replay`:
the bot plays the start of Mines path 1 (hoists, a roll, a hand stone, a running jump to a ledge), is
electrocuted on the way, goes back and carries on, and its buttons replayed with no bot end in the same world
hash.

### Measured

- Debug + ASan, offscreen: ~83 frames/s drawn, ~4000 frames/s with `draw 0` (less with the bot thinking). A
  restore is ~150 ms (the camera reload: resource loads and uncached palette PNG decodes), so ~600 frames'
  worth. Play forward freely, rewind sparingly.
- AE Abe: stands on x = 12 + 25k. A tap is one cell (10 frames). Walk 9 frames a cell, stops on the next cell
  once the button is up. Run ~4 frames a cell, slides 2-3 cells to a stop. Standing hop: 3 cells, ~23 frames;
  over a gap it catches a ledge 4 cells off as it drops. Running jump: asked for in the last half cell before
  the take-off cell (the run looks for the button as it reaches each cell), 3 cells of run-up, 4 cells in the
  air, catches a ledge 5 cells off. Roll: 4 frames a cell, stops on the next cell after the button is let go;
  fits under a ceiling up to 45 above the floor and past a wall that ends 20 above it. Turn: 10 frames. A fall
  of 180 pixels (at full scale) or more kills unless onto a soft landing.
- AO Abe: the same grid counted from each camera's left edge (cells at 240 + 25k of 1024, 240 to 615 are
  used; half scale 245 + 13k to 622). Walking past the last cell puts him on the first of the next camera.
  Vertically a camera shows 120 to 360 of its 480.
- A hoist is looked for 10 above Abe's feet to go up and 16 below to go down, and he turns by himself. A lever
  is pulled from inside its TLV with the lever one cell ahead. A door, a well, a hand stone and a wheel take up
  from inside their TLV; a hand stone's pictures go on with the confirm button. A lift is worked from the
  middle of its platform: up or down held until the floor wanted, then he lets go by himself.
- Mudokons (AE). A TLV's Created and Destroyed flags are both set while its object is about: gone for good is
  Destroyed without Created. They only hear Abe on his screen. Sorry needs the Mudokon in the cell in front of
  Abe; so does a slap (which costs it 1/13 of its health: thirteen kill it). Sad and angry are put right by
  sorry, wired by a slap, but laughing gas on the screen wires them again at once. Wired ones follow without
  being asked and laugh, and any speech wakes a sleeping Slig that is on the screen. A follower goes back to
  its work after 540 frames standing, or left on another screen. They follow by x alone: they walk off a ledge
  after Abe only when he is more than two cells away, so he has to move on from the foot of a drop. Running to
  keep up they don't stop at wheels; walking they stop at the first free wheel and stay. Told to work, the ones
  at a wheel turn it for about 20 frames after its switch comes on and then go back to chiselling, so the last
  wheel is Abe's and he has to be on it with up held before he has finished saying "work".
- Sligs (AE). One that is awake sees Abe on its own floor, the way it faces, when it is on the screen being
  shown and Abe isn't wholly inside an invisible zone. Objects of the cameras next door don't exist yet, so a
  Slig is known from its TLV until its camera loads.
- A multi switch controller wants all its inputs on within its delay of the first, else it turns them all off
  again. A wheel sets its switch every frame it has been turning for its activation time.

### Mechanics needed to win, and where each is

| mechanic | Mines (AE) | Rupture Farms (AO) | state |
|----------|-----------|--------------------|-------|
| walk, run, sneak, turn, stop on a cell | yes | yes | done |
| hop over a mine / UXB / small gap | yes | yes | done |
| hoist up and down, fall safely | yes | yes | done |
| doors (same path, into the background, shut until a switch) | yes | yes | done |
| wells | yes | no | done for express wells in a path; local wells' landing spot is learnt, not known |
| leave the path by a door or a level loader | yes | yes | done; by well or path transition: written, not yet seen working |
| levers, wheels, hand stones -> switches -> barriers | yes | yes | done, seen working in Mines path 1 |
| roll under a low ceiling, roll off a ledge | yes | yes | done (rolling off: not yet seen working) |
| running jump, hop and jump to a ledge | yes | yes | done in AE; AO's distances not measured |
| lifts | yes | yes | done in AE (one lift, three stops); lift movers, AO: not tried |
| rescue: hello, follow me, lead, chant | yes | yes | done in AE (AO's speech buttons differ) |
| sad / angry / wired Mudokons | Mines path 1 | no | done |
| wheels turned together by Mudokons | Mines path 1 (2 and 6 wheels) | no | done |
| leading Mudokons down ledges and past a sleeping Slig | Mines path 1 | yes | done |
| sleeping Sligs: sneak, no talking on their screen | yes | yes | done |
| patrolling Sligs: wait in an invisible zone, go behind their back | Mines path 1 | shadows in AO | first version: it got through, sometimes after deaths |
| killing Mudokons (for `kill_all`) | - | - | by slapping, where the way out doesn't need the Mudokon. Not by Sligs, mines or drops. |
| skip movies | yes | yes | done |
| pull rings | Mines path 1 (a wheel is on the same cell) | yes | **not working**: up turns the wheel instead |
| UXBs' timing, disarming | Mines path 1, 5 | yes | **missing**: hopped over, which went wrong once in a full run |
| trap doors, falling items, meat saws, drills | paths 2-4 | yes | **missing** (timed hazards: the retry delay is meant to find the gap) |
| possession, shooting, security orbs | paths 2, 5, 10 | yes | **missing** |
| Slogs lured into electric walls | Mines path 3 | no | done (`SlogTrapTask`) |
| flying Sligs, teleporters, mine car, rock sacks, motion detectors, the timed escape | paths 2-6, 10 | no | **missing** |
| grenades (boom machine), security claws | no | yes | **missing** |

### How far the levels get

- **Mines path 1, `any`**: beaten. From the start to path 2 in about 11,900 frames in the run measured (seed
  1): the two Mudokons whose rescue opens doors saved, two wired ones calmed and put on wheels, the lift, the
  lever, past the sleeping Slig and the patrolling one, the wells, five Mudokons fetched from their ledge, led
  down three drops and past another sleeping Slig, six wheels turned at once. One death on the way (hopping
  the UXBs, rewound) and one Mudokon dead that didn't have to be (by those UXBs).
- **Mines path 2, `any`**: beaten (2026-11-11), about 1,700 frames. The way on is a roll off the east end of
  the 1240 floor into a shaft: the 780 px fall lands in a local well on the floor at the bottom, which throws
  Abe to a ledge he catches (measured with `build-bot/sh1.txt`). The nav graph has both now: a fall that comes
  down on a well goes where the well throws, and a well that aims at a point in the air lands on the next floor
  along (`NavGraph::WellLanding`). The flying Slig over the long floor killed Abe about 14 times before a run got
  past it: nothing models it, the rewinds find a gap.
- **Mines path 3, `any`**: beaten, about 3,800 frames, no deaths. Four lifts down, five Slogs woken on the way
  that follow Abe along the floor above and drop through an open trap door by the exit. `SlogTrapTask` (a reflex,
  when Slogs are awake and the floor Abe is on has electric walls with a lever for them): stand on the far side
  of the walls, a cell further every 60 frames, until a Slog comes down; run back through the walls, throw the
  lever from the near side without turning (turning cost the 8 frames that got Abe bitten), wait for them to fry,
  walk out of the lever's stretch and back to turn the walls off, again while Slogs are left. Measured: Slogs
  run exactly as fast as Abe (6.2 px a frame) and pace about 70 px either side of him on their floor.
- **Mines path 4, `any`**: through the first two sets of mines, to the drill (2026-11-11, `build-bot/p4.txt`,
  about 3,900 frames to there, no deaths). Rocks: `GetRocksTask` stands under the sack and jumps (Up), a rock
  holding the sack's nine falls out, rolls and comes to rest, and is picked up from the cell next to it,
  crouched, with Action. `BlowMineTask` throws at a mine until it is gone. A mine's cell is a nav cell whose
  edges wait for a switch of its own (`World::HazardSwitch`, on when the mine's TLV is destroyed), so the
  planner sees "blow up the mine, then walk through" the way it sees a lever and a door, and the hop over a
  single mine is still there. Where to throw from is not worked out from how a rock flies: `Domain::ThrowSpots`
  lists places worth a try, the task tries them, and the bot keeps what set a mine off (tried first from then
  on, anywhere) and what missed or killed (per mine, never again). Measured: under a ledge 100 px up, an up
  throw from four cells off hits (five and six fall short); along a floor, a throw at the floor from five
  cells, a crouched throw from six, a forward throw from ten; under a low ceiling only the crouched throw, from
  six cells, one mine after another. The blast reaches 113 px either way and 50 up and down
  (`GroundExplosion.cpp`), and it does not set the next mine off. A rock takes the first mine in its way, so a
  mine behind another waits for that one: that also keeps the planner from trying every order of a row.
  The sack gives nothing while Abe has rocks.
  - **Drills** (`FeatureKind::eSweep`, `EntityKind::eDrill`, `World::mSweeps`, `GoToTask::SweepsClear`): the
    cells a drill reaches are ordinary floor with a mark. `GameView` reads each live drill every frame through
    `Drill::GetStroke/Offset/StrokeSpeed/PauseLeft/WillStart/KillRect` into a `SweepAt`, which says where it
    kills t frames from now (parked, out, back; a pause of unknown length is taken as its shortest). `GoToTask`
    stops in the last cell before the marked ones and goes when Abe, moved along the edges ahead at measured
    paces, never has the drill within 9 px of him until he is out the other side. If its lever can be got to
    without passing a drill, that is pulled first (`GoToTask::StopFor`); a stopped drill's cells are plain floor
    once its stroke is over. A death right after setting off shifts the timing for the next try
    (`Bot::SweepLate`: 6 frames later, 6 sooner, 12 later...). No throwing from a drill's cells while it runs.
    Measured on path 4's drill 203: it kills in a box 26 wide and 21 high, 3 px a frame, 182 frames out and
    back with no pause, so the only way through is behind it as it goes; a roll is 6.25 px a frame, a walk
    2.77, a hoist up 100 px about 54 frames. Result: through it first time with no death, lever pulled.
  - **A miss is taken back** (`Bot::RewindMiss`): the game is put back to before the throw, so the rock isn't
    lost and what was learnt (that throw at that mine from that cell is no good) stays. A throw that sets off
    another mine than the one aimed at is kept and not held against the throw. A death within 150 frames of a
    throw is put down to it. Not from a cell with a live mine within 90 px on the same floor (125 for a throw
    upwards: the rock comes back down on it), not crouched at the top of a hoist (crouching there climbs
    down), and from where Abe can get back from before anywhere a drop leads.
  - **Stalls**: six deaths in one cell give up what was being done there and ban the edge, twenty stop the
    run; after 600 frames with nothing logged the log says what the bot is at.
  Where it stops now (`build-bot/p4.txt`, 2026-11-11, about 8,900 frames, no deaths, 89 misses taken back):
  in the room after the drill (cams 36 and 37), on the 2420 ledge at 3787. Gone by then: all eight mines of
  the room's first two floors and, as far as the log shows, the three on the 2480 ledge (one hit, two set off
  on the way). Then `no_plan`, with no goal even tried: nothing is both reachable and wanted
  (`Domain::Reachable`). **Not established why.** Two candidates, check in this order: (a) the four mines on
  the 2540 floor before the door at 3425,2520 are never made a goal (the only way to them is a drop off the
  2480 ledge into the first one's cell; the stuck report's "Blocked" list does not name that edge, so the
  ledge may not count as in reach, or a fall into a mine's cell is not offered); (b) the path's exit needs
  door 2562,460, which opens on switch 190: the rescue of a blind Mudokon, and blind Mudokons are not
  modelled (they walk on until told to wait and fall off ledges). A scenario started in the room
  (`start mines 4 36 3787 2420` will not have the mines gone, so use a save) would settle it. After that: two
  more blind Mudokons whose rescue opens doors, drills with no lever in reach (timing only, some jumped
  through), lift movers, a timer trigger. One sack in the room (3937,2384) hangs over the gap between two
  ledges and is only got by jumping the gap through it; the bot leaves it alone and goes back past the drill
  for rocks. `build-bot/research/` has what four read-only passes over the game code found on each of these,
  on grenades and boom machines, the mine car and path 6's timed escape.
- **Mines path 1, `save_all`, later the same day** (`build-bot/m1s.txt`, 15,000 frames): 5 rescued, no deaths.
  The Slig by the orb is shot by a Slig Abe has taken over and sent round by teleporter to the background
  (`ClearSligTask::RaidStep`): a shot from behind takes whatever is nearest within 25 px of the snapped x over
  the whole screen, so it leads the target and holds fire while a Mudokon is nearer. #6005, #8005 and #33006
  all go out through the walkway portal: a Mudokon is only paired with a portal it can walk to, and UXBs by
  it are turned off first. A possessed Slig says "look out" (speak 7) so working Mudokons duck and a level
  shot goes over them (`ClearSligTask::ShootLevel`; unit tested, no scene has used it yet). Still to do there:
  the background Mudokons (Abe through the teleporters), #18009/#18010, #21004/5, #32002/3, the wheel crew.
  `kill_all` and the full `save_all` run have not been rerun since. What follows is the state before that.
- **Mines, `save_all` and `kill_all`**: neither finishes path 1 in 16,000 frames (2026-11-11). `save_all`: 2 rescued, none
  lost, 8 rewinds. `kill_all`: 2 rescued (the way needs them), 3 killed by slapping, 7 rewinds. Both stop dying in
  a loop: a rescue or kill that kills Abe three times is given up, and in `save_all` a Mudokon's death rewinds like
  Abe's own. What stops them: the two Mudokons on the patrolled floor (#8005 at 2412,1400 and #33006 at
  3162,1400). `ClearSligTask` takes a watching Slig over and bursts it, but only from another floor of the same
  screen within 600 px and with no security orb there: at #8005 the floor below is a long way round, at #33006
  there is an orb. So it has never run in the real game, only its `Watcher` rule is unit tested. The intended
  way is to call the Mudokon from inside an invisible zone. The secret area (path 7) is weighed and skipped: 12
  Mudokons there, none in reach of what the bot models (trap doors, a cage emptied by chanting from outside).
- **Rupture Farms, `any`**: not rerun since the work above (it was stuck at the start with `no_plan`: 100 of
  415 cells in reach; a running jump and barriers on switches 13 and 14 were next).

So the first three paths of the Mines are beaten with `any`, the other profiles not past the first, and the level isn't. Each stuck report names what stops it.

### Next, in order

1. Mines path 4: why there is no goal left in the mine room after the drill (see above), then blind Mudokons
   (hello, follow me, "wait" before each drop, the way cleared first), the sack that hangs over a gap, and the
   same timed-hazard reader for meat saws, falling items and motion detectors (`SweepAt` is meant to take them).
2. Sligs in the way of a Mudokon (#8005 and #33006 on path 1's 1400 floor). There are usually several ways and
   the planner should hold them as alternatives, cheapest first: (a) call the Mudokon from inside an invisible
   zone and lead it off behind the Slig's back; (b) take over a Slig that is easy to get at (another floor of its
   screen, no orb) and walk it over to shoot the one that isn't: `ClearSligTask` does the chant, the walk, the
   shooting (`Target`/`AsSlig`, unit tested) and the burst, but has not got into a Slig in the game yet; (c) get
   a security orb blown up (a grenade, a flying Slig's) and then chant. Measured: chanting from a shadow on the
   Slig's own floor gets Abe shot after ~50 frames (it turns to the chant and fires, seen or not), so the spot
   must be on another floor. From #8005 the floor below is reached by walking east off the end of the 1400
   floor, past the second Slig: that walk east died three times where the same walk west got through, look
   there first.
   Measured since (2026-11-11), from `SligSpawner.cpp`, `Slig.cpp` and a run: the Slig at #8005 comes from a
   spawner on switch 66 (the lever that opens the way up there). It is made when Abe comes to its screen with
   the switch on, and a killed one stays gone: its death clears the switch and only setting it again makes
   another. So killing it once is enough. Spawners are in the world as Sligs that are there while their switch
   is on, so the bot sneaks onto that screen and stops in the shadow instead of running in. Chanting from a
   shadow on the Slig's own floor was tried and taken out: `IsInInvisibleZone` is false for everyone while Abe
   chants, the Slig panics at the chant, runs off the screen and shoots on its way back, about 2 frames before
   the takeover (90 frames of chant, then 30 with it on screen) is done.

   What works is the raid (`ClearSligTask::FindRaid` / `RaidStep`, 2026-11-11): when the watching Slig can't be
   taken over where it is, look for another Slig with no orb on its screen and a teleporter on its floor, go to
   a cell of its screen on another floor (in Mines path 1 the ledge at 1062..1112,1240 under the well, for the
   Slig at 812,1140), chant, and steer the Slig: stand in the teleporter, pull its lever (Action, lever a cell
   in front), come out on the next floor, and so on through every teleporter that leads to a floor not seen yet
   (switches 116, 112, 114 here). A teleporter moves whoever is controlled (`Teleporter.cpp`). On each floor it
   shoots what is on it, and from a half scale floor it shoots out of the screen (Down + Throw) at Sligs in
   front: that bullet takes the nearest thing within a cell of the Slig's x on the whole screen
   (`Bullet.cpp`), so with Mudokons to save it holds fire while one is within 60 px. With `kill_all` it shoots
   the Mudokons too, the three behind included. Then it bursts. Stopping on a teleporter is done in single
   steps (a one frame tap from standing): a walking Slig stops 16 to 21 px after the key is let go.
   Also measured: the spawned Slig is deleted when Abe goes to a screen with none of its persist or bound
   markers (the ledge is one), and the spawner then clears switch 66, so the walk to the ledge alone gets rid
   of it. Runs from the start, seed 1: `kill_all` 17 killed into path 4 (19 rewinds, 13 of them at four
   Mudokons on path 3 that were given up), `save_all` 8 rescued and none lost into path 4 (10 rewinds).
   Open: in `save_all` #33006 is still given up (the Slig by the orb was not shot in that run), no unit test
   for the raid yet, and an action that fails six times over is now left alone for good (`mFailedTotal`).
3. UXBs: read their pattern and cross on the safe beat, or disarm them.
4. Pull rings where a wheel shares the cell.
5. The flying Slig of path 2 (hide from it or time it), trap doors, mine car, teleporters, the timed escape of
   path 6: as the paths meet them.
6. Rupture Farms: rerun, measure AO's jumps, AO's speech buttons.
7. Endings: `average` is still "every other Mudokon"; check it and `kill_all` against the games' own
   thresholds once a level can be finished.
8. `-bot=<profile>` on the command line. Until then `Source/Tools/bot_runs/` has scripts.

### Running it

`Source/Tools/bot_runs/README.md`. The automation commands are documented in `Automation.hpp`: `bot`,
`bot_wait`, `bot_stop`, `bot_report`, `bot_inputs`, `bot_log`, `bot_trace` (with the Mudokons near Abe and their
brain states), `bot_nav_dump` (the graph, the regions and every planning action of the current path: the first
thing to read when a plan is missing), `bot_goto`, `bot_do`, `replay`, `expect_bot`, `expect_bot_restores`,
`draw`. The bot's log says each goal and plan, each Mudokon talked to with its mood, each time the Sligs on
the screen change what they do, and each death and where it went back to.

## Principles

- **Simulate, don't guess.** F.E.A.R. plans symbolically because it can't rewind the world. We can:
  take an in-memory snapshot, run an action for real, look at what happened, restore. The symbolic
  plan decides *what* to do and in what order; the simulation decides *whether a step works*. Timing
  problems (run past a Slig, hop a mine) are solved by trying, not by modelling Slig AI.
- **White box.** The bot runs in-process and reads object state and path data directly (Slig state,
  lever target ids, door destinations, portal mud counts). It isn't pretending to see the screen.
- **Deterministic.** Same build, same data, same profile, same seed: same run. The bot has its own
  RNG, used only to pick among equally good strategies. Every other decision is a pure function of
  what it observed. A run's inputs are logged and can be replayed through the game without the bot,
  checked with `StateHash`.
- **Fails loudly and cheaply.** When it can't solve something it says so with a short text report
  and a quicksave that reproduces the situation in seconds, so a person (or a model) can iterate
  without reading thousands of lines.
- Engine conventions apply: no new globals, file-scope `static`, no `dynamic_cast` (check `Type()`,
  then `static_cast`), use the existing virtuals, no nested loops that pump frames (the bot does one
  step per `Update`).

## Architecture

Five layers, bottom up. Each is testable on its own.

```
 Profile / Goals        save_all,no_death,seed=7  ->  prioritised goals
 Planner (GOAP A*)      goal  ->  [GoTo(n14), TalkTo(Mud#41), LeadTo(Portal#9), Chant]
 Executors              one state machine per action, pad input per frame, retry via snapshots
 Navigation             nav graph per path (floor lines + TLVs), level graph (doors, wells, lifts)
 WorldModel             observe BaseMap -> entities, facts; the cheap text view
                        + Checkpoint: snapshot/restore stack over QuikSave
```

The `Bot` object sits in `Engine::Game_Loop` next to `Automation` and is updated once per main loop
iteration with the same `betweenFrames` flag. It writes `Input().mAutomationInput` (what `hold`
already uses) and reads the map. It never runs frames itself.

### WorldModel

`WorldModel::Observe(BaseMap&)` walks the object list and the current path's TLVs and produces:

- **Entities**: one record per object the bot cares about, keyed by a stable id (the object's
  `Guid` for dynamic objects, path + TLV offset for static ones): kind (Abe, Mudokon, Slig, Slog,
  Lever, Door, Well, LiftPoint, BirdPortal, ElectricWall, Mine, UXB, Bomb, Grenade...), position,
  the few state fields that matter (Mudokon: following/working/sick/dead; Slig: patrol/alert/
  possessed/dead; Lever: on/off + target switch id; Door: open/closed + destination; Portal: muds
  needed/present, open). Read from the real objects via `Type()` + `static_cast`, with a
  `GetGameType()` switch where AE and AO classes differ.
- **Facts**: the GOAP world state (see Planner). Derived from entities plus the game's own counters
  (`gRescuedMudokons`, `gKilledMudokons`, switch states), never from the bot's bookkeeping, so the
  bot can't fool itself.
- **Abe**: position, motion, facing, what he's holding, invisible, muds following, on which nav node.

Observation is cheap (a few hundred objects at most) and is redone every frame the bot is awake.

#### The cheap view

`WorldModel::Text()` is the one format the bot logs, reports and dumps. One line per entity that is
alive and relevant, integers only, no animation data. Target: under 40 lines for a camera, ~1 KB.

```
bot f=1832 mines/1/6 profile=save_all,no_death seed=7 deaths=0 restores=14 budget=72%
goal SaveMuds(mines/1) plan=[GoTo n14, TalkTo Mud#41, LeadTo Portal#9, Chant] step=2 step_frames=88
abe x=1012 y=385 motion=Walk node=n12 face=R hp=1 inv=0 following=1 hold=none
Mudokon#41 x=1100 y=385 follow
Slig#12 x=1500 y=385 patrol face=L alert=0
BirdPortal#9 x=1800 y=380 closed muds=0/3
Lever#33 x=700 y=385 off -> sw5
ElectricWall#7 x=1300 sw5 on
nav n12 floor 900..1200@385: R n13 walk | n13: L n12, R n14 hop(Mine#5)
blocked n13: Slig#12 line of sight; tried run-across x3 died@x1540 f+22
```

`dump_objects` stays for rendering questions; this view is for behaviour questions.

### Checkpoint

A stack of in-memory snapshots on top of `QuikSave` (AE) / `AO::QuikSave`. Needs two small API
additions: save into a `Quicksave` the caller owns, and load from one, instead of only
`gActiveQuicksaveData`. Restore goes through the same between-frames path `restore_check` uses
(`Engine::mRestoringBetweenFrames`): one screen-change step, seed and Guid counter put back.

The bot's own memory is *not* in the snapshot. That's the point: the world rewinds, the bot
remembers what it tried. A restore is one `Push/Pop` call, nothing else in the bot needs to know.

Budget: a restore costs a camera load. Headless and unthrottled that's milliseconds, so a run can
afford thousands. Each run has a restore budget; running out is a stuck condition.

### Navigation

**Nav graph** per path, built once from `BasePath` data when the path is entered and cached:

- Nodes: floor-line segments (`eFloor_0`, `eBackgroundFloor_4`, mine car floors) split where a TLV
  changes what you can do (a Hoist, an Edge, a Door, a Well, a LiftPoint, a camera boundary, an
  electric wall, a mine).
- Edges, each tagged with the Abe move that takes it and a cost in frames: walk/run along a line,
  hop a gap, jump down, hoist up (Hoist TLV), drop from an edge, climb a ledge (Edge TLV),
  door (Door TLV, with its destination), well (WellLocal/WellExpress exit vectors), lift (LiftPoint
  pairs), roll/crouch under, camera change across a screen edge, bird portal exit, mine car.
  Scale changes (background floor) are an edge attribute.
- Hazards are attributes on nodes/edges (mine, UXB, electric wall + switch id, Slig sight line),
  not removed from the graph: the planner decides whether an attribute is a precondition
  (`SwitchOff(sw5)`) or something to try.

**Level graph**: rooms (level/path/camera) connected by door, well, lift, portal and screen-edge
transitions read from TLVs, plus `Path_ChangeLevel`-style exits. Used for goals that are
elsewhere ("the Mudokon is three cameras right") and for exploration order when the bot doesn't
yet know where something is.

Both are approximate by design. The executor checks reality: if a hop edge drops Abe into a pit,
that's an action failure, the edge gets marked and the planner routes around it.

A `-bot_nav_dump` option writes the nav graph per camera as text (and optionally as an overlay
PNG via `screenshot`) so it can be eyeballed.

### Planner (GOAP)

Follows F.E.A.R.: a world state of symbols, actions with preconditions and effects, A* from the
goal backwards (regressive), the cheapest plan wins, replan when the world changes.

- **WorldState**: a small sorted vector of `(FactKey, value)` where `FactKey = {kind, id}`.
  Kinds: `AbeAt(node)`, `AbeInRoom(room)`, `MudSaved(id)`, `MudDead(id)`, `MudFollowing(id)`,
  `SwitchOn(id)`, `DoorOpen(id)`, `SligDead(id)`, `SligPossessed(id)`, `PortalOpen(id)`,
  `Holding(item)`, `Invisible`, `AtEnd`. Unspecified = don't care. No `unordered_map`, no pointer
  keys: stable ids only, so planning is deterministic.
- **Actions**: a table of action *templates* instantiated per entity each plan (`PullLever(#33)`,
  `GoTo(n14)`, `TalkTo(Mud#41)`, `LeadTo(Portal#9)`, `Chant`, `Possess(Slig#12)`, `ShootAs(Slig)`,
  `Throw(item, target)`, `PickUp(item)`, `UseWell(#3)`, `EnterDoor(#2)`, `RideLift(#5)`,
  `WaitFor(Slig#12, away)`, `SneakPast(Slig#12)`, `RunPast(x)`, `Die`). Each has preconditions,
  effects, a cost (frames, from the nav graph where it's movement), and an executor class.
  AO and AE share the table; abilities that one game lacks (AE's invisibility, AO's roll) are
  gated by `GameType`.
- **Goals** come from the profile, each with a priority: `StayAlive` (always), `SaveMud(id)` or
  `KillMud(id)` per Mudokon, `ReachEnd`, `Explore(room)` as a fallback when no other goal has a
  plan (gets the bot to find new entities). The highest-priority goal with a plan runs.
- **Replan** when: an action finishes (its effects are checked against reality, a mismatch is a
  failure), a fact the plan depends on changes (the Mudokon died), an action fails, or every N
  frames as a backstop.
- **Choice**: when two plans tie on cost, or an action has several parameterisations (which side to
  approach from, how long to wait), the bot's RNG picks. Logged, so a run is explained by its seed.

Planner code lives in `.cpp` files behind small headers (see the build-time notes in CLAUDE.md).

### Executors

One class per action, `IActionExecutor` with `Start`, `Step(WorldModel&) -> Running|Done|Failed`,
`Abort`. `Step` runs once per game frame between frames and sets the pad bits for the next frame.
It watches Abe's motion ids to confirm a transition took (walk started, hoist began, door
entered), not just elapsed time. Every executor has a frame budget and a no-progress check.

Retry is generic, in the `Bot`, not per executor: before a step that is marked risky (anything
near a hazard or an enemy, any edge the nav graph isn't sure about) the bot pushes a checkpoint.
If the step fails or Abe dies, pop, pick the next variant (RNG-ordered: wait 10 frames first, go
from the other side, sneak instead of walk...), try again up to the action's retry count. Variants
exhausted = action failed = replan with that edge/action marked blocked for this room. With
`no_death`, dying inside a try is fine (it's rewound); only the committed run must be death-free.

### Learning what the bot can't read

Some effects aren't in the TLV data (which door a switch id ends up opening in another path).
`PullLever` with an unknown effect is executed under a checkpoint: pull, observe which facts
changed, record `Lever#33 -> DoorOpen(#2)` in the bot's room knowledge, restore or keep depending
on whether that's wanted. Same for unknown Door destinations: go through, note the room, come back
through the level graph. Knowledge is per run (deterministic), optionally written out as a file
so later runs start informed; a stale file can only mislead the planner into a failed action, never
break determinism.

### Determinism and replay

- `BotRng`: xorshift seeded from `-bot_seed` (default 1). The game's seed is not touched.
- Every committed frame's pad bits go to `bot_inputs.txt` (`<frame> <bits>`); every decision to
  `bot_log.txt` (frame, goal, plan, step, variant, why). Restores are logged with the frame they
  rewound to, so a reader can see tries that were discarded.
- A new automation command `replay <inputs file>` feeds the bits frame by frame with no bot. The
  regression check is: `hash_log` of the bot run == `hash_log` of the replay. That is also how a
  gameplay change that silently alters behaviour shows up: the stored inputs from a known-good build
  no longer end in `AtEnd`.
- No wall-clock anywhere in the bot. Budgets are in frames and restore counts.

### Stuck detection and the report

Four conditions, each named in the report:

| condition       | detected by                                                                      |
|-----------------|----------------------------------------------------------------------------------|
| `no_plan`       | no goal of the profile has a plan and `Explore` has nothing left to explore       |
| `action_failed` | an executor ran out of variants/retries (each try listed with its cause)         |
| `no_progress`   | the fact set hasn't changed for N committed frames across replans (a loop)       |
| `budget`        | frame or restore budget for the run, the room or the action exceeded             |

On any of them the bot stops and writes to the output dir:

- `bot_report.txt`: the condition, the cheap view, the last 50 log lines, and for `no_plan` the
  nearest miss (the goal's cheapest partial plan and which precondition had no provider, e.g.
  "SaveMud(#41): PortalOpen(#9) needs 3 muds following, 1 reachable").
- `bot_stuck.json`: the quicksave (JSON) from the start of the failing action.
- `bot_inputs.txt`, `bot_log.txt` as above.

Reproducing takes one automation script: `start <level> <path> <cam>`, `quickload_file
bot_stuck.json`, `bot <profile> <seed>`, `bot_wait`. Seconds, not a whole run. "Challenge" =
level/path/camera + goal; the report names it so the same one can be tracked across iterations.

The run also ends on success (`done`: every profile goal met) and writes the same files.

## Integration

- Command line: `-bot=<profile>` (e.g. `save_all,no_death`, `kill_all`, `save=5,kill_rest`,
  `any`), `-bot_seed=N`, `-bot_out=<dir>`, `-bot_budget=<frames>`, `-bot_nav_dump`. Starts a new
  game like `start` does; `-bot_start=<level> <path> <cam>` to begin elsewhere.
- Automation commands: `bot <profile> [seed]`, `bot_wait [frames]` (until done or stuck),
  `bot_dump <file>` (the cheap view), `expect_bot done|stuck`, `replay <inputs>`,
  `quickload_file <json>`. So bot runs compose with `hash_log`, `expect_hash`, `screenshot`.
- Speed: headless bot runs must be unthrottled (verify what `Game_Loop` does with no display and
  `-max_fps`; add an uncapped mode if there isn't one).
- Tests: `relive_lib_tests` gets unit tests for the planner (toy world states), the nav graph
  (built from a hand-written line set) and `Checkpoint` (push, run, pop, hash equal). Real-data
  scenario tests go next to the existing `automation_*` ctest entries and skip when the data is
  missing. Regression runs (whole levels) are a script in `Source/Tools/bot_runs/`, not ctest.

## Code layout

```
Source/relive_lib/Bot/
  Bot.hpp/.cpp             the object in Game_Loop: profile, budgets, goal loop, retry, logs, report
  BotOptions.hpp/.cpp      profile and command line parsing
  BotRng.hpp
  WorldModel.hpp/.cpp      Observe, entities, facts, Text()
  Checkpoint.hpp/.cpp      snapshot stack over QuikSave / AO::QuikSave
  NavGraph.hpp/.cpp        per-path graph from lines + TLVs, A*, text dump
  LevelGraph.hpp/.cpp      rooms and transitions
  Goap/WorldState.hpp/.cpp facts
  Goap/Action.hpp          templates: preconditions, effects, cost, executor factory
  Goap/Planner.hpp/.cpp    regressive A*
  Goap/Goals.hpp/.cpp      profile -> goals
  Actions/*.cpp            one executor per action
  BotLog.hpp/.cpp          inputs, decisions, report
Source/Tools/bot_runs/     regression scripts (per game, level, profile) + expected results
```

Game-specific reads (`Slig`, `Mudokon`, `Abe` fields) are confined to `WorldModel.cpp` and the
executors, the way `Automation.cpp` already includes both games' headers. Everything above
`WorldModel` is game-neutral.

Hot reloading the bot as a shared library was considered and dropped: the stuck report plus
`quickload_file` gets a repro in seconds, and an in-tree static lib keeps one build, one ABI, and
ASan/UBSan on the bot code.

## Implementation plan

Each phase ends with something runnable and a check. AE first throughout; AO is phase 8 because its
restore isn't complete yet. Nothing in phases 1-3 depends on the mud mechanics, so those can be
built while the determinism sweep finishes.

### Phase 0: prerequisites (small engine changes)

1. `QuikSave::SaveTo(Quicksave&)` / `LoadFrom(const Quicksave&)` (AE) and the AO equivalents;
   `LoadActive` becomes `LoadFrom(gActiveQuicksaveData)`.
2. `quickload_file <json>` automation command (round-trip already exists, this is just load).
3. Unthrottled headless run; verify a restore between frames is fast when offscreen.
4. `replay <inputs file>` automation command.

Check: an automation script that does `hash_log`, `quicksave`, `hold right 60`, `quickload`, and
the same via `replay`, matches line for line.

### Phase 1: skeleton end to end

`Bot`, `BotOptions`, `BotRng`, `WorldModel` (Abe + every entity kind listed above, `Text()`),
`Checkpoint`, `BotLog`, the `-bot` options and the `bot*` automation commands, the stuck report.
One hard-coded behaviour: walk right; at a wall, report `action_failed`.

Check: `relive -bot=any -bot_start=mines 1 6` writes a report with the cheap view and a
`bot_stuck.json`; `quickload_file` + `bot_dump` reproduces that view; the inputs replay to the same
`hash_log`. Unit tests for `Checkpoint` and `WorldModel::Text` formatting.

### Phase 2: navigation

`NavGraph` from lines + TLVs with every edge type above, A* with frame costs, text/PNG dump.
`GoTo` executor with walk, run, hop, jump down, hoist, edge climb/drop, crouch/roll, door, well,
lift, screen edge; motion-id confirmation and per-edge budgets. Generic retry with checkpoints for
edges marked unsure. `LevelGraph`.

Check: a "visit every node" test on mines path 1 and a mudomo_vault path; a `GoTo` from camera 1 to
the well that leads to the wheels in mines path 1 (the first milestone from the branch notes).
Nav dumps eyeballed for a handful of cameras. Death during a hop variant is rewound and the next
variant tried.

### Phase 3: planner

`WorldState`, action templates, regressive A*, goal selection, replan triggers, `Explore`.
Actions at this point: `GoTo`, `EnterDoor`, `UseWell`, `RideLift`, `PullLever`, `Wait`. Profile
`any` with goals `StayAlive` + `ReachEnd`.

Check: planner unit tests on toy states (plan found, nearest miss reported, deterministic ordering
under ties). `any` on mines path 1 reaches its exit; the run replays; `no_plan` report on a path
with the exit lever deleted (via a modded path or a `switch` command) names `SwitchOn(id)` as the
missing fact.

### Phase 4: Mudokons

Entities and facts for Mudokons and bird portals; actions `TalkTo` (hello/follow/wait per game's
GameSpeak), `LeadTo`, `Chant` (portal), `WaitFor` muds to catch up, handling of working/sick/
angry muds (AE: sorrow/laughing gas, slapping). Profile `save_all` and `save=N`.

Check: `save_all` on mines path 1 ends with `gRescuedMudokons` equal to the path's Mudokon count;
`no_death` variant replays death-free.

### Phase 5: enemies and hazards

Sligs (sight lines in the nav graph, `SneakPast`, `WaitFor`, `RunPast` variants, `Possess` +
`ShootAs` + `Explode`), Slogs, mines/UXB/bombs (`Throw` to detonate, timed hops), electric walls,
paramites/scrabs (AE necrum/vault), flying sligs, greeters, motion detectors, laughing gas. Each is
an action or an edge attribute, and most of the hard timing is done by the retry search.

Check: `any` and `save_all` across all AE mines paths; stuck reports for whatever remains, each
one a tracked challenge.

### Phase 6: kills and the middle

`KillMud(id)` goal, actions `ShootAs(Slig)`, `LeadToHazard`, `Explode` near muds, slapping to
death where the game allows. Profiles `kill_all`, `save=N,kill_rest`. AE's ending counters checked
against the game's own thresholds.

Check: `kill_all` on mines path 1; the run replays; `save=N,kill_rest` ends with the counters the
profile asked for.

### Phase 7: whole-game runs and regression

Level graph across levels, the `Explore` goal across rooms, knowledge file, `Source/Tools/bot_runs/`
with a script per (game, profile) that runs the bot and compares inputs + final hash with the
stored expectation; a mode that only replays stored inputs (fast regression) and a mode that
re-solves (finds new stuck points). Budget tuning.

Check: `any` from AE's start to the end in one unattended run; a deliberate gameplay change (e.g.
a hop distance) is caught by the replay mode.

### Phase 8: AO

Once AO restore is exact: AO ability table (roll, no invisibility, 4 GameSpeak buttons, chant
differences), AO entity reads in `WorldModel`, AO-only objects (rock sacks, bells, meat...). Same
checks on rupture_farms.

## Risks and open points

- A restore reloads the camera; anything the game does on camera load that isn't in the save
  (music, FMV triggers) must stay harmless when it happens hundreds of times. The determinism
  sweeps are the guard.
- Nav-graph edge cases (background/foreground scale, mine cars, sligs' own nav) will be the bulk of
  the work; the retry search hides a lot but not everything. Expect to add edge types in phase 5.
- Regressive A* over per-entity facts can blow up on big paths; cap the horizon per room and use
  the level graph for the rest. Measure before optimising.
- The bot reading private fields means refactors touch `WorldModel.cpp`. Prefer existing getters
  and virtuals; add a getter rather than a friend.
- Some goals need the bot to lose something on purpose (die to reset a room, abandon a mud) in
  profiles that allow it. Left until a run needs it.
