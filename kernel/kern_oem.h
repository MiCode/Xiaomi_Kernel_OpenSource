#ifndef KERN_OEM_H
#define KERN_OEM_H
#include <linux/sched.h>

typedef void (*type_kern_sig_f)(int sig, struct task_struct *killer,
		struct task_struct *target);

struct kernel_oem_hook_s {
	type_kern_sig_f sig;
};

#endif
