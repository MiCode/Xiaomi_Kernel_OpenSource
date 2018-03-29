/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __DDP_DPI_REG_H__
#define __DDP_DPI_REG_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct {
		unsigned EN:1;
		unsigned rsv_1:31;
	} DPI_REG_EN, *PDPI_REG_EN;

	typedef struct {
		unsigned RST:1;
		unsigned rsv_1:31;
	} DPI_REG_RST, *PDPI_REG_RST;

	typedef struct {
		unsigned VSYNC:1;
		unsigned VDE:1;
		unsigned UNDERFLOW:1;
		unsigned rsv_3:29;
	} DPI_REG_INTERRUPT, *PDPI_REG_INTERRUPT;

	typedef struct {
		unsigned BG_EN:1;
		unsigned RGB_SWAP:1;
		unsigned INTL_EN:1;
		unsigned TDFP_EN:1;
		unsigned CLPF_EN:1;
		unsigned YUV422_EN:1;
		unsigned RGB2YUV_EN:1;
		unsigned R601_SEL:1;
		unsigned EMBSYNC_EN:1;
		unsigned rsv_9:3;
		unsigned PIXREP:4;	/* new */
		unsigned VS_LODD_EN:1;
		unsigned VS_LEVEN_EN:1;
		unsigned VS_RODD_EN:1;
		unsigned VS_REVEN_EN:1;
		unsigned FAKE_DE_LODD:1;
		unsigned FAKE_DE_LEVEN:1;
		unsigned FAKE_DE_RODD:1;
		unsigned FAKE_DE_REVEN:1;
		unsigned rsv_24:8;
	} DPI_REG_CNTL, *PDPI_REG_CNTL;

	typedef struct {
		unsigned CH_SWAP:3;
		unsigned BIT_SWAP:1;
		unsigned B_MASK:1;
		unsigned G_MASK:1;
		unsigned R_MASK:1;
		unsigned rsv_7:1;
		unsigned DE_MASK:1;
		unsigned HS_MASK:1;
		unsigned VS_MASK:1;
		unsigned rsv_11:1;
		unsigned DE_POL:1;
		unsigned HSYNC_POL:1;
		unsigned VSYNC_POL:1;
		unsigned CLK_POL:1;
		unsigned DPI_O_EN:1;
		unsigned DUAL_EDGE_SEL:1;
		unsigned OUT_BIT:2;
		unsigned YC_MAP:3;
		unsigned rsv_23:9;
	} DPI_REG_OUTPUT_SETTING, *PDPI_REG_OUTPUT_SETTING;

	typedef struct {
		unsigned WIDTH:13;
		unsigned rsv_13:3;
		unsigned HEIGHT:13;
		unsigned rsv_28:3;
	} DPI_REG_SIZE, *PDPI_REG_SIZE;

	typedef struct {
		unsigned DDR_EN:1;
		unsigned DDR_SEL:1;
		unsigned DDR_4PHASE:1;
		unsigned DATA_THROT:1;	/* new */
		unsigned DDR_WIDTH:2;
		unsigned rsv_6:2;
		unsigned DDR_PAD_MODE:1;
		unsigned rsv_9:23;
	} DPI_REG_DDR_SETTING, *PDPI_REG_DDR_SETTING;

	typedef struct {
		unsigned HBP:12;
		unsigned rsv_12:4;
		unsigned HFP:12;
		unsigned rsv_28:4;
	} DPI_REG_TGEN_HPORCH, *PDPI_REG_TGEN_HPORCH;

	typedef struct {
		unsigned VPW_LODD:12;
		unsigned rsv_12:4;
		unsigned VPW_HALF_LODD:1;
		unsigned rsv_17:15;
	} DPI_REG_TGEN_VWIDTH_LODD, *PDPI_REG_TGEN_VWIDTH_LODD;

	typedef struct {
		unsigned VBP_LODD:12;
		unsigned rsv_12:4;
		unsigned VFP_LODD:12;
		unsigned rsv_28:4;
	} DPI_REG_TGEN_VPORCH_LODD, *PDPI_REG_TGEN_VPORCH_LODD;

	typedef struct {
		unsigned BG_RIGHT:13;
		unsigned rsv_13:3;
		unsigned BG_LEFT:13;
		unsigned rsv_29:3;
	} DPI_REG_BG_HCNTL, *PDPI_REG_BG_HCNTL;

	typedef struct {
		unsigned BG_BOT:13;
		unsigned rsv_13:3;
		unsigned BG_TOP:13;
		unsigned rsv_29:3;
	} DPI_REG_BG_VCNTL, *PDPI_REG_BG_VCNTL;


	typedef struct {
		unsigned BG_B:8;
		unsigned BG_G:8;
		unsigned BG_R:8;
		unsigned rsv_24:8;
	} DPI_REG_BG_COLOR, *PDPI_REG_BG_COLOR;

	typedef struct {
		unsigned FIFO_VALID_SET:5;
		unsigned rsv_5:3;
		unsigned FIFO_RST_SEL:1;
		unsigned rsv_9:23;
	} DPI_REG_FIFO_CTL, *PDPI_REG_FIFO_CTL;

	typedef struct {
		unsigned V_CNT:13;
		unsigned rsv_13:3;
		unsigned DPI_BUSY:1;
		unsigned OUT_EN:1;
		unsigned rsv_18:2;
		unsigned FIELD:1;
		unsigned TDLR:1;
		unsigned rsv_22:10;
	} DPI_REG_STATUS, *PDPI_REG_STATUS;

	typedef struct {
		unsigned OEN_EN:1;
		unsigned rsv_1:31;
	} DPI_REG_TMODE, *PDPI_REG_TMODE;

	typedef struct {
		unsigned CHKSUM:24;
		unsigned rsv_24:6;
		unsigned CHKSUM_RDY:1;
		unsigned CHKSUM_EN:1;
	} DPI_REG_CHKSUM, *PDPI_REG_CHKSUM;

	typedef struct {
		unsigned VPW_LEVEN:12;
		unsigned rsv_12:4;
		unsigned VPW_HALF_LEVEN:1;
		unsigned rsv_17:15;
	} DPI_REG_TGEN_VWIDTH_LEVEN, *PDPI_REG_TGEN_VWIDTH_LEVEN;

	typedef struct {
		unsigned VBP_LEVEN:12;
		unsigned rsv_12:4;
		unsigned VFP_LEVEN:12;
		unsigned rsv_28:4;
	} DPI_REG_TGEN_VPORCH_LEVEN, *PDPI_REG_TGEN_VPORCH_LEVEN;

	typedef struct {
		unsigned VPW_RODD:12;
		unsigned rsv_12:4;
		unsigned VPW_HALF_RODD:1;
		unsigned rsv_17:15;
	} DPI_REG_TGEN_VWIDTH_RODD, *PDPI_REG_TGEN_VWIDTH_RODD;

	typedef struct {
		unsigned VBP_RODD:12;
		unsigned rsv_12:4;
		unsigned VFP_RODD:12;
		unsigned rsv_28:4;
	} DPI_REG_TGEN_VPORCH_RODD, *PDPI_REG_TGEN_VPORCH_RODD;

	typedef struct {
		unsigned VPW_REVEN:12;
		unsigned rsv_12:4;
		unsigned VPW_HALF_REVEN:1;
		unsigned rsv_17:15;
	} DPI_REG_TGEN_VWIDTH_REVEN, *PDPI_REG_TGEN_VWIDTH_REVEN;

	typedef struct {
		unsigned VBP_REVEN:12;
		unsigned rsv_12:4;
		unsigned VFP_REVEN:12;
		unsigned rsv_28:4;
	} DPI_REG_TGEN_VPORCH_REVEN, *PDPI_REG_TGEN_VPORCH_REVEN;

	typedef struct {
		unsigned ESAV_VOFST_LODD:12;
		unsigned rsv_12:4;
		unsigned ESAV_VWID_LODD:12;
		unsigned rsv_28:4;
	} DPI_REG_ESAV_VTIM_LOAD, *PDPI_REG_ESAV_VTIM_LOAD;

	typedef struct {
		unsigned ESAV_VVOFST_LEVEN:12;
		unsigned rsv_12:4;
		unsigned ESAV_VWID_LEVEN:12;
		unsigned rsv_28:4;
	} DPI_REG_ESAV_VTIM_LEVEN, *PDPI_REG_ESAV_VTIM_LEVEN;

	typedef struct {
		unsigned ESAV_VOFST_RODD:12;
		unsigned rsv_12:4;
		unsigned ESAV_VWID_RODD:12;
		unsigned rsv_28:4;
	} DPI_REG_ESAV_VTIM_ROAD, *PDPI_REG_ESAV_VTIM_ROAD;

	typedef struct {
		unsigned ESAV_VOFST_REVEN:12;
		unsigned rsv_12:4;
		unsigned ESAV_VWID_REVEN:12;
		unsigned rsv_28:4;
	} DPI_REG_ESAV_VTIM_REVEN, *PDPI_REG_ESAV_VTIM_REVEN;


	typedef struct {
		unsigned ESAV_FOFST_ODD:12;
		unsigned rsv_12:4;
		unsigned ESAV_FOFST_EVEN:12;
		unsigned rsv_28:4;
	} DPI_REG_ESAV_FTIM, *PDPI_REG_ESAV_FTIM;

	typedef struct {
		unsigned CLPF_TYPE:2;
		unsigned rsv2:2;
		unsigned ROUND_EN:1;
		unsigned rsv5:27;
	} DPI_REG_CLPF_SETTING, *PDPI_REG_CLPF_SETTING;

	typedef struct {
		unsigned Y_LIMIT_BOT:12;
		unsigned rsv12:4;
		unsigned Y_LIMIT_TOP:12;
		unsigned rsv28:4;
	} DPI_REG_Y_LIMIT, *PDPI_REG_Y_LIMIT;

	typedef struct {
		unsigned C_LIMIT_BOT:12;
		unsigned rsv12:4;
		unsigned C_LIMIT_TOP:12;
		unsigned rsv28:4;
	} DPI_REG_C_LIMIT, *PDPI_REG_C_LIMIT;

	typedef struct {
		unsigned UV_SWAP:1;
		unsigned rsv1:3;
		unsigned CR_DELSEL:1;
		unsigned CB_DELSEL:1;
		unsigned Y_DELSEL:1;
		unsigned DE_DELSEL:1;
		unsigned rsv8:24;
	} DPI_REG_YUV422_SETTING, *PDPI_REG_YUV422_SETTING;

	typedef struct {
		unsigned EMBVSYNC_R_CR:1;
		unsigned EMBVSYNC_G_Y:1;
		unsigned EMBVSYNC_B_CB:1;
		unsigned rsv_3:1;
		unsigned ESAV_F_INV:1;
		unsigned ESAV_V_INV:1;
		unsigned ESAV_H_INV:1;
		unsigned rsv_7:1;
		unsigned ESAV_CODE_MAN:1;
		unsigned rsv_9:3;
		unsigned VS_OUT_SEL:3;
		unsigned rsv_15:1;
		unsigned EMBSYNC_OPT:1;	/* new */
		unsigned rsv_17:15;
	} DPI_REG_EMBSYNC_SETTING;

	typedef struct {
		unsigned ESAV_CODE0:12;
		unsigned rsv_12:4;
		unsigned ESAV_CODE1:12;
		unsigned rsv_28:4;
	} DPI_REG_ESAV_CODE_SET0, *PDPI_REG_ESAV_CODE_SET0;

	typedef struct {
		unsigned ESAV_CODE2:12;
		unsigned rsv_12:4;
		unsigned ESAV_CODE3_MSB:1;
		unsigned rsv_17:15;
	} DPI_REG_ESAV_CODE_SET1, *PDPI_REG_ESAV_CODE_SET1;

	typedef struct {
		unsigned PAT_EN:1;
		unsigned rsv_1:3;
		unsigned PAT_SEL:3;
		unsigned rsv_6:1;
		unsigned PAT_B_MAN:8;
		unsigned PAT_G_MAN:8;
		unsigned PAT_R_MAN:8;
	} DPI_REG_PATTERN;

/* <--not be used */
	typedef struct {
		unsigned HPW:8;
		unsigned HBP:8;
		unsigned HFP:8;
		unsigned HSYNC_POL:1;
		unsigned DE_POL:1;
		unsigned rsv_26:6;
	} DPI_REG_TGEN_HCNTL, *PDPI_REG_TGEN_HCNTL;

	typedef struct {
		unsigned VPW:8;
		unsigned VBP:8;
		unsigned VFP:8;
		unsigned VSYNC_POL:1;
		unsigned rsv_25:7;
	} DPI_REG_TGEN_VCNTL, *PDPI_REG_TGEN_VCNTL;
/* --> */

	typedef struct {
		unsigned MATRIX_C00:13;
		unsigned rsv_13:3;
		unsigned MATRIX_C01:13;
		unsigned rsv_29:3;
	} DPI_REG_MATRIX_COEFF_SET0;

	typedef struct {
		unsigned MATRIX_C02:13;
		unsigned rsv_13:3;
		unsigned MATRIX_C10:13;
		unsigned rsv_29:3;
	} DPI_REG_MATRIX_COEFF_SET1;

	typedef struct {
		unsigned MATRIX_C11:13;
		unsigned rsv_13:3;
		unsigned MATRIX_C12:13;
		unsigned rsv_29:3;
	} DPI_REG_MATRIX_COEFF_SET2;

	typedef struct {
		unsigned MATRIX_C20:13;
		unsigned rsv_13:3;
		unsigned MATRIX_C21:13;
		unsigned rsv_29:3;
	} DPI_REG_MATRIX_COEFF_SET3;

	typedef struct {
		unsigned MATRIX_C22:13;
		unsigned rsv_13:19;
	} DPI_REG_MATRIX_COEFF_SET4;

	typedef struct {
		unsigned MATRIX_PRE_ADD_0:9;
		unsigned rsv_9:7;
		unsigned MATRIX_PRE_ADD_1:9;
		unsigned rsv_24:7;
	} DPI_REG_MATRIX_PREADD_SET0;

	typedef struct {
		unsigned MATRIX_PRE_ADD_2:9;
		unsigned rsv_9:23;
	} DPI_REG_MATRIX_PREADD_SET1;

	typedef struct {
		unsigned MATRIX_POST_ADD_0:13;
		unsigned rsv_13:3;
		unsigned MATRIX_POST_ADD_1:13;
		unsigned rsv_24:3;
	} DPI_REG_MATRIX_POSTADD_SET0;

	typedef struct {
		unsigned MATRIX_POST_ADD_2:13;
		unsigned rsv_13:19;
	} DPI_REG_MATRIX_POSTADD_SET1;

	typedef struct {
		unsigned DPI_CK_DIV:5;
		unsigned EDGE_SEL_EN:1;
		unsigned rsv_6:2;
		unsigned DPI_CK_DUT:5;
		unsigned rsv_13:12;
		unsigned DPI_CKOUT_DIV:1;
		unsigned rsv_26:5;
		unsigned DPI_CK_POL:1;
	} DPI_REG_CLKCNTL, *PDPI_REG_CLKCNTL;

	typedef struct {
		DPI_REG_EN DPI_EN;	/* 0000 */
		DPI_REG_RST DPI_RST;	/* 0004 */
		DPI_REG_INTERRUPT INT_ENABLE;	/* 0008 */
		DPI_REG_INTERRUPT INT_STATUS;	/* 000C */
		DPI_REG_CNTL CNTL;	/* 0010 */
		DPI_REG_OUTPUT_SETTING OUTPUT_SETTING;	/* 0014 */
		DPI_REG_SIZE SIZE;	/* 0018 */
		DPI_REG_DDR_SETTING DDR_SETTING;	/* 001c */
		UINT32 TGEN_HWIDTH;	/* 0020 */
		DPI_REG_TGEN_HPORCH TGEN_HPORCH;	/* 0024 */
		DPI_REG_TGEN_VWIDTH_LODD TGEN_VWIDTH_LODD;	/* 0028 */
		DPI_REG_TGEN_VPORCH_LODD TGEN_VPORCH_LODD;	/* 002C */
		DPI_REG_BG_HCNTL BG_HCNTL;	/* 0030 */
		DPI_REG_BG_VCNTL BG_VCNTL;	/* 0034 */
		DPI_REG_BG_COLOR BG_COLOR;	/* 0038 */
		DPI_REG_FIFO_CTL FIFO_CTL;	/* 003C */

		DPI_REG_STATUS STATUS;	/* 0040 */
		DPI_REG_TMODE TMODE;	/* 0044 */
		DPI_REG_CHKSUM CHKSUM;	/* 0048 */
		UINT32 rsv_4C;
		UINT32 DUMMY;	/* 0050 */
		UINT32 rsv_54[5];
		DPI_REG_TGEN_VWIDTH_LEVEN TGEN_VWIDTH_LEVEN;	/* 0068 */
		DPI_REG_TGEN_VPORCH_LEVEN TGEN_VPORCH_LEVEN;	/* 006C */
		DPI_REG_TGEN_VWIDTH_RODD TGEN_VWIDTH_RODD;	/* 0070 */
		DPI_REG_TGEN_VPORCH_RODD TGEN_VPORCH_RODD;	/* 0074 */
		DPI_REG_TGEN_VWIDTH_REVEN TGEN_VWIDTH_REVEN;	/* 0078 */
		DPI_REG_TGEN_VPORCH_REVEN TGEN_VPORCH_REVEN;	/* 007C */
		DPI_REG_ESAV_VTIM_LOAD ESAV_VTIM_LOAD;	/* 0080 */
		DPI_REG_ESAV_VTIM_LEVEN ESAV_VTIM_LEVEN;	/* 0084 */
		DPI_REG_ESAV_VTIM_ROAD ESAV_VTIM_ROAD;	/* 0088 */
		DPI_REG_ESAV_VTIM_REVEN ESAV_VTIM_REVEN;	/* 008C */
		DPI_REG_ESAV_FTIM ESAV_FTIM;	/* 0090 */
		DPI_REG_CLPF_SETTING CLPF_SETTING;	/* 0094 */
		DPI_REG_Y_LIMIT Y_LIMIT;	/* 0098 */
		DPI_REG_C_LIMIT C_LIMIT;	/* 009C */
		DPI_REG_YUV422_SETTING YUV422_SETTING;	/* 00A0 */
		DPI_REG_EMBSYNC_SETTING EMBSYNC_SETTING;	/* 00A4 */
		DPI_REG_ESAV_CODE_SET0 ESAV_CODE_SET0;	/* 00A8 */
		DPI_REG_ESAV_CODE_SET1 ESAV_CODE_SET1;	/* 00AC */
		UINT32 rsv_b0[12];
		DPI_REG_CLKCNTL DPI_CLKCON;	/* 00E0 */
	} volatile DPI_REGS, *PDPI_REGS;

/* LVDS TOPB_REG define */
	typedef struct {	/* 14026000 */
		unsigned CKEN_CFG_0:1;
		unsigned rsv_1:2;
		unsigned CKEN_CFG_3:1;
		unsigned rsv_4:3;
		unsigned CKEN_CFG_7:1;
		unsigned CKEN_CFG_8:1;
		unsigned rsv_9:7;
		unsigned CKRST_CFG_0:1;
		unsigned rsv_17:2;
		unsigned CKRST_CFG_3:1;
		unsigned rsv_20:3;
		unsigned CKRST_CFG_7:1;
		unsigned CKRST_CFG_8:1;
		unsigned rsv_25:1;
		unsigned CKRST_CFG_10:1;
		unsigned rsv_27:5;
	} LVDS_TOP_REG00, *PLVDS_TOP_REG00;

	typedef struct {	/* 14026004 */
		unsigned rsv_0:4;
		unsigned EH_MON_SRC_SEL:4;
		unsigned LVDS_MON_SRC_SEL:8;
		unsigned rsv_16:16;
	} LVDS_TOP_REG01, *PLVDS_TOP_REG01;

	typedef struct {	/* 14026008 */
		unsigned rsv_0:4;
		unsigned CKEN_CFG1_4:1;
		unsigned CKEN_CFG1_5:1;
		unsigned CKEN_CFG1_6:1;
		unsigned rsv_7:25;
	} LVDS_TOP_REG02, *PLVDS_TOP_REG02;

	typedef struct {	/* 1402600c */
		unsigned TCON_VS_OUT_P:1;
		unsigned TCON_HS_OUT_P:1;
		unsigned TCON_DE_OUT_P:1;
		unsigned rsv_3:1;
		unsigned TCON_VS_PWM_P:1;
		unsigned TCON_HS_PWM_P:1;
		unsigned rsv_6:26;
	} LVDS_TOP_REG03, *PLVDS_TOP_REG03;

	typedef struct {	/* 14026010 */
		unsigned RG_LVDSTX_CRC_CLR:1;
		unsigned RG_LVDSTX_CRC_START:1;
		unsigned RG_LVDSTX_CK_EN:1;
		unsigned RG_LVDSTX_IN_CRC_CLR:1;
		unsigned RG_LVDSTX_IN_CRC_START:1;
		unsigned RG_LVDS_CRC_SEL:1;
		unsigned rsv_6:26;
	} LVDS_TOP_REG04, *PLVDS_TOP_REG04;

	typedef struct {	/* 14026014 */
		unsigned RG_LVDS_ODD_SEL:2;
		unsigned RG_LVDS_EVEN_SEL:2;
		unsigned RG_LVDS_CLK_SEL:2;
		unsigned RG_LVDS_CLK_INV:2;
		unsigned DATA_FORMAT:4;
		unsigned rsv_12:1;
		unsigned _6BIT_SOURCE_SEL:1;
		unsigned rsv_14:2;
		unsigned RG_FIFO_EN:2;
		unsigned rsv_18:2;
		unsigned REG_FIFO_CTRL:3;
		unsigned LVDS_CLKDIV_CTRL:4;
		unsigned rsv_27:5;
	} LVDS_TOP_REG05, *PLVDS_TOP_REG05;


/* LVDS PANELB_REG define */
	typedef struct {	/* 14026604 */
		unsigned RG_PTGEN_H_TOTAL:13;
		unsigned rsv_13:3;
		unsigned RG_PTGEN_H_ACTIVE:13;
		unsigned rsv_29:3;
	} PANELB_PATGEN0, *PPANELB_PATGEN0;

	typedef struct {	/* 14026608 */
		unsigned RG_PTGEN_V_TOTAL:12;
		unsigned rsv_12:4;
		unsigned RG_PTGEN_V_ACTIVE:12;
		unsigned rsv_28:4;
	} PANELB_PATGEN1, *PPANELB_PATGEN1;

	typedef struct {	/* 1402660c */
		unsigned RG_PTGEN_V_START:12;
		unsigned rsv_12:4;
		unsigned RG_PTGEN_H_START:13;
		unsigned rsv_29:3;
	} PANELB_PATGEN2, *PPANELB_PATGEN2;

	typedef struct {	/* 14026610 */
		unsigned RG_PTGEN_V_WIDTH:12;
		unsigned rsv_12:4;
		unsigned RG_PTGEN_H_WIDTH:13;
		unsigned rsv_29:3;
	} PANELB_PATGEN3, *PPANELB_PATGEN3;

	typedef struct {	/* 14026614 */
		unsigned RG_PTGEN_B:10;
		unsigned RG_PTGEN_G:10;
		unsigned RG_PTGEN_R:10;
		unsigned rsv_30:2;
	} PANELB_PATGEN4, *PPANELB_PATGEN4;

	typedef struct {	/* 14026618 */
		unsigned RG_PTGEN_COLOR_BAR_TH:12;
		unsigned RG_PTGEN_FIX_DISP_R_EN:1;
		unsigned RG_PTGEN_FIX_DISP_R_VAL:1;
		unsigned rsv_14:2;
		unsigned RG_PTGEN_EN:1;
		unsigned RG_PTGEN_2CH_EN:1;
		unsigned RG_PTGEN_SEQ:1;
		unsigned RG_PTGEN_MIRROR:1;
		unsigned rsv_20:1;
		unsigned RG_INTF_PTGEN_EN:1;
		unsigned rsv_22:2;
		unsigned RG_PTGEN_TYPE:8;
	} PANELB_PATGEN5, *PPANELB_PATGEN5;

	typedef struct {	/* 14026620 */
		unsigned RG_PTGEN_BD_B:10;
		unsigned RG_PTGEN_BD_G:10;
		unsigned RG_PTGEN_BD_R:10;
		unsigned rsv_30:2;
	} PANELB_PATGEN6, *PPANELB_PATGEN6;


/* LVDS PANELP_REG define */
	typedef struct {	/* 14026700 */
		unsigned RG_VSYNC_IN_P:1;
		unsigned RG_HSYNC_IN_P:1;
		unsigned RG_DE_IN_P:1;
		unsigned RG_DISPR_IN_P:1;
		unsigned rsv_4:4;
		unsigned RG_PANEL_IN_R_SWAP:2;
		unsigned RG_PANEL_IN_G_SWAP:2;
		unsigned RG_PANEL_IN_B_SWAP:2;
		unsigned rsv_14:18;
	} PANELP_CFG0, *PPANELP_CFG0;

	typedef struct {	/* 14026704 */
		unsigned rsv_0:30;
		unsigned RG_V_TOTAL_CLR:1;
		unsigned RG_H_TOTAL_CLR:1;
	} PANELP_DETECT0, *PPANELP_DETECT0;

	typedef struct {	/* 14026708 */
		unsigned MIN_H_TOTAL:13;
		unsigned rsv_13:3;
		unsigned MAX_H_TOTAL:13;
		unsigned rsv_29:3;
	} PANELP_DETECT1, *PPANELP_DETECT1;

	typedef struct {	/* 1402670c */
		unsigned MIN_H_ACTIVE:12;
		unsigned rsv_12:4;
		unsigned MAX_H_ACTIVE:12;
		unsigned rsv_28:4;
	} PANELP_DETECT2, *PPANELP_DETECT2;

	typedef struct {	/* 14026714 */
		unsigned EVEN_ODD_INFO:1;
		unsigned TCON_3DL_PWM:1;
		unsigned TCON_3DR_PWM:1;
		unsigned TCON_3DLR_PWM:1;
		unsigned TCON_3DBL_PWM:1;
		unsigned rsv_5:27;
	} PANELP_3D_CTRL_MON, *PPANELP_3D_CTRL_MON;

	typedef struct {	/* 14026738 */
		unsigned CRC_H_START:16;
		unsigned CRC_H_END:16;
	} PANELP_CRC_CHECK_0, *PPANELP_CRC_CHECK_0;

	typedef struct {	/* 1402673c */
		unsigned CRC_START:1;
		unsigned CRC_CLR:1;
		unsigned rsv_2:6;
		unsigned CRC_MODE:2;
		unsigned CRC_ALG_SEL:1;
		unsigned rsv_11:5;
		unsigned CRC_VCNT:16;
	} PANELP_CRC_CHECK_1, *PPANELP_CRC_CHECK_1;

	typedef struct {	/* 14026740 */
		unsigned CRC_DONE:1;
		unsigned CRC_FAIL:1;
		unsigned rsv_2:30;
	} PANELP_CRC_CHECK_2, *PPANELP_CRC_CHECK_2;

	typedef struct {	/* 14026750 */
		unsigned MIN_V_TOTAL:12;
		unsigned rsv_12:4;
		unsigned MAX_V_TOTAL:12;
		unsigned rsv_28:4;
	} PANELP_DETECT3, *PPANELP_DETECT3;

	typedef struct {	/* 14026754 */
		unsigned MIN_V_ACTIVE:12;
		unsigned rsv_12:4;
		unsigned MAX_V_ACTIVE:12;
		unsigned rsv_28:4;
	} PANELP_DETECT4, *PPANELP_DETECT4;


/* LVDSTXB_REG define */
	typedef struct {	/* 14026800 */
		unsigned rsv_0:22;
		unsigned RG_LVDSTX_MON_SEL:7;
		unsigned rsv_29:3;
	} LVDSTXB_MODE0, *PLVDSTXB_MODE0;

	typedef struct {	/* 14026814 */
		unsigned RG_LVDS_INV:1;
		unsigned RG_PNSWAP:1;
		unsigned rsv_2:30;
	} LVDSTXB_LVDS_CTRL, *PLVDSTXB_LVDS_CTRL;

	typedef struct {	/* 14026824 */
		unsigned RG_MLVDS_ANA_FORCE:10;
		unsigned rsv_10:5;
		unsigned RG_MLVDS_ANA_TEST_EN:1;
		unsigned rsv_16:16;
	} LVDSTXB_ANA_TEST, *PLVDSTXB_ANA_TEST;


/* LVDSTXT_REG define */
	typedef struct {	/* 14026904 */
		unsigned RG_LLV0_SEL:4;
		unsigned RG_LLV1_SEL:4;
		unsigned RG_LLV2_SEL:4;
		unsigned RG_LLV3_SEL:4;
		unsigned RG_LLV4_SEL:4;
		unsigned RG_LLV5_SEL:4;
		unsigned RG_LLV6_SEL:4;
		unsigned RG_LLV7_SEL:4;
	} LVDSTXT_LLV_DO_SEL, *PLVDSTXT_LLV_DO_SEL;

	typedef struct {	/* 1402690c */
		unsigned rsv_0:8;
		unsigned RG_LLV8_SEL:4;
		unsigned RG_LLV9_SEL:4;
		unsigned rsv_16:8;
		unsigned RG_LLV_CK0_SEL:4;
		unsigned RG_LLV_CK1_SEL:4;
	} LVDSTXT_CKO_SEL, *PLVDSTXT_CKO_SEL;

	typedef struct {	/* 14026930 */
		unsigned rsv_0:16;
		unsigned PN_SWAP:12;
		unsigned rsv_28:4;
	} LVDSTXT_PN_SWAP, *PLVDSTXT_PN_SWAP;

	typedef struct {	/* 14026934 */
		unsigned RG_TOP_CRC_V_CNT:16;
		unsigned EG_TOP_CRC_MODE:2;
		unsigned rsv_18:6;
		unsigned RG_TOP_CRC_ALG_SEL:1;
		unsigned rsv_25:7;
	} LVDSTXT_LVDS_CRC0, *PLVDSTXT_LVDS_CRC0;

	typedef struct {	/* 14026938 */
		unsigned RG_TOP_CRC_H_START:16;
		unsigned RG_TOP_CRC_H_END:16;
	} LVDSTXT_LVDS_CRC1, *PLVDSTXT_LVDS_CRC1;

	typedef struct {	/* 1402693c */
		unsigned RG_TOP_CRC_START:1;
		unsigned RG_TOP_CRC_CLR:1;
		unsigned rsv_2:30;
	} LVDSTXT_LVDS_CRC2, *PLVDSTXT_LVDS_CRC2;

	typedef struct {	/* 14026940 */
		unsigned ST_TOP_CRC1_VALUE:30;
		unsigned ST_TOP_CRC1_FAIL:1;
		unsigned ST_TOP_CRC1_DONE:1;
	} LVDSTXT_LVDS_CRC3, *PLVDSTXT_LVDS_CRC3;

	typedef struct {	/* 14026944 */
		unsigned ST_TOP_CRC2_VALUE:30;
		unsigned ST_TOP_CRC2_FAIL:1;
		unsigned ST_TOP_CRC2_DONE:1;
	} LVDSTXT_LVDS_CRC4, *PLVDSTXT_LVDS_CRC4;

	typedef struct {	/* 14026948 */
		unsigned ST_TOP_CRC3_VALUE:30;
		unsigned ST_TOP_CRC3_FAIL:1;
		unsigned ST_TOP_CRC3_DONE:1;
	} LVDSTXT_LVDS_CRC5, *PLVDSTXT_LVDS_CRC5;

	typedef struct {	/* 1402694c */
		unsigned ST_TOP_CRC4_VALUE:30;
		unsigned ST_TOP_CRC4_FAIL:1;
		unsigned ST_TOP_CRC4_DONE:1;
	} LVDSTXT_LVDS_CRC6, *PLVDSTXT_LVDS_CRC6;


/* LVDSTXO_REG define */
	typedef struct {	/* 14026A00 */
		unsigned RG_SPECIAL_NS:1;
		unsigned RG_NS_VESA_EN:1;
		unsigned RG_10B_EN:1;
		unsigned rsv_3:1;
		unsigned RG_5381_10B_EN:1;
		unsigned rsv_5:6;
		unsigned RG_ODD_SW:1;
		unsigned RG_DUAL:1;
		unsigned rsv_13:7;
		unsigned RG_MERGE_OSD:1;
		unsigned RG_2CH_MERGE:1;
		unsigned RG_8BIT_DUAL:1;
		unsigned RG_RGB_444_MERGE:1;
		unsigned RG_RES_FLD:1;
		unsigned RG_HS_SEL:1;
		unsigned RG_VS_SEL:1;
		unsigned RG_DE_SEL:1;
		unsigned RG_RES:1;
		unsigned RG_CNTLE:1;
		unsigned RG_CNTLF:1;
		unsigned rsv_31:1;
	} LVDSTXO_LVDS_CTRL00, *PLVDSTXO_LVDS_CTRL00;

	typedef struct {	/* 14026A04 */
		unsigned RG_PD:24;
		unsigned RG_CLK_CTRL:7;
		unsigned RG_CLK_CTRL_EN:1;
	} LVDSTXO_LVDS_CTRL01, *PLVDSTXO_LVDS_CTRL01;

	typedef struct {	/* 14026A08 */
		unsigned RG_A_SW:2;
		unsigned RG_B_SW:2;
		unsigned RG_C_SW:2;
		unsigned RG_D_SW:2;
		unsigned RG_UVINV:1;
		unsigned RG_YUV2YC_EN:1;
		unsigned RG_LPF_EN:1;
		unsigned RG_C_LINE_EXT:1;
		unsigned RG_DPMODE:1;
		unsigned RG_LVDS_74FIFO_EN:1;
		unsigned rsv_14:18;
	} LVDSTXO_LVDS_CTRL02, *PLVDSTXO_LVDS_CTRL02;

	typedef struct {	/* 14026A10 */
		unsigned RG_CRC_START:1;
		unsigned RG_CRC_CLR:1;
		unsigned RG_CRC_SEL:2;
		unsigned rsv_4:4;
		unsigned RG_CRC_VCNT:16;
		unsigned rsv_24:8;
	} LVDSTXO_REG04, *PLVDSTXO_REG04;

	typedef struct {	/* 14026A14 */
		unsigned ST_VGA_CRC_RDY:1;
		unsigned ST_VGA_CRC_ERR:1;
		unsigned rsv_2:6;
		unsigned ST_VGA_CRC_OUT:24;
	} LVDSTXO_REG05, *PLVDSTXO_REG05;

	typedef struct {	/* 14026A18 */
		unsigned ST_LVDS_CRC_RDY:1;
		unsigned ST_LVDS_CRC_ERR:1;
		unsigned rsv_2:6;
		unsigned ST_LVDS_CRC_OUT_23_0:24;
	} LVDSTXO_REG06, *PLVDSTXO_REG06;

	typedef struct {	/* 14026A1c */
		unsigned ST_LVDS_CRC_OUT_41_24:18;
		unsigned rsv_18:14;
	} LVDSTXO_REG07, *PLVDSTXO_REG07;

	typedef struct {	/* 14026A30 */
		unsigned rsv_0:24;
		unsigned RG_RES_FLD_L:1;
		unsigned RG_HS_SEL_L:1;
		unsigned RG_VS_SEL_L:1;
		unsigned RG_DE_SEL_L:1;
		unsigned RG_RES_L:1;
		unsigned RG_CNTLE_L:1;
		unsigned RG_CNTLF_L:1;
		unsigned rsv_31:1;
	} LVDSTXO_LVDS_TEST01, *PLVDSTXO_LVDS_TEST01;

	typedef struct {	/* 14026A34 */
		unsigned rsv_0:15;
		unsigned RG_RES_EVEN_ODD_L_4CH:1;
		unsigned rsv_16:16;
	} LVDSTXO_LVDS_TEST02, *PLVDSTXO_LVDS_TEST02;

	typedef struct {	/* 14026A38 */
		unsigned RG_LVDSRX_CRC_V_CNT:16;
		unsigned RG_LVDSRX_CRC_ALG_SEL:1;
		unsigned RG_LVDSRX_CRC_MODE:2;
		unsigned RG_LVDSRX_CRC_CLR:1;
		unsigned RG_LVDSRX_CRC_START:1;
		unsigned RG_LVDSRX_CRC_SEL:2;
		unsigned RG_LVDSRX_FIFO_EN:1;
		unsigned rsv_31:8;
	} LVDSTXO_REG14, *PLVDSTXO_REG14;

	typedef struct {	/* 14026A3c */
		unsigned RG_LCDSRX_CRC_H_START:16;
		unsigned RG_LVDSRX_CRC_H_END:16;
	} LVDSTXO_REG15, *PLVDSTXO_REG15;

	typedef struct {	/* 14026A40 */
		unsigned LVDSRX_CRC_VALUE:30;
		unsigned LVDSRX_CRC_FAIL:1;
		unsigned LVDSRX_CRC_DONE:1;
	} LVDSTXO_LVDS_RX, *PLVDSTXO_LVDS_RX;

/* LVDSTXP_REG define */
	typedef struct {	/* 14026A80 */
		unsigned rsv_0:8;
		unsigned RG_LVDS_HSYNC_P:1;
		unsigned RG_LVDS_VSYNC_P:1;
		unsigned RG_LVDS_DE_P:1;
		unsigned RG_LVDS_DISPR_P:1;
		unsigned rsv_12:20;
	} LVDSTXP_LVDSTX_REG00, *PLVDSTXP_LVDSTX_REG00;

/* LVDS base address: 14026000 */
	typedef struct {
		/* LVDS TOPB_REG define */
		LVDS_TOP_REG00 TOP_REG00;	/* 0000 */
		LVDS_TOP_REG01 TOP_REG01;	/* 0004 */
		LVDS_TOP_REG02 TOP_REG02;	/* 0008 */
		LVDS_TOP_REG03 TOP_REG03;	/* 000C */
		LVDS_TOP_REG04 TOP_REG04;	/* 0010 */
		LVDS_TOP_REG05 TOP_REG05;	/* 0014 */
		UINT32 rsv_18[379];

		/* LVDS PANELB_REG define */
		PANELB_PATGEN0 PATGEN0;	/* 0604 */
		PANELB_PATGEN1 PATGEN1;	/* 0608 */
		PANELB_PATGEN2 PATGEN2;	/* 060c */
		PANELB_PATGEN3 PATGEN3;	/* 0610 */
		PANELB_PATGEN4 PATGEN4;	/* 0614 */
		PANELB_PATGEN5 PATGEN5;	/* 0618 */
		UINT32 rsv_61c;
		PANELB_PATGEN2 PATGEN6;	/* 0620 */
		UINT32 rsv_624[55];

		/* LVDS PANELP_REG define */
		PANELP_CFG0 CFG0;	/* 0700 */
		PANELP_DETECT0 DETECT0;	/* 0704 */
		PANELP_DETECT1 DETECT1;	/* 0708 */
		PANELP_DETECT2 DETECT2;	/* 070c */
		UINT32 rsv_710;
		PANELP_3D_CTRL_MON TD_CTRL_MON;	/* 0714 */
		UINT32 rsv_718[8];
		PANELP_CRC_CHECK_0 CRC_CHECK_0;	/* 0738 */
		PANELP_CRC_CHECK_1 CRC_CHECK_1;	/* 073C */
		PANELP_CRC_CHECK_2 CRC_CHECK_2;	/* 0740 */
		UINT32 rsv_744[3];
		PANELP_DETECT3 DETECT3;	/* 0750 */
		PANELP_DETECT4 DETECT4;	/* 0754 */
		UINT32 rsv_758[42];

		/* LVDSTXB_REG define */
		LVDSTXB_MODE0 MODE0;	/* 0800 */
		UINT32 rsv_804[4];
		LVDSTXB_LVDS_CTRL LVDS_CTRL;	/* 0814 */
		UINT32 rsv_818[3];
		LVDSTXB_ANA_TEST ANA_TEST;	/* 0824 */
		UINT32 rsv_828[55];

		/* LVDSTXT_REG define */
		LVDSTXT_LLV_DO_SEL LLV_DO_SEL;	/* 0904 */
		UINT32 rsv_908;
		LVDSTXT_CKO_SEL CKO_SEL;	/* 090c */
		UINT32 rsv_910[8];
		LVDSTXT_PN_SWAP PN_SWAP;	/* 0930 */
		LVDSTXT_LVDS_CRC0 LVDS_CRC0;	/* 0934 */
		LVDSTXT_LVDS_CRC1 LVDS_CRC1;	/* 0938 */
		LVDSTXT_LVDS_CRC2 LVDS_CRC2;	/* 093c */
		LVDSTXT_LVDS_CRC3 LVDS_CRC3;	/* 0940 */
		LVDSTXT_LVDS_CRC4 LVDS_CRC4;	/* 0944 */
		LVDSTXT_LVDS_CRC5 LVDS_CRC5;	/* 0948 */
		LVDSTXT_LVDS_CRC6 LVDS_CRC6;	/* 094c */
		UINT32 rsv_950[44];

		/* LVDSTXO_REG define */
		LVDSTXO_LVDS_CTRL00 LVDS_CTRL00;	/* 0A00 */
		LVDSTXO_LVDS_CTRL01 LVDS_CTRL01;	/* 0A04 */
		LVDSTXO_LVDS_CTRL02 LVDS_CTRL02;	/* 0A08 */
		UINT32 rsv_A0C;
		LVDSTXO_REG04 REG04;	/* 0A10 */
		LVDSTXO_REG05 REG05;	/* 0A14 */
		LVDSTXO_REG06 REG06;	/* 0A18 */
		LVDSTXO_REG07 REG07;	/* 0A1c */
		UINT32 rsv_A20[4];
		LVDSTXO_LVDS_TEST01 LVDS_TEST01;	/* 0A30 */
		LVDSTXO_LVDS_TEST02 LVDS_TEST02;	/* 0A34 */
		LVDSTXO_REG14 REG14;	/* 0A38 */
		LVDSTXO_REG15 REG15;	/* 0A3c */
		LVDSTXO_LVDS_RX LVDS_RX;	/* 0A40 */
		UINT32 rsv_A44[15];
		LVDSTXP_LVDSTX_REG00 LVDSTX_REG00;	/* 0A80 */
	} volatile LVDS_REGS, *PLVDS_REGS;


/* LVDS ANA_REG define */
/* LVDS TX1 */
	typedef struct {	/* 10215800 */
		unsigned RG_LVDSTX1_REV1_COM:32;
	} LVDSTX1_CTL1, *PLVDSTX1_CTL1;

	typedef struct {	/* 10215804 */
		unsigned RG_LVDSTX1_TVO:4;
		unsigned RG_LVDSTX1_TVCM:4;
		unsigned RG_LVDSTX1_TSTCLK_SEL:2;
		unsigned RG_LVDSTX1_TSTCLKDIV_EN:1;
		unsigned RG_LVDSTX1_TSTCLK_EN:1;
		unsigned RG_LVDSTX1_TSTCLKDIV_SEL:2;
		unsigned RG_LVDSTX1_MPX_SEL:2;
		unsigned RG_LVDSTX1_BIAS_SEL:2;
		unsigned RG_LVDSTX1_R_TERM:2;
		unsigned RG_LVDSTX1_SEL_CKTST:1;
		unsigned RG_LVDSTX1_SEL_MERGE:1;
		unsigned RG_LVDSTX1_LDO_EN:1;
		unsigned RG_LVDSTX1_BIAS_EN:1;
		unsigned RG_LVDSTX1_SER_ABIST_EN:1;
		unsigned RG_LVDSTX1_SER_ABEDG_EN:1;
		unsigned RG_LVDSTX1_SER_BIST_TOG:1;
		unsigned rsv_27:5;
	} LVDSTX1_CTL2, *PLVDSTX1_CTL2;

	typedef struct {	/* 10215808 */
		unsigned RG_LVDSTX1_VOUTABIST_EN:5;
		unsigned RG_LVDSTX1_EXT_EN:5;
		unsigned RG_LVDSTX1_DRV_EN:5;
		unsigned rsv_15:1;
		unsigned RG_LVDSTX1_SER_DIN_SEL:1;
		unsigned RG_LVDSTX1_SER_CLKDIG_INV:1;
		unsigned rsv_18:2;
		unsigned RG_LVDSTX1_SER_DIN:10;
		unsigned rsv_30:2;
	} LVDSTX1_CTL3, *PLVDSTX1_CTL3;

	typedef struct {	/* 1021580c */
		unsigned RG_LVDSTX1_REV:20;
		unsigned RG_LVDSTX1_TSTPAD_EN:1;
		unsigned RG_LVDSTX1_ABIST_EN:1;
		unsigned RG_LVDSTX1_MPX_EN:1;
		unsigned RG_LVDSTX1_LDOLPF_EN:1;
		unsigned RG_LVDSTX1_TEST_BYPASSBUF:1;
		unsigned RG_LVDSTX1_BIASLPF_EN:1;
		unsigned RG_LVDSTX1_SER_ABMUX_SEL:3;
		unsigned RG_LVDSTX1_SER_PEM_EN:1;
		unsigned RG_LVDSTX1_LVROD:2;
	} LVDSTX1_CTL4, *PLVDSTX1_CTL4;

	typedef struct {	/* 10215810 */
		unsigned RG_LVDSTX1_RESRERVE:4;
		unsigned G_LVDSTX1_MIPICK_SEL:1;
		unsigned RG_LVDSTX1_INCK_SEL:1;
		unsigned RG_LVDSTX1_SWITCH_EN:1;
		unsigned rsv_7:25;
	} LVDSTX1_CTL5, *PLVDSTX1_CTL5;

	typedef struct {	/* 1021581c */
		unsigned rsv_0:7;
		unsigned LVDS_ISO_EN:1;
		unsigned DA_LVDSTX_PWR_ON:1;
		unsigned rsv_10:23;
	} LVDS_TX1_VOPLL_CTL3, *PLVDS_TX1_VOPLL_CTL3;

/* LVDS analog base address: 10215800 */
	typedef struct {
		LVDSTX1_CTL1 CTL1;	/* 0000 */
		LVDSTX1_CTL2 CTL2;	/* 0004 */
		LVDSTX1_CTL3 CTL3;	/* 0008 */
		LVDSTX1_CTL4 CTL4;	/* 000C */
		LVDSTX1_CTL5 CTL5;	/* 0010 */
		UINT32 rsv_A10[2];
		LVDS_TX1_VOPLL_CTL3 VOPLL_CTL3;	/* 001c */
	} volatile LVDS_TX1_REGS, *PLVDS_TX1_REGS;

/* LVDS TX2 */
	typedef struct {	/* 10216800 */
		unsigned RG_LVDSTX2_REV1_COM:32;
	} LVDSTX2_CTL1, *PLVDSTX2_CTL1;

	typedef struct {	/* 10216804 */
		unsigned RG_LVDSTX2_TVO:4;
		unsigned RG_LVDSTX2_TVCM:4;
		unsigned RG_LVDSTX2_TSTCLK_SEL:2;
		unsigned RG_LVDSTX2_TSTCLKDIV_EN:1;
		unsigned RG_LVDSTX2_TSTCLK_EN:1;
		unsigned RG_LVDSTX2_TSTCLKDIV_SEL:2;
		unsigned RG_LVDSTX2_MPX_SEL:2;
		unsigned RG_LVDSTX2_BIAS_SEL:2;
		unsigned RG_LVDSTX2_R_TERM:2;
		unsigned RG_LVDSTX2_SEL_CKTST:1;
		unsigned RG_LVDSTX2_SEL_MERGE:1;
		unsigned RG_LVDSTX2_LDO_EN:1;
		unsigned RG_LVDSTX2_BIAS_EN:1;
		unsigned RG_LVDSTX2_SER_ABIST_EN:1;
		unsigned RG_LVDSTX2_SER_ABEDG_EN:1;
		unsigned RG_LVDSTX2_SER_BIST_TOG:1;
		unsigned rsv_27:5;
	} LVDSTX2_CTL2, *PLVDSTX2_CTL2;

	typedef struct {	/* 10216808 */
		unsigned RG_LVDSTX2_VOUTABIST_EN:5;
		unsigned RG_LVDSTX2_EXT_EN:5;
		unsigned RG_LVDSTX2_DRV_EN:5;
		unsigned rsv_15:1;
		unsigned RG_LVDSTX2_SER_DIN_SEL:1;
		unsigned RG_LVDSTX2_SER_CLKDIG_INV:1;
		unsigned rsv_18:2;
		unsigned RG_LVDSTX2_SER_DIN:10;
		unsigned rsv_30:2;
	} LVDSTX2_CTL3, *PLVDSTX2_CTL3;

	typedef struct {	/* 1021680c */
		unsigned RG_LVDSTX2_REV:20;
		unsigned RG_LVDSTX2_TSTPAD_EN:1;
		unsigned RG_LVDSTX2_ABIST_EN:1;
		unsigned RG_LVDSTX2_MPX_EN:1;
		unsigned RG_LVDSTX2_LDOLPF_EN:1;
		unsigned RG_LVDSTX2_TEST_BYPASSBUF:1;
		unsigned RG_LVDSTX2_BIASLPF_EN:1;
		unsigned RG_LVDSTX2_SER_ABMUX_SEL:3;
		unsigned RG_LVDSTX2_SER_PEM_EN:1;
		unsigned RG_LVDSTX2_LVROD:2;
	} LVDSTX2_CTL4, *PLVDSTX2_CTL4;

	typedef struct {	/* 10216810 */
		unsigned RG_LVDSTX2_RESRERVE:4;
		unsigned G_LVDSTX2_MIPICK_SEL:1;
		unsigned RG_LVDSTX2_INCK_SEL:1;
		unsigned RG_LVDSTX2_SWITCH_EN:1;
		unsigned rsv_7:25;
	} LVDSTX2_CTL5, *PLVDSTX2_CTL5;

	typedef struct {	/* 10216814 */
		unsigned RG_VPLL_TXMUXDIV2_EN:1;
		unsigned RG_VPLL_RESERVE:1;
		unsigned RG_VPLL_LVROD_EN:1;
		unsigned rsv_3:1;
		unsigned RG_VPLL_RST_DLY:2;
		unsigned RG_VPLL_FBKSEL:2;
		unsigned RG_VPLL_DDSFBK_EN:1;
		unsigned rsv_9:3;
		unsigned RG_VPLL_FBKDIV:7;
		unsigned rsv_19:1;
		unsigned RG_VPLL_PREDIV:2;
		unsigned RG_VPLL_POSDIV:3;
		unsigned RG_VPLL_VCO_DIV_SEL:1;
		unsigned RG_VPLL_BLP:1;
		unsigned RG_VPLL_BP:1;
		unsigned RG_VPLL_BR:1;
		unsigned rsv_29:3;
	} LVDS_VOPLL_CTL1, *PLVDS_VOPLL_CTL1;

	typedef struct {	/* 10216818 */
		unsigned RG_VPLL_DIVEN:3;
		unsigned rsv_3:1;
		unsigned RG_VPLL_MONCK_EN:1;
		unsigned RG_VPLL_MONVC_EN:1;
		unsigned RG_VPLL_MONREF_EN:1;
		unsigned RG_VPLL_EN:1;
		unsigned RG_VPLL_TXDIV1:2;
		unsigned RG_VPLL_TXDIV2:2;
		unsigned RG_VPLL_LVDS_EN:1;
		unsigned RG_VPLL_LVDS_DPIX_DIV2:1;
		unsigned RG_VPLL_TTL_EN:1;
		unsigned rsv_15:1;
		unsigned RG_VPLL_TTLDIV:2;
		unsigned RG_VPLL_TXSEL:1;
		unsigned rsv_19:1;
		unsigned RG_VPLL_RESERVE1:1;
		unsigned RG_VPLL_TXDIV5_EN:1;
		unsigned rsv_22:1;
		unsigned RG_CLOCK_SEL:1;
		unsigned RG_VPLL_BIAS_EN:1;
		unsigned RG_VPLL_BIASLPF_EN:1;
		unsigned rsv_26:6;
	} LVDS_VOPLL_CTL2, *PLVDS_VOPLL_CTL2;

	typedef struct {	/* 1021681c */
		unsigned rsv_0:7;
		unsigned LVDS_ISO_EN:1;
		unsigned DA_LVDSTX_PWR_ON:1;
		unsigned rsv_10:23;
	} LVDS_VOPLL_CTL3, *PLVDS_VOPLL_CTL3;

/* LVDS analog base address: 10216800 */
	typedef struct {
		LVDSTX2_CTL1 CTL1;	/* 0000 */
		LVDSTX2_CTL2 CTL2;	/* 0004 */
		LVDSTX2_CTL3 CTL3;	/* 0008 */
		LVDSTX2_CTL4 CTL4;	/* 000C */
		LVDSTX2_CTL5 CTL5;	/* 0010 */
		LVDS_VOPLL_CTL1 VOPLL_CTL1;	/* 0014 */
		LVDS_VOPLL_CTL2 VOPLL_CTL2;	/* 0018 */
		LVDS_VOPLL_CTL3 VOPLL_CTL3;	/* 001c */
	} volatile LVDS_TX2_REGS, *PLVDS_TX2_REGS;

/*
	 STATIC_ASSERT(0x0018 == offsetof(DPI_REGS, SIZE));
	 STATIC_ASSERT(0x0038 == offsetof(DPI_REGS, BG_COLOR));
	 STATIC_ASSERT(0x0070 == offsetof(DPI_REGS, TGEN_VWIDTH_RODD));
	 STATIC_ASSERT(0x00AC == offsetof(DPI_REGS, ESAV_CODE_SET1));
	 */
#ifdef __cplusplus
}
#endif
#endif				/* __DPI_REG_H__ */
