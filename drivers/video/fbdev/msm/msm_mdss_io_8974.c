/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/clk/msm-clk.h>
#include <linux/iopoll.h>
#include <linux/kthread.h>

#include "mdss_dsi.h"
#include "mdss_dp.h"
#include "mdss_dsi_phy.h"

#define MDSS_DSI_DSIPHY_REGULATOR_CTRL_0	0x00
#define MDSS_DSI_DSIPHY_REGULATOR_CTRL_1	0x04
#define MDSS_DSI_DSIPHY_REGULATOR_CTRL_2	0x08
#define MDSS_DSI_DSIPHY_REGULATOR_CTRL_3	0x0c
#define MDSS_DSI_DSIPHY_REGULATOR_CTRL_4	0x10
#define MDSS_DSI_DSIPHY_REGULATOR_CAL_PWR_CFG	0x18
#define MDSS_DSI_DSIPHY_LDO_CNTRL		0x1dc
#define MDSS_DSI_DSIPHY_REGULATOR_TEST		0x294
#define MDSS_DSI_DSIPHY_STRENGTH_CTRL_0		0x184
#define MDSS_DSI_DSIPHY_STRENGTH_CTRL_1		0x188
#define MDSS_DSI_DSIPHY_STRENGTH_CTRL_2		0x18c
#define MDSS_DSI_DSIPHY_TIMING_CTRL_0		0x140
#define MDSS_DSI_DSIPHY_GLBL_TEST_CTRL		0x1d4
#define MDSS_DSI_DSIPHY_CTRL_0			0x170
#define MDSS_DSI_DSIPHY_CTRL_1			0x174
#define MDSS_DSI_DSIPHY_CMN_CLK_CFG0		0x0010
#define MDSS_DSI_DSIPHY_CMN_CLK_CFG1		0x0014

#define MDSS_DSI_NUM_DATA_LANES		0x04
#define MDSS_DSI_NUM_CLK_LANES		0x01

#define SW_RESET BIT(2)
#define SW_RESET_PLL BIT(0)
#define PWRDN_B BIT(7)

/* 8996 */
#define DATALANE_OFFSET_FROM_BASE_8996		0x100
#define CLKLANE_OFFSET_FROM_BASE_8996		0x300
#define DATALANE_SIZE_8996			0x80
#define CLKLANE_SIZE_8996			0x80

#define DSIPHY_CMN_PLL_CNTRL			0x0048
#define DSIPHY_CMN_GLBL_TEST_CTRL		0x0018
#define DSIPHY_CMN_CTRL_0			0x001c
#define DSIPHY_CMN_CTRL_1			0x0020
#define DSIPHY_CMN_LDO_CNTRL			0x004c
#define DSIPHY_PLL_CLKBUFLR_EN			0x041c
#define DSIPHY_PLL_PLL_BANDGAP			0x0508

#define DSIPHY_LANE_STRENGTH_CTRL_NUM		0x0002
#define DSIPHY_LANE_STRENGTH_CTRL_OFFSET	0x0004
#define DSIPHY_LANE_STRENGTH_CTRL_BASE		0x0038

#define DSIPHY_LANE_CFG_NUM			0x0004
#define DSIPHY_LANE_CFG_OFFSET			0x0004
#define DSIPHY_LANE_CFG_BASE			0x0000

#define DSIPHY_LANE_VREG_NUM			0x0001
#define DSIPHY_LANE_VREG_OFFSET			0x0004
#define DSIPHY_LANE_VREG_BASE			0x0064

#define DSIPHY_LANE_TIMING_CTRL_NUM		0x0008
#define DSIPHY_LANE_TIMING_CTRL_OFFSET		0x0004
#define DSIPHY_LANE_TIMING_CTRL_BASE		0x0018

#define DSIPHY_LANE_TEST_STR			0x0014

#define DSIPHY_LANE_STRENGTH_CTRL_1		0x003c
#define DSIPHY_LANE_VREG_CNTRL			0x0064

#define DSI_DYNAMIC_REFRESH_PLL_CTRL0		0x214
#define DSI_DYNAMIC_REFRESH_PLL_CTRL1		0x218
#define DSI_DYNAMIC_REFRESH_PLL_CTRL2		0x21C
#define DSI_DYNAMIC_REFRESH_PLL_CTRL3		0x220
#define DSI_DYNAMIC_REFRESH_PLL_CTRL4		0x224
#define DSI_DYNAMIC_REFRESH_PLL_CTRL5		0x228
#define DSI_DYNAMIC_REFRESH_PLL_CTRL6		0x22C
#define DSI_DYNAMIC_REFRESH_PLL_CTRL7		0x230
#define DSI_DYNAMIC_REFRESH_PLL_CTRL8		0x234
#define DSI_DYNAMIC_REFRESH_PLL_CTRL9		0x238
#define DSI_DYNAMIC_REFRESH_PLL_CTRL10		0x23C
#define DSI_DYNAMIC_REFRESH_PLL_CTRL11		0x240
#define DSI_DYNAMIC_REFRESH_PLL_CTRL12		0x244
#define DSI_DYNAMIC_REFRESH_PLL_CTRL13		0x248
#define DSI_DYNAMIC_REFRESH_PLL_CTRL14		0x24C
#define DSI_DYNAMIC_REFRESH_PLL_CTRL15		0x250
#define DSI_DYNAMIC_REFRESH_PLL_CTRL16		0x254
#define DSI_DYNAMIC_REFRESH_PLL_CTRL17		0x258
#define DSI_DYNAMIC_REFRESH_PLL_CTRL18		0x25C
#define DSI_DYNAMIC_REFRESH_PLL_CTRL19		0x260
#define DSI_DYNAMIC_REFRESH_PLL_CTRL19		0x260
#define DSI_DYNAMIC_REFRESH_PLL_CTRL20		0x264
#define DSI_DYNAMIC_REFRESH_PLL_CTRL21		0x268
#define DSI_DYNAMIC_REFRESH_PLL_CTRL22		0x26C
#define DSI_DYNAMIC_REFRESH_PLL_CTRL23		0x270
#define DSI_DYNAMIC_REFRESH_PLL_CTRL24		0x274
#define DSI_DYNAMIC_REFRESH_PLL_CTRL25		0x278
#define DSI_DYNAMIC_REFRESH_PLL_CTRL26		0x27C
#define DSI_DYNAMIC_REFRESH_PLL_CTRL27		0x280
#define DSI_DYNAMIC_REFRESH_PLL_CTRL28		0x284
#define DSI_DYNAMIC_REFRESH_PLL_CTRL29		0x288
#define DSI_DYNAMIC_REFRESH_PLL_CTRL30		0x28C
#define DSI_DYNAMIC_REFRESH_PLL_CTRL31		0x290
#define DSI_DYNAMIC_REFRESH_PLL_UPPER_ADDR	0x294
#define DSI_DYNAMIC_REFRESH_PLL_UPPER_ADDR2	0x298

#define DSIPHY_DLN0_CFG1		0x0104
#define DSIPHY_DLN0_TIMING_CTRL_4	0x0118
#define DSIPHY_DLN0_TIMING_CTRL_5	0x011C
#define DSIPHY_DLN0_TIMING_CTRL_6	0x0120
#define DSIPHY_DLN0_TIMING_CTRL_7	0x0124
#define DSIPHY_DLN0_TIMING_CTRL_8	0x0128

#define DSIPHY_DLN1_CFG1		0x0184
#define DSIPHY_DLN1_TIMING_CTRL_4	0x0198
#define DSIPHY_DLN1_TIMING_CTRL_5	0x019C
#define DSIPHY_DLN1_TIMING_CTRL_6	0x01A0
#define DSIPHY_DLN1_TIMING_CTRL_7	0x01A4
#define DSIPHY_DLN1_TIMING_CTRL_8	0x01A8

#define DSIPHY_DLN2_CFG1		0x0204
#define DSIPHY_DLN2_TIMING_CTRL_4	0x0218
#define DSIPHY_DLN2_TIMING_CTRL_5	0x021C
#define DSIPHY_DLN2_TIMING_CTRL_6	0x0220
#define DSIPHY_DLN2_TIMING_CTRL_7	0x0224
#define DSIPHY_DLN2_TIMING_CTRL_8	0x0228

#define DSIPHY_DLN3_CFG1		0x0284
#define DSIPHY_DLN3_TIMING_CTRL_4	0x0298
#define DSIPHY_DLN3_TIMING_CTRL_5	0x029C
#define DSIPHY_DLN3_TIMING_CTRL_6	0x02A0
#define DSIPHY_DLN3_TIMING_CTRL_7	0x02A4
#define DSIPHY_DLN3_TIMING_CTRL_8	0x02A8

#define DSIPHY_CKLN_CFG1		0x0304
#define DSIPHY_CKLN_TIMING_CTRL_4	0x0318
#define DSIPHY_CKLN_TIMING_CTRL_5	0x031C
#define DSIPHY_CKLN_TIMING_CTRL_6	0x0320
#define DSIPHY_CKLN_TIMING_CTRL_7	0x0324
#define DSIPHY_CKLN_TIMING_CTRL_8	0x0328

#define DSIPHY_PLL_RESETSM_CNTRL5	0x043c

#define DSIPHY_CMN_CLK_CFG1_SPLIT_LINK	0x1

#define PLL_CALC_DATA(addr0, addr1, data0, data1)      \
	(((data1) << 24) | ((((addr1)/4) & 0xFF) << 16) | \
	 ((data0) << 8) | (((addr0)/4) & 0xFF))

#define MDSS_DYN_REF_REG_W(base, offset, addr0, addr1, data0, data1)   \
	writel_relaxed(PLL_CALC_DATA(addr0, addr1, data0, data1), \
			(base) + (offset))

void mdss_dsi_dfps_config_8996(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_panel_data *pdata;
	struct mdss_panel_info *pinfo;
	struct mdss_dsi_phy_ctrl *pd;
	int glbl_tst_cntrl =
		MIPI_INP(ctrl->phy_io.base + DSIPHY_CMN_GLBL_TEST_CTRL);

	pdata = &ctrl->panel_data;
	if (!pdata) {
		pr_err("%s: Invalid panel data\n", __func__);
		return;
	}
	pinfo = &pdata->panel_info;
	pd = &(((ctrl->panel_data).panel_info.mipi).dsi_phy_db);

	if (mdss_dsi_is_ctrl_clk_slave(ctrl)) {
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL0,
				DSIPHY_DLN0_CFG1, DSIPHY_DLN1_CFG1,
				0, 0);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL1,
				DSIPHY_DLN2_CFG1, DSIPHY_DLN3_CFG1,
				0x0, 0x0);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL2,
				DSIPHY_CKLN_CFG1, DSIPHY_DLN0_TIMING_CTRL_4,
				0x0, pd->timing_8996[0]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL3,
				DSIPHY_DLN1_TIMING_CTRL_4,
				DSIPHY_DLN2_TIMING_CTRL_4,
				pd->timing_8996[8],
				pd->timing_8996[16]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL4,
				DSIPHY_DLN3_TIMING_CTRL_4,
				DSIPHY_CKLN_TIMING_CTRL_4,
				pd->timing_8996[24],
				pd->timing_8996[32]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL5,
				DSIPHY_DLN0_TIMING_CTRL_5,
				DSIPHY_DLN1_TIMING_CTRL_5,
				pd->timing_8996[1],
				pd->timing_8996[9]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL6,
				DSIPHY_DLN2_TIMING_CTRL_5,
				DSIPHY_DLN3_TIMING_CTRL_5,
				pd->timing_8996[17],
				pd->timing_8996[25]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL7,
				DSIPHY_CKLN_TIMING_CTRL_5,
				DSIPHY_DLN0_TIMING_CTRL_6,
				pd->timing_8996[33],
				pd->timing_8996[2]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL8,
				DSIPHY_DLN1_TIMING_CTRL_6,
				DSIPHY_DLN2_TIMING_CTRL_6,
				pd->timing_8996[10],
				pd->timing_8996[18]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL9,
				DSIPHY_DLN3_TIMING_CTRL_6,
				DSIPHY_CKLN_TIMING_CTRL_6,
				pd->timing_8996[26],
				pd->timing_8996[34]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL10,
				DSIPHY_DLN0_TIMING_CTRL_7,
				DSIPHY_DLN1_TIMING_CTRL_7,
				pd->timing_8996[3],
				pd->timing_8996[11]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL11,
				DSIPHY_DLN2_TIMING_CTRL_7,
				DSIPHY_DLN3_TIMING_CTRL_7,
				pd->timing_8996[19],
				pd->timing_8996[27]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL12,
				DSIPHY_CKLN_TIMING_CTRL_7,
				DSIPHY_DLN0_TIMING_CTRL_8,
				pd->timing_8996[35],
				pd->timing_8996[4]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL13,
				DSIPHY_DLN1_TIMING_CTRL_8,
				DSIPHY_DLN2_TIMING_CTRL_8,
				pd->timing_8996[12],
				pd->timing_8996[20]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL14,
				DSIPHY_DLN3_TIMING_CTRL_8,
				DSIPHY_CKLN_TIMING_CTRL_8,
				pd->timing_8996[28],
				pd->timing_8996[36]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL15,
				0x0110, 0x0110,	0, 0);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL16,
				0x0110, 0x0110,	0, 0);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL17,
				0x0110, 0x0110, 0, 0);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL18,
				0x0110, 0x0110,	0, 0);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL19,
				0x0110, 0x0110,	0, 0);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL20,
				0x110, 0x110, 0, 0);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL21,
				0x110, 0x110, 0, 0);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL22,
				0x110, 0x110, 0, 0);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL23,
				0x110, 0x110, 0, 0);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL24,
				0x110, 0x110, 0, 0);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL25,
				0x110, 0x110, 0, 0);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL26,
				0x110, 0x110, 0, 0);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL27,
				0x110, 0x110, 0, 0);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL28,
				0x110, 0x110, 0, 0);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL29,
				0x110, 0x110, 0, 0);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL30,
				0x110, 0x110, 0, 0);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL31,
				0x110, 0x110, 0, 0);
		MIPI_OUTP(ctrl->ctrl_base +
				DSI_DYNAMIC_REFRESH_PLL_UPPER_ADDR, 0x0);
		MIPI_OUTP(ctrl->ctrl_base +
				DSI_DYNAMIC_REFRESH_PLL_UPPER_ADDR2, 0x0);
	} else {
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL0,
				DSIPHY_CMN_GLBL_TEST_CTRL,
				DSIPHY_PLL_PLL_BANDGAP,
				glbl_tst_cntrl | BIT(1), 0x1);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL1,
				DSIPHY_PLL_RESETSM_CNTRL5,
				DSIPHY_PLL_PLL_BANDGAP,
				0x0D, 0x03);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL2,
				DSIPHY_PLL_RESETSM_CNTRL5,
				DSIPHY_CMN_PLL_CNTRL,
				0x1D, 0x00);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL3,
				DSIPHY_CMN_CTRL_1, DSIPHY_DLN0_CFG1,
				0x20, 0);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL4,
				DSIPHY_DLN1_CFG1, DSIPHY_DLN2_CFG1,
				0, 0);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL5,
				DSIPHY_DLN3_CFG1, DSIPHY_CKLN_CFG1,
				0, 0);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL6,
				DSIPHY_DLN0_TIMING_CTRL_4,
				DSIPHY_DLN1_TIMING_CTRL_4,
				pd->timing_8996[0],
				pd->timing_8996[8]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL7,
				DSIPHY_DLN2_TIMING_CTRL_4,
				DSIPHY_DLN3_TIMING_CTRL_4,
				pd->timing_8996[16],
				pd->timing_8996[24]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL8,
				DSIPHY_CKLN_TIMING_CTRL_4,
				DSIPHY_DLN0_TIMING_CTRL_5,
				pd->timing_8996[32],
				pd->timing_8996[1]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL9,
				DSIPHY_DLN1_TIMING_CTRL_5,
				DSIPHY_DLN2_TIMING_CTRL_5,
				pd->timing_8996[9],
				pd->timing_8996[17]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL10,
				DSIPHY_DLN3_TIMING_CTRL_5,
				DSIPHY_CKLN_TIMING_CTRL_5,
				pd->timing_8996[25],
				pd->timing_8996[33]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL11,
				DSIPHY_DLN0_TIMING_CTRL_6,
				DSIPHY_DLN1_TIMING_CTRL_6,
				pd->timing_8996[2],
				pd->timing_8996[10]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL12,
				DSIPHY_DLN2_TIMING_CTRL_6,
				DSIPHY_DLN3_TIMING_CTRL_6,
				pd->timing_8996[18],
				pd->timing_8996[26]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL13,
				DSIPHY_CKLN_TIMING_CTRL_6,
				DSIPHY_DLN0_TIMING_CTRL_7,
				pd->timing_8996[34],
				pd->timing_8996[3]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL14,
				DSIPHY_DLN1_TIMING_CTRL_7,
				DSIPHY_DLN2_TIMING_CTRL_7,
				pd->timing_8996[11],
				pd->timing_8996[19]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL15,
				DSIPHY_DLN3_TIMING_CTRL_7,
				DSIPHY_CKLN_TIMING_CTRL_7,
				pd->timing_8996[27],
				pd->timing_8996[35]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL16,
				DSIPHY_DLN0_TIMING_CTRL_8,
				DSIPHY_DLN1_TIMING_CTRL_8,
				pd->timing_8996[4],
				pd->timing_8996[12]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL17,
				DSIPHY_DLN2_TIMING_CTRL_8,
				DSIPHY_DLN3_TIMING_CTRL_8,
				pd->timing_8996[20],
				pd->timing_8996[28]);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL18,
				DSIPHY_CKLN_TIMING_CTRL_8,
				DSIPHY_CMN_CTRL_1,
				pd->timing_8996[36], 0);
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL30,
				DSIPHY_CMN_GLBL_TEST_CTRL,
				DSIPHY_CMN_GLBL_TEST_CTRL,
				((glbl_tst_cntrl) & (~BIT(2))),
				((glbl_tst_cntrl) & (~BIT(2))));
		MDSS_DYN_REF_REG_W(ctrl->ctrl_base,
				DSI_DYNAMIC_REFRESH_PLL_CTRL31,
				DSIPHY_CMN_GLBL_TEST_CTRL,
				DSIPHY_CMN_GLBL_TEST_CTRL,
				((glbl_tst_cntrl) & (~BIT(2))),
				((glbl_tst_cntrl) & (~BIT(2))));
	}

	wmb(); /* make sure phy timings are updated*/
}

static void mdss_dsi_ctrl_phy_reset(struct mdss_dsi_ctrl_pdata *ctrl)
{
	/* start phy sw reset */
	MIPI_OUTP(ctrl->ctrl_base + 0x12c, 0x0001);
	udelay(1000);
	wmb();	/* make sure reset */
	/* end phy sw reset */
	MIPI_OUTP(ctrl->ctrl_base + 0x12c, 0x0000);
	udelay(100);
	wmb();	/* maek sure reset cleared */
}

int mdss_dsi_phy_pll_reset_status(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc;
	u32 val;
	u32 const sleep_us = 10, timeout_us = 100;

	pr_debug("%s: polling for RESETSM_READY_STATUS.CORE_READY\n",
		__func__);
	rc = readl_poll_timeout(ctrl->phy_io.base + 0x4cc, val,
		(val & 0x1), sleep_us, timeout_us);

	return rc;
}

static void mdss_dsi_phy_sw_reset_sub(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_dsi_ctrl_pdata *sctrl = NULL;
	struct dsi_shared_data *sdata;
	struct mdss_dsi_ctrl_pdata *octrl;
	u32 reg_val = 0;

	if (ctrl == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	sdata = ctrl->shared_data;
	octrl = mdss_dsi_get_other_ctrl(ctrl);

	if (ctrl->shared_data->phy_rev == DSI_PHY_REV_20) {
		if (mdss_dsi_is_ctrl_clk_master(ctrl))
			sctrl = mdss_dsi_get_ctrl_clk_slave();
		else
			return;
	}

	/*
	 * For dual dsi case if we do DSI PHY sw reset,
	 * this will reset DSI PHY regulators also.
	 * Since DSI PHY regulator is shared among both
	 * the DSI controllers, we should not do DSI PHY
	 * sw reset when the other DSI controller is still
	 * active.
	 */
	mutex_lock(&sdata->phy_reg_lock);
	if ((mdss_dsi_is_hw_config_dual(sdata) &&
		(octrl && octrl->is_phyreg_enabled))) {
		/* start phy lane and HW reset */
		reg_val = MIPI_INP(ctrl->ctrl_base + 0x12c);
		reg_val |= (BIT(16) | BIT(8));
		MIPI_OUTP(ctrl->ctrl_base + 0x12c, reg_val);
		/* wait for 1ms as per HW design */
		usleep_range(1000, 2000);
		/* ensure phy lane and HW reset starts */
		wmb();
		/* end phy lane and HW reset */
		reg_val = MIPI_INP(ctrl->ctrl_base + 0x12c);
		reg_val &= ~(BIT(16) | BIT(8));
		MIPI_OUTP(ctrl->ctrl_base + 0x12c, reg_val);
		/* wait for 100us as per HW design */
		usleep_range(100, 200);
		/* ensure phy lane and HW reset ends */
		wmb();
	} else {
		/* start phy sw reset */
		mdss_dsi_ctrl_phy_reset(ctrl);
		if (sctrl)
			mdss_dsi_ctrl_phy_reset(sctrl);

	}
	mutex_unlock(&sdata->phy_reg_lock);
}

void mdss_dsi_phy_sw_reset(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_dsi_ctrl_pdata *sctrl = NULL;
	struct dsi_shared_data *sdata;

	if (ctrl == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	sdata = ctrl->shared_data;

	/*
	 * When operating in split display mode, make sure that the PHY reset
	 * is only done from the clock master. This will ensure that the PLL is
	 * off when PHY reset is called.
	 */
	if (mdss_dsi_is_ctrl_clk_slave(ctrl))
		return;

	mdss_dsi_phy_sw_reset_sub(ctrl);

	if (mdss_dsi_is_ctrl_clk_master(ctrl)) {
		sctrl = mdss_dsi_get_ctrl_clk_slave();
		if (sctrl)
			mdss_dsi_phy_sw_reset_sub(sctrl);
		else
			pr_warn("%s: unable to get slave ctrl\n", __func__);
	}

	/* All other quirks go here */
	if ((sdata->hw_rev == MDSS_DSI_HW_REV_103) &&
		!mdss_dsi_is_hw_config_dual(sdata) &&
		mdss_dsi_is_right_ctrl(ctrl)) {

		/*
		 * phy sw reset will wipe out the pll settings for PLL.
		 * Need to explicitly turn off PLL1 if unused to avoid
		 * current leakage issues.
		 */
		if ((mdss_dsi_is_hw_config_split(sdata) ||
			mdss_dsi_is_pll_src_pll0(sdata)) &&
			ctrl->vco_dummy_clk) {
			pr_debug("Turn off unused PLL1 registers\n");
			clk_set_rate(ctrl->vco_dummy_clk, 1);
		}
	}
}

static void mdss_dsi_phy_regulator_disable(struct mdss_dsi_ctrl_pdata *ctrl)
{
	if (!ctrl) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	if (ctrl->shared_data->phy_rev == DSI_PHY_REV_20)
		return;

	if (ctrl->shared_data->phy_rev == DSI_PHY_REV_30)
		return;

	MIPI_OUTP(ctrl->phy_regulator_io.base + 0x018, 0x000);
}

static void mdss_dsi_phy_shutdown(struct mdss_dsi_ctrl_pdata *ctrl)
{
	if (!ctrl) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	if (ctrl->shared_data->phy_rev == DSI_PHY_REV_20) {
		MIPI_OUTP(ctrl->phy_io.base + DSIPHY_PLL_CLKBUFLR_EN, 0);
		MIPI_OUTP(ctrl->phy_io.base + DSIPHY_CMN_GLBL_TEST_CTRL, 0);
		MIPI_OUTP(ctrl->phy_io.base + DSIPHY_CMN_CTRL_0, 0);
	} else if (ctrl->shared_data->phy_rev == DSI_PHY_REV_30) {
		mdss_dsi_phy_v3_shutdown(ctrl);
	} else {
		MIPI_OUTP(ctrl->phy_io.base + MDSS_DSI_DSIPHY_CTRL_0, 0x000);
	}
}

/**
 * mdss_dsi_lp_cd_rx() -- enable LP and CD at receiving
 * @ctrl: pointer to DSI controller structure
 *
 * LP: low power
 * CD: contention detection
 */
void mdss_dsi_lp_cd_rx(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_dsi_phy_ctrl *pd;

	if (!ctrl) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	if (ctrl->shared_data->phy_rev == DSI_PHY_REV_20)
		return;

	pd = &(((ctrl->panel_data).panel_info.mipi).dsi_phy_db);

	/* Strength ctrl 1, LP Rx + CD Rxcontention detection */
	MIPI_OUTP((ctrl->phy_io.base) + 0x0188, pd->strength[1]);
	wmb();
}

static void mdss_dsi_28nm_phy_regulator_enable(
		struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct mdss_dsi_phy_ctrl *pd;
	pd = &(((ctrl_pdata->panel_data).panel_info.mipi).dsi_phy_db);

	if (pd->regulator_len == 0) {
		pr_warn("%s: invalid regulator settings\n", __func__);
		return;
	}

	if (pd->reg_ldo_mode) {
		/* Regulator ctrl 0 */
		MIPI_OUTP(ctrl_pdata->phy_regulator_io.base, 0x0);
		/* Regulator ctrl - CAL_PWR_CFG */
		MIPI_OUTP((ctrl_pdata->phy_regulator_io.base)
				+ 0x18, pd->regulator[6]);
		/* Add H/w recommended delay */
		udelay(1000);
		/* Regulator ctrl - TEST */
		MIPI_OUTP((ctrl_pdata->phy_regulator_io.base)
				+ 0x14, pd->regulator[5]);
		/* Regulator ctrl 3 */
		MIPI_OUTP((ctrl_pdata->phy_regulator_io.base)
				+ 0xc, pd->regulator[3]);
		/* Regulator ctrl 2 */
		MIPI_OUTP((ctrl_pdata->phy_regulator_io.base)
				+ 0x8, pd->regulator[2]);
		/* Regulator ctrl 1 */
		MIPI_OUTP((ctrl_pdata->phy_regulator_io.base)
				+ 0x4, pd->regulator[1]);
		/* Regulator ctrl 4 */
		MIPI_OUTP((ctrl_pdata->phy_regulator_io.base)
				+ 0x10, pd->regulator[4]);
		/* LDO ctrl */
		if ((ctrl_pdata->shared_data->hw_rev ==
			MDSS_DSI_HW_REV_103_1)
			|| (ctrl_pdata->shared_data->hw_rev ==
			MDSS_DSI_HW_REV_104_2))
			MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x1dc, 0x05);
		else
			MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x1dc, 0x0d);
	} else {
		/* Regulator ctrl 0 */
		MIPI_OUTP(ctrl_pdata->phy_regulator_io.base,
					0x0);
		/* Regulator ctrl - CAL_PWR_CFG */
		MIPI_OUTP((ctrl_pdata->phy_regulator_io.base)
				+ 0x18, pd->regulator[6]);
		/* Add H/w recommended delay */
		udelay(1000);
		/* Regulator ctrl 1 */
		MIPI_OUTP((ctrl_pdata->phy_regulator_io.base)
				+ 0x4, pd->regulator[1]);
		/* Regulator ctrl 2 */
		MIPI_OUTP((ctrl_pdata->phy_regulator_io.base)
				+ 0x8, pd->regulator[2]);
		/* Regulator ctrl 3 */
		MIPI_OUTP((ctrl_pdata->phy_regulator_io.base)
				+ 0xc, pd->regulator[3]);
		/* Regulator ctrl 4 */
		MIPI_OUTP((ctrl_pdata->phy_regulator_io.base)
				+ 0x10, pd->regulator[4]);
		/* LDO ctrl */
		MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x1dc, 0x00);
		/* Regulator ctrl 0 */
		MIPI_OUTP(ctrl_pdata->phy_regulator_io.base,
				pd->regulator[0]);
	}
}

static void mdss_dsi_28nm_phy_config(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct mdss_dsi_phy_ctrl *pd;
	int i, off, ln, offset;

	if (!ctrl_pdata) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	pd = &(((ctrl_pdata->panel_data).panel_info.mipi).dsi_phy_db);

	/* Strength ctrl 0 for 28nm PHY*/
	if ((ctrl_pdata->shared_data->hw_rev <= MDSS_DSI_HW_REV_104_2) &&
		(ctrl_pdata->shared_data->hw_rev != MDSS_DSI_HW_REV_103)) {
		MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x0170, 0x5b);
		MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x0184, pd->strength[0]);
		/* make sure PHY strength ctrl is set */
		wmb();
	}

	off = 0x0140;	/* phy timing ctrl 0 - 11 */
	for (i = 0; i < 12; i++) {
		MIPI_OUTP((ctrl_pdata->phy_io.base) + off, pd->timing[i]);
		wmb();
		off += 4;
	}

	/* 4 lanes + clk lane configuration */
	/* lane config n * (0 - 4) & DataPath setup */
	for (ln = 0; ln < 5; ln++) {
		off = (ln * 0x40);
		for (i = 0; i < 9; i++) {
			offset = i + (ln * 9);
			MIPI_OUTP((ctrl_pdata->phy_io.base) + off,
							pd->lanecfg[offset]);
			wmb();
			off += 4;
		}
	}

	/* MMSS_DSI_0_PHY_DSIPHY_CTRL_4 */
	MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x0180, 0x0a);
	wmb();

	/* DSI_0_PHY_DSIPHY_GLBL_TEST_CTRL */
	if (!mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data)) {
		MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x01d4, 0x01);
	} else {
		if (((ctrl_pdata->panel_data).panel_info.pdest == DISPLAY_1) ||
		(ctrl_pdata->shared_data->hw_rev == MDSS_DSI_HW_REV_103_1))
			MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x01d4, 0x01);
		else
			MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x01d4, 0x00);
	}
	/* ensure DSIPHY_GLBL_TEST_CTRL is set */
	wmb();

	/* MMSS_DSI_0_PHY_DSIPHY_CTRL_0 */
	MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x0170, 0x5f);
	/* make sure PHY lanes are powered on */
	wmb();

	off = 0x01b4;	/* phy BIST ctrl 0 - 5 */
	for (i = 0; i < 6; i++) {
		MIPI_OUTP((ctrl_pdata->phy_io.base) + off, pd->bistctrl[i]);
		wmb();
		off += 4;
	}

}

static void mdss_dsi_20nm_phy_regulator_enable(struct mdss_dsi_ctrl_pdata
	*ctrl_pdata)
{
	struct mdss_dsi_phy_ctrl *pd;
	void __iomem *phy_io_base;

	pd = &(((ctrl_pdata->panel_data).panel_info.mipi).dsi_phy_db);
	phy_io_base = ctrl_pdata->phy_regulator_io.base;

	if (pd->regulator_len != 7) {
		pr_err("%s: wrong regulator settings\n", __func__);
		return;
	}

	if (pd->reg_ldo_mode) {
		MIPI_OUTP(ctrl_pdata->phy_io.base + MDSS_DSI_DSIPHY_LDO_CNTRL,
			0x1d);
	} else {
		MIPI_OUTP(phy_io_base + MDSS_DSI_DSIPHY_REGULATOR_CTRL_1,
			pd->regulator[1]);
		MIPI_OUTP(phy_io_base + MDSS_DSI_DSIPHY_REGULATOR_CTRL_2,
			pd->regulator[2]);
		MIPI_OUTP(phy_io_base + MDSS_DSI_DSIPHY_REGULATOR_CTRL_3,
			pd->regulator[3]);
		MIPI_OUTP(phy_io_base + MDSS_DSI_DSIPHY_REGULATOR_CTRL_4,
			pd->regulator[4]);
		MIPI_OUTP(phy_io_base + MDSS_DSI_DSIPHY_REGULATOR_CAL_PWR_CFG,
			pd->regulator[6]);
		MIPI_OUTP(ctrl_pdata->phy_io.base + MDSS_DSI_DSIPHY_LDO_CNTRL,
			0x00);
		MIPI_OUTP(phy_io_base + MDSS_DSI_DSIPHY_REGULATOR_CTRL_0,
			pd->regulator[0]);
	}
}

static void mdss_dsi_20nm_phy_config(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct mdss_dsi_phy_ctrl *pd;
	int i, off, ln, offset;

	pd = &(((ctrl_pdata->panel_data).panel_info.mipi).dsi_phy_db);

	if (pd->strength_len != 2) {
		pr_err("%s: wrong strength ctrl\n", __func__);
		return;
	}

	MIPI_OUTP((ctrl_pdata->phy_io.base) + MDSS_DSI_DSIPHY_STRENGTH_CTRL_0,
		pd->strength[0]);


	if (!mdss_dsi_is_hw_config_dual(ctrl_pdata->shared_data)) {
		if (mdss_dsi_is_hw_config_split(ctrl_pdata->shared_data) ||
			mdss_dsi_is_left_ctrl(ctrl_pdata) ||
			(mdss_dsi_is_right_ctrl(ctrl_pdata) &&
			mdss_dsi_is_pll_src_pll0(ctrl_pdata->shared_data)))
			MIPI_OUTP((ctrl_pdata->phy_io.base) +
				MDSS_DSI_DSIPHY_GLBL_TEST_CTRL, 0x00);
		else
			MIPI_OUTP((ctrl_pdata->phy_io.base) +
				MDSS_DSI_DSIPHY_GLBL_TEST_CTRL, 0x01);
	} else {
		if (mdss_dsi_is_left_ctrl(ctrl_pdata))
			MIPI_OUTP((ctrl_pdata->phy_io.base) +
				MDSS_DSI_DSIPHY_GLBL_TEST_CTRL, 0x00);
		else
			MIPI_OUTP((ctrl_pdata->phy_io.base) +
				MDSS_DSI_DSIPHY_GLBL_TEST_CTRL, 0x01);
	}

	if (pd->lanecfg_len != 45) {
		pr_err("%s: wrong lane cfg\n", __func__);
		return;
	}

	/* 4 lanes + clk lane configuration */
	/* lane config n * (0 - 4) & DataPath setup */
	for (ln = 0; ln < 5; ln++) {
		off = (ln * 0x40);
		for (i = 0; i < 9; i++) {
			offset = i + (ln * 9);
			MIPI_OUTP((ctrl_pdata->phy_io.base) + off,
				pd->lanecfg[offset]);
			wmb();
			off += 4;
		}
	}

	off = 0;	/* phy timing ctrl 0 - 11 */
	for (i = 0; i < 12; i++) {
		MIPI_OUTP((ctrl_pdata->phy_io.base) +
			MDSS_DSI_DSIPHY_TIMING_CTRL_0 + off, pd->timing[i]);
		wmb();
		off += 4;
	}

	MIPI_OUTP((ctrl_pdata->phy_io.base) + MDSS_DSI_DSIPHY_CTRL_1, 0);
	/* make sure everything is written before enable */
	wmb();
	MIPI_OUTP((ctrl_pdata->phy_io.base) + MDSS_DSI_DSIPHY_CTRL_0, 0x7f);
}

static void mdss_dsi_8996_pll_source_standalone(
				struct mdss_dsi_ctrl_pdata *ctrl)
{
	u32 data;

	/*
	 * pll right output enabled
	 * bit clk select from left
	 */
	MIPI_OUTP((ctrl->phy_io.base) + DSIPHY_PLL_CLKBUFLR_EN, 0x01);
	data = MIPI_INP((ctrl->phy_io.base) + DSIPHY_CMN_GLBL_TEST_CTRL);
	data &= ~BIT(2);
	MIPI_OUTP((ctrl->phy_io.base) + DSIPHY_CMN_GLBL_TEST_CTRL, data);
}

static void mdss_dsi_8996_pll_source_from_right(
				struct mdss_dsi_ctrl_pdata *ctrl)
{
	u32 data;

	/*
	 * pll left + right output disabled
	 * bit clk select from right
	 */
	MIPI_OUTP((ctrl->phy_io.base) + DSIPHY_PLL_CLKBUFLR_EN, 0x00);
	data = MIPI_INP((ctrl->phy_io.base) + DSIPHY_CMN_GLBL_TEST_CTRL);
	data |= BIT(2);
	MIPI_OUTP((ctrl->phy_io.base) + DSIPHY_CMN_GLBL_TEST_CTRL, data);

	/* enable bias current for pll1 during split display case */
	MIPI_OUTP((ctrl->phy_io.base) + DSIPHY_PLL_PLL_BANDGAP, 0x3);
}

static void mdss_dsi_8996_pll_source_from_left(
				struct mdss_dsi_ctrl_pdata *ctrl)
{
	u32 data;

	/*
	 * pll left + right output enabled
	 * bit clk select from left
	 */
	MIPI_OUTP((ctrl->phy_io.base) + DSIPHY_PLL_CLKBUFLR_EN, 0x03);
	data = MIPI_INP((ctrl->phy_io.base) + DSIPHY_CMN_GLBL_TEST_CTRL);
	data &= ~BIT(2);
	MIPI_OUTP((ctrl->phy_io.base) + DSIPHY_CMN_GLBL_TEST_CTRL, data);
}

static void mdss_dsi_8996_phy_regulator_enable(
	struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_dsi_phy_ctrl *pd;
	int j, off, ln, cnt, ln_off;
	char *ip;
	void __iomem *base;
	struct mdss_panel_info *panel_info;

	if (!ctrl) {
		pr_warn("%s: null ctrl pdata\n", __func__);
		return;
	}

	panel_info = &((ctrl->panel_data).panel_info);
	pd = &(((ctrl->panel_data).panel_info.mipi).dsi_phy_db);

	if (pd->regulator_len != (MDSS_DSI_NUM_DATA_LANES +
					MDSS_DSI_NUM_CLK_LANES)) {
		pr_warn("%s: invalid regulator settings\n", __func__);
		return;
	}

	/*
	 * data lane offset from base: 0x100
	 * data lane size: 0x80
	 */
	base = ctrl->phy_io.base + DATALANE_OFFSET_FROM_BASE_8996;
	/* data lanes configuration */
	for (ln = 0; ln < MDSS_DSI_NUM_DATA_LANES; ln++) {
		/* vreg ctrl, 1 * MDSS_DSI_NUM_DATA_LANES */
		cnt = DSIPHY_LANE_VREG_NUM;
		off = DSIPHY_LANE_VREG_BASE;
		ln_off = cnt * ln;
		ip = &pd->regulator[ln_off];
		for (j = 0; j < cnt; j++) {
			MIPI_OUTP(base + off, *ip++);
			off += DSIPHY_LANE_VREG_OFFSET;
		}
		base += DATALANE_SIZE_8996; /* next lane */
	}

	/*
	 * clk lane offset from base: 0x300
	 * clk lane size: 0x80
	 */
	base = ctrl->phy_io.base + CLKLANE_OFFSET_FROM_BASE_8996;
	/*
	 * clk lane configuration for vreg ctrl
	 * for split link there are two clock lanes, one
	 * clock lane per sublink needs to be configured
	 */
	off = DSIPHY_LANE_VREG_BASE;
	ln_off = MDSS_DSI_NUM_DATA_LANES;
	ip = &pd->regulator[ln_off];
	MIPI_OUTP(base + off, *ip);
	if (panel_info->split_link_enabled)
		MIPI_OUTP(base + CLKLANE_SIZE_8996 + off, *ip);

	wmb(); /* make sure registers committed */
}

static void mdss_dsi_8996_phy_power_off(
	struct mdss_dsi_ctrl_pdata *ctrl)
{
	int ln;
	void __iomem *base;
	u32 data;
	struct mdss_panel_info *panel_info;

	if (ctrl) {
		panel_info = &((ctrl->panel_data).panel_info);
	} else {
		pr_warn("%s: null ctrl pdata\n", __func__);
		return;
	}

	/* Turn off PLL power */
	data = MIPI_INP(ctrl->phy_io.base + DSIPHY_CMN_CTRL_0);
	MIPI_OUTP(ctrl->phy_io.base + DSIPHY_CMN_CTRL_0, data & ~BIT(7));

	/* data lanes configuration */
	base = ctrl->phy_io.base + DATALANE_OFFSET_FROM_BASE_8996;
	for (ln = 0; ln < MDSS_DSI_NUM_DATA_LANES; ln++) {
		/* turn off phy ldo */
		MIPI_OUTP(base + DSIPHY_LANE_VREG_BASE, 0x1c);
		base += DATALANE_SIZE_8996; /* next lane */
	}

	/* clk lane configuration */
	base = ctrl->phy_io.base + CLKLANE_OFFSET_FROM_BASE_8996;
	/* turn off phy ldo */
	MIPI_OUTP(base + DSIPHY_LANE_VREG_BASE, 0x1c);
	if (panel_info->split_link_enabled)
		MIPI_OUTP(base + CLKLANE_SIZE_8996 +
				DSIPHY_LANE_VREG_BASE, 0x1c);

	MIPI_OUTP((ctrl->phy_io.base) + DSIPHY_CMN_LDO_CNTRL, 0x1c);

	/* data lanes configuration */
	base = ctrl->phy_io.base + DATALANE_OFFSET_FROM_BASE_8996;
	for (ln = 0; ln < MDSS_DSI_NUM_DATA_LANES; ln++) {
		MIPI_OUTP(base + DSIPHY_LANE_STRENGTH_CTRL_1, 0x0);
		base += DATALANE_SIZE_8996; /* next lane */
	}

	/* clk lane configuration */
	base = ctrl->phy_io.base + CLKLANE_OFFSET_FROM_BASE_8996;
	MIPI_OUTP(base + DSIPHY_LANE_STRENGTH_CTRL_1, 0x0);
	if (panel_info->split_link_enabled)
		MIPI_OUTP(base + CLKLANE_SIZE_8996 +
				DSIPHY_LANE_STRENGTH_CTRL_1, 0x0);

	wmb(); /* make sure registers committed */
}

static void mdss_dsi_phy_power_off(
	struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_panel_info *pinfo;

	if (ctrl->phy_power_off)
		return;

	pinfo = &ctrl->panel_data.panel_info;

	if ((ctrl->shared_data->phy_rev != DSI_PHY_REV_20) ||
		!pinfo->allow_phy_power_off) {
		pr_debug("%s: ctrl%d phy rev:%d panel support for phy off:%d\n",
			__func__, ctrl->ndx, ctrl->shared_data->phy_rev,
			pinfo->allow_phy_power_off);
		return;
	}

	/* supported for phy rev 2.0 and if panel allows it*/
	mdss_dsi_8996_phy_power_off(ctrl);

	ctrl->phy_power_off = true;
}

static void mdss_dsi_8996_phy_power_on(
	struct mdss_dsi_ctrl_pdata *ctrl)
{
	int j, off, ln, cnt, ln_off;
	void __iomem *base;
	struct mdss_dsi_phy_ctrl *pd;
	char *ip;
	u32 data;
	struct mdss_panel_info *panel_info;

	if (ctrl) {
		panel_info = &((ctrl->panel_data).panel_info);
	} else {
		pr_warn("%s: null ctrl pdata\n", __func__);
		return;
	}

	pd = &(((ctrl->panel_data).panel_info.mipi).dsi_phy_db);

	/* data lanes configuration */
	base = ctrl->phy_io.base + DATALANE_OFFSET_FROM_BASE_8996;
	for (ln = 0; ln < MDSS_DSI_NUM_DATA_LANES; ln++) {
		/* strength, 2 * MDSS_DSI_NUM_DATA_LANES */
		cnt = DSIPHY_LANE_STRENGTH_CTRL_NUM;
		ln_off = cnt * ln;
		ip = &pd->strength[ln_off];
		off = DSIPHY_LANE_STRENGTH_CTRL_BASE;
		for (j = 0; j < cnt; j++,
			off += DSIPHY_LANE_STRENGTH_CTRL_OFFSET)
			MIPI_OUTP(base + off, *ip++);
		base += DATALANE_SIZE_8996; /* next lane */
	}

	/*
	 * clk lane configuration for strength ctrl
	 * for split link there are two clock lanes, one
	 * clock lane per sublink needs to be configured
	 */
	base = ctrl->phy_io.base + CLKLANE_OFFSET_FROM_BASE_8996;
	cnt = DSIPHY_LANE_STRENGTH_CTRL_NUM;
	ln_off = MDSS_DSI_NUM_DATA_LANES;
	ip = &pd->strength[ln_off];
	off = DSIPHY_LANE_STRENGTH_CTRL_BASE;
	for (j = 0; j < cnt; j++,
		off += DSIPHY_LANE_STRENGTH_CTRL_OFFSET) {
		MIPI_OUTP(base + off, *ip);
		if (panel_info->split_link_enabled)
			MIPI_OUTP(base + CLKLANE_SIZE_8996 + off, *ip);
	}

	mdss_dsi_8996_phy_regulator_enable(ctrl);

	/* Turn on PLL power */
	data = MIPI_INP(ctrl->phy_io.base + DSIPHY_CMN_CTRL_0);
	MIPI_OUTP(ctrl->phy_io.base + DSIPHY_CMN_CTRL_0, data | BIT(7));
}

static void mdss_dsi_phy_power_on(
	struct mdss_dsi_ctrl_pdata *ctrl, bool mmss_clamp)
{
	if (mmss_clamp && !ctrl->phy_power_off)
		mdss_dsi_phy_init(ctrl);
	else if ((ctrl->shared_data->phy_rev == DSI_PHY_REV_20) &&
	    ctrl->phy_power_off)
		mdss_dsi_8996_phy_power_on(ctrl);

	ctrl->phy_power_off = false;
}

static void mdss_dsi_8996_phy_config(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_dsi_phy_ctrl *pd;
	int j, off, ln, cnt, ln_off;
	char *ip;
	void __iomem *base;
	struct mdss_panel_info *panel_info;
	int num_of_lanes = 0;

	if (ctrl) {
		panel_info = &((ctrl->panel_data).panel_info);
	} else {
		pr_warn("%s: null ctrl pdata\n", __func__);
		return;
	}

	pd = &(((ctrl->panel_data).panel_info.mipi).dsi_phy_db);
	num_of_lanes = MDSS_DSI_NUM_DATA_LANES + MDSS_DSI_NUM_CLK_LANES;

	MIPI_OUTP((ctrl->phy_io.base) + DSIPHY_CMN_LDO_CNTRL, 0x1c);

	/* clk_en */
	MIPI_OUTP((ctrl->phy_io.base) + DSIPHY_CMN_GLBL_TEST_CTRL, 0x1);

	if (pd->lanecfg_len != (num_of_lanes * DSIPHY_LANE_CFG_NUM)) {
		pr_err("%s: wrong lane cfg\n", __func__);
		return;
	}

	if (pd->strength_len != (num_of_lanes *
				DSIPHY_LANE_STRENGTH_CTRL_NUM)) {
		pr_err("%s: wrong strength ctrl\n", __func__);
		return;
	}

	if (pd->regulator_len != (num_of_lanes * DSIPHY_LANE_VREG_NUM)) {
		pr_err("%s: wrong regulator setting\n", __func__);
		return;
	}

	/* data lanes configuration */
	base = ctrl->phy_io.base + DATALANE_OFFSET_FROM_BASE_8996;
	for (ln = 0; ln < MDSS_DSI_NUM_DATA_LANES; ln++) {
		/* lane cfg, 4 * MDSS_DSI_NUM_DATA_LANES */
		cnt = DSIPHY_LANE_CFG_NUM;
		off = DSIPHY_LANE_CFG_BASE;
		ln_off = cnt * ln;
		ip = &pd->lanecfg[ln_off];
		for (j = 0; j < cnt; j++) {
			MIPI_OUTP(base + off, *ip++);
			off += DSIPHY_LANE_CFG_OFFSET;
		}

		/* test str */
		MIPI_OUTP(base + DSIPHY_LANE_TEST_STR, 0x88);	/* fixed */

		/* phy timing, 8 * MDSS_DSI_NUM_DATA_LANES */
		cnt = DSIPHY_LANE_TIMING_CTRL_NUM;
		off = DSIPHY_LANE_TIMING_CTRL_BASE;
		ln_off = cnt * ln;
		ip = &pd->timing_8996[ln_off];
		for (j = 0; j < cnt; j++) {
			MIPI_OUTP(base + off, *ip++);
			off += DSIPHY_LANE_TIMING_CTRL_OFFSET;
		}

		/* strength, 2 * MDSS_DSI_NUM_DATA_LANES */
		cnt = DSIPHY_LANE_STRENGTH_CTRL_NUM;
		off = DSIPHY_LANE_STRENGTH_CTRL_BASE;
		ln_off = cnt * ln;
		ip = &pd->strength[ln_off];
		for (j = 0; j < cnt; j++) {
			MIPI_OUTP(base + off, *ip++);
			off += DSIPHY_LANE_STRENGTH_CTRL_OFFSET;
		}

		base += DATALANE_SIZE_8996; /* next lane */
	}

	/*
	 * clk lane configuration
	 * for split link there are two clock lanes, one
	 * clock lane per sublink needs to be configured
	 */
	base = ctrl->phy_io.base + CLKLANE_OFFSET_FROM_BASE_8996;
	cnt = DSIPHY_LANE_CFG_NUM;
	off = DSIPHY_LANE_CFG_BASE;
	ln_off = cnt * MDSS_DSI_NUM_DATA_LANES;
	ip = &pd->lanecfg[ln_off];
	for (j = 0; j < cnt; j++) {
		MIPI_OUTP(base + off, *ip);
		if (panel_info->split_link_enabled)
			MIPI_OUTP(base + CLKLANE_SIZE_8996 + off, *ip);
		ip++;
		off += DSIPHY_LANE_CFG_OFFSET;
	}

	/* test str */
	MIPI_OUTP(base + DSIPHY_LANE_TEST_STR, 0x88);	/* fixed */
	if (panel_info->split_link_enabled)
		MIPI_OUTP(base + CLKLANE_SIZE_8996 + off, 0x88);

	cnt = DSIPHY_LANE_TIMING_CTRL_NUM;
	off = DSIPHY_LANE_TIMING_CTRL_BASE;
	ln_off = cnt * MDSS_DSI_NUM_DATA_LANES;
	ip = &pd->timing_8996[ln_off];
	for (j = 0; j < cnt; j++) {
		MIPI_OUTP(base + off, *ip);
		if (panel_info->split_link_enabled)
			MIPI_OUTP(base + CLKLANE_SIZE_8996 + off, *ip);
		ip++;
		off += DSIPHY_LANE_TIMING_CTRL_OFFSET;
	}

	/*
	 * clk lane configuration for timing
	 * for split link there are two clock lanes, one
	 * clock lane per sublink needs to be configured
	 */
	cnt = DSIPHY_LANE_STRENGTH_CTRL_NUM;
	off = DSIPHY_LANE_STRENGTH_CTRL_BASE;
	ln_off = cnt * MDSS_DSI_NUM_DATA_LANES;
	ip = &pd->strength[ln_off];
	for (j = 0; j < cnt; j++) {
		MIPI_OUTP(base + off, *ip);
		if (panel_info->split_link_enabled)
			MIPI_OUTP(base + CLKLANE_SIZE_8996 + off, *ip);
		ip++;
		off += DSIPHY_LANE_STRENGTH_CTRL_OFFSET;
	}

	wmb(); /* make sure registers committed */

	/* reset digital block */
	MIPI_OUTP((ctrl->phy_io.base) + DSIPHY_CMN_CTRL_1, 0x80);
	udelay(100);
	MIPI_OUTP((ctrl->phy_io.base) + DSIPHY_CMN_CTRL_1, 0x00);

	if (mdss_dsi_is_hw_config_split(ctrl->shared_data)) {
		if (mdss_dsi_is_left_ctrl(ctrl))
			mdss_dsi_8996_pll_source_from_left(ctrl);
		else
			mdss_dsi_8996_pll_source_from_right(ctrl);
	} else {
		if (mdss_dsi_is_right_ctrl(ctrl) &&
			mdss_dsi_is_pll_src_pll0(ctrl->shared_data))
			mdss_dsi_8996_pll_source_from_left(ctrl);
		else
			mdss_dsi_8996_pll_source_standalone(ctrl);
	}

	MIPI_OUTP(ctrl->phy_io.base + DSIPHY_CMN_CTRL_0, 0x7f);
	wmb(); /* make sure registers committed */
}

static void mdss_dsi_phy_regulator_ctrl(struct mdss_dsi_ctrl_pdata *ctrl,
	bool enable)
{
	struct mdss_dsi_ctrl_pdata *other_ctrl;
	struct dsi_shared_data *sdata;

	if (!ctrl) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	sdata = ctrl->shared_data;
	other_ctrl = mdss_dsi_get_other_ctrl(ctrl);

	mutex_lock(&sdata->phy_reg_lock);
	if (enable) {
		if (ctrl->shared_data->phy_rev == DSI_PHY_REV_20) {
			mdss_dsi_8996_phy_regulator_enable(ctrl);
		} else if (ctrl->shared_data->phy_rev == DSI_PHY_REV_30) {
			mdss_dsi_phy_v3_regulator_enable(ctrl);
		} else {
			switch (ctrl->shared_data->hw_rev) {
			case MDSS_DSI_HW_REV_103:
				mdss_dsi_20nm_phy_regulator_enable(ctrl);
				break;
			default:
			/*
			 * For dual dsi case, do not reconfigure dsi phy
			 * regulator if the other dsi controller is still
			 * active.
			 */
			if (!mdss_dsi_is_hw_config_dual(sdata) ||
				(other_ctrl && (!other_ctrl->is_phyreg_enabled
						|| other_ctrl->mmss_clamp)))
				mdss_dsi_28nm_phy_regulator_enable(ctrl);
				break;
			}
		}
		ctrl->is_phyreg_enabled = 1;
	} else {
		/*
		 * In split-dsi/dual-dsi configuration, the dsi phy regulator
		 * should be turned off only when both the DSI devices are
		 * going to be turned off since it is shared.
		 */
		if (mdss_dsi_is_hw_config_split(ctrl->shared_data) ||
			mdss_dsi_is_hw_config_dual(ctrl->shared_data)) {
			if (other_ctrl && !other_ctrl->is_phyreg_enabled)
				mdss_dsi_phy_regulator_disable(ctrl);
		} else {
			mdss_dsi_phy_regulator_disable(ctrl);
		}
		ctrl->is_phyreg_enabled = 0;
	}
	mutex_unlock(&sdata->phy_reg_lock);
}

static void mdss_dsi_phy_ctrl(struct mdss_dsi_ctrl_pdata *ctrl, bool enable)
{
	struct mdss_dsi_ctrl_pdata *other_ctrl;
	if (!ctrl) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	if (enable) {

		if (ctrl->shared_data->phy_rev == DSI_PHY_REV_20) {
			mdss_dsi_8996_phy_config(ctrl);
		} else if (ctrl->shared_data->phy_rev == DSI_PHY_REV_30) {
			mdss_dsi_phy_v3_init(ctrl, DSI_PHY_MODE_DPHY);
		} else {
			switch (ctrl->shared_data->hw_rev) {
			case MDSS_DSI_HW_REV_103:
				mdss_dsi_20nm_phy_config(ctrl);
				break;
			default:
				mdss_dsi_28nm_phy_config(ctrl);
				break;
			}
		}
	} else {
		/*
		 * In split-dsi configuration, the phy should be disabled for
		 * the first controller only when the second controller is
		 * disabled. This is true regardless of whether broadcast
		 * mode is enabled.
		 */
		if (mdss_dsi_is_hw_config_split(ctrl->shared_data)) {
			other_ctrl = mdss_dsi_get_other_ctrl(ctrl);
			if (mdss_dsi_is_right_ctrl(ctrl) && other_ctrl) {
				mdss_dsi_phy_shutdown(other_ctrl);
				mdss_dsi_phy_shutdown(ctrl);
			}
		} else {
			mdss_dsi_phy_shutdown(ctrl);
		}
	}
}

void mdss_dsi_phy_disable(struct mdss_dsi_ctrl_pdata *ctrl)
{
	if (ctrl == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	mdss_dsi_phy_ctrl(ctrl, false);
	mdss_dsi_phy_regulator_ctrl(ctrl, false);
	/*
	 * Wait for the registers writes to complete in order to
	 * ensure that the phy is completely disabled
	 */
	wmb();
}

static void mdss_dsi_phy_init_sub(struct mdss_dsi_ctrl_pdata *ctrl)
{
	mdss_dsi_phy_regulator_ctrl(ctrl, true);
	mdss_dsi_phy_ctrl(ctrl, true);
}

void mdss_dsi_phy_init(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_dsi_ctrl_pdata *sctrl = NULL;

	/*
	 * When operating in split display mode, make sure that both the PHY
	 * blocks are initialized together prior to the PLL being enabled. This
	 * is achieved by calling the phy_init function for the clk_slave from
	 * the clock_master.
	 */
	if (mdss_dsi_is_ctrl_clk_slave(ctrl))
		return;

	mdss_dsi_phy_init_sub(ctrl);

	if (mdss_dsi_is_ctrl_clk_master(ctrl)) {
		sctrl = mdss_dsi_get_ctrl_clk_slave();
		if (sctrl)
			mdss_dsi_phy_init_sub(sctrl);
		else
			pr_warn("%s: unable to get slave ctrl\n", __func__);
	}
}

void mdss_dsi_core_clk_deinit(struct device *dev, struct dsi_shared_data *sdata)
{
	if (sdata->mmss_misc_ahb_clk)
		devm_clk_put(dev, sdata->mmss_misc_ahb_clk);
	if (sdata->ext_pixel1_clk)
		devm_clk_put(dev, sdata->ext_pixel1_clk);
	if (sdata->ext_byte1_clk)
		devm_clk_put(dev, sdata->ext_byte1_clk);
	if (sdata->ext_pixel0_clk)
		devm_clk_put(dev, sdata->ext_pixel0_clk);
	if (sdata->ext_byte0_clk)
		devm_clk_put(dev, sdata->ext_byte0_clk);
	if (sdata->axi_clk)
		devm_clk_put(dev, sdata->axi_clk);
	if (sdata->ahb_clk)
		devm_clk_put(dev, sdata->ahb_clk);
	if (sdata->mnoc_clk)
		devm_clk_put(dev, sdata->mnoc_clk);
	if (sdata->mdp_core_clk)
		devm_clk_put(dev, sdata->mdp_core_clk);
}

int mdss_dsi_clk_refresh(struct mdss_panel_data *pdata, bool update_phy)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	int rc = 0;

	if (!pdata) {
		pr_err("%s: invalid panel data\n", __func__);
		return -EINVAL;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
							panel_data);
	pinfo = &pdata->panel_info;

	if (!ctrl_pdata || !pinfo) {
		pr_err("%s: invalid ctrl data\n", __func__);
		return -EINVAL;
	}

	if (update_phy) {
		pinfo->mipi.frame_rate = mdss_panel_calc_frame_rate(pinfo);
		pr_debug("%s: new frame rate %d\n",
				__func__, pinfo->mipi.frame_rate);
	}

	rc = mdss_dsi_clk_div_config(&pdata->panel_info,
			pdata->panel_info.mipi.frame_rate);
	if (rc) {
		pr_err("%s: unable to initialize the clk dividers\n",
								__func__);
		return rc;
	}
	ctrl_pdata->refresh_clk_rate = false;
	ctrl_pdata->pclk_rate = pdata->panel_info.mipi.dsi_pclk_rate;
	ctrl_pdata->byte_clk_rate = pdata->panel_info.clk_rate / 8;
	pr_debug("%s ctrl_pdata->byte_clk_rate=%d ctrl_pdata->pclk_rate=%d\n",
		__func__, ctrl_pdata->byte_clk_rate, ctrl_pdata->pclk_rate);

	rc = mdss_dsi_clk_set_link_rate(ctrl_pdata->dsi_clk_handle,
			MDSS_DSI_LINK_BYTE_CLK, ctrl_pdata->byte_clk_rate,
			MDSS_DSI_CLK_UPDATE_CLK_RATE_AT_ON);
	if (rc) {
		pr_err("%s: dsi_byte_clk - clk_set_rate failed\n",
				__func__);
		return rc;
	}

	rc = mdss_dsi_clk_set_link_rate(ctrl_pdata->dsi_clk_handle,
			MDSS_DSI_LINK_PIX_CLK, ctrl_pdata->pclk_rate,
			MDSS_DSI_CLK_UPDATE_CLK_RATE_AT_ON);
	if (rc) {
		pr_err("%s: dsi_pixel_clk - clk_set_rate failed\n",
				__func__);
		return rc;
	}

	if (update_phy) {
		/* phy panel timing calaculation */
		rc = mdss_dsi_phy_calc_timing_param(pinfo,
				ctrl_pdata->shared_data->phy_rev,
				pinfo->mipi.frame_rate);
		if (rc) {
			pr_err("Error in calculating phy timings\n");
			return rc;
		}
		ctrl_pdata->update_phy_timing = false;
	}

	return rc;
}

int mdss_dsi_core_clk_init(struct platform_device *pdev,
	struct dsi_shared_data *sdata)
{
	struct device *dev = NULL;
	int rc = 0;

	if (!pdev) {
		pr_err("%s: Invalid pdev\n", __func__);
		goto error;
	}

	dev = &pdev->dev;

	/* Mandatory Clocks */
	sdata->mdp_core_clk = devm_clk_get(dev, "mdp_core_clk");
	if (IS_ERR(sdata->mdp_core_clk)) {
		rc = PTR_ERR(sdata->mdp_core_clk);
		pr_err("%s: Unable to get mdp core clk. rc=%d\n",
			__func__, rc);
		goto error;
	}

	sdata->ahb_clk = devm_clk_get(dev, "iface_clk");
	if (IS_ERR(sdata->ahb_clk)) {
		rc = PTR_ERR(sdata->ahb_clk);
		pr_err("%s: Unable to get mdss ahb clk. rc=%d\n",
			__func__, rc);
		goto error;
	}

	sdata->axi_clk = devm_clk_get(dev, "bus_clk");
	if (IS_ERR(sdata->axi_clk)) {
		rc = PTR_ERR(sdata->axi_clk);
		pr_err("%s: Unable to get axi bus clk. rc=%d\n",
			__func__, rc);
		goto error;
	}

	/* Optional Clocks */
	sdata->ext_byte0_clk = devm_clk_get(dev, "ext_byte0_clk");
	if (IS_ERR(sdata->ext_byte0_clk)) {
		pr_debug("%s: unable to get byte0 clk rcg. rc=%d\n",
			__func__, rc);
		sdata->ext_byte0_clk = NULL;
	}

	sdata->ext_pixel0_clk = devm_clk_get(dev, "ext_pixel0_clk");
	if (IS_ERR(sdata->ext_pixel0_clk)) {
		pr_debug("%s: unable to get pixel0 clk rcg. rc=%d\n",
			__func__, rc);
		sdata->ext_pixel0_clk = NULL;
	}

	sdata->ext_byte1_clk = devm_clk_get(dev, "ext_byte1_clk");
	if (IS_ERR(sdata->ext_byte1_clk)) {
		pr_debug("%s: unable to get byte1 clk rcg. rc=%d\n",
			__func__, rc);
		sdata->ext_byte1_clk = NULL;
	}

	sdata->ext_pixel1_clk = devm_clk_get(dev, "ext_pixel1_clk");
	if (IS_ERR(sdata->ext_pixel1_clk)) {
		pr_debug("%s: unable to get pixel1 clk rcg. rc=%d\n",
			__func__, rc);
		sdata->ext_pixel1_clk = NULL;
	}

	sdata->mmss_misc_ahb_clk = devm_clk_get(dev, "core_mmss_clk");
	if (IS_ERR(sdata->mmss_misc_ahb_clk)) {
		sdata->mmss_misc_ahb_clk = NULL;
		pr_debug("%s: Unable to get mmss misc ahb clk\n",
			__func__);
	}

	sdata->mnoc_clk = devm_clk_get(dev, "mnoc_clk");
	if (IS_ERR(sdata->mnoc_clk)) {
		pr_debug("%s: Unable to get mnoc clk\n", __func__);
		sdata->mnoc_clk = NULL;
	}

error:
	if (rc)
		mdss_dsi_core_clk_deinit(dev, sdata);
	return rc;
}

void mdss_dsi_link_clk_deinit(struct device *dev,
	struct mdss_dsi_ctrl_pdata *ctrl)
{
	if (ctrl->byte_intf_clk)
		devm_clk_put(dev, ctrl->byte_intf_clk);
	if (ctrl->vco_dummy_clk)
		devm_clk_put(dev, ctrl->vco_dummy_clk);
	if (ctrl->pixel_clk_rcg)
		devm_clk_put(dev, ctrl->pixel_clk_rcg);
	if (ctrl->byte_clk_rcg)
		devm_clk_put(dev, ctrl->byte_clk_rcg);
	if (ctrl->byte_clk)
		devm_clk_put(dev, ctrl->byte_clk);
	if (ctrl->esc_clk)
		devm_clk_put(dev, ctrl->esc_clk);
	if (ctrl->pixel_clk)
		devm_clk_put(dev, ctrl->pixel_clk);
}

int mdss_dsi_link_clk_init(struct platform_device *pdev,
	struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct device *dev = NULL;
	int rc = 0;

	if (!pdev) {
		pr_err("%s: Invalid pdev\n", __func__);
		goto error;
	}

	dev = &pdev->dev;

	/* Mandatory Clocks */
	ctrl->byte_clk = devm_clk_get(dev, "byte_clk");
	if (IS_ERR(ctrl->byte_clk)) {
		rc = PTR_ERR(ctrl->byte_clk);
		pr_err("%s: can't find dsi_byte_clk. rc=%d\n",
			__func__, rc);
		ctrl->byte_clk = NULL;
		goto error;
	}

	ctrl->pixel_clk = devm_clk_get(dev, "pixel_clk");
	if (IS_ERR(ctrl->pixel_clk)) {
		rc = PTR_ERR(ctrl->pixel_clk);
		pr_err("%s: can't find dsi_pixel_clk. rc=%d\n",
			__func__, rc);
		ctrl->pixel_clk = NULL;
		goto error;
	}

	ctrl->esc_clk = devm_clk_get(dev, "core_clk");
	if (IS_ERR(ctrl->esc_clk)) {
		rc = PTR_ERR(ctrl->esc_clk);
		pr_err("%s: can't find dsi_esc_clk. rc=%d\n",
			__func__, rc);
		ctrl->esc_clk = NULL;
		goto error;
	}

	/* Optional Clocks */
	ctrl->byte_clk_rcg = devm_clk_get(dev, "byte_clk_rcg");
	if (IS_ERR(ctrl->byte_clk_rcg)) {
		pr_debug("%s: can't find byte clk rcg. rc=%d\n", __func__, rc);
		ctrl->byte_clk_rcg = NULL;
	}

	ctrl->pixel_clk_rcg = devm_clk_get(dev, "pixel_clk_rcg");
	if (IS_ERR(ctrl->pixel_clk_rcg)) {
		pr_debug("%s: can't find pixel clk rcg. rc=%d\n", __func__, rc);
		ctrl->pixel_clk_rcg = NULL;
	}

	ctrl->vco_dummy_clk = devm_clk_get(dev, "pll_vco_dummy_clk");
	if (IS_ERR(ctrl->vco_dummy_clk)) {
		pr_debug("%s: can't find vco dummy clk. rc=%d\n", __func__, rc);
		ctrl->vco_dummy_clk = NULL;
	}

	ctrl->byte_intf_clk = devm_clk_get(dev, "byte_intf_clk");
	if (IS_ERR(ctrl->byte_intf_clk)) {
		pr_debug("%s: can't find byte int clk. rc=%d\n", __func__, rc);
		ctrl->byte_intf_clk = NULL;
	}


error:
	if (rc)
		mdss_dsi_link_clk_deinit(dev, ctrl);
	return rc;
}

void mdss_dsi_shadow_clk_deinit(struct device *dev,
	struct mdss_dsi_ctrl_pdata *ctrl)
{
	if (ctrl->mux_byte_clk)
		devm_clk_put(dev, ctrl->mux_byte_clk);
	if (ctrl->mux_pixel_clk)
		devm_clk_put(dev, ctrl->mux_pixel_clk);
	if (ctrl->pll_byte_clk)
		devm_clk_put(dev, ctrl->pll_byte_clk);
	if (ctrl->pll_pixel_clk)
		devm_clk_put(dev, ctrl->pll_pixel_clk);
	if (ctrl->shadow_byte_clk)
		devm_clk_put(dev, ctrl->shadow_byte_clk);
	if (ctrl->shadow_pixel_clk)
		devm_clk_put(dev, ctrl->shadow_pixel_clk);
}

int mdss_dsi_shadow_clk_init(struct platform_device *pdev,
		struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct device *dev = NULL;
	int rc = 0;

	if (!pdev) {
		pr_err("%s: Invalid pdev\n", __func__);
		return -EINVAL;
	}

	dev = &pdev->dev;
	ctrl->mux_byte_clk = devm_clk_get(dev, "pll_byte_clk_mux");
	if (IS_ERR(ctrl->mux_byte_clk)) {
		rc = PTR_ERR(ctrl->mux_byte_clk);
		pr_err("%s: can't find mux_byte_clk. rc=%d\n",
			__func__, rc);
		ctrl->mux_byte_clk = NULL;
		goto error;
	}

	ctrl->mux_pixel_clk = devm_clk_get(dev, "pll_pixel_clk_mux");
	if (IS_ERR(ctrl->mux_pixel_clk)) {
		rc = PTR_ERR(ctrl->mux_pixel_clk);
		pr_err("%s: can't find mdss_mux_pixel_clk. rc=%d\n",
			__func__, rc);
		ctrl->mux_pixel_clk = NULL;
		goto error;
	}

	ctrl->pll_byte_clk = devm_clk_get(dev, "pll_byte_clk_src");
	if (IS_ERR(ctrl->pll_byte_clk)) {
		rc = PTR_ERR(ctrl->pll_byte_clk);
		pr_err("%s: can't find pll_byte_clk. rc=%d\n",
			__func__, rc);
		ctrl->pll_byte_clk = NULL;
		goto error;
	}

	ctrl->pll_pixel_clk = devm_clk_get(dev, "pll_pixel_clk_src");
	if (IS_ERR(ctrl->pll_pixel_clk)) {
		rc = PTR_ERR(ctrl->pll_pixel_clk);
		pr_err("%s: can't find pll_pixel_clk. rc=%d\n",
			__func__, rc);
		ctrl->pll_pixel_clk = NULL;
		goto error;
	}

	ctrl->shadow_byte_clk = devm_clk_get(dev, "pll_shadow_byte_clk_src");
	if (IS_ERR(ctrl->shadow_byte_clk)) {
		rc = PTR_ERR(ctrl->shadow_byte_clk);
		pr_err("%s: can't find shadow_byte_clk. rc=%d\n",
			__func__, rc);
		ctrl->shadow_byte_clk = NULL;
		goto error;
	}

	ctrl->shadow_pixel_clk = devm_clk_get(dev, "pll_shadow_pixel_clk_src");
	if (IS_ERR(ctrl->shadow_pixel_clk)) {
		rc = PTR_ERR(ctrl->shadow_pixel_clk);
		pr_err("%s: can't find shadow_pixel_clk. rc=%d\n",
			__func__, rc);
		ctrl->shadow_pixel_clk = NULL;
		goto error;
	}

error:
	if (rc)
		mdss_dsi_shadow_clk_deinit(dev, ctrl);
	return rc;
}

bool is_diff_frame_rate(struct mdss_panel_info *panel_info,
	u32 frame_rate)
{
	if (panel_info->dynamic_fps && panel_info->current_fps)
		return (frame_rate != panel_info->current_fps);
	else
		return (frame_rate != panel_info->mipi.frame_rate);
}

int mdss_dsi_clk_div_config(struct mdss_panel_info *panel_info,
			    int frame_rate)
{
	struct mdss_panel_data *pdata  = container_of(panel_info,
			struct mdss_panel_data, panel_info);
	struct  mdss_dsi_ctrl_pdata *ctrl_pdata = container_of(pdata,
			struct mdss_dsi_ctrl_pdata, panel_data);
	u64 h_period, v_period, clk_rate;
	u32 dsi_pclk_rate;
	u8 lanes = 0, bpp;

	if (!panel_info)
		return -EINVAL;

	if (panel_info->mipi.data_lane3)
		lanes += 1;
	if (panel_info->mipi.data_lane2)
		lanes += 1;
	if (panel_info->mipi.data_lane1)
		lanes += 1;
	if (panel_info->mipi.data_lane0)
		lanes += 1;

	switch (panel_info->mipi.dst_format) {
	case DSI_CMD_DST_FORMAT_RGB888:
	case DSI_VIDEO_DST_FORMAT_RGB888:
	case DSI_VIDEO_DST_FORMAT_RGB666_LOOSE:
		bpp = 3;
		break;
	case DSI_CMD_DST_FORMAT_RGB565:
	case DSI_VIDEO_DST_FORMAT_RGB565:
		bpp = 2;
		break;
	default:
		bpp = 3;	/* Default format set to RGB888 */
		break;
	}

	h_period = mdss_panel_get_htotal(panel_info, true);
	if (panel_info->split_link_enabled)
		h_period *= panel_info->mipi.num_of_sublinks;
	v_period = mdss_panel_get_vtotal(panel_info);

	if (ctrl_pdata->refresh_clk_rate || is_diff_frame_rate(panel_info,
			frame_rate) || (!panel_info->clk_rate)) {
		if (lanes > 0) {
			panel_info->clk_rate = h_period * v_period * frame_rate
				* bpp * 8;
			do_div(panel_info->clk_rate, lanes);
		} else {
			pr_err("%s: forcing mdss_dsi lanes to 1\n", __func__);
			panel_info->clk_rate =
				h_period * v_period * frame_rate * bpp * 8;
		}
	}

	if (panel_info->clk_rate == 0)
		panel_info->clk_rate = 454000000;

	clk_rate = panel_info->clk_rate;
	do_div(clk_rate, 8 * bpp);

	if (panel_info->split_link_enabled)
		dsi_pclk_rate = (u32) clk_rate *
			panel_info->mipi.lanes_per_sublink;
	else
		dsi_pclk_rate = (u32) clk_rate * lanes;

	if ((dsi_pclk_rate < 3300000) || (dsi_pclk_rate > 250000000))
		dsi_pclk_rate = 35000000;
	panel_info->mipi.dsi_pclk_rate = dsi_pclk_rate;

	return 0;
}

static bool mdss_dsi_is_ulps_req_valid(struct mdss_dsi_ctrl_pdata *ctrl,
		int enable)
{
	struct mdss_dsi_ctrl_pdata *octrl = NULL;
	struct mdss_panel_data *pdata = &ctrl->panel_data;
	struct mdss_panel_info *pinfo = &pdata->panel_info;

	pr_debug("%s: checking ulps req validity for ctrl%d\n",
		__func__, ctrl->ndx);

	if (!mdss_dsi_ulps_feature_enabled(pdata) &&
			!pinfo->ulps_suspend_enabled) {
		pr_debug("%s: ULPS feature is not enabled\n", __func__);
		return false;
	}

	/*
	 * No need to enter ULPS when transitioning from splash screen to
	 * boot animation since it is expected that the clocks would be turned
	 * right back on.
	 */
	if (enable && pinfo->cont_splash_enabled) {
		pr_debug("%s: skip ULPS config with splash screen enabled\n",
			__func__);
		return false;
	}

	/*
	 * No need to enable ULPS if panel is not yet initialized.
	 * However, this should be allowed in following usecases:
	 *   1. If ULPS during suspend feature is enabled, where we
	 *      configure the lanes in ULPS after turning off the panel.
	 *   2. When coming out of idle PC with clamps enabled, where we
	 *      transition the controller HW state back to ULPS prior to
	 *      disabling ULPS.
	 */
	if (enable && !ctrl->mmss_clamp &&
		!(ctrl->ctrl_state & CTRL_STATE_PANEL_INIT) &&
		!pdata->panel_info.ulps_suspend_enabled) {
		pr_debug("%s: panel not yet initialized\n", __func__);
		return false;
	}

	/*
	 * For split-DSI usecase, wait till both controllers are initialized.
	 * The same exceptions as above are applicable here too.
	 */
	if (mdss_dsi_is_hw_config_split(ctrl->shared_data)) {
		octrl = mdss_dsi_get_other_ctrl(ctrl);
		if (enable && !ctrl->mmss_clamp && octrl &&
			!(octrl->ctrl_state & CTRL_STATE_PANEL_INIT) &&
			!pdata->panel_info.ulps_suspend_enabled) {
			pr_debug("%s: split-DSI, other ctrl not ready yet\n",
				__func__);
			return false;
		}
	}

	return true;
}

/**
 * mdss_dsi_ulps_config_default() - Program DSI lanes to enter/exit ULPS mode
 * @ctrl: pointer to DSI controller structure
 * @enable: true to enter ULPS, false to exit ULPS
 *
 * Executes the default hardware programming sequence to enter/exit DSI
 * Ultra-Low Power State (ULPS). This function would be called whenever there
 * are no hardware version sepcific functions for configuring ULPS mode. This
 * function assumes that the link and core clocks are already on.
 */
static int mdss_dsi_ulps_config_default(struct mdss_dsi_ctrl_pdata *ctrl,
	bool enable)
{
	int rc = 0;
	struct mdss_panel_data *pdata = NULL;
	struct mdss_panel_info *pinfo;
	struct mipi_panel_info *mipi;
	u32 lane_status = 0;
	u32 active_lanes = 0;

	if (!ctrl) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	pdata = &ctrl->panel_data;
	if (!pdata) {
		pr_err("%s: Invalid panel data\n", __func__);
		return -EINVAL;
	}
	pinfo = &pdata->panel_info;
	mipi = &pinfo->mipi;

	/* clock lane will always be programmed for ulps */
	active_lanes = BIT(4);
	/*
	 * make a note of all active data lanes for which ulps entry/exit
	 * is needed
	 */
	if (mipi->data_lane0)
		active_lanes |= BIT(0);
	if (mipi->data_lane1)
		active_lanes |= BIT(1);
	if (mipi->data_lane2)
		active_lanes |= BIT(2);
	if (mipi->data_lane3)
		active_lanes |= BIT(3);

	pr_debug("%s: configuring ulps (%s) for ctrl%d, active lanes=0x%08x\n",
		__func__, (enable ? "on" : "off"), ctrl->ndx, active_lanes);

	if (enable) {
		/*
		 * ULPS Entry Request.
		 * Wait for a short duration to ensure that the lanes
		 * enter ULP state.
		 */
		MIPI_OUTP(ctrl->ctrl_base + 0x0AC, active_lanes);
		usleep_range(100, 110);

		/* Check to make sure that all active data lanes are in ULPS */
		lane_status = MIPI_INP(ctrl->ctrl_base + 0xA8);
		if (lane_status & (active_lanes << 8)) {
			pr_err("%s: ULPS entry req failed for ctrl%d. Lane status=0x%08x\n",
				__func__, ctrl->ndx, lane_status);
			rc = -EINVAL;
			goto error;
		}
	} else {
		/*
		 * ULPS Exit Request
		 * Hardware requirement is to wait for at least 1ms
		 */
		MIPI_OUTP(ctrl->ctrl_base + 0x0AC, active_lanes << 8);
		usleep_range(1000, 1010);

		/*
		 * Sometimes when exiting ULPS, it is possible that some DSI
		 * lanes are not in the stop state which could lead to DSI
		 * commands not going through. To avoid this, force the lanes
		 * to be in stop state.
		 */
		MIPI_OUTP(ctrl->ctrl_base + 0x0AC, active_lanes << 16);
		wmb(); /* ensure lanes are put to stop state */

		MIPI_OUTP(ctrl->ctrl_base + 0x0AC, 0x0);
		wmb(); /* ensure lanes are in proper state */

		lane_status = MIPI_INP(ctrl->ctrl_base + 0xA8);
	}

	pr_debug("%s: DSI lane status = 0x%08x. Ulps %s\n", __func__,
		lane_status, enable ? "enabled" : "disabled");

error:
	return rc;
}

/**
 * mdss_dsi_ulps_config() - Program DSI lanes to enter/exit ULPS mode
 * @ctrl: pointer to DSI controller structure
 * @enable: 1 to enter ULPS, 0 to exit ULPS
 *
 * Execute the necessary programming sequence to enter/exit DSI Ultra-Low Power
 * State (ULPS). This function the validity of the ULPS config request and
 * executes and pre/post steps before/after the necessary hardware programming.
 * This function assumes that the link and core clocks are already on.
 */
static int mdss_dsi_ulps_config(struct mdss_dsi_ctrl_pdata *ctrl,
	int enable)
{
	int ret = 0;

	if (!ctrl) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	if (!mdss_dsi_is_ulps_req_valid(ctrl, enable)) {
		pr_debug("%s: skiping ULPS config for ctrl%d, enable=%d\n",
			__func__, ctrl->ndx, enable);
		return 0;
	}

	pr_debug("%s: configuring ulps (%s) for ctrl%d, clamps=%s\n",
		__func__, (enable ? "on" : "off"), ctrl->ndx,
		ctrl->mmss_clamp ? "enabled" : "disabled");

	if (enable && !ctrl->ulps) {
		/*
		 * Ensure that the lanes are idle prior to placing a ULPS entry
		 * request. This is needed to ensure that there is no overlap
		 * between any HS or LP commands being sent out on the lane and
		 * a potential ULPS entry request.
		 *
		 * This check needs to be avoided when we are resuming from idle
		 * power collapse and just restoring the controller state to
		 * ULPS with the clamps still in place.
		 */
		if (!ctrl->mmss_clamp) {
			ret = mdss_dsi_wait_for_lane_idle(ctrl);
			if (ret) {
				pr_warn_ratelimited("%s: lanes not idle, skip ulps\n",
					__func__);
				ret = 0;
				goto error;
			}
		}

		if (ctrl->shared_data->phy_rev == DSI_PHY_REV_30)
			ret = mdss_dsi_phy_v3_ulps_config(ctrl, true);
		else
			ret = mdss_dsi_ulps_config_default(ctrl, true);
		if (ret)
			goto error;

		ctrl->ulps = true;
	} else if (!enable && ctrl->ulps) {
		/*
		 * Clear out any phy errors prior to exiting ULPS
		 * This fixes certain instances where phy does not exit
		 * ULPS cleanly. Also, do not print error during such cases.
		 */
		mdss_dsi_dln0_phy_err(ctrl, false);

		if (ctrl->shared_data->phy_rev == DSI_PHY_REV_30)
			ret = mdss_dsi_phy_v3_ulps_config(ctrl, false);
		else
			ret = mdss_dsi_ulps_config_default(ctrl, false);
		if (ret)
			goto error;

		/*
		 * Wait for a short duration before enabling
		 * data transmission
		 */
		usleep_range(100, 100);

		ctrl->ulps = false;
	} else {
		pr_debug("%s: No change requested: %s -> %s\n", __func__,
			ctrl->ulps ? "enabled" : "disabled",
			enable ? "enabled" : "disabled");
	}

error:
	return ret;
}

/**
 * mdss_dsi_clamp_ctrl_default() - Program DSI clamps
 * @ctrl: pointer to DSI controller structure
 * @enable: true to enable clamps, false to disable clamps
 *
 * Execute the required programming sequence to configure DSI clamps.  This
 * function would be called whenever there are no hardware version sepcific
 * functions for programming the DSI clamps.  This function assumes that the
 * core clocks are already on.
 */
static int mdss_dsi_clamp_ctrl_default(struct mdss_dsi_ctrl_pdata *ctrl,
	bool enable)
{
	struct mipi_panel_info *mipi = NULL;
	u32 clamp_reg, regval = 0;
	u32 clamp_reg_off;
	u32 intf_num = 0;

	if (!ctrl) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	if (!ctrl->mmss_misc_io.base) {
		pr_err("%s: mmss_misc_io not mapped\n", __func__);
		return -EINVAL;
	}

	/*
	 * For DSI HW version 2.1.0 ULPS_CLAMP register
	 * is moved to interface level.
	 */
	if (ctrl->shared_data->hw_rev == MDSS_DSI_HW_REV_201) {
		intf_num = ctrl->ndx ? MDSS_MDP_INTF2 : MDSS_MDP_INTF1;
		if (ctrl->clamp_handler) {
			ctrl->clamp_handler->fxn(ctrl->clamp_handler->data,
				intf_num, enable);
			pr_debug("%s: ndx: %d enable: %d\n",
					__func__, ctrl->ndx, enable);
		}
		return 0;
	}

	clamp_reg_off = ctrl->shared_data->ulps_clamp_ctrl_off;
	mipi = &ctrl->panel_data.panel_info.mipi;

	/* clock lane will always be clamped */
	clamp_reg = BIT(9);
	if (ctrl->ulps)
		clamp_reg |= BIT(8);
	/* make a note of all active data lanes which need to be clamped */
	if (mipi->data_lane0) {
		clamp_reg |= BIT(7);
		if (ctrl->ulps)
			clamp_reg |= BIT(6);
	}
	if (mipi->data_lane1) {
		clamp_reg |= BIT(5);
		if (ctrl->ulps)
			clamp_reg |= BIT(4);
	}
	if (mipi->data_lane2) {
		clamp_reg |= BIT(3);
		if (ctrl->ulps)
			clamp_reg |= BIT(2);
	}
	if (mipi->data_lane3) {
		clamp_reg |= BIT(1);
		if (ctrl->ulps)
			clamp_reg |= BIT(0);
	}
	pr_debug("%s: called for ctrl%d, enable=%d, clamp_reg=0x%08x\n",
		__func__, ctrl->ndx, enable, clamp_reg);

	if (enable) {
		regval = MIPI_INP(ctrl->mmss_misc_io.base + clamp_reg_off);
		/* Enable MMSS DSI Clamps */
		if (ctrl->ndx == DSI_CTRL_0) {
			MIPI_OUTP(ctrl->mmss_misc_io.base + clamp_reg_off,
				regval | clamp_reg);
			MIPI_OUTP(ctrl->mmss_misc_io.base + clamp_reg_off,
				regval | (clamp_reg | BIT(15)));
		} else if (ctrl->ndx == DSI_CTRL_1) {
			MIPI_OUTP(ctrl->mmss_misc_io.base + clamp_reg_off,
				regval | (clamp_reg << 16));
			MIPI_OUTP(ctrl->mmss_misc_io.base + clamp_reg_off,
				regval | ((clamp_reg << 16) | BIT(31)));
		}
	} else {
		regval = MIPI_INP(ctrl->mmss_misc_io.base + clamp_reg_off);
		/* Disable MMSS DSI Clamps */
		if (ctrl->ndx == DSI_CTRL_0)
			MIPI_OUTP(ctrl->mmss_misc_io.base + clamp_reg_off,
				regval & ~(clamp_reg | BIT(15)));
		else if (ctrl->ndx == DSI_CTRL_1)
			MIPI_OUTP(ctrl->mmss_misc_io.base + clamp_reg_off,
				regval & ~((clamp_reg << 16) | BIT(31)));
	}

	/* make sure clamps are configured */
	wmb();

	return 0;
}

/**
 * mdss_dsi_clamp_ctrl() - Program DSI clamps for supporting power collapse
 * @ctrl: pointer to DSI controller structure
 * @enable: 1 to enable clamps, 0 to disable clamps
 *
 * For idle-screen usecases with command mode panels, MDSS can be power
 * collapsed. However, DSI phy needs to remain on. To avoid any mismatch
 * between the DSI controller state, DSI phy needs to be clamped before
 * power collapsing. This function executes the required programming
 * sequence to configure these DSI clamps. This function should only be called
 * when the DSI link clocks are disabled.
 */
static int mdss_dsi_clamp_ctrl(struct mdss_dsi_ctrl_pdata *ctrl, int enable)
{
	int rc = 0;

	if (!ctrl) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: called for ctrl%d, enable=%d\n", __func__, ctrl->ndx,
		enable);

	if (enable && !ctrl->mmss_clamp) {
		if (ctrl->shared_data->phy_rev < DSI_PHY_REV_30) {
			rc = mdss_dsi_clamp_ctrl_default(ctrl, true);
			if (rc)
				goto error;
		}
		ctrl->mmss_clamp = true;
	} else if (!enable && ctrl->mmss_clamp) {
		if (ctrl->shared_data->phy_rev < DSI_PHY_REV_30) {
			rc = mdss_dsi_clamp_ctrl_default(ctrl, false);
			if (rc)
				goto error;
		}
		ctrl->mmss_clamp = false;
	} else {
		pr_debug("%s: No change requested: %s -> %s\n", __func__,
			ctrl->mmss_clamp ? "enabled" : "disabled",
			enable ? "enabled" : "disabled");
	}

error:
	if (rc)
		pr_err("%s: failed to %s clamps for ctrl%d\n", __func__,
			enable ? "enable" : "disable", ctrl->ndx);

	return rc;
}

DEFINE_MUTEX(dsi_clk_mutex);

int mdss_dsi_clk_ctrl(struct mdss_dsi_ctrl_pdata *ctrl, void *clk_handle,
	enum mdss_dsi_clk_type clk_type, enum mdss_dsi_clk_state clk_state)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *mctrl = NULL;
	int i, *vote_cnt;

	void *m_clk_handle;
	bool is_ecg = false;
	int state = MDSS_DSI_CLK_OFF;

	if (!ctrl) {
		pr_err("%s: Invalid arg\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&dsi_clk_mutex);
	/*
	 * In sync_wait_broadcast mode, we need to enable clocks
	 * for the other controller as well when enabling clocks
	 * for the trigger controller.
	 *
	 * If sync wait_broadcase mode is not enabled, but if split display
	 * mode is enabled where both DSI controller's branch clocks are
	 * sourced out of a single PLL, then we need to ensure that the
	 * controller associated with that PLL also has it's clocks turned
	 * on. This is required to make sure that if that controller's PLL/PHY
	 * are clamped then they can be removed.
	 */
	if (mdss_dsi_sync_wait_trigger(ctrl)) {
		mctrl = mdss_dsi_get_other_ctrl(ctrl);
		if (!mctrl)
			pr_warn("%s: Unable to get other control\n", __func__);
	} else if (mdss_dsi_is_ctrl_clk_slave(ctrl)) {
		mctrl = mdss_dsi_get_ctrl_clk_master();
		if (!mctrl)
			pr_warn("%s: Unable to get clk master control\n",
				__func__);
	}

	/*
	 * it should add and remove extra votes based on voting clients to avoid
	 * removal of legitimate vote from DSI client.
	 */
	if (mctrl && (clk_handle == ctrl->dsi_clk_handle)) {
		m_clk_handle = mctrl->dsi_clk_handle;
		vote_cnt = &mctrl->m_dsi_vote_cnt;
	} else if (mctrl) {
		m_clk_handle = mctrl->mdp_clk_handle;
		vote_cnt = &mctrl->m_mdp_vote_cnt;
	}

	/*
	 * When DSI is used in split mode, the link clock for master controller
	 * has to be turned on first before the link clock for slave can be
	 * turned on. In case the current controller is a slave, an ON vote is
	 * cast for master before changing the state of the slave clock. After
	 * the state change for slave, the ON votes will be removed depending on
	 * the new state.
	 */
	pr_debug("%s: DSI_%d: clk = %d, state = %d, caller = %pS, mctrl=%d\n",
		 __func__, ctrl->ndx, clk_type, clk_state,
		 __builtin_return_address(0), mctrl ? 1 : 0);
	if (mctrl && (clk_type & MDSS_DSI_LINK_CLK)) {
		if (clk_state != MDSS_DSI_CLK_ON) {
			/* preserve clk state; do not turn off forcefully */
			is_ecg = is_dsi_clk_in_ecg_state(m_clk_handle);
			if (is_ecg)
				state = MDSS_DSI_CLK_EARLY_GATE;
		}

		rc = mdss_dsi_clk_req_state(m_clk_handle,
			MDSS_DSI_ALL_CLKS, MDSS_DSI_CLK_ON, mctrl->ndx);
		if (rc) {
			pr_err("%s: failed to turn on mctrl clocks, rc=%d\n",
				 __func__, rc);
			goto error;
		}
		(*vote_cnt)++;
	}

	rc = mdss_dsi_clk_req_state(clk_handle, clk_type, clk_state, ctrl->ndx);
	if (rc) {
		pr_err("%s: failed set clk state, rc = %d\n", __func__, rc);
		goto error;
	}

	if (mctrl && (clk_type & MDSS_DSI_LINK_CLK) &&
			clk_state != MDSS_DSI_CLK_ON) {

		/*
		 * In case of split dsi, an ON vote is cast for all state change
		 * requests. If the current state is ON, then the vote would not
		 * be released.
		 *
		 * If the current state is ECG, there is one possible way to
		 * transition in to this state, which is ON -> ECG. In this case
		 * two votes will be removed because one was cast at ON and
		 * other when entering ECG.
		 *
		 * If the current state is OFF, it could have been due to two
		 * possible transitions in to OFF state.
		 *	1. ON -> OFF: In this case two votes were cast by the
		 *	   slave controller, one during ON (which is not
		 *	   removed) and one during OFF. So we need to remove two
		 *	   votes.
		 *	2. ECG -> OFF: In this case there is only one vote
		 *	   for ON, since the previous ECG state must have
		 *	   removed two votes to let clocks turn off.
		 *
		 * To satisfy the above requirement, vote_cnt keeps track of
		 * the number of ON votes for master requested by slave. For
		 * every OFF/ECG state request, Either 2 or vote_cnt number of
		 * votes are removed depending on which is lower.
		 */
		for (i = 0; (i < *vote_cnt && i < 2); i++) {
			rc = mdss_dsi_clk_req_state(m_clk_handle,
				MDSS_DSI_ALL_CLKS, state, mctrl->ndx);
			if (rc) {
				pr_err("%s: failed to set mctrl clk state, rc = %d\n",
				       __func__, rc);
				goto error;
			}
		}
		(*vote_cnt) -= i;
		pr_debug("%s: ctrl=%d, vote_cnt=%d dsi_vote_cnt=%d mdp_vote_cnt:%d\n",
			__func__, ctrl->ndx, *vote_cnt, mctrl->m_dsi_vote_cnt,
			mctrl->m_mdp_vote_cnt);
	}

error:
	mutex_unlock(&dsi_clk_mutex);
	return rc;
}

int mdss_dsi_pre_clkoff_cb(void *priv,
			   enum mdss_dsi_clk_type clk,
			   enum mdss_dsi_clk_state new_state)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *ctrl = priv;
	struct mdss_panel_data *pdata = NULL;

	pdata = &ctrl->panel_data;

	if ((clk & MDSS_DSI_LINK_CLK) && (new_state == MDSS_DSI_CLK_OFF)) {
		if (pdata->panel_info.mipi.force_clk_lane_hs)
			mdss_dsi_cfg_lane_ctrl(ctrl, BIT(28), 0);
		/*
		 * If ULPS feature is enabled, enter ULPS first.
		 * However, when blanking the panel, we should enter ULPS
		 * only if ULPS during suspend feature is enabled.
		 */
		if (!(ctrl->ctrl_state & CTRL_STATE_PANEL_INIT)) {
			if (pdata->panel_info.ulps_suspend_enabled)
				mdss_dsi_ulps_config(ctrl, 1);
		} else if (mdss_dsi_ulps_feature_enabled(pdata)) {
			rc = mdss_dsi_ulps_config(ctrl, 1);
		}
		if (rc) {
			pr_err("%s: failed enable ulps, rc = %d\n",
			       __func__, rc);
		}
	}

	if ((clk & MDSS_DSI_CORE_CLK) && (new_state == MDSS_DSI_CLK_OFF)) {
		/*
		 * Enable DSI clamps only if entering idle power collapse or
		 * when ULPS during suspend is enabled.
		 */
		if ((ctrl->ctrl_state & CTRL_STATE_DSI_ACTIVE) ||
			pdata->panel_info.ulps_suspend_enabled) {
			mdss_dsi_phy_power_off(ctrl);
			rc = mdss_dsi_clamp_ctrl(ctrl, 1);
			if (rc)
				pr_err("%s: Failed to enable dsi clamps. rc=%d\n",
					__func__, rc);
		} else {
			/*
			* Make sure that controller is not in ULPS state when
			* the DSI link is not active.
			*/
			rc = mdss_dsi_ulps_config(ctrl, 0);
			if (rc)
				pr_err("%s: failed to disable ulps. rc=%d\n",
					__func__, rc);
		}
	}

	return rc;
}

static void mdss_dsi_split_link_clk_cfg(struct mdss_dsi_ctrl_pdata *ctrl,
						int enable)
{
	struct mdss_panel_data *pdata = NULL;
	void __iomem *base;
	u32 data = 0;

	if (ctrl)
		pdata = &ctrl->panel_data;
	else {
		pr_err("%s: ctrl pdata is NULL\n", __func__);
		return;
	}

	/*
	 * for split link there are two clock lanes, and
	 * both clock lanes needs to be enabled
	 */
	if (pdata->panel_info.split_link_enabled) {
		base = ctrl->phy_io.base;
		data = MIPI_INP(base + MDSS_DSI_DSIPHY_CMN_CLK_CFG1);
		data |= (enable << DSIPHY_CMN_CLK_CFG1_SPLIT_LINK);
		MIPI_OUTP(base + MDSS_DSI_DSIPHY_CMN_CLK_CFG1, data);
	}
}

int mdss_dsi_post_clkon_cb(void *priv,
			   enum mdss_dsi_clk_type clk,
			   enum mdss_dsi_clk_state curr_state)
{
	int rc = 0;
	struct mdss_panel_data *pdata = NULL;
	struct mdss_dsi_ctrl_pdata *ctrl = priv;
	bool mmss_clamp;

	pdata = &ctrl->panel_data;

	if (clk & MDSS_DSI_CORE_CLK) {
		mmss_clamp = ctrl->mmss_clamp;
		/*
		 * controller setup is needed if coming out of idle
		 * power collapse with clamps enabled.
		 */
		if (mmss_clamp)
			mdss_dsi_ctrl_setup(ctrl);

		if (ctrl->ulps && mmss_clamp) {
			/*
			 * ULPS Entry Request. This is needed if the lanes were
			 * in ULPS prior to power collapse, since after
			 * power collapse and reset, the DSI controller resets
			 * back to idle state and not ULPS. This ulps entry
			 * request will transition the state of the DSI
			 * controller to ULPS which will match the state of the
			 * DSI phy. This needs to be done prior to disabling
			 * the DSI clamps.
			 *
			 * Also, reset the ulps flag so that ulps_config
			 * function would reconfigure the controller state to
			 * ULPS.
			 */
			ctrl->ulps = false;
			rc = mdss_dsi_ulps_config(ctrl, 1);
			if (rc) {
				pr_err("%s: Failed to enter ULPS. rc=%d\n",
					__func__, rc);
				goto error;
			}
		}

		rc = mdss_dsi_clamp_ctrl(ctrl, 0);
		if (rc) {
			pr_err("%s: Failed to disable dsi clamps. rc=%d\n",
				__func__, rc);
			goto error;
		}

		/*
		 * Phy setup is needed if coming out of idle
		 * power collapse with clamps enabled.
		 */
		if (ctrl->phy_power_off || mmss_clamp)
			mdss_dsi_phy_power_on(ctrl, mmss_clamp);
	}
	if (clk & MDSS_DSI_LINK_CLK) {
		/* toggle the resync FIFO everytime clock changes */
		if (ctrl->shared_data->phy_rev == DSI_PHY_REV_30)
			mdss_dsi_phy_v3_toggle_resync_fifo(ctrl);

		if (ctrl->ulps) {
			rc = mdss_dsi_ulps_config(ctrl, 0);
			if (rc) {
				pr_err("%s: failed to disable ulps, rc= %d\n",
				       __func__, rc);
				goto error;
			}
		}
		if (pdata->panel_info.mipi.force_clk_lane_hs)
			mdss_dsi_cfg_lane_ctrl(ctrl, BIT(28), 1);

		/* enable split link for cmn clk cfg1 */
		mdss_dsi_split_link_clk_cfg(ctrl, 1);
	}
error:
	return rc;
}

int mdss_dsi_post_clkoff_cb(void *priv,
			    enum mdss_dsi_clk_type clk_type,
			    enum mdss_dsi_clk_state curr_state)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *ctrl = priv;
	struct mdss_panel_data *pdata = NULL;
	struct dsi_shared_data *sdata;
	int i;

	if (!ctrl) {
		pr_err("%s: Invalid arg\n", __func__);
		return -EINVAL;
	}

	if ((clk_type & MDSS_DSI_CORE_CLK) &&
	    (curr_state == MDSS_DSI_CLK_OFF)) {
		sdata = ctrl->shared_data;
		pdata = &ctrl->panel_data;

		for (i = DSI_MAX_PM - 1; i >= DSI_CORE_PM; i--) {
			if ((ctrl->ctrl_state & CTRL_STATE_DSI_ACTIVE) &&
				(i != DSI_CORE_PM))
				continue;
			rc = msm_dss_enable_vreg(
				sdata->power_data[i].vreg_config,
				sdata->power_data[i].num_vreg, 0);
			if (rc) {
				pr_warn("%s: failed to disable vregs for %s\n",
					__func__,
					__mdss_dsi_pm_name(i));
				rc = 0;
			} else {
				ctrl->core_power = false;
			}
		}
	}
	return rc;
}

int mdss_dsi_pre_clkon_cb(void *priv,
			  enum mdss_dsi_clk_type clk_type,
			  enum mdss_dsi_clk_state new_state)
{
	int rc = 0;
	struct mdss_dsi_ctrl_pdata *ctrl = priv;
	struct mdss_panel_data *pdata = NULL;
	struct dsi_shared_data *sdata;
	int i;

	if (!ctrl) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	if ((clk_type & MDSS_DSI_CORE_CLK) && (new_state == MDSS_DSI_CLK_ON) &&
	    (ctrl->core_power == false)) {
		sdata = ctrl->shared_data;
		pdata = &ctrl->panel_data;
		/*
		 * Enable DSI core power
		 * 1.> PANEL_PM are controlled as part of
		 *     panel_power_ctrl. Needed not be handled here.
		 * 2.> CORE_PM are controlled by dsi clk manager.
		 * 3.> CTRL_PM need to be enabled/disabled
		 *     only during unblank/blank. Their state should
		 *     not be changed during static screen.
		 */
		pr_debug("%s: Enable DSI core power\n", __func__);
		for (i = DSI_CORE_PM; i < DSI_MAX_PM; i++) {
			if ((ctrl->ctrl_state & CTRL_STATE_DSI_ACTIVE) &&
				(!pdata->panel_info.cont_splash_enabled) &&
				(i != DSI_CORE_PM))
				continue;
			rc = msm_dss_enable_vreg(
				sdata->power_data[i].vreg_config,
				sdata->power_data[i].num_vreg, 1);
			if (rc) {
				pr_err("%s: failed to enable vregs for %s\n",
					__func__,
					__mdss_dsi_pm_name(i));
			} else {
				ctrl->core_power = true;
			}

		}
	}

	return rc;
}
