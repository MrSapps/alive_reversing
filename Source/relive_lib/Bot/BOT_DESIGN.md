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
| Look-ahead | `Lookahead`, `Bot::TryFailed` | With the `look` option: from each save the same moves are tried begun later and later against the game itself until one gets to the next save unhurt; what failed is taken back and is not in the run's inputs. See "Look-ahead" under Architecture. |
| A flying Slig flown | `FlierFlight`, `FlierBombTask`, `Domain::FlierTargets` | Taken over from where he is safe, flown to throw grenades at the Sligs he can't take over and to pull a lever he can't get to. The Barracks' path 1: three Sligs and the door's lever. |
| Grenades thrown at Sligs | `ThrowArc::Grenade`, `Domain::GrenadeTargets`, `GrenadeSligTask` | One fetched from a boom machine and thrown from another floor onto the floor of a Slig he can't take over or go by unseen | `BotGrenadeTargets.*` (4), mutation checked; the task itself only in the game (`b2l.txt`) |
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
| wheels turned together by Mudokons | Mines path 1 (2 and 6 wheels), Barracks path 13 (4, Mudokons at two, one brought by lift) | no | done |
| leading Mudokons down ledges and past a sleeping Slig | Mines path 1 | yes | done |
| sleeping Sligs: sneak, no talking on their screen | yes | yes | done |
| patrolling Sligs: wait in an invisible zone, go behind their back | Mines path 1 | shadows in AO | first version: it got through, sometimes after deaths |
| killing Mudokons (for `kill_all`) | - | - | by slapping, where the way out doesn't need the Mudokon. Not by Sligs, mines or drops. |
| skip movies | yes | yes | done |
| pull rings | Mines path 1 (a wheel is on the same cell) | yes | **not working**: up turns the wheel instead |
| UXBs' timing, disarming | Mines path 1, 5 | yes | AE: turned off where the way goes through one, hopped over otherwise (see "UXBs turned off"). AO: hopped over |
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
  machine only. (Built for Terminal 2's way back from the duct to the door, off 1187,420, taken then to come down
  at 1287,460 where a mine is. Measured since: a hop down a drop of 40 comes down three cells out, at 1262,460,
  beside that mine. Where a dropping hop lands is now worked out between the frames it meets the floor in.)

**The Slig Barracks** (2026-11-15, `build-bot/b1.txt`: path 1 from the door Feeco's train leaves him at, with
the healing power, `bot any`). Begun, not beaten: the first two screens of its first path with no deaths, and
stopped on the third. The level is thirteen paths (the walkthrough's block 0, four blocks off a hub, the war
room) and wants things not built yet: see "What the Barracks still wants" below. What these screens took:
- **Sligs in the background that shoot out of the screen** (path 1's second screen: three of them on one
  beat, out of step; see also the rifle range under "What the Barracks still wants": a cover does not stop a
  bullet once he has been seen). Slig.cpp, its idle, walking and paused brains: one shoots when Abe is on its
  screen, at full size, in front of it (his x at or past its own the way it faces) and not wholly inside a Z
  cover (`z_slig_cover`: his box inside the TLV's). It does not look while it turns. The shots (three or four) are
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

**A Slig that stands guard** (path 1's third and fourth screens; `b1.txt` is at 0 deaths to f=2,982). It
stands where it is put and turns by a wait the game draws (30 to 60 frames, 9 for the turn), shoots at once
what is in front of it on its floor, and turns to any noise made on its screen (Slig.cpp: every step, run,
roll, hop and landing of Abe's is one; a sneak, a pull up a ledge and a jump up a hoist are not). Measured:
- Hung from the end of its floor he is not seen (the game compares the two boxes' heights, and his is under
  the floor). Hung in front of one that walks the floor below he is: shot dead at 2137,1400.
- A turn sees nothing; but at its fifth frame, if he is going through the Slig the way it faced, it stays as
  it was (Brain_19 and Brain_22, `ToggleFlipX`). On its own cell he is seen as he gets there: a sneak ends a
  pixel or two past the cell and is put back.
- Third screen (guard at 2137,1739, the hoist behind it at 2162): rolled off the ledge above he ends hung at
  2087 (`NavEdge::mHung`: such a fall is no landing in front of it). Up as it begins to turn its back
  (`GoToTask::Sentry`, `TurnsFrom`, `UpBehind`: 24 frames, and no other Slig about to see: the one that walks
  the floor below shot him from 1912 as he came up). Then a running jump from the stand that catches the
  hoist in the third cell (`GoToTask::JumpUpPastGuard`: where the way is three cells' walk to a hoist
  with such a guard on it; not an edge of the map), hung 11 frames after the run began, and up at once: one hit, as the
  walkthrough has it. Paused two frames hung there he is dead.
- Fourth screen (guard on the trap door at 1962,1400 between the hoist at 1987 and the well at 1912): hung
  at 1987, up as it turns away, and through it at a run into the well without stopping (`Prim::RunWell`: the
  run is heard, it turns to it, and he is in the well, where nothing hurts him, 12 frames after the run
  began). Not hit. The walkthrough's way (over it by the ledge at 2137, the lever at 2187 that drops it
  through the trap door, back over it) was tried by script and killed him every time after the drop: the
  Slig then walks 1962..2112 on the floor the way back goes over, hears every jump, and sneaked behind it
  he is some 15 frames short. So that lever is left alone while the Slig is there (Domain: a lever whose
  timer opens a trap door under a Slig), and it is pulled from 2212, not from the cell its slam door is in
  (`Domain::UseNode`).
- A lift let go of between two stops stays there in AE (the ride up from 2087,1660 was cut short where the
  screen changes): GoToTask rides on to the stop the way goes from.
- No unit tests: the wait hung under a guard, the run into the well past it, the lift ridden on, the flee
  that now goes where its quick way ends (`Bot::MakeFlee`). The rule that a second fall between the same
  two cells also ends hung is not caught by its test when broken.

**A flying Slig that he flies** (2026-11-15; the Barracks' path 1, its fifth to seventh screens: "possess the
flying Slig. Fly him upwards and blow up the Sligs there ... the right lever deactivates the electric walls and
opens the door"). `build-bot/la/fs2.txt` starts him on the ledge it is taken over from, with `look`.
- **What the game does** (FlyingSlig.cpp, taken over): a held direction adds 0.7 of a pixel a frame to its
  speed (0.49 each way on a diagonal); it loses a quarter of a pixel a frame for each whole pixel a frame of
  its speed, and stops dead under one when nothing is held; its speed is cut to 5.5. It bounces off what it
  flies into. What is held now shows in its speed the frame after next. Throw sends a grenade the way it faces,
  from 5 over its point at 5 to 6.4 along and 6 up, that goes off where it first comes down (a small blast: 30
  to each side); not in the frames it turns or ends a climb, when the game takes no notice of the button
  (`FlyingSlig::GrenadeReady`, read only). Action, at rest in its idle motion and facing a lever one to two
  cells ahead, takes it to the lever and pulls. Chant bursts it. Its box is 25 to each side of its point, from
  23 over it to 8 under (read from the game with `dump_objects`); it is drawn 20 under its point, bobbing.
- **A Slig shoots it** when it is on the Slig's level and in front of it: its point from 46 over the Slig's
  floor to 5 under. Off that, it is not seen.
- **`FlierFlight`** (apart from the game, unit tested and mutation checked): a frame of its speed, how far it
  coasts, the buttons that leave it nearest a place if let go a frame on (`Steer`), the grenade's flight, and
  the places to throw from at a Slig (`Shots`): first from just under an end of the Slig's floor, 40 out from
  it, where nothing on that floor sees it (it waits 36 under the floor, goes up to 12 under, throws, and comes
  down again; the grenade goes up over the edge and comes down 17 to 37 in, the nearer the Slig the further out
  it throws from), then from 60 and more over the Slig's head to either side.
- **`Domain::FlierTargets`**: the Sligs of screens with a security orb (he can't take those over himself) that
  have such a place, on their own screen (the game has only the screen the flying Slig is on: a grenade from
  the next one finds no Slig), with nothing in the grenade's way and a way through the air to it from the
  spawner (`Domain::FlierWay`: `FartFlight::Route` kept to the screens the path has, every floor made longer
  by the flying Slig's width and shut off sideways at its own height). What is shut on the way and a switch
  opens is to be open first. The floor of each is a stretch nothing is crossed into until it is dead
  (`FeatureKind::eDanger`, as for the flying Sligs a fart gets rid of).
- **The plan's steps**: `Bomb Sligs #.. with the flying Slig of FlierSpawner#..` and `Have the flying Slig
  ... pull Lever#..` (what his own pull of the lever needs and gives, for a lever he can't get to), both
  `FlierBombTask`: to the cell of the spawner's screen furthest from its track (where he isn't seen, if there
  is such a cell), chant while one is about, fly it, burst it. A flying Slig that is lost is not the end: the
  spawner makes another as he stands there.
- **Measured** (`fs2.txt`): the three Sligs it can get at bombed by f=1,124 with one flying Slig, each with
  the first grenade (the map's own word that a Slig is gone is what counts: that it is not to be seen is not
  enough); the lever pulled with a second by f=1,647; Abe up two screens to the door's floor by f=2,129.
- **The Slig on the floor by the door** (#11013) is not one for the flying Slig: floors stop it from under
  as well as from over (measured: held up under the door's floor it bounced at 401, 21 under the floor), and
  the only way up there is 16 wide between a wall and the floor's end. He gets past it on foot, by a try that
  sneaks (`Lookahead::Way::eQuiet`, which also leaves out the run from a Slig that is awake next to him:
  running is heard): up from the hoist he hung on and to the door while it walks away. Walked, it has him at
  the door whatever moment he comes (467 tries before that was found).
- **Not done**: the Slig on the trap door (#11011: the walkthrough drops it with the left lever and bombs it;
  nothing of his way needs it gone). A flying Slig that is made as he climbs past its switch again goes for
  him, and the plan does not know.
- **An electric wall's beam** is four pixels to each side of where the map puts it and stands up from its
  floor (boxes read from the game): into the cells beside it only while it is off (`NavFeature::mBeam`).

**Grenades from a boom machine, thrown at Sligs** (2026-11-16; the Barracks' path 1, screens 12 and 13: "get
a grenade ... From the shadows on the far left, toss the grenade to the middle ledge ... Stand on the middle
ledge and toss it to the Slig at the top ... Climb up, turn the wheel and enter the door").
`build-bot/la/b2l.txt` starts him at the door into those rooms, with `look`.
- **The rooms**: the door leaves him shut in by a pillar; over it (a hoist, a short platform) and down to the
  ground, which a Slig walks from 275 to 627 with shadows (invisible zones) most of the way; a ledge at 1180
  (450 to 525) with a Slig that stands and listens, the top floor at 1120 (499 to 725) with the wheel, the door
  out and a Slig that walks all of it; a security orb on the screen. The boom machine is a screen to the left,
  on the ground in a shadow, on the beat of a fourth Slig.
- **What the game does** (Grenade.cpp, BoomMachine.cpp): up at the machine drops one grenade from its pipe
  (none while he has one), picked up crouched like a rock. Throw held makes one in his hand with 90 frames to
  go; a direction with it throws, as a rock is thrown. It falls a pixel a frame faster each frame, comes off a
  floor at half the speed both ways, and off a floor it flies up under (AE; AO's goes through) back down at
  half the speed. Each bounce is a noise Sligs hear. The blast is 60 to each side.
- **`ThrowArc::Grenade`**: that flight, to where it lies when the fuse has run down. One that came up under
  a floor is marked (`mUnder`): where it goes from there is not trusted.
- **`Domain::GrenadeTargets`**: in a path with such a machine, the Sligs of a screen with a security orb whose
  floor has no shadow on that screen (a floor with a shadow is gone by unseen, as always). For each, the cells
  of other floors a lob, a high throw or a far throw from comes to lie on its floor: from two pixels to
  either side of the cell's middle as well and much the same place (he does not stand to the pixel), not up
  under a floor, and no nearer the cell than the blast reaches. Cells he isn't seen in come first. With no
  such cell the Slig is not one for a grenade. The floor of each is a stretch nothing is crossed into until
  it is dead (`FeatureKind::eDanger`, as for the flying Slig's).
- **The plan's steps**: `Get a grenade from RockSack#..` (the machine is a sack that gives one: `GetRocksTask`
  as it was) and `Throw a grenade at Slig#.. from x,y` (`GrenadeSligTask`), one for each stretch of floor
  there is a place to throw from on, which uses a grenade up. The plan it makes in those rooms is the
  walkthrough's: down to the ground, a grenade, the ledge's Slig from the shadow, a grenade, up onto the
  ledge, the top floor's Slig from the ledge's left end, up, the wheel, the door.
- **`GrenadeSligTask`**: to the cell, there until the Slig will be within 45 of where a throw from that cell
  goes off when it does (`SligIn`: along its beat and back at 2.2 a frame, 86 frames on), the throw, and down
  on his knees. A Slig still there 130 frames on is a miss: the step fails and the plan fetches another.
- **Measured** (`b2l.txt`, with `look`): out of the door to path 13 at f=3,979, 0 deaths, 254 tries; three
  grenades fetched for the two Sligs (the first throw at the ledge's Slig was taken back with him dead 59
  frames in, and the first at the top floor's Slig left him knocked off the ledge). Without `look` he dies
  on the ground at f=309: the ground's Slig is got by only by tries.
- **Not good yet**: where a Slig is when the grenade goes off is a guess (it stops to listen at the first
  bounce; the 86 frames take no account of that), and what the blast does to him where he stands is not
  worked out beyond its width. `GrenadeSligTask` is only tested in the game, but for `SligIn`.

**What the Barracks still wants** (from the walkthrough and the paths' TLVs):
- **Path 13, the wheel room: done** (`b3n.txt`, from its door, without look: out to the hub, path 2, at
  f=1,604 with 0 deaths). Four wheels that have to go on within 60 frames of each other (a multi switch: the
  wheels themselves stay on), two Mudokons standing at two of them, four more a lift ride down behind electric
  walls a lever turns off. `Domain::PlanTogether` now takes a Mudokon that stands at a wheel as that wheel's
  (`Team::mFree` is the rest), and the ones brought to the others walk, drop and ride a lift; what their way
  needs (the walls' switch) is what the action needs. `Team::mFromX` is where they come onto the wheels' floor
  (the lift), and Abe's wheel is the free one furthest from there. A leading `GoToTask` goes by lift too, and
  waits on it until the ones that follow stand on it. The plan there: the lever, `Work together`, the exit.
  Not done: the six Mudokons are not rescued (the portal is there once the wheels have turned).
- **The hub, path 2** (`b3n.txt` goes on into it): the centre door wants four switches, one from each block
  (116 the rifle range's lever, 125 a wheel behind the workers' campus, 108 and 218 two more doors that each
  want switches of their own). He reads the hand stone and takes a well to a block. What he is in a block
  for comes from the level graph: `GameView::ReadLevel` reads for each path the switches its things set
  (`PathSummary::mSets`: levers, wheels, a tasks door's own switch) and the ones its shut doors want
  (`mWants`, a tasks door's through the logic that stands for its hubs). `LevelGraph::Wanted` is what the doors
  on the way on of other paths want (only paths no farther from the level's end than his: in the Mines' last
  path a lever sets what a door four paths back wanted, and that sent him off), `Errands` what of it a path
  sets, `ErrandExits` the ways out of a path that lead to such a switch without going through the path
  whose door it is for. In `Bot`: a wanted switch that can be set here is the goal before "Leave the path";
  while one is open here and no plan sets it he looks for a way (switches, regions) instead of leaving, and
  gives it up only when that runs out (`mErrandsLeft`, also for one he left the path without setting);
  `ExitAllowed` takes the ways `ErrandExits` gives and no others while there are any. From the rifle
  range's door the hub now sends him on into the workers' campus by itself.
- **Block 1, the rifle range, path 10: its lever pulled and out of its door with 0 deaths**
  (`ae_barracks_rifle_range.txt`, from its door, no look: out at f=6,463, hit ten times on the way, never twice
  without healing between). Its six Mudokons are not rescued. Fourteen Sligs in the background, two
  or three a screen, and the way goes up and down between covers by hops, drops and hoists, which
  `GoToTask::ZWatched` only timed along one floor. What was found and built:
  - A cover does not stop a bullet: Abe.cpp's `BulletDamage` takes the 0.34 first, and only for the bullet that
    would kill him looks whether he is in a cover (and then gives him all his health back). So what counts is
    being *seen*, not where he is when the burst comes. `SligCycle::Cross` already counted it that way.
  - A cover belongs to a screen: the game looks for it among the TLVs of the camera the top of him is in, so
    one of the next screen that reaches over the edge is none (`NavFeature::mCell`, `NavGraph::ZCovered`).
  - `NavNode::mZLow`: a cover he is only in crouched (the space under the boom machine). It is a place to
    stop and wait in on a roll, and to heal in.
  - `GoToTask::ZLeap`: the way out of a cover by a hop, a drop, a hoist or a running jump, with the cells of
    floor before it (rolled from a crouch, walked from standing) and the steps after it into the next cover,
    as one track of where he is on each frame (measured: roll 1+4 a cell+1, up from a crouch 9, turn 13, step
    3+9 a cell, hop 11 on the spot, 12 in the air, 10 to land, hoist up 40, down 34; his box 14 either side
    standing, up to 26 walking or in the air). `SligCycle::Through` finds the wait before it with the fewest
    bursts shot at him (each Slig's `z_shoot_delay` read from the game: `SligAt::mZShootDelay`), worked out
    again every 40 frames while he waits. Where the leap lands with no cover on the way on, a cover a step or
    two aside is where it goes (behind the boom machine, between the hoist up and the hop on).
  - With a burst in it at best, the other leaps out of that cover are tried (`mZAvoid`, no further round than
    150 of cost), and the one with the fewest is made; two bursts are only taken when 2,400 frames ahead hold
    nothing better, three never. Hurt, he stays in a cover until it is over, whatever the way on is.
  - `Bot::EdgeCostFn(…, inTheOpen)`, for the way `GoToTask` goes and nothing else (the planner's costs are
    as they were): a cell the Sligs of its screen see him on costs 30 more for each of them, 90 on its
    beat, so the way with less of the open in it is the one gone (the hoist by the boom machine, not the one
    under the left Slig's beat).
  - The last three screens took: where a hop off a ledge onto a lower floor lands, worked out between the
    frames it meets the floor in (three cells out for a drop of 40, not four); his box standing still is 52
    high, not 53 (the cutout at 1262,420 begins exactly 52 over its floor); the walk after a leap goes on up
    to six cells into the next cover, and where that cover is too low to stand under the last of it is
    rolled, all of it in the leap's track; a running jump at the end of a roll out of a cover is a leap too;
    out in the open a roll into cover is not waited for (he sat crouched a cell short of the cutout until
    he was dead); where down is no crouch (on a lift, at the top of a hoist) he steps on or walks, timed
    the same way (`GoToTask::ZWalk`); and a hop or running jump costs the open cells it goes over, not just
    the one it ends on (one long running jump past two covers had looked cheaper than the rolls between).
  - Left: he is hit on most stretches (the hoist by the boom machine takes 83 frames in the open and both
    Sligs of that screen look within any 83; a player takes the same hits or is quicker than the track
    allows), and it is slow (each hit is 180 frames of healing and a cycle to learn again). The six
    Mudokons (an orb to blow up with the boom machine's grenade first, then the Sligs taken over).
    `ZLeap`, `ZWalk` and the rest of `GoToTask` here are only tested in the game.
- **Flying mines** (`MovingBomb.cpp`; block 2, path 5, has 15, none with a stopper). One goes round a track
  (collision lines of type 8, end to end) at a fixed pace once it is up to it, from a stand half a pixel a
  frame faster each frame; it is only there while its screen is (unless it persists), and starts again from
  where it is put. It goes off when its box (about 11 left to 13 right of where it is, from 26 to 4 above;
  it changes a little with its animation) meets the box of anything alive at its scale.
  - `BombCycle` (owned by `Bot`, fed `World::mBombs` every frame). Where a mine can ever be comes from the
    map: `BombCycle::Tracks` follows the track lines from the one a mine's TLV is put on
    (`FeatureKind::eFlyingMine`), so the places to wait at are known before its screen has loaded. Where it
    will be comes from watching: once it is at its pace the places it is at are written down until it passes
    the first again, going the same way. A lap is no whole number of frames (72.8 for the second one there),
    so what is told ahead is the place on the lap, found again each frame from where the mine is, plus the
    frames. A lap is kept while the mine is out of sight and over a rewind; back from a stand it is not
    trusted until it is at its pace again. Nothing of the game's mine is modelled.
  - `GoToTask::BombStep`: the way is gone from one cell no track reaches (standing, or only crouched: then
    he waits crouched) to the next such cell in one go, when every frame of it is clear of every mine as it
    will then be (`BombsClear`; he may be a frame late). He walks up to the last such cell before the first
    edge a track crosses. A mine there that has not been round yet is watched from that cell; a track with
    no mine on it is waited for a moment (its screen may not have loaded), then taken as gone. From a crouch
    it is a roll if that is clear, else up and on. If no time in the next 600 frames will do, the edge is
    banned and another way found at once (the drops through the mines' shafts there: a mine is always in
    the way).
  - `GoToTask::Bodies` is where he is frame by frame and how big his box is then (`Body`), the same for the
    drills and the mines. Measured in path 5 (the trace has his box and the mines' when there are any): he
    stands 14 to either side and 53 high, crouched 34 high, walking up to 26 to either side. A hop is 10
    frames where he stands (23 behind and 19 ahead, then 45 ahead as he goes), 5 level through the air at 15
    a frame trailing up to 44 behind, then down 3t + 2.3t squared, and 9 landing. Walked off an edge: 6
    frames on the floor, down 0.9t squared, 10 on his feet again. A jump up to a ledge: 3 level and 6 up
    after the take-off, 3 hung under the ledge, 23 pulling up with his box over the ledge long before he
    stands. Up from a crouch 9, round 12; a running jump with a one cell run-up starts turned away (10) and
    comes round in 5. A level running jump is still a guess (its cost in frames, a wide box).
  - The map: a trap door is hopped over like a gap while it is open and walked over while it is shut (the
    hop carries the trap door's switch as its condition, as the walk onto it and the fall through it do, so
    a plan that shuts or opens it on the way counts what comes after with it as it will then be). In both
    games: hopped whatever the switch said, it broke Rupture Farms' paths 15 and 16; with the condition AO
    reaches the Stockyards 171 frames sooner. Nothing falls through a trap door into a death drop (path 5
    has a floor under its death drop).
  - `build-bot/la/blk5.txt`, from the block's door: past the first two mines, the lift, the first lever
    (which brings the third lift up), over the open trap doors with the mines in their gaps, up and back
    over the top to the second lever (the trap doors shut), down and back across them, with 0 deaths, by
    f=1,704 (`Source/Tools/bot_runs/ae_barracks_workers_campus.txt`). `BombStep` is only tested in the game.
    (That was the run before UXBs were turned off: `any` now leaves the second lever alone, it is only
    for the Mudokon. See the UXBs below.)
  - The mine's box: it changes with its animation, 18 to 25 wide. Told ahead with the box it had at the
    moment of looking, he hopped off beside a shaft as the box grew by 5 and was blown up; so the boxes of
    late are kept too, the frames after which they come round found, and the box it will have then used. Until
    that is known: as wide as it was ever seen. What he is asked to do he starts on two frames later
    (measured: `BombsClear` counts them in).
  - The Mines (`m1`): its last path now hops its trap doors where it walked them, 15 frames sooner at the
    last run to the door, and dies once there under the explosion set (falling rocks and blasts, nothing the
    bot times: before, it came through by when it happened to get there). The retry gets through.
- **UXBs turned off** (AE; `UXB.cpp`: a slap while its light is green turns it off, and its TLV keeps
  "not as it starts" for when its screen is gone). A UXB is an `EntityKind::eUxb` whose switch
  (`World::HazardSwitch`) is on once it is off or has gone off; its cell is a hazard with that switch to
  clear it, as a mine's is, so the ways through it are there once it is off and the hops over it are there
  anyway. `Domain` gives "Turn off UXB#n from x,y" from the cell on either side of it on its floor (one
  with a UXB of its own once that is off); `DisarmTask`, which before only ran for Mudokons in tow, does it,
  going to that cell first since the UXB is not about until its screen is. A UXB's switch is never tried
  "to see what it does". Only tested in the game: `DisarmTask`.
  What came with it, each found on the way through path 5 from the lift down:
  - A lift is ridden no further than a stop that ends the ride that way (`Path_LiftPoint`'s top floor and
    bottom floor, `NavFeature::mC`): the lift the first lever brings up from 1500 to 980 goes down no lower
    than 1158 with him on it. And `World::LiftAtStop` goes by the stop's ends, not a point: the lift at
    3900..3976 was taken for the one at 3850..3926 under it.
  - Every cell he has stood on counts as "been" for its region (`Bot::mCellsBeen`): the cells of a trap door
    are a region of their own once it is shut, and "somewhere new" took him back over them and off a ledge
    there was no way back from.
  - A hop to a ledge: by where he has begun to drop the ledge is out of reach by as much as he has dropped
    (level for three cells, 51 down by 88 out). From 3637,1500 at the ledge 80 up that ends 88 off he fell
    past it, twice.
  - A running jump ends at the ledge it catches on its way up (`NavGraph::CatchesUp`: 30 to 110 up, its near
    end two to three and a half cells and 4 from the take-off), not at one beyond it: off 3637,1500 he was
    hung from the edge that ends at 3549, 80 up, 12 frames later, where the map had him land on the floor
    under it.
  - A Slig that wakes does not stop the run-up of a running jump (he ran on off the end of the ledge). A
    running jump that ends hung from a ledge at its own height is in `Bodies` as measured (16 frames
    through the air, up to 31 over the take-off and 15 under it at the catch, then hung and up in 26 with
    his box down to 70 under the ledge), so the flying mine that goes round under the ledge at 2137,1660 is
    waited for. The Slig asleep under it wakes at the run, and is taken over and got rid of from the ledge
    (the reflex there was already).
  - A Slig walked into a flying mine (`ClearSligTask::IntoMine`, started by `BombStep` once a walk): where a
    mine is in the way and one Slig is on the floor it comes down to, he chants from where the blast does
    not reach (60 to either side of the mine and 60 over it: he keeps 130 off), with the Slig asleep or on
    another floor and no orb on the screen, and walks the Slig at the mine. Both are gone. The
    walkthrough's way with that screen ("possess the sleeping Slig. Use him to blow up the Flying Mine"),
    done from the left ledge before the jump: from the right one the blast would reach him. Only tested
    in the game.
  - The lifts past the Sligs in the background (the two screens over the lift at 1362,1500): a ride on a lift
    out of a cover is a leap as a hop or a hoist is (`GoToTask::ZLeap`, its track 7 frames until the lift
    moves and then 4 a frame, as measured; `Bodies` has it the same for the mines). The two flying mines
    that cross that shaft have to let him go on the same frame: `SligCycle::Through` takes what else has to
    say yes to a wait (`BombsClear` that many frames on), since the times none of the Sligs looks and the
    times the mines are away seldom meet and looking at one after the other never found one. From a lift's
    platform a ledge is hopped to (the walkthrough's "hop right and hoist up" off the lift at 1362,960).
  - The wheel (switch 125) was turned and taken for not turned: the gate by the laughing gas, which is gone
    for good once it has opened, has the same switch, and `World::mGatesGone` kept that switch at the value
    it had then for everything. Not where a door of the path waits for the same switch.
  - Under a flying mine's track that is clear of him only crouched (`GoToTask::MinesOverhead`: the floor at
    720 by the way out, with the last UXB on it) he stays down: at the end of a walk, through the slap
    (turned round where he crouches), and up only when `BombStep` has timed it.
  - **`build-bot/la/blk5.txt`, from the block's door to the hub with 0 deaths, f=4,884**: the first lever
    f=834, the lever that brings the fourth lift f=1,153, the UXBs' middle ledge and its UXB off f=1,520,
    through the door, the Slig walked into the mine f=2,432, up the lift past the first screen of Sligs and
    mines (one burst taken, 203 frames waited for), the hops up to the second lift, the top at about
    f=4,450, the UXB on the wheel off f=4,620, the wheel f=4,668, the UXB by the way out, and out. `any`
    rescues none of its seven Mudokons and leaves its secret area alone. Left to wander it once took the
    hoist at 2687,2179 down 320 into the secret area and died: not looked into.
- **Block 3, the Slig bunks, path 7: begun** (`build-bot/la/blk7.txt`, from its door). Its way out (the door
  at 1051,179, a tasks door) wants four wheels: 177 (at 524,860), 180 (1175,600), 250 (1400,180) and 190
  (425,180). What works: the first wheel; the lever at 400,860 that calls a Slig, with a step off the end of
  the floor before that Slig shoots (`Bot::MakeFlee` takes a drop for a quick way off a floor, as the
  walkthrough has it: "immediately run right to fall to the lower level"); the Slig taken over from the
  floor below and kept for the lock (the reflex that takes over and bursts a Slig on another level leaves it
  alone while a `SligLockTask` runs); on its way it shoots another Slig of its floor from 380 off, before
  that one shoots it (`SligLockTask`, measured: one newly armed at 1087 shot it at 889 five frames after it
  stood); the Slig lock at 1125,930 open at f=1,575 with the first Slig called.
  - The naked Sligs (`CrawlingSlig.cpp`; `World::mCrawlers`, those about and alive). The four asleep under
    the first wheel wake when he goes through the box at 718..742 (their panic switch, 238), crawl to the
    lockers at 1075,960 and come back armed; the three penned behind the first lock's wall (always awake)
    come out when that opens, and the one on the floor over them comes by wings at the locker at 1300,860.
    The walkthrough zaps the first four in the electric wall at 800,980 (its lever sets it on, and the box
    he stands in sets it off again five frames later: a zap is a pull as they are in it). The bot does not:
    the Slig he has called for the lock shoots them, naked (a bullet does kill one) or armed, from as far as
    300 and 380, with nothing shut between (`SligLockTask`). With that the first lock is open at f=1,575
    with the first Slig called and no deaths.
  - The second lock (1249..1375 by 564..615, switch 181) is for a flying Slig: `StepKind::eFlierLock`,
    "Open SligLock#n with the flying Slig of FlierSpawner#m", done by `FlierBombTask` as it pulls a lever:
    flown into the middle of the lock's box, and there it says "Hi" and the code back
    (`SligLockTask::Say`). What it took: `Domain::FlierWay` comes away from a wall first where the flight
    starts nearer one than it flies by (the spawner at 1436 by the wall of its shaft at 1453: no way was
    found to anywhere); the cells to take it over from are ones he can get to as things are, not through
    what is shut; where he stands on the floor of a Slig the flying Slig could bomb, the lever and the lock
    are still planned; and `Domain::FlierSpot` looks at every screen a flying Slig is or may come to be on
    (its spawner's track, one that is about, a locker for wings: `World::mWingLockers`), for the cell
    furthest from all of that. From the ledge at 620 by the lock he was killed six times, 40 under the track
    of the flying Slig that lives up there; from 1137,980, with the one that has come by its wings hanging
    at 1417,828, it is his.
  - Why the first lock comes first is in the plan, not in a cost: the floor by a locker for guns is kept off
    (`GameView`'s `MarkLockers`: a `FeatureKind::eDanger` 380 to either side of the locker, as for a Slig a
    grenade is to get) while a naked Slig that is awake can get to that locker, or an armed one is about by
    it (`World::Switch` of the locker's own switch: `EntityKind::eCrawler` has its panic switch, its locker
    and what is shut between them; `EntityKind::ePantsLocker`). "Open SligLock with Slig" gives that
    switch where the Slig's way goes along that floor, since it shoots them as it goes (and after the lock
    is open it goes after the naked ones further off before it is burst); to the Slig itself such a floor
    is not shut (`Domain::SligWay`). The place the flying Slig is taken over from is on that floor, so the
    plan is the first lock, then the second.
  - Another flying Slig (the one of the map lives on the screen of the second lock, and is only made when
    that screen is first shown: with the one he flies) bombs his as soon as it faces it on the screen
    shown (`FlyingSlig::CanChase`). In `FlierBombTask`, on the way to a lock: in front of another that is
    not coming at him it goes back off that screen and round to the column just behind where the other
    was; behind it, it keeps a hundred off at its height (or stays where it is with no way there); and it
    throws when the grenade would meet the other on its way, the blast no nearer than 50. There: back at
    f=2,092, behind it from f=2,162, two grenades at f=2,303, the other gone.
  - One flying Slig does all a flying Slig is wanted for before it is burst: sent for the Sligs it opens a
    lock it can get to first, sent for a lock it bombs the Sligs after (there is no second one to be had).
    So after the lock it goes down the shaft, left along the bottom and up the middle, as the walkthrough
    has it, and bombs the three Sligs under the orbs there (f=3,775, 4,094, 4,278). For that
    `Domain::FlierWay` looks again with everything a little to one side where its grid finds no way: the
    shaft between the floor's end at 1400 and the wall at 1450 leaves its point ten pixels, between two of
    the grid's columns.
  - A running jump off a short ledge catches the edge of the ledge across as it does a hoist
    (`NavGraph::AddLedgeCatches`: off 1012,720, three cells long, to the edge at 887,720): the way up the
    middle. With it he is at the top (862,200) at f=6,133 with no deaths, the second wheel turned at
    f=4,778 on the way.
  - The top (wheels 190 on the left, 250 on the right), begun:
    - Between two regions the planner takes one way for each thing that shuts it. It was the first in the
      list, which from 862,200 was a running jump onto the ledge the Slig walks; now one that does not come
      onto a floor a Slig walks goes before one that does (only that: by frames, the Mudanchee trial went
      from no deaths to twenty-two). So he hoists up at 887 onto the other ledge.
    - A thing a Slig watches under an orb, in a path with a boom machine: used once that orb is gone
      (`World::Switch` of the orb's own switch). A grenade that goes off on the floor under the orb at
      450,40 (60 over it) leaves it; one held in the hand and thrown late goes off in the air by it
      (`Domain::OrbTargets`, `GrenadeOrbTask`, the walkthrough's throw "on the fifth beep"): the fuse is 90
      frames from when he takes it in his hand (Grenade.cpp), so held `mHold` frames longer it flies that
      many fewer of the same flight (`ThrowArc::Grenade`'s `fuse`). `Domain::BlastGetsOrb` is where it has
      to be then (within 40 to the side, from 10 over to 35 under: from what was seen, not the game's
      boxes). From 537,100: lobbed, held 36 frames, off at 427,70, and the orb was gone (f=7,418). The two
      Sligs under it are then taken over from the ledge one after the other, the lever pulled and the third
      wheel turned at f=8,388 (one death on the way, the one at the hoist at 887). Three places took an orb
      that was gone for one that was there (`UseTask`, the perch reflex).
    - The boom machine's noise is the grenade's own bounces (Grenade.cpp: noise and "speaking" each time
      it comes down), not the button: the Slig turns when it first lands, 16 to 27 frames after the button,
      while he is still at the machine (24 frames). In the run that works the Slig says "What?" first; from
      a cold start at the top it shot at once, 13 times. Not solved: nothing he can do is fast enough then.
    - The right side: four naked Sligs in a pen between two slam doors (switch 187) with their button at
      1313,204, the locker for guns at 1162, the orb at 1175,40, the wheel at 1412, and a Slig on the ledge
      over them (987..1387 at 100, it walks 1012..1262). What is built, in the order the run does it:
      - The Slig on the ledge is one for a grenade though no orb is over the screen it starts on: its floor
        goes on under the orb's screen, and chanted at it runs off to there (seen: taken over neither by the
        reflex nor by a chant begun as it came onto the screen). `Domain::GrenadeTargets` takes an orb over
        any screen its floor is on. Bombed from 887,100 across the gap at f=8,985.
      - The orb from that ledge (`Burst a grenade` needs the Sligs a grenade is for on the floor thrown from
        gone first): two misses from 1037,100 (said to go off at 1154,68; why is not known), got from
        1262,100 at f=10,671.
      - `EntityKind::eSligButton`, `Domain::CrawlerFor`, `CrawlerButtonTask` ("Press SligButton with a naked
        Slig"): from a cell of the pen's screen off its floor, once no orb is over it, he chants and has the
        nearest naked Slig; it crawls a cell at a time (and on into the next if a way is still held at the
        end of one: let go half a cell before), and up counts only on the frame it is first held while it
        lies still (CrawlingSlig.cpp `HandleCommon`). The doors open at f=11,055. Then to the locker, where
        it would come by a gun and still be his: but the other three are awake at once and nearer, get
        theirs first and shoot it, as the walkthrough says.
      - Those three are then ones for grenades, the locker standing for them (`GrenadeTargets`: a locker
        with armed Sligs within 600 of it on its floor; `GrenadeSligTask` is done when one fewer is about).
        They are not taken over (the reflex tries twice and gives up). Two of them come to stand at
        1062,200, under the ledge.
      - **AE's grenade and the floor** (`ThrowArc::LandsOn`). The model had it bounce on off every floor; a
        throw from 662,100 that was to end at 1092 lay at 970 (traced: at 968,200 for five frames, then a
        slide of two), while one from 687,100 onto the ledge across did bounce on. Grenade.cpp sends it up
        off a floor at half the speed, and the next frame looks for a floor it has come up under along a
        way that starts where `Collisions::Raycast` put it. That works in 256ths and cuts off twice (how far
        along the frame's way the floor is met: the 256ths to the floor times the floor's length, over the
        whole pixels of the frame's way down times that length; then that far along the way). Where the way
        divides evenly it is put on the floor to the last bit, the way up is taken to start under it, and it
        is sent down again: there and back until it lies, where it landed. Else it is put a few 256ths over
        the floor and bounces on. `FlyGrenade` does the same sum and keeps the height the game put it at
        (the next landing is worked out from there). Both traced throws are unit tests.
      - `Domain::BlastGetsOrb` is now 40 to the side and 50 over to 50 under: with a grenade that lies where
        it lands, what got the first orb was one that went off in the air some ten over the floor, while
        one that lay on that floor 60 under the orb did not. The second orb took five throws (four misses
        from 887 and 987, said to go off at 1178,79 and 1178,36; got from 1237): the rule is still a guess,
        the orb's own box is not read.
      - With that the block is done from its door (`ae_barracks_slig_bunks.txt`): the ledge Slig bombed at
        f=7,669, the first orb at f=7,987, the third wheel at f=8,963, the second orb at f=11,654, the
        button at f=11,919 (this time the naked Slig he had did get to the locker), the two armed Sligs
        bombed by f=13,666, the fourth wheel at f=13,881, the door to the hub at f=14,350. One death, at the
        hoist at 887.
      - Made to come through at other timings (2026-11-16): from its door with no deaths (out to the hub
        at f=11,798), and run on from the Rifle Range and the workers' campus (`blk10.txt`) with no deaths
        in all three (out of this block at f=23,705; it then stops in block 4, which is not taught). What
        it took:
        - The flying Slig was lost. It waited for a Slig's back 170 under its level, the way up through
          that level takes 60 frames, and the Slig at 837,620 has its back turned for 60. Now it waits as
          near under the level as it is unseen, and where a Slig shows only its back it waits until that
          one has faced it and turned again (`FlierBombTask`). A spawner that gives no flying Slig when
          waited for is kept (`World::mNoFlier`) and nothing is planned with one of its after.
        - A grenade comes back off a security orb (`Grenade::OnCollision_BounceOff`: off anything that can
          explode). `ThrowArc::Grenade` takes the orbs' boxes (`ThrowArc::BoxOf`; an orb's box is read from
          the game once one is seen, `World::mOrbBox`: 14 left to 15 right, 36 to 10 over where it is):
          hit from the side it goes back at half the speed, from under or over the same up and down. Boxes
          that only touch do not count (the Barracks' path 1 throw just touches and goes on). That is why
          the lob from 537,100 at the Slig under the first orb lay at 462, 75 from him, and killed him.
          `GrenadeTargets` now also throws down (flat: under the orb, to 442), and `OrbTargets` flies its
          throws with the boxes too, so one that would go off by him is not chosen.
        - An orb is only there to be blown up while its screen is the one shown: four grenades from 887 and
          987 went off by the orb at 1175 on the next screen and it stayed. `OrbTargets` gives only cells
          of the orb's own screen.
        - Up a hoist onto a floor a Slig walks (`GoToTask`, AE): not while one can be on the screen he
          comes up on with its face to him within 90 frames (its beat's ends from the game, turning two
          cells short of them, 2.75 a frame or 6.25 running); given up after 300 frames.
        - No chant to take over a Slig whose beat goes on off the screen (`Bot::MakeReflex`, the perch):
          it runs off and back and is never taken, and runs its beat for long after, which leaves no time
          to get from the hoist into the shadow.
        - The boom machine with a Slig that stands guard (`GetRocksTask`): into the shadow by the machine
          first, out to the button only as the Slig turns its back (Slig.cpp `Brain_15_Idle`: the
          grenade's noise turns it by way of "What?", which is slow, but its own turn sees him and shoots
          at once), then back into that same shadow at a walk (the next screen was the other Slig's: shot
          there at the near end of its beat). He is all in the shadow before it has looked. Out again for
          the grenade when it has turned its back again, sneaked all the way by the task itself (left to
          `GoToTask` he was walked back three cells, and a walk is heard: it turned at once), and with it
          in hand back into the shadow.
        - The flee from a Slig awake by him is off for `GetRocksTask` and the two grenade tasks.
        - Still luck: the Slig by the machine stands with its back turned for a time the game draws anew
          each turn (seen 65 and 140 frames), and the way out for the grenade and its picking up is about
          80. It came through in each run since; not known to.
    - A Slig that has only just come to stand with its back to all of his way, and stands for long
      (`Slig::BeatPause`: the least it stands at an end of its beat; `GoToTask::Stood` keeps since when),
      is gone behind as one that walks away is: the one by the boom machine walks two cells and stands 120
      frames and more each way. And one that stands with its back to him is not run from (`Bot::MakeFlee`).
    - `GrenadeSligTask` takes the beat the game gives a Slig only if the Slig is in it (two with the same
      number for their bounds are each given the other's).
  Left in block 3: its Mudokons and its four suites.
- **Block 4**, the trophy room (path 9): done from its door, out into the bombing range (path 14) at about
  f=1,800 with no deaths (`ae_barracks_trophy_room.txt`). The bombing range is next: he stands in the shadow
  at 1812,1140 for good, the flying Slig of that screen never off it (`HideTask`). What the trophy room took:
  - A floor Sligs watch was kept off for the power to go unseen wherever a lock in the path gives it
    (`Domain::SligWatched`). Here its three ghost locks are past the Sligs. `Bot::EdgeCostFn` now keeps it off
    only where such a lock can be got to with no watched floor on the way; else the Sligs are got by behind
    their backs.
  - The three Sligs by the door (measured over 1,500 frames: the bottom one walks 887 to 1187 and is back
    every 370; the middle one stands at 1162,399 with its back to the hoist at 1212 for 117 to 124 and the
    other way for 161 to 184; the top one stands). `GoToTask`: a teleporter onto a floor a Slig walks is used
    when it cannot be on that screen facing him within 120 frames (`SligComes`: where it is, the way it goes,
    its beat's ends, 2.75 a frame; one that stands with its back to him may turn now); a hoist up onto a
    ledge a Slig stands on is taken as that Slig turns its back (its turn seen begun, or its back not 8
    frames old: `Slig::VOnSameYLevel` has him in its sight from the pull up to the hang from the ledge over,
    about 80 frames of the 117); and waiting for that under the ledge he is off back to the last shadow he
    was in when the Slig of his own floor can be back within 75 frames, to come again. That went: back once
    at f=299, up at f=652.
  - In a shadow with a Slig on the screen he waited for it to go, though the next cell was on another screen
    its beat does not come onto (the tunnel at 1137,340: 4,000 frames). Not waited for now.
  - A drop down in front of a Slig that faces the landing is refused, and with any other way to the same
    place, however long, that was taken: back up the tunnel, rolled, which the Slig below heard each time and
    never walked its beat for. Now waited for where he is when there is no other way or only one 300 dearer.
    And a drop onto a floor a Slig walks waits until it cannot be back within 110 frames and is off that
    screen by the time he lands (the landing is heard on it: dropped behind it, it turned and shot).
  - The locks: the first behind the Slig's back (`ClearSligTask`, as before; the time to be out of its sight
    again after is now counted in, where a place for that is near: `Haven`, its way back allowed a dozen
    cells). Its ring is what the other two are slapped under: with the power in hand and no place off the
    Slig's level to keep it for, he goes unseen where he stands, and the Slig is let be for as long as that
    lasts (it was sent to take over another Slig by teleporter instead, and the power ran out on the way).
    The orb of that screen knocks him down for the chant (`SecurityOrb`: any chant, 20 frames on); he is
    unseen by then and it costs 40 frames.
  - Not unit tested: all of it is in the tasks. Its Mudokons (six, and the lever that fries the Sligs) are
    left.
- **Block 4**, the bombing range (path 14): done from its door, out by its tasks door to the hub at
  f=3,078 with no deaths (`ae_barracks_bombing_range.txt`). How it goes: the first screen by way of the
  well; down the two trap doors; through the tunnel to the long floor; the lever of 208 at its east end by
  himself; west along the floor at a run, under the ledge at a roll; the lever of 210; into the well by it up
  to the cage; the flying Slig that lever lets out taken over from the cage; with it the one over the lever
  of 214 bombed from the screen before, that lever pulled, and it is burst; down, up the hoist and out.
  What it took:
  - The first screen: a flying Slig goes round over the well, the lever and the trap door and never leaves
    (`HideTask` waited for good). Measured: seen, it is 25 frames to its first grenade, which is 17 in the air
    and comes down 93 in front of it; it stops 90 from him. The walkthrough's way (in the well as it throws,
    so that the mine by the well takes it with it) does not hold: the mine's blast reaches 113 and the Slig
    stops 115 from it. But he can't be hurt from the jump into a well until he is down again
    (`Abe::CantBeDamaged`), and this well throws him out by the trap door. So `HideTask`, after 300 frames of
    a flying Slig on his screen, with the next thing to do on that screen and a well on its floor within six
    cells of it: to the well at a run, in, and from where it throws him out to the thing (the lever); and for
    as long as that flying Slig is on his screen it is not hidden from again (`Bot::FlierDash`).
  - A zone that sets a switch when he is in it (an invisible switch) is a thing to do
    (`StepKind::eZone`, `SwitchZoneTask`): the second trap door is opened by one. Unit tested.
  - The plan. A tasks door with its hubs all on sets its own switch (Door.cpp), which is what the hub's door
    to the war room wants of the block: the door's logic gives that switch too, so the three levers are one
    plan and not three tries. Only where every hub is a lever's: the blocks whose hubs are wheels and locks
    were got through one switch at a time, and planned as one thing the Slig bunks went about it in another
    order and died (to be mended there, then this limit can go). A lever under the track of a flying Slig
    that is about, or of one its own switch lets out, is 1,500 dearer for him (not where the track only just
    comes onto the lever's screen, 60 of it or less: the lever of 208). A spawner that is off with something
    that turns it on wants that first, and is not asked to pull the lever that turns it on. This path has a
    track with two ends 3 apart: `FlierTrack` joins at 2 as before and at 3 where these rules ask (at 3
    everywhere the Slig bunks' flying Slig had other Sligs to bomb, from a deadly place). So: 208 by him,
    210 by him, 214 by the flying Slig. Of two levers one flying Slig pulls, the one that lets another
    flying Slig out is last.
  - The place it is taken over from (`Domain::FlierSpot`): a cell with a floor, ceiling or wall between it
    and all of the tracks there are by it, and no Slig that walks on its level and screen, first
    (`SightBlocked`; `FlyingSlig::CanChase` wants a clear line, and the cage at 487,1380 is walled). Other
    flying Sligs' tracks count only within 1,000 of the spawner (this path has one 1,213 off that gave a
    place two floors up).
  - Flying Sligs he only runs by are not hidden from (`Bot::FlierThreat`): the next thing is on a screen its
    track does not come onto and the way there on its screens is run (after no more than a hoist up). A jump
    up to a ledge by a flying Slig's track, or by that of one a spawner that is on has let out, is 30,000
    dearer (he hangs there a second; dearer than the six cells by a bird portal at 3,000 each that the way
    round costs). No sorting out of wired Mudokons with a flying Slig within 750.
  - The flying Slig he flies (`FlierBombTask`): faced to the lever again when the last touch into place
    turned it; kept, not burst, when the plan's next step is for the same one; another flying Slig on the
    next screen on its way is waited for from this screen, where the other does not see it, at a place from
    which a grenade's flight comes onto the stretch of track the other is on, and thrown at when the other
    will be where the grenade is (then back to where it left its way); one behind it on the way to a lever
    is flown from, not fought; held at one place a third time it is sent down, down and back, back or up
    for a bit and the way found anew.
  - Not known to hold at every timing: the flying Slig's fights. The one of 1186 to 1551 is behind a gap
    under a ceiling where no grenade from the screen before comes onto its track, so nothing is flown
    there. Mudokons die in this (two or more by grenades): fine for `any`, not for the rescue.
  - Run on from the trophy room (`blk9t.txt`, the trophy room's first leg told to it) he is back in the hub at
    f=4,304 with no deaths. What that took over the run from its door:
    - A plan in hand is kept while it holds, and this one is made at the door, where no flying Slig is to be
      had: so it had the lever of 214 for him, and he went for it when the flying Slig could have. A kept
      plan with 1,500 or more left in it is looked at again, and one 400 cheaper by now is taken
      (`Bot::Think`). And a spawner counts as on once what turns it on is done, though its switch is not
      through the timer between them yet.
    - The lever of 208: he comes down out of the tunnel in the sight of the flying Slig of 1186 to 1551, which
      is 72 frames from seeing him to its first grenade and bombs on though he is on the next screen by then.
      The five cells to the lever were walked (59 frames; the run began with more than five cells to go, and
      out of a turn only with the run button held through it) and the pull is 20: bombed as he turned from it.
      With a flying Slig within 750 a move is run, begun with five cells to go, the button held through the
      turn (`Bot::Keen`, `Prim::mKeen`): 43 frames.
    - The flying Slig he flies to and fro before the way under the ledge at 700 (40 high) and no nearer for
      2,500 frames: no nearer in 200, it is sent off as when held, and the way found anew.
  - Left: its Mudokons (seven) and its secret.
- **The whole chain** (`blk10.txt`: Rifle Range, workers' campus, Slig bunks, trophy room, bombing range), run
  whole at `a683d6919`: through all four blocks and in at the war room's door at f=28,701. One death on the
  way, in the workers' campus (below); the Slig bunks, the trophy room (out at f=25,800) and the bombing range
  (out at f=28,462) with none. In the war room (path 11), which is not taught, he dies once at 862,1240 and
  stops at 862,1140.
  - The workers' campus' death is a hop of four cells from 687,620 to the ledge at 587: measured (he made
    not to be hurt), he does not reach, is 15 frames in the air, falls 50 in 4, catches the ledge and hangs
    for 37 before he can pull up, 60 deep, where a flying mine goes by under the ledge with never that long
    between. `GoToTask::Bodies` has it as a hop that lands, and he got through on the frame he came at; a
    frame's difference upstream (the zone step) and he does not. Modelled as it is, nothing is ever clear
    there and he went round by a lift, where he died twelve times: so it is left as it was. The end of that
    path wants another way (the walkthrough's is by the lift to the top and the wheel).
- **The war room** (path 11), its stealth half (`ae_barracks_war_room.txt`, from its door): all six levers,
  both tunnels, the hop into the wells and the Greeters' room with no deaths, to the ledge at 2162,340 at
  f=2,702, where General Dripik is to be taken over (not taught: he stops there). What it took, each read out of the
  game's code and measured:
  - The lever of 239 lets a Slig loose that starts out after him (`Slig::Brain_18_StartChasing`): it shoots
    as it comes, seen or not, and its first shot is 42 frames after the pull begins. A walk to the hoist and
    the climb take longer from either side. A running jump that goes through a hoist he faces catches the
    ledge over it (`Abe::Motion_31_RunJumpMid`), and from a stand with run held he runs at once: from the
    lever's right side he hangs 32 frames after the pull begins. So `NavGraph` has that jump from as near as a
    cell before the take-off, `UseTask` pulls such a lever from the side that faces one, and `EdgeCostFn`
    makes it cheap while a Slig shoots or drops in (`SligAt::mShootsNow`).
  - A naked Slig wakes 15 to 45 frames after its panic switch (random), takes 13 to wake, crawls a cell in
    8, and is 19 at the locker: armed 65 to 95 frames after Abe crosses the zone at 1237,981. Armed, it
    stands 15 frames in which it only shoots at what is a cell in front of it (`Brain_32_Inactive`), facing
    the way it crawled. So the lever of 229 is pulled and the well jumped into at a run (`Bot::Hurry`: run,
    and slid to a stop, while a naked Slig on his floor is on its way; not broken off when the Slig is
    there). Under the armed Slig's beat (1262 to 1437, 30 to 60 at each end) the lever cannot be had from
    either end: worked out, not tried.
  - A well that throws him to a point in an edge's box leaves him hung from that ledge (`NavGraph::Thrown`).
  - A locker's danger floor ends at a gate, and an armed Slig counts for the locker its beat reaches.
  - Hidden is not unheard: a Slig that has turned to a noise shoots at what makes one in front of it, in
    an invisible zone or not (`Brain_23_GetAlerted`). In the tunnel he can only roll, which is a noise: two
    cells, just after the Slig has turned its back, then still until it turns away again (`GoToTask`, the
    hush rule). The walkthrough: "Whenever the top Slig turns away, roll forward a couple of spaces".
  - A floor that a spawner's Slig comes down on when he walks into its switch's zone is not gone onto
    (`MarkSpawnTraps`): the floor of the third screen, where the walkthrough goes by the upper tunnel.
  - A hop off the end of a floor is followed further down when it ends in a well (`NavGraph`, the drop table):
    off the second tunnel at 1937,840 into the well at 2037,981, thrown on into the next, hung at 2137,881.
  - `SligComes`, for a drop onto a floor only, counts the least time a Slig that walks away stands at that
    end of its beat (`SligAt::mPauseLeft`, `mPauseRight`): the lever of 234 is 130 frames from the ledge and
    back into the well, and its Slig is off that screen 177.
  - A spawner is not a Slig that stands where its marker is (the rule for a landing a Slig faces).
  - Greeters: their beam is a motion detector's that its body carries about at 3 a frame either way, so
    `BeamNow` gives it with that, and `World::BeamClear` takes the beam to be that much nearer each frame
    (`BeamAt::mDrift256`; unit test, 5 mutants caught). The rest is the rule for a fixed beam.
  - Where two danger floors overlap, a cell is the one's whose middle it is nearer; and a locker's armed
    Sligs do not count past a gate that is shut and that their beat ends short of (the live world has the
    lockers' gates: `MarkLockerGates`).
  - A real fault in the game's data: General Dripik's and Phleg's animations and gibs were listed under the
    ender levels, which the conversion skips (they share a LVL), so the game stopped with a fatal error
    on the Greeters' screen. Listed under the Barracks and Bonewerkz now; `kAnimationVersion` 13.
  - What these broke on the way and how it showed: the whole chain. A naked Slig armed in mid-chant no
    longer broke the chant off and began it again (`Hurry`'s exemption was too wide: only a keen run now),
    which put the Slig bunks 36 frames out and lost a Slig; an orb's bolt that knocks him down as he sets
    off is not a blocked way (`Bot::KnockedLately`); a Slig a lever calls drops in as a spawner's does and
    is not one that shoots (`mShootsNow` only under a spawner whose Slig starts out after him).
  - The chain as it is now (`blk10.txt`): the one known death in the workers' campus, the Slig bunks and the
    trophy room as before, and in the bombing range he stays out of the flying Slig's sight at 1662,1500
    and does not go on (he comes in 5 frames sooner than he did; from its door and from the trophy room's
    the bombing range is still done with no deaths). That fight was never proven at every timing.
  - The war room is the level `barracks_ender`, not `barracks` (same path data): started as `barracks`, the
    Glukkon's teleporter is a change of level and the Glukkon is gone with it. The run scripts start it as
    `barracks_ender`; there the dash for the lever of 232 was sneaked and shot, so `Hurry` runs what was to
    be sneaked too, unless a Slig with a gun is near on that floor with nothing shut between (the Slig bunks),
    and only for a naked Slig that can get to a locker.
  - General Dripik: `GlukkonAt` (the live ones), `EntityKind::eVoiceLock` (GlukkonSwitch), and
    `Domain::GlukkonWay`, the way a Glukkon that is taken over has to a voice lock's box: a cell at a time,
    off a floor's end down to the next, through a teleporter (a wait on its switch), and a jump of four cells
    over mines or a gap (measured: 2087 to 1987). A lock with such a way is an action, with the switches the
    way waits on as what it needs, so the lock before it is what is tried first. `VoiceLockTask` chants from a
    cell of the Glukkon's screen, walks it along that way, says "Hey" 45 frames after it is in the box and
    "Do it" 75 after that, and waits out a teleporter. He stays in the Glukkon from one lock to the next
    (`Bot::Replan` waits while he is in his trance with nothing to be seen that he is in).
    From the door: the phone (237) answered at f=3,214, the mines jumped, the lock of 231 at f=3,585, no deaths.
  - Sligs ordered about: a lever on a floor that a Slig comes down onto when the Glukkon calls for help
    (`World::mGlukkonHelp`, the spawner with that switch) is an action, "Have a Slig pull Lever#N", and
    `SligLeverTask` does it: the Glukkon goes to the cell next to the lever, or the nearest it can get to on
    the side away from the spawner and out of a mine's blast, says "Help" when no Slig is on its screen and
    floor, then "Hey", "Come here" until the Slig is a cell from the lever (a Slig on its way stops at a
    lever), then "Do it". Mines on the Slig's way are not planned for: it walks into one, both are gone, and
    the next call brings another (five Sligs for the four mines and the lever of 241). `GlukkonWay` knows
    walls, mines that have to be gone, and a trap door it drops through when its switch opens it: for the
    lever of 242 it stands on the trap door, next to the lever, and falls with the Slig.
  - Out of the Glukkon: what Abe is to do next waits until he is himself again (`Bot::Replan`).
  - A well under the end of a floor he drops off: off it at a run (`GoToTask`, the fall case).
  **Done from its door**: the last voice lock at f=6,277, out by the door at f=6,628, through path 16 and
  into FeeCo Depot at f=6,989, no deaths (`ae_barracks_war_room.txt`).
- **The whole Barracks in one run, timing-proof** (2026-11-17; `blk10.txt`: hub, four blocks, war room, out
  into FeeCo at f=36,357, one death left, the workers' campus mine hop). The bombing range passed from its own
  door and failed in the chain, 12 frames later: three things in it were luck. Tried at 6 to 18 different
  starts each (the same script with its first wait changed), all without a death now:
  - *The first screen's flying Slig* died 5 times in 8. It dies of the mine by the well (a mine kills what is
    within 113 of it, `GroundExplosion`), not of its own grenade: it keeps 80 from him within 15, so at the
    well it hangs 95 to 120 from the mine, and coming after him along its lower run it stops anywhere in
    those 15, or throws on the way (one frame in 16) and the mine is gone early. Possession is out (an orb).
    So `HideTask`: to the last cell of the shadow; a call ("Hello", `CanHearAbe`) when the flying Slig is
    where it will hang over its upper run, past where it keeps from him as he comes down: it stops 15 frames
    after the call, 40 further on, for 91 to 117 frames, and hears nothing else meanwhile; then to the well,
    where it comes the long way round its track with its back to him, so from the mine's side, and stops at
    1648 at the most; and into the well only when its grenade is in the air (the walkthrough's "just as the
    flying Slig lobs a grenade": he stands in the well's mouth some 30 frames, the grenade flies 17).
    A track without an upper and a lower run over the well's floor is not known: after three calls that
    did not stop it there he goes as he did before.
  - *The flying Slig he flies, against the one over the lever of 214*: the place it waits at for the other
    was given up six frames after it was found. A track was told by where its box begins, and what
    `FlierTrack` finds differs with where the flying Slig is on it: two are the same track where their boxes
    meet. And the other was aimed at straight on from where it was, on a track that turns: `FlierWatch` keeps
    where a flying Slig was each frame, finds its round (the last 14 frames as they were before), and says
    where it will be. From either of the two places the grenade meets it now.
  - *The flying Slig over the long floor* (track 1186 to 1551) is made as he comes onto its screen in the
    tunnel, hangs 105 frames facing the tunnel's end, then goes round every 146, at 7 a frame to his 6. It
    sees what is in front of it on its screen and is after him 22 to 29 frames later; run from, its grenades
    come down where he was, and it is standing that it gets him at. Dropped out at once he was seen falling
    and lived or died at the lever of 208 by a few frames. Inside the tunnel a cell short of its end he is
    never seen (in a shadow there). So `GoToTask`: a roll that ends in a fall onto a screen a flying Slig
    lives on stops a cell short; there he watches it go round (`FlierWatch`) and goes on so that he lands
    as it is as far off as its round takes it, with its back to him all the way down: the landing's thud
    stops it there. (Out as it had just gone by under him, it hung over him and had him at the lever.) The
    wait is 250 to 450 frames. `Bot::FlierTimedAt` keeps the hide reflex off that cell.
  - *In the war room*, from the chain, the plan was "off the ledge, then the last lock with the Glukkon",
    and he chanted himself out of the Glukkon for the first step: what a Glukkon he is in does (a voice
    lock, a Slig sent to a lever) is planned from where he stands in his trance only.
- **FeeCo Depot, the second visit** (`fc2.txt`, from where the war room's door lets him out):
  Brew and the mines on the way up by what the first visit taught. The lobby has three ways out into path 9,
  which is three places with nothing between them; two are open and lead nowhere, the third is behind the
  Slig a fart gets (the walkthrough's door to the Bonewerkz). A way out he has come back out of with no way
  on is a dead end, and not taken again while another leads to the same path (`Bot::mDeadExits`): the third
  is then planned, fart and all. Behind it (path 9's lower part, the walkthrough's "path to Bonewerkz") four
  Slogs kill him: bones are not known yet. What the game does (Slog.cpp): a Slog after him that is 50 or
  more under him goes for a bone that has landed on the screen and eats it for its eating time; all go for
  the same bone, and it is gone when the first is done.
  - **The path to Bonewerkz done** (`fc2.txt`: from the war room's door's landing in the lobby, out of path
    9 by its door to path 4, Terminal 4, at f=5,993 with no deaths). What it took, as the walkthrough has it:
    "Stand at the hand panel and throw down a bone to the Sloggies. Walk down, throw down another bone for good
    measure, and pull the lever before climbing back up. You have to run to make it through the trapdoor in
    time. ... climb up to the ledge while the Sligs in the background are moving to the right. Toss a bone
    through the electric wall; the Sloggie will walk over the pressure plate that turns it off. Throw down your
    last bone and run to the exit."
    - The bone bag is a sack (`mInputs[1]` 3); what it gives is thrown as rocks are. It is not hit while the
      Slig a fart is for stands by it (what is done on that Slig's floor waits for the fart, as its door does),
      and a path with a bag is not left without bones where they can be had (`Bot::Replan`, the goal with
      the bag's count in it first). Four bones, and four are used.
    - In a level whose sacks give bones (`gThrowableFromOverlayIdAE`), the stretch of floor from wall to wall
      of each Slog that is awake is one nothing is crossed onto until a bone is on it or in the air over it
      (`MarkSlogPens`, `EntityKind::eSlog`, `World::Switch`, `World::mBones`).
    - "Throw a bone to the Slogs at .. from ..": from a cell 50 or more over their floor, the throw that has
      the bone down soonest counting his walk to where he goes down (`Domain::BoneThrow`, by `ThrowArc`: a bone
      flies as a rock does in the air, but rolls where a rock skips, so a throw that comes down on another
      floor first is not one; off a wall in the air is no matter). Not from within his arm's reach of an
      electric wall the way he throws (62): the arm goes into it. `FeedSlogsTask`: the bone comes to hand,
      then goes with a direction held; done when it is 70 over their floor or lower, so that he is on his
      way down as it lands. The last bone is out of the count once it is in his hand.
    - On their own floor with one bone or none lying: another, but not the ones the plan has throws for
      later, and not stood in the open under Sligs in the background (`Bot::MakeReflex`). The Slog reflexes
      (the ledge wait, the flight) leave a Slog with a bone to go for alone (`World::SlogFed`), and a try
      after a death does not hold back with a bone out.
    - A trap door that shuts by itself (stay-open time, 60 here) is not crossed some time after its lever:
      `StepKind::eTimedDoor`, "Use Lever#.. and at once Cross ..", `TimedDoorTask`: the lever, then to the far
      end of that edge at a run with the quick ways cheap (`Bot::TimedRun`: the running jump into the hoist,
      the run off the floor's end). Measured: lever to the trap door 68 frames; walked up the hoist it was 92
      and shut.
    - "Throw a bone past FootSwitch#..": a plate on their floor that anyone sets, between where the Slogs are
      (as near him as their stretch lets them) and where the bone comes down. Gives the plate's switch only.
      Waited for where it was thrown from (that ledge is cover from the Sligs of the background).
    - The last bone and the Sligs in the background: the wait for the Sligs comes before the throw. From a
      cover, with the way down to the Slogs from that cell, `FeedSlogsTask` asks a `GoToTask` for that way
      when it would go, with the throw's 34 frames stood there first (`SetZHold`), throws then, and goes down
      at a run as the bone flies. On the Slogs' own floor with a bone lying the Sligs are not waited for.
    - A bone is not thrown to see what that does, and a kept plan's step that gives nothing it has not
      already is done (it threw all four from the ledge, one after the other).
    - Not the electric wall's fault only: what killed him on that ledge was found with a log line in
      `Abe::VTakeDamage` (taken out again): the wall, 48 from the cell he threw from.
    - In a door (AE's motions 114 and 115) he is where he was until he is out of it: the plan waits. Coming
      into path 4 that old place was a cell of the new path, and he planned from it and gave up.
    - A trap door that shuts by itself is a timed one in AE only: AO's all do (20 frames), and were got
      through as they were (Rupture Farms went from 14 deaths to 25 with them timed).
    - Sligs in the background that keep no beat (they stand at the ends of their beats for a time that is
      drawn, and one that has shot is off its beat: `SligCycle` never knows them). The game's rule is that
      one shoots when it faces him, walking or not (Slig.cpp, `Brain_21_Walking`), so `GoToTask::ZLeap` goes
      when each has its back to him and cannot face him again before he is through: its walk to the end of
      its beat at 1.6 a frame, its pause there, its turn. Before, with no beat known after 700 frames, he
      went as he came. And a leap out of a cover that only hides him crouched is timed with the 11 frames he
      is up for first.
    - A trap door that shuts by itself is "the lever and through at once" only where a lever opens it (the
      Barracks' path 14 has one a zone opens), and the well dash of that path's first screen knows the lever
      of such a step.
  - **The chain from the Barracks' hub** (`blk10.txt`) now goes on through FeeCo into Terminal 4 at
    f=43,642: 5 deaths, the known mine hop, two in the war room at the lever of 232 (f=32,785: from its own
    door the war room is still done with none, so that is timing, to be found), and two in Terminal 4.
  - **Terminal 4** (path 4, in progress; `fc2.txt` goes on into it). Its first stretch, the Sligs of the
    background over the platform and the crates ("Climb up to the raised platform, walk forward and crouch,
    all while the Sligs are facing right. On the far edge, stand and climb straight down."):
    - A hoist up is hung from the ledge and then up over it, not stood on it for its 28 frames: a cover that
      reaches down past the ledge hides him for most of it (`GoToTask::ZLeap`'s track). Taken for stood on the
      platform all the way up there was no time the two Sligs (rounds of 208 and 172) gave him.
    - Up on a platform whose cover only hides him crouched: down at once, which is part of what is timed; at
      the top of a hoist, where down is back down it, a step on first (`mZLowLand`, `mZCrouchAt`).
    - With a burst at him at best in the next 600 frames, 2,400 are looked through for a time with none.
    - A plan that only tries what a switch does is dropped once there is a way out.
    So: up unseen, along crouched, down the far edge by its hoist unseen (he chose that over the hop by
    himself once the hop was known to cost a burst). Further on he is still hit (four times, not killed) at
    hops whose best time has a burst in it, and gets the bones at 1562,640. The Slog floor under it is four
    screens long (26 to 1449): a bone is one that comes down on the screen he throws from, wherever along the
    stretch that is (`Domain::BoneThrow`), and with that the whole way to the door is planned. He dies on that
    floor: a bone, down, another, and then all 1,200 of it at a run, caught a third of the way (1053,1240).
    What a bone buys there is a run to the next ledge while they eat ("Throw a bone down and run to the next
    high ledge. Repeat"): the way along such a floor has to go from one ledge over it to the next within that
    time, which is not built. (A dearer cell on a Slog floor did nothing: a hoist up with Slogs awake under it
    is dear already. Taken out again.) The door to Bonewerkz is at
    1676,1697, behind the door at 650,660 that the lever of 175 opens. Then the train.
- The hub and its four blocks, then the war room: a Glukkon taken over and its orders to Sligs, crawling
  Sligs and their button, an orb blown up with a grenade (then its Sligs taken over), grenades thrown on a
  count, Greeters, laughing gas, UXBs, Slig voice locks, ghost locks, a Shrykull.

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

### Look-ahead (the `look` option)

What the planner cannot work out is *when*: a Slig's turn, a patrol, a burst from the background. Each of those
had a hand-written tactic, and each tactic took a session and still let him be hit. With `look` the bot finds the
moment by trying it against the game itself, from a save, and taking back what fails. Nothing of the game is
modelled or copied. The tries are not in the inputs a run leaves (`bot_inputs`), so a replay shows only the one
that worked.

- **A try** is the game run on from a save. It has failed when Abe is hurt (health lower than the frame
  before), dead, or (`save_all`) a Mudokon is. It has worked when the next save is taken.
- **Saves** (`Bot::MaybeCheckpoint`) are taken as before when he is at rest, and with `look` also hanging from
  a ledge and with Sligs awake on the screen (every 30 frames there, not 90), but not hurt nor within 60 frames
  of being hurt. A save nothing gets on from costs little: see the ceiling below.
- **The first try from a save** is the bot playing as it would without `look`. What it pressed, up to where
  it failed, is kept (`Saved::mPressed`).
- **Every later try** holds still for a delay and then does one of five things (`Lookahead::Way`), in this
  order:
  1. *the same buttons*, frame for frame, at every 32nd frame up to 320 (where the first try failed the bot
     takes over again and plans from what it finds): the same thing begun later, with a tactic's own waiting
     not done twice;
  2. *planned anew* from what is found at that moment (those moments, as for the next two);
  3. planned anew with every step sneaked;
  4. planned anew with every step run;
  5. the same buttons at the moments between, down to every 8th.
  Hurt while he still held back and every delay that long or longer is ruled out without being tried, whatever
  is done after it (`Stretch::mCeiling`): a save taken where he is about to be shot is used up in a few tries.
  Six tries of one kind that began at different moments and each failed as long after it began, at the same
  place, end that kind (`Lookahead::kAlike`).
- **A try with nothing left to do has failed too** (`Bot::Stuck` with `look`): what led there is taken back
  and done another way while there is one. The same buttons played blind can leave a tactic half done (the
  power to go unseen spent early, in the second vault), and the plan then ends where no plan is.
- **Out of things to try**, the try that got to this save from the one before has failed too: that save's
  next try, down to two saves under the one the first failure was from (`mPopFloor`; the saves a try takes on
  its own way are gone back over freely). If nothing gets through unhurt from there either, being hurt is put
  up with until he has got to two places he had not saved at before (`mTolerant`); then only dying fails a
  try. So do 6,000 frames of failed tries (or 600 tries) without a save somewhere new; hurt put up with,
  60,000 more frames of tries that die is stuck.
- **Somewhere new** is a place no save of the run was taken at (`mSavePlaces`: all of them; with only the
  last dozen kept, ground gone over again from a save further back counted as new and the tries never ran
  out). A long while with no try failed is not it: a tactic that waits out a Slig's beat before each go
  waited 1,700 frames each time and so went at one screen for 50,000 frames.
- **The bot's memory is put back with the save** (`BotMemory`, a base of `Bot`: what it has seen of the world,
  where it has been, what it found to work and not, the edges ruled out; and its dice, `Saved::mRng`). A try
  that is taken back did not happen. Without this, what a failed try had seen stayed believed: a gate it had
  opened was planned through, a path it had died in counted as been in, and both trials ended with no plan.
  What is learnt from failing as such (throws that miss, a drill's timing) is not in it and stays.
- **A replay is the run** (checked 2026-11-15 with `build-bot/la/rp_play.txt`, `rp_replay.txt`, `rp_cmp.py`):
  the Barracks from its first screen with 102 tries, its buttons played back with no bot, gives the same
  world (`hash_log`) on each of the 17,613 frames. That covers saves taken hanging and with Sligs awake.

Measured (2026-11-16, Debug, headless, `build-bot/la/*l.txt` are the standing runs with `look`; a run's
frame count includes the frames a try held back, not the tries taken back):
| Run | Without `look` | With `look` |
|---|---|---|
| AO Rupture Farms `a1` | 14 deaths, Stockyards at f=5,567 | through it to the Stockyards with no death: 392 tries, hurt put up with once |
| Scrab trial `tr1` | 0 deaths, hurt by Fleeches | through it, the next vault, Necrum and Feeco and into the Barracks (f=92,289) with no death: 668 tries, hurt put up with 8 times |
| Barracks `b1` | dies at f=3,195 on its fifth screen | from its first screen through path 1's door into the boom machine's rooms (f=22,128), no death: 640 tries, hurt put up with 5 times |
| Mudomo trial `t13` | 0 deaths | (not run again with this order: 20 frames a second in its Fleech crowd; with the moments between tried before planning anew it got through and on with 424 tries) |
With planning anew tried after the moments between, AO stuck in its path 18 (hurt is dead there, and the moments
alone do not get him through); tried before them it is through with a third of the tries.

Not good yet:
- A try that fails takes a second or two of game time, and a run with hundreds of them takes minutes: `look`
  is still off by default.
- With hurt put up with it goes back up to eight saves, and what it then plays over can be long: the Barracks
  run above bombed its Sligs four times over.
- `Bot::TryFailed` and what hangs off it have no unit tests: only `Lookahead` has (mutation checked).

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

## Slogs that are after him (2026-11-18): Terminal 4 done

FeeCo lobby -> path 9 -> Terminal 4 -> Bonewerkz path 1 with 0 deaths before Bonewerkz (`build-bot/la/fc3.txt`).
What the game does (Slog.cpp), which the earlier pen rules had wrong:

- A Slog that waits where the map put it (Brain_1_Idle) takes no notice of bones. It is set on him by him within
  100 px on its floor, a noise, or his voice on its screen (`DrawSlogsTask` says "Hello" from a ledge over it).
- One that is after him (Brain_2_ChasingAbe, `SlogAt::mChasing`, read through `Slog::ChasingAbe()`) follows him along
  its floor off its own screen, and goes for a bone on the screen shown wherever it stands itself
  (`Slog::FindBone`: `RockAt::mShown`). A bone on another screen is none to it.
- The bone is gone its eating time after the first Slog is at it. Under a ledge 50 or more over it a Slog only leaps.

What is built on that:

- `World::SlogsOut(pen)`: a pen's Slogs are all after him. Its gate is then open while a bone is shown on that floor.
- A pen come onto along the floor only (a tunnel): `Draw the Slogs out of ...` then `Throw a bone away from ...`
  (`FeedAwayTask`: two bones as far from the way in as they go on his screen, down to the cell by the way in while
  they eat the first, on as they start on the last. One bone buys ~150 frames and the dash wants ~140; and once he
  is off the screen only the bone they are at counts.)
- The rest of a Slog floor, a screen at a time (`World::SlogFloor`, eDanger with mA 2): shut to what comes onto it
  along the floor while any Slog is after him and no bone is shown. Down onto it from a ledge is not gated.
- A throw into a pen whose Slogs are out is budgeted at three bones (or what he has); a throw in a plan is moved
  to just before the step that wants it; `BoneThrow` allows for the bone's roll (100 px from over the floor, 220
  along his own floor) so that it does not end off the screen.
- In a hurry, crossings that follow a one-cell walk in the plan are gone through in one (a run wants a few cells).

Owed: unit tests for all of the above. Not re-run on this: `b14.txt`, `wr.txt`, `blk9t.txt`, `blk10.txt`.

## Bonewerkz, path 1 (2026-11-18): done, into path 7 where the Annexes start

FeeCo lobby -> path 9 -> Terminal 4 -> Bonewerkz path 1 -> path 7 with 0 deaths (`build-bot/la/fc3.txt`; path 1 alone:
`bw1.txt`). What it took:

- AE's falling things (FallingItem: its switch, its interval, then down at up to 20 px a frame, over and over):
  `RockAt::mFallIn`, and `GoToTask` goes under one only when he is past before it is down to his head, which is just
  after it has smashed. AO's were waited for already.
- A running jump up a hoist three cells on from a stand (measured: from 1187,980 on the platform at 1262,900 twenty
  frames on). `NavGraph::AddRunJumps` adds it where the floor goes on behind him but is no run-up (low, hazard, a
  wall): cost `kStandJumpCost`. It is cheap only as a way off the floor of a Slig that woke (`Bot::MakeFlee`), is not
  made cheap by a Scrab or a timed run, and is not the crossing a plan names where there is another. Each of
  those was a regression first: the Fleech rooms' tests, the Scrab trial (tr1), Terminal 4.
- A hop down to the level of a Slig that looks one way and the other: made as it turns its back (`GoToTask`, eHop);
  which way it looks is no longer what shuts that hop in `EdgeCostFn` (it had him walk to and fro between two ways).
- The hop from a cell back that comes down a cell nearer, onto a lift's stop; a lift he stands on is a way off a
  Slig's floor; of crossings that were alike the plan names the one that does not start beside a Slig that is
  awake, and of hops the one that ends on a lift's stop.

Owed: unit tests for all of this and for the Slog rules above. Not re-run on this: `b14.txt`, `wr.txt`, `blk9t.txt`,
`blk10.txt`.

Next: path 7 (Annex 1). Two levers on a MultiSwitchController that counts them only when both are on within its
delay (it sets them back otherwise); alone he is ~100 frames between them. The game's way: a Mudokon that listens
pulls a lever a cell from it at "Work" (Mudokon.cpp, Brain_ListeningToAbe_State_11). `WorkTogetherTask` does this for
wheels, where following Mudokons stop by themselves; at a lever one has to be put on the cell and told to wait.

### Annex 1 so far (2026-11-18, in progress)

- `Domain::PlanTogether` and `WorkTogetherTask::Levers`: levers on a MultiSwitchController whose delay
  (`Entity::mState`) is 90 frames or less are for Mudokons as wheels are. One is recruited from the levers' floor on
  its screen (`RecruitTask::SetRemote`), led to the cell its lever is pulled from (he goes a cell past it), told to
  wait; then Abe's own lever with "Work" said as he pulls.
- Electric walls that are off across a floor a Slig of the map walks: `ElectricWall sw.. gets Slig#..`
  (`ZapWaitTask`), and nothing is crossed onto that floor until they have. With this the plan is the levers, not the
  ledge past the two Sligs.
- The Mudokon follows him by where he is along its own floor: on a floor over his it is first brought to where
  that floor lets it down, by his going past under there. (gdb: it was shot at 1012,1141, on the Sligs' ledge over
  him. The trace gave only its x.)
- `Bot::Replan`: walls that are on across the floor of a Slig whose beat goes through them are waited for before
  anything else ("Let the walls get the Sligs"). Only a beat seen to go through: FeeCo's path 9 has a wall on
  beside Sligs that stop short of it.
- A Mudokon is rescued through a portal in another region by a way it walks, with what shuts that way as the
  action's conditions (the trap door the levers shut); a led Mudokon is planned from where it stands.
- A timer trigger toggles its outputs (one that is on goes off), and a switch logic that toggles turns its output
  back when one action sets all its inputs off at once: the rescue switch -> the two levers off -> the walls off.

With these the first part of the annex is one plan and runs with 0 deaths (`build-bot/la/bw7.txt`, camera 5): the
levers together, both Sligs got, the Mudokon rescued, the ledge lever (128), through the gate at 1450.

Next: past the gate. A Slog asleep at 1550,1140 wakes as he runs under it (8 deaths). The guide: "Walk past there,
sneaking to the next screen. Sneak as close to the Slurgs as you can get, then start running. Two screens later,
jump and pull up to the ledge. Hit the bone sack ... throw it so that the Slog gets crushed by the falling boxes."

Since (2026-11-18): the annex's first part is one plan from the start. For that an action's effects through timer
triggers and toggling logics are worked out in the state the action itself wants (`stateOf` in `Domain::Build`'s
`add`), not the state now. The floor of Sligs electric walls could get is dear with them about (`kZapFloorCost`),
not shut: the Barracks' trophy room has such walls and no way to turn them on. Unit tests (mutation-checked, 14 of
14 caught): BotSlogs.*, BotDomain.ATimerTrigger..., .Levers..., .ElectricWalls..., BotNav.ARunningJump.... Not under
test yet: the falling things' timing, the hop as a Slig turns its back, the bone's roll, the draw-out and
throw-away actions. Barracks re-run on this: b14 0 deaths, wr 0, blk9t 0, blk10 2 deaths (the workers' mine hop,
and a grenade at Slig#1017 at f=19982 not looked at) and not stuck.

### Annex 1, past the gate (2026-11-18): to the ledge at 2412

- A Slog the map puts asleep that wakes at the first noise (its TLV's wake-up anger 2 or less;
  `World::mSlogsAsleepAt`): its screen is crossed at a sneak, three cells at a time with a look between (what is on
  the next screen is not seen from this one). Heavier sleepers are run past as before: the Mines and the trials.
  That three-cell rule is for a sneak on a Slog's account only: put on every sneak it broke m1 and tr1.
- Slurgs on the way of such a sneak: to the cell short of the first, and from there a run. (The level: five
  Slurgs whose switch 109 is the Slog's anger switch. The chase is not to be had out of.)
- `Bot::SlogRefuge`: a running jump that catches the ledge over a hoist, away from the Slog, is a way off its
  floor. He is on the ledge at 2412,1141 with the Slog some 150 behind.

- A Slog that is after him loose on a floor with no stretch of Slogs' own (`Key::kSlogLoose`, `Domain::Build`):
  nothing comes down onto that floor while it is. Only such a floor: on one with a stretch the bones open and shut
  it (FeeCo's path 9 was stuck on this otherwise).
- `Throw a bone under the falling thing at ..` (`StepKind::eBaitSlog`, `BaitSlogTask`): where a thing falls over and
  over onto that floor, from a cell 60 or more clear of it (he was killed throwing from 38). The plan gets the
  bone from the sack first by itself. The Slog is crushed on the first try.

With these the annex's own way runs from its start to its exit at 1587,921 with 0 deaths (`bw7.txt`). Its two
secret areas are not gone into. Next: the path after it (12 deaths about 3048..3195,440, then stuck at 3188,320).
Not under test: this section's rules, and the bait.

### Annex 2 (2026-11-18): Bonewerkz' path 1 from 3188,320 to the door at 3712,340, 0 deaths

Run by `bw7.txt` going on out of the first annex. Two things:

- The Slog asleep at 3300,440 has a wake-up anger of 10: `kLightSleeper` is 10 now (was 2). The Mines and the
  trials' sleepers are heavier and still run past (m1, tr1, t13 unchanged).
- Sligs in the background whose beat takes them off the screen are never watched for 700 frames at a stretch, so
  their round is never known and he sat in the cover at 3437 for 7,600 frames. After `kZWatchMost` (1,500) frames
  in a cover he goes by what he sees (`GoToTask::ZWatched`, `ZMove`'s no-round branch). He was hit once on the
  way to the door and lived.

Unit tests added (BotSlogs.*): the bone's roll, the draw-out and throw-away actions, the loose Slog and the bait.
Mutation check 9 of 10: the one not caught is `kFallerClear`, which the test room's geometry cannot reach. Not
under unit test, as they want the bot running: the falling things' timing, the hop as a Slig turns its back, the
sneak and the run from Slurgs, the ledge as a refuge from a Slog, the watch limit.

Next: Annex 3 (path 2): 4 deaths at the mines (Mine#2010) and the mine car (#3001). The guide: "Hop between the
first two mines from a space behind the first one, and again over the second. Walk right up to the second pair
and hop over them. Continue right to the mine car. Create some mayhem as you go up, climb out and go through the
door at the other end."
