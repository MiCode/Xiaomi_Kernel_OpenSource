/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __SENINF_CLK_H__
#define __SENINF_CLK_H__

#include <linux/device.h>
#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif

#include <linux/atomic.h>
#include <linux/platform_device.h>

#include "kd_imgsensor_define.h"

#include "seninf_common.h"

#ifdef CONFIG_FPGA_EARLY_PORTING
#define SENINF_CLK_CONTROL 0
#else
#define SENINF_CLK_CONTROL 1
#endif

#define IMGSENSOR_DFS_CTRL_ENABLE
#ifdef IMGSENSOR_DFS_CTRL_ENABLE
#include <linux/pm_qos.h>
#include <mmdvfs_pmqos.h>

enum DFS_OPTION {
	DFS_CTRL_ENABLE,
	DFS_CTRL_DISABLE,
	DFS_UPDATE,
	DFS_RELEASE,
	DFS_SUPPORTED_ISP_CLOCKS,
	DFS_CUR_ISP_CLOCK,
};
extern int imgsensor_dfs_ctrl(enum DFS_OPTION option, void *pbuff);
#endif

enum SENINF_CLK_IDX_SYS {
	SENINF_CLK_IDX_SYS_MIN_NUM = 0,
	SENINF_CLK_IDX_SYS_SCP_SYS_MDP = SENINF_CLK_IDX_SYS_MIN_NUM,
	SENINF_CLK_IDX_SYS_SCP_SYS_CAM,
	SENINF_CLK_IDX_SYS_CAMSYS_SENINF_CGPDN,
	SENINF_CLK_IDX_SYS_TOP_MUX_SENINF,
	SENINF_CLK_IDX_SYS_TOP_MUX_SENINF1,
	SENINF_CLK_IDX_SYS_TOP_MUX_SENINF2,
	SENINF_CLK_IDX_SYS_TOP_MUX_SENINF3,
	SENINF_CLK_IDX_SYS_MAX_NUM
};

enum SENINF_CLK_IDX_TG {
	SENINF_CLK_IDX_TG_MIN_NUM = SENINF_CLK_IDX_SYS_MAX_NUM,
	SENINF_CLK_IDX_TG_TOP_MUX_CAMTG = SENINF_CLK_IDX_TG_MIN_NUM,
	SENINF_CLK_IDX_TG_TOP_MUX_CAMTG2,
	SENINF_CLK_IDX_TG_TOP_MUX_CAMTG3,
	SENINF_CLK_IDX_TG_TOP_MUX_CAMTG4,
	SENINF_CLK_IDX_TG_TOP_MUX_CAMTG5,
	SENINF_CLK_IDX_TG_TOP_MUX_CAMTG6,
	SENINF_CLK_IDX_TG_MAX_NUM
};

enum SENINF_CLK_IDX_FREQ {
	SENINF_CLK_IDX_FREQ_MIN_NUM = SENINF_CLK_IDX_TG_MAX_NUM,
	SENINF_CLK_IDX_FREQ_TOP_UNIVP_192M_D32 = SENINF_CLK_IDX_FREQ_MIN_NUM,
	SENINF_CLK_IDX_FREQ_TOP_UNIVP_192M_D16,
	SENINF_CLK_IDX_FREQ_TOP_F26M_CK_D2,
	SENINF_CLK_IDX_FREQ_TOP_UNIVP_192M_D8,
	SENINF_CLK_IDX_FREQ_TOP_CLK26M,
	SENINF_CLK_IDX_FREQ_TOP_UNIVP_192M_D4,
	SENINF_CLK_IDX_FREQ_TOP_UNIVPLL_D3_D8,
	SENINF_CLK_IDX_FREQ_MAX_NUM
};

enum SENINF_CLK_TG {
	SENINF_CLK_TG_0,
	SENINF_CLK_TG_1,
	SENINF_CLK_TG_2,
	SENINF_CLK_TG_3,
	SENINF_CLK_TG_4,
	SENINF_CLK_TG_5,
	SENINF_CLK_TG_MAX_NUM
};

enum SENINF_CLK_MCLK_FREQ {
	SENINF_CLK_MCLK_FREQ_6MHZ = 6,
	SENINF_CLK_MCLK_FREQ_12MHZ = 12,
	SENINF_CLK_MCLK_FREQ_13MHZ = 13,
	SENINF_CLK_MCLK_FREQ_24MHZ = 24,
	SENINF_CLK_MCLK_FREQ_26MHZ = 26,
	SENINF_CLK_MCLK_FREQ_48MHZ = 48,
	SENINF_CLK_MCLK_FREQ_52MHZ = 52
};

#define SENINF_CLK_MCLK_FREQ_MAX    SENINF_CLK_MCLK_FREQ_52MHZ
#define SENINF_CLK_MCLK_FREQ_MIN    SENINF_CLK_MCLK_FREQ_6MHZ
#define SENINF_CLK_IDX_MIN_NUM      SENINF_CLK_IDX_SYS_MIN_NUM
#define SENINF_CLK_IDX_MAX_NUM      SENINF_CLK_IDX_FREQ_MAX_NUM
#define SENINF_CLK_IDX_FREQ_IDX_NUM \
(SENINF_CLK_IDX_FREQ_MAX_NUM - SENINF_CLK_IDX_FREQ_MIN_NUM)

struct SENINF_CLK_CTRL {
	char *pctrl;
};

struct SENINF_CLK {
	struct platform_device *pplatform_device;
	struct clk *mclk_sel[SENINF_CLK_IDX_MAX_NUM];
	atomic_t enable_cnt[SENINF_CLK_IDX_MAX_NUM];
	atomic_t wakelock_cnt;

#ifdef CONFIG_PM_WAKELOCKS
	struct wakeup_source seninf_wake_lock;
#else
	struct wake_lock seninf_wake_lock;
#endif
};

enum SENINF_RETURN seninf_clk_init(struct SENINF_CLK *pclk);
int seninf_clk_set(
	struct SENINF_CLK *pclk, struct ACDK_SENSOR_MCLK_STRUCT *pmclk);
void seninf_clk_open(struct SENINF_CLK *pclk);
void seninf_clk_release(struct SENINF_CLK *pclk);
unsigned int seninf_clk_get_meter(struct SENINF_CLK *pclk, unsigned int clk);

extern unsigned int mt_get_ckgen_freq(int ID);
extern unsigned long clk_get_rate(struct clk *clk);

#endif

