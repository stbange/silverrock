/* OpenGL backend: executes SrFrame and SrUi with one GLSL source for OpenGL 3.3 Core and
 * OpenGL ES 3.0 / WebGL 2. Renders into a low-resolution framebuffer, then a post pass applies
 * dithering, colour quantisation and the optional CRT effect while upscaling (nearest). */
#ifndef SR_GL_H
#define SR_GL_H

#include "render/sr_render.h"
#include "ui/sr_ui.h"

#define SR_GL_MAX_MESHES 512

enum { SR_GL_PROG_WORLD = 0, SR_GL_PROG_POST = 1, SR_GL_PROG_BLUR = 2 };

typedef struct SrGlMesh {
  unsigned vao, vbo, ibo;
  int count;
  bool used;
  const void *owner;  /* the SrMesh this slot holds; a mesh only uses a slot it owns */
  unsigned last_used; /* frame number, for reclaiming slots of meshes that are gone */
} SrGlMesh;

typedef struct SrGlBackend {
  bool ready;
  bool gles;
  unsigned generation;
  unsigned prog_world, prog_post, prog_blur;
  unsigned bloom_fbo[2], bloom_tex[2];
  int bloom_w, bloom_h;
  unsigned tex_array;
  unsigned fbo, fbo_color, fbo_depth;
  int fbo_w, fbo_h;
  unsigned quad_vao, quad_vbo;
  SrGlMesh meshes[SR_GL_MAX_MESHES];
  unsigned frame;
  int reclaimed; /* slots taken over from meshes that are gone */
  int draw_calls;
  int triangles;
  char error[256];
  /* uniform locations */
  int u_mvp, u_view, u_lpos, u_lcol, u_nl, u_amb, u_wet, u_snap, u_affine, u_tex, u_fog, u_fogr;
  int p_tex, p_dither, p_quant, p_crt, p_res, p_bloom, p_bloom_k;
  int b_tex, b_dir, b_bright;
} SrGlBackend;

/* Loads GL entry points through a getter (SDL_GL_GetProcAddress); NULL on the web. */
typedef void (*SrGlFn)(void);
typedef SrGlFn (*SrGlGetProc)(const char *name);
bool sr_gl_load(SrGlGetProc get);
/* Shader source for the dialect: preamble + body. prog: SR_GL_PROG_WORLD, _POST or _BLUR. */
void sr_gl_shader_source(bool gles, bool fragment, int prog, char *out, size_t cap);

bool sr_glb_init(SrGlBackend *b, bool gles);
void sr_glb_shutdown(SrGlBackend *b);
/* Forget every GPU object after a lost context (handles are invalid, nothing is deleted). */
void sr_glb_context_lost(SrGlBackend *b);
/* Renders the frame at internal resolution, the UI at window resolution, and presents. */
void sr_glb_render(SrGlBackend *b, const SrFrame *f, SrUi *ui, int win_w, int win_h, bool integer_scale);
/* Reads the internal framebuffer (RGB, bottom-up rows flipped to top-down). */
bool sr_glb_read_internal(SrGlBackend *b, uint8_t *rgb, int w, int h);
bool sr_glb_read_window(SrGlBackend *b, uint8_t *rgb, int x, int y, int w, int h);

#endif
