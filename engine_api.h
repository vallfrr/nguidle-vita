#ifndef NGU_ENGINE_API_H
#define NGU_ENGINE_API_H

#include <stdint.h>

/* Call all functions from the one thread that initialized the engine. */
typedef struct { float x, y, u, v; uint32_t rgba; } NguVertex;
typedef struct { uint32_t tex, nearest; int32_t clip[4]; uint32_t first, count; } NguBatch;

int ngu_init(const char *dir, int w, int h);
#ifdef NGU_NATIVE_LOGIC
int ngu_vita_init_native(const char *dir, int w, int h);
#define ngu_init ngu_vita_init_native
#endif
void ngu_frame(float dt, float x, float y, int down);
int ngu_pad(uint32_t action);
void ngu_scroll(float dx, float dy);
int ngu_emit(void);
uint32_t ngu_emit_seq(void);
void ngu_tex_release(uint32_t id);
uint32_t ngu_layout_us(void);
const NguBatch *ngu_batches(void);
const NguVertex *ngu_vertices(void);
const uint8_t *ngu_tex(uint32_t id, uint32_t *w, uint32_t *h);
const uint8_t *ngu_atlas(int *dirty, uint32_t *size);
int ngu_text_request(char *buf, int cap, int *max, int *numeric);
void ngu_text_submit(const char *text, int ok);
uint64_t ngu_clock_us(uint64_t *debt_us);
int ngu_autoplay_load(const char *path);
void ngu_frame_info(uint32_t *out);

#endif
