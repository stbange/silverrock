/* Guidance and navigation: a destination and a route along the pavements (set from the map or
 * following the active job), turn hints, one-time tips triggered by what happens, the current
 * goal, and the city map's view. Presentation and convenience only: nothing here changes the
 * simulation or the saves; the tips seen live in the settings file. */
#ifndef SR_GUIDE_H
#define SR_GUIDE_H

#include "core/sr_math.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct SrGame SrGame;

#define SR_ROUTE_MAX    96
#define SR_ROUTE_ARRIVE 8.0f /* metres from the destination that count as arrived */
#define SR_ROUTE_REPLAN 60   /* steps between replans (one second) */
#define SR_TIP_SECONDS  9.0  /* how long a tip stays up */

typedef enum { SR_DEST_NONE, SR_DEST_BUILDING, SR_DEST_POINT, SR_DEST_STATION } SrDestKind;

typedef struct SrRoute {
  uint8_t kind;     /* SrDestKind */
  bool automatic;   /* following the active job */
  int32_t building; /* SR_DEST_BUILDING; station index for SR_DEST_STATION */
  int32_t job;      /* the job followed, -1 */
  SrVec3 target;    /* where the route ends (door, point or metro entrance), metres */
  char label[48];
  int n; /* route points from near the player to the target */
  SrVec3 pts[SR_ROUTE_MAX];
  float length;          /* metres along the route */
  uint64_t planned;      /* step of the last plan */
  int32_t declined_job;  /* the job whose route was cleared by hand: not followed again */
  int32_t done_building; /* reached for the followed job: not routed to again until the job moves on */
} SrRoute;

typedef enum SrTip {
  SR_TIP_WELCOME,
  SR_TIP_MAP,
  SR_TIP_ROUTE,
  SR_TIP_INTERACT,
  SR_TIP_HUNGER,
  SR_TIP_FATIGUE,
  SR_TIP_RENT,
  SR_TIP_JOBS,
  SR_TIP_JOB_ROUTE,
  SR_TIP_HEAT,
  SR_TIP_SITE,
  SR_TIP_COMBAT,
  SR_TIP_HURT,
  SR_TIP_MATRIX,
  SR_TIP_METRO,
  SR_TIP_TERMINAL,
  SR_TIP_HELP,
  SR_TIP_COUNT
} SrTip;

typedef struct SrGuide {
  int tip;      /* showing, -1 none */
  double tip_t; /* game seconds it appeared */
  int queue[8], nq;
  uint64_t seen; /* bit per SrTip, mirrored in the settings (tips_seen) */
  bool site_seen, dive_seen, terminal_seen;
} SrGuide;

/* The city map's view: centre, metres per screen height, filter, follow the player. */
typedef enum { SR_MAPF_ALL, SR_MAPF_FOOD, SR_MAPF_SERVICES, SR_MAPF_WORK, SR_MAPF_TRAVEL, SR_MAPF_COUNT } SrMapFilter;
typedef struct SrMapView {
  float cx, cz, span;
  int filter;
  float aspect; /* of the screen showing it (set by the platform), 16:9 until then */
} SrMapView;

/* destinations and routes */
void sr_route_set_building(SrGame *g, int building);
void sr_route_set_point(SrGame *g, float x, float z);
void sr_route_set_station(SrGame *g, int station);
void sr_route_clear(SrGame *g);
/* Replans the route from the player now; false when there is no route. */
bool sr_route_plan(SrGame *g);
/* Distance in metres to the destination along the route (straight line inside sites). */
float sr_route_distance(const SrGame *g);
/* The next instruction, such as "TURN LEFT IN 40 M" or "DESTINATION AHEAD"; empty without a route. */
void sr_route_instruction(const SrGame *g, char *out, int cap);
/* Metro entrance of a station (metres). */
SrVec3 sr_station_entrance(const SrGame *g, int station);

/* guidance */
void sr_guide_init(SrGuide *gd);
/* A new or loaded game: no destination, no tip showing (tips seen are kept), the map recentred. */
void sr_guide_new_game(SrGame *g);
/* One step: follows the job, replans, checks arrival and the tip triggers. */
void sr_guide_step(SrGame *g);
/* Shows a tip now if guidance is on and it was not shown before; true when it was queued. */
bool sr_guide_tip(SrGame *g, SrTip tip);
/* The tip showing (NULL if none) and its strength for fading in and out. */
const char *sr_guide_tip_text(const SrGame *g, bool touch, float *alpha);
const char *sr_guide_tip_name(SrTip tip);
/* The current goal, such as "JOB: BREAK-IN AT ... (420 M)". Empty when guidance is off. */
void sr_guide_objective(const SrGame *g, char *out, int cap);
/* Forget the tips seen (they show again). */
void sr_guide_reset(SrGame *g);
/* The help page's settings: 0 tips and goal, 1 minimap, 2 route on the pavement flip; 3 forgets the
 * tips seen. The caller saves the settings. */
typedef enum { SR_GUIDE_SET_GUIDANCE, SR_GUIDE_SET_MINIMAP, SR_GUIDE_SET_NAV_PATH, SR_GUIDE_SET_RESET } SrGuideSetting;
void sr_guide_toggle(SrGame *g, SrGuideSetting which);

/* the city map */
void sr_map_open(SrGame *g);
/* Zoom limits (metres per screen height): a street corner to the whole city. */
#define SR_MAP_MIN_SPAN 150.0f
float sr_map_max_span(const SrGame *g);
/* Zooms the map by a factor (below 1 closer), within the limits. */
void sr_map_zoom(SrGame *g, float factor);
void sr_map_close(SrGame *g);
/* World position under a point of the map screen (nx, ny in 0..1). */
void sr_map_world_at(const SrMapView *v, float aspect, float nx, float ny, float *x, float *z);
/* Screen position (0..1) of a world point. */
void sr_map_screen_of(const SrMapView *v, float aspect, float x, float z, float *nx, float *ny);
/* Sets the destination at a map point: the nearest place of the filter within reach, else the point. */
void sr_map_pick(SrGame *g, float aspect, float nx, float ny);
bool sr_map_shows_poi(int filter, int poi);
const char *sr_map_filter_name(int filter);

/* help */
#define SR_HELP_PAGES 8
const char *sr_help_title(int page);
/* Lines of a help page (NULL-terminated array of at most 14). */
const char *const *sr_help_lines(int page, bool touch);
void sr_help_open(SrGame *g);
void sr_help_close(SrGame *g);

#endif
