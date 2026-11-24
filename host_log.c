#include "host_log.h"
#include "log_queue.h"
#include <psp2/kernel/threadmgr.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>

// Log lines: written at once during start-up, then handed to a worker thread so neither the logic nor the render thread waits on storage
// (a synchronous append took ~80 ms and showed up as a missed-frame run every 5 s).
static const char *g_log_path;
static LogQueue g_logq;
static atomic_flag g_log_lock = ATOMIC_FLAG_INIT;
static SceUID g_log_sema = -1;
static atomic_int g_log_async;

static void hlog_sync(const char *text) {
    FILE *f = fopen(g_log_path, "a");
    if (!f) return;
    fputs(text, f);
    fclose(f);
}

static int log_thread(SceSize args, void *argp) {
    (void)args; (void)argp;
    unsigned reported = 0;
    for (;;) {
        sceKernelWaitSema(g_log_sema, 1, NULL);
        for (;;) {
            char line[LOG_LINE_SIZE];
            while (atomic_flag_test_and_set_explicit(&g_log_lock, memory_order_acquire)) {}
            int available = log_queue_pop(&g_logq, line);
            unsigned dropped = g_logq.dropped;
            atomic_flag_clear_explicit(&g_log_lock, memory_order_release);
            if (dropped != reported) {
                char loss[80];
                snprintf(loss, sizeof loss, "[host] log lines dropped total=%u\n", dropped);
                hlog_sync(loss);
                reported = dropped;
            }
            if (!available) break;
            hlog_sync(line);
        }
    }
    return 0;
}

void hlog(const char *fmt, ...) {
    char buf[LOG_LINE_SIZE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (!atomic_load(&g_log_async)) { hlog_sync(buf); return; }
    while (atomic_flag_test_and_set_explicit(&g_log_lock, memory_order_acquire)) {}
    int wake = log_queue_push(&g_logq, buf);
    atomic_flag_clear_explicit(&g_log_lock, memory_order_release);
    if (wake) sceKernelSignalSema(g_log_sema, 1);
}

void host_log_init(const char *path) {
    g_log_path = path;
    remove(path);
}

void host_log_start(void) {
    g_log_sema = sceKernelCreateSema("ngu-log", 0, 0, 1, NULL);
    SceUID lt = g_log_sema < 0 ? g_log_sema : sceKernelCreateThread("ngu-log", log_thread, 0x10000100, 64 * 1024, 0, 0, NULL);
    if (lt >= 0 && sceKernelStartThread(lt, 0, NULL) >= 0) atomic_store(&g_log_async, 1);
}
