/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MTK_VCODEC_PM_H_
#define _MTK_VCODEC_PM_H_

#define MTK_PLATFORM_STR        "platform:mt6877"

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
	struct device   *larbvdec;
	struct device   *larbvenc;
	struct device   *larbvenclt;
	struct device   *dev;
	struct device_node      *chip_node;
	struct mtk_vcodec_dev   *mtkdev;

	struct clk *clk_MT_CG_VDEC;         /* VDEC */
	struct clk *clk_MT_CG_VENC;         /* VENC */
	struct clk *clk_MT_SCP_SYS_VDE;          /* SCP_SYS_VDE */
	struct clk *clk_MT_SCP_SYS_VEN;          /* SCP_SYS_VEN */
	struct clk *clk_MT_SCP_SYS_DIS;          /* SCP_SYS_DIS */

};

enum mtk_dec_dump_addr_type {
	DUMP_VDEC_IN_BUF,
	DUMP_VDEC_OUT_BUF,
	DUMP_VDEC_REF_BUF,
	DUMP_VDEC_MV_BUF,
};

enum mtk_dec_dtsi_reg_idx {
	VDEC_SYS,
	VDEC_UFO,
	VDEC_VLD,
	VDEC_MC,
	VDEC_MV,
	VDEC_MISC,
	NUM_MAX_VDEC_REG_BASE,
};

enum mtk_enc_dtsi_reg_idx {
	VENC_SYS,
	NUM_MAX_VENC_REG_BASE
};

#endif /* _MTK_VCODEC_PM_H_ */
