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
  X(void, VertexAttribDivisor, (GLuint, GLuint))                                                                       \
  X(void, DrawElementsInstanced, (GLenum, GLsizei, GLenum, const void *, GLsizei))                                     \
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
  X(void, Uniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat))                                                      \
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
    "layout(location=4) in vec4 i_a;\n" /* instance: x, y, z, yaw */
    "layout(location=5) in vec4 i_b;\n" /* phase, width scale, height scale, clip */
    "layout(location=6) in vec4 i_c;\n" /* outfit colour */
    "layout(location=7) in vec4 i_d;\n" /* skin colour */
    "uniform int u_inst;\n"
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
    "flat out float v_wire;\n"
    "out float v_fogz;\n"
    "out float v_em;\n"
    "flat out float v_usage;\n"
    "flat out float v_seed;\n"
    "out vec3 v_wpos;\n"
    "out vec3 v_base;\n"
    "void main() {\n"
    "  vec3 p = a_pos;\n"
    "  vec3 base = a_color.rgb;\n"
    "  if (u_inst == 1) {\n" /* instanced crowd: thighs, shins, arms and forearms pose by clip and phase */
    "    int part = int(a_extra.w + 0.5);\n"
    "    int code = int(i_b.w + 0.5);\n"
    "    float h = i_b.z, ph = 6.2831853 * i_b.x, v = i_c.a, hunch = fract(i_d.a * 0.5) * 2.0;\n"
    "    p *= vec3(i_b.y, h, i_b.y);\n"
    "    float side = (part == 0 || part == 3 || part == 6 || part == 8) ? 1.0 : -1.0;\n"
    "    float run = clamp((v - 1.6) / 1.8, 0.0, 1.0);\n"
    "    float thigh = 0.0, knee = 0.0, sho = 0.0, elb = 0.12, lean = 0.0, drop = 0.0;\n"
    "    if (code == 1) {\n"
    "      float c = cos(ph) * side, s = sin(ph) * side;\n"
    "      thigh = mix(0.42, 0.75, run) * c;\n"
    "      knee = mix(0.08, 0.25, run) + mix(0.55, 1.4, run) * max(0.0, -s);\n"
    "      sho = -mix(0.3, 0.7, run) * c;\n"
    "      elb = mix(0.25, 1.35, run);\n"
    "      lean = mix(0.03, 0.16, run) + 0.22 * hunch;\n"
    "      drop = h * (0.02 + 0.02 * run) * (1.0 - abs(cos(ph)));\n"
    "    } else if (code == 2) {\n"
    "      thigh = 1.5; knee = 1.45; sho = 0.25; elb = 0.9; drop = h * 0.22;\n"
    "    } else if (code == 3) {\n"
    "      thigh = 1.35; knee = 2.1; sho = 2.3; elb = 2.0; lean = 0.6; drop = h * 0.2;\n"
    "    } else {\n"
    "      lean = 0.02 + 0.015 * sin(ph * 1.0);\n"
    "    }\n"
    "    float hip = 0.48 * h, kn = 0.24 * h, sh = 0.8 * h, el = 0.64 * h;\n"
    "    if (part == 6 || part == 7) {\n" /* shin: bend at the knee, then swing at the hip */
    "      float ck = cos(knee), sk = sin(knee); vec2 q = vec2(p.y - kn, p.z);\n"
    "      p.yz = vec2(q.x * ck - q.y * sk, q.x * sk + q.y * ck) + vec2(kn, 0.0);\n"
    "    }\n"
    "    if (part == 0 || part == 1 || part == 6 || part == 7) {\n"
    "      float ct = cos(-thigh), st = sin(-thigh); vec2 q = vec2(p.y - hip, p.z);\n"
    "      p.yz = vec2(q.x * ct - q.y * st, q.x * st + q.y * ct) + vec2(hip, 0.0);\n"
    "    }\n"
    "    if (part == 8 || part == 9) {\n" /* forearm: bend at the elbow, then the shoulder */
    "      float ce = cos(-elb), se = sin(-elb); vec2 q = vec2(p.y - el, p.z);\n"
    "      p.yz = vec2(q.x * ce - q.y * se, q.x * se + q.y * ce) + vec2(el, 0.0);\n"
    "    }\n"
    "    if (part == 3 || part == 4 || part == 8 || part == 9) {\n"
    "      float cs = cos(-sho), ss = sin(-sho); vec2 q = vec2(p.y - sh, p.z);\n"
    "      p.yz = vec2(q.x * cs - q.y * ss, q.x * ss + q.y * cs) + vec2(sh, 0.0);\n"
    "    }\n"
    "    if (part >= 2 && part != 6 && part != 7) {\n" /* the upper body leans at the hip */
    "      float cl = cos(lean), sl = sin(lean); vec2 q = vec2(p.y - hip, p.z);\n"
    "      p.yz = vec2(q.x * cl - q.y * sl, q.x * sl + q.y * cl) + vec2(hip, 0.0);\n"
    "    }\n"
    "    p.y -= drop;\n"
    "    if (code == 4) p = vec3(p.x, 0.15 + p.z, -p.y);\n" /* lying down */
    "    float cy = cos(i_a.w), sy = sin(i_a.w);\n"
    "    p = vec3(i_a.x + p.x * cy + p.z * sy, i_a.y + p.y, i_a.z - p.x * sy + p.z * cy);\n"
    "    base = part == 5 ? i_d.rgb : ((part < 2 || part == 6 || part == 7) ? i_c.rgb * 0.6 : i_c.rgb);\n"
    "  }\n"
    "  vec4 clip = u_mvp * vec4(p, 1.0);\n"
    "  if (u_snap.x > 0.0) clip.xy = floor(clip.xy / clip.w * u_snap + 0.5) / u_snap * clip.w;\n"
    "  gl_Position = clip;\n"
    "  vec3 light = u_amb;\n"
    "  for (int i = 0; i < 8; i++) {\n"
    "    if (i >= u_nl) break;\n"
    "    float d = length(u_lpos[i] - p);\n"
    "    float att = clamp(1.0 - d / u_lcol[i].w, 0.0, 1.0);\n"
    "    light += u_lcol[i].rgb * att * att * (1.0 + a_extra.z * u_wet * 1.5);\n"
    "  }\n"
    "  float em = a_extra.y / 255.0;\n"
    "  v_em = em;\n"
    "  v_color = vec4(base * mix(light, vec3(1.0), em), a_color.a);\n"
    "  float w = u_affine == 1 ? clip.w : 1.0;\n"
    "  v_uvw = vec3(a_uv * w, w);\n"
    "  v_layer = a_extra.x;\n"
    "  v_wire = a_extra.z;\n"
    "  v_fogz = -(u_view * vec4(p, 1.0)).z;\n"
    "  v_wpos = p;\n"
    "  v_base = base;\n"
    "  v_usage = floor(a_extra.w / 64.0);\n"
    "  v_seed = mod(a_extra.w, 64.0);\n"
    "}\n";

static const char *k_world_fs =
    "uniform highp sampler2DArray u_tex;\n"
    "uniform vec3 u_fog;\n"
    "uniform vec2 u_fogr;\n"
    "uniform vec4 u_share;\n" /* lit window share per building use */
    "uniform float u_time;\n"
    "uniform int u_glow;\n"  /* 1: additive glow, faded by fog */
    "uniform int u_debug;\n" /* 1: emissive mask (tests) */
    "uniform vec3 u_band;\n" /* dawn or dusk glow on the fog toward u_band_dir (+1 east, -1 west) */
    "uniform float u_band_dir;\n"
    "uniform vec3 u_eye;\n"
    "uniform float u_mist;\n" /* extra fog below 3 m */
    "uniform int u_pixel;\n"  /* per-pixel light for the ground and the lower walls */
    "uniform vec3 u_lpos[8];\n"
    "uniform vec4 u_lcol[8];\n"
    "uniform int u_nl;\n"
    "uniform vec3 u_amb;\n"
    "uniform float u_wet;\n"
    "in vec4 v_color;\n"
    "in vec3 v_uvw;\n"
    "flat in float v_layer;\n"
    "flat in float v_wire;\n"
    "in float v_fogz;\n"
    "in float v_em;\n"
    "flat in float v_usage;\n"
    "flat in float v_seed;\n"
    "in vec3 v_wpos;\n"
    "in vec3 v_base;\n"
    "out vec4 o_color;\n"
    "float hash3(vec3 p) { return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453); }\n"
    "vec3 fog_color() {\n" /* the fog, warmed toward the dawn or dusk side low on the horizon */
    "  vec3 d = normalize(v_wpos - u_eye);\n"
    "  float side = max(0.0, d.x * u_band_dir);\n"
    "  float low = clamp(1.0 - max(d.y, 0.0) * 2.2, 0.0, 1.0);\n"
    "  return u_fog + u_band * side * side * low;\n"
    "}\n"
    "void main() {\n"
    "  float f = clamp((v_fogz - u_fogr.x) / (u_fogr.y - u_fogr.x), 0.0, 1.0);\n"
    "  vec3 fogc = fog_color();\n"
    "  f = clamp(f + u_mist * clamp((3.0 - v_wpos.y) / 3.0, 0.0, 1.0) * clamp(v_fogz / 25.0, 0.0, 1.0), 0.0, 1.0);\n"
    "  if (u_fogr.y < 1e5) f = max(f, smoothstep(190.0, 255.0, v_wpos.y) * 0.85);\n" /* towers fade into the clouds */
    "  if (v_wire > 1.5) {\n" /* Matrix wireframe: quad edges glow, faces stay dim */
    "    vec2 q = v_uvw.xy / v_uvw.z;\n"
    "    float e = min(min(q.x, 1.0 - q.x), min(q.y, 1.0 - q.y));\n"
    "    float line = 1.0 - smoothstep(0.015, 0.06, e);\n"
    "    if (v_wire > 2.5 && line < 0.05) discard;\n" /* hollow: edges only */
    "    vec3 wc = v_color.rgb * (0.1 + 1.5 * line);\n"
    "    o_color = vec4(u_debug == 1 ? vec3(0.0) : mix(wc, fogc, f), 1.0);\n"
    "    return;\n"
    "  }\n"
    "  vec2 uv = v_uvw.xy / v_uvw.z;\n"
    "  vec4 t = texture(u_tex, vec3(uv, v_layer));\n"
    "  if (u_glow == 1) {\n" /* soft sprites: alpha is the strength, light only adds */
    "    vec3 g = t.rgb * v_color.rgb * (t.a * v_color.a * (1.0 - f));\n"
    "    if (v_wire > 0.5) g *= 0.7 + 0.3 * sin(v_wpos.z * 3.1 + v_wpos.x * 1.7 + u_time * 4.0);\n" /* ripples */
    "    o_color = vec4(u_debug == 1 ? vec3(step(0.02, t.a * (1.0 - f))) : g, 1.0);\n"
    "    return;\n"
    "  }\n"
    "  if (t.a < 0.5) discard;\n"
    "  vec4 c = t * v_color;\n"
    "  float em = v_em, fog_em = v_em;\n"
    "  if (t.a > 0.7 && t.a < 0.86) {\n" /* glass: lit or dark by building use, hour and window */
    "    float share = v_usage < 0.5 ? u_share.x : v_usage < 1.5 ? u_share.y : v_usage < 2.5 ? u_share.z : u_share.w;\n"
    "    vec3 k = vec3(floor(uv), t.r * 255.0 + v_seed * 37.0);\n"
    "    float h = hash3(k), h2 = hash3(k + vec3(7.1, 3.3, 1.7));\n"
    "    if (h < share) {\n"
    "      vec3 lc = h2 < 0.6 ? vec3(1.0, 0.76, 0.46) : h2 < 0.86 ? vec3(0.70, 0.84, 1.0)\n"
    "              : h2 < 0.95 ? vec3(0.35, 0.5, 1.0) * (0.7 + 0.3 * sin(u_time * (9.0 + h2 * 30.0)))\n"
    "              : vec3(1.0, 0.45, 0.8);\n"
    "      c = vec4(lc * (0.5 + 0.3 * t.g), 1.0);\n" /* peaks at 0.8: windows never clip to white */
    "      em = 0.6;\n"
    "      fog_em = 0.35;\n" /* windows sink into the fog sooner than neon */
    "    } else {\n"
    "      c = vec4(vec3(0.03, 0.035, 0.05) + u_fog * 0.12 + v_color.rgb * 0.04, 1.0);\n"
    "      em = 0.0;\n"
    "      fog_em = 0.0;\n"
    "    }\n"
    "  }\n"
    "  if (u_pixel == 1 && v_wpos.y < 6.0 && em < 0.3 && !(t.a > 0.7 && t.a < 0.86)) {\n" /* per-pixel light */
    "    vec3 light = u_amb;\n"
    "    for (int i = 0; i < 8; i++) {\n"
    "      if (i >= u_nl) break;\n"
    "      float d = length(u_lpos[i] - v_wpos);\n"
    "      float att = clamp(1.0 - d / u_lcol[i].w, 0.0, 1.0);\n"
    "      light += u_lcol[i].rgb * att * att * (1.0 + step(0.5, v_wire) * step(v_wire, 1.5) * u_wet * 1.5);\n"
    "    }\n"
    "    c = vec4(t.rgb * v_base * mix(light, vec3(1.0), em), c.a);\n"
    "  }\n"
    "  if (v_em > 0.5 && !(t.a > 0.7 && t.a < 0.86)) {\n" /* neon motion: pad 0x80 | seed */
    "    float pad = v_usage * 64.0 + v_seed;\n"
    "    float lvl = 1.0;\n"
    "    if (pad >= 192.0) lvl = 0.55 + 0.45 * step(0.5, fract(v_wpos.x * 0.8 + v_wpos.z * 0.8 - u_time * 3.0));\n"
    "    else if (pad >= 128.0 && v_seed < 5.0) {\n" /* broken neon drops out in bursts (sr_neon_level) */
    "      float k = floor(u_time * 12.0);\n"
    "      lvl = fract(sin(k * 12.9898 + v_seed * 78.233) * 43758.5453) < 0.28 ? 0.12 : 1.0;\n"
    "    } else if (pad >= 128.0 && v_seed >= 56.0) {\n" /* tower beacons blink */
    "      lvl = mod(u_time + (v_seed - 56.0) * 0.17, 1.5) < 0.3 ? 1.0 : 0.15;\n"
    "    }\n"
    "    c.rgb *= lvl;\n"
    "    em *= lvl;\n"
    "  }\n"
    "  if (u_debug == 1) {\n"
    "    o_color = vec4(vec3(step(0.3, em)), 1.0);\n"
    "    return;\n"
    "  }\n"
    "  f *= 1.0 - 0.85 * fog_em;\n" /* neon glows through fog: same formula as sr_fog_factor */
    "  o_color = vec4(mix(c.rgb, fogc, f), c.a);\n"
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
    "uniform sampler2D u_bloom;\n"
    "uniform float u_bloom_k;\n"
    "uniform vec3 u_lift;\n"
    "uniform vec3 u_gain;\n"
    "uniform float u_bright;\n"
    "uniform int u_vig;\n"
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
    "  if (u_crt == 1) {\n" /* VHS: a little chromatic offset */
    "    c.r = texture(u_tex, v_uv + vec2(1.0 / u_res.x, 0.0)).r;\n"
    "    c.b = texture(u_tex, v_uv - vec2(1.0 / u_res.x, 0.0)).b;\n"
    "  }\n"
    "  if (u_bloom_k > 0.0) c += texture(u_bloom, v_uv).rgb * u_bloom_k;\n"
    "  c = clamp(c * u_gain + u_lift * (1.0 - c), 0.0, 1.0);\n" /* the district's grade */
    "  vec3 u = 1.0 - c;\n"
    "  c = clamp(c + u_bright * 0.05 * u * u * u, 0.0, 1.0);\n" /* brightness lifts the dark (sr_brightness) */
    "  if (u_vig == 1) { vec2 q = v_uv - 0.5; c *= 1.0 - 0.55 * dot(q, q); }\n"
    "  vec2 px = floor(v_uv * u_res);\n"
    "  if (u_dither == 1) c += bayer(px) / 24.0;\n"
    "  if (u_quant == 1) c = floor(c * vec3(31.0, 63.0, 31.0) + 0.5) / vec3(31.0, 63.0, 31.0);\n"
    "  if (u_crt == 1) c *= 0.82 + 0.18 * mod(px.y, 2.0);\n"
    "  o_color = vec4(clamp(c, 0.0, 1.0), 1.0);\n"
    "}\n";

/* Bloom: a bright pass keeps only glowing pixels, then a separable 9-tap blur (two passes). */
static const char *k_blur_fs = "uniform sampler2D u_tex;\n"
                               "uniform vec2 u_dir;\n"
                               "uniform int u_bright;\n"
                               "in vec2 v_uv;\n"
                               "out vec4 o_color;\n"
                               "void main() {\n"
                               "  if (u_bright == 1) {\n"
                               "    vec3 c = texture(u_tex, v_uv).rgb;\n"
                               "    float l = max(c.r, max(c.g, c.b));\n"
                               "    o_color = vec4(c * smoothstep(0.82, 1.0, l), 1.0);\n"
                               "    return;\n"
                               "  }\n"
                               "  vec3 s = texture(u_tex, v_uv).rgb * 0.2270270;\n"
                               "  s += (texture(u_tex, v_uv + u_dir * 1.3846).rgb + texture(u_tex, v_uv - u_dir * "
                               "1.3846).rgb) * 0.3162162;\n"
                               "  s += (texture(u_tex, v_uv + u_dir * 3.2308).rgb + texture(u_tex, v_uv - u_dir * "
                               "3.2308).rgb) * 0.0702703;\n"
                               "  o_color = vec4(s, 1.0);\n"
                               "}\n";

void sr_gl_shader_source(bool gles, bool fragment, int prog, char *out, size_t cap) {
  const char *pre = gles ? "#version 300 es\nprecision highp float;\nprecision highp int;\n" : "#version 330 core\n";
  const char *body = prog == SR_GL_PROG_BLUR   ? (fragment ? k_blur_fs : k_post_vs)
                     : prog == SR_GL_PROG_POST ? (fragment ? k_post_fs : k_post_vs)
                                               : (fragment ? k_world_fs : k_world_vs);
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

static unsigned link_program(SrGlBackend *b, int post) {
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

/* Instanced crowd: the unit body is uploaded once with the per-instance attributes (divisor 1). */
static void draw_instances(SrGlBackend *b, const SrFrame *f) {
  if (!b->inst_vao) {
    pGenVertexArrays(1, &b->inst_vao);
    pGenBuffers(1, &b->inst_vbo);
    pGenBuffers(1, &b->inst_ibo);
    pGenBuffers(1, &b->inst_data);
    pBindVertexArray(b->inst_vao);
    pBindBuffer(GL_ARRAY_BUFFER, b->inst_vbo);
    pBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(SrVertex) * (size_t)f->inst_mesh->nv), f->inst_mesh->v,
                GL_STATIC_DRAW);
    pBindBuffer(GL_ELEMENT_ARRAY_BUFFER, b->inst_ibo);
    pBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(sizeof(uint32_t) * (size_t)f->inst_mesh->ni), f->inst_mesh->idx,
                GL_STATIC_DRAW);
    GLsizei stride = (GLsizei)sizeof(SrVertex);
    pEnableVertexAttribArray(0);
    pVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)offsetof(SrVertex, x));
    pEnableVertexAttribArray(1);
    pVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void *)offsetof(SrVertex, u));
    pEnableVertexAttribArray(2);
    pVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void *)offsetof(SrVertex, r));
    pEnableVertexAttribArray(3);
    pVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_FALSE, stride, (void *)offsetof(SrVertex, layer));
    pBindBuffer(GL_ARRAY_BUFFER, b->inst_data);
    for (int k = 0; k < 4; k++) {
      pEnableVertexAttribArray((GLuint)(4 + k));
      pVertexAttribPointer((GLuint)(4 + k), 4, GL_FLOAT, GL_FALSE, SR_INST_FLOATS * 4, (void *)(uintptr_t)(k * 16));
      pVertexAttribDivisor((GLuint)(4 + k), 1);
    }
    b->inst_count_idx = f->inst_mesh->ni;
  }
  pBindVertexArray(b->inst_vao);
  pBindBuffer(GL_ARRAY_BUFFER, b->inst_data);
  pBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(float) * SR_INST_FLOATS * (size_t)f->inst_count), f->inst,
              GL_STREAM_DRAW);
  pUniform1i(b->u_inst, 1);
  pDrawElementsInstanced(GL_TRIANGLES, b->inst_count_idx, GL_UNSIGNED_INT, (void *)0, f->inst_count);
  pUniform1i(b->u_inst, 0);
  b->draw_calls++;
  b->triangles += b->inst_count_idx / 3 * f->inst_count;
}

/* Half-resolution ping-pong targets for bloom. */
static void ensure_bloom(SrGlBackend *b, int w, int h) {
  if (b->bloom_fbo[0] && b->bloom_w == w && b->bloom_h == h) return;
  for (int i = 0; i < 2; i++) {
    if (b->bloom_fbo[i]) {
      pDeleteFramebuffers(1, &b->bloom_fbo[i]);
      pDeleteTextures(1, &b->bloom_tex[i]);
    }
    pGenTextures(1, &b->bloom_tex[i]);
    pBindTexture(GL_TEXTURE_2D, b->bloom_tex[i]);
    pTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    pGenFramebuffers(1, &b->bloom_fbo[i]);
    pBindFramebuffer(GL_FRAMEBUFFER, b->bloom_fbo[i]);
    pFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, b->bloom_tex[i], 0);
  }
  pBindFramebuffer(GL_FRAMEBUFFER, 0);
  b->bloom_w = w;
  b->bloom_h = h;
}

static void bloom_pass(SrGlBackend *b, unsigned src_tex, unsigned dst_fbo, int bright, float dx, float dy) {
  pBindFramebuffer(GL_FRAMEBUFFER, dst_fbo);
  pViewport(0, 0, b->bloom_w, b->bloom_h);
  pBindTexture(GL_TEXTURE_2D, src_tex);
  pUniform1i(b->b_bright, bright);
  pUniform2f(b->b_dir, dx, dy);
  pDrawArrays(GL_TRIANGLES, 0, 6);
  b->draw_calls++;
}

/* Emissive pass and bloom: bright pixels of the scene, blurred at half resolution. */
static void render_bloom(SrGlBackend *b, int iw, int ih) {
  int w = iw / 2 > 0 ? iw / 2 : 1, h = ih / 2 > 0 ? ih / 2 : 1;
  ensure_bloom(b, w, h);
  pUseProgram(b->prog_blur);
  pUniform1i(b->b_tex, 0);
  pBindVertexArray(b->quad_vao);
  bloom_pass(b, b->fbo_color, b->bloom_fbo[0], 1, 0, 0);
  bloom_pass(b, b->bloom_tex[0], b->bloom_fbo[1], 0, 1.0f / (float)w, 0);
  bloom_pass(b, b->bloom_tex[1], b->bloom_fbo[0], 0, 0, 1.0f / (float)h);
  bloom_pass(b, b->bloom_tex[0], b->bloom_fbo[1], 0, 2.0f / (float)w, 0);
  bloom_pass(b, b->bloom_tex[1], b->bloom_fbo[0], 0, 0, 2.0f / (float)h);
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
  b->prog_world = link_program(b, SR_GL_PROG_WORLD);
  b->prog_post = link_program(b, SR_GL_PROG_POST);
  b->prog_blur = link_program(b, SR_GL_PROG_BLUR);
  if (!b->prog_world || !b->prog_post || !b->prog_blur) return false;
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
  b->u_inst = pGetUniformLocation(p, "u_inst");
  b->u_share = pGetUniformLocation(p, "u_share");
  b->u_time = pGetUniformLocation(p, "u_time");
  b->u_glow = pGetUniformLocation(p, "u_glow");
  b->u_debug = pGetUniformLocation(p, "u_debug");
  b->u_band = pGetUniformLocation(p, "u_band");
  b->u_band_dir = pGetUniformLocation(p, "u_band_dir");
  b->u_eye = pGetUniformLocation(p, "u_eye");
  b->u_mist = pGetUniformLocation(p, "u_mist");
  b->u_pixel = pGetUniformLocation(p, "u_pixel");
  p = b->prog_post;
  b->p_tex = pGetUniformLocation(p, "u_tex");
  b->p_dither = pGetUniformLocation(p, "u_dither");
  b->p_quant = pGetUniformLocation(p, "u_quant");
  b->p_crt = pGetUniformLocation(p, "u_crt");
  b->p_res = pGetUniformLocation(p, "u_res");
  b->p_bloom = pGetUniformLocation(p, "u_bloom");
  b->p_bloom_k = pGetUniformLocation(p, "u_bloom_k");
  b->p_lift = pGetUniformLocation(p, "u_lift");
  b->p_gain = pGetUniformLocation(p, "u_gain");
  b->p_bright = pGetUniformLocation(p, "u_bright");
  b->p_vig = pGetUniformLocation(p, "u_vig");
  b->b_tex = pGetUniformLocation(b->prog_blur, "u_tex");
  b->b_dir = pGetUniformLocation(b->prog_blur, "u_dir");
  b->b_bright = pGetUniformLocation(b->prog_blur, "u_bright");
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
  for (int i = 0; i < 2; i++)
    if (b->bloom_fbo[i]) {
      pDeleteFramebuffers(1, &b->bloom_fbo[i]);
      pDeleteTextures(1, &b->bloom_tex[i]);
    }
  pDeleteProgram(b->prog_blur);
  if (b->inset_fbo) {
    pDeleteFramebuffers(1, &b->inset_fbo);
    pDeleteTextures(1, &b->inset_color);
    pDeleteRenderbuffers(1, &b->inset_depth);
  }
  if (b->inst_vao) {
    pDeleteVertexArrays(1, &b->inst_vao);
    pDeleteBuffers(1, &b->inst_vbo);
    pDeleteBuffers(1, &b->inst_ibo);
    pDeleteBuffers(1, &b->inst_data);
  }
  pDeleteTextures(1, &b->tex_array);
  pDeleteVertexArrays(1, &b->quad_vao);
  pDeleteBuffers(1, &b->quad_vbo);
  pDeleteProgram(b->prog_world);
  pDeleteProgram(b->prog_post);
  sr_glb_context_lost(b);
}

/* Mesh handle = generation << 16 | (slot + 1). When every slot is taken (meshes of an earlier
 * game or scene are gone without telling the backend), the least recently drawn slot is reused. */
static SrGlMesh *mesh_slot(SrGlBackend *b, SrMesh *m) {
  unsigned slot = (m->gpu & 0xFFFFu);
  if (m->gpu && (m->gpu >> 16) == (b->generation & 0xFFFFu) && slot >= 1 && slot <= SR_GL_MAX_MESHES &&
      b->meshes[slot - 1].used && b->meshes[slot - 1].owner == m) {
    b->meshes[slot - 1].last_used = b->frame;
    return &b->meshes[slot - 1];
  }
  int oldest = -1;
  for (int i = 0; i < SR_GL_MAX_MESHES; i++) {
    SrGlMesh *gm = &b->meshes[i];
    if (gm->used) {
      if (gm->last_used != b->frame && (oldest < 0 || gm->last_used < b->meshes[oldest].last_used)) oldest = i;
      continue;
    }
    gm->used = true;
    gm->owner = m;
    gm->last_used = b->frame;
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
  if (oldest < 0) return NULL;       /* GCOVR_EXCL_LINE reason: more live meshes in one frame than slots */
  SrGlMesh *gm = &b->meshes[oldest]; /* buffers and attribute layout are reused as they are */
  gm->owner = m;
  gm->last_used = b->frame;
  b->reclaimed++;
  m->gpu = ((b->generation & 0xFFFFu) << 16) | (unsigned)(oldest + 1);
  m->dirty = true;
  return gm;
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

/* The 3D pass of a frame into a framebuffer at its internal resolution. */
static void scene_pass(SrGlBackend *b, const SrFrame *f, unsigned fbo, int iw, int ih) {
  pBindFramebuffer(GL_FRAMEBUFFER, fbo);
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
  pUniform1i(b->u_inst, 0);
  pUniform3f(b->u_amb, f->ambient[0], f->ambient[1], f->ambient[2]);
  pUniform1f(b->u_wet, f->wetness);
  pUniform2f(b->u_snap, f->vertex_snap ? (float)iw * 0.5f : 0.0f, f->vertex_snap ? (float)ih * 0.5f : 0.0f);
  pUniform1i(b->u_affine, f->affine ? 1 : 0);
  pUniform3f(b->u_fog, f->fog[0], f->fog[1], f->fog[2]);
  pUniform4f(b->u_share, f->win_share[0], f->win_share[1], f->win_share[2], f->win_share[3]);
  pUniform1f(b->u_time, f->time_s);
  pUniform1i(b->u_glow, 0);
  pUniform1i(b->u_debug, f->debug_emissive ? 1 : 0);
  pUniform3f(b->u_band, f->band[0], f->band[1], f->band[2]);
  pUniform1f(b->u_band_dir, f->band_dir_x);
  pUniform3f(b->u_eye, f->eye.x, f->eye.y, f->eye.z);
  pUniform1f(b->u_mist, f->mist);
  pUniform1i(b->u_pixel, f->pixel_lighting ? 1 : 0);
  /* the sky dome first, fully fogged and behind everything */
  pDepthMask(GL_FALSE);
  pUniform2f(b->u_fogr, -2.0f, -1.0f);
  pUniform1i(b->u_nl, 0);
  for (int i = 0; i < f->ndraws; i++)
    if (f->draws[i].pass == SR_PASS_SKY) draw_mesh(b, f->draws[i].mesh);
  pDepthMask(GL_TRUE);
  for (int i = 0; i < f->ndraws; i++) {
    const SrDraw *d = &f->draws[i];
    if (d->pass == SR_PASS_GLOW || d->pass == SR_PASS_HALO || d->pass == SR_PASS_SKY || d->pass == SR_PASS_VIEW)
      continue; /* glows after the solid geometry, the view model last */
    if (d->pass == SR_PASS_WORLD)
      pUniform2f(b->u_fogr, f->fog_start, f->fog_end);
    else if (d->pass == SR_PASS_FAR)
      pUniform2f(b->u_fogr, f->fog_end, f->fog_end * 3.5f);
    else
      pUniform2f(b->u_fogr, 1e6f, 2e6f);
    if (d->pass == SR_PASS_XRAY) pDisable(GL_DEPTH_TEST);
    set_lights(b, f, d);
    draw_mesh(b, d->mesh);
    if (d->pass == SR_PASS_XRAY) pEnable(GL_DEPTH_TEST);
  }
  if (f->inst_count > 0 && f->inst_mesh) {
    SrDraw crowd = {0};
    crowd.nlights = sr_select_lights(f->lights, f->nlights, f->eye, crowd.lights);
    pUniform2f(b->u_fogr, f->fog_start, f->fog_end);
    set_lights(b, f, &crowd);
    draw_instances(b, f);
  }
  /* glows: additive light that never hides what is behind it */
  pBlendFunc(GL_ONE, GL_ONE);
  pDepthMask(GL_FALSE);
  pUniform1i(b->u_glow, 1);
  pUniform1i(b->u_nl, 0);
  for (int i = 0; i < f->ndraws; i++) {
    const SrDraw *d = &f->draws[i];
    if (d->pass != SR_PASS_GLOW && d->pass != SR_PASS_HALO) continue;
    if (d->pass == SR_PASS_GLOW)
      pUniform2f(b->u_fogr, f->fog_start, f->fog_end);
    else
      pUniform2f(b->u_fogr, 1e6f, 2e6f);
    draw_mesh(b, d->mesh);
  }
  pUniform1i(b->u_glow, 0);
  pDepthMask(GL_TRUE);
  pBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  /* the player's hands and weapon: over a fresh depth buffer, so walls never cut them */
  bool cleared = false;
  for (int i = 0; i < f->ndraws; i++) {
    const SrDraw *d = &f->draws[i];
    if (d->pass != SR_PASS_VIEW) continue;
    if (!cleared) {
      pClear(GL_DEPTH_BUFFER_BIT);
      cleared = true;
    }
    pUniform2f(b->u_fogr, 1e6f, 2e6f);
    set_lights(b, f, d);
    draw_mesh(b, d->mesh);
  }
}

/* Small target for the meat-cam (the body's surroundings during a dive). */
static void ensure_inset(SrGlBackend *b, int w, int h) {
  if (b->inset_fbo && b->inset_w == w && b->inset_h == h) return;
  if (b->inset_fbo) {
    pDeleteFramebuffers(1, &b->inset_fbo);
    pDeleteTextures(1, &b->inset_color);
    pDeleteRenderbuffers(1, &b->inset_depth);
  }
  pGenTextures(1, &b->inset_color);
  pBindTexture(GL_TEXTURE_2D, b->inset_color);
  pTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  pTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  pGenRenderbuffers(1, &b->inset_depth);
  pBindRenderbuffer(GL_RENDERBUFFER, b->inset_depth);
  pRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
  pGenFramebuffers(1, &b->inset_fbo);
  pBindFramebuffer(GL_FRAMEBUFFER, b->inset_fbo);
  pFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, b->inset_color, 0);
  pFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, b->inset_depth);
  pBindFramebuffer(GL_FRAMEBUFFER, 0);
  b->inset_w = w;
  b->inset_h = h;
}

void sr_glb_render(SrGlBackend *b, const SrFrame *f, SrUi *ui, int ww, int wh, bool integer_scale) {
  if (!b->ready) return;
  b->frame++;
  b->draw_calls = 0;
  b->triangles = 0;
  int iw = f && f->internal_w > 0 ? f->internal_w : 480, ih = f && f->internal_h > 0 ? f->internal_h : 270;
  ensure_fbo(b, iw, ih);
  pActiveTexture(GL_TEXTURE0);
  if (f) {
    scene_pass(b, f, b->fbo, iw, ih);
    pDisable(GL_DEPTH_TEST);
    pDisable(GL_BLEND);
    if (f->bloom) render_bloom(b, iw, ih);
    pEnable(GL_BLEND);
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
    pUniform1i(b->p_bloom, 1);
    pUniform1f(b->p_bloom_k, f->bloom ? 0.75f : 0.0f);
    bool graded = f->grade_gain[0] > 0.0f; /* frames that set no grade (tests, the Matrix) pass through */
    pUniform3f(b->p_lift, graded ? f->grade_lift[0] : 0.0f, graded ? f->grade_lift[1] : 0.0f,
               graded ? f->grade_lift[2] : 0.0f);
    pUniform3f(b->p_gain, graded ? f->grade_gain[0] : 1.0f, graded ? f->grade_gain[1] : 1.0f,
               graded ? f->grade_gain[2] : 1.0f);
    pUniform1f(b->p_bright, (float)f->brightness);
    pUniform1i(b->p_vig, f->vignette ? 1 : 0);
    pActiveTexture(GL_TEXTURE1);
    pBindTexture(GL_TEXTURE_2D, f->bloom ? b->bloom_tex[0] : b->fbo_color);
    pActiveTexture(GL_TEXTURE0);
    pBindTexture(GL_TEXTURE_2D, b->fbo_color);
    pBindVertexArray(b->quad_vao);
    pDrawArrays(GL_TRIANGLES, 0, 6);
    b->draw_calls++;
  }
  /* meat-cam: the body's surroundings in a small grainy view at the top of the screen */
  if (f && f->inset) {
    const SrFrame *in = f->inset;
    int w = in->internal_w > 0 ? in->internal_w : 160, h = in->internal_h > 0 ? in->internal_h : 90;
    if (f->inset_fresh || !b->inset_fbo || b->inset_w != w || b->inset_h != h) {
      ensure_inset(b, w, h);
      scene_pass(b, in, b->inset_fbo, w, h);
      pDisable(GL_DEPTH_TEST);
    }
    pBindFramebuffer(GL_FRAMEBUFFER, 0);
    int vw = ww / 4, vh = vw * h / w;
    pViewport(ww / 2 - vw / 2, wh - vh - wh / 12, vw, vh);
    pUseProgram(b->prog_post);
    pActiveTexture(GL_TEXTURE0);
    pBindTexture(GL_TEXTURE_2D, b->inset_color);
    pUniform1i(b->p_tex, 0);
    pUniform1i(b->p_dither, 0);
    pUniform1i(b->p_quant, 1);
    pUniform1i(b->p_crt, 1);
    pUniform2f(b->p_res, (float)w, (float)h);
    pUniform1f(b->p_bloom_k, 0.0f);
    pUniform3f(b->p_lift, 0, 0, 0);
    pUniform3f(b->p_gain, 1, 1, 1);
    pUniform1f(b->p_bright, 0.0f);
    pUniform1i(b->p_vig, 0);
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
    pUniform4f(b->u_share, 0, 0, 0, 0);
    pUniform1i(b->u_glow, 0);
    pUniform1i(b->u_debug, 0);
    pUniform3f(b->u_band, 0, 0, 0);
    pUniform1f(b->u_mist, 0);
    pUniform1i(b->u_pixel, 0);
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
