/* Character animation: an 11-part rigid skeleton (pelvis, chest, head, two-part arms and legs,
 * plus feet and hands) posed by keyframe clips written in code, procedural gaits with planted
 * feet, blending, and a render-side cache keyed by agent. Presentation only: nothing here feeds
 * back into the simulation or the saves. */
#ifndef SR_ANIM_H
#define SR_ANIM_H

#include "core/sr_math.h"
#include "sim/sr_population.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct SrMesh SrMesh;

/* Joint channels of a pose. Angles in radians, offsets in metres. Positive values:
 * ROOT_PITCH tips the whole body forward about the feet, ROOT_ROLL to its -x side;
 * hips and shoulders swing the limb forward, knees bend the foot backward, elbows the hand
 * forward; abduction (HIPA, SHOA) moves the limb outward; chest and head pitch lean forward;
 * yaw turns toward +x. FX is not a joint: it carries the glow of a held cigarette or phone. */
typedef enum SrJoint {
  SR_J_ROOT_PITCH,
  SR_J_ROOT_ROLL,
  SR_J_ROOT_Y,
  SR_J_PELVIS_X,
  SR_J_PELVIS_Y,
  SR_J_PELVIS_PITCH,
  SR_J_PELVIS_ROLL,
  SR_J_PELVIS_YAW,
  SR_J_CHEST_PITCH,
  SR_J_CHEST_YAW,
  SR_J_CHEST_ROLL,
  SR_J_HEAD_PITCH,
  SR_J_HEAD_YAW,
  SR_J_HIP_L,
  SR_J_HIP_R,
  SR_J_HIPA_L,
  SR_J_HIPA_R,
  SR_J_KNEE_L,
  SR_J_KNEE_R,
  SR_J_SHO_L,
  SR_J_SHO_R,
  SR_J_SHOA_L,
  SR_J_SHOA_R,
  SR_J_ELB_L,
  SR_J_ELB_R,
  SR_J_FX,
  SR_J_COUNT
} SrJoint;

typedef struct SrPose {
  float j[SR_J_COUNT];
} SrPose;

/* Body measures from the appearance (metres). */
typedef struct SrSkel {
  float tall, bw, hip, thigh, shin, ankle, ua, fa, hand, hs, chest_h, neck_y, sho_x, sho_y, hip_x, leg_w;
  /* the body's shape: half widths and depths (m) and the way it moves */
  float waist, hip_w, chest_d, belly, bust, arm_w, foot;
  float sway;     /* hip sway in the walk */
  float arm_out;  /* arms held away from the body (muscle, bulk) */
  float stance;   /* feet apart */
  float stride_k; /* stride relative to leg length */
  bool female;
  uint8_t build;
} SrSkel;

typedef enum SrClip {
  SR_CLIP_NONE,
  /* gaits: procedural, driven by distance walked */
  SR_CLIP_WALK,
  SR_CLIP_CROUCH_WALK,
  SR_CLIP_LIMP,
  /* looping clips driven by time */
  SR_CLIP_IDLE,
  SR_CLIP_CROUCH,
  SR_CLIP_SIT,
  SR_CLIP_SLEEP,
  SR_CLIP_EAT,
  SR_CLIP_BUY,
  SR_CLIP_LEAN,
  SR_CLIP_SMOKE,
  SR_CLIP_PHONE,
  SR_CLIP_CALL,
  SR_CLIP_WINDOW,
  SR_CLIP_WAIT,
  SR_CLIP_DANCE,
  SR_CLIP_WATCH,
  SR_CLIP_POINT,
  SR_CLIP_WARM,
  SR_CLIP_BEG,
  SR_CLIP_TALK,
  SR_CLIP_LISTEN,
  SR_CLIP_FILM,
  SR_CLIP_COWER,
  SR_CLIP_LOOK_AROUND,
  SR_CLIP_CROUCH_AIM,
  SR_CLIP_PEEK,
  SR_CLIP_SURRENDER,
  SR_CLIP_RESTRAINED,
  SR_CLIP_STUNNED,
  SR_CLIP_LYING_BACK,
  SR_CLIP_LYING_FRONT,
  SR_CLIP_LYING_SIDE,
  /* overlays: only the joints they key replace the base */
  SR_CLIP_AIM,
  SR_CLIP_AIM_PISTOL,
  SR_CLIP_READY,
  SR_CLIP_SLUNG,
  SR_CLIP_BEAT,
  SR_CLIP_WAVE,
  SR_CLIP_STOP,
  SR_CLIP_UMBRELLA,
  SR_CLIP_COVER_HEAD,
  SR_CLIP_FLEE_ARMS,
  /* one-shots that replace the base and hold their last key */
  SR_CLIP_DIE_BACK,
  SR_CLIP_DIE_FRONT,
  SR_CLIP_DIE_SIDE,
  SR_CLIP_SLUMP,
  SR_CLIP_GETUP,
  /* additive one-shots that start and end at rest */
  SR_CLIP_FLINCH,
  SR_CLIP_STAGGER,
  SR_CLIP_HEADSHOT,
  SR_CLIP_SHAKE,
  SR_CLIP_RECOIL,
  SR_CLIP_COUNT
} SrClip;

typedef enum SrClipKind { SR_CK_GAIT, SR_CK_LOOP, SR_CK_OVERLAY, SR_CK_BASE_ONCE, SR_CK_ADD_ONCE } SrClipKind;

/* Things held in a hand. */
typedef enum SrHandProp {
  SR_HP_NONE,
  SR_HP_BRIEFCASE,
  SR_HP_BAG,
  SR_HP_BOX,
  SR_HP_CIGARETTE,
  SR_HP_PHONE,
  SR_HP_UMBRELLA,
  SR_HP_UMBRELLA_CLOSED,
  SR_HP_PISTOL,
  SR_HP_RIFLE,
  SR_HP_BATON,
  SR_HP_COUNT
} SrHandProp;

/* Facial expressions (columns of the face atlas). Kept small and serious: brows, lids and mouth. */
typedef enum SrExpression {
  SR_EXPR_NEUTRAL,
  SR_EXPR_STERN,
  SR_EXPR_FEAR,
  SR_EXPR_PAIN,
  SR_EXPR_TALK,
  SR_EXPR_SURPRISE,
  SR_EXPR_CLOSED,
  SR_EXPR_CONTENT,
  SR_EXPR_COUNT,
  SR_EXPR_AUTO = 255 /* chosen from the clips, the role and the time */
} SrExpression;

/* What the simulation says about one person this frame. */
typedef struct SrAnimInput {
  uint64_t key;
  const SrAppearance *look;
  uint8_t role;
  SrVec3 pos, dir;
  float speed;    /* m/s now */
  float walk;     /* metres walked */
  uint8_t base;   /* clip when standing, or a gait override (crouch walk, limp) */
  uint8_t upper;  /* overlay clip */
  uint8_t once;   /* one-shot clip (base or additive) */
  double once_t0; /* game seconds the one-shot started; < 0: already finished */
  bool mirror;    /* mirror the one-shot left to right */
  uint8_t prop_r, prop_l;
  bool look_at;
  SrVec3 target;
  float hunch; /* 0..1: rain posture */
  bool hood;   /* hood up */
  float wind_x, wind_z;
  uint8_t expr;  /* SrExpression; 0 (neutral) unless set; SR_EXPR_AUTO picks one */
  double face_t; /* game seconds for blinking and talking (set by sr_anim_character) */
} SrAnimInput;

#define SR_ANIM_SLOTS 512

typedef struct SrAnimSlot {
  uint64_t key;
  double seen;
  bool used;
  uint32_t sig;
  double blend_t0;
  float blend_len;
  float heading, phase, last_walk, lean;
  SrPose snap, out;
} SrAnimSlot;

typedef struct SrAnimCache {
  double now;
  float dt;
  int live;
  SrAnimSlot slot[SR_ANIM_SLOTS];
} SrAnimCache;

/* skeleton and clips */
void sr_anim_skel(const SrAppearance *a, SrSkel *out);
SrClipKind sr_anim_clip_kind(int clip);
float sr_anim_clip_len(int clip);
const char *sr_anim_clip_name(int clip);
bool sr_anim_clip_keys_joint(int clip, int joint); /* does the clip key this joint */
void sr_anim_limits(int joint, float *lo, float *hi);
/* Samples a keyframe clip at phase 0..1 (looping clips wrap). Unkeyed joints are 0. */
void sr_anim_sample(int clip, float phase, SrPose *out);
/* Metres per gait cycle at a speed. */
float sr_anim_stride(const SrSkel *s, float speed, int base);
/* Procedural gait at a cycle phase. */
void sr_anim_gait(const SrSkel *s, float speed, float phase, int base, uint64_t key, SrPose *out);
/* The full target pose of a person (no blending): gait or base clip, posture, overlay,
 * one-shot, hunch, limits and grounding. */
void sr_anim_pose(const SrAnimInput *in, const SrSkel *s, double now, float gait_phase, SrPose *out);
void sr_anim_mirror(SrPose *p);
void sr_anim_clamp(SrPose *p);
/* Local positions (body space, feet at the origin, facing +z): 0/1 ankles, 2/3 hands, 4 head top. */
SrVec3 sr_anim_point(const SrSkel *s, const SrPose *p, int which);
/* Builds the posed character mesh. */
void sr_anim_mesh(SrMesh *m, const SrAnimInput *in, const SrSkel *s, const SrPose *p, float yaw);
/* Digest of every keyframe clip sampled at 16 phases. */
uint64_t sr_anim_digest(void);

/* cache: begin a frame, animate people, end the frame (drops entries not seen for 2 s) */
void sr_anim_cache_init(SrAnimCache *c);
void sr_anim_cache_begin(SrAnimCache *c, double now);
/* Animates and draws one person. A NULL cache draws the target pose without blending. */
void sr_anim_character(SrAnimCache *c, const SrAnimInput *in, SrMesh *out);
void sr_anim_cache_end(SrAnimCache *c);
/* The pose drawn for a key last frame (NULL when absent). */
const SrAnimSlot *sr_anim_cache_find(const SrAnimCache *c, uint64_t key);

/* The expression a person shows: fear in flight and cover, pain when hit, a moving mouth when
 * talking, closed eyes asleep or down, stern for guards, police and gangers and anyone aiming, a
 * blink every few seconds, otherwise a calm neutral face. */
SrExpression sr_anim_expression(const SrAnimInput *in, double now);
/* Face style of an appearance: 0 plain, 1 stubble, 2 scar, 3 tattoo. */
int sr_anim_face_style(const SrAppearance *a);
/* The face atlas cell for an expression: the style, on the feminine set for women. */
int sr_anim_face_cell(const SrAppearance *a, int expr);

/* Hand prop carried by an appearance (briefcase, bag, box, cigarette, phone or none). */
int sr_anim_accessory(const SrAppearance *a);
bool sr_anim_has_coat(const SrAppearance *a);
/* Sleeves of the outfit: 0 long, 1 short (bare forearms), 2 none (bare arms). */
int sr_anim_sleeves(const SrAppearance *a);
/* A skirt or the hem of a dress over the thighs. */
bool sr_anim_has_skirt(const SrAppearance *a);
/* Hair: 0-3 crest, 4-8 short or bob, 9 long, 10 ponytail, 11 shaved. */
int sr_anim_hair_style(const SrAppearance *a);
bool sr_anim_has_backpack(const SrAppearance *a);

#endif
