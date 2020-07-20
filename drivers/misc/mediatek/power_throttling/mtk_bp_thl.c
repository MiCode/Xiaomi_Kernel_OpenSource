// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include "mtk_bp_thl.h"

#define BAT_PERCENT_LIMIT 15

static struct task_struct *bp_notify_thread;
static bool bp_notify_flag;
static DECLARE_WAIT_QUEUE_HEAD(bp_notify_waiter);
static struct wakeup_source *bp_notify_lock;
static int g_bp_thl_lv;

struct bp_thl_callback_table {
	void (*bpcb)(enum BATTERY_PERCENT_LEVEL_TAG);
};

#define BPCB_MAX_NUM 16

static struct bp_thl_callback_table bpcb_tb[BPCB_MAX_NUM] = { {0} };

static struct notifier_block bp_nb;

static unsigned int ut_status;

void set_bp_thl_ut_status(int status)
{
	ut_status = status;
}
EXPORT_SYMBOL(set_bp_thl_ut_status);

void register_bp_thl_notify(
	battery_percent_callback bp_cb,
	BATTERY_PERCENT_PRIO prio_val)
{
	if (prio_val >= BPCB_MAX_NUM || prio_val < 0) {
		pr_info("[%s] prio_val=%d, out of boundary\n", __func__, prio_val);
		return;
	}

	bpcb_tb[prio_val].bpcb = bp_cb;
	pr_info("[%s] prio_val=%d\n", __func__, prio_val);

	if (g_bp_thl_lv == 1) {
		pr_info("[%s] level 1 happen\n", __func__);
		if (bp_cb != NULL)
			bp_cb(BATTERY_PERCENT_LEVEL_1);
	}
}
EXPORT_SYMBOL(register_bp_thl_notify);

void unregister_bp_thl_notify(BATTERY_PERCENT_PRIO prio_val)
{
	if (prio_val >= BPCB_MAX_NUM || prio_val < 0) {
		pr_info("[%s] prio_val=%d, out of boundary\n", __func__, prio_val);
		return;
	}

	bpcb_tb[prio_val].bpcb = NULL;
	pr_info("[%s] prio_val=%d\n", __func__, prio_val);
}
EXPORT_SYMBOL(unregister_bp_thl_notify);

void exec_bp_thl_callback(enum BATTERY_PERCENT_LEVEL_TAG bp_level)
{
	int i;

	for (i = 0; i < BPCB_MAX_NUM; i++) {
		if (bpcb_tb[i].bpcb != NULL) {
			bpcb_tb[i].bpcb(bp_level);
			pr_info("[%s] prio_val=%d, bp_level=%d\n", __func__, i, bp_level);
		}
	}
}

int bp_notify_handler(void *unused)
{
	do {
		wait_event_interruptible(bp_notify_waiter, (bp_notify_flag == true));
		__pm_stay_awake(bp_notify_lock);
		exec_bp_thl_callback(g_bp_thl_lv);
		bp_notify_flag = false;
		__pm_relax(bp_notify_lock);
	} while (!kthread_should_stop());

	return 0;
}

int bp_psy_event(struct notifier_block *nb, unsigned long event, void *v)
{
	struct power_supply *psy = v;
	union power_supply_propval val;
	int ret, uisoc, bat_status;

	if (strcmp(psy->desc->name, "battery") != 0)
		return NOTIFY_DONE;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	if (ret)
		return NOTIFY_DONE;

	uisoc = val.intval;

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_STATUS, &val);
	if (ret)
		return NOTIFY_DONE;

	bat_status = val.intval;

	if (ut_status == 1) {
		uisoc = BAT_PERCENT_LIMIT;
		bat_status = POWER_SUPPLY_STATUS_DISCHARGING;
	} else if (ut_status == 2) {
		uisoc = BAT_PERCENT_LIMIT + 1;
		bat_status = POWER_SUPPLY_STATUS_CHARGING;
	}

	if ((bat_status != POWER_SUPPLY_STATUS_CHARGING) &&
		(g_bp_thl_lv == BATTERY_PERCENT_LEVEL_0) &&
		(uisoc <= BAT_PERCENT_LIMIT && uisoc >= 0)) {
		g_bp_thl_lv = BATTERY_PERCENT_LEVEL_1;
		bp_notify_flag = true;
		wake_up_interruptible(&bp_notify_waiter);
		pr_info("bp_notify called, l=%d s=%d soc=%d\n", g_bp_thl_lv, bat_status, uisoc);
	} else if (((bat_status == POWER_SUPPLY_STATUS_CHARGING) || (uisoc > BAT_PERCENT_LIMIT)) &&
		(g_bp_thl_lv == BATTERY_PERCENT_LEVEL_1)) {
		g_bp_thl_lv = BATTERY_PERCENT_LEVEL_0;
		bp_notify_flag = true;
		wake_up_interruptible(&bp_notify_waiter);
		pr_info("bp_notify called, l=%d s=%d soc=%d\n", g_bp_thl_lv, bat_status, uisoc);
	}

	return NOTIFY_DONE;
}

static int __init mtk_bp_thl_module_init(void)
{
	int ret;

	bp_notify_lock = wakeup_source_register(NULL, "bp_notify_lock wakelock");
	if (!bp_notify_lock) {
		pr_notice("bp_notify_lock wakeup source fail\n");
		return bp_notify_lock;
	}

	bp_notify_thread = kthread_run(bp_notify_handler, 0, "bp_notify_thread");
	if (IS_ERR(bp_notify_thread)) {
		pr_notice("Failed to create bp_notify_thread\n");
		return bp_notify_thread;
	}

	bp_nb.notifier_call = bp_psy_event;
	ret = power_supply_reg_notifier(&bp_nb);
	if (!ret) {
		pr_notice("power_supply_reg_notifier fail\n");
		return ret;
	}

	return 0;
}

static void __exit mtk_bp_thl_module_exit(void)
{
}

module_init(mtk_bp_thl_module_init);
module_exit(mtk_bp_thl_module_exit);

MODULE_AUTHOR("Samuel Hsieh");
MODULE_DESCRIPTION("MTK battery percent throttling driver");
MODULE_LICENSE("GPL");
