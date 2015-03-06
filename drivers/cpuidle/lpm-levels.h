/* Copyright (c) 2014, 2015, The Linux Foundation. All rights reserved.
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

#include <soc/qcom/pm.h>
#include <soc/qcom/spm.h>

#define NR_LPM_LEVELS 8

struct lpm_lookup_table {
	uint32_t modes;
	const char *mode_name;
};

struct power_params {
	uint32_t latency_us;		/* Enter + Exit latency */
	uint32_t ss_power;		/* Steady state power */
	uint32_t energy_overhead;	/* Enter + exit over head */
	uint32_t time_overhead_us;	/* Enter + exit overhead */
};

struct lpm_cpu_level {
	const char *name;
	enum msm_pm_sleep_mode mode;
	bool use_bc_timer;
	struct power_params pwr;
};

struct lpm_cpu {
	struct lpm_cpu_level levels[NR_LPM_LEVELS];
	int nlevels;
	struct lpm_cluster *parent;
};

struct lpm_level_avail {
	bool idle_enabled;
	bool suspend_enabled;
	struct kobject *kobj;
	struct kobj_attribute idle_enabled_attr;
	struct kobj_attribute suspend_enabled_attr;
};

struct lpm_cluster_level {
	const char *level_name;
	int *mode;			/* SPM mode to enter */
	int min_child_level;
	struct cpumask num_cpu_votes;
	struct power_params pwr;
	bool notify_rpm;
	bool disable_dynamic_routing;
	bool sync_level;
	bool last_core_only;
	struct lpm_level_avail available;
};

struct low_power_ops {
	struct msm_spm_device *spm;
	int (*set_mode)(struct low_power_ops *ops, int mode, bool notify_rpm);
	enum msm_pm_l2_scm_flag tz_flag;
};

struct lpm_cluster {
	struct list_head list;
	struct list_head child;
	const char *cluster_name;
	const char **name;
	unsigned long aff_level; /* Affinity level of the node */
	struct low_power_ops *lpm_dev;
	int ndevices;
	struct lpm_cluster_level levels[NR_LPM_LEVELS];
	int nlevels;
	enum msm_pm_l2_scm_flag l2_flag;
	int min_child_level;
	int default_level;
	int last_level;
	struct lpm_cpu *cpu;
	struct cpuidle_driver *drv;
	spinlock_t sync_lock;
	struct cpumask child_cpus;
	struct cpumask num_childs_in_sync;
	struct lpm_cluster *parent;
	struct lpm_stats *stats;
	bool no_saw_devices;
};

int set_l2_mode(struct low_power_ops *ops, int mode, bool notify_rpm);
void lpm_suspend_wake_time(uint64_t wakeup_time);
int set_system_mode(struct low_power_ops *ops, int mode, bool notify_rpm);
int set_l3_mode(struct low_power_ops *ops, int mode, bool notify_rpm);

struct lpm_cluster *lpm_of_parse_cluster(struct platform_device *pdev);
void free_cluster_node(struct lpm_cluster *cluster);
void cluster_dt_walkthrough(struct lpm_cluster *cluster);

int create_cluster_lvl_nodes(struct lpm_cluster *p, struct kobject *kobj);
bool lpm_cpu_mode_allow(unsigned int cpu,
		unsigned int mode, bool from_idle);
bool lpm_cluster_mode_allow(struct lpm_cluster *cluster,
		unsigned int mode, bool from_idle);

extern struct lpm_cluster *lpm_root_node;

#ifdef CONFIG_SMP
extern DEFINE_PER_CPU(bool, pending_ipi);
static inline bool is_IPI_pending(const struct cpumask *mask)
{
	unsigned int cpu;

	for_each_cpu(cpu, mask) {
		if per_cpu(pending_ipi, cpu)
			return true;
	}
	return false;
}
#else
static inline bool is_IPI_pending(const struct cpumask *mask)
{
	return false;
}
#endif
