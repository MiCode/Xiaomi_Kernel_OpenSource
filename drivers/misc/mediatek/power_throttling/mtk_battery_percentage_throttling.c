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
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>

#define BAT_PERCENT_LIMIT 15

static struct task_struct *bat_percent_notify_thread;
static bool bat_percent_notify_flag;
static DECLARE_WAIT_QUEUE_HEAD(bat_percent_notify_waiter);
static struct wakeup_source *bat_percent_notify_lock;
static DEFINE_MUTEX(bat_percent_notify_mutex);
static int g_battery_percent_level;
static int g_battery_percent_stop;

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

	if ((g_battery_percent_stop == 0) && (g_battery_percent_level == 1)) {
#if !IS_ENABLED(CONFIG_MTK_DYNAMIC_LOADING_POWER_THROTTLING)
		pr_info("[%s] level 1 happen\n", __func__);
		if (bp_cb != NULL)
			bp_cb(BATTERY_PERCENT_LEVEL_1);
#else
		if (prio_val == BATTERY_PERCENT_PRIO_FLASHLIGHT) {
			pr_info("[%s at DLPT] level l happen\n", __func__);
			if (bp_cb != NULL)
				bp_cb(BATTERY_PERCENT_LEVEL_1);
		}
#endif
	}
}

void exec_battery_percent_callback(
	enum BATTERY_PERCENT_LEVEL_TAG battery_percent_level)
{
	int i = 0;

	if (g_battery_percent_stop == 1) {
		pr_info("[%s] g_battery_percent_stop=%d\n"
			, __func__, g_battery_percent_stop);
	} else {
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

static int mtk_battery_percent_protect_ut_proc_show
(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", g_battery_percent_level);
	return 0;
}

static ssize_t mtk_battery_percent_protect_ut_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0, val = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	len = (len < 0) ? 0 : len;
	desc[len] = '\0';

	if (kstrtoint(desc, 10, &val) == 0) {
		if (val == 0 || val == 1) {
			pr_info("[%s] your input is %d\n", __func__, val);
			exec_battery_percent_callback(val);
		} else
			pr_info("[%s] wrong number (%d)\n", __func__, val);
	} else
		pr_info("[%s] wrong input (%s)\n", __func__, desc);

	return count;
}

static int mtk_battery_percent_protect_stop_proc_show
(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", g_battery_percent_stop);
	return 0;
}

static ssize_t mtk_battery_percent_protect_stop_proc_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0, val = 0;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	len = (len < 0) ? 0 : len;
	desc[len] = '\0';

	if (kstrtoint(desc, 10, &val) == 0) {
		if (val == 0 || val == 1)
			g_battery_percent_stop = val;
		else
			pr_info("[%s] wrong number (%d)\n", __func__, val);
	} else
		pr_info("[%s] wrong input (%s)\n", __func__, desc);

	return count;
}

static int mtk_battery_percent_protect_level_proc_show
(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", g_battery_percent_level);
	return 0;
}

#define PROC_FOPS_RW(name)						\
static int mtk_ ## name ## _proc_open(struct inode *inode, struct file *file)\
{									\
	return single_open(file, mtk_ ## name ## _proc_show, PDE_DATA(inode));\
}									\
static const struct file_operations mtk_ ## name ## _proc_fops = {	\
	.owner		= THIS_MODULE,					\
	.open		= mtk_ ## name ## _proc_open,			\
	.read		= seq_read,					\
	.llseek		= seq_lseek,					\
	.release	= single_release,				\
	.write		= mtk_ ## name ## _proc_write,			\
}

#define PROC_FOPS_RO(name)						\
static int mtk_ ## name ## _proc_open(struct inode *inode, struct file *file)\
{									\
	return single_open(file, mtk_ ## name ## _proc_show, PDE_DATA(inode));\
}									\
static const struct file_operations mtk_ ## name ## _proc_fops = {	\
	.owner		= THIS_MODULE,				\
	.open		= mtk_ ## name ## _proc_open,		\
	.read		= seq_read,				\
	.llseek		= seq_lseek,				\
	.release	= single_release,			\
}

#define PROC_ENTRY(name)	{__stringify(name), &mtk_ ## name ## _proc_fops}

PROC_FOPS_RW(battery_percent_protect_ut);
PROC_FOPS_RW(battery_percent_protect_stop);
PROC_FOPS_RO(battery_percent_protect_level);

static int battery_percent_create_procfs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(battery_percent_protect_ut),
		PROC_ENTRY(battery_percent_protect_stop),
		PROC_ENTRY(battery_percent_protect_level),
	};

	dir = proc_mkdir("bat_per_pt", NULL);

	if (!dir) {
		pr_notice("fail to create /proc/bat_per_pt\n");
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create
		    (entries[i].name, 0664, dir, entries[i].fops))
			pr_notice("@%s: create /proc/pt_bat_per/%s failed\n",
				__func__, entries[i].name);
	}

	return 0;
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
	battery_percent_create_procfs();
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
