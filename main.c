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

#define DATA_DIR "ux0:data/ngu"
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
uint32_t ngu_layout_us(void);
const NguBatch *ngu_batches(void);
const NguVertex *ngu_vertices(void);
const uint8_t *ngu_tex(uint32_t id, uint32_t *w, uint32_t *h);
const uint8_t *ngu_atlas(int *dirty, uint32_t *size);
int ngu_text_request(char *buf, int cap, int *max, int *numeric);
void ngu_text_submit(const char *text, int ok);
uint64_t ngu_clock_us(uint64_t *debt_us);

static void hlog(const char *fmt, ...) {
    FILE *f = fopen(LOG_FILE, "a");
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fclose(f);
}

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

static void upload_atlas(void) {
    int dirty = 0;
    uint32_t size = 0;
    const uint8_t *px = ngu_atlas(&dirty, &size);
    if (!px || (g_atlas_size && !dirty)) return;
    bind_texture(g_atlas);
    if (!g_atlas_size) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
        g_atlas_size = size;
    } else if (dirty) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size, size, GL_RGBA, GL_UNSIGNED_BYTE, px);
    }
}

void ngu_tex_release(uint32_t id);
#define TEX_BUDGET (70u << 20)
static uint32_t g_tex_size[MAX_TEX];
static uint64_t g_tex_use[MAX_TEX];
static uint64_t g_frame_no;
static uint32_t g_tex_total;
static int g_tex_count;

// frees the least recently used textures until the budget is met
static void evict_for(uint32_t need) {
    while (g_tex_total + need > TEX_BUDGET) {
        int best = -1;
        uint64_t bu = 0xFFFFFFFFFFFFFFFFull;
        for (int i = 2; i < MAX_TEX; i++) {
            if (g_tex[i] && g_tex_use[i] + 2 < g_frame_no && g_tex_use[i] < bu) { bu = g_tex_use[i]; best = i; }
        }
        if (best < 0) return; // everything is used in the current frame
        if (g_bound_texture == g_tex[best]) bind_texture(0);
        glDeleteTextures(1, &g_tex[best]);
        g_tex[best] = 0;
        g_filter[best] = 0;
        g_tex_total -= g_tex_size[best];
        g_tex_count--;
    }
}

static GLuint get_tex(uint32_t id) {
    if (id == 0) return g_white;
    if (id == 1) return g_atlas;
    if (id >= MAX_TEX) return g_white;
    if (!g_tex[id]) {
        uint32_t w = 0, h = 0;
        const uint8_t *px = ngu_tex(id, &w, &h);
        if (!px) return g_white;
        evict_for(w * h * 4);
        glGenTextures(1, &g_tex[id]);
        bind_texture(g_tex[id]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        g_tex_size[id] = w * h * 4;
        g_tex_total += g_tex_size[id];
        g_tex_count++;
        ngu_tex_release(id); // the RAM copy is no longer needed
    }
    g_tex_use[id] = g_frame_no;
    return g_tex[id];
}

static uint64_t now_us(void) { return sceKernelGetProcessTimeWide(); }

#ifdef NGU_VITA_BENCHMARK
#define PERF(code) code
static uint64_t g_t_emit, g_t_layout, g_t_draw, g_t_swap, g_t_logic, g_t_upload, g_t_input;
static unsigned g_frame_ms[251], g_frame_max, g_draws, g_shots;
static unsigned g_input_max, g_pad_edges;
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

static int g_want_shot;


// ---------------------------------------------------------------- system keyboard (IME)
static int g_ime_active;
static uint16_t g_ime_title[32], g_ime_init[130], g_ime_buf[130];

static void utf8_to_utf16(const char *s, uint16_t *d, int cap) {
    int n = 0;
    while (*s && n < cap - 1) {
        uint32_t c = (uint8_t)*s++;
        if (c >= 0xF0) { c = ((c & 7) << 18) | ((uint8_t)s[0] & 63) << 12 | ((uint8_t)s[1] & 63) << 6 | ((uint8_t)s[2] & 63); s += 3; }
        else if (c >= 0xE0) { c = ((c & 15) << 12) | (((uint8_t)s[0] & 63) << 6) | ((uint8_t)s[1] & 63); s += 2; }
        else if (c >= 0xC0) { c = ((c & 31) << 6) | ((uint8_t)s[0] & 63); s += 1; }
        if (c >= 0x10000) { c -= 0x10000; if (n < cap - 2) { d[n++] = 0xD800 | (c >> 10); d[n++] = 0xDC00 | (c & 0x3FF); } }
        else d[n++] = (uint16_t)c;
    }
    d[n] = 0;
}

static void utf16_to_utf8(const uint16_t *s, char *d, int cap) {
    int n = 0;
    while (*s && n < cap - 5) {
        uint32_t c = *s++;
        if (c >= 0xD800 && c < 0xDC00 && *s) { c = 0x10000 + ((c & 0x3FF) << 10) + (*s++ & 0x3FF); }
        if (c < 0x80) d[n++] = c;
        else if (c < 0x800) { d[n++] = 0xC0 | (c >> 6); d[n++] = 0x80 | (c & 63); }
        else if (c < 0x10000) { d[n++] = 0xE0 | (c >> 12); d[n++] = 0x80 | ((c >> 6) & 63); d[n++] = 0x80 | (c & 63); }
        else { d[n++] = 0xF0 | (c >> 18); d[n++] = 0x80 | ((c >> 12) & 63); d[n++] = 0x80 | ((c >> 6) & 63); d[n++] = 0x80 | (c & 63); }
    }
    d[n] = 0;
}

static void ime_start(const char *init, int max, int numeric) {
    SceImeDialogParam p;
    sceImeDialogParamInit(&p);
    utf8_to_utf16("NGU Idle", g_ime_title, 32);
    utf8_to_utf16(init, g_ime_init, 130);
    memset(g_ime_buf, 0, sizeof g_ime_buf);
    p.supportedLanguages = 0x0001FFFF;
    p.languagesForced = SCE_TRUE;
    p.type = numeric ? SCE_IME_TYPE_EXTENDED_NUMBER : SCE_IME_TYPE_DEFAULT;
    p.option = 0;
    p.dialogMode = SCE_IME_DIALOG_DIALOG_MODE_WITH_CANCEL;
    p.textBoxMode = SCE_IME_DIALOG_TEXTBOX_MODE_WITH_CLEAR;
    p.title = g_ime_title;
    p.maxTextLength = (max > 0 && max <= 128) ? max : 128;
    p.initialText = g_ime_init;
    p.inputTextBuffer = g_ime_buf;
    if (sceImeDialogInit(&p) >= 0) g_ime_active = 1;
    else ngu_text_submit("", 0);
}

static void ime_poll(void) {
    if (!g_ime_active) return;
    if (sceImeDialogGetStatus() == SCE_COMMON_DIALOG_STATUS_FINISHED) {
        SceImeDialogResult r;
        memset(&r, 0, sizeof r);
        sceImeDialogGetResult(&r);
        if (r.button == SCE_IME_DIALOG_BUTTON_ENTER) {
            char out[600];
            utf16_to_utf8(g_ime_buf, out, sizeof out);
            ngu_text_submit(out, 1);
        } else {
            ngu_text_submit("", 0);
        }
        sceImeDialogTerm();
        g_ime_active = 0;
    }
}

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

static void draw_frame(void) {
    g_frame_no++;
    PERF(uint64_t t0 = now_us());
    int nb = ngu_emit();
    PERF(uint64_t t1 = now_us());
    PERF(g_t_emit += t1 - t0);
    PERF(g_t_layout += ngu_layout_us());
    PERF(g_draws += nb);
    const NguBatch *b = ngu_batches();
    const NguVertex *v = ngu_vertices();
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
    PERF(uint64_t upload_time = now_us() - before_atlas);
    int32_t last_clip[4] = {-1, -1, -1, -1};
    for (int i = 0; i < nb; i++) {
        int x0 = b[i].clip[0] < 0 ? 0 : b[i].clip[0];
        int y0 = b[i].clip[1] < 0 ? 0 : b[i].clip[1];
        int x1 = b[i].clip[2] > 960 ? 960 : b[i].clip[2];
        int y1 = b[i].clip[3] > 544 ? 544 : b[i].clip[3];
        if (x1 <= x0 || y1 <= y0) continue;
        uint32_t id = b[i].tex;
#ifdef NGU_VITA_BENCHMARK
        int missing = id > 1 && id < MAX_TEX && !g_tex[id];
        uint64_t texture_start = missing ? now_us() : 0;
#endif
        GLuint texture = get_tex(id);
        PERF(if (missing) upload_time += now_us() - texture_start);
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
    PERF(g_t_upload += upload_time);
    PERF(g_t_draw += now_us() - t1 - upload_time);
    if (g_want_shot) {
        save_shot(PROFILE_DIR "/shot.tga");
        g_want_shot = 0;
        PERF(g_shots++);
    }
    PERF(uint64_t before_swap = now_us());
    vglSwapBuffers(g_ime_active ? GL_TRUE : GL_FALSE);
    PERF(g_t_swap += now_us() - before_swap);
}

static float axis(uint8_t value) {
    float normalized = (value - 128.0f) / 128.0f;
    if (normalized > 0.22f) return (normalized - 0.22f) / 0.78f;
    if (normalized < -0.22f) return (normalized + 0.22f) / 0.78f;
    return 0.0f;
}

static void pad_update(const SceCtrlData *pad, uint64_t now, float dt, int enabled) {
    static uint32_t previous;
    static char direction;
    static uint64_t next_repeat;
    uint32_t pressed = pad->buttons & ~previous;
    previous = pad->buttons;
    PERF(if (pressed & (SCE_CTRL_CROSS | SCE_CTRL_CIRCLE | SCE_CTRL_LTRIGGER | SCE_CTRL_RTRIGGER |
                        SCE_CTRL_UP | SCE_CTRL_DOWN | SCE_CTRL_LEFT | SCE_CTRL_RIGHT)) g_pad_edges++);
    if (!enabled) {
        direction = 0;
        return;
    }
    if (pressed & SCE_CTRL_CROSS) ngu_pad('a');
    else if (pressed & SCE_CTRL_CIRCLE) ngu_pad('b');
    else if (pressed & SCE_CTRL_LTRIGGER) ngu_pad('p');
    else if (pressed & SCE_CTRL_RTRIGGER) ngu_pad('n');

    uint32_t held = pad->buttons;
    char next = 0;
    if ((held & SCE_CTRL_UP) || pad->ly < 58) next = 'u';
    else if ((held & SCE_CTRL_DOWN) || pad->ly > 198) next = 'd';
    else if ((held & SCE_CTRL_LEFT) || pad->lx < 58) next = 'l';
    else if ((held & SCE_CTRL_RIGHT) || pad->lx > 198) next = 'r';
    if (next && (next != direction || now >= next_repeat)) {
#ifdef NGU_VITA_BENCHMARK
        unsigned start = (unsigned)now_us();
        int moved = ngu_pad(next);
        unsigned dpad = SCE_CTRL_UP | SCE_CTRL_DOWN | SCE_CTRL_LEFT | SCE_CTRL_RIGHT;
        if (moved && (pressed & dpad) && !atomic_load_explicit(&g_focus_frame, memory_order_acquire)) {
            atomic_store(&g_focus_press_us, start);
            atomic_store_explicit(&g_focus_frame, g_frame_no + 1, memory_order_release);
        }
#else
        ngu_pad(next);
#endif
        next_repeat = now + (next != direction ? 400000 : 120000);
    }
    direction = next;
    float dx = -axis(pad->rx), dy = axis(pad->ry);
    float distance = 390.0f * (dt < 0.1f ? dt : 0.1f);
    if (dx || dy) ngu_scroll(dx * distance, dy * distance);
}

static int game_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
    sceIoMkdir(DATA_DIR, 0777);
#ifdef NGU_VITA_BENCHMARK
    sceIoMkdir(PROFILE_DIR, 0777);
    if (setenv("NGU_VITA_SAVE_DIR", PROFILE_DIR "/save", 1) != 0) return 1;
#ifdef NGU_VITA_PROFILE
    setenv("NGU_VITA_PROFILE", "1", 1);
#endif
#endif
    remove(LOG_FILE);
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
#endif

    uint64_t prev = sceKernelGetProcessTimeWide();
    PERF(uint64_t fps_t = prev);
    PERF(uint64_t last_present = prev);
    PERF(int fps_n = 0);
#ifdef NGU_VITA_BENCHMARK
    uint64_t previous_run_clocks = 0;
#endif
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
        uint64_t now = sceKernelGetProcessTimeWide();
        float dt = (now - prev) / 1000000.0f;
        prev = now;
        PERF(uint64_t tl = now_us());
        SceCtrlData sample;
        if (sceCtrlPeekBufferPositive(0, &sample, 1) > 0) pad = sample;
        pad_update(&pad, now, dt, !g_ime_active && !down);
        PERF(uint64_t input_us = now_us() - tl);
        PERF(g_t_input += input_us);
        PERF(if (input_us > g_input_max) g_input_max = (unsigned)input_us);
        if (!g_ime_active) ngu_frame(dt, tx, ty, down);
        PERF(g_t_logic += now_us() - tl - input_us);
        { static int sel_prev; int sel = (pad.buttons & SCE_CTRL_SELECT) != 0;
          if (sel && !sel_prev) g_want_shot = 1;
          sel_prev = sel; }
        draw_frame();
        if (!g_ime_active) {
            char tb[300]; int tmax = 0, tnum = 0;
            if (ngu_text_request(tb, sizeof tb, &tmax, &tnum)) ime_start(tb, tmax, tnum);
        } else {
            ime_poll();
        }
#ifdef NGU_VITA_BENCHMARK
        uint64_t end = now_us();
        unsigned ms = (unsigned)((end - last_present + 999) / 1000);
        last_present = end;
        g_frame_ms[ms > 250 ? 250 : ms]++;
        if (ms > g_frame_max) g_frame_max = ms;
        fps_n++;
        if (end - fps_t >= 5000000) {
            float to_ms = 0.001f / fps_n;
            sceKernelGetFreeMemorySize(&info);
            SceKernelThreadInfo thread = {.size = sizeof thread};
            int thread_status = sceKernelGetThreadInfo(sceKernelGetThreadId(), &thread);
            struct mallinfo heap = mallinfo();
            uint64_t run_clocks = thread.runClocks - previous_run_clocks;
            previous_run_clocks = thread.runClocks;
            uint64_t debt_us = 0;
            uint64_t game_us = ngu_clock_us(&debt_us);
            unsigned focus_samples = atomic_exchange(&g_focus_samples, 0);
            unsigned focus_missed = atomic_exchange(&g_focus_missed, 0);
            unsigned focus_slow = atomic_exchange(&g_focus_slow, 0);
            unsigned focus_max = atomic_exchange(&g_focus_max_us, 0);
            if (focus_samples || focus_missed) {
                hlog("[focus] presented=%u over_two_refreshes=%u missed_samples=%u max_us=%u\n",
                     focus_samples, focus_slow, focus_missed, focus_max);
            }
            hlog("[perf] t=%.1fs fps=%.2f p50=%u p95=%u p99=%u max=%u input=%.2f input_max=%u pads=%u logic=%.2f layout=%.2f emit=%.2f draw=%.2f upload=%.2f swap=%.2f batches=%u textures=%d gpu_mb=%u shots=%u scanout=%u repeat=%u sample_errors=%u sample_gaps=%u gpu_submits=%u free_kib=%d/%d/%d heap_kib=%u/%u vgl_free_kib=%u/%u/%u thread=%d run_clocks=%llu core=%d affinity=%x game_us=%llu debt_us=%llu\n",
                (end - load_start) / 1000000.0, fps_n * 1000000.0 / (end - fps_t),
                percentile(fps_n, 50), percentile(fps_n, 95), percentile(fps_n, 99), g_frame_max,
                g_t_input * to_ms, g_input_max, g_pad_edges, g_t_logic * to_ms, g_t_layout * to_ms, (g_t_emit - g_t_layout) * to_ms,
                g_t_draw * to_ms, g_t_upload * to_ms, g_t_swap * to_ms,
                g_draws / fps_n, g_tex_count, g_tex_total >> 20, g_shots,
                atomic_exchange(&g_scanout_frames, 0), atomic_exchange(&g_scanout_repeats, 0),
                atomic_exchange(&g_scanout_errors, 0), atomic_exchange(&g_scanout_gaps, 0),
                atomic_exchange(&g_display_callbacks, 0),
                info.size_user >> 10, info.size_cdram >> 10, info.size_phycont >> 10,
                (unsigned)(heap.uordblks >> 10), (unsigned)(heap.fordblks >> 10),
                (unsigned)(vglMemFree(VGL_MEM_RAM) >> 10), (unsigned)(vglMemFree(VGL_MEM_VRAM) >> 10),
                (unsigned)(vglMemFree(VGL_MEM_PHYCONT) >> 10), thread_status, (unsigned long long)run_clocks,
                thread.lastExecutedCpuId, thread.currentCpuAffinityMask,
                (unsigned long long)game_us, (unsigned long long)debt_us);
            sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
            sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_OLED_DIMMING);
            g_t_input = g_t_logic = g_t_layout = g_t_emit = g_t_draw = g_t_upload = g_t_swap = 0;
            g_input_max = g_pad_edges = 0;
            memset(g_frame_ms, 0, sizeof g_frame_ms);
            g_frame_max = g_draws = g_shots = 0;
            fps_t = end;
            fps_n = 0;
        }
#endif
    }
    return 0;
}

int main(void) {
    SceUID th = sceKernelCreateThread("ngu", game_thread, 0x10000100, 24 * 1024 * 1024, 0, 0, NULL);
    sceKernelStartThread(th, 0, NULL);
    sceKernelWaitThreadEnd(th, NULL, NULL);
    return 0;
}
