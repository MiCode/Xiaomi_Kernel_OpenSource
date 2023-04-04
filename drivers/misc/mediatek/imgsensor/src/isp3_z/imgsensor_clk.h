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


enum IMGSENSOR_CCF {
	IMGSENSOR_CCF_MCLK_TG_MIN_NUM,
	IMGSENSOR_CCF_MCLK_TOP_CAMTG_SEL = IMGSENSOR_CCF_MCLK_TG_MIN_NUM,
	IMGSENSOR_CCF_MCLK_TOP_CAMTG2_SEL,
	IMGSENSOR_CCF_MCLK_TG_MAX_NUM,

	IMGSENSOR_CCF_MCLK_FREQ_MIN_NUM = IMGSENSOR_CCF_MCLK_TG_MAX_NUM,
	IMGSENSOR_CCF_MCLK_TOP_CLK26M = IMGSENSOR_CCF_MCLK_FREQ_MIN_NUM,
	IMGSENSOR_CCF_MCLK_TOP_UNIVPLL_48M_D2,
	IMGSENSOR_CCF_MCLK_TOP_UNIVPLL2_D8,
	IMGSENSOR_CCF_MCLK_TOP_UNIVPLL_D26,
	IMGSENSOR_CCF_MCLK_TOP_UNIVPLL2_D32,
	IMGSENSOR_CCF_MCLK_TOP_UNIVPLL_48M_D4,
	IMGSENSOR_CCF_MCLK_TOP_UNIVPLL_48M_D8,
	IMGSENSOR_CCF_MCLK_FREQ_MAX_NUM,

	IMGSENSOR_CCF_CG_MIN_NUM = IMGSENSOR_CCF_MCLK_FREQ_MAX_NUM,
	IMGSENSOR_CCF_CG_SENINF_SEL = IMGSENSOR_CCF_CG_MIN_NUM,
	IMGSENSOR_CCF_CG_SCAM_SEL,
	IMGSENSOR_CCF_CG_MAX_NUM,
	IMGSENSOR_CCF_MAX_NUM = IMGSENSOR_CCF_CG_MAX_NUM,
};

struct IMGSENSOR_CLK {
	struct clk *imgsensor_ccf[IMGSENSOR_CCF_MAX_NUM];
	atomic_t    enable_cnt[IMGSENSOR_CCF_MAX_NUM];
};

extern unsigned int mt_get_ckgen_freq(int ID);
enum IMGSENSOR_RETURN imgsensor_clk_init(struct IMGSENSOR_CLK *pclk);
int imgsensor_clk_set(struct IMGSENSOR_CLK *pclk,
	struct ACDK_SENSOR_MCLK_STRUCT *pmclk);
void imgsensor_clk_enable_all(struct IMGSENSOR_CLK *pclk);
void imgsensor_clk_disable_all(struct IMGSENSOR_CLK *pclk);
int imgsensor_clk_ioctrl_handler(void *pbuff);
extern struct platform_device *gpimgsensor_hw_platform_device;

#endif

