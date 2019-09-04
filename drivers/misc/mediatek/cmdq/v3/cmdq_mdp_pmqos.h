/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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

