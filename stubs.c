// POSIX functions missing from newlib/Vita but referenced by the Rust standard library.
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

int symlink(const char *a, const char *b) { (void)a; (void)b; errno = ENOSYS; return -1; }
int dup2(int a, int b) { (void)a; (void)b; errno = ENOSYS; return -1; }
int mkfifo(const char *p, mode_t m) { (void)p; (void)m; errno = ENOSYS; return -1; }
int lchown(const char *p, uid_t u, gid_t g) { (void)p; (void)u; (void)g; errno = ENOSYS; return -1; }
int fchown(int fd, uid_t u, gid_t g) { (void)fd; (void)u; (void)g; errno = ENOSYS; return -1; }
ssize_t readlink(const char *p, char *b, size_t n) { (void)p; (void)b; (void)n; errno = ENOSYS; return -1; }
pid_t getppid(void) { return 1; }
