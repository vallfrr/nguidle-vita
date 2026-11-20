// NGU Idle PS Vita: C host (vitaGL + touch) around the Rust library `clr`.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vitasdk.h>
#include <vitaGL.h>

#define DATA_DIR "ux0:data/ngu"
#define LOG_FILE DATA_DIR "/host.txt"
#define HEAP_MB 170
#define VGL_RAM_MB 24

int _newlib_heap_size_user = HEAP_MB * 1024 * 1024;

typedef struct { float x, y, u, v; uint32_t rgba; } NguVertex;
typedef struct { uint32_t tex, nearest; int32_t clip[4]; uint32_t first, count; } NguBatch;

int ngu_init(const char *dir, int w, int h);
void ngu_frame(float dt, float x, float y, int down);
int ngu_emit(void);
const NguBatch *ngu_batches(void);
const NguVertex *ngu_vertices(void);
const uint8_t *ngu_tex(uint32_t id, uint32_t *w, uint32_t *h);
const uint8_t *ngu_atlas(int *dirty, uint32_t *size);
int ngu_text_request(char *buf, int cap, int *max, int *numeric);
void ngu_text_submit(const char *text, int ok);

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
static int g_atlas_size;

static void upload_atlas(void) {
    int dirty = 0;
    uint32_t size = 0;
    const uint8_t *px = ngu_atlas(&dirty, &size);
    if (!px) return;
    glBindTexture(GL_TEXTURE_2D, g_atlas);
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
        glDeleteTextures(1, &g_tex[best]);
        g_tex[best] = 0;
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
        glBindTexture(GL_TEXTURE_2D, g_tex[id]);
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

static double g_t_emit, g_t_draw, g_t_logic;
static uint64_t now_us(void) { return sceKernelGetProcessTimeWide(); }

static void save_shot(const char *path) {
    static uint8_t buf[960 * 544 * 4];
    glReadPixels(0, 0, 960, 544, GL_RGBA, GL_UNSIGNED_BYTE, buf);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    uint8_t hdr[18] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 960 & 255, 960 >> 8, 544 & 255, 544 >> 8, 32, 0x20};
    fwrite(hdr, 1, 18, f);
    for (int y = 543; y >= 0; y--) { // glReadPixels : origine bas ; TGA haut-gauche (0x20)
        uint8_t row[960 * 4];
        for (int x = 0; x < 960; x++) {
            uint8_t *p = &buf[(y * 960 + x) * 4];
            row[x * 4 + 0] = p[2]; row[x * 4 + 1] = p[1]; row[x * 4 + 2] = p[0]; row[x * 4 + 3] = 255;
        }
        fwrite(row, 1, sizeof row, f);
    }
    fclose(f);
    hlog("[host] capture %s\n", path);
}

static int g_want_shot, g_frames;


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

static void ime_start(const char *init, int max) {
    SceImeDialogParam p;
    sceImeDialogParamInit(&p);
    utf8_to_utf16("NGU Idle", g_ime_title, 32);
    utf8_to_utf16(init, g_ime_init, 130);
    memset(g_ime_buf, 0, sizeof g_ime_buf);
    p.supportedLanguages = 0x0001FFFF;
    p.languagesForced = SCE_TRUE;
    p.type = 0;
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

static void draw_frame(void) {
    g_frame_no++;
    uint64_t t0 = now_us();
    int nb = ngu_emit();
    uint64_t t1 = now_us();
    g_t_emit += (t1 - t0) / 1000.0;
    const NguBatch *b = ngu_batches();
    const NguVertex *v = ngu_vertices();
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
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
    glEnable(GL_SCISSOR_TEST);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(2, GL_FLOAT, sizeof(NguVertex), &v[0].x);
    glTexCoordPointer(2, GL_FLOAT, sizeof(NguVertex), &v[0].u);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(NguVertex), &v[0].rgba);
    upload_atlas();
    uint32_t last = 0xFFFFFFFF, last_n = 0xFFFFFFFF;
    for (int i = 0; i < nb; i++) {
        GLuint t = get_tex(b[i].tex);
        if (b[i].tex != last) {
            glBindTexture(GL_TEXTURE_2D, t);
            last = b[i].tex;
            last_n = 0xFFFFFFFF;
        }
        if (b[i].nearest != last_n) {
            GLint f = b[i].nearest ? GL_NEAREST : GL_LINEAR;
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
            last_n = b[i].nearest;
        }
        int x0 = b[i].clip[0] < 0 ? 0 : b[i].clip[0];
        int y0 = b[i].clip[1] < 0 ? 0 : b[i].clip[1];
        int x1 = b[i].clip[2] > 960 ? 960 : b[i].clip[2];
        int y1 = b[i].clip[3] > 544 ? 544 : b[i].clip[3];
        if (x1 <= x0 || y1 <= y0) continue;
        glScissor(x0, 544 - y1, x1 - x0, y1 - y0);
        glDrawArrays(GL_TRIANGLES, b[i].first, b[i].count);
    }
    if (g_want_shot) { save_shot(DATA_DIR "/shot.tga"); g_want_shot = 0; }
    vglSwapBuffers(g_ime_active ? GL_TRUE : GL_FALSE);
    g_t_draw += (now_us() - t1) / 1000.0;
}

static int game_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
    sceIoMkdir(DATA_DIR, 0777);
    remove(LOG_FILE);
    hlog("[host] demarrage\n");
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    scePowerSetArmClockFrequency(333);
    scePowerSetGpuClockFrequency(166);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

    SceKernelFreeMemorySizeInfo info = {.size = sizeof info};
    sceKernelGetFreeMemorySize(&info);
    hlog("[host] RAM utilisateur libre: %d Mo\n", (int)(info.size_user >> 20));
    int threshold = (int)info.size_user - (VGL_RAM_MB << 20);
    vglInitExtended(0, 960, 544, threshold, SCE_GXM_MULTISAMPLE_NONE);
    vglWaitVblankStart(GL_TRUE);

    uint8_t one[4] = {255, 255, 255, 255};
    glGenTextures(1, &g_white);
    glBindTexture(GL_TEXTURE_2D, g_white);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, one);
    glGenTextures(1, &g_atlas);

    // simple loading screen (one colour)
    glClearColor(0.12f, 0.16f, 0.22f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    vglSwapBuffers(GL_FALSE);

    int r = ngu_init(DATA_DIR, 960, 544);
    hlog("[host] ngu_init -> %d\n", r);
    if (r != 0) {
        for (;;) {
            glClearColor(0.5f, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            vglSwapBuffers(GL_FALSE);
        }
    }
    sceKernelGetFreeMemorySize(&info);
    hlog("[host] apres init: RAM libre %d Mo\n", (int)(info.size_user >> 20));

    uint64_t prev = sceKernelGetProcessTimeWide();
    uint64_t fps_t = prev;
    int fps_n = 0;
    float tx = 0, ty = 0;
    int down = 0;
    for (;;) {
        SceTouchData td;
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
        uint64_t tl = now_us();
        if (!g_ime_active) ngu_frame(dt, tx, ty, down);
        g_t_logic += (now_us() - tl) / 1000.0;
        { SceCtrlData pad; sceCtrlPeekBufferPositive(0, &pad, 1);
          static int sel_prev; int sel = (pad.buttons & SCE_CTRL_SELECT) != 0;
          if ((sel && !sel_prev) || g_frames == 150) g_want_shot = 1;
          sel_prev = sel; g_frames++; }
        draw_frame();
        if (!g_ime_active) {
            char tb[300]; int tmax = 0, tnum = 0;
            if (ngu_text_request(tb, sizeof tb, &tmax, &tnum)) ime_start(tb, tmax);
        } else {
            ime_poll();
        }
        fps_n++;
        if (now - fps_t > 5000000) {
            hlog("[host] %.2f img/s | logique %.0f ms, emit %.0f ms, dessin %.0f ms | textures GPU: %d (%u Mo)\n", fps_n * 1000000.0f / (now - fps_t), g_t_logic / fps_n, g_t_emit / fps_n, g_t_draw / fps_n, g_tex_count, g_tex_total >> 20);
            g_t_logic = g_t_emit = g_t_draw = 0;
            fps_t = now;
            fps_n = 0;
        }
    }
    return 0;
}

int main(void) {
    SceUID th = sceKernelCreateThread("ngu", game_thread, 0x10000100, 24 * 1024 * 1024, 0, 0, NULL);
    sceKernelStartThread(th, 0, NULL);
    sceKernelWaitThreadEnd(th, NULL, NULL);
    return 0;
}
