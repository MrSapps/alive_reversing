# Bot: design and implementation plan

A bot that plays AE and AO on its own, headless, for coverage and regression. It is given a run
profile (save every Mudokon, kill every Mudokon, something in between, with or without dying) and
has to work out the rest: where to go, which lever opens what, how to get past a Slig, when to
chant. It plans with GOAP (goal-oriented action planning, as F.E.A.R. does) and, unlike F.E.A.R.,
it can test a plan step for real before committing to it, because a quicksave restore is exact.

Premise: quicksave/restore is deterministic (the determinism work on this branch). Everything below
leans on that.

## Where it stands (2026-11-15)

The sections after this one are the design as first written. This one is what is built, what was measured and
what is left, and wins where they differ.

### Built and tested

| part | file | what it does |
|------|------|--------------|
| Game view | `GameView` | The only code that knows the two games' classes: Abe's state (both games' motions mapped to one small `Move` set), the path's lines and TLVs as a `NavInput`, the entities as a `World` (Mudokons with mood and brain state, Sligs, lifts, switches), every path of the level as a `PathSummary`. |
| Pilot | `Pilot` | Turns a `Prim` (wait, move to a cell by walk/run/sneak/roll, face, press-and-let-play-out, walk off an edge, running jump, run into a well without stopping, ride a lift) into pad buttons a frame at a time, by watching what Abe does. |
| Nav graph | `NavGraph` | A cell of floor per grid position. Edges: walk, roll (low ceilings, walls that stop short of the floor), fall (not if deadly), hop (over mines and gaps, and to a ledge four cells off), running jump (to a floor four cells off or a ledge five off), hoist up/down, door, well, lift. `Condition` says which switch an edge waits for (slam door, electric wall, a door that starts shut). Knows AO's gaps between cameras and both games' grids. `LearnEdge` corrects an edge by where it really led. |
| Regions | `NavGraph::Regions` | Cells Abe can move between freely both ways. Planning is over regions, not cells. |
| Level graph | `LevelGraph` | Which path leads to which, and how many paths each is from the way out of the level. An exit is only taken if it leads nearer the end, or (when the ending wants Mudokons) once into a path off the way (a secret area) that is worth it: `Domain::WeighSideTrip` builds that path's graph ahead of time and counts the Mudokons Abe could get to from where he would come in, how many of them have a bird portal in reach, and whether there's a way back. `save_all` and `average` go for Mudokons that can be rescued, `kill_all` for any that can be reached, `any` never goes. |
| Planner | `Goap` | Facts, actions with needs and effects, forward search with the actions that can't help the goal left out. The cheapest plan if 400 states find it (A* on an estimate that is never over: each fact at the price of its cheapest giver on top of the dearest thing that needs). Else any plan, going for the state that looks nearest (the same estimate with needs summed), which finds a hundred steps in a few hundred states; states the goal can't be had from are dropped. What is used up (`Action::mSpend`, rocks) is planned as if any were enough, then the plan is gone through with the count and the way to more put in where it runs out, from as far back as it takes. `Planner::Short` is the estimate alone, to rank goals. Deterministic. Says what nothing gives when there's no plan. `Bot::Replan` keeps following a plan while what is left of it still holds, and plans only the two or three nearest looking goals. |
| Domain | `Domain` | Builds the planning problem of a path: cross to another region (needs the edge's switch), use a lever/ring/wheel/hand stone, rescue a Mudokon through a portal in its region (sets its rescue switch), kill one (`kill_all` only), switch logic, wheels that must turn together (needs a Mudokon per wheel but one), leave by an exit or a level loader. `Frontier` says what blocks, `Reachable` which switches are worth trying when there's no plan to the exit. |
| Tasks | `Task`, `Tasks.cpp` | `GoTo` (replans the route after every move, bans edges that go nowhere, sneaks past sleeping Sligs, waits in cover for an awake one to look away, leads Mudokons by ways they can follow), `Use`, `Recruit` (hello, sorry or a slap by mood, follow me), `Rescue`, `WorkTogether`, `Kill`, `Exit`; for Scrabs `ScrabRampage`, `ScrabKill`, `ScrabLure` (off the end of their floor), `ScrabPen` (shut in behind a gate) and `ScrabWary` (a floor one walks crossed in timed stints). |
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
| flying Sligs, teleporters, mine car, rock sacks, motion detectors, the timed escape | paths 2-6, 10 | no | done for `any` (see the Mines below); path 2's flying Slig has no invisible zone to hide in and is still got past by luck |
| grenades (boom machine), security claws | no | yes | grenades at mines: done (path 20); the claw counts as an orb |

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
  **Path 4 is done with `any`** (`build-bot/p4.txt`, 2026-11-11, later): from its start into path 5 in about
  15,100 frames, no deaths, 22 missed throws taken back, four minutes of wall time headless. From the
  Mines' start (`build-bot/m1.txt`) path 5 is reached at f=31,513 with no deaths at all (paths 2, 3 and 4 at
  9,786, 11,471 and 16,456). What it took after the drill: trap doors, blind Mudokons led a stretch at a time
  (`RescueTask::LeadBlind`) and sent to a wheel, drills overhead rolled under, lift movers and shafts, pull
  rings, and the planner below. Three Mudokons die on the way (two to the drill lever 160 starts, one to the
  Slig trap): fine for `any`, not for `save_all`.
  - **Planning a path like this one**: 584 actions over 127 regions and a plan of a hundred steps. Searching
    for the cheapest plan never ended (each decision ran some twenty goals to a 25,000 state limit: minutes).
    Now: the cheapest if 400 states find it, else a search that goes for what looks nearest; a plan is kept
    while it holds; rocks are counted (a sack gives nine, and nothing to a hand that has some) by going through
    the plan with the count and putting a trip to a sack in where it runs out; a dear step (a throw past a
    running drill) is swapped for a cheaper way to the same when there is one (its lever first).
  - **Throws**: a spot with a live mine within 90 px of where Abe would stand (125 from below) needs that mine
    gone, in the plan as in the task. Throws from a ledge above are only offered for a mine with no other spot.
    A drill whose travel lies between thrower and mine has to be stopped first if a lever stops it. Still
    found by trying: 23 of 55 throws on path 4 missed, mostly crouched ones from 7 to 9 cells, forward ones
    from 10 to 12 and the ones from above. **Next: measure each kind of throw's arc once, in grid cells, and
    keep it as a table**, so a spot is one whose arc has the mine on it and nothing before it, and no throw
    is tried that can't hit.
  - **A run to watch has no quick loads in it**: `bot_inputs` writes the buttons of the frames that counted,
    and `replay` plays them back in a window (`build-bot/m1_replay.txt`). Going back to a checkpoint after a
    death is a failure to be got rid of, not part of a run; taking back a missed throw should not happen
    once throws are known beforehand.
  - **The sack over the gap** (3937,2384, between the 2420 ledges): `Domain::SackJump`, a running jump off
    the ledge end beside it and down to where the rocks fall. The planner goes to it now, not back to the
    first one.
  - **Six wheels at once (path 1)**: "all o' ya" then one "follow me" brings the whole screen
    (`RecruitTask::SetAll`); then Abe walks past the wheels without stopping. A following Mudokon stops at
    the first free wheel it walks past (`Mudokon::StopAtWheel`): not if it runs, which it does when Abe runs,
    and one that catches Abe up standing among the wheels stops behind him. One pass now, 4,500 frames less.
  - **Throw measurements**: `build-bot/arc.txt` dumps the rock frame by frame for each kind of throw
    (`build-bot/arc/*.json`), not yet made into the table. The flight is `Rock::InTheAir`: gravity 1.01 a
    frame (AO 1.0), start speeds (3,-14) back, (10,-10) up, (15,-8) forward, (10,3) down, (10,-4) crouched,
    a floor halves both speeds and turns it up (under 5 a frame down it falls through instead), a ceiling or
    wall halves and turns the one; off Abe's screen it goes through floors.
  - **The mine car** (`EntityKind::eMineCar`, `StepKind::eRide`, `RideCarTask`): a planner action from the
    car's floor to either end of each car floor that has a floor to get out on; which it can get to is found
    by driving. A direction held carries the car on round corners, up walls and along ceilings
    (`build-bot/car.txt` drove path 5 by script: right, right again where it stops under the ceiling, up at
    3312,600, up again at 2787,335, then it runs to 3937,200). The task holds the way that moves it nearer and
    where that does nothing tries the others in turn, not straight back the way it came. Started beside the
    car (`build-bot/p5c.txt`) the bot is through path 5 and into path 10 in 1,100 frames. It runs six Mudokons
    over on the way: fine for `any`, wrong for `save_all`.
  - **Motion detectors** (`World::mBeams`, `World::BeamClear`, `Bot::BeamHolds`, `MotionDetector::BeamNow`):
    Abe is not seen standing or crouching still, or hanging; anything else under the beam sets the alarm off
    (`MotionDetector::IsInLaser`), which wakes the Sligs. Before each frame of the pilot the bot works out,
    frame by frame, whether the beam comes over Abe during what he is about to do (a cell of walking or
    sneaking is 25 on and 13 frames, a hop 75 and 24, a turn or a crouch 9 in place), and holds the buttons
    off if it does: walking he stops at the next cell. Held up for 150 frames he hops on or crouches and
    rolls up to three cells behind the beam when that fits (`Bot::BeamRoll`), but not with a Slig asleep on
    the screen: of Abe's moves only sneaking makes no noise (walk, roll, hop, run and landing all broadcast
    `kEventNoise`). After 900 frames held the move fails.
  - **Path 5 on foot** (`build-bot/f4.txt`): to the car with no deaths and the three Sligs asleep. What was
    missing was the beam's wait of 15 frames at each end (`MotionDetector::BeamNow` gives the frames left of
    it, `BeamAt::mPause`/`mEndPause`), and that it is the beam's middle that goes from end to end: with those a
    sneaked cell fits between the passes of the fast detector too.
- **The Mines are beaten with `any`** (2026-11-12, `build-bot/m1.txt`, seed 1): from the start of path 1 into
  Necrum path 2 in 41,178 frames, 1 death (path 10's first sleeper, below), 22 missed throws rewound, 6
  rescued, 13 killed. Paths at f=10,258 (2), 11,913 (3), 15,557 (4), 30,563 (5), 32,843 (10), 35,994 (6). The
  walkthrough in `build-bot/research/ae_100_guide.txt` (its Tunnel 6, Tunnel 7 and Boiler revisited are paths
  5, 10 and 6) gives the same ways through as what follows; read it before working a screen out from the code.
- **Mines path 10, `any`** (`build-bot/p10.txt`, about 2,700 frames, no deaths in the run measured alone):
  - *What wakes a Slig* (`Slig::Brain_34_Sleeping`): a noise by something of its size within its
    `noise_wake_up_distance` cells (6 here: 75 px at half size), anyone speaking or chanting while it is on the
    screen, the alarm. Of Abe's moves only sneaking and standing are silent: every landing is a noise, a well's
    too. `Slig::Beat` gives the bot each Slig's bounds, that distance and its `shoot_on_sight_delay`
    (`SligAt::mBeatMin/Max`, `mWake`, `mSightDelay`), and Abe sneaks only within earshot of a sleeper, not on
    its whole screen.
  - *Out of the well beside three sleepers*: the landing wakes the nearest. Chanting there is too slow
    (measured twice: shot 95 frames in, a takeover needs 120, and the chant wakes the rest). It walks its beat
    away from him first, so the lever beyond it is pulled behind its back: `SligAt::FramesUntilSees` (the walk
    to the end of its beat and back to the lever's screen, and its wait) against the frames Abe needs
    (`ClearSligTask`, "behind its back").
  - *Down between two sleepers*: a Slig that waits before it shoots (`World::kOutrunDelay`, 45 frames) is run
    from, not hidden from or taken over: `GoToTask` runs while one is awake on his floor and screen. They come
    after him and are stopped by the enemy stoppers under the teleporter's ledge, on a screen with no security
    orb. `Bot::MakeReflex`: Sligs awake on his screen, none on his level, no orb: take one over from where he
    stands (`ClearSligTask::Ambush`), shoot the rest, burst. Left there they shoot out of the screen at him
    the moment he is in front of them at full size (`Slig::ToZShoot`).
  - *A Slig that wakes beside him and shoots at once* (the first sleeper, woken by the foreground Slig's
    talk): `Bot::MakeFlee`, at a run to another level if one is five moves off or less (the ledge by the hoist
    there, from which the reflex above takes it over). The one death of the full run was here, before the flee
    was let run in mid step; not seen to work since (the Slig did not wake in the runs after).
  - Sligs that go off the screen are made again asleep, so getting away by the well only starts it over.
  - *Teleporters for Abe*: `FeatureKind::eTeleporter`, `EdgeKind::eTeleport` from the cell in one to the one it
    leads to; `GoToTask` faces its lever and presses Action. A local well on switch 1 (always on) throws to its
    "on" place (`GameView`).
  - *The car*: `Ride` needs the slam doors across the car's floors open, which is what makes the plan pull the
    three levers. It runs the four blind Mudokons and the foreground Sligs over: wrong for `save_all` (the
    walkthrough has the taken-over Slig pull the last lever and shoot the foreground Sligs, and the Mudokons
    led to the portal).
- **Mines path 6, `any`** (`build-bot/p6.txt`, about 5,200 frames, no deaths):
  - The way out is a crawl: a cell under a low ceiling can be an exit (`NavGraph`). Wheels whose switch stays
    on are turned one after the other; only ones that count while they turn need a Mudokon each (`Domain`).
    Crossings on foot that follow one another are gone in one run (`Bot::Replan`), and a wheel is held for its
    activation time and 16 frames, not 45.
  - *The flying Slig* (`World::mFliers`, `Bot::FlierThreat`, `HideTask`): it goes along its track end to end
    (measured: up to 6.2 px a frame, 30 to 60 frames at an end, about 900 frames there and back) and goes for
    Abe when it is on his screen, faces him and he isn't in an invisible zone (`FlyingSlig::CanChase`). The bot
    goes from one invisible zone to the next: the stretch he is out in the open for (to the next zone on his
    way; or to the thing to do and back to a zone; or, with no zone after it, on to the next thing for as long
    as that is within 300 px of the track) has a cost in frames from the nav edges, and he sets out only when
    the Slig can't be at the end of it before then, and isn't on it. Otherwise he goes to a zone (40 moves or
    less) and waits. After any reflex the plan is made again from where he stands.
  - *The timed escape*: the third wheel starts 450 frames; run without stopping it takes about 590 to the way
    out, so the last 800 px are under the explosions. Those come from one `ExplosionSet` that sweeps each
    screen left to right at 5 px a frame, with air explosions only behind the sweep, so a run that doesn't stop
    stays ahead of it. It got out in every run since the stops were taken out.
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
- **Rupture Farms, `any`**: beaten from its first screen to the Stockyards with 0 deaths (2026-11-15; see
  "AO, Rupture Farms" below).

- **Throws are known before they are made** (2026-11-12, later; `ThrowArc`). A rock's flight is worked out
  frame by frame from where it leaves the hand and how fast (measured per kind of throw: standing from 11
  ahead and 55 up, crouched from 22 ahead and 15 up), the pull of 1.01 a frame, and the halving bounces off
  floors, ceilings and walls of the nav lines. What sets a mine off was measured too, and is not the mine's own
  look-out but the rock's: its box, five lower, touching the mine's, which is while the rock is 1 to 11 over
  the mine's floor and within 7 to 9 of its middle. The rock comes back off the mine at half speed. Higher than
  that it flies over, which is why a crouched throw passes the mines three cells off and takes the one four
  off. `Domain::ThrowSpots` gives only cells from which the flight ends on the mine, with the mines it would
  meet first and the slam doors it goes through as conditions, the ones that are sure whatever frame the
  mine's light is at first. Also measured: any blast on the screen knocks a standing Abe over, and he falls on
  a mine a cell behind him, so no standing throw with a mine within two cells; off his own screen a rock goes
  through floors; a shut slam door is a wall to a rock. A miss now logs the flight the table gave beside the
  one the game made (`Bot::ThrowMissed`), which is how the door was found. Result: the Mines from path 1 to
  Necrum in 39,884 frames, 28 throws, none missed (it was 41,178 with 22 rewound).
- **Deaths left in that run**: 7 at path 2's flying Slig and 1 at path 10's first sleeper. Path 2: Abe ran to
  the low part at 3287,1240, stopped, crouched and was shot. Now a flying Slig within a screen or so with no
  shadow to go to means a run, and a run that leads under something low goes straight into the roll
  (`Prim::RunRoll`: the roll button is looked for as the run reaches each cell). Path 2 alone, six different
  arrival times: no deaths. Path 10's is not touched.
- **Full runs since, seed 1, and what they show: the run is not yet the same from one day's code to the
  next.** (a) `build-bot/m1.txt`: into Necrum at f=42,012 and on to path 6 at f=47,514, the Mines' only
  deaths two at a drill on path 4 that a new hop-down edge jumped through (fixed: no hop-down across a
  drill's column; path 4 alone is 0 deaths, 0 misses again). Path 10's sleeper did not wake. (b)
  `build-bot/m1b.txt` after that fix: 13 deaths and not out of the Mines. Three at path 10's first sleeper
  (938,599: woken while Abe sneaks past at 1013, he is shot at 1112 on his way to the well; `MakeFlee` did
  not get him off the floor), and after those rewinds ten at path 6's timed escape, which the run before
  did without a death: from the last wheel he was at 3562,1500 when the 450 frames were up. So the escape
  has no margin and depends on what came before.
- **Path 10's first sleeper, fixed** (`build-bot/p10.txt` at five arrival times, no deaths; in four the Slig
  woke). `Bot::MakeFlee` never fired: it left out Sligs that wait before they shoot (the sneak he was at was
  then never thought over again), looked for the nearest other floor by any edge and gave up if that was not a
  quick one, and counted five cells, where the well is twenty half-scale cells on. Now: any woken Slig beside
  him, by walks, wells and hoists up only, as far as 12 cells (24 at half scale), at a run.
- **The Mines with no death and nothing taken back** (`build-bot/m1b.txt`, seed 1, after the fixes above):
  from the start of path 1 into Necrum at f=42,003, on through Necrum's path 2 and into path 3 by f=44,710
  when the run's time was up: 0 deaths, 0 restores, 28 throws, none missed. Paths entered at 10,231 (2),
  11,855 (3), 17,989 (4), 31,941 (5), 34,212 (10), 36,819 (6). (Path 3 took 6,100 frames here against
  3,800 measured alone: not looked into.)
- **Necrum, `any`** (2026-11-12, `build-bot/n2.txt`, `n3.txt`, `n1.txt`, `n6.txt`; runs that start there take
  `been=mines`, since a way back to a level already played is no way on):
  - **Path 2**, 1,278 frames, no deaths. New: a hop off a ledge end comes down four cells out on a floor up to
    130 lower (measured arc); a hop catches a ledge two to four cells off and up to 85 higher; a fall or a
    let-go from a hoist that passes through a well's box goes where the well throws (`NavGraph::WellInFall`);
    a bird portal of Abe's own is an edge (`EdgeKind::ePortal`): there once its switch is on (a hand stone's),
    chanted open from at least three cells off (nearer, its doves fly off and it is gone until the screen is
    come back to: `BirdPortal::IsScaredAway`), then hopped into from two cells off.
  - **Path 3** (the hub), 3,092 frames, no deaths, planned in one piece. New: wells go one way with their
    switch off and another with it on, and an express well puts Abe in the well it names and shoots him out
    of that one, on through whatever wells he is thrown into (`NavGraph::WellOut`, edges with `mWhen`); a
    tasks door opens once all its hubs' switches are on (a switch of its own, `World::DoorSwitch`); a door
    that leads into the path but to no door of it is not an exit.
  - **Path 1**, 1,091 frames, no deaths. New: slap locks (`EntityKind::eSlapLock`: GameSpeak 5 from the cell
    beside it, then the ghost has to fly to its tomb before the switch comes on).
  - **Path 6** (four rooms of levers, Slurgs and Fleeches), 3,929 frames alone, no deaths. What it took,
    all measured (`build-bot/fl.txt` samples Abe's health and each Fleech's motion every two frames):
    - A foot coming down on a Slurg bursts it and the noise wakes every Fleech on the screen. While a Fleech
      sleeps the next cell is waited for or hopped over if a Slurg is in it (`World::SlurgBy`; standing still by
      one is safe), and a hop or drop down onto a floor they crawl along is timed for the gap between two of
      them, from each one's real box and the way it is going (`World::SlurgWhereHeLands`).
    - A footstep within 150 of a sleeping Fleech angers it and two wake it: sneaking there, with the sneak
      button held through a turn too (he came out of a turn at a walk and that one step woke one).
    - A Fleech bites only along its own level within three cells, or at Abe hanging from a ledge with it
      under him within two (`Fleech.cpp`). Each bite is 0.15 of his health, about one every nine frames, and
      a door gives it all back. It climbs the hoist he used in about 25 frames, so no ledge is safe for long.
    - So the order of a room's switches is the plan (`Domain::PulledFirst`): what wakes nothing first (the
      rings), then the levers as a sweep towards the door, the one the door is quickest from last (in frames:
      a drop off a ledge beats three cells of floor), and from it straight down into the door.
    - With a Fleech awake, or the switch that wakes one of the screen's on: at a run, no minding Slurgs, a
      lever from the side he is on.
    - A Fleech under the ledge he is leaving, with another hoist still to climb: he waits until it has
      started up after him, so that it has to come down again (it was catching him as he hung).
    - A hop from a ledge to a ledge at the same height four cells off lands on it (the nav graph had it
      falling to the floor).
  - **Path 4**, no deaths (1,616 frames alone). A slap lock that gives the power to go invisible is fetched
    before anything else when the path has Fleeches (`Key::kInvis`: no exit without it), he chants when a
    Fleech is awake on his screen or lies on his floor within a screen's width, and runs the rest. Also: a hop
    catches a ledge two or three cells off at its own height.
  - **Path 5**, no deaths, into the Mudanchee vault 2,458 frames after the start of path 4.
  - **All of it in one go** (`build-bot/nall.txt`, from the arrival in path 2): paths 2, 3, 1, 6 and 4 and into
    path 5 at f=13,965, in path 5 at 14,672 when the run's time was up, 0 deaths, 0 restores.
- **Mudanchee vault, path 6, `any`** (`build-bot/mv.txt`, with `been=mines,been=necrum`): beaten. From its
  start to the exit into path 7 in 8,918 frames, 0 deaths, 0 restores. What its four screens took:
  - Screen 2 (the walkthrough: lock, chant, run right, lever, door before it wears off): a slap lock with a
    ghost and the power is one action that gives both; a gate that holds a Scrab is opened only with the power
    in hand (`Key::kInvisNow`).
  - Screen 3: a Fleech hangs over the next lock behind a gate, and a Scrab is shut in beside it. A lock with a
    Fleech over it, and a gate with one just behind it, wait for that Fleech to be dead
    (`Key::Mud(fleech) = 2`), and `ScrabKillTask` makes it so: from the lever on the ledge, open the Scrab's
    gate, chant until Abe is in the Scrab, walk it to the Fleech, stamp (Down: the tenth frame of the stamp
    kills what the Scrab's box is on), walk it home, chant to let go, shut the gate.
  - Screen 4, three Scrabs on one floor between gates that foot switches open (`EntityKind::eFootSwitch`):
    `ScrabRampageTask`. From the ledge over the one Scrab that can get out to the rest (every shut gate
    between it and another has its opening foot switch on this Scrab's side), chant into it; for each other
    Scrab and for the Fleech at the end: howl while standing (GameSpeak 1; the power is there when the howl
    is over), then at it at a run with Throw held, which is the spin that kills what its box is on for 30
    frames; a Fleech that sleeps is come at the same way, since it wakes as its gate opens and is up the
    nearest hoist in forty frames, out of reach. Then the Scrab is walked through the gate one foot switch
    opens and over the one that shuts it, and let go (`World::mScrabParked`). Measured: the howl asked for
    while it walks is lost.
  - Abe keeps off a floor a loose Scrab can get at him on (`Domain::ScrabReaches`: a Scrab on that floor with
    no shut gate or wall between, not while he is invisible or has the power): for the planner's crossings
    and for the routes.
  - With no way out known and no switch left to try, the next goal is a region he has not stood in, through
    a door or a well before along a floor (`Go somewhere new`): walking onto an unknown floor first cost six
    deaths to Fleeches.
- **Mudanchee vault, path 7** (the hub), 791 frames alone, no deaths: up, the bird portal, the well. Its way
  on (a tasks door to the ender) wants a switch from each of the six vaults, which nothing in it sets, so
  any path of the level not yet been in is taken (`Bot::mNewPathFor`).
- **Mudanchee vault 3 (path 3)**, a chase from end to end: beaten with no deaths from the hub
  (`build-bot/mv7.txt`: in at f=791, back in the hub by 3,100) and in the run from path 6
  (`build-bot/mv.txt`), which used to lose him nine times at the last leg. What it took:
  - A slapped lock is done the moment it is slapped (`World::mLocksSlapped`, its switch counted as on): the
    slap starts a clock here and standing to wait for the ghost got him eaten. He waits for the ghost at
    the door it opens, and for the ring if the lock gives the power.
  - Wells that go by one switch go the same way down a chain, and a throw that ends in the air over a well
    comes down into it (`NavGraph::Thrown`): the chain from 462,200 comes out at 1062,460 as worked out.
  - A Scrab that waits before it strikes (`attack_delay`, 35 here) is run from, not kept away from. With
    one on his floor Abe runs, rolls the last cells to a stop (no walking them), and goes up a hoist by a
    running jump through it, which catches the ledge without stopping (`Abe::Motion_31_RunJumpMid` looks for
    a hoist; an edge made dear and made cheap only when chased, `Bot::EdgeCostFn`). Measured: standing at
    the hoist to climb it took 54 frames he did not have.
  - The last leg, measured frame by frame (the trace has the Scrabs near Abe now: place, motion, facing).
    The well chain throws him out at the Scrab's feet and on to 1062,460. The Scrab cannot see him above it
    (`Scrab::CanSeeAbe`: not when he is 45 px or more higher), so it starts its run as he comes down, its
    `attack_delay` long over. A running Scrab kills what its box touches (`Scrab::KillTarget` from
    `Motion_2_Run`), a hanging or climbing Abe too. Standing up from the landing first, Abe was 3 frames
    short of the ledge at 1337; the one run that got away did so because the Scrab happened to shriek (a 1 in
    16 chance a frame while it stands in its chase).
  - What a player does and the bot did not: hold the way on while landing. `Abe::Motion_16_LandSoft` goes
    into the walk or run on its third frame if left or right is held, else plays out (12 frames) and stands.
    `Prim::mAfter` are the buttons held from when his feet touch after a fall or a well, and the prim is
    done as soon as he walks or runs (`Pilot::LandsInto`). `GoToTask` asks for it only when a Scrab is loose
    on the floor he comes down on: the way the path goes on, or away from the Scrab where the path ends
    with the landing. Ten frames sooner at the ledge, in both runs.
  Tried and taken out: going down the last well only when the Scrab has just stopped at the near end of its
  beat (the only places to wait are within the Fleeches' reach). Tried by hand and no use: chanting as he
  lands. A chasing Scrab does stop and howl for some 48 frames at a chant, but every screen here has a
  security orb, which zaps him 20 frames after the first chant and puts him on his back for 26.
- **The door come in by**: a vault has two doors to the hub, the one Abe comes in by and its own, which wants
  the locks. `Leave the path` took the first. Now, of two ways out to the same place, the one he has just
  come in by is left out (`Bot::mArrivedX/Y`), until every other fallback has failed (`Bot::mBackFor`).
- **Mudanchee vault 4 (path 8)**, beaten with no deaths (`build-bot/mv8.txt`, 1,236 frames; `mv7.txt` goes
  hub, vault 3, hub, vault 4, hub). The walkthrough's way, worked out by hand first (`build-bot/x8.txt`, a
  script of `bot_goto` and `bot_do` steps) and then left to the planner:
  - Down to the lock at 212,720 without waking the two Fleeches by it (one hangs, `"hanging": true`). A fall is
    heard where it lands (`Abe::Motion_16_LandSoft`, not after letting go of a ledge): a fall or a hop down
    that lands within 150 px of a sleeping Fleech costs 3,000 more (`Bot::EdgeCostFn`), so he goes down the
    hoist at 262 and lets go, then hops the Slurg (the gate that times a landing for Slurgs waited for ever at
    a hoist: it wanted him facing the way of the edge, and down a hoist he turns by himself).
  - The slap wakes them. Measured: a woken Fleech next to him takes 60 to 90 frames to strike. He is on his
    feet 32 frames after the slap and the well at 89 is four cells past them. No standing to wait for the ring
    with a Fleech awake on his screen: it finds him as he goes (74 frames after the slap, wherever he is on
    that screen). `World::mInvisComing` says it is on its way, so the plan made as he gets up can count on it.
    Up a hoist with a Fleech awake on the screen costs 2,000 more: it is up after him in 25 frames.
  - The Scrab's floor is walled off from the start (a pillar from 100 to 468): the only way onto it is down
    through two trap doors from the top, the second a pen with two Fleeches awake in it. A crossing onto a
    floor a loose Scrab can get at him on is now there for the planner with `Key::kInvisNow` wanted, where it
    was left out (`Domain::Build`; `Domain::ScrabReaches(..., charged)` says whether the power not yet used
    counts as cover: yes for a route, no for this).
  - When to chant: the power lasted 387 frames here and the way from the pen to the door is about that long.
    With a step ahead that wants him unseen, in a path with no security orbs, the power is not used on
    Fleeches met before (`Bot::InvisSaved`) and is chanted for when that step is this one or the next
    (`Bot::InvisChantNow`): in the pen, before the lever that drops him to the Scrab. Chanted for on arriving
    at the top (95 frames sooner) he was eaten three cells from the door.
  - A Fleech that hangs over a floor that is over a lock does not guard the lock (it did: no plan).
  - The way to the door: over a block by a ledge, then two low stretches three cells apart. He rolled the
    first, stood up, walked, and crouched again; now a roll goes on through up to three cells of open floor to
    the next low stretch. He rolls under the two Fleeches as the invisibility ends and is through the door
    with them 100 px behind.
- **Mudanchee vault 2 (path 5)**, beaten with no deaths from the hub (`build-bot/mvp5.txt`: in at f=805, out
  by its own door at f=13,728, 0 deaths, 0 restores; its third part alone is `build-bot/s53.txt`). Three screens
  of Scrabs and nothing else, each worked out by hand first (`build-bot/x5.txt`, `x52.txt`, `x53.txt`). What it
  took, in the order it comes:
  - **Where a Scrab is.** A live Scrab is tied to its TLV (`ScrabAt::mId`, by `mBaseGameObjectTlvInfo`), and
    `Domain::ScrabReaches` asks about the floor it is on now, not the one the map put it on. One that has lost a
    fight counts as dead from the frame its health is gone. A Scrab shut in behind a gate is no reason to run on
    landing (`Domain::GateBetween` in `GoToTask`).
  - **Lured off a ledge** (`ScrabLureTask`, the action `Lure the Scrabs off the ledge at x,y`). The walkthrough:
    "climb up, immediately press down and hang again until the Scrab runs over to you, pull back up as he falls
    over the edge". Measured: a hanging Abe is not seen (`Scrab::CanSeeAbe`: only if on its level), a Scrab that
    has started its run keeps running at where he was, and with no floor within three cells at its height it runs
    off the end (`Domain::LureLanding` finds such hoist tops and where it comes down). He hangs until the nearest
    is 300 or more off (the climb is 21 frames, back down to hang 24, a Scrab runs 7.4 pixels a frame), climbs,
    is seen, goes down to hang as it starts its run, and climbs again the frame it is over the edge: it lands a
    cell from where he hangs.
  - **Shut in behind a gate** (`ScrabPenTask`, `Shut the Scrabs in left|right of the gate at x,y with Lever#n`).
    A gate with a toggle lever that is out of the Scrabs' reach is worked until it is shut with every Scrab of
    that floor on one side. With a Scrab on each side it is opened while both are on his screen: two Scrabs that
    see each other fight (`Scrab::FindScrabToFight` wants both on the screen that is up) and one dies; Abe is
    safe on the floor above for all of it. Tried by hand and not used: slapping the lock during the fight (it
    worked with 12 frames in hand). Shut in, the Scrab is waited for to be 50 pixels past the gate going away, or
    220 coming.
  - **In the planner** a stretch of floor between gates is a `Domain::ScrabZone`, and `Key::ScrabClear(zone)` says
    no loose Scrab can get at him there. The two actions above give it; whatever is done from a cell a Scrab can
    reach now (a use, a crossing, an exit) wants it, where there is a way to give it. The first screen's plan:
    lure, shut in right (which is where they fight), lock, shut in left, door.
  - **A floor crossed in stints** (`ScrabWaryTask`), for a Scrab that walks a beat under ledges: the second and
    third screens. From a place it can't see him he waits until it is so far off that he is down, done and out
    of its sight before it could be at him. The way is found with the Scrab's floor made dear, cut into stints at
    the places out of its sight, and each stint timed edge by edge from what was measured (a cell run is 4
    frames, walked 9, a turn 13, a slap 39, a drop seen for 6 frames and stood up from in 10 or 3 with the way on
    held, up a hoist 20 to hanging and half the climb more); the Scrab is its delay, 3 frames and 7.35 pixels a
    frame away. While a stint is on, the route is held to the edges that were timed (`Bot::KeepTo`). What it has
    to know about the Scrab, all measured:
    - For 210 frames after it last saw him it is still after him and strikes with no delay
      (`Bot::SeenByScrabAt`): no stint then.
    - It runs its beat at the pace it runs at him, so where it will be when it sees him (the lead: a turn, a
      jump, a drop) is not where it is now. Facing away it has to get to the end, turn and stand out its pause
      (`mPauseLeftMin`/`mPauseRightMin`, read into the entity); facing him it comes once that pause is over
      (`Bot::ScrabStillSince`).
    - A Scrab at a run kills what its box is on, a hanging Abe too: it may not come by any cell of the stint on
      its way (every cell of a long running jump is checked), and a Scrab within a cell and a half strikes at
      once whatever its delay.
    - Its beat is what it is seen to do (`Bot::ScrabBeat`: where it has stood for eight frames, not the frame or
      two after a shriek), since its TLVs bound it to less than its floor, and a Scrab chased out of one cage of
      bounds walks the next one. The beat is the last two places it stopped at. (Until 2026-11-12 it was the
      leftmost and rightmost stops, started again by a stop more than 150 px outside them: so a beat wider than
      150 px was never learnt, and every stint needed the full 12 frames. That stalled a full run on the third
      screen: the Scrab chased him into the left cage (bounds at 25 and 350), under the trap door of lock #6000,
      where the most there is to spare is 4 or 5. That is the way the walkthrough gives: "slap the lock when the
      Scrab is on the left, and run right". `build-bot/x54.txt` sets this up by hand and the bot gets out.)
    A stint is gone on with 12 frames to spare, or, where the best moment of the beat gives fewer (the third
    screen is made to be close), within 5 of that best and no fewer than 4. A stint that can't be done even at
    the best moment is left out and another way looked for. A lock is slapped from the side that leaves most
    time, and the place made for after is the one with room to go on from (a ledge with nothing but the Scrab's
    floor round it is a corner), by how it would be at the best moment so that the choice does not change as the
    Scrab walks.
  - **A lock that drops him.** The third screen's locks set a switch of their own (`mSlapOutputSwitchId`) that
    opens the trap door they are slapped from. That is a stint that starts with the slap: he waits on the trap
    door, slaps, and the way to run is held as he lands (`UseTask::SetAfter`). Measured: through the door 10
    frames after the slap begins, down 11 later, and 8 more to turn if the way out is behind him.
  - **Into a well at a run** (`Prim::RunWell`). The walkthrough: "jumping into the well without stopping to
    position yourself, you have to do it on the fly". `Abe::RunTryEnterWell`: from the fifth frame of the slide
    that stops a run, with up held, in the well's box. A run can only be stopped at frames 4 and 12 of its
    sixteen, 50 pixels apart, and the slide is 21 pixels on by its fifth frame: so the way is let go of before
    the one of those frames that puts the slide over the well. Once the well has him nothing can hurt him
    (`Abe::CantBeDamaged`). From the first lock's drop to the well is 33 frames; the Scrab is there at 39.
  - **Down onto a well** from a ledge takes him where the well leads (`NavGraph`: such a hop's edge has the well
    in `mWell`), and the last frames of that fall count as time on the floor the well is in.
  - Smaller things: a cell or two with nothing leading on is not somewhere new to go (the cell a lock stands
    in, past its trap door). Tried and taken out: counting every region a plan was made from as been in. Necrum's
    path 4 gets on by going "somewhere new" to where it came in while the ring it has slapped for is on its
    way, and stuck without that. The level's graph is made again
    when a profile starts (a script's `bot_goto` before it had made it without the profile's `been`).
  Not good yet: after the first trap-door lock the planner's way back onto the floor is the well that throws him
  onto the platform over the middle Scrab's beat, where the only stint is a drop into the well below it with the
  Scrab at the near end: he stood there 4,800 frames for it.
- **Mudanchee vault 1 (path 2)**, beaten with no deaths, no restores and no bites from the hub
  (`build-bot/mvp2.txt`, 2026-11-12: in at f=638, out of its own door at f=3,255). Two locks, each with Fleeches
  by it that nothing in the path can kill: they are led off. Read the walkthrough's vault 1 first.
  - **How a Fleech chases** (read out of `Fleech.cpp`, then measured in `build-bot/x2b.txt`, `x2c.txt`, hand
    scripts with the trace's `fl=` and `hp=`): it comes at where he is, not along the way he went, and needs no
    sight of him. It crawls a cell in seven frames (3.57 px a frame against his 6.24 at a run or a roll; half
    of both in the background). With him over it, it makes for a hoist within twelve cells ahead of it or
    eight behind and goes up it (40 frames from the foot to on top, measured; 110 px at most) and otherwise
    crawls to under him; with him under it, it crawls his way and falls off the end of its floor (more than
    300 px kills it). No wells, no jumps. It licks along its own floor within three cells, or at him hanging
    over it, and only one that is on the screen. A lick is 15 in 100 of his health (seven kill, and the
    health comes back after 180 frames without one). It does not give him up while he is within 750 px and
    260 px of it at its own scale, and through a well into the background it goes on following his x from the
    foreground, up and down the nearest hoist (watched for 600 frames): no shaking them off that way. The
    vault's are `persistant`: they are not unloaded with their screen, but are not there before it has been
    on once.
  - **The way that works** is the walkthrough's: get them to follow him away from the lock along a floor, to
    the foot of a hoist, go up it and back to the lock over another floor. They come up the hoist after him
    and are behind him by the climb and by what he gains at a run. `Domain::LureFleeches` finds it: of the
    feet of hoists they can crawl to (`mBait`), the one with a way back to the lock (`mBack`) that starts up
    the hoist, keeps off the cells they come by, and loses them the most frames (`mLead`: they follow where
    a Fleech can, and from where one cannot, a jump, they make for the lock as best they may). `mOut` is the
    way he shows himself (their way, the other way round, to some 150 px from where they would have him),
    `mCommit` the last place to turn back at (where the last fall or climb of the way back starts) and
    `mAway` the way out after: to the nearest cell a Fleech at the lock would be 150 frames getting to, a
    well if there is one (the lock's own switch taken to be on: the second lock turns its well round).
  - **The planner**: `Lead the Fleeches off and use SlapLock#n` (`StepKind::eFleechLure`, cost 600) beside
    the plain use, which still wants its guards dead. Where they are shut in with the lock and cannot be led
    anywhere, the barrier that keeps them in is taken to be open and its switch is asked for first, with the
    lever's place as where he sets out from: only a place he gets to 45 frames before they do will do (the
    first choice without that, the foot of the pillar they come by, killed him). So path 2's plan is
    `[Use Lever#1004, Lead the Fleeches off and use SlapLock#1009, ..., Lead the Fleeches off and use
    SlapLock#4000, ..., Exit]`. A lever that a lure wants pulled may open its gate onto Fleeches that are awake
    behind it (not ones asleep or hanging, `mInputs[0]`, `[1]`, and no other lever: with that let go for
    every lever, path 1 pulled one to see what it did and died).
  - **The task** (`FleechLureTask`): to `mBait`; along `mOut` two cells at a time until a Fleech needs 60
    frames or fewer to him (`kFleechDrawn`; with them let out by a lever they are coming already and he just
    waits); back to `mBait` and along `mBack` to `mCommit`, held to those edges (`Bot::KeepTo`). There the
    Fleeches as they really are decide it: the frames the nearest needs to any cell he will be within reach
    on (`Domain::FleechTimes`, by the shortest way a Fleech has; one going up a hoist, `RockAt::mClimbing`,
    is taken to the top of it first, which is what it does) against what he needs for the rest of the way,
    the slap and into the way out, with 5 over. Short of that he waits there while they are 45 frames or
    more from him, for them to start up a hoist, and then gives up. The lock is used from the side the way
    back comes down on (`UseTask::FixSide`: from the other side of the second lock he was 36 frames longer).
  - **It is close, and is meant to be.** First lock (#4000, a tunnel under a floor with a hoist at each end):
    at the commit they needed 112 frames and he 80 by the count; in the game he was in the well 4 frames
    before the first Fleech landed 25 px from him. Second lock (#1009, half scale): 144 against 89 by the
    count (in the game 151 from the commit to the well: the fall goes through the lock's well, which throws
    him out beside it, 40 frames the count leaves out), and he was in the well 13 frames before they landed
    (2 before the top was crossed from a run into a roll). What saves him both times is the 15 frames and more a Fleech needs to land, turn and put its tongue
    out; no bite either time (`hp=` in the trace). The count of his frames (`Domain::RunFrames`: a run
    everywhere, no turns, no well on the way down) is short by 20 to 60 frames and theirs is about right, so
    the 5 over is not a real margin. A truer count fails the first lock by a few frames, which is how tight
    the game makes it.
  - Slapped without leading them off (dropped in beside the second lock, `build-bot/x2a.txt` before the
    lure): four bites, health at 0.40, alive. Not taken as a way.
  - On the way: with Fleeches led on a timed way, a floor that goes on under something low is taken from a run
    straight into the roll with three cells to run in (`Prim::RunRoll`; elsewhere it wants five), and with
    fewer rolled from its start, but not from over a hoist, where down is the way down it. Walked and then
    crouched, the 100 px over the second lock took 72 frames; now 36.
  - Seen with `hp=` and not looked into: in vault 3 (path 3) he is bitten down to 9 in 100 at f=4,215 of
    `mvp2.txt`, and in vault 4 (path 8) to 39. Both paths count as beaten with no deaths; neither has a bite
    to spare.
- **The other two vaults** (`build-bot/mvp1.txt`, `mvp4.txt`: `bot_goto` to the door, up, then the bot). By the
  doors' flames and the walkthrough's numbers: path 2 is its vault 1, path 5 vault 2, path 3 vault 3, path 8
  vault 4, path 4 vault 5, path 1 vault 6.
  - Path 1 (`build-bot/v1.txt`, started at its door): **beaten, out of its second door at f=4,737 with no
    deaths and nothing taken back** (2026-11-12, late; before it stopped at the door with no plan). The
    walkthrough: "pull the lever to free the Fleeches. Lead them back across the top. When they drop down to
    the Scrab, they'll cower in fear. Possess the Scrab and take them out". What it took:
    - **Fleeches led to a Scrab** (`Domain::LeadFleeches`, the action `Let the Fleeches out with Lever#n and
      lead them to Scrab#m`, `StepKind::eFleechLead`, cost 1500, `FleechLeadTask`). The planner looks for a hop
      or running jump they cannot make (`FleechFrames` < 0) with, off the end of the floor it starts from, a
      fall they can make that lands on a Scrab's floor within six cells of where the map puts it (nearer than
      that they stay scared, `Fleech.cpp`'s `IsScrabOrParamiteNear`), a way from the lever to there made only
      of edges they can follow, and a cell on the Scrab's screen beyond the hop, out of its reach and more than
      three cells from the drop, to chant from. The ones let out are those with a way to the lever once its
      switch is on and none before. Wants no power to go unseen in hand, gives the switch and the Fleeches
      dead. Beside the plain use of the lever, which still wants them dead first.
    - **How they follow, as measured** (`build-bot/exp1.txt`, `exp2.txt`): freed, they are on him in a second
      (they set out 13 frames into the pull and stood 100 px off: three bites in 40 frames when he waited), so
      he leaves at once. They make for where he is, not along the way he went: with him over their heads
      further on they crawl to the wall under him and stay there. A hoist they take only if they come by its
      cell with him above (`Fleech::TryGetHoist`: one cell ahead of them). So the task: the lever; to the top
      of the hoist on their way by another way if there is one not much longer (here a running jump to the
      ledge beyond and a hop back: at the foot of their hoist he is within their reach, and hanging from it
      too); there until the first of them starts up; then three to six edges at a time while the nearest on his
      floor is within 190 px, waiting when it is not; at the drop until it is within 150; over; to the chant
      cell; there until none is left on his floor and one at least is down; then `ScrabHuntTask` with the group
      given (`mLed`: the ones by the Scrab are the ones there are, and none left after a spin is all dead).
      One run: lever at f=553, on the hoist top 673, over the gap 883, all three down 973, dead 1279.
    - **A thing on a Scrab's floor that is reached from a ledge over it** (the last lock, #7002): the ledge and
      the floor are one region, so no crossing asks for the power. The use itself does now (`kInvisNow`), where
      a Scrab that strikes at once gets at the cell, the region has cells it does not get at, and there is no
      gate anywhere along that floor (with one the Scrabs are shut in or lured off first: path 3's test).
    - **The power kept for that lock**: a Scrab's floor on the way is routed over as if he had no power while
      it is kept for a later step (`Bot::EdgeCostFn`), so the background Scrab's floor is crossed by its beat
      (`ScrabWaryTask`), and the chant "from the step before" only counts when that step ends on a Scrab's
      floor. The chant then waits, out of reach, while a Scrab stands within 90 px of the thing
      (`Bot::UnseenUse`): this one stands at the lock for 100 to 200 frames each time round.
    - **A slap that does not land**: the bot took a lock for slapped once he had slapped. Here the slap landed on
      the Scrab at the lock (and once on the wall, facing away, carried past the cell by the run he landed
      into), the door stayed shut and the run stood at it for a thousand frames. `World::mLocksIntact` is what
      the game says (`SlapLock::Intact`, a read-only accessor); `UseTask` slaps again while it is intact, only
      from the lock's cell and at a stand, and not with a Scrab between him and it.
    - **Half size**: `ScrabWaryTask::Spare` took distances and the Scrab's speed at full size, so the
      background Scrab's floor never had time to spare; `LandingBits` took the ledge 30 px over it for its
      floor and had him run on as he landed, off the end and into it (two deaths).
    - Left: at the left lock (#5002) the four Fleeches of the pit are on its ledge as he slaps and he leaves
      through them, health 24 in 100. The background Scrab's floor waited for 630 frames on the way to the
      last lock (61 frames on it, 5 to spare).
    - Unit tests: `FleechesLetOutAreLedOverAGapToDropToAScrab`,
      `TheLeverThatLetsFleechesOutIsPulledToLeadThemToTheScrab`, `AThingOnAScrabsFloorUnderALedgeIsUsedUnseen`.
  - Path 4 (`mvp4.txt`): **beaten, out of its door at f=9,074 with no deaths** (2026-11-12, night; before it got
    both locks and died 8 times). What did it is a Scrab taken over to kill Fleeches, made general:
    - **The planner** (`Domain::Build`, before the entities): for each Scrab, the Fleeches on the floor of its
      floor (not hanging, no wall between) are a group; `Have Scrab#n kill the Fleeches of its floor from x,y`
      (`StepKind::eScrabHunt`, cost 900, 3000 from a screen the Scrab is neither on nor at home on) wants every
      gate between them open, and no power to go unseen in hand (the chant would spend it, so hunts come before
      the lock). Chanted for from any cell on a screen its floor crosses that is not low, not on a Scrab's floor
      and that no Scrab gets at. Not on floors with foot switches: those are the rampage's (path 6).
    - What follows from a Fleech being huntable: one asleep on the floor by a lock that gives the power is its
      guard again; one within 100 px of the way out has to be dead; a lever that opens a gate a hunt goes
      through is not held back for the ring.
    - **Gates and Scrabs as facts** (`Domain::ScrabReaches`, now with what it found): a crossing onto a floor
      that only a shut gate keeps a Scrab from wants that gate still shut, or him unseen; onto one a Scrab gets
      at, unseen or with the open gate between them shut. Two actions for the one edge, the one without the
      power first (of two as good the planner takes the first) and 300 cheaper. This replaced the rule that a
      lever which opens a Scrab's gate through a timer waits for the ring.
    - **A timer trigger's outputs follow its input both ways** (`mHoldOnly` on the logic entity, folded into the
      effects of whatever sets the input; no step of their own): lever #3003 off shuts gates 167 and 168 again,
      which the plan needs three times.
    - **The task** (`ScrabHuntTask`): to the cell; waits until the Scrab is the one a chant would take
      (`Abe::FindObjectToPossess`: on his screen, one of his size before any other, the nearest) and chants;
      then the nearest Fleech: never walked up to (a Fleech with a wall behind it and the Scrab within three
      cells eats it, `ScaredBrain` `eCornered`: the first try lost the Scrab that way after one stamp), but the
      howl from 150 to 280 px for the power, a run, and the spin from 110 px, which kills what it touches, one
      a frame. One spin took all four both times. Then left: at home, or where the Fleeches were if a gate lies
      between, that stretch has fewer ways on and off it and nothing he has still to work (a lock) is on it
      (`mScrabParked`: the foreground Scrab is left by the door, the background one walked home).
    - A Scrab left shut in counts as one that gets at him wherever no shut gate is between, even one that
      waits before it strikes (there is nowhere to run to there).
    - **The ring, as late as can be**: for a crossing, chanted when that crossing is the step at hand and then
      only at the last cell before the way goes onto a Scrab's floor; and, whatever the plan says, with the
      ring in hand before any step from a floor no Scrab gets at onto one that one does. A step sooner with a
      Fleech awake on his screen (path 8 died seven times without that).
    - On the way: an exit that "didn't lead out of the path" is given up for good (the way-in door of the second
      screen was tried again each time he came by); the timed Scrab crossing is not given up for Fleeches awake
      that have no way to him (three deaths); a Scrab behind a shut gate does not make him hurry (it had him
      run at a well); the run into a well is right at half size (`Pilot::StepRunWell`).
    - The run: first screen with the Scrab killing the four Fleeches by lock #1008 (no bites there now; the
      four on the way in over the top are still taken, health 39 in 100), door at f=3,149. Second: lever #4003,
      lever #3003, Scrab #3002 kills four and is walked home, Scrab #5000 kills two and is left by the door,
      lever off, lock #4004, lever on, up at the right end, the chant, the run to the door. It was slow,
      5,900 frames for the second screen; later the same day out of the door at f=7,545 instead of 9,074:
      the drop next to the foreground Scrab was refused when it stood exactly 100 px off (it turns at 2037 on
      most rounds and at 2062 on few: 880 frames waited, now 176), a well that brings him onto a Scrab's floor
      was timed from the far well's place instead of where he comes out, the way off that floor is now the
      one away from where the Scrab is, and a Scrab that is taken over runs when it has far to go (home in
      257 frames, not 523). Luring it would not help: it cannot see him on the ledge over it (`CanSeeAbe`:
      nothing more than 45 px above it), and the other wait is for one of the other size.
    - Unit tests: `AGateBetweenAScrabAndAPlaceIsWhatKeepsItOffOrCouldIfShut`,
      `FleechesOnAScrabsFloorAreKilledWithItBeforeTheLockTheyLieBy`, `ASwitchThatFollowsAnotherIsSetAndUnsetWithIt`.

**One run, on the final code** (`build-bot/mall.txt`, seed 1, `any`): from the start of the Mines' path 1,
through the Mines (Necrum at f=39,847), all of Necrum (the vault at 52,494), the vault's path 6 (out at
61,118), its hub, vault 3 (in at 61,905), the hub, vault 4 (path 8, in at 64,022) and the hub again, to the
door of the next vault (path 5) at f=65,428, where the script ends it (`wait_until camera mudanchee_vault 5
2`): 0 deaths, 0 restores, none of its throws missed. The run before it, with two of vault 4's rules wider
than they are now, lost him five times: a climb made dear with a Fleech awake turned him back from the hoist
to Necrum path 1's well (a running jump up it was not made dear alike), and rolling on between low stretches
everywhere moved the Mines' path 6 escape, which has no margin, by 48 frames.

So the Mines, Necrum and all of the Scrab vault (paths 6, 7, 3, 8, 5, 4, 1 and 2) are beaten with `any` and no
deaths. One run from the Mines' start through path 5 (`build-bot/mall2.txt`, out of path 5 at f=73,127) is from
before the work on paths 4 and 1. One run from the vault's start (`build-bot/vall.txt`, 2026-11-12, final code):
path 6 out at f=8,160, vault 3 in at 8,947, path 8 at 11,064, path 5 at 12,470, path 4 at 21,049, path 1 at
28,063, path 2 at 32,911, no deaths, nothing taken back; its time limit (3,500 s) cut it off in path 2 at
f=34,017. What comes after the sixth vault is below (the trial). Of the other profiles `kill_all` gets as far as the vault's path 8 with 3 deaths and
`save_all` sticks in the Mines' path 4 (below). Each stuck report names what stops it.

**All three profiles without a death** (final code, seed 1, headless, 2026-11-12, last runs of the day:
`build-bot/mall.txt`, `long_save_all.txt`, `long_kill_all.txt`; 11/11 ctest, 110 bot unit tests):
- `any`: the door of the vault's path 5 at f=64,970, **0 deaths, 0 restores**.
- `kill_all`: the Mines (paths 1, 2, 3, 4, 5, 10, 6), Necrum, the vault's paths 6, 7 and 8, into path 1 at
  f=87,484: 30 killed, 6 rescued, **0 deaths, 0 restores**. Stops there with no plan, as `any` does in path 1.
- `save_all`: the Mines' paths 1, 2, 3 and into 4: 16 rescued, **0 deaths, 0 restores**, stuck with no plan
  at f=45,864 at the door of path 4's drill room. That room is not solved for `save_all`: the walkthrough
  leads the two blind Mudokons at 512 and 637,1400 off their ledge from the floor below before Lever#11000
  (sw160: it starts the drill over that ledge and moves the lift), then lift, ring, well, Lever#8002 (sw158),
  the three levers (sw153..155) of the drills at 1575, 1650 and 1725, and the portal at 1812,1500. None of that
  leading is built.
What the second round of long runs showed and what was done (the first round is under it):
- Three deaths in `kill_all` at 1562..1637,1500: a running jump up to 1837,1400 with its run-up under the
  three drills, and the kill task standing Abe up in a cell a drill sweeps. The jump rule below no longer
  wants both ends at one height, and neither task stands him up where `GoToTask::SweepOver` has a drill.
- `save_all` at 2137,1500: `hop 2062,1400 -> 2162,1400` fell short and he came down under the drill along the
  floor. A hop to a ledge at his own height four cells off is only an edge where a hoist at that ledge's end
  catches him as he drops (Necrum's path 6 has one, this has none):
  `BotNecrum.AHopToALedgeFourCellsOffNeedsAHoistThereToCatch`.
- `save_all` at 637,1500: Lever#11000 starts the drill at 400..701,1360 over the two blind Mudokons. With
  Mudokons to save, `UseTask` fails a lever or ring that would start a drill where a Mudokon stands.
- `save_all` at 1512,1500: the blind Mudokon #7008 ran for its portal through two drills. `RescueTask` stops
  a drill at work between a blind Mudokon and its portal with its lever first (`mDrillStop`), and with
  Mudokons to save fails when no lever can be got to.
- `kill_all` in the vault's path 8, to and fro at 212..237,980: on the way back up from the secret, a roll had
  to start at the top of the hoist at 187,880 (and again at 287,720), where down climbs down. Both games duck
  Abe by themselves when he walks at a wall that is at his head and not at his knees
  (`Abe::ToLeftRightMovement`): `Prim::mWalkIn` presses the direction there instead of down.

**The other profiles, run on for 140,000 frames** (`build-bot/long_save_all.txt`, `long_kill_all.txt`, seed 1,
later on 2026-11-12, with the fixes listed under them):
- `kill_all`: the Mines (paths 1, 2, 3, 4, 5, 10, 6; Necrum at f=59,747), all of Necrum, the vault's paths 6
  and 7 and into path 8 at f=82,408: 30 killed, 6 rescued, **3 deaths**, all at one place (f≈48,400,
  1562..1637,1500 in the Mines' path 4, killing the blind Mudokon #7008 among the drills). It then walks to and
  fro at 212..237,980 in the vault's path 8 on `Cross portal 162,980->862,1140` until the frames run out.
- `save_all`: stuck in the Mines' path 4 at f=52,573 with 16 rescued, **5 deaths** and 20 restores, all in the
  drill room with the blind Mudokons (2137,1500 and 637,1500; a blind Mudokon is drilled at 637 and 1512).
  Neither `Use PullRing#10001` nor `Use Lever#11000` is reached alive, and the portal for #7008 does not open
  from 1862,1400. Not looked into further: the walkthrough's way through that room has not been read yet.
- `any` with the same code (`build-bot/mall.txt`): the door of the vault's path 5 at **f=64,970, 0 deaths, 0
  restores** (458 frames sooner than before).
What the first long runs showed and what was done:
- Two deaths each at 2912,3060 in path 4: a running jump from 2937 with its run-up under Drill#27002.
  `Bot::EdgeCostFn` has no hop or running jump along a floor with a working drill over a cell of that floor on
  the way (a drill down in a gap he jumps does not count: path 4's way in at 2537..2312,1500 is that).
- Both stood at 2162,1500 for 90,000 frames pressing up: the kill and rescue tasks stood Abe up whenever he was
  crouched, under a low ceiling too, and while their own walk had him ducked for a drill. They now leave him
  down once a press of up got no reaction (`mNoStand`) and while the walk ducks (`GoToTask::Ducked`).
- A hoist at 2062,1500 with a drill going along the floor 15 to 35 above it: `GoToTask::SweepsClear` had him
  off the floor 14 frames into the hoist. Measured: 3 frames before the jump starts and out of reach 17 frames
  into it, so 20.

**The other profiles**, on the final code from the start of the Mines for 30,000 frames
(`build-bot/prof_save_all.txt`, `prof_kill_all.txt`, seed 1): no deaths in either.
- `save_all`: in path 2 at f=28,904 with 12 rescued, 0 deaths, 0 restores.
- `kill_all`: in path 3 at f=28,975 with 14 killed and 2 rescued, 0 deaths, 0 restores.
They began at 15 and 21 deaths. What it was, and what was done:
- Twelve deaths each at 853,880 in path 2: out of the well at 812,720 Abe walked off the ledge and came down
  two cells in front of a Slig on patrol that faced him. `Bot::EdgeCostFn` has no way down (a fall, a hop, a
  hoist, a well) onto a floor within 250 px in front of a Slig that is awake and faces the landing; a Slig
  whose screen is not loaded is taken from the map. The five Mudokons behind it are out of reach and left.
  Tried and taken out, because they moved or broke the `any` run: the same for a Slig asleep close to the
  landing (the Mines' path 10 goes on by exactly such a landing and a run to a well), and a cost for landing
  in a sleeping Slig's hearing (1,100 frames more in the Mines, and a death in Necrum the shift brought out).
- The rest, twelve between them, at one Slig in path 1 that walks 3012..3287 on the 1400 floor and is never
  off its screen (shoot on sight delay 240). Read in Slig.cpp: that delay is for what it comes on as it walks
  (`Slig::RespondToEnemyOrPatrol`). A footstep that is not a sneak, or a word, on its screen alerts it
  (`kEventSuspiciousNoise`, `kEventSpeaking`), and alerted it turns and shoots at once. So:
  - A Mudokon on the floor of a Slig whose whole beat is on the screen Abe is on with it is left, for good
    and before he steps out (`Bot::BeatStaysOnScreen`, `SligNeverLeaves` in Tasks.cpp, for the recruiting and
    the killing tasks): there is never quiet to talk in. Mudokon #33006 is not rescued and not killed.
  - A Slig that waits before it shoots is still run from, but not run past from its front to behind its back
    in the open (`pastIt` in `GoToTask`): then he waits in the shadow and goes behind it at a sneak, as for
    any other Slig. Run past, he was shot 13 frames after it began to turn, a cell short of the next shadow.
  Tried and taken out: sneaking whenever he goes after a Slig with its back turned (eight deaths in rescues
  the run gets through at f=5,100), and holding while the Slig is at the end of its beat.
`any` is as it was to the frame after all of this (f=65,428).

**The Mudanchee trial and the way to the Paramite vaults** (2026-11-15, `any`, seed 1). One run from the Scrab
vault's start (`build-bot/vall.txt`): the six vaults, the trial behind the hub's middle door (paths 9, 10 and
11 of `mudanchee_vault_ender`), out through Necrum and into the Mudomo vaults' way in, 0 deaths. The same from
the hub's door (`tr1.txt`) and from path 9's end (`tr6.txt`). What the trial took:
- A Slog under a ledge he has to leave by a hop or a fall: it leaps and looks in turn (Slog.cpp states 15, 16,
  18) and goes for him once he is within 50 px of its level. `GoToTask` faces the way he will go and leaves as
  the Slog begins `Motion_19_JumpUpwards` (24 frames it cannot break off).
- A refuge reached through a teleporter on a foot switch (`SlogFleeTask`, `mThrough`): run over the plate with
  the run held; the teleporter takes him about 13 frames after the switch, wherever he is by then.
- A Slog spawner's Slig is burst when all its Slogs are out (`World::mSlogSpawners` carries `mMaxSlogs`), and
  the task is done only once Abe is on his feet again after the chant.
- The last lock is slapped unseen (`Use <lock> ... on unseen`, `Bot::HoldInvis`): its Fleeches take no notice.
- Not modelled: Sligs that shoot from the background (`can_zshoot`). Path 10's shoots about 80 frames after
  its screen comes in; the bot is through by then.

**The Mudomo vaults' way in (path 1)**: beaten, 0 deaths, the door to path 7 at f=3,820 from the path's start
(`build-bot/mh1.txt`). It is the walkthrough's way, and all of it came out of general rules:
- Paramites are in the world (`ParamiteAt`, `World::mParamites`, `EntityKind::eParamite`; `Paramite::BrainIndex`
  and `BrainSubState` are read-only accessors). Read in Paramite.cpp: it finds Abe on its own level (40 px),
  same scale, no wall between, whatever the distance. One alone keeps off him and strikes only cornered. With
  another on its screen it chases: 7.14 px a frame against his 6.24, a running attack from 100 px.
- With Paramites on his floor a running jump that catches a hoist costs 20 instead of 400 (`Bot::EdgeCostFn`,
  as for Scrabs and Fleeches). Measured at 2137,460: stopping to climb is where the pair got him, six times;
  the catch gets him up with 70 px to spare. The same rule carries the last run to the door past five of them.
- `Have Paramite#P pull PullRing#R from x,y` (`StepKind::eParamitePull`, `ParamitePullTask`): for a ring a
  Paramite can get under. Abe goes to a cell of the Paramite's screen that is not on its level (any cell if it
  is of the other size), with no orb on that screen, chants until he is in one, walks it under the ring, jumps
  it at the ring (up), and lets it go (the chant held from a stand: motion 29 ends as soon as it is let up, so
  the chant is held through it). The ring may be on another screen of the Paramite's floor.
- Webs (`Domain::WebUp`): a track line (type 8) whose foot is within 20 of the Paramite's floor and whose top
  is by the ring's floor. It is grabbed from a jump within a cell of it, facing it, climbed with up held, and
  left by itself at the top. The floor at its foot may be another line at the same height, a lift's platform
  between them included. Rings 155, 156 and 159 are all of this kind.
- Paramites that come down a web when Abe comes on their screen (`surprise_web`) count once their switch is
  one a screen sets; the task waits until one is down before it chants.
- A lever with a trap door for the cell on its other side is also offered from the trap door, cheaper
  (`... on from the trap door`): at 3187,1140 he lands on it between two Paramites that come down their webs,
  and the walk round to the lever's other side was what killed him, every try. Turned and pulled where he
  lands, he is through the floor 48 frames after landing.
- A lift on its way to a stop the route walks onto is waited for (`GoToTask`, `mLiftComeWaits`): the same
  lever sends one by a timer, and "no route" came before it was there.
Unit tests: `BotParamite.AWebLeadsFromTheFloorAtItsFootToTheOneByItsTop`,
`BotParamite.AParamiteThatCanGetUnderARingIsHadToPullIt`. None yet for the Slog leap wait, the foot switch
teleporter, the spawner count, the trap door lever or the lift wait.

**Meat** (2026-11-15, the Mudomo vaults' path 10): a meat sack is a rock sack that gives meat
(`Entity::mInputs[1]`, `World::CarriesMeat`, `Key::kMeat`). A lock or lever with one Paramite alone within 250 px
on its floor is only used after meat is thrown (`StepKind::eMeatUse`, `MeatLureTask`): from five cells or more on
the side he comes from, thrown on away from it so that it lands on its floor on the screen he is on
(`Paramite::FindMeat` only sees meat on the current screen; its wall test is made at a height where there is no
wall). It runs past him to the meat and eats for `meat_eating_time`. Measured at the lock at 1612,720: thrown
from 1937, slapped with no death. With two or more Paramites on the floor (`Domain::ParamitePack`) the thing is
only used unseen, and the chant comes before he steps onto their floor (`Domain::HuntedAt`).
**Paths 7 and 10, and the bird portal to the vaults' hub (path 8)**: beaten, 0 deaths, in one run from the
pillar of path 10 (`build-bot/n10.txt`: path 7 at f=2,045, path 8 at f=2,367). Path 7's ring was pulled first
try by the same action as the way in's (the Paramite walks a screen and a half through eleven sleeping
Fleeches, which a Paramite does not wake: Fleech.cpp). What path 10 and the way out took:
- Rolled off a low end over a gap he clears it for drops from 40 down (was 80), and a wall that ends 20 over
  the floor does not stop the roll (measured: off 2037,600 onto 1962,640).
- The lock on the trio's floor at 1037,740: slapped unseen. The power (330 frames from the chant) is chanted for
  on the last cell before the first pack's floor; unseen too, a running jump that catches a hoist is cheap with
  Paramites on his floor; a lock is slapped from the side the way to it is shorter (`UseTask`). He is off
  their floor with 8 frames of it left, which is no margin. Before those two he was 50 frames short.
- A bird portal of his own that leads to another path is an exit (`NavGraph`, `ExitTask`): chanted open from
  four cells off or more on its screen, from a floor no pack hunts him on, then hopped into from two cells.
- Down onto a pack's floor seen: off the edge at a run through every ledge on the way (`Prim::mRunOff`), and a
  hop that lands within 150 px of one of them costs 400 more. Measured from the door at 1187,1400: the hop to
  1312,1520 was death 12 frames after landing, a ledge at a time took 100 frames and lived one try in six, the
  run is on the pack's floor at 1345 after 34 frames and gets away.
One run from the way in's start (`build-bot/mh1.txt`, 2026-11-15): path 7 at f=3,820, path 10 at 5,691, path 7
again, the vaults' hub (path 8) at f=8,418, 0 deaths. For that the level's map counts a bird portal to another
path as a way between paths (`GameView::ReadLevel`), and with no way out of a path known an exit into a path not
been in yet, from which the level goes on, is taken before any wandering (`Bot.cpp`, "any path of this level
not yet been in will do": without it he went to and fro through path 7's door for 570 frames first).
Known weak: the 8 frames above. The run off the edge does not happen at the first pit (the fall is the last
edge of that crossing, so nothing says the way goes on the same way).
Also weak, seen once in the long chain from the Scrab vaults (`vall`, f=37,800; the same room is clean from
`tr1` and `tr6`): in the trial's last room he lands on the ledge at 1712,840 with the Scrab below stood still
at 1762 facing right, no crossing is found (spare -1000) for 100 frames and the two Fleeches on the ledge
bite him dead. Nothing is done about the Fleeches while he waits.
Open: a wait for a lift and the rest have no unit tests yet; the Mudomo vaults themselves (path 8 on) are
untouched: a taken over Paramite has to eat Fleeches, talk to another, and pull several rings in one go.

**The Mudomo vaults** (from the hub, path 8; the guide's numbering in brackets). All six beaten, one at a time,
each from its door back to the hub with 0 deaths:

- Path 12 (vault 3), `v3.txt`: f=1,386. The drop down the shaft at 437,460 goes into the well at 437,1500,
  which throws him two cells right into a hoist that ends 40 over the floor: he ends hung from the lock's ledge
  at 487,1380, which nothing else leads to (`NavGraph::WellOut`, `caught`). Fleeches under a lock's floor are
  not its guards. The Paramite on the 1100 floor has the floor's end three cells behind it and turns on whoever
  comes up the hoist beside it (`Domain::ParamiteCornered`, from `Brain_Patrol_State_1_IdleForAbe`: it backs
  off within six cells and strikes with the end within two): with lock 112's power in hand he chants first.
  An open trap door with a well under it drops him into the well (`trapFall` in `NavGraph::Build`).
- Path 4 (vault 5), `v4.txt`: f=1,890. New steps: `eFleechPen` / `FleechPenTask` (a lever that wakes Fleeches,
  and a gate one foot switch beyond it opens and one before it shuts: lever from the far side, over the top to
  the first switch, a cell off it until all are on his side, back over the second: `Domain::FleechPen`);
  `eMeatFeed` / `MeatFeedTask` and `Key::kPackFed` (Paramites that team up over the way out, by where the
  level has them, `Domain::PackLevel`: the exit needs meat lying on his screen, thrown from the last cell
  before their level onto it on the side the way does not go; only in a path with meat and no lock that makes
  him invisible); `GetRocksTask` leaves the first sack's meat for a Paramite of that level and takes the meat
  to keep from the other sack; `Pilot::StepLift` lets go of the rope 20 from a stop it was carried past (a
  middle stop). Sleeping Fleeches on the next screen are sneaked up to from before they can be seen.
- Path 3 (vault 4), `p3.txt`: f=675. Paramites that are after him (brain 2, or on their way down a web) are
  run from like a Scrab: run into roll under the rocks, into the well without stopping, and off the end of a
  floor at a run where the way goes on down (landing further out than the fall was drawn counts as there).
  The way out is a roll off the low end at 1912,980 that ends hung from the exit's ledge 220 below and three
  cells out: a fall edge to a ledge whose `edge` box faces the way he goes (`NavGraph::Build`).
- Path 9 (vault 1), `p9.txt`: f=3,306. `eParamiteTour` / `ParamiteTourTask`: one Paramite taken over and
  walked to every ring it can get to (`Domain::ParamiteTour`), by `Domain::ParamiteRoute`: floors, webs up and
  down, a hop of four cells over a gap, a drop off a floor's end; a lift's stop is floor only while the lift is
  at it (`Domain::ParamiteFloor`). Here: its own ring, down, three screens left, up the web to the ring that
  lowers the lift, on over two gaps, down a 240 web, up another to the ring that raises the second lift, and
  let go there, on a ledge off his way. Fleeches awake on its level are hunted, the nearest to him first:
  the running bite (action at a run, Motion_31) where there is floor or a wall past them and he is not in its
  way (it kills him too), else walked up to and eaten (down). It walks up to sleeping ones, so that it is
  among them when they wake. The chant takes the nearest Paramite on his screen (`Abe::FindObjectToPossess`),
  so a tour or a ring is only offered from a place its Paramite is the nearest to.
  The control sheet of a played Paramite (from Paramite.cpp): walk stops a cell after the way is let go, a run
  two; neither stops at a floor's end; hop from a stand with no way held, four cells; a web is grabbed by
  walking, hopping or falling into it, or from a jump up beside it; at its top it steps off half a cell to a
  side onto a floor within 20, at its foot onto the floor within 50 under it; down eats a Fleech or Slurg a
  cell ahead; a Fleech's hit costs it 0.15 of its life; a fall of over 180 kills it.
- Path 5 (vault 6), `p5.txt`: f=2,784. The Paramite of the background, which the lever lets down its web,
  makes the round of three rings by sixteen legs to the first: webs at the very ends of floors, hops from a
  cell back so as not to stand under the rocks that fall into each gap (`World::mFallers`: a hop is begun as
  a rock has gone by under its feet), under a block that hangs to 19 over the floor (a Paramite looks for
  walls 20 over its feet, 10 at half size: `ParamiteRoute` has its own wall test).
- Path 11 (vault 2), `p11.txt` (in by the hub's door: there is no start at half size): out at f=3,612. The
  levers are in the background and their gates in the foreground: what a gate lets out does not guard a lever
  worked from the other layer. The background wells are a chain of eight, each in an invisible switch's box
  that turns it (`FeatureKind::eSwitchZone`: a well jumped into from inside such a box leads where the box
  sets it), and thrown to a point he flies on into a well just past it (`NavGraph::Thrown`). Then
  `eParamiteClear` (the tour task with no rings): a Paramite taken over to go to where each Fleech of its
  floors is kept and eat what it meets, call the other Paramites along (hello, come on) and stay at the end of
  the floor away from the way out. As measured both Paramites die at it, one after the other, and the floor
  is left clear of Fleeches and of Paramites: it works, but not the way it was meant to (the parking and the
  calling have not been seen to run). Nothing is done on a floor with loose Fleeches until then. Then the two
  locks, each between two foot switches that turn its two gates over: `eStepOn` / `StepOnTask`, and a shut
  gate is opened from the switch nearest to it (`Key::kPlate`).
- All six in one run from the hub, `h8.txt`: paths 12, 4, 11, 9, 5, 3 in that order, 0 deaths, and through
  the trial's door (path 13) at f=16,059.

**The Mudomo trial** (path 13; `t13.txt` starts at its door): lever to the exit door in 1,900 frames, 0 deaths,
out into Necrum's path 5. It is one chase of fourteen screens: four Fleeches let loose at the lever, more woken
on the way, and Paramites that come down behind him on every screen. What it took:
- Being chased is a state of the path (`Bot::RecentlyChased`, 900 frames after a pack of Paramites or three
  Fleeches were last after him): everything is run, and the plan's crossings are not stopped at: all that follow
  one another go in one `GoToTask`, and a thing to use or the way out at their end is gone straight on to
  (`Bot::Replan`).
- Moves that run into one another (`GoToTask`, the chased block, and `Pilot`): a run into a roll under a low
  thing and on under the next ones a few cells apart, in one roll; out of a roll at a run (`Prim::mRunOut`: the
  run button again past the last low cell) where six cells of floor follow; out of a running jump's landing
  at a run; a run, a roll and a running jump as one move (`Pilot::StepRunJump` with `mRunUp`: the roll goes two
  cells at a time and has to end at the take-off, so it is begun an even number of cells before it); a running
  jump into a hoist where the way goes up one; off the end of a floor at a run.
- The map (`NavGraph`): a roll off a low end that ends hung from a hoist (or an edge) out and below; a
  running jump off the last cell of a floor that catches an edge up to 100 higher, three cells on (measured);
  where a well throws him when there is no floor under the point (he flies on about a cell and a half as he
  falls: `Thrown`, which `WellLanding` now uses); a hop off a ledge into a well that a switch turns leads
  either way.
- A gate that is taken away for good the first time it opens (SlamDoor's delete flag) stays open in his view
  whatever its switch does after (`World::mGatesGone`): the trial's three under the ring, whose switch a
  self-closing trap door sets back twenty frames later.
- A Paramite goes under blocks that stop a Scrab (`Domain::ParamiteWall`), so a pack behind such a block is
  still after him.
Forced and left alone: at 2112,620 the low passage ends over a 25 px shaft with a wall beyond it; rolled off he
bounces off the wall and is 30 frames on his back at the bottom (the walkthrough: "You'll fall down").

**Necrum's end, and into Feeco Depot** (`n3.txt`: necrum path 3 from where the trial's movie leaves him, with
the automation command `healing` for the power both trials give): Feeco Depot's path 1 at f=1,104, 0 deaths.
The plan is three steps: the bird portal at 2450,960 to the sick Mudokons, `Work together` on the five wheels,
and the well at 3687,1240, which leads to Feeco only with the wheels' switch on (a well that leads out of the
path with its switch on is a way out that needs it: `NavGraph` and the exit's precondition in `Domain`). A sick
Mudokon is chanted at until it is cured before it is spoken to (`RecruitTask`). An engine bug was in the way:
the movie played on the way to another level was looked for in the list of the level gone to and the game
stopped with a fatal error (Necrum's NEP1C13.DDV on the way to Feeco); `Path_Get_FMV_Record` now looks in the
other levels' lists too.
In one run, `full.txt` (from the Scrab vaults' path 7: the Scrab trial, Necrum, Mudomo's paths 1, 7 and 10, the
hub, the six vaults, the Mudomo trial, Necrum's end): Feeco Depot's path 1 at f=31,828, 0 deaths, with the
healing power as the game gives it. It stops there (Feeco is untouched: at 2312,884, with nothing known to do).

**Feeco Depot** (2026-11-15, `build-bot/f1.txt`: from its first screen with the healing power, `bot any`):
Terminal 1 (path 1), Terminal 2 (path 7), the Main Lobby (path 2) and the junction (path 9), into the path to
the Slig Barracks (path 3) at f=10,578, and through it into the Slig Barracks at f=16,941, with 0 deaths. `f2.txt` is Terminal 2 alone (out at f=3,397, 0 deaths on
seeds 1 to 3), `f3.txt` from the lobby, `f9.txt` path 9 from its ledge, `f4.txt` path 3 from its door (into the
Slig Barracks at f=6,377, 0 deaths), `f5.txt` path 3 from the lever's ledge.
The other way on, to Bonewerkz through path 9's lower half and path 4, is untouched. What Feeco needed:
- **Path 3, the way to the Slig Barracks** (the walkthrough: a ghost lock for the power to go unseen, a lever
  that summons a Slig to take over, two Slig locks that want a code said back, a teleporter, flying Sligs, a
  lift):
  - **Slig locks** (`EntityKind::eSligLock`, `StepKind::eSligLock`, `SligLockTask`; AE's security door). The
    lock says "Hi" to whoever is controlled inside its box, waits 150 frames for a Slig's "Hi", says its code
    with 30 frames between the parts, and then wants the same back, the first part within 60 frames
    (SecurityDoor.cpp). `World::mLockCodes` has each code as the speak buttons a Slig says it with, read with
    the game's own `Code_Convert` / `Code_LookUp`; what the lock is doing is read from it
    (`SecurityDoor::State`, a read-only accessor: the bot can't hear). `SligLockTask::Say` is what is said when.
  - **The Slig** is one a spawner makes when its lever is pulled (the spawner puts its switch back when its
    Slig dies, so the lever calls another). It is taken over from by the lever: on its screen, with no orb
    there. The chant is held two frames at a time: held on after the Slig is his, it is the chant that bursts
    it. For a moment as it is taken it is not listed at all, which is not it dying.
  - **Its way** (`Domain::SligWay`) is over the same graph as Abe's with the edges a Slig can take: walks (not
    under a low ceiling), off the ends of floors, lifts, and teleporters whose lever it pulls itself. Gates on
    the way are what the action waits for: the second lock's action wants the first lock's switch, which is how
    the two come in the right order. The Sligs on the stretches of floor it walks (`Domain::SligsMet`) are
    given as dead by the action, and Abe's own way after goes over their floor.
  - **Other Sligs on its floor** shoot a Slig that is not their own as soon as they face it on their screen
    (no delay here), and a shot takes the nearest thing in its line anywhere on the screen (Bullet.cpp). So
    it stops at the end of the floor above (single steps: it walks a cell or two on after the key is let go),
    waits until every Slig on the screen below is ahead of where it lands and walks away
    (`SligLockTask::DropClear`), drops, and holds the end of their floor away from them, facing them, shooting
    each as it comes onto the screen, until the map's Sligs of that floor are all gone. Walked towards them it
    lost every time; held three cells in from the end the flying Sligs' grenades got it (the walkthrough's
    "wait on the left side on the ground").
  - A Slig that has opened its lock is left as it is when another lock that is shut can be got to from where
    it stands (the next step of the plan opens that one with it); else it is burst.
  - **A floor Sligs watch that has no shadow** (`Domain::SligWatched`: a Slig awake on that floor and that
    screen, no invisible zone on that floor on that screen), in a path where the power to go unseen is to be
    had. The map's Sligs count where the map puts them (one in a box over a floor is not on it: the Mudanchee
    trial's last room), a spawner's only when it is seen there: crossings from or onto it want him unseen, or those Sligs dead (two actions for the one edge); the
    route does not go onto it seen (`Bot::EdgeCostFn`); and the chant is at the last cell before it whose
    screen has no security orb (`GoToTask`: the path's three Slig screens each have one, so it is on the ledge
    by the lift, a screen before). Elsewhere such floors are crossed behind the Sligs' backs as they always
    were.
  - **The run** is 235 frames of the power for a way that takes about 230: off the ledge at a run, a running
    jump up the hoist of the Sligs' block, off its far end at a run, and from the run into the roll through
    the gap the fence was in. Unseen with such a floor on the way it is run like a chase (`chased` in
    `GoToTask`: no stop for a drop, a climb or a roll); a drop is taken rather than let down from the hands; a
    hop off a floor's end costs 60 more than the run off it; a roll is gone into from the run through a gate
    that stands open now (it was only through no gate at all). All of that only while he is unseen by a chant
    made for a crossing (`Bot::InvisRun`): a thing used unseen by a Slig keeps the timing it had (the
    Mudanchee trial's locks, where a run off the ledge cost 33 frames and the power was gone at the last
    lock), and a run off an end is only taken where the way goes on the same way below.
  - The fence is in a crawl gap, put 39 under the floor it shuts (`GameView`: its box reaches 40 up).
  - A hop catches a ledge 60 or more over his own floor with floor still under him, two or three cells off
    (`NavGraph`: the red ledge at 837,1420 from 787,1500 by the wall; from 812 he goes by under it).
  - **A lift that is at its other stop** with a lever by the way that sends it here (a lift mover): the lever is
    pulled (`GoToTask`, `mLiftCall`). The Slig has ridden it down; Abe calls it back up.
  - "no route" in the log says which edges out of what he can get to were refused.
- **Which path the level goes on through** (`LevelGraph::Shut`). The lobby has a door out of the level itself
  (to the executive offices) and one to the path that leads to the Brewery, both behind gates that the ends of
  other paths open. With no plan for the way on and nothing left to switch, the ways towards the end the path
  has are struck out and the distances measured again: the lobby is then two paths from the end, through the
  junction. Backing out of a path is by the way he came in wherever it leads, and a path can be backed out of
  three times (it is come into again by another door: the junction's ledge first, then its background floor
  for the lever that turns the fence off, then the ledge again).
- **Down into a shadow past Sligs** (path 9, AE only). The drop off a hoist is seen and its landing heard by
  every Slig on that screen (AE: a noise is heard on the screen it is made on), so it waits until each Slig
  awake on that floor walks away and is off the screen (`GoToTask`, `mDropWait`); walking off the ledge instead
  costs 300 more where there is a hoist. From cover to cover a Slig that stands or turns has not got its back
  to him; and a Slig that walks ahead of him the way he goes, or is a screen off and will not be on his before
  he is across, is as good as one with its back turned (the two there get out of step, and are never both
  facing away). Tried and dropped: AO's rule with the beat's ends alone (7 deaths for 3).
- **A fart left under a Slig** on a platform up to 84 over his floor (`Leave a fart under Slig#`): the lobby's
  guard by the door at 2162,900, which killed him six times before the door was left alone. Not seen to work
  yet: the plan takes the other doors.
- **The film stone** that opens the first gate is a hand stone to the bot (`eMovieHandStone` with a switch:
  looked into, it sets it).
- **Brew and farts** (`FartFlight`, `FartBombTask`, `DrinkTask`; `StepKind::eFartBomb`, `eDrink`; `Key::kBrew`).
  Up at a Brew machine he drinks, and his next fart is one that goes off: left alone 220 frames later where it
  was made, or, chanted at, his to fly for 900 frames (EvilFart.cpp: a fifth of a pixel a frame more speed each
  frame a way is held, up to 4; walls stop it sideways, floors and ceilings up and down; chanting while it
  stands still sets it off 15 frames later, 60 to each side, 60 up and 30 down). Three uses:
  - flown from the machine to a mine or a Slig (`Fly a fart from Brew# to ...`): the way through the air is
    found on a grid of cell middles (`FartFlight::Route`), round every gate and trap door if it can be, else
    through the ones a switch opens with that switch as what the action waits for. Not with an orb on the
    machine's screen (it is flown by chanting). Sligs are aimed at where the most of them stand once their
    screen is the one shown; until a Slig the fart can get to is dead nothing is crossed onto its floor within
    300 of it. Terminal 1: six farts for the mine at the door, the eight mines and the four Sligs.
  - drunk and carried (`Drink at Brew#`), then made where a flying Slig can be got at from (`Fly a fart from
    x,y to FlyingSlig#`): the cells he comes into a region at, out of the Slig's stretch, no orb on the screen.
    Terminal 2: the floor of its machine has no way down for a fart, so it is drunk there, he drops through
    the trap door and flies it from the ledge at 637,480.
  - left by a mine (`Leave a fart by Mine#`): made two cells from it with his back to it, then off to 200
    away until it has gone, and up off any ledge he hangs from (hung over it his legs are in the blast). For
    the lobby, whose machine has an orb over it.
- **Flying Sligs to get rid of** (`FeatureKind::eDanger`, `EntityKind::eFlier`). In a path with a Brew machine
  the stretch a flying Slig's track goes along (`Domain::FlierTrack`, down to the floor under it) is not gone
  into until the Slig is dead: the edges into it wait for a switch the world gives as on once it is
  (`World::HazardSwitch`). With several over the same corridor it is the highest that is waited for (a Brew is
  one fart), its stretch is all of theirs, and the way into another's costs 1,500 more. Where the plan finds no
  way to get a fart flown at one (the Mines' path 2: the machine is past the Slig) it is got by as before.
  The fart waits at the end of the Slig's track nearest to where it was made (it stops there 30 to 60 frames
  each time round) and is set off when the Slig will be within 40 of it 15 frames on. **Both of Terminal 2's
  with one fart:** the two fly as a pair (the low one passes under the high one's left end, 792, as that one
  stops there, each time round), so the fart is parked between the two tracks and waits, while it has 260
  frames left, for both to be in the blast. Measured before that was built: with one left alive the corridor is
  a matter of luck whatever was tried (1 to 15 deaths a run over three seeds). It cruises at his run's pace,
  an alerted one turns to face him wherever he is, it sees him as soon as it is on his screen and faces him,
  chases at the same 6 a frame, throws its grenade 70 ahead of itself (where a runner will be) when it is
  within 15 of him or at a chance of 1 in 16 a frame, and only a wall between them ends the chase
  (FlyingSlig.cpp).
- **Running jumps one after the other with no stop** (`NavEdge::mX2`, `mX3`; `Prim::mX2`, `mX3`): where a
  running jump comes down in the run-up of the next along one floor the two (or three) are also one edge, taken
  only with a flying Slig about (`Bot::EdgeCostFn`), and run into straight from a walk the same way. A standing
  hop along a floor costs 30 more then. Built for Terminal 2's corridor, and not what gets him through it now.
- A hop down off a ledge onto a cell a mine is in is an edge that waits for the mine, in paths with a Brew
  machine only (Terminal 2's way back from the duct to the door, off 1187,420 onto 1287,460).

**The Slig Barracks** (2026-11-15, `build-bot/b1.txt`: path 1 from the door Feeco's train leaves him at, with
the healing power, `bot any`). Begun, not beaten: the first two screens of its first path with no deaths, and
stopped on the third. The level is thirteen paths (the walkthrough's block 0, four blocks off a hub, the war
room) and wants things not built yet: see "What the Barracks still wants" below. What these screens took:
- **Sligs in the background that shoot out of the screen** (path 1's second screen: three of them on one
  beat, out of step). Slig.cpp, its idle, walking and paused brains: one shoots when Abe is on its screen, at
  full size, in front of it (his x at or past its own the way it faces) and not wholly inside a Z cover
  (`z_slig_cover`: his box inside the TLV's). It does not look while it turns. The shots (three or four) are
  aimed at his x less two frames of his speed and 16 for each shot still to come, towards the Slig, so one of
  a burst hits him standing; a hit takes 0.34 of his health and he dies of the third; 180 frames after the
  last, all of it is back (Abe.cpp).
  - `FeatureKind::eZCover`, `NavNode::mZCover` (standing, 26 wide and 53 high, he is wholly in one),
    `NavGraph::ZCovered` for his box crouched (26 by 34) and rolling (37 wide, 24 of it ahead of him:
    measured). The rock is one cell of cover.
  - `SligCycle` (owned by `Bot`, fed every frame): what each such Slig of his screen does is written down as
    it is seen, and its period is known once the whole of the last period is what the one before it was (a
    shorter match is found by chance after one has stopped to shoot; its motion is part of what is compared
    for the same reason). From then what it does next is what it did a period ago. Nothing of the game's Slig
    is modelled: the bot watches, as a player does. Measured there: 171, 170 and 170 frames, so the three
    drift against each other a frame a round.
  - `SligCycle::Cross`: the way through the open between two covers as rolls from cell to cell with waits
    between (crouched, so a roll starts the frame after it is asked for), found over frames, cells and the
    set of Sligs that have seen him: the fewest that see him, and of those the soonest done. Only its first
    move is made; it is asked again where that ends. `GoToTask::ZWatched` / `ZMove`: along such a screen he
    goes from cover to cover, watches from cover until the periods are known (two rounds), sets out only at
    full health, and not at all if more than two would see him. Only where he is in cover or the way ahead
    has some: the Mudanchee trial's last room has such a Slig over a floor that is run along and dropped
    off with Slogs behind him, and waiting there killed him. Measured: there is no moment when none of
    the three looks on the way to the rock (two see him at best, one shot lands, 180 frames in the rock's
    cover until it is over), and one when none does on the way on from it.
- **Hurt, he rests** (`Bot::Resting`, AE): between two things to do, with nothing on his floor or screen
  that could hurt him more, he waits for his health to come back before he goes on.
- **A roll is heard all over the screen** (Abe's `Motion_23_RollLoop` is a suspicious noise every frame; a
  walking Slig turns to it at once). Under a low ceiling in a shadow, with a Slig awake on that floor and
  screen: no further than the shadow's last cell, and out of it only when the Slig is off the screen and not
  about to be back (`ClearSligTask::SligAbout`: 1 on the screen, 2 off it but standing or coming, 3 off it and
  walking away, when it neither sees nor hears what is on the screen).
- **A sortie** (`ClearSligTask::Sortie`, `Haven`; `Bot::Dash`): a thing to use on a floor a Slig walks, when
  the Slig can't be taken over (a security orb on the screen) but leaves the screen on its beat. A place out
  of its sight right by the thing is found (three moves at most: a ledge over the floor, a shadow), by the
  map with the gates as they will be once the thing is used. If it can be got to first he waits there;
  if the thing itself opens the way to it (here: the lever turns off the electric wall by the ledge), he goes
  there after. While a sortie is on and the Slig has walked off the screen, the way along its floor is rolled
  on from the shadow without standing up, or run. Path 1's second screen, measured: the Slig is off the
  screen for 59 frames and shoots 84 after it left; the roll from the shadow takes 39, standing up 8, the
  lever 18, the hoist 15 to the hang and 23 more to the top, so he is hit as he climbs: twice in the run
  that is kept, one hit short of dying. That margin is the game's (the walkthrough has the same moves), but
  it is thin: see below.
- **A pull-up beside an electric wall** (`NavEdge::mFence`): pulled up onto a ledge (a hoist, a hop or a
  running jump that catches one) he reaches into the cell ahead, and an electric wall there fries him (its box
  is 4 either side of its middle, ElectricWall.cpp). Such an edge waits for the wall's switch.
- **A floor a Slig walks is dear to step onto** (`Domain::SligBeat`, `kSligBeatCost` 400 on a crossing onto
  it from off it or from another Slig's): with the wall off, the way on is along the ledge, not back down.

**What the Barracks still wants** (from the walkthrough and the paths' TLVs; nothing of it is built):
- Path 1, third screen: a Slig that stands and listens (start state `listening`, sight delay 0, turns to
  noise) on a three-cell ledge. The way is a roll off the upper ledge that ends hung from its ledge's end
  (falling with speed past an `edge` he grabs it), up when it has turned away, a run past it and up the hoist
  behind it to the lift's lever. Today the bot goes to and fro on the upper ledge: the rule against landing
  in front of a Slig that faces the landing refuses the fall while it faces left, the route flips to the
  other way round, and flips back when the Slig turns. Then a lift, two more Sligs to pass by hanging and
  running, a well, and a flying Slig to take over that kills Sligs with grenades and pulls a lever for him.
- A flying Slig as a thing he flies himself (grenades, levers), a Glukkon taken over and its orders to Sligs
  (the war room), crawling Sligs and their button, grenades thrown from a boom machine on a count,
  Greeters, laughing gas, flying mines to time, a Shrykull.

**AO, Rupture Farms** (`a1.txt`: from its first screen, `bot any`): paths 15, 16, 18, 19 and 20 with 0 deaths, out
to the Stockyards at f=5,567 (the milestone's AO half; f=6,291 as committed and since: the grenades are
fetched twice in path 20 when the run comes from path 15, not when it starts at path 19). `a3.txt` is path 18 alone, `a4.txt` paths 19 and 20. In the
Stockyards he dies at once: nothing there has been looked at. What AO needed so far:
- A walk goes on from one screen's floor to the next's over the gap between cameras, and the first cell past the
  gap cannot be stopped in (a step back to it is a step back over the gap): crossings on foot the same way are
  merged across it (`Bot::Replan`), and nothing but the walk on starts from that cell (`NavGraph::Build`).
- A way out that begins a cell or two past the last cell of a floor is walked on into (`NavGraph`, `ExitTask`).
- `eSligDrop` / `SligDropTask`: a Slig that walks over a trap door a lever opens is dropped through it. The
  lever is pulled so that the door opens as the Slig is on it: by the Slig's pace and the frames the pull
  takes, measured on the first pull (34 in AO). Until it is down nothing is crossed onto its floor there.
- A Slig in the background (half size, `can_zshoot`: path 18's first screen) shoots into the foreground at
  whatever it faces on its screen, its delay after it first faces it (Slig.cpp: `VIsFacingMe`, no invisible
  zone, no Z cover). Noise is not it and bullet walls don't stop it. Its three shots are aimed behind where he
  runs (Bullet.cpp: his x less two frames of his speed, and 32, 16 and 0 towards the Slig), so they hit one who
  runs towards it or stands and miss one who runs away from it. `GoToTask`: he doesn't start towards one that
  faces him or is turning, keeps a cell and a half behind it as it walks ahead of him, and goes past it at a
  run when it stands at the end of its beat, so that it is behind him when it shoots.
- `FeatureKind::eSaw`: a meat saw that a lever brings down once. `eSligDrop` takes it like the trap door: the
  lever is pulled so that the Slig is under it as it is down (6 frames after its switch is set). Where a hoist
  joins the lever's ledge and the Slig's floor they are one region with nothing to cross, so the way out
  itself needs the Slig dead when the way from the lever to it goes along that floor (path 18's second screen).
- A step over a camera gap that leaves fewer than four cells to its end is walked, not run (he came out of it
  at speed and off the ledge's end), and nothing stops in the first cell past a gap: one cell on (`GoToTask`).
- Things that fall from the ceiling (Rupture Farms' meat) are in `World::mFallers` for AO too, and nothing is
  begun that goes under one that is on its way down or set off (path 16: the switch box sets it off again
  each time he comes down into it after a hop).
- An AO Slig's beat and its wait at each end are read (`AO::Slig::Beat`). From cover to cover past one that is
  awake he goes only as it walks away from him with the time for it: what it has left to walk at 2.7 a frame
  and its wait have to cover his way at a sneak's 2.5 (path 18's fourth screen, two invisible zones on a
  Slig's beat: begun as it stood at its beat's end, it turned on him between them).
- Grenades (path 20: three mines on the only way to the way out, a boom machine two screens down a lift).
  To the planner a grenade is a rock and the boom machine a rock sack (`GameView`): up at it, and what drops
  out of its pipe is picked up crouched, as `GetRocksTask` does for rocks. What differs is the flight, so AO
  has its own (`ThrowArc::FlyGrenade`, from Grenade.cpp): a pixel a frame faster down, off floors and walls at
  half the speed, to rest when it comes down slower than one, through a floor it comes up under. A mine it
  touches goes off at once; else it goes off itself when its fuse has run down (90 frames from when the throw
  is begun) and every mine within 45 of it goes with it. A spot its own blast reaches is no spot
  (`Domain::ThrowSpots`). Here: thrown down from the ledge over the mines it comes off the wall, drops through
  the cell-wide slot between wall and ledge and lies among the three, and one grenade takes them all.
- A lift's stops count as one lift when they are within two pixels of each other across (454 and 455 here).
- Walked off a ledge in AO he keeps the walk's speed all the way down, so a long fall lands well out
  (`NavGraph`: no fall edge where there is no floor there; he went down the lift's shaft past the floor the
  graph had him land on).
- The landing off path 20's last hoist wakes the Slig across the lift's shaft, four cells from the way out: he
  is out before it shoots as things are (a rule to run from a Slig that has seen him was tried and taken out:
  a run of two cells is slower than the walk, and it got him shot at path 16's end).
- AO's security claw counts as AE's orb (no chanting on its screen), and the reflex that takes over a Slig
  that woke is off in AO: the chant took none and left him standing in it for 800 frames.

**The Mines from the start again** (`m1.txt`): all of it and into Necrum's path 3 at f=42,663, 0 deaths. It
had stopped getting through some time after the run of 11-12 (`mall2.botlog`), from three things:
- Path 4: a running jump from 2562,1400 over the gap at 2487, where a drill goes up and down. With a run-up
  of one cell the jump was not even made (he ran off the end); made, he was drilled in the air. A running jump
  whose flight goes through where a drill travels is not taken (`Bot::EdgeCostFn`), and a jump with a one-cell
  run-up is begun out of a turn (`GoToTask`: only then is he at a run by the take-off).
- Path 10: a lever by an awake Slig under an orb was only to be used unseen (a rule from the Scrab vaults),
  which in a path with no lock that gives the power is never. It is asked for only where the power can be had.
- Path 6: after the last wheel a running jump to the ledge at 1662,880 and a pull up were planned in place of
  the step off and the drop, 40 frames the timed escape does not have. The run of 11-12 only kept off that
  jump because an earlier failure had banned it. A jump that ends hung from a ledge costs 150 more.
Left as it was: path 3's "Trap the Slogs" fails three to five times before it works (1,300 frames each).

Not built, from the user: with one Paramite the meat can also be thrown past it into the next screen and the
Paramite walked into that screen (it backs off from him), where it then finds the meat. The fallback for when
no throw from a safe cell lands on his own screen.

### Next, in order

1. The Mudomo (Paramite) vaults from path 7 on: Fleeches killed with a Paramite, meat thrown to draw one off,
   locks slapped unseen past Paramites, the bird portal. The full run from the Mines again (`mall2.txt`), which
   has not seen the Scrab vault's paths 2, 4 and 1 nor the trial. In path 1 the bites at the left lock
   (health 24 in 100) and the 630 frames waited for the background Scrab; path 4's four bites over the top.
   The bites in paths 3 and 8. The wait
   on the platform in path 5's third part. Then the Paramite vaults, untouched. The Mines' path 6
   escape has no margin.
   `save_all` and `kill_all` through the Mines: neither has been rerun. Known wrong for `save_all`: the car
   runs Mudokons over in paths 5 and 10 (the walkthrough stops it short and leads them out), path 4 loses
   three.
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
5. The flying Slig of path 2 (take it over, as the walkthrough does), trap doors.
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
