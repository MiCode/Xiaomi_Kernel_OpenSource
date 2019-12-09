/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2014-2016, 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _ARCH_ARM_MACH_MSM_BUS_ADHOC_H
#define _ARCH_ARM_MACH_MSM_BUS_ADHOC_H

#include <linux/types.h>
#include <linux/device.h>
#include <linux/msm-bus-board.h>
#include <linux/msm-bus.h>
#include <linux/msm_bus_rules.h>
#include "msm_bus_core.h"

struct msm_bus_node_device_type;
struct link_node {
	uint64_t lnode_ib[NUM_CTX];
	uint64_t lnode_ab[NUM_CTX];
	int next;
	struct device *next_dev;
	struct list_head link;
	uint32_t in_use;
	const char *cl_name;
};

/* New types introduced for adhoc topology */
struct msm_bus_noc_ops {
	int (*qos_init)(struct msm_bus_node_device_type *dev,
			void __iomem *qos_base, uint32_t qos_off,
			uint32_t qos_delta, uint32_t qos_freq);
	int (*set_bw)(struct msm_bus_node_device_type *dev,
			void __iomem *qos_base, uint32_t qos_off,
			uint32_t qos_delta, uint32_t qos_freq);
	int (*limit_mport)(struct msm_bus_node_device_type *dev,
			void __iomem *qos_base, uint32_t qos_off,
			uint32_t qos_delta, uint32_t qos_freq, int enable_lim,
			uint64_t lim_bw);
	bool (*update_bw_reg)(int mode);
};

struct nodebw {
	uint64_t sum_ab;
	uint64_t last_sum_ab;
	uint64_t max_ib;
	uint64_t cur_clk_hz;
	uint32_t util_used;
	uint32_t vrail_used;
};

struct msm_bus_fab_device_type {
	void __iomem *qos_base;
	phys_addr_t pqos_base;
	size_t qos_range;
	uint32_t base_offset;
	uint32_t qos_freq;
	uint32_t qos_off;
	struct msm_bus_noc_ops noc_ops;
	enum msm_bus_hw_sel bus_type;
	bool bypass_qos_prg;
};

struct msm_bus_noc_limiter {
	uint32_t bw;
	uint32_t sat;
};

struct msm_bus_noc_regulator {
	uint32_t low_prio;
	uint32_t hi_prio;
	uint32_t bw;
	uint32_t sat;
};

struct msm_bus_noc_regulator_mode {
	uint32_t read;
	uint32_t write;
};

struct qos_params_type {
	int mode;
	unsigned int prio_lvl;
	unsigned int prio_rd;
	unsigned int prio_wr;
	unsigned int prio1;
	unsigned int prio0;
	unsigned int reg_prio1;
	unsigned int reg_prio0;
	unsigned int gp;
	unsigned int thmp;
	unsigned int ws;
	unsigned int prio_dflt;
	struct msm_bus_noc_limiter limiter;
	bool limiter_en;
	struct msm_bus_noc_regulator reg;
	struct msm_bus_noc_regulator_mode reg_mode;
	bool urg_fwd_en;
	u64 bw_buffer;
};

struct node_util_levels_type {
	uint64_t threshold;
	uint32_t util_fact;
};

struct node_agg_params_type {
	uint32_t agg_scheme;
	uint32_t num_aggports;
	unsigned int buswidth;
	uint32_t vrail_comp;
	uint32_t num_util_levels;
	struct node_util_levels_type *util_levels;
};

struct msm_bus_node_info_type {
	const char *name;
	unsigned int id;
	int mas_rpm_id;
	int slv_rpm_id;
	int num_ports;
	int num_qports;
	int *qport;
	struct qos_params_type qos_params;
	unsigned int num_connections;
	unsigned int num_blist;
	bool is_fab_dev;
	bool virt_dev;
	bool is_traversed;
	unsigned int *connections;
	unsigned int *bl_cons;
	struct device **dev_connections;
	struct device **black_connections;
	unsigned int bus_device_id;
	struct device *bus_device;
	struct rule_update_path_info rule;
	uint64_t lim_bw;
	bool defer_qos;
	struct node_agg_params_type agg_params;
};

struct msm_bus_node_device_type {
	struct msm_bus_node_info_type *node_info;
	struct msm_bus_fab_device_type *fabdev;
	int num_lnodes;
	struct link_node *lnode_list;
	struct nodebw node_bw[NUM_CTX];
	struct list_head link;
	unsigned int ap_owned;
	struct nodeclk clk[NUM_CTX];
	struct nodeclk bus_qos_clk;
	uint32_t num_node_qos_clks;
	struct nodeclk *node_qos_clks;
	struct device_node *of_node;
	struct device dev;
	bool dirty;
	struct list_head dev_link;
	struct list_head devlist;
};

static inline struct msm_bus_node_device_type *to_msm_bus_node(struct device *d)
{
	return container_of(d, struct msm_bus_node_device_type, dev);
}


int msm_bus_enable_limiter(struct msm_bus_node_device_type *nodedev,
				int throttle_en, uint64_t lim_bw);
int msm_bus_commit_data(struct list_head *clist);
void *msm_bus_realloc_devmem(struct device *dev, void *p, size_t old_size,
					size_t new_size, gfp_t flags);

extern struct msm_bus_device_node_registration
	*msm_bus_of_to_pdata(struct platform_device *pdev);
extern void msm_bus_arb_setops_adhoc(struct msm_bus_arb_ops *arb_ops);
extern int msm_bus_bimc_set_ops(struct msm_bus_node_device_type *bus_dev);
extern int msm_bus_noc_set_ops(struct msm_bus_node_device_type *bus_dev);
extern int msm_bus_qnoc_set_ops(struct msm_bus_node_device_type *bus_dev);
extern int msm_bus_of_get_static_rules(struct platform_device *pdev,
					struct bus_rule_type **static_rule);
extern int msm_rules_update_path(struct list_head *input_list,
				struct list_head *output_list);
extern void print_all_rules(void);
#ifdef CONFIG_DEBUG_BUS_VOTER
int msm_bus_floor_init(struct device *dev);
#else
static inline int msm_bus_floor_init(struct device *dev)
{
	return 0;
}
#endif /* CONFIG_DBG_BUS_VOTER */
#endif /* _ARCH_ARM_MACH_MSM_BUS_ADHOC_H */
