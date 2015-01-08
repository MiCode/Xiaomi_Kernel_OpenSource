/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
	uint64_t ab[NUM_CTX];
	bool dirty;
};

struct msm_bus_fab_device_type {
	void __iomem *qos_base;
	phys_addr_t pqos_base;
	size_t qos_range;
	uint32_t base_offset;
	uint32_t qos_freq;
	uint32_t qos_off;
	uint32_t util_fact;
	uint32_t vrail_comp;
	struct msm_bus_noc_ops noc_ops;
	enum msm_bus_hw_sel bus_type;
	bool bypass_qos_prg;
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
	int cur_mode;
	u64 bw_buffer;
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
	unsigned int *black_listed_connections;
	struct device **dev_connections;
	struct device **black_connections;
	unsigned int bus_device_id;
	struct device *bus_device;
	unsigned int buswidth;
	struct rule_update_path_info rule;
	uint64_t lim_bw;
	uint32_t util_fact;
	uint32_t vrail_comp;
	uint32_t num_aggports;
};

struct msm_bus_node_device_type {
	struct msm_bus_node_info_type *node_info;
	struct msm_bus_fab_device_type *fabdev;
	int num_lnodes;
	struct link_node *lnode_list;
	uint64_t cur_clk_hz[NUM_CTX];
	struct nodebw node_ab;
	struct list_head link;
	unsigned int ap_owned;
	struct nodeclk clk[NUM_CTX];
	struct nodeclk qos_clk;
};

int msm_bus_enable_limiter(struct msm_bus_node_device_type *nodedev,
				int throttle_en, uint64_t lim_bw);
int msm_bus_update_clks(struct msm_bus_node_device_type *nodedev,
	int ctx, int **dirty_nodes, int *num_dirty);
int msm_bus_commit_data(int *dirty_nodes, int ctx, int num_dirty);
int msm_bus_update_bw(struct msm_bus_node_device_type *nodedev, int ctx,
	int64_t add_bw, int **dirty_nodes, int *num_dirty);
void *msm_bus_realloc_devmem(struct device *dev, void *p, size_t old_size,
					size_t new_size, gfp_t flags);

extern struct msm_bus_device_node_registration
	*msm_bus_of_to_pdata(struct platform_device *pdev);
extern void msm_bus_arb_setops_adhoc(struct msm_bus_arb_ops *arb_ops);
extern int msm_bus_bimc_set_ops(struct msm_bus_node_device_type *bus_dev);
extern int msm_bus_noc_set_ops(struct msm_bus_node_device_type *bus_dev);
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
