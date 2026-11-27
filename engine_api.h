#ifndef NGU_ENGINE_API_H
#define NGU_ENGINE_API_H

#include <stdint.h>

/* The engine's C ABI. The full contract, per function, is in the engine's clr/src/capi.rs (cargo doc) and docs/ENGINE-API.md.
 *
 * Threading: call ngu_init once, then every ngu_* function, from that one thread, one call at a time. All state is
 * process-wide. Memory: pointers the engine returns are borrowed until the next engine call (copy before handing them to
 * another thread); pointers you pass in stay yours; strings are NUL-terminated UTF-8. Units: seconds for ngu_frame,
 * microseconds for ngu_clock_us, device pixels with a top-left origin for positions. */
typedef struct { float x, y, u, v; uint32_t rgba; } NguVertex;                                  /* 20 bytes */
typedef struct { uint32_t tex, nearest; int32_t clip[4]; uint32_t first, count; } NguBatch;      /* 32 bytes */

/* Host tuning, before ngu_init (process-wide): 1 background autosave, 2 single-writer storage (no hard links), 4 startup fonts
 * on a worker thread, 8 time the layout (ngu_layout_us), 16 console mode always on (no on / off setting). All are off by default. */
void ngu_set_host_tuning(uint32_t bits);
/* Load the game. data_dir holds the prepared data (or ngu.pak). 0 ok, 1 DLL missing, 2 scene, 3 save menu. */
int ngu_init(const char *dir, int w, int h);
/* Advance dt seconds and set the pointer (x, y device pixels, negative = none; down = finger down), once per display frame. */
void ngu_frame(float dt, float x, float y, int down);
/* Navigation: one ASCII code, u d l r (move), a (activate), b (back), p n (previous, next menu). Returns 1 if the focus moved. */
int ngu_pad(uint32_t action);
/* One frame of gamepad state, before ngu_frame: buttons 1 cross, 2 circle, 4 square, 8 triangle, 16 L, 32 R, 64 start, 128 select,
 * 256 up, 512 down, 1024 left, 2048 right; sticks -1..1 (x right, y down); dt seconds. The engine decodes taps and holds, repeats and
 * dead zones. Returns 1 if the selection moved. */
int ngu_gamepad(uint32_t buttons, float lx, float ly, float rx, float ry, float dt);
/* Scroll the focused list by dx, dy game pixels. */
void ngu_scroll(float dx, float dy);
/* Build the frame; returns the batch count. Read ngu_batches / ngu_vertices before the next engine call. */
int ngu_emit(void);
/* Number of draw lists built so far: publish a list to a render thread only when it changes. */
uint32_t ngu_emit_seq(void);
/* The texture is uploaded: let the engine free its RAM copy. */
void ngu_tex_release(uint32_t id);
/* Microseconds the last ngu_emit spent on layout (trace only). */
uint32_t ngu_layout_us(void);
const NguBatch *ngu_batches(void);
const NguVertex *ngu_vertices(void);
/* RGBA8 pixels of texture id (> 1), size in *w, *h; NULL if unknown. Borrowed. */
const uint8_t *ngu_tex(uint32_t id, uint32_t *w, uint32_t *h);
/* The glyph atlas (texture id 1); *dirty = 1 if it changed since the last call, *size its side in pixels. */
const uint8_t *ngu_atlas(int *dirty, uint32_t *size);
/* 1 once per text request, then copies the initial text into buf; show a keyboard and answer with ngu_text_submit. */
int ngu_text_request(char *buf, int cap, int *max, int *numeric);
/* Answer a text request: ok != 0 commits text, 0 cancels. */
void ngu_text_submit(const char *text, int ok);
/* Game time in microseconds; *debt_us = real time received but not yet simulated. */
uint64_t ngu_clock_us(uint64_t *debt_us);
/* Benchmark builds: load a scripted scenario; 0 on success. */
int ngu_autoplay_load(const char *path);
/* What the last ngu_frame did, for the per-frame trace: out points to eight uint32_t. */
void ngu_frame_info(uint32_t *out);

#endif
