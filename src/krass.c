#include "krass.h"

#include "internal/pack.c.h"

#ifndef NDEBUG
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "internal/stb_image_write.h"
#endif

#include <kinc/graphics4/graphics.h>
#include <kinc/graphics4/rendertarget.h>
#include <krink/graphics2/graphics.h>
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
	kinc_g4_render_target_t target;
	int top, cap, cursor, step, mipmap_levels;
	bool first;
#ifdef KR_FULL_RGBA_FONTS
	int font_count;
#endif
};

krass_ctx_t *krass_init(int reserve, int step, int mipmap_levels) {
	assert(reserve > 0);
	krass_ctx_t *ctx = (krass_ctx_t *)kr_malloc(sizeof(krass_ctx_t));
	assert(ctx != NULL);
	memset(ctx, 0, sizeof(krass_ctx_t));
	krass_pack_init(&ctx->canvas, reserve);
	ctx->assets = (krass_asset_t *)kr_malloc(reserve * sizeof(krass_asset_t));
	assert(ctx->assets != NULL);
	ctx->cap = reserve;
	ctx->step = (step > 1) ? step : 1;
	ctx->mipmap_levels = (mipmap_levels > 1) ? mipmap_levels : 1;
	ctx->cursor = -1;
	return ctx;
}

void krass_destroy(krass_ctx_t *ctx) {
	if (ctx->img != NULL) {
		kr_image_destroy(ctx->img);
		kr_free(ctx->img);
	}
	krass_pack_destroy(&ctx->canvas);
	for (int i = 0; i < ctx->top; ++i) {
		if (ctx->assets[i].type != KRASS_TYPE_FONT) continue;
		kr_ttf_font_destroy(&ctx->assets[i].data.font.font);
	}
}

static void grow_data(krass_ctx_t *ctx) {
	if (ctx->top >= ctx->cap) {
		ctx->assets =
		    (krass_asset_t *)kr_realloc(ctx->assets, (ctx->top + 1) * sizeof(krass_asset_t));
		ctx->cap = ctx->top + 1;
		assert(ctx->assets != NULL);
	}
}

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

static void render_font(krass_ctx_t *ctx) {
	krass_font_t *font = &ctx->assets[ctx->cursor].data.font;
	krass_rect_t *r = &ctx->canvas.rects[font->pack_id];
	kinc_g4_texture_t *tex = kr_ttf_get_texture(&font->font, font->size);
	kr_image_t img;
	kr_image_from_texture(&img, tex, tex->tex_width, tex->tex_height);
	kr_g2_scissor(r->x, r->y, r->w, r->h);
	kr_g2_draw_scaled_sub_image(&img, 0, 0, r->w, r->h, r->x, r->y, r->w, r->h);
	kr_g2_disable_scissor();
}

static void map_fonts(krass_ctx_t *ctx) {
	int remaining = ctx->font_count;
	int cursor = 0;
	while (remaining > 0) {
		while (ctx->assets[cursor].type != KRASS_TYPE_FONT) {
			++cursor;
			if (cursor >= ctx->top) {
				kinc_log(KINC_LOG_LEVEL_ERROR, "Reached end of assets before finding all fonts");
			}
		}
		krass_font_t *font = &ctx->assets[cursor].data.font;
		krass_rect_t *r = &ctx->canvas.rects[font->pack_id];
		kr_ttf_font_t tmp;
		kr_ttf_font_init_empty(&tmp);
		kr_ttf_load_baked_font(&tmp, &font->font, font->size, ctx->img->tex, r->x, r->y, false);
		kr_ttf_font_destroy(&font->font);
		memcpy(&font->font, &tmp, sizeof(kr_ttf_font_t));
		--remaining;
	}
}
#else
#define krass_reserve_quad_font(ctx, fontpath, size, font_index) -1
#define krass_get_font(ctx, id) NULL
#define load_fonts(ctx)
#define render_font(ctx)
#define map_fonts(ctx)
#endif

void krass_finalize(krass_ctx_t *ctx) {
	load_fonts(ctx);
	krass_pack_compute(&ctx->canvas);
	ctx->cursor = 0;
}

static void render_image(krass_ctx_t *ctx) {
	krass_image_t *img = &ctx->assets[ctx->cursor].data.image;
	krass_rect_t *r = &ctx->canvas.rects[img->pack_id];
	// TODO: figure out why scissor doesn't work as expected
	// kr_g2_scissor(r->x, r->y, r->w, r->h);
	img->cb(ctx->cursor, r->x, r->y, img->data);
	// kr_g2_disable_scissor();
}

static uint8_t *invert_pixels(uint8_t *data, int width, int height) {
	uint8_t *inverted_pixels = (uint8_t *)kr_malloc(width * height * 4);
	assert(inverted_pixels != NULL);
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			for (int c = 0; c < 4; ++c) {
				inverted_pixels[y * width * 4 + x * 4 + c] =
				    data[(height - 1 - y) * width * 4 + x * 4 + c];
			}
		}
	}
	kr_free(data);
	return inverted_pixels;
}

static void create_texture(krass_ctx_t *ctx) {
	int width = (int)ctx->canvas.w;
	int height = (int)ctx->canvas.h;
	uint8_t *data = (uint8_t *)kr_malloc(width * height * 4);
	assert(data != NULL);
	kinc_g4_render_target_get_pixels(&ctx->target, data);
	kinc_g4_restore_render_target();
	if (kinc_g4_render_targets_inverted_y()) data = invert_pixels(data, width, height);
#ifndef NDEBUG
	stbi_write_png("test.png", width, height, 4, data, width * 4);
#endif
	kinc_g4_texture_t *tex = (kinc_g4_texture_t *)kr_malloc(sizeof(kinc_g4_texture_t));
	assert(tex != NULL);
	kinc_image_t img;
	kinc_image_init_from_bytes(&img, data, width, height, KINC_IMAGE_FORMAT_RGBA32);
	kinc_g4_texture_init_from_image(tex, &img);
	kinc_image_destroy(&img);
	kr_free(data);
	ctx->img = (kr_image_t *)kr_malloc(sizeof(kr_image_t));
	assert(ctx->img != NULL);
	kr_image_from_texture(ctx->img, tex, (float)width, (float)height);
	kr_image_generate_mipmaps(ctx->img, ctx->mipmap_levels);
}

bool krass_tick(krass_ctx_t *ctx) {
	if (ctx->cursor < 0) {
		kinc_log(KINC_LOG_LEVEL_ERROR, "Called tick on non finalized context");
		return true;
	}
	if (ctx->cursor - 1 > ctx->top) return false;
	int width = (int)ctx->canvas.w;
	int height = (int)ctx->canvas.h;
	if (ctx->cursor == 0) {
		kinc_g4_render_target_init_with_multisampling(&ctx->target, width, height,
		                                              KINC_G4_RENDER_TARGET_FORMAT_32BIT, 16, 0, 1);
		ctx->first = true;
	}
	if (ctx->cursor < ctx->top) {
		kinc_g4_render_target_t *t = {&ctx->target};
		kinc_g4_set_render_targets(&t, 1);
		if (ctx->first) {
			kinc_g4_clear(KINC_G4_CLEAR_COLOR, 0x0, -1, 0);
			ctx->first = false;
		}
		kr_g2_begin(0);
		kr_g2_set_render_target_dim(width, height);
		for (int i = 0; i < ctx->step; ++i) {
			if (ctx->assets[ctx->cursor].type == KRASS_TYPE_FONT)
				render_font(ctx);
			else if (ctx->assets[ctx->cursor].type == KRASS_TYPE_IMAGE)
				render_image(ctx);
			++ctx->cursor;
			if (ctx->cursor >= ctx->top) break;
		}
		kr_g2_reset_render_target_dim();
		kr_g2_end();
		kinc_g4_restore_render_target();
	}
	else if (ctx->cursor == ctx->top) {
		create_texture(ctx);
		++ctx->cursor;
	}
	else {
		map_fonts(ctx);
		++ctx->cursor;
		return false;
	}
	return true;
}

float krass_progress(krass_ctx_t *ctx) {
	if (ctx->cursor < 0) {
		kinc_log(KINC_LOG_LEVEL_ERROR, "Called progress on non finalized context");
		return 0;
	}
	return (float)ctx->cursor / (float)(ctx->top + 1);
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

void krass_draw(krass_ctx_t *ctx, int id, float dx, float dy) {
	assert(ctx->assets[id].type == KRASS_TYPE_IMAGE);
	krass_image_t *img = &ctx->assets[id].data.image;
	krass_rect_t *r = &ctx->canvas.rects[img->pack_id];
	kr_g2_draw_scaled_sub_image(ctx->img, r->x, r->y, r->w, r->h, dx, dy, r->w, r->h);
}

void krass_draw_scaled(krass_ctx_t *ctx, int id, float dx, float dy, float dw, float dh) {
	assert(ctx->assets[id].type == KRASS_TYPE_IMAGE);
	krass_image_t *img = &ctx->assets[id].data.image;
	krass_rect_t *r = &ctx->canvas.rects[img->pack_id];
	kr_g2_draw_scaled_sub_image(ctx->img, r->x, r->y, r->w, r->h, dx, dy, dw, dh);
}

kr_image_t *krass_get_asset(krass_ctx_t *ctx, int id, krass_quad_t *quad) {
	assert(ctx->assets[id].type == KRASS_TYPE_IMAGE);
	krass_image_t *img = &ctx->assets[id].data.image;
	krass_rect_t *r = &ctx->canvas.rects[img->pack_id];
	quad->x = r->x;
	quad->y = r->y;
	quad->dim.width = r->w;
	quad->dim.height = r->h;
	return ctx->img;
}

#ifdef KR_FULL_RGBA_FONTS
#else
#endif
