

#ifndef UAPI_SYS_COST_H
#define UAPI_SYS_COST_H

#include <linux/ioctl.h>

struct xm_sys_cost_settings {
	unsigned int activated;
};

#define USER_NR_syscalls_virt 500

#if 0  //syscall count
struct sys_cost_detail {
	unsigned long cpu;
	unsigned long count[USER_NR_syscalls_virt];
	unsigned long cost[USER_NR_syscalls_virt];
};
#endif

#endif /* UAPI_SYS_COST_H */
