#include "config.h"

#if (defined(WIN32) || defined(_X64))
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "tfa_internal.h"

void *kmalloc(size_t size, gfpt_t flags)
{
	/* flags are not used outside the Linux kernel */
	(void)flags;

#if !defined(__REDLIB__)
	return malloc(size);
#else

#endif
}

void kfree(const void *ptr)
{
#if !defined(__REDLIB__)
	free((void *)ptr);
#else

#endif
}

unsigned long msleep_interruptible(unsigned int msecs)
{
#if (defined(WIN32) || defined(_X64))
	Sleep(msecs);
#else
	usleep(1000 * msecs);
#endif
	return 0;
}
