// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#include <linux/sched/clock.h>
#include <linux/sched/mm.h>
#include <linux/sched/numa_balancing.h>
#include <linux/sched/task_stack.h>
#include <linux/sched/task.h>
#include <linux/sched/cputime.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/kallsyms.h>
#include <linux/trace_events.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>   /* for misc_register, and SYNTH_MINOR */
#include <linux/proc_fs.h>
#include "cpu_ctrl.h"
#include "eas_ctrl.h"

#include <linux/workqueue.h>
#include <linux/unistd.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/pm_qos.h>
#include "gbe_common.h"
#include "gbe1.h"
#include "gbe2.h"
#include "gbe_sysfs.h"

static DEFINE_MUTEX(gbe_lock);
static int boost_set[KIR_NUM];
static unsigned long policy_mask;

enum GBE_BOOST_DEVICE {
	GBE_BOOST_UNKNOWN = -1,
	GBE_BOOST_CPU = 0,
	GBE_BOOST_EAS = 1,
	GBE_BOOST_VCORE = 2,
	GBE_BOOST_IO = 3,
	GBE_BOOST_HE = 4,
	GBE_BOOST_GPU = 5,
	GBE_BOOST_LLF = 6,
	GBE_BOOST_NUM = 7,
};

static unsigned long __read_mostly tracing_mark_write_addr;
static inline void __mt_update_tracing_mark_write_addr(void)
{
	if (unlikely(tracing_mark_write_addr == 0))
		tracing_mark_write_addr =
			kallsyms_lookup_name("tracing_mark_write");
}
void gbe_trace_printk(int pid, char *module, char *string)
{
	__mt_update_tracing_mark_write_addr();
	preempt_disable();
	event_trace_printk(tracing_mark_write_addr, "%d [%s] %s\n",
			pid, module, string);
	preempt_enable();
}

void gbe_trace_count(int tid, unsigned long long bufID,
	int val, const char *fmt, ...)
{
	char log[32];
	va_list args;
	int len;

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 32))
		log[31] = '\0';

	__mt_update_tracing_mark_write_addr();
	preempt_disable();

	if (!strstr(CONFIG_MTK_PLATFORM, "mt8")) {
		if (!bufID)
			event_trace_printk(tracing_mark_write_addr, "C|%d|%s|%d\n",
				tid, log, val);
		else
			event_trace_printk(tracing_mark_write_addr, "C|%d|%s|%d|0x%llx\n",
					tid, log, val, bufID);
	} else {
		event_trace_printk(tracing_mark_write_addr, "C|%s|%d\n",
				log, val);
	}

	preempt_enable();
}

struct k_list {
	struct list_head queue_list;
	int gbe2pwr_cmd;
	int gbe2pwr_value1;
	int gbe2pwr_value2;
};
static LIST_HEAD(head);
static int condition_get_cmd;
static DEFINE_MUTEX(gbe2pwr_lock);
static DECLARE_WAIT_QUEUE_HEAD(pwr_queue);
void gbe_sentcmd(int cmd, int value1, int value2)
{
	static struct k_list *node;

	mutex_lock(&gbe2pwr_lock);
	node = kmalloc(sizeof(*node), GFP_KERNEL);
	if (node == NULL)
		goto out;
	node->gbe2pwr_cmd = cmd;
	node->gbe2pwr_value1 = value1;
	node->gbe2pwr_value2 = value2;
	list_add_tail(&node->queue_list, &head);
	condition_get_cmd = 1;
out:
	mutex_unlock(&gbe2pwr_lock);
	wake_up_interruptible(&pwr_queue);
}

void gbe_ctrl2base_get_pwr_cmd(int *cmd, int *value1, int *value2)
{
	static struct k_list *node;

	wait_event_interruptible(pwr_queue, condition_get_cmd);
	mutex_lock(&gbe2pwr_lock);
	if (!list_empty(&head)) {
		node = list_first_entry(&head, struct k_list, queue_list);
		*cmd = node->gbe2pwr_cmd;
		*value1 = node->gbe2pwr_value1;
		*value2 = node->gbe2pwr_value2;
		list_del(&node->queue_list);
		kfree(node);
	}
	if (list_empty(&head))
		condition_get_cmd = 0;
	mutex_unlock(&gbe2pwr_lock);
}

void gbe_boost(enum GBE_KICKER kicker, int boost)
{
	int i;
	int boost_final = 0;
	int cpu_boost = 0, eas_boost = -1, vcore_boost = -1,
			io_boost = 0, he_boost = 0, gpu_boost = 0,
			llf_boost = 0;

	mutex_lock(&gbe_lock);

	if (boost_set[kicker] == !!boost)
		goto out;

	boost_set[kicker] = !!boost;

	for (i = 0; i < KIR_NUM; i++)
		if (boost_set[i] == 1) {
			boost_final = 1;
			break;
		}

	if (boost_final) {
		cpu_boost = 1;
		eas_boost = 100;
		vcore_boost = 0;
		io_boost = 1;
		he_boost = 1;
		gpu_boost = 1;
		llf_boost = 1;
	}

	if (test_bit(GBE_BOOST_CPU, &policy_mask))
		gbe_sentcmd(GBE_BOOST_CPU, cpu_boost, -1);

	if (test_bit(GBE_BOOST_EAS, &policy_mask))
		gbe_sentcmd(GBE_BOOST_EAS, eas_boost, -1);

	if (test_bit(GBE_BOOST_VCORE, &policy_mask))
		gbe_sentcmd(GBE_BOOST_VCORE, vcore_boost, -1);

	if (test_bit(GBE_BOOST_IO, &policy_mask))
		gbe_sentcmd(GBE_BOOST_IO, io_boost, -1);

	if (test_bit(GBE_BOOST_HE, &policy_mask))
		gbe_sentcmd(GBE_BOOST_HE, he_boost, -1);

	if (test_bit(GBE_BOOST_GPU, &policy_mask))
		gbe_sentcmd(GBE_BOOST_GPU, gpu_boost, -1);

	if (test_bit(GBE_BOOST_LLF, &policy_mask))
		gbe_sentcmd(GBE_BOOST_LLF, llf_boost, -1);

out:
	mutex_unlock(&gbe_lock);

}

static ssize_t gbe_policy_mask_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", policy_mask);
}

static ssize_t gbe_policy_mask_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int val = 0;
	char acBuffer[GBE_SYSFS_MAX_BUFF_SIZE];
	int arg;
	int cpu_boost = 0, eas_boost = -1, vcore_boost = -1,
			io_boost = 0, he_boost = 0, gpu_boost = 0,
			llf_boost = 0;

	if ((count > 0) && (count < GBE_SYSFS_MAX_BUFF_SIZE)) {
		if (scnprintf(acBuffer, GBE_SYSFS_MAX_BUFF_SIZE, "%s", buf)) {
			if (kstrtoint(acBuffer, 0, &arg) == 0)
				val = arg;
			else
				return count;
		}
	}

	if (val > 1 << GBE_BOOST_NUM || val < 0)
		return count;

	mutex_lock(&gbe_lock);
	policy_mask = val;

	gbe_sentcmd(GBE_BOOST_CPU, cpu_boost, -1);
	gbe_sentcmd(GBE_BOOST_EAS, eas_boost, -1);
	gbe_sentcmd(GBE_BOOST_VCORE, vcore_boost, -1);
	gbe_sentcmd(GBE_BOOST_IO, io_boost, -1);
	gbe_sentcmd(GBE_BOOST_HE, he_boost, -1);
	gbe_sentcmd(GBE_BOOST_GPU, gpu_boost, -1);
	gbe_sentcmd(GBE_BOOST_LLF, llf_boost, -1);
	mutex_unlock(&gbe_lock);

	return count;
}

static KOBJ_ATTR_RW(gbe_policy_mask);

static void __exit gbe_common_exit(void)
{
	gbe_sysfs_remove_file(&kobj_attr_gbe_policy_mask);
	gbe_sysfs_exit();
}

struct dentry *gbe_debugfs_dir;
static int __init gbe_common_init(void)
{
	gbe_get_cmd_fp = gbe_ctrl2base_get_pwr_cmd;

	gbe_sysfs_init();
	gbe1_init();
	gbe2_init();

	gbe_sysfs_create_file(&kobj_attr_gbe_policy_mask);


	set_bit(GBE_BOOST_CPU, &policy_mask);
	set_bit(GBE_BOOST_EAS, &policy_mask);
	set_bit(GBE_BOOST_VCORE, &policy_mask);
	set_bit(GBE_BOOST_HE, &policy_mask);

	return 0;
}

module_init(gbe_common_init);
module_exit(gbe_common_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek GBE");
MODULE_AUTHOR("MediaTek Inc.");
