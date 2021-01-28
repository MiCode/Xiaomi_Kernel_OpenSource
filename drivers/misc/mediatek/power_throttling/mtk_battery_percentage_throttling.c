// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/power_supply.h>
#include <mtk_battery_percentage_throttling.h>

#define BAT_PERCENT_LIMIT 15

static struct task_struct *bat_percent_notify_thread;
static bool bat_percent_notify_flag;
static DECLARE_WAIT_QUEUE_HEAD(bat_percent_notify_waiter);
static struct wakeup_source *bat_percent_notify_lock;
static DEFINE_MUTEX(bat_percent_notify_mutex);
static int g_battery_percent_level;

struct battery_percent_callback_table {
	void (*bpcb)(enum BATTERY_PERCENT_LEVEL_TAG);
};

#define BPCB_MAX_NUM 16

static struct battery_percent_callback_table bpcb_tb[BPCB_MAX_NUM] = { {0} };

static struct notifier_block bp_nb;

void register_battery_percent_notify(
	battery_percent_callback bp_cb,
	BATTERY_PERCENT_PRIO prio_val)
{
	if (prio_val >= BPCB_MAX_NUM || prio_val < 0) {
		pr_info("[%s] prio_val=%d, out of boundary\n",
			__func__, prio_val);
		return;
	}

	bpcb_tb[prio_val].bpcb = bp_cb;
	pr_info("[%s] prio_val=%d\n", __func__, prio_val);

	if (g_battery_percent_level == 1) {
		pr_info("[%s] level 1 happen\n", __func__);
		if (bp_cb != NULL)
			bp_cb(BATTERY_PERCENT_LEVEL_1);
	}
}

void exec_battery_percent_callback(
	enum BATTERY_PERCENT_LEVEL_TAG battery_percent_level)
{
	int i = 0;

#if !IS_ENABLED(CONFIG_MTK_DYNAMIC_LOADING_POWER_THROTTLING)
	for (i = 0; i < BPCB_MAX_NUM; i++) {
		if (bpcb_tb[i].bpcb != NULL) {
			bpcb_tb[i].bpcb(battery_percent_level);
			pr_info("[%s] prio_val=%d, battery_percent_level=%d\n"
				, __func__, i, battery_percent_level);
		}
	}
#else
	if (bpcb_tb[BATTERY_PERCENT_PRIO_FLASHLIGHT].bpcb != NULL) {
		bpcb_tb[BATTERY_PERCENT_PRIO_FLASHLIGHT].bpcb(
			battery_percent_level);
		pr_info("[%s] prio_val=%d, battery_percent_level=%d\n"
				, __func__, i, battery_percent_level);
	} else
		pr_notice("[%s]BATTERY_PERCENT_PRIO_FLASHLIGHT is null\n"
			, __func__);
#endif
}

int bat_percent_notify_handler(void *unused)
{
	do {
		wait_event_interruptible(bat_percent_notify_waiter,
			(bat_percent_notify_flag == true));

		__pm_stay_awake(bat_percent_notify_lock);
		mutex_lock(&bat_percent_notify_mutex);

		exec_battery_percent_callback(g_battery_percent_level);
		bat_percent_notify_flag = false;

		mutex_unlock(&bat_percent_notify_mutex);
		__pm_relax(bat_percent_notify_lock);
	} while (!kthread_should_stop());

	return 0;
}

int bp_psy_event(struct notifier_block *nb, unsigned long event, void *v)
{
	struct power_supply *psy = v;
	union power_supply_propval val;
	int ret = 0;
	int uisoc = -1, bat_status = -1;

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

	if ((bat_status != POWER_SUPPLY_STATUS_CHARGING &&
		bat_status != -1) &&
		(g_battery_percent_level == BATTERY_PERCENT_LEVEL_0) &&
		(uisoc <= BAT_PERCENT_LIMIT && uisoc >= 0)) {
		g_battery_percent_level = BATTERY_PERCENT_LEVEL_1;
		bat_percent_notify_flag = true;
		wake_up_interruptible(&bat_percent_notify_waiter);
		pr_info("bat_percent_notify called, l=%d s=%d soc=%d\n",
			g_battery_percent_level, bat_status, uisoc);
	} else if (((bat_status == POWER_SUPPLY_STATUS_CHARGING) ||
		(uisoc > BAT_PERCENT_LIMIT)) &&
		(g_battery_percent_level == BATTERY_PERCENT_LEVEL_1)) {
		g_battery_percent_level = BATTERY_PERCENT_LEVEL_0;
		bat_percent_notify_flag = true;
		wake_up_interruptible(&bat_percent_notify_waiter);
		pr_info("bat_percent_notify called, l=%d s=%d soc=%d\n",
			g_battery_percent_level, bat_status, uisoc);
	}

	return NOTIFY_DONE;
}

void bat_percent_notify_init(void)
{
	bat_percent_notify_lock = wakeup_source_register(NULL, 
		"bat_percent_notify_lock wakelock");
	if (!bat_percent_notify_lock)
		pr_notice("bat_percent_notify_lock wakeup source fail\n");

	bat_percent_notify_thread =
		kthread_run(bat_percent_notify_handler, 0,
			"bat_percent_notify_thread");
	if (IS_ERR(bat_percent_notify_thread))
		pr_notice("Failed to create bat_percent_notify_thread\n");

	bp_nb.notifier_call = bp_psy_event;
	power_supply_reg_notifier(&bp_nb);
}

static int __init battery_percentage_throttling_module_init(void)
{
	bat_percent_notify_init();
	return 0;
}

static void __exit battery_percentage_throttling_module_exit(void)
{
}

module_init(battery_percentage_throttling_module_init);
module_exit(battery_percentage_throttling_module_exit);

MODULE_AUTHOR("Samuel Hsieh");
MODULE_DESCRIPTION("MTK battery percentage throttling driver");
MODULE_LICENSE("GPL");
