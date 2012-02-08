/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef _ARCH_ARM_MACH_MSM_BUS_BIMC_H
#define _ARCH_ARM_MACH_MSM_BUS_BIMC_H

struct msm_bus_bimc_params {
	uint32_t bus_id;
	uint32_t addr_width;
	uint32_t data_width;
	uint32_t nmasters;
	uint32_t nslaves;
};

struct msm_bus_bimc_commit {
	struct msm_bus_node_hw_info *mas;
	struct msm_bus_node_hw_info *slv;
};

struct msm_bus_bimc_info {
	void __iomem *base;
	uint32_t base_addr;
	uint32_t qos_freq;
	struct msm_bus_bimc_params params;
	struct msm_bus_bimc_commit cdata[NUM_CTX];
};

struct msm_bus_bimc_node {
	uint32_t conn_mask;
	uint32_t data_width;
	uint8_t slv_arb_mode;
};

enum msm_bus_bimc_arb_mode {
	BIMC_ARB_MODE_RR = 0,
	BIMC_ARB_MODE_PRIORITY_RR,
	BIMC_ARB_MODE_TIERED_RR,
};


enum msm_bus_bimc_interleave {
	BIMC_INTERLEAVE_NONE = 0,
	BIMC_INTERLEAVE_ODD,
	BIMC_INTERLEAVE_EVEN,
};

struct msm_bus_bimc_slave_seg {
	bool enable;
	uint64_t start_addr;
	uint64_t seg_size;
	uint8_t interleave;
};

enum msm_bus_bimc_qos_mode_type {
	BIMC_QOS_MODE_FIXED = 0,
	BIMC_QOS_MODE_LIMITER,
	BIMC_QOS_MODE_BYPASS,
	BIMC_QOS_MODE_REGULATOR,
};

struct msm_bus_bimc_qos_health {
	bool limit_commands;
	uint32_t areq_prio;
	uint32_t prio_level;
};

struct msm_bus_bimc_mode_fixed {
	uint32_t prio_level;
	uint32_t areq_prio_rd;
	uint32_t areq_prio_wr;
};

struct msm_bus_bimc_mode_rl {
	uint8_t qhealthnum;
	struct msm_bus_bimc_qos_health qhealth[4];
};

struct msm_bus_bimc_qos_mode {
	uint8_t mode;
	struct msm_bus_bimc_mode_fixed fixed;
	struct msm_bus_bimc_mode_rl rl;
};

struct msm_bus_bimc_qos_bw {
	uint64_t bw;	/* bw is in Bytes/sec */
	uint32_t ws;	/* Window size in nano seconds*/
	uint64_t thh;	/* Threshold high, bytes per second */
	uint64_t thm;	/* Threshold medium, bytes per second */
	uint64_t thl;	/* Threshold low, bytes per second */
};

struct msm_bus_bimc_clk_gate {
	bool core_clk_gate_en;
	bool arb_clk_gate_en;	/* For arbiter */
	bool port_clk_gate_en;	/* For regs on BIMC core clock */
};

void msm_bus_bimc_set_slave_seg(struct msm_bus_bimc_info *binfo,
	uint32_t slv_index, uint32_t seg_index,
	struct msm_bus_bimc_slave_seg *bsseg);
void msm_bus_bimc_set_slave_clk_gate(struct msm_bus_bimc_info *binfo,
	uint32_t slv_index, struct msm_bus_bimc_clk_gate *bgate);
void msm_bus_bimc_set_mas_clk_gate(struct msm_bus_bimc_info *binfo,
	uint32_t mas_index, struct msm_bus_bimc_clk_gate *bgate);
void msm_bus_bimc_arb_en(struct msm_bus_bimc_info *binfo,
	uint32_t slv_index, bool en);
void msm_bus_bimc_get_params(struct msm_bus_bimc_info *binfo,
	struct msm_bus_bimc_params *params);
void msm_bus_bimc_get_mas_params(struct msm_bus_bimc_info *binfo,
	uint32_t mas_index, struct msm_bus_bimc_node *mparams);
void msm_bus_bimc_get_slv_params(struct msm_bus_bimc_info *binfo,
	uint32_t slv_index, struct msm_bus_bimc_node *sparams);
bool msm_bus_bimc_get_arb_en(struct msm_bus_bimc_info *binfo,
	uint32_t slv_index);

#endif /*_ARCH_ARM_MACH_MSM_BUS_BIMC_H*/
