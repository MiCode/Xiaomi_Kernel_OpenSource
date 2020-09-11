/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/rtc.h>
#include <linux/cpu.h>
/*  */

#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/watchdog.h>
#include <linux/platform_device.h>



#include <linux/uaccess.h>
#include <mach/irqs.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_typedefs.h>
#include <mach/mtk_wdt.h>
#include <linux/delay.h>

#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <mach/wd_api.h>
#include <mt-plat/aee.h>
#include <ext_wd_drv.h>

/* extern int nr_cpu_ids; */
/* int enable_clock(int id, unsigned char *name); */
static int test_case;
module_param(test_case, int, 0664);
static DEFINE_SPINLOCK(wdt_test_lock0);
static DEFINE_SPINLOCK(wdt_test_lock1);
static struct task_struct *wk_tsk[2];	/* cpu: 2 */
static int data;

static int __cpuinit cpu_callback(struct notifier_block *nfb,
	unsigned long action, void *hcpu)
{
	int hotcpu = (unsigned long)hcpu;

	switch (action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
/* watchdog_prepare_cpu(hotcpu); */
		break;
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
/*  */
		if (hotcpu < nr_cpu_ids) {
			kthread_bind(wk_tsk[hotcpu], hotcpu);
			wake_up_process(wk_tsk[hotcpu]);
			pr_notice("[WDK-test]cpu %d plug on ", hotcpu);
		}
		break;
#ifdef CONFIG_HOTPLUG_CPU
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		pr_notice("[WDK-test]:start Stop CPU:%d\n", hotcpu);

		break;
#endif				/* CONFIG_HOTPLUG_CPU */
	}

	/*
	 * hardlockup and softlockup are not important enough
	 * to block cpu bring up.  Just always succeed and
	 * rely on printk output to flag problems.
	 */
	return NOTIFY_OK;
}

static struct notifier_block cpu_nfb __cpuinitdata = {
	.notifier_call = cpu_callback
};


static int kwdt_thread_test(void *arg)
{
	struct sched_param param = {.sched_priority = 99 };
	int cpu;
	unsigned int flags;

	sched_setscheduler(current, SCHED_FIFO, &param);

	set_current_state(TASK_INTERRUPTIBLE);
	for (;;) {
		pr_debug("wd_test debug start, cpu:%d\n", cpu);
		spin_lock(&wdt_test_lock0);
		cpu = smp_processor_id();
		spin_unlock(&wdt_test_lock0);

		if (test_case == (cpu * 10 + 1)) {/* cpu0 Preempt disale */
			pr_debug("CPU:%d, Preempt disable\n", cpu);
			spin_lock(&wdt_test_lock1);
		}
		if (test_case == (cpu * 10 + 2)) {/* cpu0 Preempt&irq disale */
			pr_debug("CPU:%d, irq & Preempt disable\n", cpu);
			spin_lock_irqsave(&wdt_test_lock1, flags);
		}
		msleep(5 * 1000);	/* 5s */
		wdt_dump_reg();
		pr_debug("wd_test debug end, cpu:%d\n", cpu);
	}
	return 0;
}

static int start_kicker(void)
{

	int i;
	unsigned char name[64] = { 0 };


	for (i = 0; i < nr_cpu_ids; i++) {
		sprintf(name, "wdtk-test-%d", i);
		pr_debug("[WDK]:thread name: %s\n", name);
		wk_tsk[i] = kthread_create(kwdt_thread_test, &data, name);
		if (IS_ERR(wk_tsk[i])) {
			int ret = PTR_ERR(wk_tsk[i]);

			wk_tsk[i] = NULL;
			return ret;
		}
		kthread_bind(wk_tsk[i], i);
		wake_up_process(wk_tsk[i]);
	}
	return 0;
}

static int __init test_init(void)
{
	/* enable_clock(12, "Vfifo"); */

	register_cpu_notifier(&cpu_nfb);
	start_kicker();
	return 0;
}

static void __init test_exit(void)
{

}
module_init(test_init);
module_exit(test_exit);
