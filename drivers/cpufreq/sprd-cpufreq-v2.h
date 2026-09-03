/* SPDX-License-Identifier: GPL-2.0 */
// Copyright (C) 2021 Unisoc, Inc.

#ifndef __SPRD_CPUFREQ_V2_H__
#define __SPRD_CPUFREQ_V2_H__

#include <linux/arch_topology.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/random.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sprd_sip_svc.h>
#include <linux/topology.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

#define DVFS_TEMP_UPPER_LIMIT		(274) /* Degrees Celsius */
#define DVFS_TEMP_LOW_LIMIT		(-274) /* Degrees Celsius */
#define DVFS_TEMP_DEFAULT		(10) /* Degrees Celsius */
#define DVFS_TEMP_MAX_TICKS		(3)
#define DVFS_TEMP_UPDATE_MS		(1500)

#define DVFS_SUSPEND_INDEX		(0)

struct temp_node {
	int temp;
	struct cpufreq_frequency_table *temp_table;
	struct list_head list;
};

struct cluster_info {
	u32 id, cpu;
	struct cpumask cpus;
	struct cpufreq_policy *policy_data;

	struct delayed_work temp_work;
	bool temp_enable;

	struct freq_qos_request max_req;

	struct mutex mutex;
	struct device_node *node;

	int (*dvfs_init)(u32 flag, u32 flag2, u32 flag3);

	int (*dvfs_debug_init)(void);

	int (*dvfs_enable)(u32 cluster);

	u32 voltage_step;
	int (*step_set)(u32 cluster, u32 step);

	u32 voltage_margin;
	int (*margin_set)(u32 cluster, u32 margin);

	u32 pmic_type;
	int (*pmic_set)(u32 cluster, u32 num);

	u32 transition_delay;

	u32 table_entry_num;

	bool boost_enable;

	u32 temp_tick;
	int best_temp;
	struct temp_node *temp_currt_node;
	struct temp_node *temp_level_node;
	struct list_head temp_list_head;

	struct thermal_zone_device *cpu_tz;
	const char *tz_name;

	int (*table_update)(u32 cluster, u32 temp, u32 *num);

	int (*freq_set)(u32 cluster, u32 index);
	int (*freq_get)(u32 cluster, u64 *freq);
	int (*pair_get)(u32 cluster, u32 index, u64 *freq, u64 *vol);
};

unsigned int sprd_cpufreq_update_opp(struct cluster_info *cluster, int now_temp);

int sprd_cluster_num(void);

int sprd_debug_cluster_init(struct cpufreq_policy *policy);
int sprd_debug_cluster_exit(struct cpufreq_policy *policy);
int sprd_debug_init(struct device *pdev);
int sprd_debug_exit(void);

#endif /* __SPRD_CPUFREQ_V2_H__ */

