# Silverrock — Soundtrack brief

Status: draft for generation, 2026-11-05. Companion to the [game design](01-game-design.md).

Every track is **ambient and instrumental**. The only voice allowed is a **wordless choir or a breathy pad-like
vocal texture**, never words. The city never sees the sun, so the whole score lives at night: cold synths, rain,
distant machinery, warm analogue pads, slow pulses, tape hiss. It should sound like it belongs to a retro low-poly
world: slightly lo-fi, a little worn, never glossy pop.

## How to use these prompts with Suno

- Switch on **Instrumental**.
- **Simple mode:** paste the *Prompt* line into the song description.
- **Custom mode:** paste the *Style* line into "Style of Music" and leave the lyrics empty. The title goes into the
  title field.
- If a voice with words slips in anyway, add `no vocals, no lyrics, no singing` at the end of the style. For tracks
  that allow a choir, keep the word `wordless` next to it.
- Every track should loop in the game. Generate a few takes and pick the one with the steadiest ending and no big
  final hit. Generate the in-game loops at 2–4 minutes.
- Suno does not know artist names reliably and they can cause refusals, so the prompts describe the sound, not
  other composers.

Shared sound palette (already part of each prompt where it fits): *dark ambient, analog synth pads, slow arpeggios,
sub bass, rain texture, tape hiss, reverb, cinematic, cyberpunk, no drums or soft muted drums.*

Wherever there are several variants of one situation (district, alert state), the variants use the same key and
tempo, so the game can crossfade between them.

---

## 1. Menus and the start of a game

### 1.1 Silverrock (title theme)
- **When:** title screen.
- **Prompt:** Slow dark cyberpunk ambient theme, lonely analog synth melody over deep pads, distant city hum and rain, wordless choir swells, melancholic and grand, instrumental.
- **Style:** `dark ambient, cyberpunk, cinematic, analog synth lead, warm pads, wordless choir, rain ambience, slow 70 bpm, D minor, melancholic, majestic, instrumental, no vocals`
- **Description:** The city's main theme. A simple five-note synth motif rises out of rain noise and a low drone; a wordless choir enters halfway and lifts it, then everything sinks back into the rain. It should feel like looking at an endless night skyline: beautiful, cold and a little hopeless. The motif returns in other tracks.

### 1.2 Who Are You (character creation)
- **When:** choosing background and look.
- **Prompt:** Calm introspective synth ambient, soft pulsing arpeggio, glassy bell tones, gentle sub bass, curious and quiet, instrumental, loopable.
- **Style:** `ambient electronic, minimal, soft arpeggiator, glass bells, warm pads, sub bass, 80 bpm, D minor, introspective, calm, loopable, instrumental, no vocals`
- **Description:** A quiet mirror moment. A soft, repeating arpeggio like a scanner reading a face, with glassy bell notes picking out the title motif. No tension, just anticipation.

### 1.3 Pause
- **When:** pause screen and settings.
- **Prompt:** Very sparse muted synth drone, slow low pad, faint rain, almost silent, instrumental, seamless loop.
- **Style:** `drone ambient, minimal, low pad, faint rain, very quiet, slow, loopable, instrumental, no vocals`
- **Description:** A held breath. One low pad with slow filter movement and faint rain, quiet enough to sit under menus without being noticed.

---

## 2. Home

### 2.1 Four Walls (apartment, ordinary night)
- **When:** inside your own apartment.
- **Prompt:** Cozy lo-fi ambient, warm electric piano chords, rain on the window, soft vinyl crackle, muffled city outside, tired but safe, instrumental, no drums.
- **Style:** `lo-fi ambient, warm electric piano, rain on window, vinyl crackle, muffled city ambience, soft pads, 65 bpm, F major 7, cozy, tired, intimate, loopable, instrumental, no vocals`
- **Description:** The one warm place in the city. Slow electric piano chords, rain against glass, the muffled sound of traffic and neighbours through thin walls. Safe, small and tired, like coming home after a long shift.

### 2.2 Rent Is Due (apartment, rent due in under two days or debt open)
- **When:** at home while rent is due soon or unpaid.
- **Prompt:** Uneasy lo-fi ambient, detuned electric piano, ticking clock pulse, low dissonant drone, rain, quiet anxiety, instrumental.
- **Style:** `lo-fi ambient, detuned electric piano, ticking clock percussion, dissonant low drone, rain, 65 bpm, F minor, anxious, restless, loopable, instrumental, no vocals`
- **Description:** The same room, but the chords have gone slightly out of tune and a soft tick, like a clock or a dripping pipe, keeps time. A low drone sits under everything. Nothing dramatic, just the steady weight of money worries.

### 2.3 Lights Out (sleeping, time skip)
- **When:** sleeping in bed or in a capsule hotel.
- **Prompt:** Slow dreamy ambient, soft pads swelling and fading, distant wordless choir, gentle rain, sleep, instrumental, 60 seconds.
- **Style:** `dream ambient, soft pads, slow swells, distant wordless choir, gentle rain, 50 bpm, F major, peaceful, sleepy, instrumental, no vocals`
- **Description:** A short sting for the sleep transition. Pads slowly swell and dissolve, with a faint wordless choir far away, like drifting off while it rains.

### 2.4 Still Night (waking up)
- **When:** waking up after sleep.
- **Prompt:** Short gentle ambient sting, soft synth bell motif, rising pad, rain, a new night begins, instrumental, 30 seconds.
- **Style:** `ambient sting, soft synth bells, rising pad, rain, 70 bpm, D minor, gentle, hopeful but grey, instrumental, no vocals`
- **Description:** Waking up and it is still night. Three soft bell notes from the title motif over a rising pad: a little hope, a lot of grey.

### 2.5 New Keys (moved to a better apartment)
- **When:** after moving up a housing tier.
- **Prompt:** Warm uplifting ambient, bright synth arpeggio, soft strings pad, sense of progress and relief, cyberpunk night, instrumental, 45 seconds.
- **Style:** `uplifting ambient, bright synth arpeggio, string pad, warm bass, 85 bpm, A major, relief, progress, instrumental, no vocals`
- **Description:** A short, warm reward cue. A bright arpeggio climbs over soft strings: you have moved a step up. It ends open, not triumphant.

---

## 3. The streets (one loop per district type)

All district loops share **75 bpm, D minor** and the same sub bass pulse, so they can crossfade while walking.

### 3.1 Glass and Gold (Corporate Spire)
- **Prompt:** Cold sleek corporate ambient, crystalline synth pads, precise arpeggio, deep sub bass, high wordless choir, sterile and powerful, cyberpunk, instrumental.
- **Style:** `corporate ambient, cyberpunk, crystalline pads, precise arpeggio, deep sub bass, airy wordless choir, 75 bpm, D minor, cold, sleek, imposing, loopable, instrumental, no vocals`
- **Description:** Towers of glass and money. Clean, expensive, slightly inhuman: a precise arpeggio like ticker data, cold crystal pads and a thin high choir far above the streets. Nothing out of place.

### 3.2 City Hall at Night (Civic Center)
- **Prompt:** Solemn civic ambient, slow organ-like synth chords, distant bells, low brass pad, rain, bureaucratic and grey, instrumental.
- **Style:** `dark ambient, organ-like synth, distant bells, low brass pad, rain, 75 bpm, D minor, solemn, institutional, loopable, instrumental, no vocals`
- **Description:** Stone steps, flags and surveillance. Slow organ-like chords and a distant bell give it a public, official feel, worn down and grey.

### 3.3 Neon Market (Commercial)
- **Prompt:** Busy cyberpunk market ambient, layered synth arpeggios, muffled crowd chatter texture, soft pulsing bass, neon hum, light percussion, instrumental.
- **Style:** `cyberpunk ambient, layered arpeggios, crowd ambience texture, neon hum, soft pulsing bass, light muted percussion, 75 bpm, D minor, busy, colorful, loopable, instrumental, no vocals`
- **Description:** Shops, signs and crowds. Overlapping bright arpeggios like competing ads, a warm pulse underneath and the indistinct murmur of people. Lively but still night.

### 3.4 After Hours (Entertainment)
- **Prompt:** Hazy nightlife ambient, muffled club bass from behind walls, sultry synth chords, glittering arps, neon glow, wet streets, instrumental.
- **Style:** `nightlife ambient, muffled club kick, deep house bass muffled, sultry synth chords, glittering arpeggios, 75 bpm half-time, D minor, hazy, seductive, loopable, instrumental, no vocals`
- **Description:** Bars and clubs everywhere. The beat is heard only through walls, muffled and distant, while hazy chords and glittering arps hang in the neon. Seductive and a little dangerous.

### 3.5 Blocks (Residential)
- **Prompt:** Quiet residential night ambient, soft warm pads, distant traffic, sparse piano notes, gentle rain, lonely but human, instrumental.
- **Style:** `ambient, soft warm pads, sparse piano, distant traffic ambience, gentle rain, 75 bpm, D minor, lonely, human, calm, loopable, instrumental, no vocals`
- **Description:** Endless apartment blocks with a few lit windows. Sparse piano notes over warm pads, far-off traffic and rain: thousands of small lives behind glass.

### 3.6 Old Quarter
- **Prompt:** Nostalgic old town ambient, worn tape-saturated synths, distant accordion-like pad, rain on tiles, faded memories, cyberpunk, instrumental.
- **Style:** `nostalgic ambient, tape saturation, worn analog synths, reed organ pad, rain on rooftops, 75 bpm, D minor, faded, melancholic, warm, loopable, instrumental, no vocals`
- **Description:** The part of the city that existed before the towers. Tape-worn synths, a reed-organ-like pad and rain on old tiles; it feels like a memory that is slowly being demolished.

### 3.7 Foundry (Industrial)
- **Prompt:** Heavy industrial ambient, rhythmic metallic machinery, deep droning bass, steam hiss, dark synth pads, relentless, instrumental.
- **Style:** `industrial ambient, metallic percussion loops, machinery rhythm, deep drone, steam hiss, dark pads, 75 bpm, D minor, heavy, grinding, loopable, instrumental, no vocals`
- **Description:** Factories that never sleep. Machines thud and clank in a slow rhythm, steam hisses, and a dark drone grinds underneath. Workers walk past in silence.

### 3.8 Low Tide (Docks)
- **Prompt:** Foggy harbour ambient, distant ship horns, deep swelling bass like waves, creaking metal, cold synth pads, lonely, instrumental.
- **Style:** `dark ambient, foggy harbor, distant ship horns, wave-like bass swells, creaking metal, cold pads, 75 bpm, D minor, lonely, vast, loopable, instrumental, no vocals`
- **Description:** Containers, cranes and black water. Bass swells like slow waves, a ship horn sounds far off, metal creaks. Cold, empty and huge.

### 3.9 Clean Rooms (Research Campus)
- **Prompt:** Clinical futuristic ambient, pristine sine tones, slow data-like blips, soft airy pads, curious and slightly eerie, instrumental.
- **Style:** `futuristic ambient, sine tones, data blips, airy pads, subtle glitch, 75 bpm, D minor, clinical, curious, eerie, loopable, instrumental, no vocals`
- **Description:** Labs and lawns under floodlights. Pure sine tones and soft data blips, airy and clean, with a slight eeriness: something is being tested here.

### 3.10 Under the Highway (Undercity)
- **Prompt:** Dark claustrophobic ambient, rumbling traffic overhead, dripping water, low distorted drone, distant echoes, gritty, instrumental.
- **Style:** `dark ambient, claustrophobic, rumbling overhead traffic, dripping water, distorted low drone, cave reverb, 75 bpm, D minor, gritty, oppressive, loopable, instrumental, no vocals`
- **Description:** The city's basement under the elevated highways. Traffic rumbles overhead, water drips, sounds echo off concrete. Gritty and close, never fully dark.

### 3.11 Scrap (Barrens)
- **Prompt:** Desolate wasteland ambient, wind, crackling fire, sparse detuned guitar-like synth plucks, empty space, lonely, instrumental.
- **Style:** `desolate ambient, wind, crackling fire, detuned plucked synth, sparse, wide reverb, 75 bpm, D minor, empty, lonely, harsh, loopable, instrumental, no vocals`
- **Description:** Empty lots and burning barrels at the city's edge. Wind, fire crackle and a few detuned plucks in a lot of empty space. The skyline glows far away.

### 3.12 Weather layers (optional stems)
Generate these as separate textures to mix over any district loop.
- **Downpour:** `heavy rain ambience, thunder rumble, no music, field recording style, loopable, no vocals`
- **Fog:** `muffled foghorn, soft white noise, low airy drone, very quiet, loopable, no vocals`
- **Smog alert / acid-rain advisory:** `distant warning siren, sickly detuned pad, hiss, public address tone without words, loopable, no vocals`

---

## 4. Getting around

### 4.1 Line 3 (metro ride)
- **Prompt:** Hypnotic train ambient, rhythmic rail clatter as percussion, steady motorik pulse, passing light flickers as synth swells, instrumental.
- **Style:** `motorik ambient, train rhythm percussion, steady pulse, synth swells, tunnel reverb, 90 bpm, D minor, hypnotic, travelling, instrumental, no vocals`
- **Description:** Riding under the city. The clatter of rails becomes the beat, a steady pulse carries it and passing lights sweep by as synth swells. Hypnotic, a moment to think.

### 4.2 Up on the Roofs (rooftops, after the grapple line)
- **Prompt:** Wide open rooftop ambient, soaring airy pads, high wordless choir, wind, distant flying cars, skyline at night, breathtaking, instrumental.
- **Style:** `cinematic ambient, soaring pads, high wordless choir, wind, distant engines, wide reverb, 75 bpm, D minor, breathtaking, free, loopable, instrumental, no vocals`
- **Description:** Above the noise for once. Wind, wide pads and a high wordless choir, aerodynes humming past: the whole skyline at your feet. The title motif appears very slowly.

### 4.3 Cab Ride (cab travel transition)
- **Prompt:** Smooth night drive ambient, warm bass groove, gentle synth chords, city lights passing, relaxed, instrumental, 40 seconds.
- **Style:** `night drive, smooth synthwave ambient, warm bass, gentle chords, 85 bpm, D minor, relaxed, instrumental, no vocals`
- **Description:** A short travel cue. Warm bass and smooth chords as lights slide past the window.

---

## 5. Places and people

### 5.1 The Back Room (fixer meeting)
- **Prompt:** Smoky noir ambient, slow upright-bass-like synth, muted trumpet-like pad, low murmurs, cigarette smoke and secrets, instrumental.
- **Style:** `noir ambient, cyberpunk noir, synth upright bass, muted brass pad, soft brushes, smoky, 70 bpm, D minor, secretive, cool, loopable, instrumental, no vocals`
- **Description:** A corner booth where work is handed out. Noir mood: a walking bass, a muted brass-like pad, soft brushes. Cool, discreet, a little dangerous.

### 5.2 Chrome (ripperdoc)
- **Prompt:** Unsettling clinical ambient, medical beeps, whirring servo sounds, cold metallic pads, low heartbeat pulse, instrumental.
- **Style:** `dark clinical ambient, medical monitor beeps, servo whirs, metallic pads, heartbeat pulse, 70 bpm, D minor, unsettling, cold, loopable, instrumental, no vocals`
- **Description:** A back-alley surgery. Monitor beeps, the whir of servos, a heartbeat-like pulse and cold metal pads. Fascinating and a little nauseating.

### 5.3 Counter Service (shops)
- **Prompt:** Light quirky shop ambient, plinky synth melody, soft bass, retro electronic, friendly but tired, instrumental, loopable.
- **Style:** `retro electronic, plinky synth melody, soft bass, light percussion, 90 bpm, F major, friendly, tired, loopable, instrumental, no vocals`
- **Description:** Gun shops, electronics stalls and noodle counters. A plinky little melody, slightly tired, like store music that has played all night.

### 5.4 Clinic Lights (clinic, waking up after MedEvac)
- **Prompt:** Soft sterile ambient, gentle beeping monitor, slow warm pad, relief and exhaustion, quiet, instrumental.
- **Style:** `ambient, soft hospital monitor beeps, warm slow pad, quiet, 60 bpm, F major, relief, exhaustion, instrumental, no vocals`
- **Description:** Waking up patched together. Quiet beeps and a warm pad: you are alive, and the bill is on its way.

### 5.5 Terminal (home terminal, messages and news)
- **Prompt:** Minimal retro computer ambient, soft data hum, gentle modem-like tones, slow pads, focused, instrumental, loopable.
- **Style:** `minimal electronic, retro computer hum, soft data tones, slow pads, 80 bpm, D minor, focused, quiet, loopable, instrumental, no vocals`
- **Description:** Reading messages at night. Soft machine hum and gentle data tones, focused and calm.

---

## 6. Jobs and missions

All mission states share **95 bpm, E minor**, so the game can move between them as guards notice you.

### 6.1 Casing the Place (approaching a target building)
- **Prompt:** Tense low-key ambient, steady muted pulse, slow ominous pads, faint ticking, preparation before a heist, instrumental.
- **Style:** `tension ambient, muted pulse, ominous pads, faint ticking, 95 bpm, E minor, focused, ominous, loopable, instrumental, no vocals`
- **Description:** Standing across the street from the target. A steady muted pulse and dark pads; you are planning the way in.

### 6.2 Ghost (inside, unnoticed)
- **Prompt:** Stealth ambient, very quiet sub bass pulse, soft breathy pads, sparse metallic clicks, held tension, instrumental, loopable.
- **Style:** `stealth ambient, quiet sub bass pulse, breathy pads, sparse metallic clicks, 95 bpm, E minor, tense, quiet, controlled, loopable, instrumental, no vocals`
- **Description:** Moving through the building unseen. Almost silent: a low heartbeat-like pulse, breathy pads and the occasional click. Every sound should feel risky.

### 6.3 Something Moved (a guard is suspicious or searching)
- **Prompt:** Rising stealth tension, pulse speeds up, dissonant string-like synth swells, sharp ticks, suspense, instrumental, loopable.
- **Style:** `suspense ambient, faster pulse, dissonant swells, sharp ticks, 95 bpm, E minor, suspenseful, uneasy, loopable, instrumental, no vocals`
- **Description:** A guard heard something. The pulse doubles, dissonant swells rise, sharp ticks like a flashlight sweeping. Hold your breath.

### 6.4 Hot (combat)
- **Prompt:** Driving dark cyberpunk combat track, aggressive distorted bass, fast synth arpeggios, punchy electronic drums, urgent, instrumental, no vocals.
- **Style:** `dark synthwave combat, distorted bass, fast arpeggios, punchy electronic drums, industrial hits, 95 bpm double-time, E minor, aggressive, urgent, loopable, instrumental, no vocals`
- **Description:** Guns out. Distorted bass, fast arpeggios and hard electronic drums. Urgent, not heroic: this is a mercenary fighting for rent.

### 6.5 Alarm (alarm raised, getting out)
- **Prompt:** Frantic escape track, siren-like synth rising and falling, pounding drums, racing arpeggio, panic, instrumental, no vocals.
- **Style:** `cyberpunk action, siren-like synth, pounding drums, racing arpeggio, 95 bpm double-time, E minor, frantic, escape, loopable, instrumental, no vocals`
- **Description:** The building knows. A siren-like synth wails over pounding drums and a racing arpeggio; the only goal is the exit.

### 6.6 Clean (objective taken without an alarm)
- **Prompt:** Short satisfying ambient sting, soft rising arpeggio resolving, warm bass, quiet confidence, instrumental, 20 seconds.
- **Style:** `ambient sting, rising arpeggio, resolving chord, warm bass, 95 bpm, E major, satisfying, confident, instrumental, no vocals`
- **Description:** A small, cool success cue: the arpeggio rises and resolves into a major chord. No fanfare.

### 6.7 Sideways (job failed)
- **Prompt:** Short downbeat sting, falling detuned synth notes, low drone, disappointment, instrumental, 20 seconds.
- **Style:** `ambient sting, falling detuned synth, low drone, 70 bpm, E minor, disappointed, heavy, instrumental, no vocals`
- **Description:** It went wrong. A few detuned notes fall into a low drone.

### 6.8 Paid (reporting back, payment)
- **Prompt:** Smooth cool reward cue, mellow synth chords, light groove, relief and satisfaction, cyberpunk noir, instrumental, 30 seconds.
- **Style:** `noir synth, mellow chords, light groove, warm bass, 85 bpm, A minor to C major, relief, satisfaction, instrumental, no vocals`
- **Description:** Cred in the account. Mellow chords and a light groove, a little noir: you got away with it, this time.

---

## 7. Trouble

### 7.1 Heat (wanted by the police, Heat 2–3)
- **Prompt:** Paranoid pursuit ambient, distant sirens woven into synths, tense pulsing bass, helicopter-like rhythmic whoosh, instrumental, loopable.
- **Style:** `tension electronic, distant sirens texture, pulsing bass, rotor whoosh rhythm, 100 bpm, E minor, paranoid, hunted, loopable, instrumental, no vocals`
- **Description:** The police are looking. A pulsing bass, sirens melted into the pads and a rotor-like whoosh overhead. You feel watched on every corner.

### 7.2 Manhunt (Heat 4–5)
- **Prompt:** Relentless chase track, heavy industrial drums, aggressive distorted synth bass, alarm tones, overwhelming force, instrumental, no vocals.
- **Style:** `industrial action, heavy drums, distorted synth bass, alarm tones, 100 bpm double-time, E minor, relentless, overwhelming, loopable, instrumental, no vocals`
- **Description:** The whole city is after you. Heavy industrial drums and distorted bass, alarm tones everywhere. Run or hide.

### 7.3 Gang Turf (entering a hostile gang's territory)
- **Prompt:** Menacing street ambient, slow heavy bass, distorted low synth, sparse trap-like hi-hats, graffiti and danger, instrumental.
- **Style:** `dark street ambient, heavy slow bass, distorted low synth, sparse hi-hats, 75 bpm half-time, D minor, menacing, loopable, instrumental, no vocals`
- **Description:** You are on someone else's block. Slow heavy bass and sparse hi-hats; eyes are on you.

### 7.4 Neural Glitch (over capacity, glitching)
- **Prompt:** Glitchy corrupted ambient, stuttering synths, digital noise bursts, bitcrushed pads, disorienting, instrumental, 15 seconds.
- **Style:** `glitch, stuttering synths, digital noise, bitcrushed pads, disorienting, instrumental, no vocals`
- **Description:** A short stinger for when your chrome glitches: stuttering, bitcrushed, broken audio for a second or two.

### 7.5 Thermal (thermal optics active)
- **Prompt:** Low infrared ambient layer, warm humming drone, soft heartbeat thumps, muffled, instrumental, loopable.
- **Style:** `drone ambient, warm hum, heartbeat thumps, muffled, low pass, loopable, instrumental, no vocals`
- **Description:** A layer under any track while thermal vision is on: everything muffled, a warm hum and heartbeats.

### 7.6 Flatline (down)
- **Prompt:** Slow fading ambient, heartbeat slowing to silence, distant muffled sounds, cold pad, wordless choir fading, instrumental, 30 seconds.
- **Style:** `dark ambient, slowing heartbeat, muffled ambience, cold pad, fading wordless choir, 60 bpm, D minor, fading, somber, instrumental, no vocals`
- **Description:** The screen goes dark. A heartbeat slows, sounds go muffled, a wordless choir fades away.

### 7.7 Evicted (lost the apartment)
- **Prompt:** Bleak lonely ambient, cold wind, sparse sad piano, distant city noise, homeless in the rain, instrumental.
- **Style:** `bleak ambient, cold wind, sparse sad piano, rain, distant city noise, 60 bpm, D minor, lonely, homeless, loopable, instrumental, no vocals`
- **Description:** Locked out with nowhere to go. Cold wind, rain and a few sad piano notes from the apartment theme, now outside in the street.

---

## 8. Later milestones (M4 Matrix, M5 story)

### 8.1 Jack In (entering the Matrix)
- **Prompt:** Euphoric digital ambient, shimmering arpeggios accelerating, rising white noise, bright crystalline pads, entering cyberspace, instrumental.
- **Style:** `digital ambient, shimmering arpeggios, rising white noise, crystalline pads, 110 bpm, A minor, euphoric, transcendent, instrumental, no vocals`
- **Description:** The body falls away. Arpeggios accelerate, noise rises, then everything opens into bright crystal pads.

### 8.2 The Grid (moving through the Matrix)
- **Prompt:** Vast geometric cyberspace ambient, clean pulsing sequencer, cold neon pads, deep space reverb, mathematical, instrumental, loopable.
- **Style:** `cyberspace ambient, pulsing sequencer, neon pads, deep reverb, 110 bpm, A minor, vast, abstract, loopable, instrumental, no vocals`
- **Description:** An endless glowing grid. A clean sequencer pulse, cold neon pads and huge reverb: abstract and beautiful.

### 8.3 ICE (fighting security programs)
- **Prompt:** Aggressive digital combat, harsh glitch percussion, sharp square-wave bass, fast arpeggios, data storm, instrumental, no vocals.
- **Style:** `glitch electronic, harsh glitch drums, square wave bass, fast arpeggios, 110 bpm, A minor, aggressive, digital, loopable, instrumental, no vocals`
- **Description:** Security programs attack. Harsh glitch percussion and square-wave bass, cold and machine-like.

### 8.4 The Patron (first contact with the black-ops division)
- **Prompt:** Ominous corporate power theme, deep slow brass-like synth, low wordless male choir, cold strings, dangerous offer, instrumental.
- **Style:** `cinematic dark ambient, slow brass synth, low wordless male choir, cold strings, 60 bpm, D minor, ominous, powerful, instrumental, no vocals`
- **Description:** A megacorp's shadow reaches out. Slow brass-like synths and a low wordless choir: power, money and danger in one offer.

### 8.5 Breaking News (news and big world events)
- **Prompt:** Short urgent news sting, bright synth stabs, ticking pulse, broadcast feel, instrumental, 15 seconds.
- **Style:** `news sting, synth stabs, ticking pulse, broadcast, 120 bpm, instrumental, no vocals`
- **Description:** The city's news jingle, used when big events break (a councillor falls, a corp strikes). No voice.

### 8.6 Dawn That Never Comes (ending theme)
- **Prompt:** Emotional cinematic ambient finale, title melody on warm analog synth, swelling strings, wordless choir, bittersweet hope, instrumental.
- **Style:** `cinematic ambient, emotional, warm analog synth melody, swelling strings, wordless choir, 70 bpm, D major, bittersweet, hopeful, grand, instrumental, no vocals`
- **Description:** The ending. The title motif, now in a major key, on warm synth with strings and a wordless choir. Bittersweet: maybe, one day, the sun comes back.
