# Silverrock — Soundtrack: *Songs for a City Without a Sun*

Status: draft for generation, 2026-11-05. Companion to the [game design](01-game-design.md).

## 1. The artistic vision

**The sun went out forty years ago. The music remembers it.**

Silverrock's score is one long nocturne in many rooms. It is ambient and instrumental: no words, ever. The only
voice is a **wordless choir**, used sparingly, and it stands for the one thing nobody in the city has seen in
decades: daylight. When the choir appears, the music is reaching for the sun.

The score is built from three layers that mirror the city itself:

| Layer | Stands for | Sound |
|---|---|---|
| **The Ground** | the street, the poor, rent, rain, people | warm and worn: tape-saturated electric piano, detuned analogue pads, vinyl and tape hiss, rain, soft hand-played textures, things slightly out of tune |
| **The Glass** | the megacorps, money, control | cold and perfect: crystalline FM bells, glassy pads, quantised arpeggios like data, pristine sub bass, no noise, no wobble, nothing human |
| **The Memory** | the lost sun, hope, the player's will | the wordless choir and one long, bowed, string-like synth tone that slowly rises; only in rare moments, never in loops you hear for hours |

Every track sits somewhere between those layers. A slum alley is all Ground. A corporate lobby is all Glass. A
rooftop above the clouds of smog lets the Memory in.

### Leitmotifs

Four short melodic ideas recur in different disguises. Suno will not reproduce exact notes, but describing them the
same way keeps the tracks related.

- **The Sun motif:** five notes, rising slowly and resting on a long, unresolved fifth, like a question. On the title
  screen it is grand; on a rooftop it is hopeful; at the ending it finally resolves into a major chord.
- **The Rent motif:** a two-note falling figure, like a clock or a dripping pipe (tock… tick). It hides in every home
  track and turns anxious when money runs out.
- **The Grid motif:** a fast, perfectly even sixteenth-note arpeggio, the sound of the corps' data. Wherever the
  Glass is near, it is there, cold and precise.
- **The Heartbeat:** a soft, low double pulse. Missions, stealth and danger are built on it; it speeds up as you are
  noticed and stops when you flatline.

### Rules for the whole score

- **Instrumental only.** No sung words, no spoken words, no rap. The choir is always wordless ("ah", "oo"), distant
  and reverberant.
- **Night, always.** No bright major-key pop, no festival drops, no happy guitars. Even rewards are cool and
  understated.
- **Retro texture.** The game is low-poly and dithered; the music should feel the same: slightly lo-fi, tape-worn,
  a little grainy, like a cassette found in a drawer. Avoid glossy, modern, over-compressed production.
- **Built to loop.** In-game tracks have no big intro, no final hit, no fade-out story. They breathe, they evolve
  slowly, and they can repeat.
- **Families share key and tempo:** all street loops, all mission states and all Matrix states, so the game can
  crossfade between them seamlessly.

## 2. How to use these prompts with Suno

- Switch on **Instrumental**.
- **Custom mode (recommended):** paste the *Style* block into "Style of Music". The style tags come first, the
  description after them; current Suno versions read long, descriptive style prompts well. Put the title in the
  title field and leave the lyrics empty.
- **Simple mode:** paste only the first sentence of the description plus the style tags.
- If a voice with words appears anyway, generate again and end the style with `no vocals, no lyrics, no singing,
  wordless only`.
- Generate several takes and keep the one with the steadiest ending (for loops) or the clearest shape (for short
  stingers). In-game loops: 2–4 minutes. Stingers: ask for "short, 20 seconds" and trim.
- The prompts describe sounds, never other artists: artist names are unreliable and can be refused.

---

## 3. The tracks

### Part I — The Threshold

#### 1. Silverrock
*Title screen. The city's overture.*

**Style:**
`dark ambient, cyberpunk, cinematic, analog synth, wordless choir, rain, 70 bpm, D minor, melancholic, majestic, tape warmth, instrumental, no vocals.` Rain on glass and a low city drone open the piece, as if heard through a window high above an endless night skyline. A warm, slightly detuned analog synth plays a slow five-note melody that rises and stops on an open, unresolved interval, like a question asked into the dark. Deep pads swell beneath it. Halfway through, a distant wordless choir enters, singing only "ah", soft and enormous, as if the city remembered the sun for a moment. The choir lifts the melody once, then everything sinks back into rain and tape hiss. Beautiful, cold and lonely; grand but never triumphant.

#### 2. Mirror, Mirror
*Character creation.*

**Style:**
`ambient electronic, minimal, glass bells, soft arpeggio, warm pads, 80 bpm, D minor, introspective, curious, loopable, instrumental, no vocals.` A quiet room with a scanner humming. A soft, patient arpeggio repeats like a light passing over a face, and glassy bell tones pick out fragments of a rising five-note melody, never finishing it. Warm low pads breathe underneath; a faint tape hiss gives it a worn, retro feel. The mood is curious and calm: someone deciding who to be before stepping out into the rain.

#### 3. Held Breath
*Pause and settings.*

**Style:**
`drone ambient, minimal, very quiet, low pad, distant rain, slow filter movement, loopable, instrumental, no vocals.` Time stands still. A single low pad slowly opens and closes like breathing, with distant rain and the faintest electrical hum. Almost nothing happens, and that is the point: a quiet held breath that can sit under menus for minutes without asking for attention.

---

### Part II — Four Walls (home)

All home tracks hide the **Rent motif**: a soft two-note falling figure, tock… tick, like a clock or a dripping
pipe.

#### 4. Four Walls
*Inside your apartment on an ordinary night.*

**Style:**
`lo-fi ambient, warm electric piano, rain on window, vinyl crackle, muffled city, soft pads, 65 bpm, F major seventh, cozy, tired, intimate, loopable, instrumental, no vocals.` The one warm place in a cold city. Slow, tired electric piano chords with soft jazz colours, slightly wobbly like an old tape. Rain taps on the window; through thin walls you hear muffled traffic, a neighbour's TV and the hum of the building. Somewhere in the background a soft two-note falling figure, like a dripping tap, repeats quietly. No drums. It should feel like taking off wet boots after a long shift: small, safe and human.

#### 5. Rent Is Due
*At home while rent is due within two days, or debt is open.*

**Style:**
`lo-fi ambient, detuned electric piano, ticking clock, dissonant drone, rain, 65 bpm, F minor, anxious, restless, loopable, instrumental, no vocals.` The same small room, but something has gone wrong with it. The electric piano is now slightly out of tune and the chords keep sliding into minor. The two-note falling figure has become a steady tick, like a clock you cannot stop hearing at 3 a.m. A low, uneasy drone sits under everything and slowly grows. The rain is heavier. Nothing dramatic happens; it is the quiet, grinding weight of money you do not have.

#### 6. Lights Out
*Falling asleep (bed or capsule hotel).*

**Style:**
`dream ambient, slow swelling pads, distant wordless choir, gentle rain, tape warmth, 50 bpm, F major, peaceful, drifting, short, instrumental, no vocals.` Drifting off. Warm pads swell slowly and dissolve into each other like lights blurring through a rainy window. Far away, a wordless choir hums a soft "oo", as if from a dream of daylight. The rain softens. It fades into silence after about a minute.

#### 7. Still Night
*Waking up.*

**Style:**
`ambient sting, soft synth bells, rising pad, rain, 70 bpm, D minor, gentle, grey hope, short 30 seconds, instrumental, no vocals.` Eyes open, and it is still night. Three soft bell notes rise from a quiet pad, the beginning of a melody that never finishes. A little hope, a lot of grey.

#### 8. New Keys
*Moving to a better apartment.*

**Style:**
`uplifting ambient, bright synth arpeggio, warm string pad, soft bass, 85 bpm, A major, relief, progress, short 45 seconds, instrumental, no vocals.` A rare, warm moment. A bright arpeggio climbs over soft string-like pads and a gentle bass, the tired electric piano of home replaced by something clearer and more open. One step up the ladder; the music ends open rather than triumphant, because the climb is long.

#### 9. Locked Out
*Evicted: the street is your home now.*

**Style:**
`bleak ambient, cold wind, sparse sad piano, rain, distant city noise, 60 bpm, D minor, lonely, homeless, loopable, instrumental, no vocals.` The warm room is gone. Fragments of the home piano now play outside, in the wind and rain, sparse and far apart, with long silences between them. The city rumbles in the distance, indifferent. Cold, lonely and very quiet.

---

### Part III — The Districts (street loops)

Every district loop shares **75 bpm, D minor** and the same soft sub-bass pulse, so walking across a district
border is a slow crossfade, not a cut. Each district has its own character, made from the three layers.

#### 10. Glass and Gold — Corporate Spire
*All Glass.*

**Style:**
`corporate ambient, cyberpunk, crystalline FM bells, precise arpeggio, deep clean sub bass, airy high wordless choir, 75 bpm, D minor, cold, sleek, imposing, loopable, instrumental, no vocals.` Towers of glass and money, lit from inside and empty at night. Everything is perfect and nothing is warm: a fast, perfectly even arpeggio streams like stock data, crystalline bells ring with long reflections, the sub bass is clean and powerful. High above, a thin, distant wordless choir sounds like air conditioning in a cathedral of glass. No noise, no tape, no human touch: beautiful, expensive and inhuman.

#### 11. The Steps — Civic Center
*Ground under Glass.*

**Style:**
`dark ambient, organ-like synth chords, distant bells, low brass pad, light rain, 75 bpm, D minor, solemn, institutional, worn, loopable, instrumental, no vocals.` Stone steps, flags and cameras. Slow, organ-like chords move like a ceremony nobody believes in anymore. A distant bell tolls the hour; a low brass-like pad gives it a sense of official weight. Under it all, a faint data arpeggio flickers, the corps watching through the city's institutions. Grey, solemn and slightly hollow.

#### 12. Neon Bazaar — Commercial
*Ground with Glass signs.*

**Style:**
`cyberpunk ambient, layered synth arpeggios, crowd murmur texture, neon hum, soft pulsing bass, light muted percussion, 75 bpm, D minor, busy, colorful, loopable, instrumental, no vocals.` Shopfronts, holo-signs and crowds under umbrellas. Several bright arpeggios overlap like advertisements competing for attention; a warm bass pulse walks underneath; neon tubes buzz and the crowd is an indistinct murmur without words. Colourful and lively, yet still night, still wet, still a little tired.

#### 13. Last Call — Entertainment
*Ground, glowing.*

**Style:**
`nightlife ambient, muffled club beat behind walls, deep bass, sultry synth chords, glittering arpeggios, wet streets, 75 bpm half-time, D minor, hazy, seductive, loopable, instrumental, no vocals.` The district of bars, clubs and bad decisions. The beat is only ever heard through walls: a muffled kick and bass as if from the club next door, swelling when a door opens and fading again. Over it float hazy, sultry chords and glittering arpeggios like reflections in puddles. Seductive, warm and a little dangerous.

#### 14. Ten Thousand Windows — Residential
*All Ground.*

**Style:**
`ambient, soft warm pads, sparse piano, distant traffic, gentle rain, tape hiss, 75 bpm, D minor, lonely, human, calm, loopable, instrumental, no vocals.` Endless apartment blocks with a few lit windows. A sparse piano plays slow notes with lots of space, each one like a window lighting up and going dark. Warm pads, rain and faraway traffic. Somewhere inside, the quiet two-note figure of home. Thousands of small lives behind glass; lonely, but human.

#### 15. Before the Towers — Old Quarter
*Ground and Memory.*

**Style:**
`nostalgic ambient, tape-worn analog synths, reed organ pad, rain on tiled roofs, faint wordless choir, 75 bpm, D minor, faded, melancholic, warm, loopable, instrumental, no vocals.` The part of the city that existed when there was still a sun. Old, tape-worn synths wobble gently; a reed-organ-like pad sounds like a forgotten church; rain runs off tiled roofs. Very faintly, a wordless choir drifts in, like a memory of a summer afternoon. Warm and sad, slowly being demolished.

#### 16. The Foundry — Industrial
*Ground, grinding.*

**Style:**
`industrial ambient, metallic machinery rhythm, deep drone, steam hiss, dark pads, 75 bpm, D minor, heavy, grinding, relentless, loopable, instrumental, no vocals.` Factories that never stop. Machines thud and clank in a slow, heavy rhythm that becomes the beat; steam hisses; chains rattle somewhere above. A dark drone grinds underneath and dark pads rise like smoke. Workers walk past in silence. Heavy, relentless and strangely hypnotic.

#### 17. Black Water — Docks
*Ground and vastness.*

**Style:**
`dark ambient, foggy harbor, distant ship horns, slow wave-like bass swells, creaking metal, cold pads, 75 bpm, D minor, lonely, vast, loopable, instrumental, no vocals.` Containers, cranes and black water under a sky that never lightens. The bass swells and recedes like slow waves; a ship horn sounds far away in the fog; metal creaks. Cold pads stretch to the horizon. Empty, enormous and lonely.

#### 18. Clean Rooms — Research Campus
*Glass, curious.*

**Style:**
`futuristic ambient, pure sine tones, soft data blips, airy pads, subtle glitches, 75 bpm, D minor, clinical, curious, eerie, loopable, instrumental, no vocals.` Labs and manicured lawns under floodlights. Pure sine tones and soft data blips drift like instruments being calibrated. Airy, clean pads. Every so often something glitches for a moment, a hint that what is being researched here is not meant to be seen. Curious, clinical and slightly eerie.

#### 19. Under the Overpass — Undercity
*Ground, buried.*

**Style:**
`dark ambient, claustrophobic, rumbling traffic overhead, dripping water, distorted low drone, concrete echo, 75 bpm, D minor, gritty, oppressive, loopable, instrumental, no vocals.` The city's basement beneath the elevated highways. Traffic rumbles overhead like thunder that never ends; water drips into puddles; every sound echoes off concrete. A low, slightly distorted drone holds it all together. Gritty and close, but alive: people live down here.

#### 20. Burning Barrels — Barrens
*Ground, scorched.*

**Style:**
`desolate ambient, wind, crackling fire, sparse detuned plucked synth, wide empty reverb, 75 bpm, D minor, empty, lonely, harsh, loopable, instrumental, no vocals.` Empty lots at the edge of everything. Wind, the crackle of fires in oil barrels, and a few detuned, guitar-like plucks in an enormous empty space. The glowing skyline is far away; here nobody is watching, and nobody is coming.

#### 21–23. Weather layers
*Textures mixed over any district loop.*

- **Downpour:** `heavy rain ambience, rolling thunder, water rushing in gutters, no music, field recording, loopable, no vocals.` A wall of rain that swallows the city's sounds.
- **Fog:** `muffled foghorn, soft white noise, low airy drone, very quiet, loopable, no vocals.` Everything muted and far away.
- **Smog / acid-rain advisory:** `distant warning siren, sickly detuned pad, electric hiss, wordless public-address tone, loopable, no vocals.` The air itself is a threat; a siren wails somewhere, a sour pad hangs in the haze.

---

### Part IV — In Between (travel and heights)

#### 24. Line 3
*Riding the metro.*

**Style:**
`motorik ambient, rail clatter rhythm, steady pulse, passing light synth swells, tunnel reverb, 90 bpm, D minor, hypnotic, travelling, contemplative, instrumental, no vocals.` Riding beneath the city. The clatter of wheels on rails becomes a steady, hypnotic beat; a calm bass pulse drives it forward; passing tunnel lights sweep by as slow synth swells. A moment to sit, think and watch your reflection in the dark window.

#### 25. Above the Smog
*Rooftops, after the grapple line.*

**Style:**
`cinematic ambient, soaring airy pads, high wordless choir, wind, distant flying car engines, wide reverb, 75 bpm, D minor, breathtaking, free, hopeful, loopable, instrumental, no vocals.` For once you are above the noise. Wind, wide soaring pads and the hum of aerodynes gliding along their sky lanes. Slowly, a high wordless choir rises, and the five-note melody of the title appears very slowly, hopeful, almost reaching a resolution before drifting away. The whole skyline at your feet: the closest anyone gets to the sky.

#### 26. Window Seat
*Cab ride.*

**Style:**
`night drive, smooth synthwave ambient, warm bass, gentle chords, soft rain on glass, 85 bpm, D minor, relaxed, short 40 seconds, instrumental, no vocals.` City lights slide past a rain-streaked window. Warm bass, smooth chords, the quiet hum of an electric engine. A short breath between places.

---

### Part V — Faces (places and people)

#### 27. Corner Booth
*Meeting a fixer.*

**Style:**
`cyberpunk noir ambient, walking synth bass, muted brass-like pad, soft brushes, smoky, 70 bpm, D minor, secretive, cool, loopable, instrumental, no vocals.` The back corner of a smoky bar where work is handed out. A walking bass line, soft brushes, and a muted, trumpet-like synth pad straight out of an old noir film, reshaped by neon. Cool, discreet and a little dangerous: everyone here wants something.

#### 28. Chrome and Needles
*The ripperdoc.*

**Style:**
`dark clinical ambient, medical monitor beeps, whirring servos, cold metallic pads, low heartbeat pulse, 70 bpm, D minor, unsettling, fascinating, loopable, instrumental, no vocals.` A back-alley surgery behind a noodle shop. Monitors beep, servos whir and click, a soft heartbeat pulse keeps time. Cold metallic pads mix with the warm hum of fluorescent tubes: the Glass is entering a human body. Fascinating and slightly nauseating.

#### 29. Counter Service
*Shops and stalls.*

**Style:**
`retro electronic, plinky synth melody, soft bass, light percussion, tape warmth, 90 bpm, F major, friendly, tired, loopable, instrumental, no vocals.` The store music of a gun shop, an electronics stall or a noodle counter at 2 a.m.: a plinky little melody, friendly but tired, like a tape that has played all night and is starting to wear out.

#### 30. Patched Up
*Clinic, waking up after MedEvac.*

**Style:**
`ambient, soft hospital monitor beeps, warm slow pad, quiet, 60 bpm, F major, relief, exhaustion, short, instrumental, no vocals.` Waking up stitched together under harsh light. Quiet monitor beeps and a warm, slowly breathing pad: you are alive. The bill is on its way.

#### 31. Green Text
*The home terminal: messages and news.*

**Style:**
`minimal electronic, retro computer hum, soft modem-like data tones, slow pads, 80 bpm, D minor, focused, quiet, loopable, instrumental, no vocals.` Reading messages in the dark by the light of a green screen. The soft hum of an old machine, gentle data tones like a distant modem, slow pads. Focused, private, late.

---

### Part VI — The Job (missions)

All mission states share **95 bpm, E minor** and the **Heartbeat** (a soft low double pulse), so the game can move
between them without a cut as guards notice you.

#### 32. Casing the Place
*Approaching a target building.*

**Style:**
`tension ambient, muted double pulse, ominous pads, faint ticking, 95 bpm, E minor, focused, ominous, preparation, loopable, instrumental, no vocals.` Standing across the street from the target. A low double pulse like a calm heartbeat, dark pads, a faint tick like a watch. You are counting cameras, guards and doors. Focused, quiet, with the cold Grid arpeggio glimmering at the edge of hearing: the corp's building is awake.

#### 33. Ghost
*Inside, unnoticed.*

**Style:**
`stealth ambient, very quiet low heartbeat pulse, breathy pads, sparse metallic clicks, 95 bpm, E minor, tense, controlled, quiet, loopable, instrumental, no vocals.` Moving through the building unseen. The music almost disappears: a very low heartbeat pulse, breathy pads like held breath, and the occasional metallic click, a lock, a footstep, a camera turning. Every sound feels like a risk. Tense, controlled and very quiet.

#### 34. Something Moved
*A guard is suspicious or searching.*

**Style:**
`suspense ambient, faster heartbeat pulse, dissonant string-like swells, sharp ticks, 95 bpm, E minor, suspenseful, uneasy, loopable, instrumental, no vocals.` A guard heard something. The heartbeat doubles; dissonant, string-like swells rise and fall like a flashlight sweeping a corridor; sharp ticks cut through. Hold your breath and don't move.

#### 35. Hot
*Combat.*

**Style:**
`dark synthwave combat, aggressive distorted bass, fast synth arpeggios, punchy electronic drums, industrial hits, 95 bpm double-time, E minor, urgent, desperate, loopable, instrumental, no vocals.` Guns out. The heartbeat becomes a pounding drum, the bass distorts, a fast arpeggio races ahead. Urgent and desperate rather than heroic: this is a mercenary fighting for rent, not a hero saving the world. Gritty, tape-saturated and loud.

#### 36. Alarm
*The alarm is raised; getting out.*

**Style:**
`cyberpunk action, siren-like synth rising and falling, pounding drums, racing arpeggio, 95 bpm double-time, E minor, frantic, escape, loopable, instrumental, no vocals.` The building knows you are here. A siren-like synth wails up and down over pounding drums and a racing arpeggio; cold Grid tones flash like security lights. Only one thing matters: the exit.

#### 37. Clean Exit
*Objective taken without an alarm.*

**Style:**
`ambient sting, rising arpeggio resolving, warm bass, 95 bpm, E major, satisfying, quiet confidence, short 20 seconds, instrumental, no vocals.` The data is in your pocket and no one saw you. A soft arpeggio rises and resolves into a warm major chord. No fanfare, just a cool, private smile.

#### 38. Sideways
*Job failed.*

**Style:**
`ambient sting, falling detuned synth notes, low drone, 70 bpm, E minor, disappointed, heavy, short 20 seconds, instrumental, no vocals.` It went wrong. A few detuned notes fall slowly into a low drone, like a coin rolling into a drain.

#### 39. Paid in Full
*Reporting back and getting paid.*

**Style:**
`noir synth, mellow chords, light groove, warm bass, 85 bpm, A minor to C major, relief, satisfaction, short 30 seconds, instrumental, no vocals.` Cred lands in the account. Mellow, smoky chords over a light groove, a nod from the fixer across the table. For a moment, the Rent motif turns into something that sounds almost like a sigh of relief.

---

### Part VII — Trouble

#### 40. Heat
*Wanted by the police (Heat 2–3).*

**Style:**
`tension electronic, distant sirens woven into pads, pulsing bass, helicopter rotor whoosh rhythm, 100 bpm, E minor, paranoid, hunted, loopable, instrumental, no vocals.` The police are looking for you. A pulsing bass, sirens melted into the pads so you are never sure if they are real, and a rotor-like whoosh that circles overhead. Every corner might have a camera; every passer-by might call it in.

#### 41. Manhunt
*Heat 4–5.*

**Style:**
`industrial action, heavy industrial drums, distorted synth bass, alarm tones, 100 bpm double-time, E minor, relentless, overwhelming, loopable, instrumental, no vocals.` The whole city is after you. Heavy industrial drums, a distorted bass that never lets up, alarm tones on every side. The Glass and the Ground both turn against you. Run or hide.

#### 42. Not Your Block
*Entering a hostile gang's territory.*

**Style:**
`dark street ambient, heavy slow bass, distorted low synth, sparse hi-hats, 75 bpm half-time, D minor, menacing, watchful, loopable, instrumental, no vocals.` You crossed an invisible line. Slow, heavy bass, a growling low synth and a few sparse hi-hats. Graffiti glows in the neon; eyes follow you from doorways.

#### 43. Static in the Nerves
*Neural glitch (cyberware over capacity).*

**Style:**
`glitch, stuttering synths, digital noise bursts, bitcrushed pads, disorienting, short 10 seconds, instrumental, no vocals.` For a second or two your chrome betrays you: the audio stutters, breaks into bitcrushed noise and snaps back.

#### 44. Warm Bodies
*Thermal optics active (a layer under any track).*

**Style:**
`drone ambient, warm low hum, soft heartbeat thumps, muffled, low pass, loopable, instrumental, no vocals.` Seeing heat. The world goes muffled and red; a warm hum and soft heartbeats, as if you could hear the bodies you see.

#### 45. Flatline
*Going down.*

**Style:**
`dark ambient, slowing heartbeat, muffled sounds, cold pad, fading wordless choir, 60 bpm, D minor, fading, somber, short 30 seconds, instrumental, no vocals.` The heartbeat slows, the city sounds go muffled and far away, a cold pad stretches out, and a wordless choir fades like a memory of light. Then silence.

---

### Part VIII — Later chapters (M4 Matrix, M5 the corporate game)

#### 46. Jack In
*Entering the Matrix.*

**Style:**
`digital ambient, accelerating shimmering arpeggios, rising white noise, crystalline pads, 110 bpm, A minor, euphoric, transcendent, instrumental, no vocals.` The body falls away. Arpeggios accelerate, white noise rises like a rushing wind, and then everything opens into bright crystalline pads: a sky made of light, the only sky left.

#### 47. The Grid
*Moving through the Matrix.*

**Style:**
`cyberspace ambient, clean pulsing sequencer, cold neon pads, deep space reverb, 110 bpm, A minor, vast, abstract, loopable, instrumental, no vocals.` An endless glowing geometry. The Grid motif, fast and perfectly even, pulses through vast neon pads and deep reverb. Abstract, beautiful and inhuman: the corps' own world.

#### 48. ICE
*Fighting security programs.*

**Style:**
`glitch electronic, harsh glitch drums, square-wave bass, fast arpeggios, 110 bpm, A minor, aggressive, digital, loopable, instrumental, no vocals.` The system fights back. Harsh glitch percussion, a biting square-wave bass and arpeggios that break apart and reassemble. Cold, machine-like violence.

#### 49. The Offer
*First contact from the black-ops division of a megacorp.*

**Style:**
`cinematic dark ambient, slow brass-like synth, low wordless male choir, cold strings, deep sub bass, 60 bpm, D minor, ominous, powerful, seductive, instrumental, no vocals.` A shadow reaches out of the Glass. Slow, heavy brass-like synths, a low wordless male choir humming like the engine of a building, cold strings and a sub bass you feel more than hear. Power, money and danger in a single offer you cannot really refuse.

#### 50. Breaking
*News of big world events.*

**Style:**
`news sting, bright synth stabs, ticking pulse, broadcast feel, short 15 seconds, instrumental, no vocals.` The city's news jingle, used when something big happens: a councillor falls, a corporation strikes, a gang is wiped out. Bright, cold stabs over a ticking pulse; no voice.

#### 51. Dawn That Never Comes
*The ending.*

**Style:**
`cinematic ambient, emotional, warm analog synth melody, swelling strings, wordless choir, 70 bpm, D major, bittersweet, hopeful, grand, instrumental, no vocals.` The end of the story. The five-note melody from the title returns on a warm analog synth, and this time it finally resolves into a major chord. Strings swell, the wordless choir rises fully for the first and only time, and the rain slowly stops. Bittersweet and hopeful: the sun is still gone, but somewhere, maybe, it could come back.
