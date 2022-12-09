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

#define SYSRAM_LOG_SIZE sizeof(int)

enum gpu_bm_counter {
	BM_COUNTER_CURRENT_VERSION          = 0,
	BM_COUNTER_CTX                      = 1,
	BM_COUNTER_FRAME                    = 2,
	BM_COUNTER_JOB                      = 3,
	BM_COUNTER_FREQ                     = 4,

	NR_BM_COUNTER
};

/* 6983 0x112000~0x112400 */
#define FASTDVFS_POWERMODEL_SYSRAM_BASE 0x112000U

#define SYSRAM_GPUBM_CURRENT_VERSION                        \
(                                                    \
(BM_COUNTER_CURRENT_VERSION*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPUBM_CTX                        \
(                                                    \
(BM_COUNTER_CTX*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPUBM_FRAME                        \
(                                                    \
(BM_COUNTER_FRAME*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPUBM_JOB                       \
(                                                    \
(BM_COUNTER_JOB*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPUBM_FREQ                       \
(                                                    \
(BM_COUNTER_FREQ*SYSRAM_LOG_SIZE) \
)


int mtk_bandwidth_resource_init(void);
void mtk_bandwidth_update_info(int pid, int frame_nr, int job_id);
void mtk_bandwidth_check_SF(int pid, int isSF);
u32 qos_inc_frame_nr(void);
u32 qos_get_frame_nr(void);

#endif /* __GED_GPU_BM_H__ */

