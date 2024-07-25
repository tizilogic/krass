#include "krass.h"

#include "internal/pack.c.h"

#include <krink/memory.h>

#include <assert.h>
#include <string.h>

typedef enum krass_type {
	KRASS_TYPE_IMAGE,
	KRASS_TYPE_FONT,
} krass_type_t;

typedef struct krass_image {
	int pack_id;
	krass_quad_t quad;
	krass_draw_callback_t cb;
	void *data;
} krass_image_t;

typedef struct krass_font {
	int pack_id;
	const char *fontpath;
	int size;
	int font_index;
	kr_ttf_font_t font;
} krass_font_t;

typedef union krass_data {
	krass_image_t image;
	krass_font_t font;
} krass_data_t;

typedef struct krass_asset {
	krass_type_t type;
	krass_data_t data;
} krass_asset_t;

struct krass_ctx {
	kr_image_t *img;
	krass_canvas_t canvas;
	krass_asset_t *assets;
	int top, cap, cursor;
#ifdef KR_FULL_RGBA_FONTS
	int font_count;
#endif
};

krass_ctx_t *krass_init(int reserve) {
	assert(reserve > 0);
	krass_ctx_t *ctx = (krass_ctx_t *)kr_malloc(sizeof(krass_ctx_t));
	assert(ctx != NULL);
	memset(ctx, 0, sizeof(krass_ctx_t));
	krass_pack_init(&ctx->canvas, reserve);
	ctx->assets = (krass_asset_t *)kr_malloc(reserve * sizeof(krass_asset_t));
	assert(ctx->assets != NULL);
	ctx->cap = reserve;
	ctx->cursor = -1;
	return ctx;
}

void krass_destroy(krass_ctx_t *ctx) {
	if (ctx->img != NULL) {
		kr_image_destroy(ctx->img);
		kr_free(ctx->img);
	}
	krass_pack_destroy(&ctx->canvas);
	// TODO: destroy fonts
}

#ifdef KR_FULL_RGBA_FONTS
static void load_fonts(krass_ctx_t *ctx) {
	int remaining = ctx->font_count;
	int cursor = 0;
	while (remaining > 0) {
		while (ctx->assets[cursor].type != KRASS_TYPE_FONT) {
			++cursor;
			if (cursor >= ctx->cap) {
				kinc_log(KINC_LOG_LEVEL_ERROR, "Font count appears wrong");
				return;
			}
		}
		kr_ttf_font_init(&ctx->assets[cursor].data.font.font,
		                 ctx->assets[cursor].data.font.fontpath,
		                 ctx->assets[cursor].data.font.font_index);
		kr_ttf_load(&ctx->assets[cursor].data.font.font, ctx->assets[cursor].data.font.size);
		int width = kr_ttf_get_texture(&ctx->assets[cursor].data.font.font,
		                               ctx->assets[cursor].data.font.size)
		                ->tex_width;
		int height = kr_ttf_get_first_unused_y(&ctx->assets[cursor].data.font.font,
		                                       ctx->assets[cursor].data.font.size);
		ctx->assets[cursor].data.font.pack_id = krass_pack_add_rect(&ctx->canvas, width, height);
        --remaining;
	}
}
#else
#define load_fonts(ctx)
#endif

void krass_finalize(krass_ctx_t *ctx) {
	load_fonts(ctx);
	krass_pack_compute(&ctx->canvas);
	ctx->cursor = 0;
}

bool krass_tick(krass_ctx_t *ctx) {
	if (ctx->cursor < 0) {
		kinc_log(KINC_LOG_LEVEL_ERROR, "Called tick on non finalized context");
		return true;
	}
	if (ctx->cursor - 2 > ctx->top) return false;
	// TODO: set scissor and run callback
	return true;
}

float krass_progress(krass_ctx_t *ctx) {
	if (ctx->cursor < 0) {
		kinc_log(KINC_LOG_LEVEL_ERROR, "Called progress on non finalized context");
		return 0;
	}
	return (float)ctx->cursor / (float)(ctx->top + 1);
}

static void grow_data(krass_ctx_t *ctx) {
	if (ctx->top >= ctx->cap) {
		ctx->assets =
		    (krass_asset_t *)kr_realloc(ctx->assets, (ctx->top + 1) * sizeof(krass_asset_t));
		ctx->cap = ctx->top + 1;
		assert(ctx->assets != NULL);
	}
}

int krass_reserve_quad(krass_ctx_t *ctx, krass_dim_t dim, krass_draw_callback_t cb, void *data) {
	if (ctx->cursor > -1) {
		kinc_log(KINC_LOG_LEVEL_ERROR, "Cannot reserve on finalized context");
		return -1;
	}
	grow_data(ctx);
	int id = krass_pack_add_rect(&ctx->canvas, dim.width, dim.height);
	ctx->assets[ctx->top].type = KRASS_TYPE_IMAGE;
	ctx->assets[ctx->top].data.image.pack_id = id;
	ctx->assets[ctx->top].data.image.cb = cb;
	ctx->assets[ctx->top].data.image.data = data;
	ctx->assets[ctx->top].data.image.quad.dim.width = dim.width;
	ctx->assets[ctx->top++].data.image.quad.dim.height = dim.height;
	return ctx->top - 1;
}

void krass_draw(krass_ctx_t *ctx, int id, float dx, float dy) {}

void krass_draw_scaled(krass_ctx_t *ctx, int id, float dx, float dy, float dw, float dh) {}

kr_image_t *krass_get_asset(krass_ctx_t *ctx, int id, krass_quad_t *quad) {}

#ifdef KR_FULL_RGBA_FONTS
int krass_reserve_quad_font(krass_ctx_t *ctx, const char *fontpath, int size, int font_index) {
	if (ctx->cursor > -1) {
		kinc_log(KINC_LOG_LEVEL_ERROR, "Cannot reserve on finalized context");
		return -1;
	}
	grow_data(ctx);
	ctx->assets[ctx->top].type = KRASS_TYPE_FONT;
	ctx->assets[ctx->top].data.font.fontpath = fontpath;
	ctx->assets[ctx->top].data.font.size = size;
	ctx->assets[ctx->top++].data.font.font_index = font_index;
	++ctx->font_count;
	return ctx->top - 1;
}

kr_ttf_font_t *krass_get_font(krass_ctx_t *ctx, int id) {
	assert(ctx->cap > id && id >= 0);
	assert(ctx->assets[id].type == KRASS_TYPE_FONT);
	return &ctx->assets[id].data.font.font;
}
#else
#define krass_reserve_quad_font(ctx, fontpath, size, font_index) -1
#define krass_get_font(ctx, id) NULL
#endif
