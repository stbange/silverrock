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

#endif
