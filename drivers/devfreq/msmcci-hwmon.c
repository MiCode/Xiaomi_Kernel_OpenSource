/*
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "msmcci-hwmon: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/spinlock.h>
#include <linux/cpu_pm.h>
#include <soc/qcom/scm.h>
#include "governor_cache_hwmon.h"

#define	EVNT_SEL		 0x0
#define	EVNT_CNT_MATCH_VAL	 0x18
#define	MATCH_FLG		 0x30
#define	MATCH_FLG_CLR		 0x48
#define	OVR_FLG			 0x60
#define	OVR_FLG_CLR		 0x78
#define	CNT_CTRL		 0x94
#define	CNT_VALUE		 0xAC

#define ENABLE_OVR_FLG		BIT(4)
#define ENABLE_MATCH_FLG	BIT(5)
#define ENABLE_EVNT_CNT		BIT(0)
#define RESET_EVNT_CNT		BIT(1)

#define CNT_DISABLE	(ENABLE_OVR_FLG | ENABLE_MATCH_FLG)
#define CNT_RESET_CLR	(ENABLE_OVR_FLG | ENABLE_MATCH_FLG)
#define CNT_ENABLE	(ENABLE_OVR_FLG | ENABLE_MATCH_FLG | ENABLE_EVNT_CNT)
#define CNT_RESET	(ENABLE_OVR_FLG | ENABLE_MATCH_FLG | RESET_EVNT_CNT)

struct msmcci_hwmon {
	struct list_head list;

	union {
		phys_addr_t phys_base[MAX_NUM_GROUPS];
		void __iomem *virt_base[MAX_NUM_GROUPS];
	};
	int irq[MAX_NUM_GROUPS];
	u32 event_sel[MAX_NUM_GROUPS];
	int num_counters;

	/*
	 * Multiple interrupts might fire together for one device.
	 * In that case, only one re-evaluation needs to be done.
	 */
	struct mutex update_lock;

	/* For counter state save and restore */
	unsigned long cur_limit[MAX_NUM_GROUPS];
	unsigned long cur_count[MAX_NUM_GROUPS];
	bool mon_enabled;

	struct cache_hwmon hw;
	struct device *dev;
	bool secure_io;
	bool irq_shared;
};

#define to_mon(ptr) container_of(ptr, struct msmcci_hwmon, hw)

static LIST_HEAD(msmcci_hwmon_list);
static DEFINE_MUTEX(list_lock);

static int use_cnt;
static DEFINE_MUTEX(notifier_reg_lock);

static inline int write_mon_reg(struct msmcci_hwmon *m, int idx,
				unsigned long offset, u32 value)
{
	int ret = 0;

	if (m->secure_io)
		ret = scm_io_write(m->phys_base[idx] + offset, value);
	else
		writel_relaxed(value, m->virt_base[idx] + offset);

	return ret;
}

static inline u32 read_mon_reg(struct msmcci_hwmon *m, int idx,
			       unsigned long offset)
{
	if (m->secure_io)
		return scm_io_read(m->phys_base[idx] + offset);
	else
		return readl_relaxed(m->virt_base[idx] + offset);
}

static int mon_init(struct msmcci_hwmon *m)
{
	int ret, i;

	for (i = 0; i < m->num_counters; i++) {
		ret = write_mon_reg(m, i, EVNT_SEL, m->event_sel[i]);
		if (ret)
			return ret;
	}
	return 0;
}

static void mon_enable(struct msmcci_hwmon *m)
{
	int i;

	for (i = 0; i < m->num_counters; i++)
		write_mon_reg(m, i, CNT_CTRL, CNT_ENABLE);
}

static void mon_disable(struct msmcci_hwmon *m)
{
	int i;

	for (i = 0; i < m->num_counters; i++)
		write_mon_reg(m, i, CNT_CTRL, CNT_DISABLE);
}

static bool mon_is_match_flag_set(struct msmcci_hwmon *m, int idx)
{
	return (bool)read_mon_reg(m, idx, MATCH_FLG);
}

/* mon_clear_single() can only be called when monitor is disabled */
static void mon_clear_single(struct msmcci_hwmon *m, int idx)
{
	write_mon_reg(m, idx, CNT_CTRL, CNT_RESET);
	write_mon_reg(m, idx, CNT_CTRL, CNT_RESET_CLR);
	/* reset counter before match/overflow flags are cleared */
	mb();
	write_mon_reg(m, idx, MATCH_FLG_CLR, 1);
	write_mon_reg(m, idx, MATCH_FLG_CLR, 0);
	write_mon_reg(m, idx, OVR_FLG_CLR, 1);
	write_mon_reg(m, idx, OVR_FLG_CLR, 0);
}

static void mon_set_limit_single(struct msmcci_hwmon *m, int idx, u32 limit)
{
	write_mon_reg(m, idx, EVNT_CNT_MATCH_VAL, limit);
}

static irqreturn_t msmcci_hwmon_shared_intr_handler(int irq, void *dev)
{
	struct msmcci_hwmon *m = dev;
	int idx = -1, i;

	for (i = 0; i < m->num_counters; i++) {
		if (mon_is_match_flag_set(m, i)) {
			idx = i;
			break;
		}
	}
	if (idx == -1)
		return IRQ_NONE;

	update_cache_hwmon(&m->hw);

	return IRQ_HANDLED;
}

static irqreturn_t msmcci_hwmon_intr_handler(int irq, void *dev)
{
	struct msmcci_hwmon *m = dev;
	int idx = -1, i;

	for (i = 0; i < m->num_counters; i++) {
		if (m->irq[i] == irq) {
			idx = i;
			break;
		}
	}

	if (idx == -1) {
		WARN(true, "idx == -1!\n");
		return IRQ_NONE;
	}

	/*
	 * Multiple independent interrupts could fire together and trigger
	 * update_cache_hwmon() for same device. If we don't lock, we
	 * could end up calling devfreq_monitor_start/stop()
	 * concurrently, which would cause timer/workqueue object
	 * corruption. However, we can't re-evaluate a few times back to
	 * back either because the very short window won't be
	 * representative. Since update_cache_hwmon() will clear match
	 * flags for all counters, interrupts for other counters can
	 * simply return if their match flags have already been cleared.
	 */
	mutex_lock(&m->update_lock);
	if (mon_is_match_flag_set(m, idx))
		update_cache_hwmon(&m->hw);
	mutex_unlock(&m->update_lock);
	return IRQ_HANDLED;
}

static unsigned long mon_read_count_single(struct msmcci_hwmon *m, int idx)
{
	unsigned long count, ovr;

	count = read_mon_reg(m, idx, CNT_VALUE);
	ovr = read_mon_reg(m, idx, OVR_FLG);
	if (ovr == 1) {
		count += 0xFFFFFFFFUL;
		dev_warn(m->dev, "Counter[%d]: overflowed\n", idx);
	}
	return count;
}

static int count_to_mrps(unsigned long count, unsigned int us)
{
	do_div(count, us);
	count++;
	return count;
}

static unsigned int mrps_to_count(unsigned int mrps, unsigned int ms,
				  unsigned int tolerance)
{
	mrps += tolerance;
	mrps *= ms * USEC_PER_MSEC;
	return mrps;
}

static unsigned long meas_mrps_and_set_irq(struct cache_hwmon *hw,
		unsigned int tol, unsigned int us, struct mrps_stats *mrps)
{
	struct msmcci_hwmon *m = to_mon(hw);
	unsigned long count;
	unsigned int sample_ms = hw->df->profile->polling_ms;
	int i;
	u32 limit;

	mon_disable(m);

	/* calculate mrps and set limit */
	for (i = 0; i < m->num_counters; i++) {
		count = mon_read_count_single(m, i);
		/*
		 * When CCI is power collapsed, counters are cleared. Add
		 * saved count to the current reading and clear saved count
		 * to ensure we won't apply it more than once.
		 */
		count += m->cur_count[i];
		m->cur_count[i] = 0;

		mrps->mrps[i] = count_to_mrps(count, us);
		limit = mrps_to_count(mrps->mrps[i], sample_ms, tol);

		mon_clear_single(m, i);
		mon_set_limit_single(m, i, limit);
		/* save current limit for restoring after power collapse */
		m->cur_limit[i] = limit;

		dev_dbg(m->dev, "Counter[%d] count 0x%lx, limit 0x%x\n",
			i, count, limit);
	}

	/*
	 * There is no cycle counter for this device.
	 * Treat all cycles as busy.
	 */
	mrps->busy_percent = 100;

	/* re-enable monitor */
	mon_enable(m);

	return 0;
}

static void msmcci_hwmon_save_state(void)
{
	int i;
	struct msmcci_hwmon *m;

	list_for_each_entry(m, &msmcci_hwmon_list, list) {
		if (!m->mon_enabled)
			continue;
		mon_disable(m);
		/*
		 * Power collapse might happen multiple times before
		 * re-evaluation is done. Accumulate the saved count.
		 * Clear counter after read in case power collapse is
		 * aborted and register values are not wiped.
		 */
		for (i = 0; i < m->num_counters; i++) {
			m->cur_count[i] += mon_read_count_single(m, i);
			mon_clear_single(m, i);
		}
	}
}

static void msmcci_hwmon_restore_limit(struct msmcci_hwmon *m, int i)
{
	u32 new_limit;

	if (m->cur_count[i] < m->cur_limit[i]) {
		new_limit = m->cur_limit[i] - m->cur_count[i];
	} else {
		/*
		 * If counter is larger than limit, interrupt should have
		 * fired and prevented power collapse from happening. Just
		 * in case the interrupt does not come, restore previous
		 * limit so that interrupt will be triggered at some point.
		 */
		new_limit = m->cur_limit[i];
	}
	mon_set_limit_single(m, i, new_limit);
	dev_dbg(m->dev, "Counter[%d] restore limit to 0x%x, saved count 0x%lx\n",
		i, new_limit, m->cur_count[i]);
}

static void msmcci_hwmon_restore_state(void)
{
	int i;
	struct msmcci_hwmon *m;

	list_for_each_entry(m, &msmcci_hwmon_list, list) {
		if (!m->mon_enabled)
			continue;
		mon_init(m);
		for (i = 0; i < m->num_counters; i++)
			msmcci_hwmon_restore_limit(m, i);
		mon_enable(m);
	}
}

#define CCI_LEVEL 2
static int msmcci_hwmon_pm_callback(struct notifier_block *nb,
					unsigned long val, void *data)
{
	unsigned int level = (unsigned long) data;

	if (level != CCI_LEVEL)
		return NOTIFY_DONE;

	/*
	 * When CCI power collapse callback happens, only current CPU
	 * would be executing code. Thus there is no need to hold
	 * mutex or spinlock.
	 */
	switch (val) {
	case CPU_CLUSTER_PM_ENTER:
		msmcci_hwmon_save_state();
		break;
	case CPU_CLUSTER_PM_ENTER_FAILED:
	case CPU_CLUSTER_PM_EXIT:
		msmcci_hwmon_restore_state();
		break;
	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static struct notifier_block pm_notifier_block = {
	.notifier_call = msmcci_hwmon_pm_callback,
};

static int register_pm_notifier(struct msmcci_hwmon *m)
{
	int ret;

	mutex_lock(&notifier_reg_lock);
	if (!use_cnt) {
		ret = cpu_pm_register_notifier(&pm_notifier_block);
		if (ret) {
			dev_err(m->dev, "Failed to register for PM notification\n");
			mutex_unlock(&notifier_reg_lock);
			return ret;
		}
	}
	use_cnt++;
	mutex_unlock(&notifier_reg_lock);

	return 0;
}

static void unregister_pm_nofitifier(void)
{
	mutex_lock(&notifier_reg_lock);
	use_cnt--;
	if (!use_cnt)
		cpu_pm_unregister_notifier(&pm_notifier_block);
	mutex_unlock(&notifier_reg_lock);
}

static int request_shared_interrupt(struct msmcci_hwmon *m)
{
	int ret;

	ret = request_threaded_irq(m->irq[HIGH], NULL,
			msmcci_hwmon_shared_intr_handler,
			IRQF_ONESHOT | IRQF_SHARED,
			dev_name(m->dev), m);
	if (ret)
		dev_err(m->dev, "Unable to register shared interrupt handler for irq %d\n",
			m->irq[HIGH]);

	return ret;
}

static int request_interrupts(struct msmcci_hwmon  *m)
{
	int i, ret;

	for (i = 0; i < m->num_counters; i++) {
		ret = request_threaded_irq(m->irq[i], NULL,
				msmcci_hwmon_intr_handler, IRQF_ONESHOT,
				dev_name(m->dev), m);
		if (ret) {
			dev_err(m->dev, "Unable to register interrupt handler for irq %d\n",
				m->irq[i]);
			goto irq_failure;
		}
	}
	return 0;

irq_failure:
	for (i--; i > 0; i--) {
		disable_irq(m->irq[i]);
		free_irq(m->irq[i], m);
	}
	return ret;
}

static int start_hwmon(struct cache_hwmon *hw, struct mrps_stats *mrps)
{
	struct msmcci_hwmon *m = to_mon(hw);
	unsigned int sample_ms = hw->df->profile->polling_ms;
	int ret, i;
	u32 limit;

	ret = register_pm_notifier(m);
	if (ret)
		return ret;

	if (m->irq_shared)
		ret = request_shared_interrupt(m);
	else
		ret = request_interrupts(m);

	if (ret) {
		unregister_pm_nofitifier();
		return ret;
	}
	mon_init(m);
	mon_disable(m);
	for (i = 0; i < m->num_counters; i++) {
		mon_clear_single(m, i);
		limit = mrps_to_count(mrps->mrps[i], sample_ms, 0);
		mon_set_limit_single(m, i, limit);
	}
	mon_enable(m);
	m->mon_enabled = true;

	return 0;
}

static void stop_hwmon(struct cache_hwmon *hw)
{
	struct msmcci_hwmon *m = to_mon(hw);
	int i;

	m->mon_enabled = false;
	mon_disable(m);

	for (i = 0; i < m->num_counters; i++) {
		if (!m->irq_shared || i == HIGH) {
			disable_irq(m->irq[i]);
			free_irq(m->irq[i], m);
		}
		mon_clear_single(m, i);
	}

	unregister_pm_nofitifier();
}

static int msmcci_hwmon_parse_dt(struct platform_device *pdev,
				 struct msmcci_hwmon *m, int idx)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	u32 sel;
	int ret;

	if (idx >= MAX_NUM_GROUPS)
		return -EINVAL;

	res = platform_get_resource(pdev, IORESOURCE_MEM, idx);
	if (!res)
		return (idx == HIGH) ? -EINVAL : 0;

	if (m->secure_io)
		m->phys_base[idx] = res->start;
	else {
		m->virt_base[idx] = devm_ioremap(&pdev->dev, res->start,
						resource_size(res));
		if (!m->virt_base[idx]) {
			dev_err(dev, "failed to ioremap\n");
			return -ENOMEM;
		}
	}

	ret = of_property_read_u32_index(pdev->dev.of_node,
				"qcom,counter-event-sel", idx, &sel);
	if (ret) {
		dev_err(dev, "Counter[%d] failed to read event sel\n", idx);
		return ret;
	}
	m->event_sel[idx] = sel;

	if (!m->irq_shared || idx == HIGH) {
		m->irq[idx] = platform_get_irq(pdev, idx);
		if (m->irq[idx] < 0) {
			dev_err(dev, "Counter[%d] failed to get IRQ number\n",
									idx);
			return m->irq[idx];
		}
	}
	m->num_counters++;
	return 0;
}

static int msmcci_hwmon_driver_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct msmcci_hwmon *m;
	int ret;

	m = devm_kzalloc(dev, sizeof(*m), GFP_KERNEL);
	if (!m)
		return -ENOMEM;
	m->dev = &pdev->dev;

	m->secure_io = of_property_read_bool(pdev->dev.of_node,
					"qcom,secure-io");

	m->irq_shared = of_property_read_bool(pdev->dev.of_node,
						"qcom,shared-irq");

	ret = msmcci_hwmon_parse_dt(pdev, m, HIGH);
	if (ret)
		return ret;
	ret = msmcci_hwmon_parse_dt(pdev, m, MED);
	if (ret)
		return ret;
	ret = msmcci_hwmon_parse_dt(pdev, m, LOW);
	if (ret)
		return ret;

	m->hw.of_node = of_parse_phandle(dev->of_node, "qcom,target-dev", 0);
	if (!m->hw.of_node) {
		dev_err(dev, "No target device specified\n");
		return -EINVAL;
	}
	m->hw.start_hwmon = &start_hwmon;
	m->hw.stop_hwmon = &stop_hwmon;
	m->hw.meas_mrps_and_set_irq = &meas_mrps_and_set_irq;
	mutex_init(&m->update_lock);

	/*
	 * This tests whether secure IO for monitor registers
	 * is supported.
	 */
	ret = mon_init(m);
	if (ret) {
		dev_err(dev, "Failed to config monitor. Cache hwmon not registered\n");
		return ret;
	}

	ret = register_cache_hwmon(dev, &m->hw);
	if (ret) {
		dev_err(dev, "MSMCCI cache hwmon registration failed\n");
		return ret;
	}

	mutex_lock(&list_lock);
	list_add_tail(&m->list, &msmcci_hwmon_list);
	mutex_unlock(&list_lock);

	dev_info(dev, "MSMCCI cache hwmon registered\n");
	return 0;
}

static const struct of_device_id cci_match_table[] = {
	{ .compatible = "qcom,msmcci-hwmon" },
	{}
};

static struct platform_driver msmcci_hwmon_driver = {
	.probe = msmcci_hwmon_driver_probe,
	.driver = {
		.name = "msmcci-hwmon",
		.of_match_table = cci_match_table,
	},
};

module_platform_driver(msmcci_hwmon_driver);
MODULE_DESCRIPTION("QTI CCI performance monitor driver");
MODULE_LICENSE("GPL v2");
