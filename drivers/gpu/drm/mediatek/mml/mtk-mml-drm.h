/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#ifndef __MTK_MML_DRM_H__
#define __MTK_MML_DRM_H__

struct mml_task;

enum mml_mutex_timing {
	MML_MUTEX_DEFAULT_TIMING = 0,
	MML_MUTEX_VSYNC_FALLING,	/* SOF default timing */
	MML_MUTEX_VSYNC_FALLING_WAIT,
	MML_MUTEX_VSYNC_RISING,
	MML_MUTEX_VSYNC_RISING_WAIT,
	MML_MUTEX_VDE_FALLING,		/* EOF default timing */
	MML_MUTEX_VDE_FALLING_WAIT,
	MML_MUTEX_FRAME_DONE,
	MML_MUTEX_FRAME_DONE_WAIT,
};

struct mml_mutex_ctl {
	bool is_cmd_mode;
	u32 sof_src;	/* enum mtk_ddp_comp_id */
	enum mml_mutex_timing sof_timing;
	u32 eof_src;	/* enum mtk_ddp_comp_id */
	enum mml_mutex_timing eof_timing;
};

#define MML_DL_OUT_CNT	2

#endif	/* __MTK_MML_DRM_H__ */
