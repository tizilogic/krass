#pragma once

#include <krink/graphics2/ttf.h>
#include <krink/image.h>

#include <stdbool.h>

typedef struct krass_ctx krass_ctx_t;

typedef struct krass_dim {
	float width;
	float height;
} krass_dim_t;

typedef struct krass_quad {
	float x;
	float y;
	krass_dim_t dim;
} krass_quad_t;

/**
 * @brief
 *
 * @param id The id of the asset
 * @param x
 * @param y
 * @param data
 */
typedef void (*krass_draw_callback_t)(int id, float x, float y, void *data);

/**
 * @brief Initialize an empty context
 *
 * @return krass_ctx_t*
 */
krass_ctx_t *krass_init(void);

/**
 * @brief Destroy a previously initialized context
 *
 * @param ctx
 */
void krass_destroy(krass_ctx_t *ctx);

/**
 * @brief Finalize an asset packing context. Call this after all quads have been reserved using
 * `krass_reserve_quad`
 *
 * @param ctx
 */
void krass_finalize(krass_ctx_t *ctx);

/**
 * @brief Call until it returns `false` after finalizing a context
 *
 * @param ctx
 * @return true
 * @return false
 */
bool krass_tick(krass_ctx_t *ctx);

/**
 * @brief Returns a float in the range of 0..1 of the linear progress
 *
 * @param ctx
 * @return float
 */
float krass_progress(krass_ctx_t *ctx);

/**
 * @brief Reserve space for an asset
 *
 * @param ctx
 * @param dim The size of the asset
 * @param cb Callback function that draws the asset into a rendertarget
 * @param data User data that gets passed into the callback
 * @return int The id of the asset in the final texture
 */
int krass_reserve_quad(krass_ctx_t *ctx, krass_dim_t dim, krass_draw_callback_t cb, void *data);

/**
 * @brief Draw a specific asset. This is expected to be called inside a `kr_g2_begin/_end` block
 *
 * @param ctx
 * @param id The id of the asset
 * @param dx
 * @param dy
 */
void krass_draw(krass_ctx_t *ctx, int id, float dx, float dy);

/**
 * @brief Draw a specific asset using custom dimension of the destination quad. This is expected to
 * be called inside a `kr_g2_begin/_end` block
 *
 * @param ctx
 * @param id The id of the asset
 * @param dx
 * @param dy
 * @param dw
 * @param dh
 */
void krass_draw_scaled(krass_ctx_t *ctx, int id, float dx, float dy, float dw, float dh);

/**
 * @brief Retrieve the krink image and get the source quad data of a specific asset
 *
 * @param ctx
 * @param id The id of the asset
 * @param quad Filled with the source quad data inside the krink image
 * @return kr_image_t* krink image that can be used to draw
 */
kr_image_t *krass_get_asset(krass_ctx_t *ctx, int id, krass_quad_t *quad);

/**
 * @brief Reserve space for a baked, full RGBA font. Only available when the `KR_FULL_RGBA_FONTS`
 * macro is defined
 *
 * @param ctx
 * @param fontpath
 * @param size
 * @param font_index
 * @return int The id of the asset or `-1` if the `KR_FULL_RGBA_FONTS` macro is undefined
 */
int krass_reserve_quad_font(krass_ctx_t *ctx, const char *fontpath, int size, int font_index);

/**
 * @brief Retrieve the baked font with the corresponding id. Only available when the
 * `KR_FULL_RGBA_FONTS` macro is defined
 *
 * @param ctx
 * @param id
 * @return kr_ttf_font_t* The baked font or `NULL` if the `KR_FULL_RGBA_FONTS` macro is undefined
 */
kr_ttf_font_t *krass_get_font(krass_ctx_t *ctx, int id);
