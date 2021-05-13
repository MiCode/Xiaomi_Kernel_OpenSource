/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */

#ifndef _MTK_VCODEC_PM_H_
#define _MTK_VCODEC_PM_H_

//#include "ion_drv.h"
//#include "mach/mt_iommu.h"

#define MTK_PLATFORM_STR        "platform:mt6873" //TODO: remove when venc ready
#define MTK_VDEC_RACING_INFO_OFFSET  0x100
#define MTK_VDEC_RACING_INFO_SIZE 68
#define MTK_VDEC_MAX_LARB_COUNT 2

#define MTK_MAX_VDEC_CLK_COUNT 10
#define MTK_MAX_VDEC_CLK_CORE_COUNT 3
#define MTK_MAX_VDEC_CLK_LAT_COUNT 3
#define MTK_MAX_VDEC_CLK_MAIN_COUNT 4
#define MTK_MAX_VDEC_CLK_PARENT_SET_COUNT 4

// to distingush clock type, the clock name in dts should start with these prefix
#define MTK_VDEC_CLK_LAT_PREFIX		"LAT"
#define MTK_VDEC_CLK_CORE_PREFIX	"CORE"
#define MTK_VDEC_CLK_MAIN_PREFIX	"MAIN"

#define MTK_MAX_VENC_CLK_COUNT 2
#define MTK_MAX_VENC_CLK_CORE_COUNT 2


#define IS_SPECIFIC_CLK_TYPE(clk_name, prefix) \
		!strncmp(clk_name, prefix, strlen(prefix)) ? true : false

struct mtk_vcodec_clk {
	unsigned int clk_id; // the array index of vdec_clks
	const char *clk_name;
};

struct mtk_vcodec_parent_clk {
	struct mtk_vcodec_clk parent;
	struct mtk_vcodec_clk child;
};

struct mtk_vdec_clks_data {
	struct mtk_vcodec_clk			main_clks[MTK_MAX_VDEC_CLK_MAIN_COUNT];
	unsigned int					main_clks_len;
	struct mtk_vcodec_clk			lat_clks[MTK_MAX_VDEC_CLK_LAT_COUNT];
	unsigned int					lat_clks_len;
	struct mtk_vcodec_clk			core_clks[MTK_MAX_VDEC_CLK_CORE_COUNT];
	unsigned int					core_clks_len;
	struct mtk_vcodec_parent_clk	parent_clks[MTK_MAX_VDEC_CLK_PARENT_SET_COUNT];
	unsigned int					parent_clks_len;
};

struct mtk_venc_clks_data {
	struct mtk_vcodec_clk			core_clks[MTK_MAX_VENC_CLK_CORE_COUNT];
	unsigned int					core_clks_len;
};


/**
 * struct mtk_vcodec_pm - Power management data structure
 */
struct mtk_vcodec_pm {
	struct clk      *vdec_bus_clk_src;
	struct clk      *vencpll;

	struct clk      *vcodecpll;
	struct clk      *univpll_d2;
	struct clk      *clk_cci400_sel;
	struct clk      *vdecpll;
	struct clk      *vdec_sel;
	struct clk      *vencpll_d2;
	struct clk      *venc_sel;
	struct clk      *univpll1_d2;
	struct clk      *venc_lt_sel;
	struct clk      *img_resz;
	struct device   *larbvdecs[MTK_VDEC_MAX_LARB_COUNT];
	struct device   *larbvenc;
	struct device   *larbvenclt;
	struct device   *dev;
	struct device_node      *chip_node;
	struct mtk_vcodec_dev   *mtkdev;

	struct mtk_vdec_clks_data vdec_clks_data;
	struct clk *vdec_clks[MTK_MAX_VDEC_CLK_COUNT];
	struct mtk_venc_clks_data venc_clks_data;
	struct clk *venc_clks[MTK_MAX_VENC_CLK_COUNT];

	atomic_t dec_active_cnt;
	__u32 vdec_racing_info[MTK_VDEC_RACING_INFO_SIZE];
	struct mutex dec_racing_info_mutex;
};

// The reg name should same with dts
#define MTK_VDEC_REG_NAME_VDEC_SYS				"VDEC_SYS"
#define MTK_VDEC_REG_NAME_VDEC_VLD				"VDEC_VLD"
#define MTK_VDEC_REG_NAME_VDEC_MISC				"VDEC_MISC"
#define MTK_VDEC_REG_NAME_VDEC_LAT_MISC			"VDEC_LAT_MISC"
#define MTK_VDEC_REG_NAME_VDEC_RACING_CTRL		"VDEC_RACING_CTRL"
#define MTK_VDEC_REG_NAME_VENC_SYS				"VENC_SYS"
#define MTK_VDEC_REG_NAME_VENC_C1_SYS			"VENC_C1_SYS"
#define MTK_VDEC_REG_NAME_VENC_GCON				"VENC_GCON"


enum mtk_dec_dtsi_reg_idx {
	VDEC_SYS,
	VDEC_VLD,
	VDEC_MISC,
	VDEC_LAT_MISC,
	VDEC_RACING_CTRL,
	NUM_MAX_VDEC_REG_BASE,
};

enum mtk_enc_dtsi_reg_idx {
	VENC_SYS,
	VENC_C1_SYS,
	VENC_GCON,
	NUM_MAX_VENC_REG_BASE
};

#endif /* _MTK_VCODEC_PM_H_ */
