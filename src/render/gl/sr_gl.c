#include "render/gl/sr_gl.h"

#include "core/sr_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#define GLAPIENTRY_ GL_APIENTRY
#else
#include <SDL3/SDL_opengl.h>
#define GLAPIENTRY_ APIENTRY
#endif

#ifndef GL_TEXTURE_2D_ARRAY
#define GL_TEXTURE_2D_ARRAY 0x8C1A
#endif

/* ---- entry points ---------------------------------------------------------------------- */

#define SR_GL_FUNCS(X)                                                                                                 \
  X(void, Viewport, (GLint, GLint, GLsizei, GLsizei))                                                                  \
  X(void, ClearColor, (GLfloat, GLfloat, GLfloat, GLfloat))                                                            \
  X(void, Clear, (GLbitfield))                                                                                         \
  X(void, Enable, (GLenum))                                                                                            \
  X(void, Disable, (GLenum))                                                                                           \
  X(void, BlendFunc, (GLenum, GLenum))                                                                                 \
  X(void, DepthFunc, (GLenum))                                                                                         \
  X(void, DepthMask, (GLboolean))                                                                                      \
  X(void, GenVertexArrays, (GLsizei, GLuint *))                                                                        \
  X(void, BindVertexArray, (GLuint))                                                                                   \
  X(void, DeleteVertexArrays, (GLsizei, const GLuint *))                                                               \
  X(void, GenBuffers, (GLsizei, GLuint *))                                                                             \
  X(void, BindBuffer, (GLenum, GLuint))                                                                                \
  X(void, BufferData, (GLenum, GLsizeiptr, const void *, GLenum))                                                      \
  X(void, DeleteBuffers, (GLsizei, const GLuint *))                                                                    \
  X(void, VertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *))                              \
  X(void, EnableVertexAttribArray, (GLuint))                                                                           \
  X(GLuint, CreateShader, (GLenum))                                                                                    \
  X(void, ShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *))                                       \
  X(void, CompileShader, (GLuint))                                                                                     \
  X(void, GetShaderiv, (GLuint, GLenum, GLint *))                                                                      \
  X(void, GetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *))                                                    \
  X(GLuint, CreateProgram, (void))                                                                                     \
  X(void, AttachShader, (GLuint, GLuint))                                                                              \
  X(void, BindAttribLocation, (GLuint, GLuint, const GLchar *))                                                        \
  X(void, LinkProgram, (GLuint))                                                                                       \
  X(void, GetProgramiv, (GLuint, GLenum, GLint *))                                                                     \
  X(void, GetProgramInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *))                                                   \
  X(void, UseProgram, (GLuint))                                                                                        \
  X(void, DeleteShader, (GLuint))                                                                                      \
  X(void, DeleteProgram, (GLuint))                                                                                     \
  X(GLint, GetUniformLocation, (GLuint, const GLchar *))                                                               \
  X(void, Uniform1i, (GLint, GLint))                                                                                   \
  X(void, Uniform1f, (GLint, GLfloat))                                                                                 \
  X(void, Uniform2f, (GLint, GLfloat, GLfloat))                                                                        \
  X(void, Uniform3f, (GLint, GLfloat, GLfloat, GLfloat))                                                               \
  X(void, Uniform3fv, (GLint, GLsizei, const GLfloat *))                                                               \
  X(void, Uniform4fv, (GLint, GLsizei, const GLfloat *))                                                               \
  X(void, UniformMatrix4fv, (GLint, GLsizei, GLboolean, const GLfloat *))                                              \
  X(void, GenTextures, (GLsizei, GLuint *))                                                                            \
  X(void, BindTexture, (GLenum, GLuint))                                                                               \
  X(void, DeleteTextures, (GLsizei, const GLuint *))                                                                   \
  X(void, TexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *))                   \
  X(void, TexImage3D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *))          \
  X(void, TexParameteri, (GLenum, GLenum, GLint))                                                                      \
  X(void, ActiveTexture, (GLenum))                                                                                     \
  X(void, PixelStorei, (GLenum, GLint))                                                                                \
  X(void, GenFramebuffers, (GLsizei, GLuint *))                                                                        \
  X(void, BindFramebuffer, (GLenum, GLuint))                                                                           \
  X(void, FramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint))                                               \
  X(void, FramebufferRenderbuffer, (GLenum, GLenum, GLenum, GLuint))                                                   \
  X(GLenum, CheckFramebufferStatus, (GLenum))                                                                          \
  X(void, DeleteFramebuffers, (GLsizei, const GLuint *))                                                               \
  X(void, GenRenderbuffers, (GLsizei, GLuint *))                                                                       \
  X(void, BindRenderbuffer, (GLenum, GLuint))                                                                          \
  X(void, RenderbufferStorage, (GLenum, GLenum, GLsizei, GLsizei))                                                     \
  X(void, DeleteRenderbuffers, (GLsizei, const GLuint *))                                                              \
  X(void, DrawElements, (GLenum, GLsizei, GLenum, const void *))                                                       \
  X(void, DrawArrays, (GLenum, GLint, GLsizei))                                                                        \
  X(void, ReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *))                                        \
  X(GLenum, GetError, (void))

#define DECLARE(ret, name, args) static ret(GLAPIENTRY_ *p##name) args;
SR_GL_FUNCS(DECLARE)
#undef DECLARE

bool sr_gl_load(SrGlGetProc get) {
#ifdef __EMSCRIPTEN__
  (void)get;
#define ASSIGN(ret, name, args) p##name = gl##name;
  SR_GL_FUNCS(ASSIGN)
#undef ASSIGN
  return true;
#else
  bool ok = true;
#define LOAD(ret, name, args)                                                                                          \
  p##name = (ret(GLAPIENTRY_ *) args)get("gl" #name);                                                                  \
  if (!p##name) ok = false;
  SR_GL_FUNCS(LOAD)
#undef LOAD
  return ok;
#endif
}

/* ---- shaders ------------------------------------------------------------------------------- */

static const char *k_world_vs =
    "layout(location=0) in vec3 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "layout(location=2) in vec4 a_color;\n"
    "layout(location=3) in vec4 a_extra;\n"
    "uniform mat4 u_mvp;\n"
    "uniform mat4 u_view;\n"
    "uniform vec3 u_lpos[8];\n"
    "uniform vec4 u_lcol[8];\n"
    "uniform int u_nl;\n"
    "uniform vec3 u_amb;\n"
    "uniform float u_wet;\n"
    "uniform vec2 u_snap;\n"
    "uniform int u_affine;\n"
    "out vec4 v_color;\n"
    "out vec3 v_uvw;\n"
    "flat out float v_layer;\n"
    "out float v_fogz;\n"
    "out float v_em;\n"
    "void main() {\n"
    "  vec4 clip = u_mvp * vec4(a_pos, 1.0);\n"
    "  if (u_snap.x > 0.0) clip.xy = floor(clip.xy / clip.w * u_snap + 0.5) / u_snap * clip.w;\n"
    "  gl_Position = clip;\n"
    "  vec3 light = u_amb;\n"
    "  for (int i = 0; i < 8; i++) {\n"
    "    if (i >= u_nl) break;\n"
    "    float d = length(u_lpos[i] - a_pos);\n"
    "    float att = clamp(1.0 - d / u_lcol[i].w, 0.0, 1.0);\n"
    "    light += u_lcol[i].rgb * att * att * (1.0 + a_extra.z * u_wet * 1.5);\n"
    "  }\n"
    "  float em = a_extra.y / 255.0;\n"
    "  v_em = em;\n"
    "  v_color = vec4(a_color.rgb * mix(light, vec3(1.0), em), a_color.a);\n"
    "  float w = u_affine == 1 ? clip.w : 1.0;\n"
    "  v_uvw = vec3(a_uv * w, w);\n"
    "  v_layer = a_extra.x;\n"
    "  v_fogz = -(u_view * vec4(a_pos, 1.0)).z;\n"
    "}\n";

static const char *k_world_fs = "uniform highp sampler2DArray u_tex;\n"
                                "uniform vec3 u_fog;\n"
                                "uniform vec2 u_fogr;\n"
                                "in vec4 v_color;\n"
                                "in vec3 v_uvw;\n"
                                "flat in float v_layer;\n"
                                "in float v_fogz;\n"
                                "in float v_em;\n"
                                "out vec4 o_color;\n"
                                "void main() {\n"
                                "  vec4 t = texture(u_tex, vec3(v_uvw.xy / v_uvw.z, v_layer));\n"
                                "  if (t.a < 0.5) discard;\n"
                                "  vec4 c = t * v_color;\n"
                                "  float lum = max(t.r, max(t.g, t.b));\n"
                                "  c.rgb += t.rgb * step(0.78, lum) * step(t.b, 0.99) * 1.1;\n"
                                "  float f = clamp((v_fogz - u_fogr.x) / (u_fogr.y - u_fogr.x), 0.0, 1.0);\n"
                                "  f *= 1.0 - 0.7 * v_em;\n" /* neon glows through fog: same formula as sr_fog_factor */
                                "  o_color = vec4(mix(c.rgb, u_fog, f), c.a);\n"
                                "}\n";

static const char *k_post_vs = "layout(location=0) in vec2 a_pos;\n"
                               "out vec2 v_uv;\n"
                               "void main() { v_uv = a_pos * 0.5 + 0.5; gl_Position = vec4(a_pos, 0.0, 1.0); }\n";

static const char *k_post_fs =
    "uniform sampler2D u_tex;\n"
    "uniform int u_dither;\n"
    "uniform int u_quant;\n"
    "uniform int u_crt;\n"
    "uniform vec2 u_res;\n"
    "in vec2 v_uv;\n"
    "out vec4 o_color;\n"
    "float bayer(vec2 p) {\n"
    "  int x = int(mod(p.x, 4.0)), y = int(mod(p.y, 4.0));\n"
    "  int i = x + y * 4;\n"
    "  int m[16] = int[16](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);\n"
    "  return float(m[i]) / 16.0 - 0.5;\n"
    "}\n"
    "void main() {\n"
    "  vec3 c = texture(u_tex, v_uv).rgb;\n"
    "  vec2 px = floor(v_uv * u_res);\n"
    "  if (u_dither == 1) c += bayer(px) / 24.0;\n"
    "  if (u_quant == 1) c = floor(c * vec3(31.0, 63.0, 31.0) + 0.5) / vec3(31.0, 63.0, 31.0);\n"
    "  if (u_crt == 1) c *= 0.82 + 0.18 * mod(px.y, 2.0);\n"
    "  o_color = vec4(clamp(c, 0.0, 1.0), 1.0);\n"
    "}\n";

void sr_gl_shader_source(bool gles, bool fragment, bool post, char *out, size_t cap) {
  const char *pre = gles ? "#version 300 es\nprecision highp float;\nprecision highp int;\n" : "#version 330 core\n";
  const char *body = post ? (fragment ? k_post_fs : k_post_vs) : (fragment ? k_world_fs : k_world_vs);
  snprintf(out, cap, "%s%s", pre, body);
}

static unsigned compile(SrGlBackend *b, GLenum type, const char *src) {
  GLuint s = pCreateShader(type);
  pShaderSource(s, 1, &src, NULL);
  pCompileShader(s);
  GLint ok = 0;
  pGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    pGetShaderInfoLog(s, (GLsizei)sizeof b->error, NULL, b->error);
    sr_log(SR_LOG_ERROR, "gl", "shader compile failed: %s", b->error);
    pDeleteShader(s);
    return 0;
  }
  return s;
}

static unsigned link_program(SrGlBackend *b, bool post) {
  char vs[4096], fs[4096];
  sr_gl_shader_source(b->gles, false, post, vs, sizeof vs);
  sr_gl_shader_source(b->gles, true, post, fs, sizeof fs);
  unsigned v = compile(b, GL_VERTEX_SHADER, vs), f = compile(b, GL_FRAGMENT_SHADER, fs);
  if (!v || !f) return 0;
  GLuint p = pCreateProgram();
  pAttachShader(p, v);
  pAttachShader(p, f);
  pLinkProgram(p);
  pDeleteShader(v);
  pDeleteShader(f);
  GLint ok = 0;
  pGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    pGetProgramInfoLog(p, (GLsizei)sizeof b->error, NULL, b->error);
    sr_log(SR_LOG_ERROR, "gl", "program link failed: %s", b->error);
    pDeleteProgram(p);
    return 0;
  }
  return p;
}

/* ---- setup ---------------------------------------------------------------------------------- */

static void upload_textures(SrGlBackend *b) {
  uint8_t *pixels = (uint8_t *)malloc((size_t)SR_TEX_SIZE * SR_TEX_SIZE * 4 * SR_TEX_LAYERS);
  for (int l = 0; l < SR_TEX_LAYERS; l++) sr_texgen_layer(l, pixels + (size_t)l * SR_TEX_SIZE * SR_TEX_SIZE * 4);
  pGenTextures(1, &b->tex_array);
  pBindTexture(GL_TEXTURE_2D_ARRAY, b->tex_array);
  pPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  pTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, SR_TEX_SIZE, SR_TEX_SIZE, SR_TEX_LAYERS, 0, GL_RGBA, GL_UNSIGNED_BYTE,
              pixels);
  pTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  pTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  pTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
  pTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
  free(pixels);
}

static void ensure_fbo(SrGlBackend *b, int w, int h) {
  if (b->fbo && b->fbo_w == w && b->fbo_h == h) return;
  if (b->fbo) {
    pDeleteFramebuffers(1, &b->fbo);
    pDeleteTextures(1, &b->fbo_color);
    pDeleteRenderbuffers(1, &b->fbo_depth);
  }
  pGenTextures(1, &b->fbo_color);
  pBindTexture(GL_TEXTURE_2D, b->fbo_color);
  pTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  pGenRenderbuffers(1, &b->fbo_depth);
  pBindRenderbuffer(GL_RENDERBUFFER, b->fbo_depth);
  pRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
  pGenFramebuffers(1, &b->fbo);
  pBindFramebuffer(GL_FRAMEBUFFER, b->fbo);
  pFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, b->fbo_color, 0);
  pFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, b->fbo_depth);
  if (pCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    sr_log(SR_LOG_ERROR, "gl", "framebuffer incomplete"); /* GCOVR_EXCL_LINE reason: driver failure */
  pBindFramebuffer(GL_FRAMEBUFFER, 0);
  b->fbo_w = w;
  b->fbo_h = h;
}

bool sr_glb_init(SrGlBackend *b, bool gles) {
  unsigned gen = b->generation;
  memset(b, 0, sizeof *b);
  b->generation = gen + 1;
  b->gles = gles;
  b->prog_world = link_program(b, false);
  b->prog_post = link_program(b, true);
  if (!b->prog_world || !b->prog_post) return false;
  GLuint p = b->prog_world;
  b->u_mvp = pGetUniformLocation(p, "u_mvp");
  b->u_view = pGetUniformLocation(p, "u_view");
  b->u_lpos = pGetUniformLocation(p, "u_lpos");
  b->u_lcol = pGetUniformLocation(p, "u_lcol");
  b->u_nl = pGetUniformLocation(p, "u_nl");
  b->u_amb = pGetUniformLocation(p, "u_amb");
  b->u_wet = pGetUniformLocation(p, "u_wet");
  b->u_snap = pGetUniformLocation(p, "u_snap");
  b->u_affine = pGetUniformLocation(p, "u_affine");
  b->u_tex = pGetUniformLocation(p, "u_tex");
  b->u_fog = pGetUniformLocation(p, "u_fog");
  b->u_fogr = pGetUniformLocation(p, "u_fogr");
  p = b->prog_post;
  b->p_tex = pGetUniformLocation(p, "u_tex");
  b->p_dither = pGetUniformLocation(p, "u_dither");
  b->p_quant = pGetUniformLocation(p, "u_quant");
  b->p_crt = pGetUniformLocation(p, "u_crt");
  b->p_res = pGetUniformLocation(p, "u_res");
  upload_textures(b);
  static const float quad[12] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};
  pGenVertexArrays(1, &b->quad_vao);
  pBindVertexArray(b->quad_vao);
  pGenBuffers(1, &b->quad_vbo);
  pBindBuffer(GL_ARRAY_BUFFER, b->quad_vbo);
  pBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
  pEnableVertexAttribArray(0);
  pVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, (void *)0);
  pBindVertexArray(0);
  b->ready = true;
  return true;
}

void sr_glb_context_lost(SrGlBackend *b) {
  unsigned gen = b->generation;
  memset(b, 0, sizeof *b);
  b->generation = gen + 1; /* stale mesh handles are detected by generation */
}

void sr_glb_shutdown(SrGlBackend *b) {
  if (!b->ready) return;
  for (int i = 0; i < SR_GL_MAX_MESHES; i++) {
    SrGlMesh *m = &b->meshes[i];
    if (!m->used) continue;
    pDeleteVertexArrays(1, &m->vao);
    pDeleteBuffers(1, &m->vbo);
    pDeleteBuffers(1, &m->ibo);
  }
  if (b->fbo) {
    pDeleteFramebuffers(1, &b->fbo);
    pDeleteTextures(1, &b->fbo_color);
    pDeleteRenderbuffers(1, &b->fbo_depth);
  }
  pDeleteTextures(1, &b->tex_array);
  pDeleteVertexArrays(1, &b->quad_vao);
  pDeleteBuffers(1, &b->quad_vbo);
  pDeleteProgram(b->prog_world);
  pDeleteProgram(b->prog_post);
  sr_glb_context_lost(b);
}

/* Mesh handle = generation << 16 | (slot + 1). */
static SrGlMesh *mesh_slot(SrGlBackend *b, SrMesh *m) {
  unsigned slot = (m->gpu & 0xFFFFu);
  if (m->gpu && (m->gpu >> 16) == (b->generation & 0xFFFFu) && slot >= 1 && slot <= SR_GL_MAX_MESHES &&
      b->meshes[slot - 1].used)
    return &b->meshes[slot - 1];
  for (int i = 0; i < SR_GL_MAX_MESHES; i++) {
    SrGlMesh *gm = &b->meshes[i];
    if (gm->used) continue;
    gm->used = true;
    pGenVertexArrays(1, &gm->vao);
    pGenBuffers(1, &gm->vbo);
    pGenBuffers(1, &gm->ibo);
    pBindVertexArray(gm->vao);
    pBindBuffer(GL_ARRAY_BUFFER, gm->vbo);
    pBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gm->ibo);
    GLsizei stride = (GLsizei)sizeof(SrVertex);
    pEnableVertexAttribArray(0);
    pVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)offsetof(SrVertex, x));
    pEnableVertexAttribArray(1);
    pVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void *)offsetof(SrVertex, u));
    pEnableVertexAttribArray(2);
    pVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void *)offsetof(SrVertex, r));
    pEnableVertexAttribArray(3);
    pVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_FALSE, stride, (void *)offsetof(SrVertex, layer));
    pBindVertexArray(0);
    m->gpu = ((b->generation & 0xFFFFu) << 16) | (unsigned)(i + 1);
    m->dirty = true;
    return gm;
  }
  return NULL; /* GCOVR_EXCL_LINE reason: more meshes than slots never happens */
}

static void draw_mesh(SrGlBackend *b, SrMesh *m) {
  if (!m || m->ni == 0) return;
  SrGlMesh *gm = mesh_slot(b, m);
  if (!gm) return; /* GCOVR_EXCL_LINE reason: see mesh_slot */
  pBindVertexArray(gm->vao);
  if (m->dirty) {
    pBindBuffer(GL_ARRAY_BUFFER, gm->vbo);
    pBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(SrVertex) * (size_t)m->nv), m->v, GL_DYNAMIC_DRAW);
    pBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gm->ibo);
    pBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(sizeof(uint32_t) * (size_t)m->ni), m->idx, GL_DYNAMIC_DRAW);
    m->dirty = false;
    gm->count = m->ni;
  }
  pDrawElements(GL_TRIANGLES, gm->count, GL_UNSIGNED_INT, (void *)0);
  b->draw_calls++;
  b->triangles += gm->count / 3;
}

static void set_lights(SrGlBackend *b, const SrFrame *f, const SrDraw *d) {
  float pos[24], col[32];
  memset(pos, 0, sizeof pos);
  memset(col, 0, sizeof col);
  for (int i = 0; i < d->nlights; i++) {
    const SrLight *l = &f->lights[d->lights[i]];
    pos[i * 3] = l->pos.x;
    pos[i * 3 + 1] = l->pos.y;
    pos[i * 3 + 2] = l->pos.z;
    col[i * 4] = l->r;
    col[i * 4 + 1] = l->g;
    col[i * 4 + 2] = l->b;
    col[i * 4 + 3] = l->radius;
  }
  pUniform3fv(b->u_lpos, 8, pos);
  pUniform4fv(b->u_lcol, 8, col);
  pUniform1i(b->u_nl, d->nlights);
}

void sr_glb_render(SrGlBackend *b, const SrFrame *f, SrUi *ui, int ww, int wh, bool integer_scale) {
  if (!b->ready) return;
  b->draw_calls = 0;
  b->triangles = 0;
  int iw = f && f->internal_w > 0 ? f->internal_w : 480, ih = f && f->internal_h > 0 ? f->internal_h : 270;
  ensure_fbo(b, iw, ih);
  pActiveTexture(GL_TEXTURE0);
  if (f) {
    /* 3D pass at internal resolution */
    pBindFramebuffer(GL_FRAMEBUFFER, b->fbo);
    pViewport(0, 0, iw, ih);
    pClearColor(f->sky[0], f->sky[1], f->sky[2], 1.0f);
    pClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    pEnable(GL_DEPTH_TEST);
    pDepthFunc(GL_LEQUAL);
    pEnable(GL_BLEND);
    pBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    pUseProgram(b->prog_world);
    pBindTexture(GL_TEXTURE_2D_ARRAY, b->tex_array);
    pUniform1i(b->u_tex, 0);
    SrMat4 mvp = sr_m4_mul(f->proj, f->view);
    pUniformMatrix4fv(b->u_mvp, 1, GL_FALSE, mvp.m);
    pUniformMatrix4fv(b->u_view, 1, GL_FALSE, f->view.m);
    pUniform3f(b->u_amb, f->ambient[0], f->ambient[1], f->ambient[2]);
    pUniform1f(b->u_wet, f->wetness);
    pUniform2f(b->u_snap, f->vertex_snap ? (float)iw * 0.5f : 0.0f, f->vertex_snap ? (float)ih * 0.5f : 0.0f);
    pUniform1i(b->u_affine, f->affine ? 1 : 0);
    pUniform3f(b->u_fog, f->fog[0], f->fog[1], f->fog[2]);
    for (int i = 0; i < f->ndraws; i++) {
      const SrDraw *d = &f->draws[i];
      if (d->pass == SR_PASS_NOFOG)
        pUniform2f(b->u_fogr, 1e6f, 2e6f);
      else
        pUniform2f(b->u_fogr, f->fog_start, f->fog_end);
      set_lights(b, f, d);
      draw_mesh(b, d->mesh);
    }
    pDisable(GL_DEPTH_TEST);
  }
  /* post pass: upscale with nearest filtering into the letterboxed viewport */
  pBindFramebuffer(GL_FRAMEBUFFER, 0);
  pViewport(0, 0, ww, wh);
  pClearColor(0, 0, 0, 1);
  pClear(GL_COLOR_BUFFER_BIT);
  if (f) {
    SrViewport vp = sr_letterbox(ww, wh, iw, ih, integer_scale);
    pViewport(vp.x, vp.y, vp.w, vp.h);
    pUseProgram(b->prog_post);
    pBindTexture(GL_TEXTURE_2D, b->fbo_color);
    pUniform1i(b->p_tex, 0);
    pUniform1i(b->p_dither, f->dither ? 1 : 0);
    pUniform1i(b->p_quant, f->quantize ? 1 : 0);
    pUniform1i(b->p_crt, f->crt ? 1 : 0);
    pUniform2f(b->p_res, (float)iw, (float)ih);
    pBindVertexArray(b->quad_vao);
    pDrawArrays(GL_TRIANGLES, 0, 6);
    b->draw_calls++;
  }
  /* UI at window resolution with an orthographic projection */
  if (ui && ui->mesh.ni > 0) {
    pViewport(0, 0, ww, wh);
    pEnable(GL_BLEND);
    pBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    pUseProgram(b->prog_world);
    pBindTexture(GL_TEXTURE_2D_ARRAY, b->tex_array);
    SrMat4 ortho = sr_m4_ortho(0, (float)ui->w, (float)ui->h, 0, -1, 1);
    SrMat4 id = sr_m4_identity();
    pUniformMatrix4fv(b->u_mvp, 1, GL_FALSE, ortho.m);
    pUniformMatrix4fv(b->u_view, 1, GL_FALSE, id.m);
    pUniform3f(b->u_amb, 1, 1, 1);
    pUniform1i(b->u_nl, 0);
    pUniform2f(b->u_snap, 0, 0);
    pUniform1i(b->u_affine, 0);
    pUniform2f(b->u_fogr, 1e6f, 2e6f);
    draw_mesh(b, &ui->mesh);
  }
  pBindVertexArray(0);
}

bool sr_glb_read_internal(SrGlBackend *b, uint8_t *rgb, int w, int h) {
  if (!b->ready || !b->fbo || w != b->fbo_w || h != b->fbo_h) return false;
  uint8_t *rgba = (uint8_t *)malloc((size_t)w * h * 4);
  pBindFramebuffer(GL_FRAMEBUFFER, b->fbo);
  pPixelStorei(GL_PACK_ALIGNMENT, 1);
  pReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  pBindFramebuffer(GL_FRAMEBUFFER, 0);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++)
      for (int k = 0; k < 3; k++) rgb[((size_t)(h - 1 - y) * w + x) * 3 + k] = rgba[((size_t)y * w + x) * 4 + k];
  free(rgba);
  return pGetError() == GL_NO_ERROR;
}

bool sr_glb_read_window(SrGlBackend *b, uint8_t *rgb, int x0, int y0, int w, int h) {
  if (!b->ready) return false;
  uint8_t *rgba = (uint8_t *)malloc((size_t)w * h * 4);
  pBindFramebuffer(GL_FRAMEBUFFER, 0);
  pPixelStorei(GL_PACK_ALIGNMENT, 1);
  pReadPixels(x0, y0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++)
      for (int k = 0; k < 3; k++) rgb[((size_t)(h - 1 - y) * w + x) * 3 + k] = rgba[((size_t)y * w + x) * 4 + k];
  free(rgba);
  return true;
}
