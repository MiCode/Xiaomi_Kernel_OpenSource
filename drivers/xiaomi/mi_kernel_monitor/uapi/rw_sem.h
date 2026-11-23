

#ifndef UAPI_RW_SEM_H
#define UAPI_RW_SEM_H

#include <linux/ioctl.h>

struct xm_rw_sem_settings {
	unsigned int activated;
	////unsigned int verbose;
	////unsigned int style;
	int threshold;
};

struct rw_sem_detail {
	unsigned long delay_ns;
	////void *lock;
	////struct xm_task_detail task;
	////struct xm_kern_stack_detail kern_stack;
	////struct xm_user_stack_detail user_stack;
	////struct xm_proc_chains_detail proc_chains;
};

#endif /* UAPI_RW_SEM_H */
