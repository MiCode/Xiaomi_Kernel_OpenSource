/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __IMGSENSOR_CLK_H__
#define __IMGSENSOR_CLK_H__
#include "imgsensor_common.h"

#include <linux/atomic.h>
#include <linux/platform_device.h>
#include <kd_imgsensor_define.h>

#ifdef IMGSENSOR_DFS_CTRL_ENABLE
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#endif

enum IMGSENSOR_CCF {
	IMGSENSOR_CCF_MCLK_TG_MIN_NUM,
	IMGSENSOR_CCF_MCLK_TOP_CAMTG_SEL = IMGSENSOR_CCF_MCLK_TG_MIN_NUM,
	IMGSENSOR_CCF_MCLK_TOP_CAMTG1_SEL,
	IMGSENSOR_CCF_MCLK_TOP_CAMTG2_SEL,
	IMGSENSOR_CCF_MCLK_TOP_CAMTG3_SEL,
	IMGSENSOR_CCF_MCLK_TG_MAX_NUM,
	IMGSENSOR_CCF_MCLK_FREQ_MIN_NUM = IMGSENSOR_CCF_MCLK_TG_MAX_NUM,
	IMGSENSOR_CCF_MCLK_SRC_6M = IMGSENSOR_CCF_MCLK_FREQ_MIN_NUM,
	IMGSENSOR_CCF_MCLK_SRC_12M,
	IMGSENSOR_CCF_MCLK_SRC_13M,
	IMGSENSOR_CCF_MCLK_SRC_24M,
	IMGSENSOR_CCF_MCLK_SRC_26M,
	IMGSENSOR_CCF_MCLK_SRC_48M,
	IMGSENSOR_CCF_MCLK_SRC_52M,
	IMGSENSOR_CCF_MCLK_FREQ_MAX_NUM,

	IMGSENSOR_CCF_CG_MIN_NUM = IMGSENSOR_CCF_MCLK_FREQ_MAX_NUM,
	IMGSENSOR_CCF_CG_SENINF = IMGSENSOR_CCF_CG_MIN_NUM,
	IMGSENSOR_CCF_CG_MIPIC0_26M,
	IMGSENSOR_CCF_CG_MIPIC1_26M,
	IMGSENSOR_CCF_CG_MIPI_ANA_0A,
	IMGSENSOR_CCF_CG_CAMTM_SEL,
	IMGSENSOR_CCF_CG_CAMTM_SRC_208,
	IMGSENSOR_CCF_CG_MAX_NUM,
	IMGSENSOR_CCF_MTCMOS_MIN_NUM = IMGSENSOR_CCF_CG_MAX_NUM,
#ifndef IMGSENSOR_USE_RPM
	IMGSENSOR_CCF_MTCMOS_CAM = IMGSENSOR_CCF_MTCMOS_MIN_NUM,
	IMGSENSOR_CCF_MTCMOS_MAX_NUM,
#else
	IMGSENSOR_CCF_MTCMOS_MAX_NUM = IMGSENSOR_CCF_MTCMOS_MIN_NUM,
#endif
	IMGSENSOR_CCF_MAX_NUM = IMGSENSOR_CCF_MTCMOS_MAX_NUM,
};

struct IMGSENSOR_CLK {
	struct clk *imgsensor_ccf[IMGSENSOR_CCF_MAX_NUM];
	atomic_t    enable_cnt[IMGSENSOR_CCF_MAX_NUM];
#if IS_ENABLED(CONFIG_PM_SLEEP)
	struct wakeup_source *seninf_wake_lock;
#endif
};

#ifdef IMGSENSOR_DFS_CTRL_ENABLE
struct imgsensor_dfs_ctx {
	struct device *dev;
	struct regulator *reg;
	unsigned long *freqs;
	unsigned long *volts;
	int cnt;
};
enum DFS_OPTION {
	DFS_CTRL_ENABLE,
	DFS_CTRL_DISABLE,
	DFS_UPDATE,
	DFS_RELEASE,
	DFS_SUPPORTED_ISP_CLOCKS,
	DFS_CUR_ISP_CLOCK,
};
int imgsensor_dfs_init(struct imgsensor_dfs_ctx *ctx, struct device *dev);
void imgsensor_dfs_exit(struct imgsensor_dfs_ctx *ctx);
int imgsensor_dfs_ctrl(struct imgsensor_dfs_ctx *ctx, enum DFS_OPTION option, void *pbuff);
#endif
extern unsigned int mt_get_ckgen_freq(int ID);
enum IMGSENSOR_RETURN imgsensor_clk_init(struct IMGSENSOR_CLK *pclk);
#ifdef SENINF_USE_WAKE_LOCK
enum IMGSENSOR_RETURN imgsensor_clk_exit(struct IMGSENSOR_CLK *pclk);
enum IMGSENSOR_RETURN seninf_wake_lock_get(struct IMGSENSOR_CLK *pclk);
enum IMGSENSOR_RETURN seninf_wake_lock_put(struct IMGSENSOR_CLK *pclk);
#endif
int  imgsensor_clk_set(
	struct IMGSENSOR_CLK *pclk, struct  ACDK_SENSOR_MCLK_STRUCT *pmclk);

void imgsensor_clk_enable_all(struct IMGSENSOR_CLK *pclk);
void imgsensor_clk_disable_all(struct IMGSENSOR_CLK *pclk);
int imgsensor_clk_ioctrl_handler(void *pbuff);
extern struct platform_device *gpimgsensor_hw_platform_device;

#endif

