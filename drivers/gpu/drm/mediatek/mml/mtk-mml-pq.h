/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_MML_PQ_H__
#define __MTK_MML_PQ_H__

#include <linux/kernel.h>
#include <linux/types.h>

#include "mtk-mml.h"

struct mml_pq_rsz_tile_init_param {
	u32 coeff_step_x;
	u32 coeff_step_y;
	u32 precision_x;
	u32 precision_y;
	u32 crop_offset_x;
	u32 crop_subpix_x;
	u32 crop_offset_y;
	u32 crop_subpix_y;
	u32 hor_dir_scale;
	u32 hor_algorithm;
	u32 ver_dir_scale;
	u32 ver_algorithm;
	u32 vertical_first;
	u32 ver_cubic_trunc;
};

struct mml_pq_reg {
	u16 offset;
	u32 value;
	u32 mask;
};

struct mml_pq_tile_init_result {
	u8 rsz_param_cnt;
	struct mml_pq_rsz_tile_init_param *rsz_param;
	u32 rsz_reg_cnt;
	struct mml_pq_reg *rsz_regs;
};

struct mml_pq_tile_init_job {
	/* input from user-space */
	u32 result_job_id;
	struct mml_pq_tile_init_result *result;

	/* output to user-space */
	u32 new_job_id;
	struct mml_frame_info info;
	struct mml_pq_param param[MML_MAX_OUTPUTS];
};

#define MML_PQ_IOC_MAGIC 'W'
#define MML_PQ_IOC_TILE_INIT _IOWR(MML_PQ_IOC_MAGIC, 0,\
		struct mml_pq_tile_init_job)

#endif	/* __MTK_MML_PQ_H__ */
