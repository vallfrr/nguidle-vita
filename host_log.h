#ifndef NGU_HOST_LOG_H
#define NGU_HOST_LOG_H

/* Pass a path that remains valid for the lifetime of the worker. */
void host_log_init(const char *path);
void host_log_start(void);
void hlog(const char *format, ...) __attribute__((format(printf, 1, 2)));

#endif
