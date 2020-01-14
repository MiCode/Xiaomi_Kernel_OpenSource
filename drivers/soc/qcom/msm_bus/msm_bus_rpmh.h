/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2014-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _ARCH_ARM_MACH_MSM_BUS_ADHOC_H
#define _ARCH_ARM_MACH_MSM_BUS_ADHOC_H

#include <linux/types.h>
#include <linux/device.h>
#include <linux/msm-bus-board.h>
#include <linux/msm-bus.h>
#include <linux/msm_bus_rules.h>
#include "msm_bus_core.h"
#include "msm_bus_noc.h"

#define VCD_MAX_CNT 16

struct msm_bus_node_device_type;

struct link_node {
	uint64_t lnode_ib[NUM_CTX];
	uint64_t lnode_ab[NUM_CTX];
	uint64_t query_ib[NUM_CTX];
	uint64_t query_ab[NUM_CTX];
	uint64_t alc_idx[NUM_CTX];
	int next;
	struct device *next_dev;
	struct list_head link;
	uint32_t in_use;
	const char *cl_name;
	unsigned int bus_dev_id;
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
	int (*sbm_config)(struct msm_bus_node_device_type *node_dev,
			void __iomem *noc_base, uint32_t sbm_offset,
			bool enable);
};

struct nodebw {
	uint64_t sum_ab;
	uint64_t last_sum_ab;
	uint64_t last_max_ib;
	uint64_t max_ib;
	uint64_t max_ab;
	uint64_t sum_query_ab;
	uint64_t max_query_ib;
	uint64_t max_query_ab;
	uint64_t max_alc;
	uint64_t cur_clk_hz;
	uint32_t util_used;
	uint32_t vrail_used;
};

struct nodevector {
	uint64_t vec_a;
	uint64_t vec_b;
	uint64_t query_vec_a;
	uint64_t query_vec_b;
};

struct node_regulator {
	char name[MAX_REG_NAME];
	struct regulator *reg;
};

struct qos_bcm_type {
	int qos_bcm_id;
	struct nodevector vec;
};

struct msm_bus_rsc_device_type {
	struct device *mbox;
	struct list_head bcm_clist[VCD_MAX_CNT];
	int req_state;
	uint32_t acv[NUM_CTX];
	uint32_t query_acv[NUM_CTX];
	struct tcs_cmd *cmdlist_active;
	struct tcs_cmd *cmdlist_wake;
	struct tcs_cmd *cmdlist_sleep;
	int num_bcm_devs;
};

struct msm_bus_bcm_device_type {
	const char *name;
	uint32_t width;
	uint32_t clk_domain;
	uint32_t type;
	uint32_t unit_size;
	uint32_t addr;
	uint32_t drv_id;
	int num_bus_devs;
};

struct msm_bus_fab_device_type {
	void __iomem *qos_base;
	phys_addr_t pqos_base;
	size_t qos_range;
	uint32_t base_offset;
	uint32_t qos_freq;
	uint32_t qos_off;
	uint32_t sbm_offset;
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

struct msm_bus_noc_qos_params {
	uint32_t prio_dflt;
	struct msm_bus_noc_limiter limiter;
	bool limiter_en;
	struct msm_bus_noc_regulator reg;
	struct msm_bus_noc_regulator_mode reg_mode;
	bool urg_fwd_en;
	bool defer_init_qos;
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
	struct msm_bus_noc_qos_params qos_params;
	unsigned int num_connections;
	unsigned int num_blist;
	unsigned int num_bcm_devs;
	unsigned int num_rsc_devs;
	bool is_fab_dev;
	bool virt_dev;
	bool is_bcm_dev;
	bool is_rsc_dev;
	bool is_traversed;
	unsigned int *connections;
	unsigned int *bl_cons;
	unsigned int *bcm_dev_ids;
	unsigned int *rsc_dev_ids;
	struct device **dev_connections;
	struct device **black_connections;
	struct device **bcm_devs;
	struct device **rsc_devs;
	int *bcm_req_idx;
	unsigned int bus_device_id;
	struct device *bus_device;
	struct rule_update_path_info rule;
	uint64_t lim_bw;
	bool defer_qos;
	uint32_t *disable_ports;
	int num_disable_ports;
	struct node_agg_params_type agg_params;
};

struct msm_bus_node_device_type {
	struct msm_bus_node_info_type *node_info;
	struct msm_bus_fab_device_type *fabdev;
	struct msm_bus_bcm_device_type *bcmdev;
	struct msm_bus_rsc_device_type *rscdev;
	int num_lnodes;
	struct link_node *lnode_list;
	struct nodebw node_bw[NUM_CTX];
	struct nodevector node_vec[NUM_CTX];
	struct list_head link;
	struct list_head query_link;
	struct list_head dbg_link;
	struct nodeclk clk[NUM_CTX];
	struct nodeclk bus_qos_clk;
	uint32_t num_node_qos_clks;
	struct nodeclk *node_qos_clks;
	uint32_t num_qos_bcms;
	struct qos_bcm_type *qos_bcms;
	uint32_t num_regs;
	struct node_regulator *node_regs;
	unsigned int ap_owned;
	struct device_node *of_node;
	struct device dev;
	bool dirty;
	bool updated;
	bool query_dirty;
	struct list_head dev_link;
	struct list_head devlist;
	bool is_connected;
};

static inline struct msm_bus_node_device_type *to_msm_bus_node(struct device *d)
{
	return container_of(d, struct msm_bus_node_device_type, dev);
}


int msm_bus_enable_limiter(struct msm_bus_node_device_type *nodedev,
				int throttle_en, uint64_t lim_bw);
int msm_bus_commit_data(struct list_head *clist);
int bcm_remove_handoff_req(struct device *dev, void *data);
int commit_late_init_data(bool lock);
int msm_bus_query_gen(struct list_head *qlist,
				struct msm_bus_tcs_usecase *tcs_usecase);
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
