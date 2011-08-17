/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/pm.h>
#include <linux/mfd/pmic8058.h>
#include <linux/jiffies.h>
#include <linux/suspend.h>
#include <linux/interrupt.h>
#include <mach/msm_iomap.h>
#include <asm/mach-types.h>
#include <mach/scm-io.h>
#include <mach/scm.h>
#include "msm_watchdog.h"

#define TCSR_WDT_CFG 0x30

#define WDT0_RST	(MSM_TMR0_BASE + 0x38)
#define WDT0_EN		(MSM_TMR0_BASE + 0x40)
#define WDT0_BARK_TIME	(MSM_TMR0_BASE + 0x4C)
#define WDT0_BITE_TIME	(MSM_TMR0_BASE + 0x5C)

/* Watchdog pet interval in ms */
#define PET_DELAY 3000
static unsigned long delay_time;
static unsigned long long last_pet;

/*
 * On the kernel command line specify
 * msm_watchdog.enable=1 to enable the watchdog
 * By default watchdog is turned on
 */
static int enable = 1;
module_param(enable, int, 0);

/*
 * If the watchdog is enabled at bootup (enable=1),
 * the runtime_disable sysfs node at
 * /sys/module/msm_watchdog/runtime_disable
 * can be used to deactivate the watchdog.
 * This is a one-time setting. The watchdog
 * cannot be re-enabled once it is disabled.
 */
static int runtime_disable;
static DEFINE_MUTEX(disable_lock);
static int wdog_enable_set(const char *val, struct kernel_param *kp);
module_param_call(runtime_disable, wdog_enable_set, param_get_int,
			&runtime_disable, 0644);

/*
 * On the kernel command line specify msm_watchdog.appsbark=1 to handle
 * watchdog barks in Linux. By default barks are processed by the secure side.
 */
static int appsbark;
module_param(appsbark, int, 0);

/*
 * Use /sys/module/msm_watchdog/parameters/print_all_stacks
 * to control whether stacks of all running
 * processes are printed when a wdog bark is received.
 */
static int print_all_stacks = 1;
module_param(print_all_stacks, int,  S_IRUGO | S_IWUSR);

/* Area for context dump in secure mode */
static void *scm_regsave;

static void pet_watchdog_work(struct work_struct *work);
static void init_watchdog_work(struct work_struct *work);
static DECLARE_DELAYED_WORK(dogwork_struct, pet_watchdog_work);
static DECLARE_WORK(init_dogwork_struct, init_watchdog_work);

static int msm_watchdog_suspend(void)
{
	__raw_writel(1, WDT0_RST);
	__raw_writel(0, WDT0_EN);
	mb();
	return NOTIFY_DONE;
}
static int msm_watchdog_resume(void)
{
	__raw_writel(1, WDT0_EN);
	__raw_writel(1, WDT0_RST);
	return NOTIFY_DONE;
}

static int msm_watchdog_power_event(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	switch (event) {
	case PM_POST_HIBERNATION:
	case PM_POST_SUSPEND:
		return msm_watchdog_resume();
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
		return msm_watchdog_suspend();
	default:
		return NOTIFY_DONE;
	}
}

static int panic_wdog_handler(struct notifier_block *this,
			      unsigned long event, void *ptr)
{
	if (panic_timeout == 0) {
		__raw_writel(0, WDT0_EN);
		mb();
		secure_writel(0, MSM_TCSR_BASE + TCSR_WDT_CFG);
	} else {
		__raw_writel(32768 * (panic_timeout + 4), WDT0_BARK_TIME);
		__raw_writel(32768 * (panic_timeout + 4), WDT0_BITE_TIME);
		__raw_writel(1, WDT0_RST);
	}
	return NOTIFY_DONE;
}

static struct notifier_block panic_blk = {
	.notifier_call	= panic_wdog_handler,
};

static struct notifier_block msm_watchdog_power_notifier = {
	.notifier_call = msm_watchdog_power_event,
};

static int wdog_enable_set(const char *val, struct kernel_param *kp)
{
	int ret = 0;
	int old_val = runtime_disable;

	mutex_lock(&disable_lock);

	if (!enable) {
		printk(KERN_INFO "MSM Watchdog is not active.\n");
		ret = -EINVAL;
		goto done;
	}

	ret = param_set_int(val, kp);

	if (ret)
		goto done;

	switch (runtime_disable) {

	case 1:
		if (!old_val) {
			__raw_writel(0, WDT0_EN);
			unregister_pm_notifier(&msm_watchdog_power_notifier);

			/* may be suspended after the first write above */
			__raw_writel(0, WDT0_EN);
			mb();
			secure_writel(0, MSM_TCSR_BASE + TCSR_WDT_CFG);
			free_irq(WDT0_ACCSCSSNBARK_INT, 0);
			enable = 0;
			atomic_notifier_chain_unregister(&panic_notifier_list,
			       &panic_blk);
			cancel_delayed_work(&dogwork_struct);
			printk(KERN_INFO "MSM Watchdog deactivated.\n");
		}
	break;

	default:
		runtime_disable = old_val;
		ret = -EINVAL;
	break;

	}

done:
	mutex_unlock(&disable_lock);
	return ret;
}

void pet_watchdog(void)
{
	__raw_writel(1, WDT0_RST);
	last_pet = sched_clock();
}

static void pet_watchdog_work(struct work_struct *work)
{
	pet_watchdog();

	if (enable)
		schedule_delayed_work_on(0, &dogwork_struct, delay_time);
}

static void __exit exit_watchdog(void)
{
	if (enable) {
		__raw_writel(0, WDT0_EN);
		unregister_pm_notifier(&msm_watchdog_power_notifier);
		__raw_writel(0, WDT0_EN); /* In case we got suspended
					   * mid-exit */
		mb();
		secure_writel(0, MSM_TCSR_BASE + TCSR_WDT_CFG);
		free_irq(WDT0_ACCSCSSNBARK_INT, 0);
		enable = 0;
	}
	printk(KERN_INFO "MSM Watchdog Exit - Deactivated\n");
}

static irqreturn_t wdog_bark_handler(int irq, void *dev_id)
{
	unsigned long nanosec_rem;
	unsigned long long t = sched_clock();
	struct task_struct *tsk;

	nanosec_rem = do_div(t, 1000000000);
	printk(KERN_INFO "Watchdog bark! Now = %lu.%06lu\n", (unsigned long) t,
		nanosec_rem / 1000);

	nanosec_rem = do_div(last_pet, 1000000000);
	printk(KERN_INFO "Watchdog last pet at %lu.%06lu\n", (unsigned long)
		last_pet, nanosec_rem / 1000);

	if (print_all_stacks) {

		/* Suspend wdog until all stacks are printed */
		msm_watchdog_suspend();

		printk(KERN_INFO "Stack trace dump:\n");

		for_each_process(tsk) {
			printk(KERN_INFO "\nPID: %d, Name: %s\n",
				tsk->pid, tsk->comm);
			show_stack(tsk, NULL);
		}

		msm_watchdog_resume();
	}

	panic("Apps watchdog bark received!");
	return IRQ_HANDLED;
}

#define SCM_SET_REGSAVE_CMD 0x2

static void init_watchdog_work(struct work_struct *work)
{
	int ret;
	struct {
		unsigned addr;
		int len;
	} cmd_buf;

	if (!enable) {
		printk(KERN_INFO "MSM Watchdog Not Initialized\n");
		return;
	}

	/* Must request irq before sending scm command */
	ret = request_irq(WDT0_ACCSCSSNBARK_INT, wdog_bark_handler, 0,
			  "apps_wdog_bark", NULL);
	if (ret)
		return;

#ifdef CONFIG_MSM_SCM
	if (!appsbark) {
		scm_regsave = (void *)__get_free_page(GFP_KERNEL);

		if (scm_regsave) {
			cmd_buf.addr = __pa(scm_regsave);
			cmd_buf.len  = PAGE_SIZE;

			ret = scm_call(SCM_SVC_UTIL, SCM_SET_REGSAVE_CMD,
				       &cmd_buf, sizeof(cmd_buf), NULL, 0);
			if (ret)
				pr_err("Setting register save address failed.\n"
				       "Registers won't be dumped on a dog "
				       "bite\n");
		} else
			pr_err("Allocating register save space failed\n"
			       "Registers won't be dumped on a dog bite\n");
			/*
			 * No need to bail if allocation fails. Simply don't
			 * send the command, and the secure side will reset
			 * without saving registers.
			 */
	}
#endif
	secure_writel(1, MSM_TCSR_BASE + TCSR_WDT_CFG);
	delay_time = msecs_to_jiffies(PET_DELAY);

	/* 32768 ticks = 1 second */
	if (machine_is_msm8960_sim()) {
		__raw_writel(32768*8, WDT0_BARK_TIME);
		__raw_writel(32768*10, WDT0_BITE_TIME);
	} else {
		__raw_writel(32768*4, WDT0_BARK_TIME);
		__raw_writel(32768*5, WDT0_BITE_TIME);
	}

	ret = register_pm_notifier(&msm_watchdog_power_notifier);
	if (ret) {
		free_irq(WDT0_ACCSCSSNBARK_INT, NULL);
		return;
	}

	schedule_delayed_work_on(0, &dogwork_struct, delay_time);

	atomic_notifier_chain_register(&panic_notifier_list,
				       &panic_blk);

	__raw_writel(1, WDT0_EN);
	__raw_writel(1, WDT0_RST);
	last_pet = sched_clock();

	printk(KERN_INFO "MSM Watchdog Initialized\n");

	return;
}

static int __init init_watchdog(void)
{
	schedule_work_on(0, &init_dogwork_struct);
	return 0;
}

late_initcall(init_watchdog);
module_exit(exit_watchdog);
MODULE_DESCRIPTION("MSM Watchdog Driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
