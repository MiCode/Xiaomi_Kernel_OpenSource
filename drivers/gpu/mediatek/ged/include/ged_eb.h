/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GED_EB_H__
#define __GED_EB_H__

#include <linux/types.h>
#include "ged_type.h"
#include "ged_dvfs.h"

/**************************************************
 * GPU FAST DVFS Log Setting
 **************************************************/
#define GED_FAST_DVFS_TAG "[GPU/FDVFS]"
#define GPUFDVFS_LOGE(fmt, args...) \
	pr_info(GED_FAST_DVFS_TAG"[ERROR]@%s: "fmt"\n", __func__, ##args)
#define GPUFDVFS_LOGW(fmt, args...) \
	pr_debug(GED_FAST_DVFS_TAG"[WARN]@%s: "fmt"\n", __func__, ##args)
#define GPUFDVFS_LOGI(fmt, args...) \
	pr_info(GED_FAST_DVFS_TAG"[INFO]@%s: "fmt"\n", __func__, ##args)
#define GPUFDVFS_LOGD(fmt, args...) \
	pr_debug(GED_FAST_DVFS_TAG"[DEBUG]@%s: "fmt"\n", __func__, ##args)


/**************************************************
 * GPU FAST DVFS SYSRAM Setting
 **************************************************/
#define SYSRAM_LOG_SIZE sizeof(int)

enum gpu_fastdvfs_counter {
	FASTDVFS_COUNTER_CURRENT_FREQUENCY          = 0,
	FASTDVFS_COUNTER_PREDICTED_FREQUENCY        = 1,
	FASTDVFS_COUNTER_FINISHED_WORKLOAD          = 2,
	FASTDVFS_COUNTER_PREDICTED_WORKLOAD         = 3,
	FASTDVFS_COUNTER_FRAGMENT_LOADING           = 4,
	FASTDVFS_COUNTER_KERNEL_FRAME_DONE_INTERVAL = 5,
	FASTDVFS_COUNTER_EB_FRAME_DONE_INTERVAL     = 6,
	FASTDVFS_COUNTER_TARGET_TIME                = 7,
	FASTDVFS_COUNTER_FRAME_BOUNDARY             = 8,
	FASTDVFS_COUNTER_LEFT_WL                    = 9,
	FASTDVFS_COUNTER_ELAPSED_TIME               = 10,
	FASTDVFS_COUNTER_LEFT_TIME                  = 11,
	FASTDVFS_COUNTER_FRAME_END_HINT_COUNT       = 12,
	FASTDVFS_COUNTER_UNDER_HINT_WL              = 13,
	FASTDVFS_COUNTER_UNDER_HINT_CNT             = 14,

	NR_FASTDVFS_COUNTER
};

/* 6983 0x112000~0x112400 */
#define FASTDVFS_POWERMODEL_SYSRAM_BASE 0x112000U

#define SYSRAM_GPU_CURR_FREQ                         \
(                                                    \
(FASTDVFS_COUNTER_CURRENT_FREQUENCY*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_PRED_FREQ                           \
(                                                      \
(FASTDVFS_COUNTER_PREDICTED_FREQUENCY*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_FINISHED_WORKLOAD                 \
(                                                    \
(FASTDVFS_COUNTER_FINISHED_WORKLOAD*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_PRED_WORKLOAD                      \
(                                                     \
(FASTDVFS_COUNTER_PREDICTED_WORKLOAD*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_FRAGMENT_LOADING                 \
(                                                   \
(FASTDVFS_COUNTER_FRAGMENT_LOADING*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_KERNEL_FRAME_DONE_INTERVAL                 \
(                                                             \
(FASTDVFS_COUNTER_KERNEL_FRAME_DONE_INTERVAL*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_EB_FRAME_DONE_INTERVAL                 \
(                                                         \
(FASTDVFS_COUNTER_EB_FRAME_DONE_INTERVAL*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_TARGET_TIME                 \
(                                              \
(FASTDVFS_COUNTER_TARGET_TIME*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_TARGET_FRAME_BOUNDARY          \
(                                                 \
(FASTDVFS_COUNTER_FRAME_BOUNDARY*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_LEFT_WL                 \
(                                          \
(FASTDVFS_COUNTER_LEFT_WL*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_ELAPSED_TIME                 \
(                                               \
(FASTDVFS_COUNTER_ELAPSED_TIME*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_LEFT_TIME                 \
(                                            \
(FASTDVFS_COUNTER_LEFT_TIME*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_FRAME_END_HINT_CNT                   \
(                                                       \
(FASTDVFS_COUNTER_FRAME_END_HINT_COUNT*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_UNDER_HINT_WL                 \
(                                                \
(FASTDVFS_COUNTER_UNDER_HINT_WL*SYSRAM_LOG_SIZE) \
)
#define SYSRAM_GPU_UNDER_HINT_CNT                 \
(                                                 \
(FASTDVFS_COUNTER_UNDER_HINT_CNT*SYSRAM_LOG_SIZE) \
)

/**************************************************
 * GPU FAST DVFS IPI CMD
 **************************************************/

#define FASTDVFS_IPI_TIMEOUT 2000 //ms

enum {
	GPUFDVFS_IPI_SET_FRAME_DONE         = 1,
	GPUFDVFS_IPI_GET_FRAME_LOADING      = 2,
	GPUFDVFS_IPI_SET_NEW_FREQ           = 3,
	GPUFDVFS_IPI_GET_CURR_FREQ          = 4,
	GPUFDVFS_IPI_PMU_START              = 5,
	GPUFDVFS_IPI_SET_FRAME_BASE_DVFS    = 6,
	GPUFDVFS_IPI_SET_TARGET_FRAME_TIME  = 7,
	GPUFDVFS_IPI_SET_FRAG_DONE_INTERVAL = 8,
	GPUFDVFS_IPI_SET_MODE               = 9,

	NR_GPUFDVFS_IPI,
};

/* IPI data structure */
struct fdvfs_ipi_data {
	unsigned int cmd;
	union {
		struct {
			unsigned int arg[5];
		} set_para;
	} u;
};


/**************************************************
 * Definition
 **************************************************/
#define FDVFS_IPI_DATA_LEN \
	DIV_ROUND_UP(sizeof(struct fdvfs_ipi_data), MBOX_SLOT_SIZE)

extern void fdvfs_init(void);
extern int ged_to_fdvfs_command(unsigned int cmd,
	struct fdvfs_ipi_data *fdvfs_d);


/**************************************************
 * GPU FAST DVFS EXPORTED API
 **************************************************/
extern int mtk_gpueb_dvfs_set_frame_done(void);
extern unsigned int mtk_gpueb_dvfs_get_cur_freq(void);
extern unsigned int mtk_gpueb_dvfs_get_frame_loading(void);
extern void mtk_gpueb_dvfs_commit(unsigned long ui32NewFreqID,
		GED_DVFS_COMMIT_TYPE eCommitType, int *pbCommited);
extern unsigned int mtk_gpueb_dvfs_set_frame_base_dvfs(unsigned int enable);
extern int
	mtk_gpueb_dvfs_set_taget_frame_time(unsigned int target_frame_time);
extern unsigned int
	mtk_gpueb_dvfs_set_frag_done_interval(int frag_done_interval_in_ns);
extern unsigned int mtk_gpueb_dvfs_get_mode(unsigned int *pAction);
extern unsigned int mtk_gpueb_dvfs_set_mode(unsigned int action);

extern int fastdvfs_proc_init(void);
extern void fastdvfs_proc_exit(void);

#endif // __GED_EB_H__
