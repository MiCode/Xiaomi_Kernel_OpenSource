/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __N3D_CLK_H__
#define __N3D_CLK_H__

#include <linux/device.h>
#ifdef CONFIG_PM_SLEEP
#include <linux/pm_wakeup.h>
#endif

#include <linux/atomic.h>
#include <linux/platform_device.h>

enum SENINF_CLK_IDX_SYS {
	N3D_CLK_IDX_SYS_MIN_NUM = 0,
	SENINF_CLK_IDX_SYS_SCP_SYS_MDP = N3D_CLK_IDX_SYS_MIN_NUM,
	SENINF_CLK_IDX_SYS_SCP_SYS_CAM,
	N3D_CLK_IDX_SYS_CAMSYS_SENINF_CGPDN,
	N3D_CLK_IDX_SYS_CAMSYS_CAMTG_CGPDN,
	N3D_CLK_IDX_SYS_MAX_NUM
};

struct SENINF_N3D_CLK_CTRL {
	char *pctrl;
};

struct SENINF_N3D_CLK {
	struct platform_device *pplatform_device;
	struct clk *clk_sel[N3D_CLK_IDX_SYS_MAX_NUM];
	atomic_t enable_cnt[N3D_CLK_IDX_SYS_MAX_NUM];
	atomic_t wakelock_cnt;

#ifdef CONFIG_PM_SLEEP
	struct wakeup_source *n3d_wake_lock;
#endif
};

void n3d_clk_init(struct SENINF_N3D_CLK *pclk);
void n3d_clk_exit(struct SENINF_N3D_CLK *pclk);
void n3d_clk_open(struct SENINF_N3D_CLK *pclk);
void n3d_clk_release(struct SENINF_N3D_CLK *pclk);

#endif

