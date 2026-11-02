# Chapter maps 1-13

**Owner decision, final (2026-10-07):** every chapter has its own map; Chapter 1 is Neo Tokyo. All maps follow the BB style (the
anime look). They must be unique, vibrant and dynamic to play, with areas to **jump up to and climb**, places to **swim**, and
ways to **move around levels and between sections**. This round is greyboxing only.

The owner's brief for each map is in `HANDOFF.md` ("Chapter maps 1-13") and is copied word for word below. **The greybox layout
under each brief is the assistant's reading (PROPOSED).** The owner can change any of it. Sizes, counts, section placement and
traversal placement are greybox placeholders.

## How to build, open and check
- Build: `Demo/BoldInBeautyDemo/Tools/build_greybox.sh` builds every greybox map. Set `BIB_GREYBOX_ONLY=chapters` for chapters
  2-13 only, or give a list such as `BIB_GREYBOX_ONLY=ch03,ch11`. The code is in `Tools/greybox_chapters.py` and the helpers in
  `greybox_common.py`. The maps go to `/Game/Greybox/Maps/L_ChNN_*`. They are generated and git-ignored, so edit the scripts,
  not the maps.
- Check: run `UnrealEditor <uproject> -ExecCmds="py <abs path>/Tools/verify_chapters.py"`. The log goes to
  `Saved/VerifyChapters.log` and the screenshots to `Saved/Greybox/`. Set `BIB_CHAPTERS=3,11` to check only some maps.
- Play: open a map and press Play. You get a free-fly pawn, the same as in Neo Tokyo and the Ark. These maps have no walking,
  climbing or swimming character yet (OPEN).

## Shared greybox language (all maps)
| Look | Meaning | Actor tag |
| --- | --- | --- |
| orange | can be climbed: walls with a hand ledge every 4 m, pipes, chain links, vines | `Climbable` |
| teal | can be jumped up onto: rises of 80 cm (the demo jump clears about 90 cm, `BIBTuning::BaseJumpZ`) | `JumpUp` |
| blue surface | swimmable water: a water `PhysicsVolume` that switches characters to swimming | `Swim`, `WaterVolume` |
| white ring / frame | edge of a combat section (radius about 45 m, MAIN section 7: about 8 s from centre to wall) and the section gate | `Section`, `Section_N`, `BossSection`, `SectionGate` |
| moving block | `ABIBGreyboxMover` (new, `BIBGame/BIBGreyboxMover.*`): moves back and forth, spins or swings. Used for swinging bridges, lifts, waves, conveyor cubes, presses, robot arms, drifting steps and the lighthouse beam | `Mover` |

Every map has **four sections**; section 4 is the boss section (MAIN: 2-5 sections per level, a boss at each chapter's end). The
sections are joined by routes that use that chapter's way of moving. Chapter 11 is the exception: no sections, no combat. Every
map has the anime volume, except Chapter 11. Rain, wind, white noise and the voice line are tagged placeholders: no FX or audio
assets exist yet. (A separate weather session exists in worktree `ai-agent-weather`. Hooking these maps to it is OPEN.)

## Detail level (owner, 2026-10-07: "more fleshed out, higher detail", 8,000-12,000 pieces minimum, Neo Tokyo too)
Every map gets a detail pass (`Tools/greybox_detail.py`) built from multi-part props (`Tools/greybox_props.py`). Each prop is
made of several shapes: cars, trucks, lamps, benches, bins, crate stacks, barrels, trees, palms, bushes, flowers, rubble, rebar,
rocks, sandbags, barriers, traffic lights, vending machines, AC units, water tanks, antennas, power poles and wires, market
stalls, boats, desk sets, filing cabinets, plants, server racks, consoles, tanks, pipes, angels (body, wings, halo) and window
grids. The build warns when a map has fewer than 8,000 pieces, and `verify_chapters.py` / `verify_greybox.py` fail it. Counts as
of the 2026-10-07 build:

| Map | Pieces | Map | Pieces |
| --- | --- | --- | --- |
| L_NeoTokyo | 15,687 | L_Ch08_Intersection | 9,576 |
| L_Ch02_Ruins | 9,244 | L_Ch09_RemnantRemains | 9,228 |
| L_Ch03_Docks | 8,688 | L_Ch10_Hallucinations | 8,675 |
| L_Ch04_HangingCity | 8,967 | L_Ch11_ReturnToNowhere | 9,943 |
| L_Ch05_ArcTech | 8,934 | L_Ch12_Paradise | 9,190 |
| L_Ch06_MoonKingdom | 8,982 | L_Ch13_SecretiveSoul | 9,042 |
| L_Ch07_TOMBS | 9,920 | | |

What each pass adds (all PROPOSED):
- **Neo Tokyo:** clutter on every rooftop (tanks, AC units, antennas, roof rails); traffic lights on every crossing corner; shop
  awnings and vending machines on every building face; benches, bins, bollards and parked cars along every street; power poles
  with sagging cables down the alleys.
- **Ruins:** a broken road grid; about 60 more ruined buildings, each with a facade climb route and a rubble skirt; wrecked
  cars; broken lamps; rubble, rebar, dead trees and barricades. At the sections: classroom desks and the schoolyard fence, wrecked
  fire trucks, police cars and barriers, hospital beds and an ambulance, and stadium seats.
- **Docks:** quay stones and cracked paving; planks and posts on every pier; bollards and rope coils; shore and sea rocks, sea
  stacks you can climb, and kelp; driftwood; washed-up crates and barrels; wrecked boats; broken lamps and nets; tide pools.
  Lighthouse detail: stone bands, windows, door, gallery rail, lamp-room mullions, roof, plinth. More waves. Still no other
  structure.
- **Hanging City:** ring roads and avenues; about 70 more buildings with climb routes and rooftop clutter; lamps, benches, cars
  and crates. A rim railing with 520 hanging lanterns. Underside girders, rings, hanging pipes and lights. Chain collars. In the
  districts: crates, barrels, lamps and laundry lines.
- **A.R.C TECH:** columns, roof trusses, wall ribs and panels; overhead pipes with hangers, ducts, cable trays and risers;
  storage racks of glowing cubes; tanks; 250 unmanned consoles; cross feeders; cubes on every belt; catwalk grating and
  rails; floor stripes and bollards; autonomous forklifts, 80 drones and four gantry cranes (all moving). No workers.
- **Moon Kingdom:** overgrown stone streets with moss; about 95 more overgrown buildings; a palace dome and a civic spire (the
  "second arc" landmarks); dead lamps; giant trees, roots, bushes and flowers; benches; market stalls; mossy cars; fountains.
- **T.O.M.B.S:** on every floor: columns, ceiling lights, about 60 desk sets (desk, chair, monitor, keyboard), filing cabinets,
  plants, whiteboards, water coolers and glass meeting rooms. Lobby turnstiles. A window grid and fins on the facade. Plaza
  pillars, lamps, benches and planters.
- **Intersection:** tunnel ribs; tracks (rails on ties) down both tunnels; cable trays, emergency lights, signs, panel seams,
  side doors, drains, ceiling pipes and fans in the roof grates; abandoned cars; debris and floating debris; puddles; **950
  angels** with wings and halos (a third of them gliding).
- **Remnant Remains:** facade panels on the data centre (some lit); roof units and antennas; cooling-tower rings; a perimeter
  fence; floodlights; pylons with wires along the cables; half-buried server racks; rocks, extra dunes, puddles and wind-blown
  sheets; server racks along the hall walls.
- **Hallucinations:** checker tiles on the islands; 250 islets; 420 floating everyday objects (books, doors, clocks, teacups,
  chairs, umbrellas); clouds; giant flowers; rainbow arches; more impossible stairs; balloons (some drifting); picture frames;
  lollipop trees; more apparition placeholders.
- **Return to Nowhere:** every piece is forced to the one flat grey. Each leg's pasted map is now a dense chunk: a tower block
  with cars and traffic lights, a rubble field, pier planks and rocks, chain columns and lanterns, dead conveyors with cubes and
  tanks, houses and trees, an office floor of 140 desks, a tunnel with ribs and grey unlit angel shapes, dunes and racks,
  floating islets and objects. The path has kerbs. There is still no colour, no light, no life and no combat; nothing happens.
- **Paradise:** neon signs, billboards and climb routes on every building (the Neo Tokyo facade dressing), rooftop units,
  awnings in bright colours, 480 palms, beach umbrellas and loungers, market stalls, lamps, benches, flowers and bushes, boats
  in the canals (some cruising), fountains.
- **A Secretive Soul:** about 20 more houses; every house gets an engawa porch, posts, a shoji door, windows, a plot fence and
  garden stones. Country roads with utility poles and wires; stone lanterns (a ring around the monolith); roadside shrines;
  vending machines; a bus stop; scarecrows; wells; rice plants in rows; bamboo fences; two bamboo groves; forest; a small
  cemetery; stepping stones; 200 more red glyphs on the monolith.

## Second detail pass: where the player fights and travels (owner, 2026-10-07: "continue greyboxing all 13 chapters + the arc with more details")
`Tools/greybox_detail2.py` runs after each map's own builder and first detail pass. The first pass had filled the maps around the
fights; the combat sections themselves held only their marker ring and a few traversal pieces (about 29 actors each).

| Where | What is added (all PROPOSED) |
| --- | --- |
| Every combat section, chapters 2-10, 12, 13 | 7-9 cover walls, 6 pillars, 2-3 raised platforms (teal jump-up steps on one side, an orange climb face on the other, railings; none on the T.O.M.B.S office floors), 12-16 themed props, about 46 ground tiles and 8 lights in the chapter's palette. The middle stays open for the fight and the boss. |
| Between consecutive sections | floor markers every 7 m in the chapter's light colour, and a two-tier lookout tower beside the route (jump up 3.2 m, climb 3.2 m) to look over the next section. |
| Chapter 1, Neo Tokyo | 66 fire escapes up street facades (orange landings and stairs: climbable), 50 vertical neon signs, balconies (teal: jump onto) with AC units, about 200 pavement props (bins, crates, vending machines, bikes, A-boards). |
| The Ark | town square: benches, planters with trees, kiosks, bins, lamps, holo-ad pillars, vending machines; operators' platform: supply crates, lockers, lamps, roll-call lines; the Underworld: barrels, crates, scrap, dim neon lamps, hanging wires; the civilian boulevard: lamps, benches, flower beds, fountains. Station rings, the factory, Merchant Row and the other hub buildings stay clear. |
| Chapter 11 | nothing: the owner wants "nothing in it". |

Themes (props by kind of place): Ruins = rubble, wrecks, sandbags, rebar, barriers; Docks = crates, barrels, bollards, nets; Hanging
City = benches, bins, vending, stalls; A.R.C TECH = hazard crates, consoles, tanks, pallets of glowing yellow cubes; Moon Kingdom =
flora, trees, stone benches; T.O.M.B.S = cubicles with desks, cabinets, plants, server racks; Intersection = wrecked cars, jersey
barriers, hazard crates; Remnant Remains = rocks, sand drifts, server racks, cables; Hallucinations = pastel toy stacks, cones,
tilted cubes and a few horror placeholders; Paradise = palms, bright benches, stalls, flower beds; A Secretive Soul = stone
lanterns, bamboo fences, rocks, moss. Every piece is placed on the floor really there (an editor line trace), so nothing floats
over the Hanging City's void or sinks into water. Totals after the pass (2026-10-07): Neo Tokyo 18,191 actors, the Ark 13,348,
chapters 9,387-10,632.

## 1. Neo Tokyo: `L_NeoTokyo` (existing)
Brief: many buildings and streets, heavy rain and fog, travel by climbing buildings and running the streets. Built on 2026-10-07
(`reports/demo/2026-10-07-greybox-neotokyo-ark.md`): climb routes, rooftop bridges, neon placeholders. This round kept its layout and
added the detail pass (15,687 pieces).

## 2. The Ruins: `L_Ch02_Ruins`
Brief: *based on the Outer World, on top of the arc.*
Greybox: the surface above the Ark. The start is a pad of **16 surface hatches**, the same count as the Ark task room's chutes
that lead to the surface. The Outer World's building types stand in ruins (sunk, tilted, rubble-coloured): **School**,
**Police + Fire station**, **Hospital**, and the **Gym** as a broken stadium bowl with climbable stands (boss). Routes: teal rubble
mounds to jump up, broken facades to climb, and a **flooded crater** with a **toppled tower** lying across it (walk the tower or
swim the crater). Dusty light and fog.

## 3. Docks: `L_Ch03_Docks`
Brief: *a semi-ruined area near the sea, waves from the sea crashing, heavy fog; the only structure is a lighthouse.*
Greybox: a broken quay with slabs tipping into the sea. Three broken piers have missing planks (gaps to jump) and collapsed parts
that slope into the water. **Stepping rocks** cross the sea (jump, or swim the whole sea). The **lighthouse island** is the boss
section. The lighthouse has an outside **spiral stair** of 84 steps to a gallery and a **rotating beam**. **Waves** roll in and
crash against the quay (movers, visual only). Very heavy fog. Nothing else is built.

## 4. The Hanging City: `L_Ch04_HangingCity`
Brief: *a city hanging above an endless void, seemingly held up by a single massive chain that ascends into the sky; the top
cannot be seen.*
Greybox: a 400 m city disc with buildings and an underside cone. **One chain** (150 climbable links, about 3 km tall) disappears
into the fog. The boss section is at the chain anchor. **Four hanging districts** hang on cables below the rim, joined by
**swinging plank bridges** (jump from plank to plank). A **lift** runs from each district up to the rim, beside a climb wall.
There is no ground anywhere: falling means death (kill Z).

## 5. A.R.C TECH INCORPORATED: `L_Ch05_ArcTech`
Brief: *a massive factory, fully functional, seemingly undamaged, running fully automatically with no visible workers at all;
it seems to be making tiny cubes that glow insanely bright yellow.*
Greybox: a 600 m x 300 m hall with "A.R.C TECH INCORPORATED" over the open intake. It has four **conveyor lines** carrying
glowing **30 cm cubes**, **robot arms** that swing, a **press hall** with ten stampers to time a run under, **overhead catwalks**
reached by ladders, and **coolant pools** to swim. Pallets of finished cubes stand around. The **cube core** is a glass pillar
full of cubes behind the boss section. There are no people anywhere. Cube emissive is 200 (PROPOSED "insanely bright").

## 6. The Moon Kingdom: `L_Ch06_MoonKingdom`
Brief: *seemingly a second arc, built like an actual liveable arc with all the structures you would expect, yet not a single
living creature, everything covered in flora, and a massive Moon inside that casts a deep darkness over the entire "Arc".*
Greybox: a cavern like the Ark, with a town of houses and towers. Every roof is covered in flora and has climbable **vines**.
There is a **gate plaza**, a **market**, and **residential terraces** that you jump up to. A 240 m dim **Moon** hangs in the
middle above a **lake** (swim). Lily-pad stepping stones lead to the island under the Moon (boss). Lighting is very dark.

## 7. T.O.M.B.S (The Old Man's Battings Site): `L_Ch07_TOMBS`
Brief: *a massive structure that seems larger than any man-made structure; inside it just looks like a regular office, yet holds
many "Angel"-based secrets.*
Greybox: a 600 m x 600 m x 2.5 km dark shell with one door and "T.O.M.B.S" above it. Inside is an ordinary **ten-floor office**
with a lobby, reception, cubicles (desks to vault over) and an **atrium** with jump-up stacks from floor to floor. There are
**elevator shafts** to climb. Sections are on the lobby and floors 3, 6 and 9 (boss). Five **secret rooms** sit behind walls that
look like plain office walls (non-colliding `SecretDoor` placeholders), each holding a glowing **Angel secret** placeholder.

## 8. Intersection: `L_Ch08_Intersection`
Brief: *a simple tunnel drenched in rain and seemingly flooding, brimming with angels everywhere, as it is the main transport
path the angels take from their assumed headquarters.*
Greybox: a 1.5 km tunnel with a **cross tunnel** (the intersection) and rain pouring through roof grates. The **flood rises**
along the tunnel: ankle deep, then waist deep, then 4 m (swim). The floor drops into a fully flooded **pump room**. Side
**maintenance ledges** (jump up) and **overhead pipes** (climb) keep you above the water. **160 angels** stand or fly along the
tunnel (white, glowing). The last section heads toward the angels' assumed headquarters (boss).

## 9. Remnant Remains: `L_Ch09_RemnantRemains`
Brief: *an empty desert with violent winds and constant downpour; in the middle a massive data centre / electrical cyber thing
(assistant's choice) hosting a small screen.*
Greybox (choice: a **data centre**): dunes, with downpour pools in the hollows (swim). Rain and wind are placeholder volumes.
Eight **cable trunks** run out from the centre (climb along them), with fallen server racks to jump. The centre is a tiered
data-centre block with four **cooling towers** (climb). Inside, a **server hall** (boss) holds the **small screen** with the
owner's text exactly as written:
`Angel//V1 Success Line down Operator//V! Faillllllllllllllllll ...---... ////// .- -.--. . .-.. ... / .-- .. .-.. .-.. / -.- .. .-.. .-.. / ..- ... / .- .-.. .-..`

## 10. Hallucinations: `L_Ch10_Hallucinations`
Brief: *the whole chapter is a hallucination: a nonsensical pastel dreamscape with a drawing look, juxtaposed with hyper-realistic
terrifying things appearing at random, and Morse messages telling a small story.*
Greybox: four pastel **floating islands** over a pastel sea (swim). Floating stairs join them, and some steps **drift** up and
down. Impossible stairs float in the sky. **Giant everyday objects** stand on the islands: a chair to climb, a teacup and a
melting clock. **24 apparition spawn points** mark where the hyper-realistic horrors appear, with a few placed placeholders. One
**Morse message** on each island (in game) tells the owner's story in order:
1. "The angels bring mercy for all"
2. "Not those stupid pure ones you call angels"
3. "but TRUE ANGELS THE ONE"
4. "The rest are all filth compared to its existence"
The pastel drawing look would be a per-map tune of the anime material (OPEN).

## 11. Return to Nowhere: `L_Ch11_ReturnToNowhere`
Brief: *nowhere: a mashup of all 10 past maps pasted together; pure grey, no post-processing, nothing in it, no texture, just
pure grey and white noise; no levels, no bosses, nothing; a single walk of about 5 minutes and you walk out; nothing happens, but
it should feel horrifying the whole time.*
Greybox: one **1.4 km path** made of ten bent legs. That is about 5.2 minutes at the demo walking speed of 450 cm/s. Each leg
passes a grey piece of one earlier map, slightly wrong: Neo Tokyo towers and a crossing, the toppled tower, the lighthouse and a
pier, chain links rising out of the ground, a dead conveyor, a house under a low moon, office desks and a wall too tall to see
past, a tunnel segment, a dune and the data centre, a floating island and a giant teacup. The **exit** frame is at the end.
Everything uses one flat grey material. There is **no anime volume**. A **neutral post-process volume** turns off bloom,
vignette, grain, motion blur, lens flare, fringe and AO, and fixes exposure. There is a **white-noise ambience placeholder**. There
are no sections, enemies or bosses.

## 12. Paradise: `L_Ch12_Paradise`
Brief: *a beautiful neo-tropical city, not ruined, bright vibrant colours, constant good vibes, quite heavy rainfall.*
Greybox: bright coral, turquoise, lime, sunflower and magenta buildings, with **canals** (swim) and bridges. **Rooftop
gardens** have **waterfalls** pouring off them, climb walls lead up to them, and **sky bridges** join them. Teal **planter
steps** let you jump up. There are palms everywhere and a **lagoon with a beach** (boss). The sun is bright and the heavy-rain
volume is a placeholder.

## 13. A Secretive Soul: `L_Ch13_SecretiveSoul`
Brief: *a small Japanese-looking countryside town in consistent low to medium rain; in the middle a pure black monolith with red
hieroglyphics in an undecipherable language; when you get close the voice of the angel god speaks.*
Greybox: a valley ringed by hills and forest, with **rice paddies** (shallow water) and a **river** (swim) crossed by wooden
bridges. There is a village street of wooden houses with stone walls to jump, a red **torii**, and a **shrine** at the top of a
50-step stair (or climb the hillside). In the middle stands the **monolith**: 50 m tall, pure black, with 60 red glyph
placeholders. A **trigger box** (50 m) around it is tagged `AngelGodVoice` for the line, which is kept exactly as the owner wrote
it: "They wree purity, Impure sin, Thats all". The rain volume is a placeholder. The boss section surrounds the monolith.

## Elite_Bundle dressing (owner, 2026-10-08)
Owner: "Here's a bundle with 100s of basically random assets to fill up the 13 main chapter levels and arc with those assets".
`Tools/greybox_bundle.py` runs last in every map build (Neo Tokyo, chapters 2-13, the Ark). Of the bundle's 376 static meshes,
299 are used (Dekogon Dockhouse, Medieval Kitchen, Medieval VOL9, Victorian, Anemoia; Meshingun Gothic Cemetery; Scansfactory
Cemetery, Ruined Crypt, Urban Props); 77 are left out (glass planes, roof / frame pieces, backdrops, whole rooms, tiny tools).
Several packs in the bundle have meshes without their materials or textures (92 meshes): those slots get the nearest palette
colour. Many folders hold only materials (Feudal Japan, Egypt, Boneyard, Foliage Sets, Stone Decor ...), so they add nothing.
Themes (assistant's reading, PROPOSED): 1 Neo Tokyo urban / crates / street food on pavements and rooftops; 2 Ruins graves, statues,
dead trees, stone arches; 3 Docks the dockhouse set (pier, boat stand, crates, ropes, tarps); 4 Hanging City manor furniture and
kitchen goods; 5 A.R.C TECH storage units, power boxes, spools, pallets; 6 Moon Kingdom heavy flora over kitchen and manor
furniture; 7 T.O.M.B.S bookshelves, cabinets, books, pictures; 8 Intersection crates, pallets, planks, power boxes; 9 Remnant Remains
storage, wrecks (carriage, wheel), dead trees; 10 Hallucinations everything, pastel (grave scans kept realistic as the "terrifying
things"); 11 Return to Nowhere each leg gets the props of the map it echoes, all flat grey; 12 Paradise market food, flora, boats;
13 A Secretive Soul baskets, buckets, firewood, trees and ferns. The Ark: Merchant Row food stalls, library books, training / arcade /
factory storage, gang hall, town square, Underworld crates, civilian flora and fountains. Station rings and fight centres stay clear.

## OPEN (for the owner)
- A walking, climbing and swimming character for these maps (the demo's operators have no climb). How the maps join the 104
  campaign stages. The demo's chapters 2-13 still run as code-built stages in `L_Hub`.
- Whether chapter 13 has a boss like the other chapters. The greybox gives it one (MAIN, 2026-10-05: every chapter ends with a
  boss).
- The Chapter 9 screen Morse: in standard Morse `-.--.` is "(", so the line reads "A(ELS WILL KILL US ALL". "ANGELS" would be
  `.- -. --. . .-.. ...`. The text is kept exactly as written. Change it only if the owner asks.
- The Chapter 13 line "They wree purity" is kept as written (typo or intended?).
- Real rain, wind and waves FX, the white-noise and voice audio, the pastel look for Chapter 10, and the horror apparition models.
- The Chapter 11 tone is placeholder. "White noise" is read as audio; a visual static overlay is possible on request.
