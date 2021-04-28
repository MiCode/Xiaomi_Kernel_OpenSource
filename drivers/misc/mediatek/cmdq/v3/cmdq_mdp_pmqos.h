/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#ifndef __CMDQ_MDP_PMQOS_H__
#define __CMDQ_MDP_PMQOS_H__

#define PMQOS_ISP_PORT_NUM  16
#define PMQOS_MDP_PORT_NUM  4

struct mdp_pmqos {
	uint32_t isp_total_datasize;
	uint32_t isp_total_pixel;
	uint32_t mdp_total_datasize;
	uint32_t mdp_total_pixel;

	uint32_t qos2_isp_bandwidth[PMQOS_ISP_PORT_NUM];
	uint32_t qos2_isp_total_pixel[PMQOS_ISP_PORT_NUM];
	uint32_t qos2_isp_port[PMQOS_ISP_PORT_NUM];
	uint32_t qos2_isp_count;

	uint32_t qos2_mdp_bandwidth[PMQOS_MDP_PORT_NUM];
	uint32_t qos2_mdp_total_pixel[PMQOS_MDP_PORT_NUM];
	uint32_t qos2_mdp_port[PMQOS_MDP_PORT_NUM];
	uint32_t qos2_mdp_count;

	uint64_t tv_sec;
	uint64_t tv_usec;

	uint64_t ispMetString;
	uint32_t ispMetStringSize;
	uint64_t mdpMetString;
	uint32_t mdpMetStringSize;

	uint64_t mdpMMpathString;
	uint32_t mdpMMpathStringSize;
};
#endif

