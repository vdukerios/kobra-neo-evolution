/* Minimal newlib-nano syscalls for the GCC build of the Kobra Neo firmware.
 * The Keil build used MicroLIB with a 0x3200 heap placed by startup (__heap_base/__heap_limit). */
#include <stddef.h>
#include <errno.h>
#include <sys/stat.h>
#undef errno
extern int errno;
extern char __heap_base, __heap_limit;

void *_sbrk(ptrdiff_t incr) {
    static char *brk = 0;
    if (!brk) brk = &__heap_base;
    if (brk + incr > &__heap_limit) { errno = ENOMEM; return (void *)-1; }
    char *p = brk; brk += incr; return p;
}
void _init(void) {}
void _fini(void) {}
int _close(int fd) { (void)fd; return -1; }
int _fstat(int fd, struct stat *st) { (void)fd; st->st_mode = S_IFCHR; return 0; }
int _isatty(int fd) { (void)fd; return 1; }
int _lseek(int fd, int off, int wh) { (void)fd; (void)off; (void)wh; return 0; }
int _read(int fd, char *p, int n) { (void)fd; (void)p; (void)n; return 0; }
int _getpid(void) { return 1; }
int _kill(int pid, int sig) { (void)pid; (void)sig; errno = EINVAL; return -1; }
void _exit(int status) { (void)status; for (;;) {} }
