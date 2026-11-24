#include "host_ime.h"
#include <psp2/ime_dialog.h>
#include <stdint.h>
#include <string.h>

// ---------------------------------------------------------------- system keyboard (IME)
atomic_int g_ime_active, g_ime_req, g_ime_res; // active: set by L when it posts a request, cleared by L once it has the result
int g_ime_dialog;                              // R only
char g_ime_tb[300], g_ime_out[600];
int g_ime_tmax, g_ime_tnum;
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

void ime_start(const char *init, int max, int numeric) {
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
    if (sceImeDialogInit(&p) >= 0) g_ime_dialog = 1;
    else { g_ime_out[0] = 0; atomic_store(&g_ime_res, 2); }
}

void ime_poll(void) {
    if (!g_ime_dialog) return;
    if (sceImeDialogGetStatus() == SCE_COMMON_DIALOG_STATUS_FINISHED) {
        SceImeDialogResult r;
        memset(&r, 0, sizeof r);
        sceImeDialogGetResult(&r);
        if (r.button == SCE_IME_DIALOG_BUTTON_ENTER) {
            utf16_to_utf8(g_ime_buf, g_ime_out, sizeof g_ime_out);
            atomic_store(&g_ime_res, 1);
        } else {
            g_ime_out[0] = 0;
            atomic_store(&g_ime_res, 2);
        }
        sceImeDialogTerm();
        g_ime_dialog = 0;
    }
}
