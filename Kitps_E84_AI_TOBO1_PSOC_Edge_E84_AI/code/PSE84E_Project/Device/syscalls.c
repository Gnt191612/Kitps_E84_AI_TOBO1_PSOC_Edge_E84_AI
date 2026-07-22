/**
 * @file    syscalls.c
 * @brief   Newlib-nano syscall stubs for bare-metal ARM
 *
 * Required to link with -nostdlib + -lgcc. These define the minimal
 * syscalls needed by newlib (used indirectly via printf/puts/abort/etc.).
 */

#include <sys/stat.h>
#include <sys/types.h>

/* TLS / errno */
int *__errno(void)
{
    static int _errno_val = 0;
    return &_errno_val;
}

void _exit(int status)
{
    (void)status;
    while (1) { }
}

/* ======================================================================== */
/* PDL 要求的临界区保护函数（pdld 未提供实现）                                */
/* ======================================================================== */
uint32_t Cy_SysLib_EnterCriticalSection(void)
{
    uint32_t primask;
    __asm volatile("mrs %0, PRIMASK" : "=r"(primask));
    __asm volatile("cpsid i");
    return primask;
}

void Cy_SysLib_ExitCriticalSection(uint32_t saved)
{
    __asm volatile("msr PRIMASK, %0" : : "r"(saved));
}

int _close(int file)
{
    (void)file;
    return -1;
}

int _fstat(int file, struct stat *st)
{
    (void)file;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    (void)file;
    return 1;
}

int _lseek(int file, int ptr, int dir)
{
    (void)file; (void)ptr; (void)dir;
    return 0;
}

int _read(int file, char *ptr, int len)
{
    (void)file; (void)ptr; (void)len;
    return 0;
}

int _write(int file, char *ptr, int len)
{
    (void)file; (void)ptr;
    return len;
}

/* ──── Cy_SysLib_DelayCycles ──── */
void Cy_SysLib_DelayCycles(uint32_t cycles)
{
    volatile uint32_t i;
    for (i = 0; i < cycles; i++) {
        __asm volatile("nop");
    }
}

int _getpid(void)
{
    return 1;
}

int _kill(int pid, int sig)
{
    (void)pid; (void)sig;
    return -1;
}

caddr_t _sbrk(int incr)
{
    extern char __heap_start__;  /* from linker script */
    extern char __heap_end__;
    static char *heap_end = NULL;

    if (heap_end == NULL) {
        heap_end = &__heap_start__;
    }

    char *prev = heap_end;
    if (heap_end + incr > &__heap_end__) {
        return (caddr_t)-1;
    }
    heap_end += incr;
    return (caddr_t)prev;
}
