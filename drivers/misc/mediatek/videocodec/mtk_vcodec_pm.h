/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
 *         Tiffany Lin <tiffany.lin@mediatek.com>
 */
#ifndef _VCODEC_PM_H_
#define _VCODEC_PM_H_


#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#define MTK_MAX_CLK_COUNT 7

// The reg name should same with dts
#define MTK_VDEC_REG_NAME_VDEC_BASE				"VDEC_BASE"
#define MTK_VDEC_REG_NAME_VDEC_SYS				"VDEC_SYS"
#define MTK_VDEC_REG_NAME_VDEC_VLD				"VDEC_VLD"
#define MTK_VDEC_REG_NAME_VDEC_MC				"VDEC_MC"
#define MTK_VDEC_REG_NAME_VDEC_MV				"VDEC_MV"
#define MTK_VDEC_REG_NAME_VDEC_MISC				"VDEC_MISC"
#define MTK_VDEC_REG_NAME_VDEC_LAT_MISC				"VDEC_LAT_MISC"
#define MTK_VDEC_REG_NAME_VDEC_LAT_VLD				"VDEC_LAT_VLD"
#define MTK_VDEC_REG_NAME_VDEC_SOC_GCON				"VDEC_SOC_GCON"
#define MTK_VDEC_REG_NAME_VDEC_RACING_CTRL			"VDEC_RACING_CTRL"
#define MTK_VDEC_REG_NAME_VENC_SYS				"VENC_SYS"
#define MTK_VDEC_REG_NAME_VENC_C1_SYS				"VENC_C1_SYS"
#define MTK_VDEC_REG_NAME_VENC_GCON				"VENC_GCON"
#define MTK_VDEC_REG_NAME_VENC_BASE				"VENC_BASE"


#define MTK_PLATFORM_MT6765				"platform:mt6765"
#define MTK_PLATFORM_MT6761				"platform:mt6761"
#define MTK_PLATFORM_MT6739				"platform:mt6739"
#define MTK_PLATFORM_MT6580				"platform:mt6580"

#define MTK_VENC_MAX_LARB_COUNT 2
#define MTK_VDEC_MAX_LARB_COUNT 2

#define MTK_MAX_LARB_COUNT 1


enum mtk_dec_dtsi_reg_idx {
	VDEC_BASE,
	VDEC_SYS,
	VDEC_VLD,
	VDEC_MC,
	VDEC_MV,
	VDEC_MISC,
	NUM_MAX_VDEC_REG_BASE,
};

enum mtk_enc_dtsi_reg_idx {
	VENC_SYS,
	VENC_C1_SYS,
	VENC_GCON,
	VENC_BASE,
	NUM_MAX_VENC_REG_BASE
};
struct mtk_vcodec_clk {
	unsigned int clk_id; // the array index of vdec_clks
	const char *clk_name;
};
struct mtk_clks_data {
	struct mtk_vcodec_clk   core_clks[MTK_MAX_CLK_COUNT];
	unsigned int            core_clks_len;
};

/**
 * struct mtk_vcodec_pm - Power management data structure
 */
struct mtk_vcodec_pm {
	struct device   *dev;
	struct mtk_vcodec_dev   *mtkdev;
	struct mtk_clks_data clks_data;
	struct device   *larbvdecs[MTK_VDEC_MAX_LARB_COUNT];
	struct device   *larbvencs[MTK_VENC_MAX_LARB_COUNT];

	struct clk *vcodec_clks[MTK_MAX_CLK_COUNT];
};
#endif
