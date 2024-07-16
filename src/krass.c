#include "krass.h"

struct krass_ctx {

};

krass_ctx_t *krass_init(void) {}

void krass_destroy(krass_ctx_t *ctx) {}

void krass_finalize(krass_ctx_t *ctx) {}

bool krass_tick(krass_ctx_t *ctx) {}

float krass_progress(krass_ctx_t *ctx) {}

int krass_reserve_quad(krass_ctx_t *ctx, krass_dim_t dim, krass_draw_callback_t cb, void *data) {}

void krass_draw(krass_ctx_t *ctx, int id, float dx, float dy) {}

void krass_draw_scaled(krass_ctx_t *ctx, int id, float dx, float dy, float dw, float dh) {}

kr_image_t *krass_get_asset(krass_ctx_t *ctx, int id, krass_quad_t *quad) {}


#ifdef KR_FULL_RGBA_FONTS
int krass_reserve_quad_font(krass_ctx_t *ctx, const char *fontpath, int size, int font_index) {}

kr_ttf_font_t *krass_get_font(krass_ctx_t *ctx, int id) {}
#else
#define krass_reserve_quad_font(ctx, fontpath, size, font_index) -1
#define krass_get_font(ctx, id) NULL
#endif
