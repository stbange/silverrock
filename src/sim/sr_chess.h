/* The corporate game (M5): factions with resources pursue goals by doctrine and run operations
 * whose steps become contracts; contracts nobody takes are resolved off-screen; outcomes change
 * the city (ownership, security, prices, turf, people, advertising), drive politics, share
 * prices and the news, and the story campaign rides on top. All state is plain data advanced by
 * pure functions over (city, events, time); the game layer turns contracts into jobs and reports
 * the player's results back. */
#ifndef SR_CHESS_H
#define SR_CHESS_H

#include "sim/sr_events.h"
#include "sim/sr_population.h"
#include "world/sr_environ.h"
#include "world/sr_world.h"

#define SR_TICK_S         (6 * 3600) /* strategic tick */
#define SR_MAX_OPS        48
#define SR_MAX_CONTRACTS  32
#define SR_MAX_SECMODS    64
#define SR_MAX_CHANGES    64
#define SR_MAX_LINKS      64
#define SR_MAX_NEWS       48
#define SR_MAX_PENDING    8
#define SR_MAX_TRADES     16
#define SR_MAX_SUSPICION  8
#define SR_MAX_ARCS       3
#define SR_MAX_ARC_BEATS  6
#define SR_MAX_HISTORY    96
#define SR_COUNCIL_SEATS  9
#define SR_DOMINANCE      0.40f /* share of all megacorp resources that triggers the endgame */
#define SR_GANG_REBIRTH_S (3 * SR_WEEK_S)
#define SR_PRICE_MIN      5  /* cred per share */
#define SR_INSIDER_QTY    50 /* trades at least this large draw attention */

/* ---- factions and operations -------------------------------------------------------------- */

typedef enum SrResource {
  SR_RES_CAPITAL,
  SR_RES_INFLUENCE,
  SR_RES_FORCE,
  SR_RES_INTEL,
  SR_RES_NETDEF,
  SR_RES_IMAGE,
  SR_RES_COUNT
} SrResource;

typedef enum SrOpType {
  SR_OP_SABOTAGE,  /* hit a rival facility: security, prices, image */
  SR_OP_THEFT,     /* steal data: intel up, rival intel down */
  SR_OP_TAKEOVER,  /* take a rival facility */
  SR_OP_SMEAR,     /* damage a rival's public image */
  SR_OP_BRIBE,     /* buy a councillor */
  SR_OP_TURF,      /* gangs: take a district */
  SR_OP_FORTIFY,   /* raise own defences */
  SR_OP_EXPAND,    /* invest: capital and force */
  SR_OP_RETALIATE, /* strike back at whoever the evidence points to */
  SR_OP_HUNT,      /* a hostile patron hunts the player */
  SR_OP_COUNT
} SrOpType;

typedef enum SrChannel { SR_CH_BOARD, SR_CH_FIXER, SR_CH_GANG, SR_CH_HANDLER } SrChannel;

typedef struct SrFactionSim {
  int16_t res[SR_RES_COUNT];
  uint8_t goals[3];
  uint8_t goal_count;
  int8_t retaliate_against; /* faction to strike back at, -1 none */
  int64_t retaliate_by;
  int64_t cooldown_until;
  bool dissolved;
  int64_t reborn_at;
  uint8_t generation; /* gangs: how often the slot was refilled */
  uint16_t ops_by_type[SR_OP_COUNT];
} SrFactionSim;

typedef struct SrOperation {
  int32_t id;
  uint8_t type;
  int8_t client, target;
  int32_t building; /* target facility, -1 none */
  int8_t district;  /* target district (turf), -1 none */
  uint8_t step, steps;
  bool active, succeeded, failed;
  int8_t blamed; /* faction the victim blames after the outcome (-2 the player, -1 nobody) */
  int16_t arc;   /* arc index or -1 */
  int32_t contract;
  int64_t started;
} SrOperation;

typedef struct SrContract {
  int32_t id;
  int32_t op;
  uint8_t job_type; /* SrJobType */
  uint8_t channel;
  int8_t client;
  int32_t building;
  int64_t deadline;
  int32_t job_id; /* job on the board, -1 not posted */
  bool open, success, by_player;
  int8_t framed;        /* faction whose evidence the player planted, -1 */
  bool player_evidence; /* the player left traces */
} SrContract;

typedef struct SrSecMod {
  int32_t building;
  int8_t level; /* added security 0..5 */
} SrSecMod;

typedef struct SrOwnerChange {
  int32_t building;
  int8_t owner;
} SrOwnerChange;

typedef enum SrLinkType { SR_LINK_FUNDS, SR_LINK_OWNS, SR_LINK_BRIBES, SR_LINK_FEUD, SR_LINK_COUNT } SrLinkType;

typedef struct SrLink {
  int8_t a, b; /* factions; for BRIBES b is a council seat */
  uint8_t type;
  bool known; /* revealed to the player's Board */
} SrLink;

/* ---- politics -------------------------------------------------------------------------------- */

typedef struct SrSeat {
  int8_t backer;  /* faction behind the holder, -1 independent */
  int8_t lean;    /* -2 reform .. +2 law & order */
  int8_t bribed;  /* corp that bought the holder, -1 */
  int8_t scandal; /* vote-share penalty of a published scandal, decays */
  bool exposed, vacant;
  int64_t resign_at; /* exposed: resigns at this time */
  char name[SR_NAME_LEN];
} SrSeat;

typedef struct SrPolitics {
  int seat_count; /* the city's council size (5, 7 or 9) */
  SrSeat seats[SR_COUNCIL_SEATS];
  SrSeat mayor;
  int64_t next_council, next_mayor, next_vote, by_election;
  uint8_t seat_group;     /* which third of the council is up next (seat % 3) */
  int8_t police_budget;   /* 0 cut .. 2 raised */
  uint16_t curfew;        /* districts under curfew */
  int8_t police_contract; /* corp running the police, -1 the city */
  int8_t tamper_for;      /* faction the vote host was altered for, -1 none */
  int elections_held;
  float last_share[SR_COUNCIL_SEATS + 1]; /* vote share of the winner (mayor last) */
} SrPolitics;

/* ---- market ---------------------------------------------------------------------------------- */

typedef struct SrTrade {
  int8_t corp;
  int32_t qty; /* negative: sold / shorted */
  int32_t price;
  int64_t time;
} SrTrade;

typedef struct SrSuspicion {
  int8_t corp; /* the traded corp */
  int8_t by;   /* finance corp that noticed */
  int64_t time;
} SrSuspicion;

typedef struct SrMarket {
  int32_t price[SR_MAX_FACTIONS]; /* cred per share, megacorps only */
  int32_t shares[SR_MAX_FACTIONS];
  int32_t shock[SR_MAX_FACTIONS]; /* pending event impact in per-mille */
  int trade_count;
  SrTrade trades[SR_MAX_TRADES];
  int suspicion_count;
  SrSuspicion suspicion[SR_MAX_SUSPICION];
} SrMarket;

/* ---- news ------------------------------------------------------------------------------------ */

/* News kinds beyond the world event types. */
#define SR_NEWS_ELECTION 32
#define SR_NEWS_POLICY   33
#define SR_NEWS_PLANTED  34
#define SR_NEWS_SCANDAL  35
#define SR_NEWS_STORY    36
#define SR_NEWS_RESIGN   37

typedef struct SrNews {
  int64_t time;
  int8_t outlet;  /* media faction, -1 the independent pirate feed */
  int8_t subject; /* faction the item is about, -1 none */
  int8_t district;
  int8_t perpetrator; /* -1 unknown, -2 the player, else faction */
  uint8_t kind;       /* SrEventType or SR_NEWS_* */
  bool scandal;
  char headline[64];
} SrNews;

typedef struct SrPendingStory {
  int64_t publish_at;
  int8_t outlet, subject;
  int8_t seat; /* council seat the story exposes, -1 none */
  bool planted, scandal;
  char headline[64];
} SrPendingStory;

typedef struct SrNewsDesk {
  int count; /* total published */
  SrNews items[SR_MAX_NEWS];
  int pending_count;
  SrPendingStory pending[SR_MAX_PENDING];
  int events_seen; /* events log index already considered */
} SrNewsDesk;

/* ---- story ----------------------------------------------------------------------------------- */

typedef enum SrAct { SR_ACT_PROLOGUE, SR_ACT_PAWN, SR_ACT_KNIGHT, SR_ACT_PROMOTION, SR_ACT_EPILOGUE } SrAct;

typedef enum SrArcType {
  SR_ARC_TAKEOVER,
  SR_ARC_PROTOTYPE,
  SR_ARC_ELECTION,
  SR_ARC_PROXY,
  SR_ARC_DEFECTOR,
  SR_ARC_SHORTAGE,
  SR_ARC_MOLE,
  SR_ARC_SCANDAL,
  SR_ARC_COUNT
} SrArcType;

typedef enum SrScenario { SR_SC_NONE, SR_SC_MERGER, SR_SC_COUP, SR_SC_PALL, SR_SC_GHOST } SrScenario;

typedef enum SrEnding {
  SR_END_NONE,
  SR_END_LOYAL,
  SR_END_DEFECTOR,
  SR_END_BURN,
  SR_END_KINGMAKER,
  SR_END_SUNRISE
} SrEnding;

typedef enum SrCastRole {
  SR_CAST_LANDLORD,
  SR_CAST_FIXER,
  SR_CAST_NEIGHBOUR,
  SR_CAST_HANDLER,
  SR_CAST_RIVAL,
  SR_CAST_JOURNALIST,
  SR_CAST_GHOST,
  SR_CAST_COUNT
} SrCastRole;

typedef struct SrCast {
  char name[SR_NAME_LEN];
  uint8_t generation; /* successors so far */
  bool alive;
  int64_t vacant_since; /* role empty since (0 = filled) */
} SrCast;

typedef struct SrArc {
  uint8_t type;
  bool active;
  int8_t a, b;      /* the two factions the arc is about */
  int32_t building; /* the place it centres on */
  uint8_t beat, beats;
  uint8_t path;   /* bit per beat: 1 = took the success branch */
  int32_t op;     /* operation of the current beat */
  int32_t person; /* NPC the arc revolves around (an executive of b), -1 none */
  int64_t started;
} SrArc;

typedef struct SrStory {
  uint8_t act;
  int8_t patron;
  int8_t first_patron; /* the patron the campaign started with */
  int8_t rival_offer;  /* corp whose counter-offer is open, -1 */
  int8_t hostile;      /* former patron hunting the player, -1 */
  int trust;           /* 0..100 */
  bool retainer;
  int32_t test_job;
  int64_t offer_at; /* handler contact with the retainer offer, 0 none */
  bool offer_open;
  bool offer_told; /* the handler's message went out */
  int64_t act_started;
  uint8_t scenario;
  int8_t scenario_corp, scenario_other;
  uint8_t ending;
  bool sandbox;      /* the campaign ended; the city goes on */
  bool finale_ready; /* Act III climax reached: the ending choice is open */
  int64_t finale_at; /* Act III: the climax comes at the latest here */
  int64_t next_offer, next_counter, next_hunt;
  int hunts; /* operations a hostile patron ran against the player */
  int arcs_started, arcs_finished, beats_offscreen;
  int corps_worked[SR_MAX_FACTIONS]; /* jobs done per client in Act III */
  int leaks;                         /* stories planted against corps */
  int matrix_depth;                  /* nodes hacked in total (the Deep) */
  SrArc arcs[SR_MAX_ARCS];
  SrCast cast[SR_CAST_COUNT];
} SrStory;

/* ---- the whole corporate game ---------------------------------------------------------------- */

typedef struct SrChess {
  uint64_t seed;
  int64_t next_tick;
  int32_t next_op_id, next_contract_id;
  SrFactionSim f[SR_MAX_FACTIONS];
  int op_count;
  SrOperation ops[SR_MAX_OPS];
  int contract_count;
  SrContract contracts[SR_MAX_CONTRACTS];
  int secmod_count;
  SrSecMod secmods[SR_MAX_SECMODS];
  int8_t price_mod[SR_MAX_DISTRICTS]; /* percent */
  int8_t patrol_mod[SR_MAX_DISTRICTS];
  int change_count;
  SrOwnerChange changes[SR_MAX_CHANGES];
  int link_count;
  SrLink links[SR_MAX_LINKS];
  uint32_t turf_version; /* bumps when turf or gang identities change (graffiti) */
  SrPolitics pol;
  SrMarket market;
  SrNewsDesk news;
  SrStory story;
  int32_t street_cred; /* the player's, set by the game before advancing */
  int ticks;
  int history_count;
  char history[SR_MAX_HISTORY][72]; /* recent notable outcomes, for the soak review */
} SrChess;

/* Bounds of a resource. */
void sr_resource_bounds(SrResource r, int16_t *lo, int16_t *hi);
const char *sr_op_name(SrOpType t);
const char *sr_arc_name(SrArcType t);
const char *sr_ending_name(SrEnding e);
const char *sr_scenario_name(SrScenario s);
const char *sr_cast_role_name(SrCastRole r);

/* ---- board (sr_board.c) ---- */
void sr_chess_init(SrChess *ch, SrCity *c, uint64_t seed, int patron_choice, int64_t now);
/* One strategic tick: income, strategy, contracts, off-screen resolution, counterplay, gangs. */
void sr_chess_tick(SrChess *ch, SrCity *c, SrPopulation *pop, SrEventLog *log, int64_t now);
/* Runs strategic ticks up to `now`. */
void sr_chess_advance(SrChess *ch, SrCity *c, SrPopulation *pop, SrEventLog *log, int64_t now);
int sr_chess_start_op(SrChess *ch, SrCity *c, SrOpType type, int client, int target, int building, int district,
                      int64_t now);
SrOperation *sr_chess_op(SrChess *ch, int32_t id);
SrContract *sr_chess_contract(SrChess *ch, int32_t id);
SrContract *sr_chess_contract_for_job(SrChess *ch, int32_t job_id);
/* Resolves a contract (by the player or off-screen) and applies its step outcome. */
void sr_chess_resolve(SrChess *ch, SrCity *c, SrPopulation *pop, SrEventLog *log, SrContract *k, bool success,
                      bool by_player, int64_t now);
int sr_chess_security(const SrChess *ch, int building);  /* added facility security */
int sr_chess_price_pct(const SrChess *ch, int district); /* 100 = normal */
float sr_chess_dominance(const SrChess *ch, const SrCity *c, int *leader);
int sr_chess_gang_of(const SrCity *c, int district); /* controlling gang or -1 */
void sr_chess_reveal(SrChess *ch, int a, int b, SrLinkType t);
/* Reveals every link involving a faction (a stolen file from one of its hosts). */
int sr_chess_reveal_faction(SrChess *ch, int faction);
bool sr_chess_link_known(const SrChess *ch, int a, int b, SrLinkType t);
/* Re-applies recorded ownership, turf and gang changes to a freshly generated city (load). */
void sr_chess_apply_changes(const SrChess *ch, SrCity *c);
void sr_chess_note(SrChess *ch, int64_t now, const char *text);
uint64_t sr_chess_digest(const SrChess *ch);

/* ---- politics (sr_politics.c) ---- */
void sr_politics_init(SrPolitics *p, const SrCity *c, int64_t now);
void sr_politics_tick(SrChess *ch, SrCity *c, SrEventLog *log, int64_t now);
/* Vote share (0..1) of a candidate backed by `backer` against `rival` for a seat. */
float sr_politics_share(const SrChess *ch, const SrCity *c, int backer, int rival, int scandal);
void sr_politics_expose(SrChess *ch, const SrCity *c, int seat, int64_t now);
/* Holds an election for a seat (SR_COUNCIL_SEATS = the mayor) now; returns the winner's backer. */
int sr_politics_elect(SrChess *ch, const SrCity *c, int seat, int64_t now);
/* Seats a faction controls (backed or bought). */
int sr_politics_seats_of(const SrPolitics *p, int faction);
bool sr_politics_curfew_now(const SrPolitics *p, int district, int64_t t);
/* Faction handling police Heat: the city police or the contract holder. */
int sr_politics_police(const SrPolitics *p, const SrCity *c);

/* ---- market (sr_market.c) ---- */
void sr_market_init(SrMarket *m, const SrChess *ch, const SrCity *c);
void sr_market_tick(SrChess *ch, const SrCity *c);
int32_t sr_market_fee(int32_t value);
/* Buys (qty > 0) or sells/shorts (qty < 0); returns the cash change (negative when buying). */
int64_t sr_market_trade(SrChess *ch, int corp, int32_t qty, int64_t cash, int64_t now, bool *ok);
int64_t sr_market_portfolio(const SrMarket *m, const SrCity *c);
/* Called when the player takes part in an operation against `corp`: flags large recent trades. */
int sr_market_check_insider(SrChess *ch, const SrCity *c, int corp, int64_t now);

/* ---- news (sr_news.c) ---- */
void sr_news_tick(SrChess *ch, const SrCity *c, const SrEventLog *log, int64_t now);
const SrNews *sr_news_latest(const SrNewsDesk *d, int i); /* 0 newest */
int sr_news_size(const SrNewsDesk *d);
/* Headline a billboard in a district shows at time t (rotating), NULL when there is none. */
const char *sr_news_billboard(const SrNewsDesk *d, int district, int64_t t);
/* Plants a damaging story about a faction (seat >= 0: evidence of that councillor's bribery). */
void sr_news_plant(SrChess *ch, const SrCity *c, int subject, int seat, int64_t now);
/* Queues a story for later publication (investigations the player can kill). */
void sr_news_queue(SrChess *ch, int outlet, int subject, int seat, bool scandal, bool planted, const char *headline,
                   int64_t at);
/* Publishes pending stories that are due. */
void sr_news_pending_tick(SrChess *ch, const SrCity *c, int64_t now);
/* Kills the next pending story (about subject, or any when -1); returns true if one was killed. */
bool sr_news_kill(SrChess *ch, int subject);
/* Publishes an item; an owned outlet refuses its owner's scandals until an independent outlet ran
 * one (returns false then). Outlet -1 is the independent pirate feed. */
bool sr_news_publish(SrChess *ch, const SrCity *c, int outlet, int subject, int district, int perpetrator, int kind,
                     bool scandal, const char *headline, int64_t now);
bool sr_news_reported(const SrNewsDesk *d, int subject, bool scandal_only);
/* Whether an independent outlet (the pirate feed or an unowned station) reported a scandal about subject. */
bool sr_news_independent_scandal(const SrNewsDesk *d, const SrCity *c, int subject);

/* ---- story (sr_story.c) ---- */
#define SR_ACT2_MAX_S (12 * SR_WEEK_S) /* Act II lasts at most this long */
#define SR_FINALE_S   (2 * SR_WEEK_S)
#define SR_TRUST_ACT2 40
#define SR_TRUST_T4   60
#define SR_BETRAYAL   25
void sr_story_init(SrStory *s, const SrCity *c, uint64_t seed, int patron_choice);
void sr_story_trust(SrStory *s, int delta);
/* Story upkeep each strategic tick: arcs, acts, offers, succession, the endgame. */
void sr_story_tick(SrChess *ch, SrCity *c, const SrPopulation *pop, int64_t now);
int sr_story_arc_start(SrChess *ch, SrCity *c, const SrPopulation *pop, SrArcType t, int64_t now);
void sr_story_beat_done(SrChess *ch, SrCity *c, int arc, bool success, int64_t now);
/* The prologue test gig was completed: the handler calls within 2 days. */
void sr_story_test_done(SrStory *s, int64_t now);
void sr_story_accept_retainer(SrStory *s, int64_t now);
/* A job the player finished: trust for the patron's work, betrayal when attributed against it. */
void sr_story_job_done(SrChess *ch, SrCity *c, int client, int target, bool success, bool attributed, int64_t now);
void sr_story_defect(SrStory *s, const SrCity *c, int corp, int64_t now);
SrScenario sr_story_pick_scenario(const SrChess *ch, const SrCity *c, int leader, int *other);
/* Endings the player can choose at the climax (bit per SrEnding). */
unsigned sr_story_endings(const SrStory *s);
void sr_story_end(SrChess *ch, SrCity *c, SrEnding e, int64_t now);
void sr_story_cast_died(SrStory *s, SrCastRole r, int64_t now);
/* Fills vacant cast roles whose successor is due. Returns roles filled. */
int sr_story_succession(SrStory *s, uint64_t seed, int64_t now);
bool sr_story_t4_allowed(const SrStory *s, int street_cred_tier);

#endif
