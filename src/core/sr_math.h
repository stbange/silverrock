/* Float vector and matrix math for simulation and rendering (not world generation). */
#ifndef SR_MATH_H
#define SR_MATH_H

#include "core/sr_types.h"

#define SR_PI 3.14159265358979323846f

typedef struct SrVec2 { float x, y; } SrVec2;
typedef struct SrVec3 { float x, y, z; } SrVec3;
typedef struct SrVec4 { float x, y, z, w; } SrVec4;
/* Column-major 4x4 matrix, as OpenGL expects. */
typedef struct SrMat4 { float m[16]; } SrMat4;

SrVec3 sr_v3(float x, float y, float z);
SrVec3 sr_v3_add(SrVec3 a, SrVec3 b);
SrVec3 sr_v3_sub(SrVec3 a, SrVec3 b);
SrVec3 sr_v3_scale(SrVec3 a, float s);
float sr_v3_dot(SrVec3 a, SrVec3 b);
SrVec3 sr_v3_cross(SrVec3 a, SrVec3 b);
float sr_v3_len(SrVec3 a);
SrVec3 sr_v3_norm(SrVec3 a);
SrVec3 sr_v3_lerp(SrVec3 a, SrVec3 b, float t);
float sr_v2_len(SrVec2 a);

float sr_clampf(float v, float lo, float hi);
float sr_lerpf(float a, float b, float t);
float sr_wrap_angle(float a);

SrMat4 sr_m4_identity(void);
SrMat4 sr_m4_mul(SrMat4 a, SrMat4 b);
SrMat4 sr_m4_translate(SrVec3 t);
SrMat4 sr_m4_scale(SrVec3 s);
SrMat4 sr_m4_rotate_y(float radians);
SrMat4 sr_m4_perspective(float fovy_radians, float aspect, float znear, float zfar);
SrMat4 sr_m4_ortho(float l, float r, float b, float t, float n, float f);
/* First-person view matrix from eye position, yaw (around Y) and pitch. */
SrMat4 sr_m4_fps_view(SrVec3 eye, float yaw, float pitch);
SrVec4 sr_m4_mul_v4(SrMat4 m, SrVec4 v);
/* Forward vector for a yaw/pitch pair (yaw 0 looks along -Z). */
SrVec3 sr_dir_from_yaw_pitch(float yaw, float pitch);

#endif
