/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __GED_FDVFS_H__
#define __GED_FDVFS_H__

/* ****************************************** */

#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/spinlock.h>
#include <linux/of.h>
#include <linux/of_address.h>

/* ****************************************** */

#include <mtk_gpu_utility.h>

/* ****************************************** */

#include "ged_type.h"
#include "ged_base.h"
#include "ged_log.h"

/* #include "ged_dvfs.h" */

/* ****************************************** */

GED_ERROR ged_fdvfs_system_init(void);
void ged_fdvfs_exit(void);

/* ****************************************** */

extern int mt_gpufreq_get_idx_by_target_freq(unsigned int target_freq);
extern unsigned int mt_gpufreq_get_cur_freq(void);

extern void mtk_gpu_gas_hint(unsigned int);
extern void mtk_gpu_touch_hint(unsigned int);

extern void mtk_gpu_freq_hint(unsigned int val);
extern bool mtk_gpu_get_freq_hint(unsigned int *val);


extern phys_addr_t gpu_fdvfs_virt_addr;



/* ****************************************** */

#define GED_FDVFS_TIMER_TIMEOUT 1000000

/* ****************************************** */

#define base_read(addr)             (*(volatile uint64_t *)(addr))
#define MFG_read(addr)              base_read(g_MFG_base+addr)

#define RGX_PERF_TA_CYCLES 0x0000000000006020
#define RGX_PERF_3D_CYCLES 0x0000000000006028
#define RGX_PERF_TA_OR_3D_CYCLES 0x0000000000006038

enum {
	MTK_CNT_TA_OR_3D,
	MTK_CNT_3D,
};

/* ****************************************** */

typedef enum GED_FDVFS_COMMIT_TAG {
	GED_FDVFS_DEFAULT_COMMIT,
} GED_FDVFS_COMMIT_TYPE;

typedef struct GED_FDVFS_COUNTER_TAG {
	const char *name;
	uint32_t offset;
	unsigned int sum;
	unsigned int val_pre;
	unsigned int diff_pre;
	int overflow;
	int tick_time;
} GED_FDVFS_COUNTER;

typedef struct GED_FDVFS_WORK_TAG {
	struct work_struct sWork;
} GED_FDVFS_WORK;


enum {
	GED_FDVFS_DVFS_HINT = 0,
	GED_KPI_FPS_HINT,
	GED_KPI_ACCM_HINT,
	GED_GAS_TOUCH_HINT,
	GED_FREQ_HINT,
};


enum {
	GED_FDVFS_GPU_ACTIVE = 10,
	GED_FDVFS_JS0_ACTIVE,
	GED_FDVFS_LEVEL_STATE,
	GED_FDVFS_CHK_CNT,
	GED_FDVFS_CYCLE_SUM,
	GED_FDVFS_GPU_UTILITY,
	GED_FDVFS_PREDICT_FREQ,
	GED_FDVFS_PREDICT_MODE,
	GED_FDVFS_DO_DVFS,
	GED_FDVFS_CYCLE_COUNT_0,
	GED_FDVFS_CYCLE_COUNT_1,
	GED_FDVFS_CYCLE_COUNT_2,
	GED_FDVFS_CYCLE_COUNT_3,
	GED_FDVFS_GPU_FREQ,
	GED_FDVFS_GPU_CUR_IDX,
	GED_FDVFS_GPU_SCALE,
	GED_FDVFS_GPU_SCALE_X_100,
	GED_FDVFS_GPU_SCALE_X_100_ROUNDING,
	GED_FDVFS_DO_COMPUTE,
	GED_FDVFS_FRAME_DONE,
	GED_FDVFS_FREQ_HINT,
	GED_FDVFS_PREDICT_CYCLE,
	GED_FDVFS_POWER_STATUS,
	GED_FDVFS_GPU_UTILITY2,
};

enum {
	GED_DEBUG_ACTIVE_BUFFER0_0 = 30,
	GED_DEBUG_ACTIVE_BUFFER1_0 = 46,
	GED_DEBUG_ACTIVE_BUFFER2_0 = 62,
	GED_DEBUG_ACTIVE_BUFFER3_0 = 78,
	GED_DEBUG_JS0_ACTIVE0_0 = 94,
	GED_DEBUG_JS0_ACTIVE1_0 = 110,
	GED_DEBUG_JS0_ACTIVE2_0 = 126,
	GED_DEBUG_JS0_ACTIVE3_0 = 142,
};



#endif /* __GED_FDVFS_H__ */
