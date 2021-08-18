/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __GED_GPU_BM_H__
#define __GED_GPU_BM_H__

#include <linux/types.h>

struct job_status_qos {
	phys_addr_t phyaddr;
	size_t size;
};

struct v1_data {
	unsigned int version;
	unsigned int ctx;
	unsigned int frame;
	unsigned int job;
	unsigned int freq;
};

int mtk_bandwidth_resource_init(void);
void mtk_bandwidth_update_info(int pid, int frame_nr, int job_id);
void mtk_bandwidth_check_SF(int pid, int isSF);
u32 qos_inc_frame_nr(void);
u32 qos_get_frame_nr(void);

#endif /* __GED_GPU_BM_H__ */

