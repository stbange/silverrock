/* Renderer front-end (pure C, no OpenGL): CPU meshes, procedural textures, the bitmap font,
 * district palettes, light selection, city chunk meshes with an LRU cache, characters, rain,
 * skyline, landmark logos, and the frame description the GL backend executes. */
#ifndef SR_RENDER_H
#define SR_RENDER_H

#include "core/sr_math.h"
#include "game/sr_game.h"

#define SR_TEX_SIZE            128
#define SR_TEX_LAYERS          64
#define SR_MAX_LIGHTS_PER_DRAW 8
#define SR_MAX_FRAME_LIGHTS    96
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
  SR_LAYER_RESERVED = 48      /* 48..63 free for M6 */
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
 * 0/1 legs, 2 torso, 3/4 arms, 5 head) and per-instance data of SR_INST_FLOATS floats:
 * x y z yaw | swing width height 0 | outfit rgb 0 | skin rgb 0. */
#define SR_INST_FLOATS  16
#define SR_CROWD_NEAR_M 28.0f
void sr_build_instance_body(SrMesh *out);
/* Near citizens as detailed meshes into `near`, far ones as instances; returns the instance count. */
int sr_build_crowd_lod(const SrCrowd *cr, SrVec3 cam, float near_m, float far_m, SrMesh *near, float *inst, int max);

/* Characters, rain, skyline, landmarks */
void sr_build_characters(const SrCrowd *cr, SrVec3 cam, float max_dist, SrMesh *out);
/* Aerodynes on their sky lanes near the camera (time in real seconds). */
void sr_build_aerodynes(const SrAirTraffic *air, double t, SrVec3 cam, float radius, SrMesh *out);
/* Appends the vehicles within range to a mesh. */
void sr_build_vehicles(const SrCrowd *cr, SrVec3 cam, float max_dist, SrMesh *out);
void sr_build_character(const SrAppearance *a, SrVec3 pos, SrVec3 dir, bool umbrella, SrMesh *out);
/* Character facing `dir`; walk >= 0 is the distance walked and swings legs and arms (walk cycle). */
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
void sr_build_guards(const SrSite *s, SrMesh *out);
int sr_site_collect_lights(const SrSite *s, SrLight *out, int max);
/* Thermal vision: recolours vertices from `from` as glowing heat signatures. */
void sr_mesh_thermal(SrMesh *m, int from_vertex);
void sr_build_rain(SrVec3 cam, float intensity, uint64_t frame, const SrCity *c, SrMesh *out);
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
  SR_PASS_SKY /* drawn first without depth writes, fully fogged: the sky dome takes the fog colour */
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
  int halo_count;      /* neon halos drawn beyond the fog end this frame */
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
  SrBeacon *beacons;
  int nbeacons;
  uint64_t sky_seed;
  SrColor deck_light[SR_MAX_BLOCKS]; /* neon glow each block throws up at the clouds */
  SrVec3 sil_at;                     /* where the silhouettes were built */
  float sil_fog_end;
} SrSceneCache;

/* The gloom (sr_sky_mesh.c): sky dome, the Pall's cloud deck, halos through fog, silhouettes. */
void sr_sky_prepare(SrSceneCache *s, const SrCity *c);
void sr_build_sky_dome(SrVec3 eye, SrMesh *out);
/* Cloud deck lit by the districts below at night, slate by day, warmed by dawn and dusk. */
void sr_build_deck(const SrSceneCache *s, const SrCity *c, SrVec3 eye, const SrSky *sky, const float fog[3],
                   float time_s, SrMesh *base, SrMesh *glow);
/* Halos of beacons between the fog end and three visibility distances; returns how many. */
int sr_build_halos(const SrBeacon *b, int n, const SrBillboard *bb, float fog_end, float visibility, SrMesh *out);
/* Faint halos around lights inside the fog (lamps and signs in wet air). */
void sr_build_near_halos(const SrLight *l, int n, const SrBillboard *bb, float wet_air, SrMesh *out);
/* Tall buildings just beyond the fog end as flat shapes one shade darker than the fog. */
void sr_build_silhouettes(const SrCity *c, SrVec3 eye, float fog_end, const float fog[3], SrMesh *out);
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
