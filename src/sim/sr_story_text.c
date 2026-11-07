/* What the cast says: the lines that announce arc beats, stage the acts, report skipped beats,
 * nudge a stalled player, hint at the Pall and tell the endings. Three variants where it matters;
 * %1 %2 %3 are filled by sr_story_fill (first faction, second faction, place or person). */
#include "core/sr_rng.h"
#include "sim/sr_chess.h"

#include <stdio.h>
#include <string.h>

void sr_story_fill(char *out, size_t cap, const char *tmpl, const char *a, const char *b, const char *c) {
  size_t n = 0;
  for (const char *p = tmpl; *p && n + 1 < cap; p++) {
    if (p[0] == '%' && p[1] >= '1' && p[1] <= '3') {
      const char *ins = p[1] == '1' ? a : p[1] == '2' ? b : c;
      for (const char *q = ins ? ins : ""; *q && n + 1 < cap; q++) out[n++] = *q;
      p++;
      continue;
    }
    out[n++] = *p;
  }
  out[n] = '\0';
}

/* Who tells each arc type. */
SrCastRole sr_story_teller(SrArcType t) {
  switch (t) {
    case SR_ARC_ELECTION:
    case SR_ARC_SCANDAL: return SR_CAST_JOURNALIST;
    case SR_ARC_PROXY: return SR_CAST_FIXER;
    case SR_ARC_DEFECTOR:
    case SR_ARC_MOLE: return SR_CAST_RIVAL;
    default: return SR_CAST_HANDLER;
  }
}

/* [arc][0 opening, 1 after a won beat, 2 after a lost beat][variant]; %1 a, %2 b, %3 place. */
static const char *const k_beats[SR_ARC_COUNT][3][3] = {
    {{"%1 wants %2's site in %3. First we steal the blueprints.", "%1 is buying %2 the hard way. It starts in %3.",
      "Takeover season: %1 against %2. Plans first, at %3."},
     {"The plans worked. %1 moves on %2 in %3 next.", "%2 never saw it coming. Time to finish the job in %3.",
      "We have their layout. Now %1 takes %3."},
     {"%2 kept their secrets. %1 goes for their image instead.", "That went badly. %1 will soften %2 up first.",
      "%2 is ready for us now. We hit them another way."}},
    {{"%2 has a prototype in %3. %1 wants it, quietly.", "There is a device in %3 that %1 would kill for. Let's not.",
      "Word is %2 built something new in %3. %1 wants the files."},
     {"We have the files. Now make sure %2 can't build another.", "Copy secured. %1 wants %2's lab slowed down.",
      "Clean grab. Next, %2's production line in %3."},
     {"%2 still has it. Try again, harder.", "The prototype is still in %3. %1 is not patient.",
      "We missed it. %2 doubled the guards in %3."}},
    {{"Elections are coming. %1 is buying votes before %2 can.", "Council seats are for sale and %1 is shopping.",
      "I smell money in city hall. %1 wants a seat, %2 wants it more."},
     {"The envelope landed. Now %2's candidate needs a scandal.", "One councillor bought. Time to muddy %2.",
      "%1 has a friend in council. Let's make %2's friends look bad."},
     {"The bribe bounced. %1 will try another councillor.", "Nobody took the money. %1 is annoyed and persistent.",
      "City hall said no. For now."}},
    {{"%1 and %2 are at war in %3. Pick a side or get out of the way.",
      "The gangs are moving in %3: %1 against %2. Corps are paying both.",
      "Street war in %3. %1 needs muscle against %2."},
     {"%1 holds %3 now. %2 wants revenge.", "%2 lost ground in %3. Next round soon.",
      "The street remembers who helped %1. More work coming."},
     {"%2 held %3. %1 is regrouping.", "%1 got pushed back in %3. They want a second try.",
      "Bad night in %3. The war is not over."}},
    {{"Someone inside %2 wants out. %1 will pay for what they bring.", "%2 has a defector in %3 and %1 has an offer.",
      "A scientist at %2 is talking to %1. We need proof first."},
     {"The defector's data checks out. %2 will try to stop the move.", "Good data. Now %1 wants %2 distracted.",
      "They're real. Keep %2 busy while %1 gets them out."},
     {"The defector got cold feet. %1 wants their files anyway.", "No defector, no deal. %1 settles for the files.",
      "%2 found the leak. Things get messy now."}},
    {{"%2 runs supply out of %3. %1 wants the shelves empty.", "Cut %2's supply line in %3 and prices go up.",
      "A shortage would hurt %2 and help %1. It starts in %3."},
     {"Prices are climbing. %1 wants %2 blamed in the press.", "The shortage bites. %2 is losing face.",
      "%2 is scrambling. One more push in %3."},
     {"%2 kept the trucks rolling. %1 goes after their reputation.", "Supply held. %1 wants a scandal instead.",
      "%2 rerouted. Try again in %3."}},
    {{"%1 thinks %2 has a mole inside. Find them.", "There is a leak between %1 and %2. Follow the files.",
      "Somebody inside %1 talks to %2. We need to know who."},
     {"We have a name. Now we feed %2 the wrong story.", "Mole found. %1 wants to use them, not burn them.",
      "We know the leak. Let's make %2 trust it."},
     {"The trail went cold. Dig again at %2.", "No mole yet. %1 is getting paranoid.",
      "%2 covered their tracks. Not well enough, I hope."}},
    {{"I have a story on %1 and %2. I need proof from %3.", "Something stinks between %1 and %2. Help me prove it.",
      "Off the record: %1 has dirt on %2. I need the files."},
     {"The files are real. Now someone has to leak them.", "Proof in hand. %2 will want this buried.",
      "This goes all the way up. One more piece and I publish."},
     {"They shredded everything. I'll find another way in.", "No proof yet. %1 is watching me now.",
      "Dead end at %3. Not giving up."}}};

void sr_story_beat_text(char *out, size_t cap, SrArcType t, int beat, int branch, const char *a, const char *b,
                        const char *place, uint64_t key) {
  int stage = beat == 0 ? 0 : branch ? 1 : 2;
  sr_story_fill(out, cap, k_beats[(unsigned)t % SR_ARC_COUNT][stage][key % 3u], a, b, place);
}

void sr_story_skipped_text(char *out, size_t cap, bool success, const char *a, const char *b) {
  sr_story_fill(out, cap,
                success ? "While you were away, someone else did %1's job on %2. It worked. You weren't paid."
                        : "While you were away, %1's crew went after %2 without you. It failed.",
                a, b, NULL);
}

const char *sr_story_act_text(SrAct act) {
  switch (act) {
    case SR_ACT_PAWN: return "Welcome aboard. Small jobs first. Do them clean and the big ones follow.";
    case SR_ACT_KNIGHT: return "You're one of ours now. Real operations from here on. Rivals will call. Don't answer.";
    case SR_ACT_PROMOTION: return "This is the endgame. The board is about to change and you're on it. Stay close.";
    default: return "It's over. The city looks different from up here.";
  }
}

const char *sr_story_nudge_text(void) {
  return "You're still doing errands. To move up you need Runner-level cred and our trust at 40. Take this one.";
}

const char *sr_story_ghost_text(void) {
  return "You go deep for a pawn. The Pall is a machine, not weather. Find who runs it and the sun comes back.";
}

/* [ending][line]: the handler, the journalist, the Ghost. */
static const char *const k_endings[SR_END_SUNRISE + 1][3] = {
    {"", "", ""},
    {"We won. You did that. Nobody will ever know, which is the point.", "%1 owns the board now. I can't prove how.",
     "Loyal to the end. The Pall stays."},
    {"New colours suit you. %1 sends its thanks.", "Turncoat runner hands %1 the city. Sources say.",
     "You changed sides once. You'll do it again."},
    {"You burned it all. Don't call this number again.", "Files everywhere. Every corp in the city is bleeding.",
     "Fire clears the ground. Something will grow there."},
    {"You picked a king. Hope you picked right.", "%1 rises out of nowhere. Somebody made that happen.",
     "Kings come and go. The runner who crowns them stays."},
    {"The grid is down. Look outside.", "The sky over the city is blue. Nobody can explain it.",
     "You found the machine. Enjoy the sun."}};

void sr_story_ending_text(char *out, size_t cap, SrEnding e, int line, const char *corp) {
  if ((unsigned)e > SR_END_SUNRISE || line < 0 || line > 2) {
    out[0] = '\0';
    return;
  }
  sr_story_fill(out, cap, k_endings[e][line], corp, NULL, NULL);
}
