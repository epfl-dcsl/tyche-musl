#include <sys/mman.h>
#include "syscall.h"
#include "tyche.h"

static void dummy(void) { }
weak_alias(dummy, __vm_wait);

int __munmap(void *start, size_t len)
{
#ifndef TYCHE_RAW_MEM_SYSCALL
    return tyche_munmap(start, len);
#else
	__vm_wait();
	return syscall(SYS_munmap, start, len);
#endif
}

weak_alias(__munmap, munmap);
