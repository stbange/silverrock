/* Renderer front-end (pure C, no OpenGL): CPU meshes, procedural textures, the bitmap font,
 * district palettes, light selection, city chunk meshes with an LRU cache, characters, rain,
 * skyline, landmark logos, and the frame description the GL backend executes. */
#ifndef SR_RENDER_H
#define SR_RENDER_H

#include "core/sr_math.h"
#include "game/sr_game.h"
#include "render/sr_anim.h"
#include "world/sr_dressing.h"

#define SR_TEX_SIZE            128
#define SR_TEX_LAYERS          64
#define SR_MAX_LIGHTS_PER_DRAW 8
#define SR_MAX_FRAME_LIGHTS    160
#define SR_BLOCK_LIGHTS        32 /* lights of one block in the light grid */
#define SR_MAX_DRAWS           256
#define SR_CHUNK_BLOCKS        4

/* texture array layers */
enum {
  SR_LAYER_WHITE = 0,
  SR_LAYER_CONCRETE,
  SR_LAYER_WIN_A,
  SR_LAYER_WIN_B,
  SR_LAYER_WIN_GLASS,
  SR_LAYER_BRICK,
  SR_LAYER_METAL,
  SR_LAYER_ASPHALT,
  SR_LAYER_SIDEWALK,
  SR_LAYER_SIGN,
  SR_LAYER_FONT,
  SR_LAYER_WATER,
  SR_LAYER_WINMOD_FIRST = 12, /* 8 window modules (sr_facade): office, balcony, hive, curtain, ... */
  SR_LAYER_BASE_FIRST = 20,   /* 6 ground-floor modules: shopfront, shutter, lobby, dock, stall, posters */
  SR_LAYER_TRIM_FIRST = 26,   /* 6 roof and trim materials: tiles, corrugated, parapet, cornice, gravel, helipad */
  SR_LAYER_ICONS = 32,        /* 2 atlas layers: one 32 px icon per SrPoi */
  SR_LAYER_SPRITES = 34,      /* 4 atlas layers of 16 sprites (SrSpriteId) */
  SR_LAYER_DECALS = 38,       /* 4 atlas layers of 16 decals (SrDecalId) */
  SR_LAYER_ADS = 42,          /* 4 layers of billboard and poster content */
  SR_LAYER_SCRIPT = 46,       /* 64 neon script glyphs */
  SR_LAYER_NOISE = 47,        /* cloud noise for the Pall */
  SR_LAYER_FACES = 48,        /* 4 atlas layers: 8 face styles (4 masculine, 4 feminine) x 8 expressions */
  SR_LAYER_RESERVED = 52      /* 52..63 free for M6 */
};

/* A cell of a 4x4 atlas layer (32 px cells); v0 is the top edge, v1 the bottom. */
typedef struct SrAtlasCell {
  int layer;
  float u0, v0, u1, v1;
} SrAtlasCell;
SrAtlasCell sr_atlas_cell(int first_layer, int cell);
SrAtlasCell sr_icon_cell(int poi);

/* Sprites of the sprite atlas (16 per layer). */
typedef enum SrSpriteId {
  SR_SPR_GLOW = 0, /* soft round glow */
  SR_SPR_POOL,     /* light pool on the ground */
  SR_SPR_HALO,     /* soft halo of a light in wet air */
  SR_SPR_CONE,     /* searchlight or lamp cone */
  SR_SPR_STEAM,    /* 4 frames */
  SR_SPR_SPLASH = SR_SPR_STEAM + 4,
  SR_SPR_SMOKE = SR_SPR_SPLASH + 4,
  SR_SPR_SPARK = SR_SPR_SMOKE + 4, /* 4 frames */
  SR_SPR_DRIP = SR_SPR_SPARK + 4,
  SR_SPR_LITTER,                   /* 4 variants */
  SR_SPR_BIRD = SR_SPR_LITTER + 4, /* 2 frames */
  SR_SPR_DRONE = SR_SPR_BIRD + 2,
  SR_SPR_FLARE,
  SR_SPR_RIPPLE,                     /* 3 frames */
  SR_SPR_STREAK = SR_SPR_RIPPLE + 3, /* reflection streak */
  SR_SPR_HOLO,                       /* holo-ad frame */
  SR_SPR_TREE,                       /* glowing tree */
  SR_SPR_COUNT
} SrSpriteId;

/* Decals of the decal atlas (flat on the ground or a wall). */
typedef enum SrDecalId {
  SR_DECAL_MANHOLE = 0,
  SR_DECAL_DRAIN,
  SR_DECAL_CROSSWALK,
  SR_DECAL_ARROW,
  SR_DECAL_CRACK, /* 2 variants */
  SR_DECAL_STAIN = SR_DECAL_CRACK + 2,
  SR_DECAL_PUDDLE = SR_DECAL_STAIN + 2,
  SR_DECAL_STOPLINE = SR_DECAL_PUDDLE + 2,
  SR_DECAL_TAG,                       /* 6 graffiti tags */
  SR_DECAL_POSTER = SR_DECAL_TAG + 6, /* 4 posters */
  SR_DECAL_WANTED = SR_DECAL_POSTER + 4,
  SR_DECAL_COUNT
} SrDecalId;

/* Advert layouts: 2x2 cells of 64 px per layer (8 layouts, vandalism overlays, LED strips). */
enum { SR_AD_LAYOUTS = 8, SR_AD_VANDAL = 8, SR_AD_LED = 12, SR_AD_BULBS = 13, SR_AD_PLAIN = 14 };
SrAtlasCell sr_ad_cell(int first_layer, int cell);
/* One of the 64 glyphs of the neon script. */
SrAtlasCell sr_script_cell(int glyph);

typedef struct SrVertex {
  float x, y, z;
  float u, v;
  uint8_t r, g, b, a;
  uint8_t layer;
  uint8_t emissive; /* 0..255 */
  uint8_t ground;   /* 1 for wet-capable ground surfaces */
  uint8_t pad;
} SrVertex;

typedef struct SrMesh {
  SrVertex *v;
  uint32_t *idx;
  int nv, ni;
  int cap_v, cap_i;
  uint32_t gpu; /* backend handle, 0 = not uploaded */
  bool dirty;
} SrMesh;

typedef struct SrColor {
  uint8_t r, g, b, a;
} SrColor;

void sr_mesh_init(SrMesh *m);
void sr_mesh_free(SrMesh *m);
void sr_mesh_clear(SrMesh *m);
/* Adds a quad (4 vertices, 2 triangles). Corners in order a,b,c,d (counter-clockwise). */
void sr_mesh_quad(SrMesh *m, SrVec3 a, SrVec3 b, SrVec3 c, SrVec3 d, float u0, float v0, float u1, float v1,
                  SrColor col, int layer, int emissive, int ground);
/* Axis-aligned box with optional per-face UV tiling (u per metre). */
void sr_mesh_box(SrMesh *m, SrVec3 lo, SrVec3 hi, SrColor col, int layer, float tile, int emissive);
uint64_t sr_mesh_digest(const SrMesh *m);

/* Procedural textures: fills SR_TEX_SIZE^2 RGBA pixels for a layer, deterministic. */
void sr_texgen_layer(int layer, uint8_t *rgba);
uint64_t sr_texgen_digest(void);
/* 5x7 glyph columns for ASCII 32..126; bit 0 is the top row. */
const uint8_t *sr_font_glyph(char ch);
/* Places the font into the font layer: glyph i at cell (i % 16, i / 16), 8x8 px cells. */
void sr_font_cell(char ch, int *cx, int *cy);

/* Palettes */
typedef struct SrPalette {
  SrColor fog;
  SrColor ambient;
  SrColor neon[3];
  SrColor ground;
} SrPalette;
const SrPalette *sr_palette(SrDistrictType t);
SrColor sr_corp_color(int hue);
float sr_color_distance(SrColor a, SrColor b);

/* Lights */
typedef struct SrLight {
  SrVec3 pos;
  float radius;
  float r, g, b;
} SrLight;
/* Influence-ordered selection of up to SR_MAX_LIGHTS_PER_DRAW lights for a point. */
int sr_select_lights(const SrLight *lights, int n, SrVec3 at, int *out_idx);

/* City chunks */
typedef struct SrChunk {
  int cx, cy;
  SrMesh mesh;
  uint64_t last_used;
  uint64_t digest;
  bool valid;
} SrChunk;

#define SR_CHUNK_CACHE 128

typedef struct SrChunkCache {
  SrChunk chunks[SR_CHUNK_CACHE];
  int capacity; /* LRU capacity (desktop 128, mobile 48) */
  uint64_t tick;
  int built_this_frame;
  int evictions;
} SrChunkCache;

void sr_chunk_cache_init(SrChunkCache *cc, int capacity);
void sr_chunk_cache_free(SrChunkCache *cc);
/* Builds the mesh of one chunk (deterministic for the city). */
void sr_chunk_build(const SrCity *c, const SrDeltas *d, int cx, int cy, SrMesh *out);
/* Returns a ready chunk or NULL; builds at most `budget` chunks per frame. */
SrChunk *sr_chunk_get(SrChunkCache *cc, const SrCity *c, const SrDeltas *d, int cx, int cy, int budget);
int sr_chunk_count(const SrCity *c); /* chunks per side */

/* Rotated box for rigid parts (see sr_mesh.c). */
void sr_mesh_part(SrMesh *m, SrVec3 origin, float yaw, SrVec3 pivot, float swing, SrVec3 lo, SrVec3 hi, SrColor col,
                  int layer, int em);

/* Instanced crowd (far level of detail): a unit body whose vertices carry a part id (pad byte:
 * 0/1 thighs, 2 torso, 3/4 upper arms, 5 head, 6/7 shins, 8/9 forearms) and per-instance data of
 * SR_INST_FLOATS floats: x y z yaw | phase width height clip | outfit rgb speed | skin rgb hunch.
 * Clip codes: 0 stand, 1 walk or run (by speed), 2 sit, 3 crouch, 4 lying. The hunch slot adds 2
 * when a phone or cigarette glows in the hand. */
#define SR_INST_FLOATS  16
#define SR_CROWD_NEAR_M 28.0f
void sr_build_instance_body(SrMesh *out);

/* What the crowd builders need from the frame. */
typedef struct SrCrowdCtx {
  double now; /* game seconds */
  float rain, wet;
  SrVec3 wind; /* m/s */
  int heat;    /* highest Heat */
  SrVec3 player, eye;
  int max_near; /* near skeletons this frame (budget) */
  SrMesh *fx;   /* glow-pass mesh for footfall splashes (may be NULL) */
  int splashes, max_splashes;
} SrCrowdCtx;
/* Animation input of a citizen: clip from the state and activity, props, rain, police stance. */
void sr_agent_anim(const SrAgent *a, const SrCrowdCtx *x, SrAnimInput *out);
/* Index of the member of a talking group who speaks at `now` (one at a time). */
int sr_group_speaker(uint64_t group, int n, double now);
/* Near citizens as animated meshes into `near`, far ones as instances; returns the instance count. */
int sr_build_crowd_lod(const SrCrowd *cr, SrAnimCache *anim, const SrCrowdCtx *x, SrVec3 cam, float near_m, float far_m,
                       SrMesh *near, float *inst, int max);

/* Characters, rain, skyline, landmarks */
void sr_build_characters(const SrCrowd *cr, SrAnimCache *anim, const SrCrowdCtx *x, SrVec3 cam, float max_dist,
                         SrMesh *out);
/* Aerodynes on their sky lanes near the camera, banking and bobbing, and those landing on helipads
 * within 200 m (time in seconds); thruster glows go into fx (may be NULL). */
void sr_build_aerodynes(const SrAirTraffic *air, const SrCity *c, double t, SrVec3 cam, float radius, SrMesh *out,
                        SrMesh *fx);
/* Appends the vehicles within range to a mesh: body, cabin, turning wheels, suspension, lights. */
void sr_build_vehicles(const SrCrowd *cr, const SrCrowdCtx *x, SrVec3 cam, float max_dist, SrMesh *out);
/* Wheel rotation (radians) from the distance driven. */
float sr_wheel_angle(const SrVehicle *v);
/* Headlight beams of cars within 60 m (at most `max`) and spray behind fast wheels; returns beams. */
int sr_build_headlights(const SrCrowd *cr, SrVec3 eye, float wet, uint64_t steps, int max, SrMesh *fx);
/* The route in the world (sr_nav_mesh.c): chevrons on the walking line along the next 60 m (into
 * chevrons, may be NULL) and a shaft of light over the destination (into beacon, may be NULL).
 * Returns the pieces drawn. */
int sr_build_route_ar(const SrRoute *r, SrVec3 eye, double t, SrMesh *chevrons, SrMesh *beacon);
void sr_build_character(const SrAppearance *a, SrVec3 pos, SrVec3 dir, bool umbrella, SrMesh *out);
/* Character facing `dir`; walk >= 0 is the distance walked and drives the walk cycle. */
void sr_build_character_walk(const SrAppearance *a, SrVec3 pos, SrVec3 dir, float walk, bool umbrella, SrMesh *out);
/* Leg swing angle for a walked distance (radians). */
float sr_walk_swing(float walk);

/* The Matrix (sr_matrix_mesh.c) */
void sr_mesh_wire_box(SrMesh *m, SrVec3 lo, SrVec3 hi, SrColor c);
void sr_build_grid(const SrCity *c, SrMesh *out);
void sr_build_host(const SrGame *g, float time_s, SrMesh *out);

/* Interiors (sr_interior_mesh.c) */
void sr_build_interior_room(const SrSite *s, int room, SrMesh *out);
uint64_t sr_site_mesh_key(const SrSite *s, const SrDeltas *d);
void sr_build_site_dynamic(const SrSite *s, const SrDeltas *d, float time_s, SrMesh *out);
/* Guards posed by their alert state, hits and falls (time in game seconds; the cache may be NULL). */
void sr_build_guards(const SrSite *s, SrAnimCache *anim, double now, SrVec3 player, SrMesh *out);
/* Door leaves swinging toward their state over 0.3 s; angle holds 2 per room (0 shut, 1 open). */
void sr_build_doors(const SrSite *s, float *angle, float dt, bool snap, SrMesh *out);
/* Animation input of one guard (now in site seconds). */
void sr_guard_anim(const SrSite *s, const SrGuard *g, const SrAppearance *a, double now, SrVec3 player,
                   SrAnimInput *in);
/* Muzzle flashes, tracers and flash lights of guards that just fired; returns the light count. */
int sr_build_enemy_fire(const SrSite *s, double now, SrVec3 player, SrVec3 eye, SrMesh *fx, SrLight *lights, int nl,
                        int max_lights);
int sr_site_collect_lights(const SrSite *s, SrLight *out, int max);
/* The player's body (sr_viewmodel.c): the camera with head bob, landing dip, eased crouch, recoil,
 * flinch and shake, and the hands and weapon. */
typedef struct SrViewCam {
  SrVec3 eye;
  float yaw, pitch;
  float bob_x, bob_y; /* head bob offsets this frame (metres) */
} SrViewCam;
SrViewCam sr_view_camera(const SrGame *g);
/* Weapon category of the equipped weapon (SR_WC_*), -1 unarmed. */
int sr_view_category(const SrGame *g);
/* Builds the view model in world space; returns its triangle count (0 when nothing is shown). */
int sr_build_view_model(const SrGame *g, SrViewCam c, float sway_x, float sway_y, SrMesh *out);
/* Muzzle position while a shot's flash lights the room (about two frames). */
bool sr_view_muzzle(const SrGame *g, SrViewCam c, float sway_x, float sway_y, SrVec3 *pos);
/* The muzzle flash and the ejected shell of the last shot (glow pass). */
void sr_build_muzzle_fx(const SrGame *g, SrViewCam c, float sway_x, float sway_y, SrMesh *fx);
/* Strength 0..1 of the red edge after a hit, and the hit's direction relative to the view. */
float sr_view_hurt_edge(const SrGame *g, float *dir, bool *known);
/* Thermal vision: recolours vertices from `from` as glowing heat signatures. */
void sr_mesh_thermal(SrMesh *m, int from_vertex);
void sr_build_rain(SrVec3 cam, float intensity, uint64_t frame, const SrCity *c, SrMesh *out);
/* Rain tinted by the lights around it, in the weather's colour (acid rain is green). */
void sr_build_rain_lit(SrVec3 cam, float intensity, uint64_t frame, const SrCity *c, const SrLight *l, int nl,
                       SrColor base, SrMesh *out);
bool sr_point_covered(const SrCity *c, SrVec3 p);
void sr_build_skyline(const SrCity *c, SrMesh *out);
void sr_build_landmarks(const SrCity *c, float time_s, SrMesh *out);

/* Sprites (sr_sprite.c): quads that face the camera or lie flat. */
typedef struct SrBillboard {
  SrVec3 eye, right, up;
} SrBillboard;
SrBillboard sr_billboard(SrVec3 eye, float yaw, float pitch);
typedef enum SrSpriteMode {
  SR_SPRITE_UPRIGHT, /* turns about the vertical axis; pos is the bottom centre */
  SR_SPRITE_FACING,  /* faces the camera fully; pos is the centre */
  SR_SPRITE_DECAL    /* lies flat at pos.y, turned by yaw; pos is the centre */
} SrSpriteMode;
void sr_mesh_sprite(SrMesh *m, const SrBillboard *bb, SrVec3 pos, float w, float h, int sprite, SrSpriteMode mode,
                    float yaw, SrColor col, int emissive);
/* Animation frame for game time t (deterministic). */
int sr_anim_frame(double t, float fps, int frames, uint32_t seed);
/* Fog factor with ground mist added below 3 m (mirrors the shader). */
float sr_fog_with_mist(float fog, float mist, float y, float view_z);
/* Building use for window lighting (vertex pad byte: use << 6 | seed). */
enum { SR_USE_OFFICE = 0, SR_USE_HOME, SR_USE_INDUSTRIAL, SR_USE_VACANT };
/* Share of lit windows for a building use at an hour (0..24). Mirrored by the shader via u_share. */
float sr_window_share(int usage, float hour);
/* How much of a glow survives the fog at view depth z (1 near, 0 beyond the fog end). Mirrors the shader. */
float sr_glow_fade(float view_z, float fog_start, float fog_end);

/* Frame description */
/* XRAY: no depth test and no fog (thermal signatures through walls and smoke).
 * GLOW: additive, no depth write, faded by fog. HALO: additive, no depth write, no fog (lights
 * seen through fog, their strength is set on the CPU). */
typedef enum SrPass {
  SR_PASS_WORLD,
  SR_PASS_NOFOG,
  SR_PASS_XRAY,
  SR_PASS_GLOW,
  SR_PASS_HALO,
  SR_PASS_SKY, /* drawn first without depth writes, fully fogged: the sky dome takes the fog colour */
  SR_PASS_FAR, /* the distant city: fog stretched to three and a half times the fog end */
  SR_PASS_VIEW /* the player's hands and weapon: last, over a cleared depth buffer, no fog */
} SrPass;

typedef struct SrDraw {
  SrMesh *mesh;
  SrPass pass;
  int nlights;
  int lights[SR_MAX_LIGHTS_PER_DRAW];
} SrDraw;

typedef struct SrFrame {
  SrMat4 view, proj;
  SrVec3 eye;
  float fog_start, fog_end;
  float fog[3];
  float ambient[3];
  float sky[3];
  float wetness;
  bool daylight;
  float time_s;       /* game time of day in seconds (animation) */
  float hour;         /* 0..24 */
  float win_share[4]; /* lit window share per building use (sr_window_share) */
  float band[3];      /* dawn or dusk glow on the fog toward band_dir_x (+1 east, -1 west) */
  float band_dir_x;
  int halo_count;                     /* neon halos drawn beyond the fog end this frame */
  float mist;                         /* extra fog below 3 m (Undercity, Docks, foggy weather) */
  float grade_lift[3], grade_gain[3]; /* the district's colour grade (post pass) */
  int brightness;                     /* -2..2 */
  bool pixel_lighting;                /* per-pixel light for the ground and lower facades */
  bool vignette;
  bool debug_emissive; /* tests: draw emissive surfaces white, everything else black */
  int nlights;
  SrLight lights[SR_MAX_FRAME_LIGHTS];
  int ndraws;
  SrDraw draws[SR_MAX_DRAWS];
  int internal_w, internal_h;
  bool dither, quantize, vertex_snap, affine, crt, bloom;
  /* instanced crowd: far citizens drawn from one unit body */
  const SrMesh *inst_mesh;
  const float *inst;
  int inst_count;
  /* meat-cam during a dive: a second, small frame of the body's surroundings */
  const struct SrFrame *inset;
  bool inset_fresh; /* rebuilt this frame (it is refreshed a few times a second) */
} SrFrame;

/* Light grid: every city light (street lamps, neon signs) precomputed and binned per block, so
 * the lights near the camera are gathered block ring by block ring, nearest first. */
typedef struct SrLightGrid {
  uint64_t city_seed;
  int blocks;
  uint16_t start[SR_MAX_BLOCKS + 1];
  int count;
  SrLight *lights;
} SrLightGrid;

void sr_light_grid_build(SrLightGrid *lg, const SrCity *c);
void sr_light_grid_free(SrLightGrid *lg);
int sr_light_grid_query(const SrLightGrid *lg, const SrCity *c, SrVec3 cam, float radius, SrLight *out, int max);

/* Adverts follow the corporate board (sr_sign_mesh.c). */
typedef struct SrAdContext {
  const SrCity *c;
  int64_t now;
  uint32_t scandal_mask;               /* corps with a scandal in the last 7 days */
  int leader;                          /* corp above 25 % of megacorp resources, or -1 */
  uint32_t owners[SR_MAX_DISTRICTS];   /* corps owning a facility in each district */
  int8_t main_owner[SR_MAX_DISTRICTS]; /* corp owning the most there, or -1 */
  uint64_t key;                        /* changes whenever an advert would change */
} SrAdContext;
typedef struct SrAd {
  int corp;
  bool vandal, empty;
} SrAd;
/* A large light seen from afar through the fog (logos, beacons, signs, billboards). */
typedef struct SrBeacon {
  SrVec3 pos;
  float size;
  SrColor col;
} SrBeacon;

typedef struct SrSceneCache {
  SrLightGrid light_grid;
  SrChunkCache chunks;
  SrMesh characters, rain, skyline, landmarks;
  SrMesh rooms[SR_INT_MAX_ROOMS], site_dyn, guards;
  SrMesh inst_body, air;
  SrMesh grid, host;
  uint64_t grid_seed;
  uint32_t turf_version; /* chunks carry gang graffiti: rebuilt when turf changes */
  float inst[SR_MAX_AGENTS * SR_INST_FLOATS];
  uint64_t site_key;
  bool skyline_built;
  uint64_t frame_no;
  int view_chunks; /* radius in chunks */
  /* the gloom (sr_sky_mesh.c) */
  SrMesh sky, deck, deck_glow, halos, near_glow, silhouettes, sun;
  SrMesh route_beacon; /* the destination's shaft of light (halo pass: seen through the fog) */
  SrMesh beams, blimps, blimp_glow, distant, skyline_glow;
  SrMesh vertical, trains; /* skyways and metro tracks (built once), trains (every frame) */
  SrMesh pools;            /* light pools and wall splashes (every frame) */
  SrMesh crowd_fx;         /* footfall splashes, headlight beams, car spray, muzzle flashes (glow pass) */
  SrAnimCache anim;        /* poses of citizens and guards (presentation only) */
  int near_drawn, beams_drawn, route_drawn; /* diagnostics for the budget test */
  SrMesh view_model;                        /* the player's hands and weapon */
  float door_angle[SR_INT_MAX_ROOMS * 2];
  int32_t door_building; /* the site the door angles belong to */
  float sway_x, sway_y;  /* the weapon's lag behind the look */
  int view_tris;
  struct {
    int block;
    uint64_t used;
    uint32_t turf;
    SrMesh mesh;
  } blocks[24]; /* per-block meshes of the camera's chunk, each drawn with its own lights */
  uint64_t block_tick;
  bool vertical_built;
  bool distant_built;
  SrBeacon *beacons;
  int nbeacons;
  uint64_t sky_seed;
  SrColor deck_light[SR_MAX_BLOCKS]; /* neon glow each block throws up at the clouds */
  SrVec3 sil_at;                     /* where the silhouettes were built */
  float sil_fog_end;
  /* wet streets (sr_wet_mesh.c) and the props of the blocks around the camera */
  SrMesh wet, fronts;
  struct {
    int block, detail;
    uint64_t used;
    SrMesh mesh;
  } street[32]; /* street meshes of the blocks around the camera */
  uint64_t street_tick;
  int street_detail; /* 0 low, 1 medium, 2 high */
  uint64_t street_seed;
  uint64_t ads_key;
  SrAdContext ads;
  struct {
    int block;
    int n;
    uint64_t used;
    SrStreetProp props[SR_MAX_BLOCK_PROPS];
  } prop_cache[16];
  uint64_t prop_tick;
  float mist;      /* ground mist this frame */
  int reflections; /* reflections drawn this frame */
} SrSceneCache;

/* Buildings (sr_facade_mesh.c). */
void sr_build_box_facade(SrMesh *m, const SrFacade *f, float x0, float z0, float x1, float z1, float ybot, float ytop,
                         bool with_base, uint8_t blank);
void sr_build_crown(SrMesh *m, const SrFacade *f, float x0, float z0, float x1, float z1, float h);
/* Lit fronts of open shops near the eye; shutters roll over about three real seconds at opening
 * and closing (at most two at once). Returns how many are rolling. */
int sr_build_poi_fronts(const SrCity *c, const SrDeltas *d, SrVec3 eye, int64_t t, int time_rate, SrMesh *out);
/* How far a shop's shutter is up: 0 shut, 1 open. */
float sr_shutter_open(const SrCity *c, int building, int64_t t, int time_rate);
void sr_build_door(SrMesh *m, float cx, float z, SrColor frame);
/* A whole building: volumes with facades, crowns, roof kits, landmark crowns. */
void sr_build_building(const SrCity *c, int building, SrColor neon, SrMesh *m);

/* Signs and adverts (sr_sign_mesh.c). */
void sr_ads_context(SrAdContext *a, const SrChess *ch, const SrCity *c, int64_t now);
SrAd sr_ad_for_slot(const SrAdContext *a, const SrBillboardSlot *s, int district);
void sr_product_name(const SrCity *c, int corp, uint32_t seed, char *out, size_t cap);
void sr_build_billboard(SrMesh *m, const SrCity *c, const SrBillboardSlot *s, SrAd ad);
void sr_mesh_text(SrMesh *m, SrVec3 o, SrVec3 right, SrVec3 up, float h, const char *s, SrColor col, int em);
float sr_text_width(const char *s, float h);
void sr_build_shop_name(SrMesh *m, const SrBuilding *b, SrColor neon, bool lit);
void sr_build_blade_signs(const SrCity *c, int building, SrMesh *m);
void sr_build_holo_ads(const SrCity *c, const SrAdContext *ads, SrVec3 eye, float time_s, SrMesh *out);
int sr_build_tickers(const SrCity *c, const char *headline, SrVec3 eye, float time_s, SrMesh *out);
int sr_build_wanted(const SrCity *c, int district, int heat, SrColor outfit, SrVec3 eye, SrMesh *out);
SrColor sr_outfit_color(const SrAppearance *a);
/* Clothing and skin colours of a character. */
SrColor sr_anim_cloth(const SrAppearance *a);
SrColor sr_anim_skin(const SrAppearance *a);
/* Colours of one arm (0 left, 1 right): hand and forearm (chrome, bare skin or sleeve) and the layer. */
void sr_anim_arm_look(const SrAppearance *a, int side, SrColor *hand, SrColor *fore, int *layer);

/* The street of a block (sr_street_mesh.c): kerbs, markings, props, overhead lines and billboards
 * (when ads is given). detail 0..2. */
void sr_build_block_props(const SrCity *c, int block, int detail, const SrAdContext *ads, SrMesh *out);
/* Lights of a block's lit props (lamps, fires, vending machines, booths, stalls). */
int sr_block_prop_lights(const SrCity *c, int block, SrLight *out, int max);
SrColor sr_lamp_color(SrDistrictType t);
/* How many of a block's props are drawn at a detail level (and how many of them carry a light). */
int sr_block_props_drawn(const SrCity *c, int block, int detail, int *lit);

/* The cached street mesh of a block at the current detail level. */
SrMesh *sr_scene_street(SrSceneCache *s, const SrCity *c, int block);
/* Vertical layers (sr_vertical_mesh.c). */
void sr_build_highway(const SrCity *c, int block, SrMesh *m);
void sr_build_highway_traffic(const SrCity *c, SrVec3 eye, float time_s, SrMesh *out);
void sr_build_skyways(const SrCity *c, SrMesh *m);
void sr_build_metro_tracks(const SrCity *c, SrMesh *m);
/* Where the train between a metro edge's stations is at time t, from the timetable. */
bool sr_metro_train_pos(const SrCity *c, int edge, int64_t t, SrVec3 *pos, SrVec3 *dir);
/* The same with the stop: 0 no train, 1 moving (braking into and pulling out of stations),
 * 2 standing at the station with its doors open. */
int sr_metro_train_state(const SrCity *c, int edge, int64_t t, SrVec3 *pos, SrVec3 *dir);
#define SR_TRAIN_DWELL_S 20
int sr_build_trains(const SrCity *c, int64_t t, SrVec3 eye, SrMesh *out);

/* Motion (sr_motion_mesh.c). */
float sr_neon_level(uint8_t pad, float t); /* brightness of a neon with that pad byte at time t (mirrors the shader) */
uint8_t sr_sign_pad(uint64_t seed, uint32_t id);
SrVec3 sr_wind(uint64_t seed, int64_t t, float rain);
bool sr_vehicle_is_patrol(const SrVehicle *v);
void sr_build_police_lights(const SrCrowd *cr, SrVec3 eye, float time_s, int heat, SrMesh *out);
int sr_build_flyers(const SrCity *c, SrVec3 eye, float time_s, SrMesh *out);
int sr_build_litter(const SrCity *c, SrVec3 eye, float time_s, SrVec3 wind, SrMesh *out);
void sr_build_sparks(const SrCity *c, SrVec3 eye, float time_s, SrMesh *out);
void sr_build_fans(const SrCity *c, SrVec3 eye, float time_s, SrMesh *out);

/* Lighting (sr_light_mesh.c). */
int sr_build_light_pools(const SrCity *c, const SrLight *l, int n, SrVec3 eye, SrMesh *out);
void sr_grade_at(const SrCity *c, float x, float z, float lift[3], float gain[3]);
float sr_brightness(float v, int level); /* mirrors the post shader */

/* Props of a block, cached (for sprites and lights around the camera). */
const SrStreetProp *sr_scene_props(SrSceneCache *s, const SrCity *c, int block, int *n);

/* Wet streets (sr_wet_mesh.c). */
int sr_build_reflections(const SrLight *l, int n, SrVec3 eye, float wetness, int max, SrMesh *out);
void sr_build_splashes(const SrCity *c, SrVec3 eye, float rain, uint64_t steps, SrMesh *out);
void sr_build_drips(const SrCity *c, SrVec3 eye, float rain, float time_s, SrMesh *out);
void sr_build_lamp_cones(const SrLight *l, int n, SrVec3 eye, float wet_air, SrMesh *out);
void sr_build_steam(const SrStreetProp *p, int n, SrVec3 eye, float rain, float time_s, float dist, SrMesh *out);
void sr_build_stack_smoke(const SrCity *c, SrVec3 eye, float time_s, SrMesh *out);
void sr_build_ripples(const SrStreetProp *p, int n, SrVec3 eye, float rain, float time_s, SrMesh *out);
/* Rain drop colour lit by the lights around it. */
SrColor sr_rain_tint(const SrLight *l, int n, SrVec3 p, SrColor base);

/* The gloom (sr_sky_mesh.c): sky dome, the Pall's cloud deck, halos through fog, silhouettes. */
void sr_sky_prepare(SrSceneCache *s, const SrCity *c);
void sr_build_sky_dome(SrVec3 eye, SrMesh *out);
/* Cloud deck lit by the districts below at night, slate by day, warmed by dawn and dusk. */
void sr_build_deck(const SrSceneCache *s, const SrCity *c, SrVec3 eye, const SrSky *sky, const float fog[3],
                   float time_s, int dark_district, SrMesh *base, SrMesh *glow);
/* Halos of beacons between the fog end and three visibility distances; returns how many. */
int sr_build_halos(const SrBeacon *b, int n, const SrBillboard *bb, float fog_end, float visibility, SrMesh *out);
/* Faint halos around lights inside the fog (lamps and signs in wet air). */
void sr_build_near_halos(const SrLight *l, int n, const SrBillboard *bb, float wet_air, SrMesh *out);
/* Tall buildings just beyond the fog end as flat shapes one shade darker than the fog. */
void sr_build_silhouettes(const SrCity *c, SrVec3 eye, float fog_end, const float fog[3], SrMesh *out);
/* Searchlights from the HQs up into the clouds, brighter for corps on alert; returns the beams. */
int sr_build_searchlights(const SrCity *c, SrVec3 eye, float time_s, float visibility, uint32_t alert_mask,
                          SrMesh *out);
/* Advertising blimps on slow loops just under the clouds (hull and screens, glows). */
void sr_build_blimps(const SrCity *c, const SrAdContext *ads, SrVec3 eye, float time_s, const float fog[3],
                     float fog_end, float visibility, SrMesh *body, SrMesh *glow);
/* One box per building with lit windows, for rooftops and clearer nights. */
void sr_build_distant_city(const SrCity *c, SrMesh *out);
/* The city's light in the clouds above the skyline. */
void sr_build_skyline_glow(const SrCity *c, SrMesh *out);
/* The sun of the Sunrise ending. */
void sr_build_sun(SrVec3 eye, const SrBillboard *bb, SrMesh *out);

void sr_scene_init(SrSceneCache *s, bool mobile);
void sr_scene_free(SrSceneCache *s);
/* The body's surroundings in meatspace, even during a dive (meat-cam). */
void sr_scene_build_body(SrSceneCache *s, const SrGame *g, float aspect, SrFrame *f);
/* Fills the frame for the game's current state. */
void sr_scene_build(SrSceneCache *s, const SrGame *g, float aspect, SrFrame *f);
/* City lights near the camera (neon signs, landmark logos). */
int sr_collect_lights(const SrCity *c, SrVec3 cam, float radius, SrLight *out, int max);

/* Fog factor 0..1 (1 = fully fogged); emissive surfaces glow through. Mirrors the shader. */
float sr_fog_factor(float view_z, float fog_start, float fog_end, float emissive);

/* View math */
typedef struct SrViewport {
  int x, y, w, h;
} SrViewport;
SrViewport sr_letterbox(int win_w, int win_h, int src_w, int src_h, bool integer_scale);
/* Projected size in pixels of an object of `size_m` at `dist_m` (vertical fov). */
float sr_projected_pixels(float size_m, float dist_m, float fovy, int screen_h);

typedef struct SrDynRes {
  int level;        /* 0 = preset, higher = lower resolution */
  float over_time;  /* seconds over budget */
  float under_time; /* seconds within budget */
} SrDynRes;
/* Feeds one frame time; returns true when the level changed. */
bool sr_dynres_update(SrDynRes *d, float frame_ms, float budget_ms, float dt);
void sr_dynres_apply(const SrDynRes *d, int *w, int *h);

#endif
