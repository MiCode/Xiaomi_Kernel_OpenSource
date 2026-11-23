#include <linux/syscalls.h>
#include <linux/hrtimer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/timex.h>
#include <linux/tracepoint.h>
//#include <trace/events/irq.h>
#include <trace/events/napi.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sysctl.h>
#include <linux/semaphore.h>
#include <linux/vmalloc.h>

#include "pub/trace_point.h"

#include "internal.h"

#include "uapi/sys_delay.h"
#include "uapi/irq_stats.h"
#include "uapi/irq_trace.h"
////#include "uapi/kprobe.h"
#include "uapi/rw_sem.h"

static atomic64_t xm_nr_running = ATOMIC64_INIT(0);

static char *xm_context_mem;
struct xm_percpu_context *xm_percpu_context[NR_CPUS];

static int lookup_syms(void)
{
	return 0;
}

int xiaomi_linux_proc_init(void)
{
	struct proc_dir_entry *pe;

	pe = xiaomi_proc_mkdir(XIAOMI_PROC_TRACE, NULL);
	if (!pe)
	{
	    printk("mi_kernel create /proc/mi_kernel failed\n");
	}
	
	return 0;
}

struct pt_regs *xm_get_irq_regs(void)
{
	return get_irq_regs();//task_pt_regs(current); //__this_cpu_read(orig___irq_regs);
}

void xiaomi_linux_proc_exit(void)
{
	remove_proc_subtree(XIAOMI_PROC_TRACE, NULL);
}

static void trace_sys_enter_hit(void *__data, struct pt_regs *regs, long id)
{
}

static int sys_enter_hooked = 0;

void xiaomi_hook_sys_enter(void)
{
	if (sys_enter_hooked)
		return;

	hook_tracepoint("sys_enter", trace_sys_enter_hit, NULL);
	sys_enter_hooked = 1;
}

void xiaomi_unhook_sys_enter(void)
{
	if (!sys_enter_hooked)
		return;

	unhook_tracepoint("sys_enter", trace_sys_enter_hit, NULL);
	sys_enter_hooked = 0;
}

static int __init xiaomi_kernel_monitor_init(void)
{
	int ret = 0;
	char cgroup_buf[256];
	int xm_context_size;
	int i;

	ret = xiaomi_init_symbol();
	if (ret)
		goto out;

	printk("mi_kernel xiaomi_kernel_monitor_init begin\n");
	ret = xiaomi_symbols_init();
	if (ret)
		goto out;

	xiaomi_cgroup_name(current, cgroup_buf, 255, 0);
	if ((strlen(cgroup_buf) > 1 && strcmp("user.slice", cgroup_buf) != 0 && strcmp("system.slice", cgroup_buf) != 0
	    && strcmp("sshd.service", cgroup_buf) != 0 && strcmp("tianji", cgroup_buf) != 0)
	  || (strlen(cgroup_buf) == 1 && cgroup_buf[0] != '/')) {
		printk(KERN_ALERT "mi_kernel: insmod in %s\n", cgroup_buf);
	}

	ret = -ENOMEM;
	memset(xm_percpu_context, 0, NR_CPUS * sizeof(struct xm_percpu_context *));

	xm_context_size = num_possible_cpus() * sizeof(struct xm_percpu_context);
	xm_context_mem = vmalloc(xm_context_size);
	if (!xm_context_mem)
		goto out_context;

	memset(xm_context_mem, 0, xm_context_size);
	for (i = 0; i < num_possible_cpus(); i++) {
		xm_percpu_context[i] = (struct xm_percpu_context *)(xm_context_mem + i * sizeof(struct xm_percpu_context));
	}

	ret = xiaomi_linux_proc_init();

	if (ret) {
		pr_err("xiaomi_linux_proc_init failed.\n");
		goto out_proc;
	}
        
	ret = xiaomi_kernel_init();
	if (ret) {
		pr_err("xiaomi_kernel_init failed.\n");
		goto out_kern;
	}
                 
	ret = xiaomi_huqh_test_init();
	if (ret) {
		pr_err("xiaomi_huqh_test_init failed.\n");
		goto out_huqh_test;
	}

	lookup_syms();
	
	if (orig_kptr_restrict) {
		*orig_kptr_restrict = 0;
	}

	printk("mi_kernel xiaomi_kernel_monitor_init end\n");

	return 0;

out_huqh_test:
	xiaomi_kernel_exit();
out_kern:
	xiaomi_linux_proc_exit();
out_proc:
	if (xm_context_mem) {
		vfree(xm_context_mem);
		xm_context_mem = NULL;
	}
out_context:
	xiaomi_symbols_exit();
out:
	return ret;
}

static void __exit xiaomi_kernel_monitor_exit(void)
{
	printk("mi_kernel xiaomi_kernel_monitor_exit begin.\n");

	if (sys_enter_hooked)
		xiaomi_unhook_sys_enter();
	
	msleep(20);
	while (atomic64_read(&xm_nr_running) > 0)
		msleep(20);

	xiaomi_huqh_test_exit();
	msleep(20);

	xiaomi_kernel_exit();
	msleep(20);

	xiaomi_symbols_exit();
	msleep(20);

	xiaomi_linux_proc_exit();
	msleep(20);

	synchronize_sched();

	if (xm_context_mem) {
		vfree(xm_context_mem);
		xm_context_mem = NULL;
	}

	printk("mi_kernel xiaomi_kernel_monitor_exit end.\n");
}

late_initcall(xiaomi_kernel_monitor_init);
module_exit(xiaomi_kernel_monitor_exit);

////MODULE_DESCRIPTION("Xiaomi kernel monitor module");
////MODULE_AUTHOR("qinghe hu <huqinghe1@xiaomi.com>");
MODULE_LICENSE("GPL v2");
