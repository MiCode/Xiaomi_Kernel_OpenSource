/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * Copyright (c) 2014-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __LPM_LEVELS_H__
#define __LPM_LEVELS_H__

#include <soc/qcom/pm.h>

#define NR_LPM_LEVELS 8
#define MAXSAMPLES 5
#define CLUST_SMPL_INVLD_TIME 40000
#define DEFAULT_PREMATURE_CNT 3
#define DEFAULT_STDDEV 100
#define DEFAULT_IPI_STDDEV 400
#define DEFAULT_TIMER_ADD 100
#define DEFAULT_IPI_TIMER_ADD 900
#define TIMER_ADD_LOW 100
#define TIMER_ADD_HIGH 1500
#define STDDEV_LOW 100
#define STDDEV_HIGH 1000
#define PREMATURE_CNT_LOW 1
#define PREMATURE_CNT_HIGH 5

/* RIMPS registers */
#define TIMER_CTRL		0x0
#define TIMER_VAL		0x4
#define TIMER_PENDING		0x18
#define TIMER_THRESHOLD		0x1C

/* RIMPS registers offset */
#define TIMER_CONTROL_EN	0x1

/* RIMPS timer clock */
#define ARCH_TIMER_HZ	19200000

struct power_params {
	uint32_t entry_latency;		/* Entry latency */
	uint32_t exit_latency;		/* Exit latency */
	uint32_t min_residency;
	uint32_t max_residency;
};

struct lpm_cpu_level {
	const char *name;
	bool use_bc_timer;
	struct power_params pwr;
	unsigned int psci_id;
	bool is_reset;
};

struct lpm_cpu {
	struct list_head list;
	struct cpumask related_cpus;
	struct lpm_cpu_level levels[NR_LPM_LEVELS];
	int nlevels;
	const char *domain_name;
	unsigned int psci_mode_shift;
	unsigned int psci_mode_mask;
	uint32_t ref_stddev;
	uint32_t ref_premature_cnt;
	uint32_t tmr_add;
	bool lpm_prediction;
	void __iomem *rimps_tmr_base;
	spinlock_t cpu_lock;
	bool ipi_prediction;
	uint64_t bias;
	struct cpuidle_driver *drv;
	struct lpm_cluster *parent;
};

struct lpm_level_avail {
	bool idle_enabled;
	bool suspend_enabled;
	uint32_t exit_latency;
	struct kobject *kobj;
	struct kobj_attribute idle_enabled_attr;
	struct kobj_attribute suspend_enabled_attr;
	struct kobj_attribute latency_attr;
	void *data;
	int idx;
	bool cpu_node;
};

struct lpm_cluster_level {
	const char *level_name;
	int min_child_level;
	struct cpumask num_cpu_votes;
	struct power_params pwr;
	bool notify_rpm;
	bool sync_level;
	struct lpm_level_avail available;
	unsigned int psci_id;
	bool is_reset;
};

struct cluster_history {
	uint32_t resi[MAXSAMPLES];
	int mode[MAXSAMPLES];
	int64_t stime[MAXSAMPLES];
	uint32_t hptr;
	uint32_t hinvalid;
	uint32_t htmr_wkup;
	uint64_t entry_time;
	int entry_idx;
	int nsamp;
	int flag;
};

struct lpm_cluster {
	struct list_head list;
	struct list_head child;
	const char *cluster_name;
	unsigned long aff_level; /* Affinity level of the node */
	struct lpm_cluster_level levels[NR_LPM_LEVELS];
	int nlevels;
	int min_child_level;
	int default_level;
	int last_level;
	uint32_t tmr_add;
	bool lpm_prediction;
	struct list_head cpu;
	spinlock_t sync_lock;
	struct cpumask child_cpus;
	struct cpumask num_children_in_sync;
	struct lpm_cluster *parent;
	struct lpm_stats *stats;
	unsigned int psci_mode_shift;
	unsigned int psci_mode_mask;
	struct cluster_history history;
	struct hrtimer histtimer;
};

struct lpm_cluster *lpm_of_parse_cluster(struct platform_device *pdev);
void free_cluster_node(struct lpm_cluster *cluster);
void cluster_dt_walkthrough(struct lpm_cluster *cluster);
uint32_t us_to_ticks(uint64_t sleep_val);

int create_cluster_lvl_nodes(struct lpm_cluster *p, struct kobject *kobj);
int lpm_cpu_mode_allow(unsigned int cpu,
		unsigned int mode, bool from_idle);
bool lpm_cluster_mode_allow(struct lpm_cluster *cluster,
		unsigned int mode, bool from_idle);
uint32_t *get_per_cpu_max_residency(int cpu);
uint32_t *get_per_cpu_min_residency(int cpu);
extern struct lpm_cluster *lpm_root_node;
#endif /* __LPM_LEVELS_H__ */
