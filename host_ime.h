#ifndef NGU_HOST_IME_H
#define NGU_HOST_IME_H

#include <stdatomic.h>

/* The engine and renderer exchange requests and replies through the atomics. */
extern atomic_int g_ime_active, g_ime_req, g_ime_res;
extern char g_ime_tb[300], g_ime_out[600];
extern int g_ime_tmax, g_ime_tnum;

/* Render thread only. */
extern int g_ime_dialog;
void ime_start(const char *init, int max, int numeric);
void ime_poll(void);

#endif
