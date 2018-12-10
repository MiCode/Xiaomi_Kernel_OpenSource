/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2016, 2018, The Linux Foundation. All rights reserved.
 */

#ifndef _ARCH_ARM_MACH_MSM_BUS_BIMC_H
#define _ARCH_ARM_MACH_MSM_BUS_BIMC_H

enum msm_bus_noc_qos_mode_type {
	NOC_QOS_MODE_FIXED = 0,
	NOC_QOS_MODE_LIMITER,
	NOC_QOS_MODE_BYPASS,
	NOC_QOS_MODE_REGULATOR,
	NOC_QOS_MODE_MAX,
};

enum msm_bus_noc_qos_mode_perm {
	NOC_QOS_PERM_MODE_FIXED = (1 << NOC_QOS_MODE_FIXED),
	NOC_QOS_PERM_MODE_LIMITER = (1 << NOC_QOS_MODE_LIMITER),
	NOC_QOS_PERM_MODE_BYPASS = (1 << NOC_QOS_MODE_BYPASS),
	NOC_QOS_PERM_MODE_REGULATOR = (1 << NOC_QOS_MODE_REGULATOR),
};

#define NOC_QOS_MODES_ALL_PERM (NOC_QOS_PERM_MODE_FIXED | \
	NOC_QOS_PERM_MODE_LIMITER | NOC_QOS_PERM_MODE_BYPASS | \
	NOC_QOS_PERM_MODE_REGULATOR)

struct msm_bus_noc_commit {
	struct msm_bus_node_hw_info *mas;
	struct msm_bus_node_hw_info *slv;
};

struct msm_bus_noc_info {
	void __iomem *base;
	uint32_t base_addr;
	uint32_t nmasters;
	uint32_t nqos_masters;
	uint32_t nslaves;
	uint32_t qos_freq; /* QOS Clock in KHz */
	uint32_t qos_baseoffset;
	uint32_t qos_delta;
	uint32_t *mas_modes;
	uint32_t sbm_offset;
	struct msm_bus_noc_commit cdata[NUM_CTX];
};

struct msm_bus_noc_qos_priority {
	uint32_t high_prio;
	uint32_t low_prio;
	uint32_t read_prio;
	uint32_t write_prio;
	uint32_t p1;
	uint32_t p0;
};

struct msm_bus_noc_qos_bw {
	uint64_t bw; /* Bandwidth in bytes per second */
	uint32_t ws; /* Window size in nano seconds */
};

void msm_bus_noc_init(struct msm_bus_noc_info *ninfo);
uint8_t msm_bus_noc_get_qos_mode(void __iomem *base, uint32_t qos_off,
	uint32_t mport, uint32_t qos_delta, uint32_t mode, uint32_t perm_mode);
void msm_bus_noc_get_qos_priority(void __iomem *base, uint32_t qos_off,
	uint32_t mport, uint32_t qos_delta,
	struct msm_bus_noc_qos_priority *qprio);
void msm_bus_noc_get_qos_bw(void __iomem *base, uint32_t qos_off,
	uint32_t qos_freq, uint32_t mport, uint32_t qos_delta,
	uint8_t perm_mode, struct msm_bus_noc_qos_bw *qbw);
#endif /*_ARCH_ARM_MACH_MSM_BUS_NOC_H */
