/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
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
#include <linux/percpu.h>
#include <linux/interrupt.h>
#include <linux/reboot.h>
#include <asm/fiq.h>
#include <asm/hardware/gic.h>
#include <mach/msm_iomap.h>
#include <asm/mach-types.h>
#include <asm/cacheflush.h>
#include <mach/scm.h>
#include <mach/socinfo.h>
#include "msm_watchdog.h"
#include "timer.h"

#define MODULE_NAME "msm_watchdog"

#define TCSR_WDT_CFG	0x30

#define WDT_RST		0x0
#define WDT_EN		0x8
#define WDT_STS		0xC
#define WDT_BARK_TIME	0x14
#define WDT_BITE_TIME	0x24

#define WDT_HZ		32768

struct msm_watchdog_dump msm_dump_cpu_ctx;

static void __iomem *msm_wdt_base;

static unsigned long delay_time;
static unsigned long bark_time;
static unsigned long long last_pet;
static bool has_vic;
static unsigned int msm_wdog_irq;

/*
 * On the kernel command line specify
 * msm_watchdog.enable=1 to enable the watchdog
 * By default watchdog is turned on
 */
static int enable = 1;
module_param(enable, int, 0);

/*
 * Watchdog bark reboot timeout in seconds.
 * Can be specified in kernel command line.
 */
static int reboot_bark_timeout = 22;
module_param(reboot_bark_timeout, int, 0644);
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

static int appsbark_fiq;

/*
 * Use /sys/module/msm_watchdog/parameters/print_all_stacks
 * to control whether stacks of all running
 * processes are printed when a wdog bark is received.
 */
static int print_all_stacks = 1;
module_param(print_all_stacks, int,  S_IRUGO | S_IWUSR);

/* Area for context dump in secure mode */
static void *scm_regsave;

static struct msm_watchdog_pdata __percpu **percpu_pdata;

static void pet_watchdog_work(struct work_struct *work);
static void init_watchdog_work(struct work_struct *work);
static DECLARE_DELAYED_WORK(dogwork_struct, pet_watchdog_work);
static DECLARE_WORK(init_dogwork_struct, init_watchdog_work);

/* Called from the FIQ bark handler */
void msm_wdog_bark_fin(void)
{
	flush_cache_all();
	pr_crit("\nApps Watchdog bark received - Calling Panic\n");
	panic("Apps Watchdog Bark received\n");
}

static int msm_watchdog_suspend(struct device *dev)
{
	if (!enable)
		return 0;

	__raw_writel(1, msm_wdt_base + WDT_RST);
	__raw_writel(0, msm_wdt_base + WDT_EN);
	mb();
	return 0;
}

static int msm_watchdog_resume(struct device *dev)
{
	if (!enable)
		return 0;

	__raw_writel(1, msm_wdt_base + WDT_EN);
	__raw_writel(1, msm_wdt_base + WDT_RST);
	mb();
	return 0;
}

static int panic_wdog_handler(struct notifier_block *this,
			      unsigned long event, void *ptr)
{
	if (panic_timeout == 0) {
		__raw_writel(0, msm_wdt_base + WDT_EN);
		mb();
	} else {
		__raw_writel(WDT_HZ * (panic_timeout + 4),
				msm_wdt_base + WDT_BARK_TIME);
		__raw_writel(WDT_HZ * (panic_timeout + 4),
				msm_wdt_base + WDT_BITE_TIME);
		__raw_writel(1, msm_wdt_base + WDT_RST);
	}
	return NOTIFY_DONE;
}

static struct notifier_block panic_blk = {
	.notifier_call	= panic_wdog_handler,
};

#define get_sclk_hz(t_ms) ((t_ms / 1000) * WDT_HZ)
#define get_reboot_bark_timeout(t_s) ((t_s * MSEC_PER_SEC) < bark_time ? \
		get_sclk_hz(bark_time) : get_sclk_hz(t_s * MSEC_PER_SEC))

static int msm_watchdog_reboot_notifier(struct notifier_block *this,
		unsigned long code, void *unused)
{

	u64 timeout = get_reboot_bark_timeout(reboot_bark_timeout);
	__raw_writel(timeout, msm_wdt_base + WDT_BARK_TIME);
	__raw_writel(timeout + 3 * WDT_HZ,
			msm_wdt_base + WDT_BITE_TIME);
	__raw_writel(1, msm_wdt_base + WDT_RST);

	return NOTIFY_DONE;
}

static struct notifier_block msm_reboot_notifier = {
	.notifier_call = msm_watchdog_reboot_notifier,
};

struct wdog_disable_work_data {
	struct work_struct work;
	struct completion complete;
};

static void wdog_disable_work(struct work_struct *work)
{
	struct wdog_disable_work_data *work_data =
		container_of(work, struct wdog_disable_work_data, work);
	__raw_writel(0, msm_wdt_base + WDT_EN);
	mb();
	if (has_vic) {
		free_irq(msm_wdog_irq, 0);
	} else {
		disable_percpu_irq(msm_wdog_irq);
		if (!appsbark_fiq) {
			free_percpu_irq(msm_wdog_irq,
					percpu_pdata);
			free_percpu(percpu_pdata);
		}
	}
	enable = 0;
	atomic_notifier_chain_unregister(&panic_notifier_list, &panic_blk);
	unregister_reboot_notifier(&msm_reboot_notifier);
	cancel_delayed_work(&dogwork_struct);
	/* may be suspended after the first write above */
	__raw_writel(0, msm_wdt_base + WDT_EN);
	complete(&work_data->complete);
	pr_info("MSM Watchdog deactivated.\n");
}

static int wdog_enable_set(const char *val, struct kernel_param *kp)
{
	int ret = 0;
	int old_val = runtime_disable;
	struct wdog_disable_work_data work_data;

	mutex_lock(&disable_lock);
	if (!enable) {
		printk(KERN_INFO "MSM Watchdog is not active.\n");
		ret = -EINVAL;
		goto done;
	}

	ret = param_set_int(val, kp);
	if (ret)
		goto done;

	if (runtime_disable == 1) {
		if (old_val)
			goto done;
		init_completion(&work_data.complete);
		INIT_WORK_ONSTACK(&work_data.work, wdog_disable_work);
		schedule_work_on(0, &work_data.work);
		wait_for_completion(&work_data.complete);
	} else {
		runtime_disable = old_val;
		ret = -EINVAL;
	}
done:
	mutex_unlock(&disable_lock);
	return ret;
}

unsigned min_slack_ticks = UINT_MAX;
unsigned long long min_slack_ns = ULLONG_MAX;

void pet_watchdog(void)
{
	int slack;
	unsigned long long time_ns;
	unsigned long long slack_ns;
	unsigned long long bark_time_ns = bark_time * 1000000ULL;

	if (!enable)
		return;

	slack = __raw_readl(msm_wdt_base + WDT_STS) >> 3;
	slack = ((bark_time*WDT_HZ)/1000) - slack;
	if (slack < min_slack_ticks)
		min_slack_ticks = slack;
	__raw_writel(1, msm_wdt_base + WDT_RST);
	time_ns = sched_clock();
	slack_ns = (last_pet + bark_time_ns) - time_ns;
	if (slack_ns < min_slack_ns)
		min_slack_ns = slack_ns;
	last_pet = time_ns;
}

static void pet_watchdog_work(struct work_struct *work)
{
	pet_watchdog();

	if (enable)
		schedule_delayed_work_on(0, &dogwork_struct, delay_time);
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
		msm_watchdog_suspend(NULL);

		printk(KERN_INFO "Stack trace dump:\n");

		for_each_process(tsk) {
			printk(KERN_INFO "\nPID: %d, Name: %s\n",
				tsk->pid, tsk->comm);
			show_stack(tsk, NULL);
		}

		msm_watchdog_resume(NULL);
	}

	panic("Apps watchdog bark received!");
	return IRQ_HANDLED;
}

#define SCM_SET_REGSAVE_CMD 0x2

static void configure_bark_dump(void)
{
	int ret;
	struct {
		unsigned addr;
		int len;
	} cmd_buf;

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
		} else {
			pr_err("Allocating register save space failed\n"
			       "Registers won't be dumped on a dog bite\n");
			/*
			 * No need to bail if allocation fails. Simply don't
			 * send the command, and the secure side will reset
			 * without saving registers.
			 */
		}
	}
}

struct fiq_handler wdog_fh = {
	.name = MODULE_NAME,
};

static void init_watchdog_work(struct work_struct *work)
{
	u64 timeout = (bark_time * WDT_HZ)/1000;
	void *stack;
	int ret;

	if (has_vic) {
		ret = request_irq(msm_wdog_irq, wdog_bark_handler, 0,
				  "apps_wdog_bark", NULL);
		if (ret)
			return;
	} else if (appsbark_fiq) {
		claim_fiq(&wdog_fh);
		set_fiq_handler(&msm_wdog_fiq_start, msm_wdog_fiq_length);
		stack = (void *)__get_free_pages(GFP_KERNEL, THREAD_SIZE_ORDER);
		if (!stack) {
			pr_info("No free pages available - %s fails\n",
					__func__);
			return;
		}

		msm_wdog_fiq_setup(stack);
		gic_set_irq_secure(msm_wdog_irq);
	} else {
		percpu_pdata = alloc_percpu(struct msm_watchdog_pdata *);
		if (!percpu_pdata) {
			pr_err("%s: memory allocation failed for percpu data\n",
					__func__);
			return;
		}

		/* Must request irq before sending scm command */
		ret = request_percpu_irq(msm_wdog_irq,
			wdog_bark_handler, "apps_wdog_bark", percpu_pdata);
		if (ret) {
			free_percpu(percpu_pdata);
			return;
		}
	}

	configure_bark_dump();

	__raw_writel(timeout, msm_wdt_base + WDT_BARK_TIME);
	__raw_writel(timeout + 3*WDT_HZ, msm_wdt_base + WDT_BITE_TIME);

	schedule_delayed_work_on(0, &dogwork_struct, delay_time);

	atomic_notifier_chain_register(&panic_notifier_list,
				       &panic_blk);

	ret = register_reboot_notifier(&msm_reboot_notifier);
	if (ret)
		pr_err("Failed to register reboot notifier\n");

	__raw_writel(1, msm_wdt_base + WDT_EN);
	__raw_writel(1, msm_wdt_base + WDT_RST);
	last_pet = sched_clock();

	if (!has_vic)
		enable_percpu_irq(msm_wdog_irq, IRQ_TYPE_EDGE_RISING);

	printk(KERN_INFO "MSM Watchdog Initialized\n");

	return;
}

static int msm_watchdog_probe(struct platform_device *pdev)
{
	struct msm_watchdog_pdata *pdata = pdev->dev.platform_data;

	if (!enable || !pdata || !pdata->pet_time || !pdata->bark_time) {
		printk(KERN_INFO "MSM Watchdog Not Initialized\n");
		return -ENODEV;
	}

	bark_time = pdata->bark_time;
	/* reboot_bark_timeout (in seconds) might have been supplied as
	 * module parameter.
	 */
	if ((reboot_bark_timeout * MSEC_PER_SEC) < bark_time)
		reboot_bark_timeout = (bark_time / MSEC_PER_SEC);
	has_vic = pdata->has_vic;
	if (!pdata->has_secure) {
		appsbark = 1;
		appsbark_fiq = pdata->use_kernel_fiq;
	}

	msm_wdt_base = pdata->base;
	msm_wdog_irq = platform_get_irq(pdev, 0);

	/*
	 * This is only temporary till SBLs turn on the XPUs
	 * This initialization will be done in SBLs on a later releases
	 */
	if (cpu_is_msm9615())
		__raw_writel(0xF, MSM_TCSR_BASE + TCSR_WDT_CFG);

	if (pdata->needs_expired_enable)
		__raw_writel(0x1, MSM_CLK_CTL_BASE + 0x3820);

	delay_time = msecs_to_jiffies(pdata->pet_time);
	schedule_work_on(0, &init_dogwork_struct);
	return 0;
}

static const struct dev_pm_ops msm_watchdog_dev_pm_ops = {
	.suspend_noirq = msm_watchdog_suspend,
	.resume_noirq = msm_watchdog_resume,
};

static struct platform_driver msm_watchdog_driver = {
	.probe = msm_watchdog_probe,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.pm = &msm_watchdog_dev_pm_ops,
	},
};

static int init_watchdog(void)
{
	return platform_driver_register(&msm_watchdog_driver);
}

late_initcall(init_watchdog);
MODULE_DESCRIPTION("MSM Watchdog Driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
