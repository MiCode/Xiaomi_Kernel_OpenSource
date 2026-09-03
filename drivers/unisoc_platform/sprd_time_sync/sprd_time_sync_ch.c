// SPDX-License-Identifier: GPL-2.0
/*
 * AP/CH time synchronization for Spreadtrum SoCs
 *
 * Copyright (C) 2021 Spreadtrum corporation. http://www.unisoc.com
 *
 * Author: Ruifeng Zhang <Ruifeng.Zhang1@unisoc.com>
 */

#include <clocksource/arm_arch_timer.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/sipc.h>
#include <linux/soc/sprd/sprd_systimer.h>
#include <linux/spinlock.h>
#include <linux/syscore_ops.h>
#include <linux/soc/sprd/sprd_time_sync.h>
#include <uapi/linux/sched/types.h>

#define SENSOR_TS_CTRL_NUM	4
#define SENSOR_TS_CTRL_SIZE	4
#define SENSOR_TS_CTRL_SEL	(1 << 8)

#undef pr_fmt
#define pr_fmt(fmt) "sprd_time_sync_ch: " fmt

#define DEFAULT_TIMEVALE_MS (1000 * 60 * 10) //10min

static void __iomem *sprd_time_sync_ch_addr_base;

static struct hrtimer sprd_time_sync_ch_timer;

static struct task_struct *sprd_time_sync_task;

static struct sprd_cnt_to_boot{
	u64 ts_cnt;
	u64 sysfrt_cnt;
	u64 boottime;
} sprd_ctb;

static DEFINE_SPINLOCK(sprd_time_sync_ch_lock);

static struct ch_cfg_bus_context {
	unsigned int ctrl_reg;
	unsigned int ctrl_mask;
	struct regmap *regmap;
} ch_cfg_bus;

#if IS_ENABLED(CONFIG_ARM)
static inline u64 sprd_arch_counter_get_cntpct(void)
{
	u64 cval;

	isb();
	asm volatile("mrrc p15, 0, %Q0, %R0, c14" : "=r" (cval));
	return cval;
}
#else
static inline u64 sprd_arch_counter_get_cntpct(void)
{
	u64 cval;

	isb();
	asm volatile("mrs %0, cntpct_el0" : "=r" (cval));
	return cval;
}
#endif

static inline void update_cnt_to_boot(void)
{
	sprd_ctb.ts_cnt = sprd_arch_counter_get_cntpct();
	sprd_ctb.sysfrt_cnt = sprd_sysfrt_read();
	sprd_ctb.boottime = ktime_get_boot_fast_ns();
}

/* send only once to ensure resume time */
static int sprd_time_sync_ch_send_thread(void *data)
{
	struct sched_param param = {.sched_priority = 50};

	/* set the thread as a real time thread, and its priority is 50 */
	sched_setscheduler(sprd_time_sync_task, SCHED_RR, &param);

	while (!kthread_should_stop()) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule();

		sprd_send_ap_time();

#if IS_ENABLED(CONFIG_SPRD_DEBUG)
		pr_info("send time to ch. ts_cnt = %llu sysfrt_cnt = %lu boottime = %llu\n",
			 sprd_ctb.ts_cnt,
			 sprd_ctb.sysfrt_cnt,
			 sprd_ctb.boottime);
#endif
	}

	return 0;
}

void sprd_time_sync_ch_resume(void)
{
	u32 time_sync_ch_ctrl;
	unsigned long flags;
	int i;

	if (!sprd_time_sync_ch_addr_base || !ch_cfg_bus.regmap)
		return;

	spin_lock_irqsave(&sprd_time_sync_ch_lock, flags);
	/* set REG_AON_APB_CH_CFG_BUS bit[0] to 0 */
	regmap_update_bits(ch_cfg_bus.regmap, ch_cfg_bus.ctrl_reg,
			   ch_cfg_bus.ctrl_mask, 0);

	/* set 0 since ap active */
	for (i = 0; i < SENSOR_TS_CTRL_NUM; i++) {
		time_sync_ch_ctrl = readl_relaxed(sprd_time_sync_ch_addr_base + i * SENSOR_TS_CTRL_SIZE);
		time_sync_ch_ctrl &= ~SENSOR_TS_CTRL_SEL;
		writel_relaxed(time_sync_ch_ctrl, sprd_time_sync_ch_addr_base + i * SENSOR_TS_CTRL_SIZE);
	}

	/* set REG_AON_APB_CH_CFG_BUS bit[0] to 1 */
	regmap_update_bits(ch_cfg_bus.regmap, ch_cfg_bus.ctrl_reg,
			   ch_cfg_bus.ctrl_mask, 1);

	update_cnt_to_boot();
	spin_unlock_irqrestore(&sprd_time_sync_ch_lock, flags);

	wake_up_process(sprd_time_sync_task);
}

int sprd_time_sync_ch_suspend(void)
{
	u32 time_sync_ch_ctrl;
	unsigned long flags;
	int i;

	if (!sprd_time_sync_ch_addr_base || !ch_cfg_bus.regmap)
		return 0;

	spin_lock_irqsave(&sprd_time_sync_ch_lock, flags);
	/* set REG_AON_APB_CH_CFG_BUS bit[0] to 0 */
	regmap_update_bits(ch_cfg_bus.regmap, ch_cfg_bus.ctrl_reg,
			   ch_cfg_bus.ctrl_mask, 0);

	/* set 1 since AP suspend */
	for (i = 0; i < SENSOR_TS_CTRL_NUM; i++) {
		time_sync_ch_ctrl = readl_relaxed(sprd_time_sync_ch_addr_base + i * SENSOR_TS_CTRL_SIZE);
		time_sync_ch_ctrl |= SENSOR_TS_CTRL_SEL;
		writel_relaxed(time_sync_ch_ctrl, sprd_time_sync_ch_addr_base + i * SENSOR_TS_CTRL_SIZE);
	}

	/* set REG_AON_APB_CH_CFG_BUS bit[0] to 1 */
	regmap_update_bits(ch_cfg_bus.regmap, ch_cfg_bus.ctrl_reg,
			   ch_cfg_bus.ctrl_mask, 1);
	spin_unlock_irqrestore(&sprd_time_sync_ch_lock, flags);

	return 0;
}

/* sysfs resume/suspend bits for time_sync_ch */
static struct syscore_ops sprd_time_sync_ch_syscore_ops = {
	.resume		= sprd_time_sync_ch_resume,
	.suspend	= sprd_time_sync_ch_suspend,
};

static enum hrtimer_restart sprd_sync_timer_ch(struct hrtimer *hr)
{
	unsigned long flags;

	spin_lock_irqsave(&sprd_time_sync_ch_lock, flags);
	update_cnt_to_boot();
	spin_unlock_irqrestore(&sprd_time_sync_ch_lock, flags);

	wake_up_process(sprd_time_sync_task);

	hrtimer_forward_now(&sprd_time_sync_ch_timer, ms_to_ktime(DEFAULT_TIMEVALE_MS));

	return HRTIMER_RESTART;
}

static int sprd_time_sync_ch_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	unsigned int syscon_args[2];
	unsigned long flags;
	struct regmap *regmap;

	sprd_time_sync_ch_addr_base = of_iomap(np, 0);
	if (!sprd_time_sync_ch_addr_base) {
		pr_err("Can't map sprd time sync ch addr.\n");
		return -EFAULT;
	}

	/*ch_cfg_bus.regmap = syscon_regmap_lookup_by_name(np,"ch_cfg_bus");*/
	/*ch_cfg_bus.regmap = syscon_regmap_lookup_by_phandle(np, "ch_cfg_bus");
	if (IS_ERR(ch_cfg_bus.regmap)) {
		pr_err("failed to map ch_cfg_bus reg.\n");
		return PTR_ERR(ch_cfg_bus.regmap);
	}*/

	/*ret = syscon_get_args_by_name(np, "ch_cfg_bus", 2, syscon_args);*/
	regmap = syscon_regmap_lookup_by_phandle_args(np, "ch-syscons", 2, syscon_args);
	if (IS_ERR(regmap)) {
		pr_err("failed to parse ch_cfg_bus reg.\n");
		return -EFAULT;
	} else {
		ch_cfg_bus.ctrl_reg = syscon_args[0];
		ch_cfg_bus.ctrl_mask = syscon_args[1];
	}

	register_syscore_ops(&sprd_time_sync_ch_syscore_ops);

	/* update/send sprd_ctb value first */
	spin_lock_irqsave(&sprd_time_sync_ch_lock, flags);
	update_cnt_to_boot();
	spin_unlock_irqrestore(&sprd_time_sync_ch_lock, flags);

	sprd_time_sync_task = kthread_create(sprd_time_sync_ch_send_thread,
					     NULL,
					     pdev->name);
	if (IS_ERR(sprd_time_sync_task)) {
		pr_err("Create %s thread error.\n", pdev->name);
		return PTR_ERR(sprd_time_sync_task);
	}

	wake_up_process(sprd_time_sync_task);

	hrtimer_init(&sprd_time_sync_ch_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	sprd_time_sync_ch_timer.function = sprd_sync_timer_ch;
	hrtimer_start(&sprd_time_sync_ch_timer, ms_to_ktime(DEFAULT_TIMEVALE_MS), HRTIMER_MODE_REL);

	pr_info("probe done.\n");

	return 0;
}

static int sprd_time_sync_ch_remove(struct platform_device *pdev)
{
	sprd_time_sync_ch_addr_base = NULL;

	return 0;
}

static const struct of_device_id sprd_time_sync_ch_ids[] = {
	{ .compatible = "sprd,time-sync-ch", },
	{},
};

static struct platform_driver sprd_time_sync_ch_driver = {
	.probe = sprd_time_sync_ch_probe,
	.remove = sprd_time_sync_ch_remove,
	.driver = {
		.name = "sprd_time_sync_ch",
		.of_match_table = sprd_time_sync_ch_ids,
	},
};

module_platform_driver(sprd_time_sync_ch_driver);

MODULE_AUTHOR("Ruifeng Zhang");
MODULE_DESCRIPTION("sprd time sync ch driver");
MODULE_LICENSE("GPL v2");
