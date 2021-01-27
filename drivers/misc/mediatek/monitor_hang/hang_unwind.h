#ifndef _HANG_UNWIND_H
#define _HANG_UNWIND_H
extern unsigned int hang_kernel_trace(struct task_struct *tsk,
					unsigned long *store, unsigned int size);
#endif
