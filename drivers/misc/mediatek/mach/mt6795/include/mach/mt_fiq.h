#ifndef __MT6577_FIQ_H__
#define __MT6577_FIQ_H__

#include <asm/fiq.h>
#include <asm/fiq_debugger.h>
#include <asm/fiq_glue.h>
#include <asm/hardware/gic.h>

#define THREAD_INFO(sp) ((struct thread_info *) \
		((unsigned long)(sp) & ~(THREAD_SIZE - 1)))

extern int request_fiq(int fiq, struct fiq_glue_handler *h);
extern int free_fiq(int fiq);

#endif

