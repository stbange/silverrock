/* Renderer front-end (pure C, no OpenGL): CPU meshes, procedural textures, the bitmap font,
 * district palettes, light selection, city chunk meshes with an LRU cache, characters, rain,
 * skyline, landmark logos, and the frame description the GL backend executes. */
#ifndef SR_RENDER_H
#define SR_RENDER_H

#include "core/sr_math.h"
#include "game/sr_game.h"

#define SR_TEX_SIZE 128
#define SR_TEX_LAYERS 32
#define SR_MAX_LIGHTS_PER_DRAW 8
#define SR_MAX_FRAME_LIGHTS 96
#define SR_MAX_DRAWS 256
#define SR_CHUNK_BLOCKS 4

/* texture array layers */
enum {
  SR_LAYER_WHITE = 0, SR_LAYER_CONCRETE, SR_LAYER_WIN_A, SR_LAYER_WIN_B, SR_LAYER_WIN_GLASS,
  SR_LAYER_BRICK, SR_LAYER_METAL, SR_LAYER_ASPHALT, SR_LAYER_SIDEWALK, SR_LAYER_SIGN,
  SR_LAYER_FONT, SR_LAYER_WATER, SR_LAYER_ICON_FIRST /* one icon per SrPoi */
};

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
  uint32_t gpu;    /* backend handle, 0 = not uploaded */
  bool dirty;
} SrMesh;

typedef struct SrColor { uint8_t r, g, b, a; } SrColor;

void sr_mesh_init(SrMesh *m);
void sr_mesh_free(SrMesh *m);
void sr_mesh_clear(SrMesh *m);
/* Adds a quad (4 vertices, 2 triangles). Corners in order a,b,c,d (counter-clockwise). */
void sr_mesh_quad(SrMesh *m, SrVec3 a, SrVec3 b, SrVec3 c, SrVec3 d, float u0, float v0,
                  float u1, float v1, SrColor col, int layer, int emissive, int ground);
/* Axis-aligned box with optional per-face UV tiling (u per metre). */
void sr_mesh_box(SrMesh *m, SrVec3 lo, SrVec3 hi, SrColor col, int layer, float tile,
                 int emissive);
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
  int capacity;      /* LRU capacity (desktop 128, mobile 48) */
  uint64_t tick;
  int built_this_frame;
  int evictions;
} SrChunkCache;

void sr_chunk_cache_init(SrChunkCache *cc, int capacity);
void sr_chunk_cache_free(SrChunkCache *cc);
/* Builds the mesh of one chunk (deterministic for the city). */
void sr_chunk_build(const SrCity *c, const SrDeltas *d, int cx, int cy, SrMesh *out);
/* Returns a ready chunk or NULL; builds at most `budget` chunks per frame. */
SrChunk *sr_chunk_get(SrChunkCache *cc, const SrCity *c, const SrDeltas *d, int cx, int cy,
                      int budget);
int sr_chunk_count(const SrCity *c); /* chunks per side */

/* Characters, rain, skyline, landmarks */
void sr_build_characters(const SrCrowd *cr, SrVec3 cam, float max_dist, SrMesh *out);
void sr_build_character(const SrAppearance *a, SrVec3 pos, SrVec3 dir, bool umbrella, SrMesh *out);
void sr_build_rain(SrVec3 cam, float intensity, uint64_t frame, const SrCity *c, SrMesh *out);
bool sr_point_covered(const SrCity *c, SrVec3 p);
void sr_build_skyline(const SrCity *c, SrMesh *out);
void sr_build_landmarks(const SrCity *c, float time_s, SrMesh *out);

/* Frame description */
typedef enum SrPass { SR_PASS_WORLD, SR_PASS_NOFOG } SrPass;

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
  int nlights;
  SrLight lights[SR_MAX_FRAME_LIGHTS];
  int ndraws;
  SrDraw draws[SR_MAX_DRAWS];
  int internal_w, internal_h;
  bool dither, quantize, vertex_snap, affine, crt;
} SrFrame;

typedef struct SrSceneCache {
  SrChunkCache chunks;
  SrMesh characters, rain, skyline, landmarks;
  bool skyline_built;
  uint64_t frame_no;
  int view_chunks; /* radius in chunks */
} SrSceneCache;

void sr_scene_init(SrSceneCache *s, bool mobile);
void sr_scene_free(SrSceneCache *s);
/* Fills the frame for the game's current state. */
void sr_scene_build(SrSceneCache *s, const SrGame *g, float aspect, SrFrame *f);
/* City lights near the camera (neon signs, landmark logos). */
int sr_collect_lights(const SrCity *c, SrVec3 cam, float radius, SrLight *out, int max);

/* Fog factor 0..1 (1 = fully fogged); emissive surfaces glow through. Mirrors the shader. */
float sr_fog_factor(float view_z, float fog_start, float fog_end, float emissive);

/* View math */
typedef struct SrViewport { int x, y, w, h; } SrViewport;
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
