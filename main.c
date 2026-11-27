#include "host_ime.h"
// NGU Idle PS Vita: C host (vitaGL + touch) around the Rust library `clr`.
#include <stdint.h>
#include <stdatomic.h>
#include <stddef.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vitasdk.h>
#include <vitaGL.h>
#include "host_log.h"
#include "engine_api.h"
#ifdef NGU_VITA_BENCHMARK
#include "perf_stats.h"
#endif

#define DATA_DIR "ux0:data/nguidle"
#ifdef NGU_VITA_BENCHMARK
#define PROFILE_DIR DATA_DIR "/bench"
#else
#define PROFILE_DIR DATA_DIR
#endif
#define LOG_FILE PROFILE_DIR "/host.txt"
#define HEAP_MB 170
#define VGL_RAM_MB 24
#define SYSTEM_RAM_MB 4

int _newlib_heap_size_user = HEAP_MB * 1024 * 1024;

#define MAX_TEX 8192
static GLuint g_tex[MAX_TEX];
static GLuint g_white, g_atlas;
#ifdef NGU_VITA_VERTEX_BUFFER
static GLuint g_vertex_buffer;
#endif
static int g_atlas_size;
static GLuint g_bound_texture;
static uint8_t g_filter[MAX_TEX];

static void bind_texture(GLuint texture) {
    if (texture != g_bound_texture) {
        glBindTexture(GL_TEXTURE_2D, texture);
        g_bound_texture = texture;
    }
}

static uint64_t now_us(void);

// Two threads (docs/VITA-60.md): L, the big-stack "ngu" thread, runs input, simulation, layout and the draw list;
// R (render_thread) owns the GL context, uploads textures and presents the newest finished list at the display rate.
// Finished lists travel through a lock-free triple buffer; textures and the glyph atlas are copied by L and uploaded by R.
typedef struct { NguVertex *v; size_t vcap; NguBatch *b; size_t bcap; int nb; } Slot;
static Slot g_slot[3];
static atomic_int g_mid = 2;   // middle slot; | 4 while R has not taken it yet
static int g_back = 1;         // owned by L
static int g_front = 0;        // owned by R

typedef struct { uint8_t *px; uint32_t id, w, h; } TexUp;
static TexUp *g_upq; static int g_upq_n, g_upq_cap;   // copies made by L, waiting for R
static TexUp *g_rq; static int g_rq_n, g_rq_cap, g_rq_head; // R's own list
static atomic_flag g_upq_lock = ATOMIC_FLAG_INIT;
static atomic_uchar g_tex_state[MAX_TEX];            // 0 absent, 1 queued, 2 resident on the GPU

static void upq_lock(void) { while (atomic_flag_test_and_set_explicit(&g_upq_lock, memory_order_acquire)) {} }
static void upq_unlock(void) { atomic_flag_clear_explicit(&g_upq_lock, memory_order_release); }

#define ATLAS_MAX 1024
static uint8_t g_atlas_stage[ATLAS_MAX * ATLAS_MAX * 4];
static atomic_int g_atl;   // 0 idle, 1 L writing, 2 ready for R, 3 R uploading
static int g_atl_first = 1;
static uint32_t g_atl_size;

// L: copy the glyph atlas into the staging buffer when it changed (and R is not reading it)
static void stage_atlas(void) {
    int prev = 0;
    if (!atomic_compare_exchange_strong(&g_atl, &prev, 1)) {
        prev = 2;
        if (!atomic_compare_exchange_strong(&g_atl, &prev, 1)) return;
    }
    int dirty = 0;
    uint32_t size = 0;
    const uint8_t *px = ngu_atlas(&dirty, &size);
    if (px && size <= ATLAS_MAX && (dirty || g_atl_first)) {
        memcpy(g_atlas_stage, px, (size_t)size * size * 4);
        g_atl_size = size;
        g_atl_first = 0;
        atomic_store(&g_atl, 2);
    } else {
        atomic_store(&g_atl, prev);
    }
}

// R: send the staged atlas to the GPU. glTexSubImage2D on the texture in use costs ~65 ms per call whatever its size
// (measured), so each update goes into a fresh texture object that replaces the old one.
static unsigned g_tex_us_max, g_atl_us_max, g_tex_bytes;
static GLuint g_atlas_spare;
static void upload_atlas(void) {
    int expected = 2;
    if (!atomic_compare_exchange_strong(&g_atl, &expected, 3)) return;
    uint64_t t = now_us();
    GLuint fresh = g_atlas_spare;
    if (!fresh) glGenTextures(1, &fresh);
    bind_texture(fresh);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_atl_size, g_atl_size, 0, GL_RGBA, GL_UNSIGNED_BYTE, g_atlas_stage);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    g_atlas_spare = g_atlas; // the previous one may still be read by the frame in flight: reuse it only for the next update
    g_atlas = fresh;
    g_atlas_size = g_atl_size;
    g_filter[1] = 0;
    atomic_store(&g_atl, 0);
    unsigned us = (unsigned)(now_us() - t);
    if (us > g_atl_us_max) g_atl_us_max = us;
}

#define TEX_BUDGET (70u << 20)
static uint32_t g_tex_size[MAX_TEX];
static uint32_t g_tex_use[MAX_TEX];
static atomic_uint g_frame_no;
static uint32_t g_tex_total;
static int g_tex_count;

// R: frees the least recently used textures until the budget is met
static void evict_for(uint32_t need) {
    uint32_t frame = atomic_load_explicit(&g_frame_no, memory_order_relaxed);
    while (g_tex_total + need > TEX_BUDGET) {
        int best = -1;
        uint32_t oldest = 2;
        for (int i = 2; i < MAX_TEX; i++) {
            uint32_t age = frame - g_tex_use[i];
            if (g_tex[i] && age > oldest) { oldest = age; best = i; }
        }
        if (best < 0) return; // everything is used in the current frame
        if (g_bound_texture == g_tex[best]) bind_texture(0);
        glDeleteTextures(1, &g_tex[best]);
        g_tex[best] = 0;
        g_filter[best] = 0;
        atomic_store(&g_tex_state[best], 0);
        g_tex_total -= g_tex_size[best];
        g_tex_count--;
    }
}

// L: copy a texture the GPU does not have yet and hand it to R (the engine keeps no RAM copy afterwards)
static void queue_tex(uint32_t id) {
    uint32_t w = 0, h = 0;
    const uint8_t *px = ngu_tex(id, &w, &h);
    if (!px || !w || !h) return;
    uint8_t *copy = malloc((size_t)w * h * 4);
    if (!copy) return;
    memcpy(copy, px, (size_t)w * h * 4);
    ngu_tex_release(id);
    upq_lock();
    if (g_upq_n == g_upq_cap) {
        int cap = g_upq_cap ? g_upq_cap * 2 : 64;
        TexUp *grown = realloc(g_upq, cap * sizeof *grown);
        if (!grown) { upq_unlock(); free(copy); return; }
        g_upq = grown; g_upq_cap = cap;
    }
    g_upq[g_upq_n++] = (TexUp){copy, id, w, h};
    // The render thread takes entries under this lock. It can only publish
    // residency after the queued state is visible, including on a fast upload.
    atomic_store(&g_tex_state[id], 1);
    upq_unlock();
}

// R: send one queued texture to the GPU
static void upload_tex(TexUp *u) {
    uint32_t id = u->id;
    uint64_t t = now_us();
    evict_for(u->w * u->h * 4);
    glGenTextures(1, &g_tex[id]);
    bind_texture(g_tex[id]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, u->w, u->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, u->px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    g_tex_size[id] = u->w * u->h * 4;
    g_tex_total += g_tex_size[id];
    g_tex_count++;
    g_tex_use[id] = atomic_load_explicit(&g_frame_no, memory_order_relaxed);
    free(u->px);
    atomic_store(&g_tex_state[id], 2);
    unsigned us = (unsigned)(now_us() - t);
    if (us > g_tex_us_max) { g_tex_us_max = us; g_tex_bytes = u->w * u->h * 4; }
}

// R: upload what L queued, at most `budget_us` of work per frame (the rest waits for the next frame; sprites that are not on the GPU yet are skipped)
static uint64_t run_uploads(uint64_t budget_us) {
    upq_lock();
    if (g_upq_n) {
        if (g_rq_n + g_upq_n > g_rq_cap) {
            int cap = g_rq_n + g_upq_n + 64;
            TexUp *grown = realloc(g_rq, cap * sizeof *grown);
            if (grown) { g_rq = grown; g_rq_cap = cap; }
        }
        if (g_rq_n + g_upq_n <= g_rq_cap) {
            memcpy(g_rq + g_rq_n, g_upq, g_upq_n * sizeof *g_upq);
            g_rq_n += g_upq_n;
            g_upq_n = 0;
        }
    }
    upq_unlock();
    uint64_t t0 = now_us();
    while (g_rq_head < g_rq_n) {
        upload_tex(&g_rq[g_rq_head++]);
        if (now_us() - t0 > budget_us) break;
    }
    if (g_rq_head == g_rq_n) g_rq_n = g_rq_head = 0;
    return now_us() - t0;
}

static GLuint get_tex(uint32_t id) {
    if (id == 0) return g_white;
    if (id == 1) return g_atlas;
    if (id >= MAX_TEX || !g_tex[id]) return g_white;
    g_tex_use[id] = atomic_load_explicit(&g_frame_no, memory_order_relaxed);
    return g_tex[id];
}

// L: hand the finished list to R (copies it, so the engine can build the next one at once)
static void publish(int nb) {
    Slot *s = &g_slot[g_back];
    const NguBatch *b = ngu_batches();
    const NguVertex *v = ngu_vertices();
    size_t vn = nb > 0 ? (size_t)b[nb - 1].first + b[nb - 1].count : 0;
    if (vn > s->vcap) {
        size_t cap = vn + vn / 4 + 1024;
        NguVertex *grown = realloc(s->v, cap * sizeof *grown);
        if (!grown) return;
        s->v = grown; s->vcap = cap;
    }
    if ((size_t)nb > s->bcap) {
        size_t cap = (size_t)nb + nb / 4 + 64;
        NguBatch *grown = realloc(s->b, cap * sizeof *grown);
        if (!grown) return;
        s->b = grown; s->bcap = cap;
    }
    if (vn) memcpy(s->v, v, vn * sizeof *v);
    if (nb > 0) memcpy(s->b, b, (size_t)nb * sizeof *b);
    s->nb = nb;
    for (int i = 0; i < nb; i++) {
        uint32_t id = b[i].tex;
        if (id >= 2 && id < MAX_TEX && atomic_load(&g_tex_state[id]) == 0) queue_tex(id);
    }
    stage_atlas();
    int old = atomic_exchange(&g_mid, g_back | 4);
    g_back = old & 3;
}

static uint64_t now_us(void) { return sceKernelGetProcessTimeWide(); }

#ifdef NGU_VITA_BENCHMARK
#define PERF(code) code
static uint64_t g_t_draw, g_t_swap, g_t_upload;
static unsigned g_frame_ms[251], g_frame_max, g_shots;
static unsigned g_pad_edges; // logic thread only, reset after each sample
static atomic_flag g_metrics_lock = ATOMIC_FLAG_INIT;
static LogicSnapshot g_logic_snapshot;
static LogicWindow g_logic_trace, g_logic_report;
static int g_collect_trace;
static uint64_t g_load_start;

static void metrics_lock(void) {
    while (atomic_flag_test_and_set_explicit(&g_metrics_lock, memory_order_acquire)) {}
}

static void metrics_unlock(void) {
    atomic_flag_clear_explicit(&g_metrics_lock, memory_order_release);
}
static atomic_uint g_scanout_frames, g_scanout_repeats, g_scanout_errors, g_scanout_gaps;
static atomic_uint g_display_callbacks;
static atomic_uint g_focus_frame, g_focus_press_us;
static atomic_uintptr_t g_focus_buffer;
static atomic_uint g_focus_samples, g_focus_missed, g_focus_slow, g_focus_max_us;

static void display_callback(void *base) {
    static unsigned callback_frame;
    unsigned frame = ++callback_frame;
    atomic_fetch_add_explicit(&g_display_callbacks, 1, memory_order_relaxed);
    uintptr_t address = (uintptr_t)base;
    uintptr_t pending = address;
    if (atomic_compare_exchange_strong(&g_focus_buffer, &pending, 0)) {
        // Reusing a buffer before the monitor has seen it makes that sample unusable.
        atomic_fetch_add_explicit(&g_focus_missed, 1, memory_order_relaxed);
        atomic_store_explicit(&g_focus_frame, 0, memory_order_release);
    }
    if (frame == atomic_load_explicit(&g_focus_frame, memory_order_acquire)) {
        atomic_store_explicit(&g_focus_buffer, address, memory_order_release);
    }
}

// Per-frame trace (docs/VITA-60.md): 16 u32 per frame, written in chunks by a worker thread so the game thread never waits on storage.
// Fields: frame, t_us, work_us (no swap wait), logic, layout, emit, draw, swap, upload, tick, steps, instr, menu, action, marks, flags.
#define TRACE_CHUNK 2048
typedef struct { uint32_t f[16]; } TraceRec;
static TraceRec g_trace_buf[2][TRACE_CHUNK];
static int g_trace_n, g_trace_cur, g_trace_on, g_trace_ready_n;
static atomic_int g_trace_ready = -1;
static unsigned g_trace_dropped;
static SceUID g_trace_sema = -1, g_trace_fd = -1;

static int trace_writer(SceSize args, void *argp) {
    (void)args; (void)argp;
    for (;;) {
        sceKernelWaitSema(g_trace_sema, 1, NULL);
        int idx = atomic_load(&g_trace_ready);
        if (idx >= 0) {
            sceIoWrite(g_trace_fd, g_trace_buf[idx], g_trace_ready_n * sizeof(TraceRec));
            atomic_store(&g_trace_ready, -1);
        }
    }
    return 0;
}

static void trace_start(void) {
    g_trace_fd = sceIoOpen(PROFILE_DIR "/trace.bin", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    g_trace_sema = sceKernelCreateSema("ngu-trace", 0, 0, 1, NULL);
    SceUID th = sceKernelCreateThread("ngu-trace", trace_writer, 0x10000100, 16 * 1024, 0, 0, NULL);
    if (g_trace_fd < 0 || g_trace_sema < 0 || th < 0) { hlog("[trace] cannot start\n"); return; }
    uint32_t head[2] = {0x5455474eu /* NGUT */, 0};
    sceIoWrite(g_trace_fd, head, sizeof head);
    sceKernelStartThread(th, 0, NULL);
    g_trace_on = 1;
    g_collect_trace = 1;
}

static void trace_push(const TraceRec *r) {
    if (!g_trace_on) return;
    g_trace_buf[g_trace_cur][g_trace_n++] = *r;
    if (g_trace_n < TRACE_CHUNK) return;
    if (atomic_load(&g_trace_ready) >= 0) { g_trace_dropped++; g_trace_n = 0; return; } // writer behind: this chunk is lost, counted
    g_trace_ready_n = g_trace_n;
    atomic_store(&g_trace_ready, g_trace_cur);
    g_trace_cur ^= 1;
    g_trace_n = 0;
    sceKernelSignalSema(g_trace_sema, 1);
}

static void trace_finish(void) {
    if (!g_trace_on) return;
    g_trace_on = 0;
    metrics_lock();
    g_collect_trace = 0;
    metrics_unlock();
    while (atomic_load(&g_trace_ready) >= 0) sceKernelDelayThread(1000);
    sceIoWrite(g_trace_fd, g_trace_buf[g_trace_cur], g_trace_n * sizeof(TraceRec));
    sceIoClose(g_trace_fd);
    SceUID d = sceIoOpen(PROFILE_DIR "/done.txt", SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
    char msg[64];
    int n = snprintf(msg, sizeof msg, "done dropped_chunks=%u\n", g_trace_dropped);
    if (d >= 0) { sceIoWrite(d, msg, n); sceIoClose(d); }
    hlog("[trace] finished, dropped chunks=%u\n", g_trace_dropped);
}

static int sample_display(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    void *previous = NULL;
    uint16_t last_vblank = (uint16_t)sceDisplayGetVcount();
    for (;;) {
        if (sceDisplayWaitVblankStart() < 0) {
            atomic_fetch_add_explicit(&g_scanout_errors, 1, memory_order_relaxed);
            sceKernelDelayThread(16667);
            continue;
        }
        // Let the new buffer latch before reading the current scanout address.
        sceKernelDelayThread(600);
        uint16_t vblank = (uint16_t)sceDisplayGetVcount();
        // The hardware counter rolled over after 65,536 pulses during the soak.
        uint16_t elapsed = (uint16_t)(vblank - last_vblank);
        if (elapsed == 0) continue;
        if (elapsed > 1) {
            atomic_fetch_add_explicit(&g_scanout_gaps, elapsed - 1, memory_order_relaxed);
        }
        last_vblank = vblank;
        SceDisplayFrameBuf frame = {.size = sizeof frame};
        if (sceDisplayGetFrameBuf(&frame, SCE_DISPLAY_SETBUF_IMMEDIATE) < 0 ||
            !frame.base || frame.width != 960 || frame.height != 544) {
            atomic_fetch_add_explicit(&g_scanout_errors, 1, memory_order_relaxed);
            previous = NULL;
            continue;
        }
        atomic_uint *samples = frame.base == previous ? &g_scanout_repeats : &g_scanout_frames;
        atomic_fetch_add_explicit(samples, 1, memory_order_relaxed);
        uintptr_t address = (uintptr_t)frame.base;
        if (frame.base != previous && atomic_compare_exchange_strong(&g_focus_buffer, &address, 0)) {
            unsigned latency = (unsigned)now_us() - atomic_load(&g_focus_press_us);
            atomic_fetch_add_explicit(&g_focus_samples, 1, memory_order_relaxed);
            if (latency > 33367) atomic_fetch_add_explicit(&g_focus_slow, 1, memory_order_relaxed);
            unsigned peak = atomic_load(&g_focus_max_us);
            while (peak < latency && !atomic_compare_exchange_weak(&g_focus_max_us, &peak, latency)) {}
            atomic_store_explicit(&g_focus_frame, 0, memory_order_release);
        }
        previous = frame.base;
    }
    return 0;
}

static unsigned percentile(unsigned total, unsigned percent) {
    unsigned target = (total * percent + 99) / 100;
    unsigned seen = 0;
    for (unsigned ms = 0; ms < sizeof g_frame_ms / sizeof g_frame_ms[0]; ms++) {
        seen += g_frame_ms[ms];
        if (seen >= target) return ms;
    }
    return 0;
}
#else
#define PERF(code)
#endif

static void save_shot(const char *path) {
    static uint8_t buf[960 * 544 * 4];
    glReadPixels(0, 0, 960, 544, GL_RGBA, GL_UNSIGNED_BYTE, buf);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    // Both GL and this TGA use a bottom-left origin.
    uint8_t hdr[18] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 960 & 255, 960 >> 8, 544 & 255, 544 >> 8, 32, 0};
    for (size_t i = 0; i < sizeof buf; i += 4) {
        uint8_t red = buf[i];
        buf[i] = buf[i + 2];
        buf[i + 2] = red;
        buf[i + 3] = 255;
    }
    fwrite(hdr, 1, 18, f);
    fwrite(buf, 1, sizeof buf, f);
    fclose(f);
    hlog("[host] capture %s\n", path);
}

static atomic_uint g_shot_request; // zero: none, UINT32_MAX: manual, otherwise scenario number


static void setup_draw_state(void) {
    glClearColor(0, 0, 0, 1);
    glViewport(0, 0, 960, 544);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 960, 544, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
}

// R: draw one published list and present it
static void draw_slot(const Slot *s, uint64_t *upload_us) {
    (void)upload_us;
    atomic_fetch_add_explicit(&g_frame_no, 1, memory_order_relaxed);
    PERF(uint64_t t1 = now_us());
    int nb = s->nb;
    const NguBatch *b = s->b;
    const NguVertex *v = s->v;
    glDisable(GL_SCISSOR_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
    if (nb > 0) {
#ifdef NGU_VITA_VERTEX_BUFFER
        glBindBuffer(GL_ARRAY_BUFFER, g_vertex_buffer);
        size_t count = b[nb - 1].first + b[nb - 1].count;
        glBufferData(GL_ARRAY_BUFFER, count * sizeof(NguVertex), v, GL_STREAM_DRAW);
        glVertexPointer(2, GL_FLOAT, sizeof(NguVertex), (const void *)offsetof(NguVertex, x));
        glTexCoordPointer(2, GL_FLOAT, sizeof(NguVertex), (const void *)offsetof(NguVertex, u));
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(NguVertex), (const void *)offsetof(NguVertex, rgba));
#else
        glVertexPointer(2, GL_FLOAT, sizeof(NguVertex), &v[0].x);
        glTexCoordPointer(2, GL_FLOAT, sizeof(NguVertex), &v[0].u);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(NguVertex), &v[0].rgba);
#endif
    }
    PERF(uint64_t before_atlas = now_us());
    upload_atlas();
    PERF(uint64_t atlas_us = now_us() - before_atlas);
    PERF(*upload_us += atlas_us);
    int32_t last_clip[4] = {-1, -1, -1, -1};
    for (int i = 0; i < nb; i++) {
        int x0 = b[i].clip[0] < 0 ? 0 : b[i].clip[0];
        int y0 = b[i].clip[1] < 0 ? 0 : b[i].clip[1];
        int x1 = b[i].clip[2] > 960 ? 960 : b[i].clip[2];
        int y1 = b[i].clip[3] > 544 ? 544 : b[i].clip[3];
        if (x1 <= x0 || y1 <= y0) continue;
        uint32_t id = b[i].tex;
        // not on the GPU yet (still queued): skip the sprite for a frame or two rather than stall the presentation
        if (id >= 2 && id < MAX_TEX && atomic_load(&g_tex_state[id]) == 1) continue;
        GLuint texture = get_tex(id);
        bind_texture(texture);
        if (texture == g_white) id = 0;
        uint8_t filter = b[i].nearest ? 2 : 1;
        if (g_filter[id] != filter) {
            GLint mode = b[i].nearest ? GL_NEAREST : GL_LINEAR;
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mode);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mode);
            g_filter[id] = filter;
        }
        if (last_clip[0] != x0 || last_clip[1] != y0 || last_clip[2] != x1 || last_clip[3] != y1) {
            glScissor(x0, 544 - y1, x1 - x0, y1 - y0);
            last_clip[0] = x0; last_clip[1] = y0; last_clip[2] = x1; last_clip[3] = y1;
        }
        glDrawArrays(GL_TRIANGLES, b[i].first, b[i].count);
    }
    PERF(g_t_draw += now_us() - t1 - atlas_us);
    unsigned shot = atomic_exchange(&g_shot_request, 0);
    if (shot) {
        if (shot == UINT32_MAX) save_shot(PROFILE_DIR "/shot.tga");
        else { char name[96]; snprintf(name, sizeof name, PROFILE_DIR "/shot%u.tga", shot); save_shot(name); }
        PERF(g_shots++);
    }
    PERF(uint64_t before_swap = now_us());
    vglSwapBuffers(g_ime_dialog ? GL_TRUE : GL_FALSE);
    PERF(g_t_swap += now_us() - before_swap);
}

static void pad_update(const SceCtrlData *pad, uint64_t now, float dt, int enabled) {
    static uint32_t previous;
    (void)now;
    uint32_t pressed = pad->buttons & ~previous;
    previous = pad->buttons;
    PERF(if (pressed & (SCE_CTRL_CROSS | SCE_CTRL_CIRCLE | SCE_CTRL_LTRIGGER | SCE_CTRL_RTRIGGER |
                        SCE_CTRL_UP | SCE_CTRL_DOWN | SCE_CTRL_LEFT | SCE_CTRL_RIGHT)) g_pad_edges++);
    uint32_t bits = 0;
    float lx = 0, ly = 0, rx = 0, ry = 0;
    if (enabled) {
        static const struct { uint32_t sce, api; } map[] = {
            {SCE_CTRL_CROSS, 1}, {SCE_CTRL_CIRCLE, 2}, {SCE_CTRL_SQUARE, 4}, {SCE_CTRL_TRIANGLE, 8},
            {SCE_CTRL_LTRIGGER, 16}, {SCE_CTRL_RTRIGGER, 32}, {SCE_CTRL_START, 64}, {SCE_CTRL_SELECT, 128},
            {SCE_CTRL_UP, 256}, {SCE_CTRL_DOWN, 512}, {SCE_CTRL_LEFT, 1024}, {SCE_CTRL_RIGHT, 2048},
        };
        for (unsigned i = 0; i < sizeof map / sizeof map[0]; i++) if (pad->buttons & map[i].sce) bits |= map[i].api;
        lx = (pad->lx - 128.0f) / 128.0f;
        ly = (pad->ly - 128.0f) / 128.0f;
        rx = (pad->rx - 128.0f) / 128.0f;
        ry = (pad->ry - 128.0f) / 128.0f;
    }
#ifdef NGU_VITA_BENCHMARK
    unsigned start = (unsigned)now_us();
    int moved = ngu_gamepad(bits, lx, ly, rx, ry, dt);
    unsigned dpad = SCE_CTRL_UP | SCE_CTRL_DOWN | SCE_CTRL_LEFT | SCE_CTRL_RIGHT;
    if (moved && (pressed & dpad) && !atomic_load_explicit(&g_focus_frame, memory_order_acquire)) {
        atomic_store(&g_focus_press_us, start);
        unsigned frame = atomic_load_explicit(&g_frame_no, memory_order_relaxed);
        atomic_store_explicit(&g_focus_frame, frame + 1, memory_order_release);
    }
#else
    (void)pressed;
    ngu_gamepad(bits, lx, ly, rx, ry, dt);
#endif
}

// R: owns the GL context from here on. Takes the newest finished list, uploads queued textures, draws, presents (vsync).
static int render_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
#ifdef NGU_VITA_BENCHMARK
    uint64_t fps_t = now_us();
    uint64_t last_present = fps_t;
    unsigned fps_n = 0;
    SceKernelFreeMemorySizeInfo info = {.size = sizeof info};
    uint64_t previous_run_clocks = 0;
#endif
    for (;;) {
#ifdef NGU_VITA_BENCHMARK
        uint64_t now = now_us();
        uint64_t s_dr = g_t_draw, s_sw = g_t_swap, s_up = g_t_upload;
        static uint64_t trace_t0;
        static unsigned trace_frame, last_repeats;
        if (!trace_t0) trace_t0 = now;
#endif
        if (atomic_load(&g_mid) & 4) {
            int old = atomic_exchange(&g_mid, g_front);
            g_front = old & 3;
        }
        if (atomic_exchange(&g_ime_req, 0)) ime_start(g_ime_tb, g_ime_tmax, g_ime_tnum);
        uint64_t upload_us = run_uploads(5000);
        draw_slot(&g_slot[g_front], &upload_us);
        PERF(g_t_upload += upload_us);
        ime_poll();
#ifdef NGU_VITA_BENCHMARK
        uint64_t end = now_us();
        if (g_trace_on) {
            TraceRec r;
            metrics_lock();
            LogicSnapshot snapshot = g_logic_snapshot;
            LogicWindow window = logic_take(&g_logic_trace);
            metrics_unlock();
            unsigned rep = atomic_load_explicit(&g_scanout_repeats, memory_order_relaxed);
            uint32_t up = (uint32_t)(g_t_upload - s_up);
            r.f[0] = trace_frame++;
            r.f[1] = (uint32_t)(now - trace_t0);
            uint32_t r_work = (uint32_t)(end - now - (g_t_swap - s_sw));
            r.f[2] = r_work > window.peak_us ? r_work : window.peak_us;
            r.f[3] = (uint32_t)(window.input + window.logic);
            r.f[4] = (uint32_t)window.layout;
            r.f[5] = (uint32_t)window.emit;
            r.f[6] = (uint32_t)(g_t_draw - s_dr);
            r.f[7] = (uint32_t)(g_t_swap - s_sw);
            r.f[8] = up;
            memcpy(&r.f[9], snapshot.info, 6 * sizeof(uint32_t));
            r.f[15] = (snapshot.info[6] ? 4u : 0u) | (up ? 2u : 0u) | (rep != last_repeats ? 1u : 0u);
            last_repeats = rep;
            trace_push(&r);
            if (snapshot.info[6]) trace_finish();
        }
        unsigned ms = (unsigned)((end - last_present + 999) / 1000);
        last_present = end;
        g_frame_ms[ms > 250 ? 250 : ms]++;
        if (ms > g_frame_max) g_frame_max = ms;
        fps_n++;
        if (end - fps_t >= 5000000) {
            double to_ms = 0.001 / fps_n;
            metrics_lock();
            LogicSnapshot snapshot = g_logic_snapshot;
            LogicWindow logic = logic_take(&g_logic_report);
            metrics_unlock();
            sceKernelGetFreeMemorySize(&info);
            SceKernelThreadInfo thread = {.size = sizeof thread};
            int thread_status = sceKernelGetThreadInfo(sceKernelGetThreadId(), &thread);
            uint64_t b2 = now_us();
            struct mallinfo heap = mallinfo();
            uint64_t b3 = now_us();
            uint64_t run_clocks = thread.runClocks - previous_run_clocks;
            previous_run_clocks = thread.runClocks;
            unsigned focus_samples = atomic_exchange(&g_focus_samples, 0);
            unsigned focus_missed = atomic_exchange(&g_focus_missed, 0);
            unsigned focus_slow = atomic_exchange(&g_focus_slow, 0);
            unsigned focus_max = atomic_exchange(&g_focus_max_us, 0);
            if (focus_samples || focus_missed) {
                hlog("[focus] presented=%u over_two_refreshes=%u missed_samples=%u max_us=%u\n",
                     focus_samples, focus_slow, focus_missed, focus_max);
            }
            hlog("[perf] t=%.1fs fps=%.2f p50=%u p95=%u p99=%u max=%u input=%.2f input_max=%u pads=%u logic=%.2f layout=%.2f emit=%.2f draw=%.2f upload=%.2f swap=%.2f batches=%u textures=%d gpu_mb=%u shots=%u scanout=%u repeat=%u sample_errors=%u sample_gaps=%u gpu_submits=%u free_kib=%d/%d/%d heap_kib=%u/%u vgl_free_kib=%u/%u/%u thread=%d run_clocks=%llu core=%d affinity=%x game_us=%llu debt_us=%llu logic_samples=%u\n",
                (end - g_load_start) / 1000000.0, fps_n * 1000000.0 / (end - fps_t),
                percentile(fps_n, 50), percentile(fps_n, 95), percentile(fps_n, 99), g_frame_max,
                logic_average_ms(logic.input, logic.samples), logic.input_max, logic.pads,
                logic_average_ms(logic.logic, logic.samples), logic_average_ms(logic.layout, logic.samples),
                logic_average_ms(logic.emit, logic.samples),
                g_t_draw * to_ms, g_t_upload * to_ms, g_t_swap * to_ms,
                logic.samples ? (unsigned)(logic.batches / logic.samples) : 0, g_tex_count, g_tex_total >> 20, g_shots,
                atomic_exchange(&g_scanout_frames, 0), atomic_exchange(&g_scanout_repeats, 0),
                atomic_exchange(&g_scanout_errors, 0), atomic_exchange(&g_scanout_gaps, 0),
                atomic_exchange(&g_display_callbacks, 0),
                info.size_user >> 10, info.size_cdram >> 10, info.size_phycont >> 10,
                (unsigned)(heap.uordblks >> 10), (unsigned)(heap.fordblks >> 10),
                (unsigned)(vglMemFree(VGL_MEM_RAM) >> 10), (unsigned)(vglMemFree(VGL_MEM_VRAM) >> 10),
                (unsigned)(vglMemFree(VGL_MEM_PHYCONT) >> 10), thread_status, (unsigned long long)run_clocks,
                thread.lastExecutedCpuId, thread.currentCpuAffinityMask,
                (unsigned long long)snapshot.game_us, (unsigned long long)snapshot.debt_us, logic.samples);
            uint64_t b4 = now_us();
            sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
            sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_OLED_DIMMING);
            hlog("[perf-cost] mallinfo=%u log=%u atlas_max=%u tex_max=%u tex_bytes=%u us\n", (unsigned)(b3 - b2), (unsigned)(b4 - b3), g_atl_us_max, g_tex_us_max, g_tex_bytes); g_atl_us_max = g_tex_us_max = 0;
            g_t_draw = g_t_upload = g_t_swap = 0;
            memset(g_frame_ms, 0, sizeof g_frame_ms);
            g_frame_max = g_shots = 0;
            fps_t = end;
            fps_n = 0;
        }
#endif

    }
    return 0;
}

static int game_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
    sceIoMkdir(DATA_DIR, 0777);
#ifdef NGU_VITA_BENCHMARK
    sceIoMkdir(PROFILE_DIR, 0777);
    if (setenv("NGU_SAVE_DIR", PROFILE_DIR "/save", 1) != 0) return 1;
#ifdef NGU_VITA_PROFILE
    setenv("NGU_PROFILE", "1", 1);
#endif
#endif
    host_log_init(LOG_FILE);
    hlog("[host] Starting %s\n", PROFILE_DIR);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
#ifdef NGU_VITA_FAST_CLOCKS
    scePowerSetArmClockFrequency(444);
    scePowerSetGpuClockFrequency(222);
#else
    scePowerSetArmClockFrequency(333);
    scePowerSetGpuClockFrequency(166);
#endif
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);
    hlog("[host] Clocks CPU=%d MHz GPU=%d MHz\n", scePowerGetArmClockFrequency(), scePowerGetGpuClockFrequency());

    SceKernelFreeMemorySizeInfo info = {.size = sizeof info};
    sceKernelGetFreeMemorySize(&info);
    hlog("[host] Free user RAM before initialization: %d MiB\n", (int)(info.size_user >> 20));
    int free_ram = (int)info.size_user;
    int threshold = free_ram - (VGL_RAM_MB << 20);
    if (threshold < (SYSTEM_RAM_MB << 20)) threshold = SYSTEM_RAM_MB << 20;
    if (threshold > free_ram) threshold = free_ram;
    vglInitExtended(0, 960, 544, threshold, SCE_GXM_MULTISAMPLE_NONE);
    vglWaitVblankStart(GL_TRUE);

    uint8_t one[4] = {255, 255, 255, 255};
    glGenTextures(1, &g_white);
    bind_texture(g_white);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, one);
    glGenTextures(1, &g_atlas);

    // simple loading screen (one colour)
    glClearColor(0.12f, 0.16f, 0.22f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    vglSwapBuffers(GL_FALSE);

    uint64_t load_start = now_us();
    ngu_set_host_tuning(31); /* background autosave, single-writer storage, fonts worker, layout timing, console mode always on */
    int r = ngu_init(DATA_DIR, 960, 544);
    hlog("[host] ngu_init -> %d in %llu ms\n", r, (unsigned long long)((now_us() - load_start) / 1000));
    if (r != 0) {
        for (;;) {
            glClearColor(0.5f, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            vglSwapBuffers(GL_FALSE);
        }
    }
    sceKernelGetFreeMemorySize(&info);
    hlog("[host] Free user RAM after initialization: %d MiB\n", (int)(info.size_user >> 20));
    setup_draw_state();
#ifdef NGU_VITA_VERTEX_BUFFER
    glGenBuffers(1, &g_vertex_buffer);
#endif
#ifdef NGU_VITA_BENCHMARK
    // Finish the initialization swap before numbering gameplay queue callbacks.
    sceGxmDisplayQueueFinish();
    vglSetDisplayCallback(display_callback);
    SceUID monitor = sceKernelCreateThread("ngu-display-sample", sample_display,
                                           0x10000100, 16 * 1024, 0, 0, NULL);
    int monitor_status = monitor < 0 ? monitor : sceKernelStartThread(monitor, 0, NULL);
    hlog("[host] Display monitor: %d\n", monitor_status);
    { SceIoStat st; // a scenario file turns this run into a recorded, scripted session
      if (sceIoGetstat(PROFILE_DIR "/scenario.txt", &st) >= 0) {
          sceIoRemove(PROFILE_DIR "/done.txt");
          int r = ngu_autoplay_load(PROFILE_DIR "/scenario.txt");
          hlog("[host] Scenario: %d\n", r);
          if (r == 0) trace_start();
      } }
#endif

    // From here on two threads: this one (L) runs input, simulation, layout and the draw list at ~60 Hz;
    // R (render_thread) presents the newest finished list at the display rate, whatever L is doing.
    PERF(g_load_start = load_start);
    host_log_start();
    // the render thread gets its own core (user core 0); if the system refuses the mask, any core
    SceUID rt = sceKernelCreateThread("ngu-render", render_thread, 0x10000100, 512 * 1024, 0, SCE_KERNEL_CPU_MASK_USER_0, NULL);
    if (rt < 0) rt = sceKernelCreateThread("ngu-render", render_thread, 0x10000100, 512 * 1024, 0, 0, NULL);
    int rstatus = rt < 0 ? rt : sceKernelStartThread(rt, 0, NULL);
    hlog("[host] Render thread: %d\n", rstatus);

    uint64_t prev = now_us();
    uint64_t next = prev;
    uint32_t last_seq = 0;
    float tx = 0, ty = 0;
    int down = 0;
    SceCtrlData pad = {.lx = 128, .ly = 128, .rx = 128, .ry = 128};
    for (;;) {
        SceTouchData td = {0};
        sceTouchPeek(SCE_TOUCH_PORT_FRONT, &td, 1);
        if (td.reportNum > 0) {
            tx = td.report[0].x / 2.0f;
            ty = td.report[0].y / 2.0f;
            down = 1;
        } else {
            down = 0;
        }
        uint64_t now = now_us();
        float dt = (now - prev) / 1000000.0f;
        prev = now;
        int res = atomic_exchange(&g_ime_res, 0);
        if (res) {
            ngu_text_submit(g_ime_out, res == 1);
            atomic_store(&g_ime_active, 0);
        }
        PERF(uint64_t tl = now_us());
        SceCtrlData sample;
        if (sceCtrlPeekBufferPositive(0, &sample, 1) > 0) pad = sample;
        int ime = atomic_load(&g_ime_active);
        pad_update(&pad, now, dt, !ime && !down);
        PERF(uint64_t input_us = now_us() - tl);
        if (!ime) ngu_frame(dt, tx, ty, down);
        PERF(uint64_t t_frame = now_us());
        { static int sel_prev; int sel = (pad.buttons & SCE_CTRL_SELECT) != 0;
          if (sel && !sel_prev) atomic_store(&g_shot_request, UINT32_MAX);
          sel_prev = sel; }
        int nb = ngu_emit();
        PERF(uint64_t t_emit = now_us());
        uint32_t seq = ngu_emit_seq();
        int emitted = seq != last_seq;
        if (emitted) {
            last_seq = seq;
            publish(nb);
        }
#ifdef NGU_VITA_BENCHMARK
        {
            LogicSnapshot snapshot = {0};
            ngu_frame_info(snapshot.info);
            static uint32_t shots_seen;
            if (snapshot.info[7] != shots_seen) {
                shots_seen = snapshot.info[7];
                atomic_store(&g_shot_request, shots_seen);
            }
            snapshot.game_us = ngu_clock_us(&snapshot.debt_us);
            unsigned emit_us = (unsigned)(t_emit - t_frame);
            unsigned layout_us = emitted ? ngu_layout_us() : 0;
            if (layout_us > emit_us) layout_us = emit_us;
            unsigned logic_us = (unsigned)(t_frame - tl - input_us);
            unsigned iteration_us = (unsigned)(now_us() - tl);
            unsigned batches = nb > 0 ? (unsigned)nb : 0;
            metrics_lock();
            g_logic_snapshot = snapshot;
            logic_record(&g_logic_report, (unsigned)input_us, logic_us, layout_us,
                         emit_us - layout_us, batches, g_pad_edges, iteration_us);
            if (g_collect_trace) {
                logic_record(&g_logic_trace, (unsigned)input_us, logic_us, layout_us,
                             emit_us - layout_us, batches, g_pad_edges, iteration_us);
            }
            metrics_unlock();
            g_pad_edges = 0;
        }
#endif
        if (!atomic_load(&g_ime_active)) {
            char tb[300]; int tmax = 0, tnum = 0;
            if (ngu_text_request(tb, sizeof tb, &tmax, &tnum)) {
                memcpy(g_ime_tb, tb, sizeof g_ime_tb);
                g_ime_tmax = tmax;
                g_ime_tnum = tnum;
                atomic_store(&g_ime_active, 1);
                atomic_store(&g_ime_req, 1);
            }
        }
        // pace: ~60 iterations per second (a long iteration just delays the next ones; R keeps presenting)
        next += 16667;
        uint64_t after = now_us();
        if (after < next) sceKernelDelayThread((SceUInt32)(next - after));
        else if (after - next > 50000) next = after;
    }
    return 0;
}

int main(void) {
    SceUID th = sceKernelCreateThread("ngu", game_thread, 0x10000100, 24 * 1024 * 1024, 0, SCE_KERNEL_CPU_MASK_USER_1, NULL);
    sceKernelStartThread(th, 0, NULL);
    sceKernelWaitThreadEnd(th, NULL, NULL);
    return 0;
}
