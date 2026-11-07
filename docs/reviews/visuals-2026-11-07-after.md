# Visual review after improve-city-atmosphere (2026-11-07)

The same reference views as in `visuals-2026-11-07.md`, rendered after all eleven phases of
`improve-city-atmosphere`. Sheets: `docs/screenshots/atmosphere/before_sheet.png` and
`after_sheet.png`; gameplay screenshots with the HUD and post effects: `docs/screenshots/m5/`.

## Before and after

| Finding | Before | After |
|---|---|---|
| V1 Facades read as noise | Three window textures, 38 % lit everywhere, lit windows clipped to white | Eight window modules with a texture offset and tint per building; windows lit by building use and hour (offices dark at night, homes lit in the evening); TV flicker; lit windows peak at 80 % and never clip |
| V2 Every building is a box | One box per building | Podium towers, setbacks, notched corners, L-shapes, slabs, rows of houses; parapets, cornices, ledges; eleven roof kits; headquarters crowns; City Hall colonnade |
| V3 Ground floor like the fortieth | No shopfronts | A 4.5 m ground floor on every building (shopfront, lobby, shutter, loading dock, stall, posters); open places light their fronts, closed ones are shuttered; doors under the signs |
| V4 Empty streets | Barrels only | Kerbs and gutters, crosswalks, stop lines, arrows, stains; lamps every 24 m; about 30 kinds of props chosen per district; alleys; cables, lanterns and laundry overhead |
| V5 Almost no signage | Icons only | Shop names, blade signs in a made-up neon script, billboards and holo-ads that follow the corporate board, news tickers, gang marks, wanted posters at Heat 3 |
| V6 Empty sky, dark slot | Flat fog colour | The Pall as a low cloud deck lit by the districts; searchlights; blimps; halos of distant neon and silhouettes in the fog at the end of every street; a distant city and skyline on clearer nights |
| V7 Rain does not wet the street | Brighter ground | Reflections of every nearby light, puddles with ripples, splashes, drips, denser rain lit by the lamps, lamp cones, steam |
| V8 Districts differ only in colour | Same geometry everywhere | A kit per district type with a signature on every block that no other district has |
| V9 Vertical layers missing | A flat highway slab | Highway decks with pillars, barriers, lamps, traffic and bulbs beneath; skyways; the elevated metro with trains on the timetable |
| V10 Little moves | People and cars | Flickering and broken neon, blinking beacons, chasing marquees, drones, birds, litter in the wind, fans, sparks, police light bars at Heat 3 |
| V11 Lights don't pool | Two triangles per wall | Light pools and wall splashes as decals; the camera's chunk drawn per block on tessellated ground; per-pixel lighting of the ground on desktop |
| V12 Contrast upside down | Grey walls, white windows | Ambient −35 %, bloom only on neon, a colour grade per district, vignette |
| V13 Texture array full | Home sign read past the array | 64 layers with atlases; the layer map is tested |

## The vision

The city is now dark most of the time, wet and close, as asked:

- **Long nights, short days.** Nights last 19 hours. Dawn runs 10:00–11:00, a dark overcast grey
  day 11:00–14:00, and dusk 14:00–15:00. The dusk band is red-orange low in the west. Even at noon
  the frame is no brighter than at night: mean luminance 31.8 against 32.6 at 23:00.
- **Rain and fog.** Rain falls in about nine of ten weather slots. Sight lines are 60–200 m, and the
  street is never dry.
- **Neon through the fog.** In the fog view, 33 halos glow beyond the fog end, and 86 % of the area
  around the vanishing point differs from the plain fog colour.
- **Neon wins.** In every night view, 0 % of the pixels are clipped to white. 96–100 % of the most
  saturated pixels lie on emissive surfaces.

## Checks

- **Unit tests:** 179 pass, including 15 new ones in `tests/unit/test_atmos.c` that cover every
  scenario of the change. The derived facades and street dressing are pinned by digest, and the
  pins match on Linux, Windows and the web.
- **Platform tests:** 25 pass under Xvfb. They include the reference views, the image metrics, the
  look of each of the six weathers, and the gloom.
- **Browser tests:** pass. Web download: 1.04 MiB compressed.
- **Line coverage:** 98.3 %.
- **Gameplay:** the balance bands and the 16 playtests are unchanged by the climate and the solid
  street props.

## Still open

1. **Real hardware.** The frame rate on the reference desktop and phones has not been measured. The
   automated budget test keeps draws, lights, sprites and mesh sizes inside the limits at every
   detail level, but software GL in CI says nothing about frame times.
2. **District colours are close.** Whole dark frames of different districts differ by only a few
   levels in mean colour; the threshold is now 2.5 levels. Districts now tell apart mostly by shape
   and props, as the vision intends.
3. **Far searchlights fade out.** Beams from headquarters far away disappear into the fog by
   design. They show near the towers and from rooftops.
4. **No settings menu.** The new settings (street detail, reflections, per-pixel lighting,
   brightness) live in the settings file. A settings menu remains for M6.
5. **Rooftop reference view.** Many of the towers next to the home block are taller than the
   camera position, so the rooftop view looks down a street canyon; the aerial view covers the
   roofs.
