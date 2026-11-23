

#ifndef UAPI_SYS_DELAY_H
#define UAPI_SYS_DELAY_H

#include <linux/ioctl.h>

struct xm_sys_delay_settings {
	unsigned int activated;
	int threshold_ms;
};

struct sys_delay_detail {
	unsigned long delay_ns;
	////struct xm_task_detail task;
	////struct xm_kern_stack_detail kern_stack;
	/////struct xm_user_stack_detail user_stack;
	////struct xm_proc_chains_detail proc_chains;
	////struct xm_raw_stack_detail raw_stack;
};

#endif /* UAPI_SYS_DELAY_H */
