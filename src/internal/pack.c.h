#pragma once

#include <assert.h>
#include <kinc/log.h>
#include <krink/math/vector.h>
#include <krink/memory.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

#define KRASS_MIN_FREE 5

typedef struct krass_rect {
	float x, y, w, h;
} krass_rect_t;

typedef struct krass_canvas {
	float w, h;
	krass_rect_t *rects;
	int top, cap;
	bool init;
} krass_canvas_t;

typedef struct free_area {
	krass_rect_t *rects;
	int top;
	int cap;
} free_area_t;

static void internal_fa_init(free_area_t *a, float w, float h, int reserve) {
	a->rects = (krass_rect_t *)kr_malloc(reserve * sizeof(krass_rect_t));
	assert(a->rects != NULL);
	a->rects[0].x = 0.0f;
	a->rects[0].y = 0.0f;
	a->rects[0].w = w;
	a->rects[0].h = h;
	a->top = 1;
	a->cap = reserve;
}

static void internal_fa_destroy(free_area_t *a) {
	assert(a->rects != NULL);
	kr_free(a->rects);
	a->top = 0;
	a->cap = 0;
}

static void internal_fa_grow(free_area_t *a) {
	if (a->top + 1 >= a->cap) {
		if (a->cap > 0) {
			a->rects = (krass_rect_t *)kr_realloc(a->rects, (a->cap * 2) * sizeof(krass_rect_t));
			assert(a->rects != NULL);
			a->cap *= 2;
		}
		else {
			a->rects = (krass_rect_t *)kr_malloc(2 * sizeof(krass_rect_t));
			assert(a->rects != NULL);
			a->cap = 2;
		}
	}
}

static void internal_fa_shift_left(free_area_t *a, int first) {
	for (int i = first; i < a->top - 1; ++i) {
		memcpy(&a->rects[i], &a->rects[i + 1], sizeof(krass_rect_t));
	}
	--a->top;
}

static bool internal_fa_place(free_area_t *a, kr_vec2_t *pos, float w, float h) {
	if (a->top == 0) return false;
	w = ceilf(w);
	h = ceilf(h);
	for (int current = 0; current < a->top; ++current) {
		if (a->rects[current].w >= w + 1 && a->rects[current].h >= h + 1) {
			float fa_left = a->rects[current].x;
			float fa_right = a->rects[current].x + a->rects[current].w + 1;
			float fa_top = a->rects[current].y;
			float fa_bottom = a->rects[current].y + a->rects[current].h + 1;
			pos->x = fa_left;
			pos->y = fa_top;

			if (w + KRASS_MIN_FREE >= a->rects[current].w &&
			    h + KRASS_MIN_FREE >= a->rects[current].h) {
				// Consume entire rect
				internal_fa_shift_left(a, current);
			}
			else if (w + KRASS_MIN_FREE >= a->rects[current].w) {
				// Merge down
				a->rects[current].y = fa_bottom;
				a->rects[current].h -= h + 1;
			}
			else if (h + KRASS_MIN_FREE >= a->rects[current].h) {
				// Merge right
				a->rects[current].x = fa_right;
				a->rects[current].w -= w + 1;
			}
			else {
				// Split into two
				internal_fa_grow(a);
				float prev_h = a->rects[current].h;
				a->rects[current].y = fa_bottom;
				a->rects[current].h -= h + 1;
				a->rects[a->top].x = fa_right;
				a->rects[a->top].y = fa_top;
				a->rects[a->top].w = a->rects[current].w - w - 1;
				a->rects[a->top].h = prev_h - h - 1;
				++a->top;
			}
			return true;
		}
	}
	return false;
}

static void krass_pack_init(krass_canvas_t *canvas, int reserve) {
	assert(reserve >= 0);
	canvas->w = 0;
	canvas->h = 0;
	canvas->top = 0;
	canvas->cap = reserve;
	if (reserve > 0) {
		canvas->rects = (krass_rect_t *)kr_malloc(reserve * sizeof(krass_rect_t));
		assert(canvas->rects != NULL);
	}
	else {
		canvas->rects = NULL;
	}
	canvas->init = true;
}

static void krass_pack_destroy(krass_canvas_t *canvas) {
	if (canvas->rects != NULL) kr_free(canvas->rects);
	canvas->init = false;
}

static int krass_pack_add_rect(krass_canvas_t *canvas, float w, float h) {
	assert(canvas->init);
	if (canvas->top == canvas->cap) {
		canvas->rects =
		    (krass_rect_t *)kr_realloc(canvas->rects, canvas->cap * sizeof(krass_rect_t));
		assert(canvas->rects != NULL);
		canvas->cap *= 2;
	}
	krass_rect_t *dest = &canvas->rects[canvas->top++];
	dest->x = 0.0f;
	dest->y = 0.0f;
	dest->w = w;
	dest->h = h;
	return canvas->top - 1;
}

static bool internal_is_in(int a, int *values, int count) {
	for (int i = 0; i < count; ++i)
		if (values[i] == a) return true;
	return false;
}

static void internal_sort_by_height(krass_canvas_t *canvas, int *ids) {
	for (int i = 0; i < canvas->top; ++i) ids[i] = i;
	for (int i = 0; i < canvas->top - 1; ++i) {
		for (int j = i + 1; j < canvas->top; ++j) {
			if (canvas->rects[ids[j]].h > canvas->rects[ids[i]].h) {
				int tmp = ids[i];
				ids[i] = ids[j];
				ids[j] = tmp;
			}
		}
	}
}

static void krass_pack_compute(krass_canvas_t *canvas) {
	assert(canvas->init);
	int *ids = (int *)kr_malloc(canvas->top * sizeof(int));
	assert(ids != NULL);
	internal_sort_by_height(canvas, ids);
	float w = 1.0f;
	float h = 1.0f;
	float area = 0.0f;
	for (int i = 0; i < canvas->top; ++i) {
		area += canvas->rects[i].w * canvas->rects[i].h;
	}
	while (w * h < area) {
		if (h > w)
			w *= 2.0f;
		else
			h *= 2.0f;
	}

	kr_vec2_t *pos = (kr_vec2_t *)kr_malloc(canvas->top * sizeof(kr_vec2_t));
	assert(pos != NULL);
	while (true) {
#ifndef NDEBUG
		kinc_log(KINC_LOG_LEVEL_INFO, "Area to pack %d, using %dx%d to pack.", (int)area, (int)w,
		         (int)h);
		for (int i = 0; i < canvas->top; ++i) {
			kinc_log(KINC_LOG_LEVEL_INFO, "%d - %fx%f", ids[i], canvas->rects[ids[i]].w,
			         canvas->rects[ids[i]].h);
		}
#endif
		free_area_t a;
		internal_fa_init(&a, w, h, canvas->top); // Reserve too much...
		bool success = true;
		for (int i = 0; i < canvas->top; ++i) {
			if (!internal_fa_place(&a, &pos[i], canvas->rects[ids[i]].w, canvas->rects[ids[i]].h)) {
				if (h > w)
					w *= 2.0f;
				else
					h *= 2.0f;
				success = false;
				break;
			}
		}
		internal_fa_destroy(&a);
		if (success) {
			canvas->w = w;
			canvas->h = h;
			for (int i = 0; i < canvas->top; ++i) {
				canvas->rects[ids[i]].x = pos[i].x;
				canvas->rects[ids[i]].y = pos[i].y;
			}
			break;
		}
	}
	kr_free(ids);
	kr_free(pos);
}

static krass_rect_t krass_pack_get_rect(krass_canvas_t *canvas, int id) {
	assert(canvas->init);
	assert(canvas->top > id && id >= 0);
	return canvas->rects[id];
}
