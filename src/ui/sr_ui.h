/* Immediate-mode 2D UI built into a mesh (screen pixels, origin top-left): bitmap text, HUD,
 * menus, terminal, rotate prompt and the touch overlay. Pure C; drawn by the GL backend. */
#ifndef SR_UI_H
#define SR_UI_H

#include "input/sr_input.h"
#include "render/sr_render.h"

typedef struct SrUi {
  SrMesh mesh;
  int w, h;         /* UI resolution in pixels */
  int scale;        /* integer text scale */
  float px_per_css; /* device pixels per CSS pixel (1 on desktop) */
} SrUi;

void sr_ui_init(SrUi *ui);
void sr_ui_free(SrUi *ui);
/* Text scale from the screen height in CSS pixels (>= 2 so body text is >= 14 px). */
int sr_ui_auto_scale(float css_height);
void sr_ui_begin(SrUi *ui, int w, int h, int scale);
void sr_ui_rect(SrUi *ui, float x, float y, float w, float h, SrColor c);
void sr_ui_text(SrUi *ui, float x, float y, const char *s, SrColor c, int scale);
int sr_ui_text_width(const char *s, int scale);
int sr_ui_glyph_height(int scale);
void sr_ui_icon(SrUi *ui, float x, float y, float size, int poi, SrColor c);

/* Full UI for the current game state. portrait = device in portrait orientation. */
void sr_ui_build(SrUi *ui, const SrGame *g, const SrInput *in, bool portrait);
const char *sr_ui_context_label(SrContextKind k);
/* Icon (an SrPoi icon layer index) for an interaction, -1 when there is nothing to do. */
int sr_ui_context_icon(const SrGame *g, const SrContext *c);
/* Shop row under a screen y coordinate, -1 above the list. */
int sr_ui_shop_row_at(const SrUi *ui, float y);

/* Navigation and help (sr_ui_nav.c). */
/* The city map screen and its touch buttons: 1 zoom in, 2 zoom out, 3 filter, 4 clear, 5 close, 0 none. */
void sr_ui_map_screen(SrUi *ui, const SrGame *g, const SrInput *in);
int sr_ui_map_button_at(const SrUi *ui, float x, float y);
/* The help screen; the topic row under a screen y (-1 none), and on the settings page the setting
 * under a screen y (0 tips and goal, 1 minimap, 2 route on the pavement, -1 none). */
void sr_ui_help_screen(SrUi *ui, const SrGame *g, const SrInput *in);
int sr_ui_help_row_at(const SrUi *ui, float y);
int sr_ui_help_setting_at(const SrUi *ui, const SrGame *g, bool touch, float y);
/* The destination marker on the compass bar, with distance and the next turn under it. */
void sr_ui_nav_compass(SrUi *ui, const SrGame *g, float cx, float cw, float y, int s);
/* The objective, minimap and tip panel over the street HUD. */
void sr_ui_nav_overlay(SrUi *ui, const SrGame *g, const SrInput *in);

#endif
