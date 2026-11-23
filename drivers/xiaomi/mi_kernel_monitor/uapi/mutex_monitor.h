

#ifndef UAPI_MUTEX_MONITOR_H
#define UAPI_MUTEX_MONITOR_H

#include <linux/ioctl.h>

struct diag_mutex_monitor_settings {
	unsigned int activated;
	////unsigned int verbose;
	////unsigned int style;
	int threshold;
};

struct mutex_monitor_detail {
	////int et_type;
	////struct xm_timespec tv;
	unsigned long delay_ns;
	////void *lock;
	////struct xm_task_detail task;
	////struct xm_kern_stack_detail kern_stack;
	////struct xm_user_stack_detail user_stack;
	////struct xm_proc_chains_detail proc_chains;
};

#endif /* UAPI_MUTEX_MONITOR_H */
