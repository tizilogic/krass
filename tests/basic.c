#include <kinc/graphics4/graphics.h>
#include <kinc/graphics4/rendertarget.h>
#include <kinc/system.h>
#include <krink/graphics2/graphics.h>
#include <krink/graphics2/ttf.h>
#include <krink/memory.h>
#include <krink/system.h>

#include <krass.h>

#include <assert.h>
#include <stdlib.h>

#ifdef NDEBUG
#define STB_IMAGE_WRITE_IMPLEMENTATION
#endif
#include <internal/stb_image_write.h>

#define WINDOW_WIDTH 512
#define WINDOW_HEIGHT 256
#define FONT_SIZE 24
#define FONT_PATH "B612Mono-Regular.ttf"
#define IMAGE_PATH "tex.k"

enum asset_name {
	CIRCLE0 = 0,
	CIRCLE1,
	CIRCLE2,
	CIRCLE3,
	CIRCLE4,
	IMAGE0,
	IMAGE1,
	IMAGE2,
	IMAGE3,
	FONT,
	ASSET_COUNT
};

static krass_ctx_t *krass_ctx = NULL;
static const char *sample_text[10] = {"abc123", "def4",     "ghi56789", "jkl11111111111", "mno",
                                      "pqr",    "stu90909", "vwx-=+/",  "z98 !!!",        "7"};
static uint64_t colors[5] = {0xffff0000, 0xff00ff00, 0xff0000ff, 0xffff00ff, 0xff00ffff};
static int assets[ASSET_COUNT] = {0};
static kr_ttf_font_t *font = NULL;
static kr_image_t image;

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

static void create_png(kinc_g4_render_target_t *target) {
	uint8_t *data = (uint8_t *)kr_malloc(WINDOW_WIDTH * WINDOW_HEIGHT * 4);
	assert(data != NULL);
	kinc_g4_render_target_get_pixels(target, data);
	if (kinc_g4_render_targets_inverted_y())
		data = invert_pixels(data, WINDOW_WIDTH, WINDOW_HEIGHT);
	stbi_write_png("basic.png", WINDOW_WIDTH, WINDOW_HEIGHT, 4, data, WINDOW_WIDTH * 4);
	kr_free(data);
}

static void update(void *unused) {
	kinc_g4_begin(0);
	kinc_g4_render_target_t target;
	kinc_g4_render_target_init_with_multisampling(&target, WINDOW_WIDTH, WINDOW_HEIGHT,
	                                              KINC_G4_RENDER_TARGET_FORMAT_32BIT, 16, 0, 1);
	kinc_g4_render_target_t *targets = {&target};
	kinc_g4_set_render_targets(&targets, 1);
	kinc_g4_clear(KINC_G4_CLEAR_COLOR, 0x0, 0, 0);
	kr_g2_begin(0);
	kr_g2_set_render_target_dim(WINDOW_WIDTH, WINDOW_HEIGHT);
	kr_g2_set_color(0xffffffff);
	for (int x = 0; x < 16; ++x)
		for (int y = 0; y < 8; ++y) {
			int id = CIRCLE0 + ((x + y * 16) % 5);
			krass_draw(krass_ctx, assets[id], x * 32, y * 32);
		}
	krass_draw(krass_ctx, assets[IMAGE0], 10, 10);
	krass_draw(krass_ctx, assets[IMAGE1], 266, 10);
	krass_draw(krass_ctx, assets[IMAGE2], 10, 128);
	krass_draw(krass_ctx, assets[IMAGE3], 266, 128);
	font = krass_get_font(krass_ctx, assets[FONT]);
	kr_g2_set_font(font, FONT_SIZE);
	kr_g2_set_color(0xff888888);
	for (int i = 0; i < 10; ++i) {
		kr_g2_draw_string(sample_text[i], 0,
		                  i * (FONT_SIZE + kr_ttf_line_gap(font, FONT_SIZE)) +
		                      kr_ttf_baseline(font, FONT_SIZE));
	}
	kr_g2_reset_render_target_dim();
	kr_g2_end();
	create_png(&target);
	kinc_g4_restore_render_target();
	kinc_g4_end(0);
	kinc_g4_swap_buffers();
	kinc_stop();
}

static void pre_update(void *unused) {
	kinc_g4_begin(0);

	if (!krass_tick(krass_ctx)) kinc_set_update_callback(update, NULL);

	kr_g2_begin(0);
	kr_g2_clear(0xff000000);
	kr_g2_set_color(0xffffffff);
	kr_g2_fill_rect(0, 112, 511 * krass_progress(krass_ctx) + 1, 30);
	kr_g2_end();

	kinc_g4_end(0);
	kinc_g4_swap_buffers();
}

static void circle_cb(int id, float x, float y, void *data) {
	uint64_t *color = (uint64_t *)data;
	kr_g2_set_color(*color);
	kr_g2_draw_sdf_circle(x + 16, y + 16, 16, 0, 0, 2.2f);
}

static void image_cb(int id, float x, float y, void *data) {
	uint64_t image_id = (uint64_t)data;
	uint64_t xoff = image_id % 2;
	uint64_t yoff = image_id / 2;
	float sx = xoff * 512;
	float sy = yoff * 512;
	kr_g2_draw_scaled_sub_image(&image, sx, sy, 512, 512, x, y, 128, 128);
}

int kickstart(int argc, char **argv) {
	kinc_init("krass basic", WINDOW_WIDTH, WINDOW_HEIGHT, NULL, NULL);
	kinc_set_update_callback(pre_update, NULL);

	void *mem = malloc(10 * 1024 * 1024);
	kr_init(mem, 10 * 1024 * 1024, NULL, 0);
	kr_g2_init();
	kr_image_init(&image);
	kr_image_load(&image, IMAGE_PATH, false);
	kr_image_generate_mipmaps(&image, 30);
	krass_ctx = krass_init(15, 2, 30);
	for (int i = 0; i < 5; ++i)
		assets[CIRCLE0 + i] = krass_reserve_quad(
		    krass_ctx, (krass_dim_t){.width = 32, .height = 32}, circle_cb, &colors[i]);
	for (int i = 0; i < 4; ++i)
		assets[IMAGE0 + i] = krass_reserve_quad(
		    krass_ctx, (krass_dim_t){.width = 128, .height = 128}, image_cb, (void *)(uint64_t)i);

	assets[FONT] = krass_reserve_quad_font(krass_ctx, FONT_PATH, FONT_SIZE, 0);

	krass_finalize(krass_ctx);

	kinc_start();
	return 0;
}
