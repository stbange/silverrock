/* The player's side of the Matrix: the dive (transient) and the network state that persists
 * across dives and saves (ownership tokens, backdoors, forged credentials, pending tamper
 * detections, host alerts, blackouts, the loaded programs). */
#ifndef SR_NET_H
#define SR_NET_H

#include "sim/sr_matrix.h"

#define SR_NET_MAX_TOKENS    48
#define SR_NET_MAX_BACKDOORS 16
#define SR_NET_MAX_CREDS     16
#define SR_NET_MAX_TAMPER    48
#define SR_NET_MAX_ALERTS    16
#define SR_BODY_ALERT_M      15.0f
#define SR_DUMPSHOCK_DAMAGE  15.0f
#define SR_DUMPSHOCK_STUN    3.0f
#define SR_DISPATCH_ETA      90 /* in-game seconds until a traced body gets visitors */
#define SR_BLACKOUT_S        (20 * 60)
#define SR_WIRELESS_SPEED    0.6f
#define SR_CAMERA_CHECK_S    20.0f /* a disabled camera draws a guard after this delay */

/* Extra device delta bits for Matrix control (doors and cameras also use SR_DV_*). */
#define SR_DV_LOOPED 4
#define SR_DV_FOF    8
/* Elevator floors are kept under their own delta id. */
uint32_t sr_net_elevator_id(uint32_t device_id);

typedef enum SrAccess { SR_ACCESS_RIG, SR_ACCESS_BOOTH, SR_ACCESS_PORT, SR_ACCESS_WIRELESS, SR_ACCESS_SPIKE } SrAccess;

typedef struct SrToken {
  uint32_t device; /* device delta id */
  int32_t building;
  int64_t expires;
} SrToken;

typedef struct SrBackdoor {
  int32_t building;
  int64_t discover_at;
} SrBackdoor;

typedef struct SrCred {
  int32_t building;
  int64_t expires;
} SrCred;

typedef struct SrTamper {
  uint32_t device;
  int32_t building;
  uint8_t before; /* delta value before the change, 255 = unset */
  bool logs_erased;
  int64_t detect_at;
} SrTamper;

typedef struct SrHostAlert {
  int32_t building;
  uint8_t level;
  int64_t until;
} SrHostAlert;

typedef struct SrNet {
  int token_count;
  SrToken tokens[SR_NET_MAX_TOKENS];
  int backdoor_count;
  SrBackdoor backdoors[SR_NET_MAX_BACKDOORS];
  int cred_count;
  SrCred creds[SR_NET_MAX_CREDS];
  int tamper_count;
  SrTamper tamper[SR_NET_MAX_TAMPER];
  int alert_count;
  SrHostAlert alerts[SR_NET_MAX_ALERTS];
  int8_t blackout_district; /* -1 none */
  int64_t blackout_until;
  SrLoadout programs;
  bool netsight;
  /* a traced body: security is on its way */
  bool dispatch;
  SrVec3 dispatch_pos;
  int64_t dispatch_at;
  int detections; /* tamper detections so far */
} SrNet;

typedef enum SrNodeAction {
  SR_NA_NONE,
  SR_NA_JACK_OUT,
  SR_NA_WORM,
  SR_NA_UNLOCK,
  SR_NA_LOCK,
  SR_NA_LOOP,
  SR_NA_DISABLE,
  SR_NA_RESTORE,
  SR_NA_FEED,
  SR_NA_FOF,
  SR_NA_CANCEL_ALARM,
  SR_NA_FALSE_ALARM,
  SR_NA_LIGHTS_OFF,
  SR_NA_LIGHTS_ON,
  SR_NA_ELEVATOR,
  SR_NA_DOWNLOAD,
  SR_NA_ERASE_LOGS,
  SR_NA_ERASE_WARRANT,
  SR_NA_FORGE_CREDS,
  SR_NA_ALTER_ACCOUNT,
  SR_NA_BLACKOUT,
  SR_NA_OWN_ALL,
  SR_NA_ENTER_HOST,
  /* the corporate game */
  SR_NA_PLANT_STORY,
  SR_NA_KILL_STORY,
  SR_NA_TAMPER_VOTE,
  SR_NA_LEAK_BRIBES,
  SR_NA_COUNT
} SrNodeAction;

typedef struct SrDive {
  bool active;
  uint8_t access;
  float speed; /* action speed: wireless dives are slower */
  bool in_host;
  int32_t building; /* host's building, -1 on the global grid */
  SrHost host;
  SrInterior in;
  SrVec3 pos; /* persona position (grid or host space) */
  float yaw, pitch;
  SrPersona persona;
  uint8_t open[SR_HOST_MAX_NODES]; /* breached or bypassed nodes */
  int ice_count;
  SrIce ice[SR_MX_MAX_ICE];
  int alert;   /* 0 calm .. 3 lockdown */
  float trace; /* 0..1 */
  bool traced;
  SrBreach breach;
  int breach_node; /* -1 when no breach is running */
  int download_node;
  float download_left, download_total;
  bool logs_erased;
  float veil, shield;
  int feed_device; /* camera whose feed is shown, -1 none */
  bool body_alert;
  int64_t started;
  int nodes_hacked;
} SrDive;

const char *sr_node_action_name(SrNodeAction a);

#endif
