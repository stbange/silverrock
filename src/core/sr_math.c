#include "core/sr_math.h"

#include <math.h>
#include <string.h>

SrVec3 sr_v3(float x, float y, float z) { return (SrVec3){x, y, z}; }
SrVec3 sr_v3_add(SrVec3 a, SrVec3 b) { return (SrVec3){a.x + b.x, a.y + b.y, a.z + b.z}; }
SrVec3 sr_v3_sub(SrVec3 a, SrVec3 b) { return (SrVec3){a.x - b.x, a.y - b.y, a.z - b.z}; }
SrVec3 sr_v3_scale(SrVec3 a, float s) { return (SrVec3){a.x * s, a.y * s, a.z * s}; }
float sr_v3_dot(SrVec3 a, SrVec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
SrVec3 sr_v3_cross(SrVec3 a, SrVec3 b) {
  return (SrVec3){a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float sr_v3_len(SrVec3 a) { return sqrtf(sr_v3_dot(a, a)); }
SrVec3 sr_v3_norm(SrVec3 a) {
  float l = sr_v3_len(a);
  return l > 1e-8f ? sr_v3_scale(a, 1.0f / l) : (SrVec3){0, 0, 0};
}
SrVec3 sr_v3_lerp(SrVec3 a, SrVec3 b, float t) { return sr_v3_add(a, sr_v3_scale(sr_v3_sub(b, a), t)); }
float sr_v2_len(SrVec2 a) { return sqrtf(a.x * a.x + a.y * a.y); }

float sr_clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
float sr_lerpf(float a, float b, float t) { return a + (b - a) * t; }
float sr_wrap_angle(float a) {
  while (a > SR_PI) a -= 2.0f * SR_PI;
  while (a < -SR_PI) a += 2.0f * SR_PI;
  return a;
}

SrMat4 sr_m4_identity(void) {
  SrMat4 r;
  memset(&r, 0, sizeof r);
  r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
  return r;
}

SrMat4 sr_m4_mul(SrMat4 a, SrMat4 b) {
  SrMat4 r;
  for (int c = 0; c < 4; c++) {
    for (int row = 0; row < 4; row++) {
      float s = 0.0f;
      for (int k = 0; k < 4; k++) s += a.m[k * 4 + row] * b.m[c * 4 + k];
      r.m[c * 4 + row] = s;
    }
  }
  return r;
}

SrMat4 sr_m4_translate(SrVec3 t) {
  SrMat4 r = sr_m4_identity();
  r.m[12] = t.x;
  r.m[13] = t.y;
  r.m[14] = t.z;
  return r;
}

SrMat4 sr_m4_scale(SrVec3 s) {
  SrMat4 r = sr_m4_identity();
  r.m[0] = s.x;
  r.m[5] = s.y;
  r.m[10] = s.z;
  return r;
}

SrMat4 sr_m4_rotate_y(float a) {
  SrMat4 r = sr_m4_identity();
  float c = cosf(a), s = sinf(a);
  r.m[0] = c;
  r.m[2] = -s;
  r.m[8] = s;
  r.m[10] = c;
  return r;
}

SrMat4 sr_m4_perspective(float fovy, float aspect, float n, float f) {
  SrMat4 r;
  memset(&r, 0, sizeof r);
  float t = 1.0f / tanf(fovy * 0.5f);
  r.m[0] = t / aspect;
  r.m[5] = t;
  r.m[10] = (f + n) / (n - f);
  r.m[11] = -1.0f;
  r.m[14] = (2.0f * f * n) / (n - f);
  return r;
}

SrMat4 sr_m4_ortho(float l, float rr, float b, float t, float n, float f) {
  SrMat4 r = sr_m4_identity();
  r.m[0] = 2.0f / (rr - l);
  r.m[5] = 2.0f / (t - b);
  r.m[10] = -2.0f / (f - n);
  r.m[12] = -(rr + l) / (rr - l);
  r.m[13] = -(t + b) / (t - b);
  r.m[14] = -(f + n) / (f - n);
  return r;
}

SrVec3 sr_dir_from_yaw_pitch(float yaw, float pitch) {
  float cp = cosf(pitch);
  return (SrVec3){-sinf(yaw) * cp, sinf(pitch), -cosf(yaw) * cp};
}

SrMat4 sr_m4_fps_view(SrVec3 eye, float yaw, float pitch) {
  SrVec3 f = sr_dir_from_yaw_pitch(yaw, pitch);
  SrVec3 s = sr_v3_norm(sr_v3_cross(f, (SrVec3){0, 1, 0}));
  SrVec3 u = sr_v3_cross(s, f);
  SrMat4 r = sr_m4_identity();
  r.m[0] = s.x;
  r.m[4] = s.y;
  r.m[8] = s.z;
  r.m[1] = u.x;
  r.m[5] = u.y;
  r.m[9] = u.z;
  r.m[2] = -f.x;
  r.m[6] = -f.y;
  r.m[10] = -f.z;
  r.m[12] = -sr_v3_dot(s, eye);
  r.m[13] = -sr_v3_dot(u, eye);
  r.m[14] = sr_v3_dot(f, eye);
  return r;
}

SrVec4 sr_m4_mul_v4(SrMat4 m, SrVec4 v) {
  return (SrVec4){m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z + m.m[12] * v.w,
                  m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z + m.m[13] * v.w,
                  m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14] * v.w,
                  m.m[3] * v.x + m.m[7] * v.y + m.m[11] * v.z + m.m[15] * v.w};
}
