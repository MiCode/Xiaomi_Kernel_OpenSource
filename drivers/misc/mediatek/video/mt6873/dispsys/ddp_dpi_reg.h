// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __DDP_DPI_REG_H__
#define __DDP_DPI_REG_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DPI_REG_EN {
	unsigned EN:1;
	unsigned rsv_1:31;
};

struct DPI_REG_RST {
	unsigned RST:1;
	unsigned rsv_1:31;
};

struct DPI_REG_INTERRUPT {
	unsigned VSYNC:1;
	unsigned VDE:1;
	unsigned UNDERFLOW:1;
	unsigned rsv_3:29;
};

struct DPI_REG_CNTL {
	unsigned BG_EN:1;
	unsigned RGB_SWAP:1;
	unsigned INTL_EN:1;
	unsigned TDFP_EN:1;
	unsigned CLPF_EN:1;
	unsigned YUV422_EN:1;
	unsigned RGB2YUV_EN:1;
	unsigned rsv_7:1;
	unsigned EMBSYNC_EN:1;
	unsigned rsv_9:3;
	unsigned PIXREP:4;
	unsigned VS_LODD_EN:1;
	unsigned VS_LEVEN_EN:1;
	unsigned VS_RODD_EN:1;
	unsigned VS_REVEN_EN:1;
	unsigned FAKE_DE_LODD:1;
	unsigned FAKE_DE_LEVEN:1;
	unsigned FAKE_DE_RODD:1;
	unsigned FAKE_DE_REVEN:1;
	unsigned rsv_24:8;
};

struct DPI_REG_OUTPUT_SETTING {
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
};

struct DPI_REG_SIZE {
	unsigned WIDTH:13;
	unsigned rsv_13:3;
	unsigned HEIGHT:13;
	unsigned rsv_28:3;
};

struct DPI_REG_DDR_SETTING {
	unsigned DDR_EN:1;
	unsigned DDR_SEL:1;
	unsigned DDR_4PHASE:1;
	unsigned DATA_THROT:1;
	unsigned DDR_WIDTH:2;
	unsigned rsv_6:2;
	unsigned DDR_PAD_MODE:1;
	unsigned rsv_9:23;
};

struct DPI_REG_TGEN_HPORCH {
	unsigned HBP:12;
	unsigned rsv_12:4;
	unsigned HFP:12;
	unsigned rsv_28:4;
};

struct DPI_REG_TGEN_VWIDTH_LODD {
	unsigned VPW_LODD:12;
	unsigned rsv_12:4;
	unsigned VPW_HALF_LODD:1;
	unsigned rsv_17:15;
};

struct DPI_REG_TGEN_VPORCH_LODD {
	unsigned VBP_LODD:12;
	unsigned rsv_12:4;
	unsigned VFP_LODD:12;
	unsigned rsv_28:4;
};

struct DPI_REG_BG_HCNTL {
	unsigned BG_RIGHT:13;
	unsigned rsv_13:3;
	unsigned BG_LEFT:13;
	unsigned rsv_29:3;
};

struct DPI_REG_BG_VCNTL {
	unsigned BG_BOT:13;
	unsigned rsv_13:3;
	unsigned BG_TOP:13;
	unsigned rsv_29:3;
};


struct DPI_REG_BG_COLOR {
	unsigned BG_B:8;
	unsigned BG_G:8;
	unsigned BG_R:8;
	unsigned rsv_24:8;
};

struct DPI_REG_FIFO_CTL {
	unsigned FIFO_VALID_SET:5;
	unsigned rsv_5:3;
	unsigned FIFO_RST_SEL:1;
	unsigned rsv_9:23;
};

struct DPI_REG_STATUS {
	unsigned V_CNT:13;
	unsigned rsv_13:3;
	unsigned DPI_BUSY:1;
	unsigned OUT_EN:1;
	unsigned rsv_18:2;
	unsigned FIELD:1;
	unsigned TDLR:1;
	unsigned rsv_22:10;
};

struct DPI_REG_TMODE {
	unsigned OEN_EN:1;
	unsigned rsv_1:31;
};

struct DPI_REG_CHKSUM {
	unsigned CHKSUM:24;
	unsigned rsv_24:6;
	unsigned CHKSUM_RDY:1;
	unsigned CHKSUM_EN:1;
};

struct DPI_REG_TGEN_VWIDTH_LEVEN {
	unsigned VPW_LEVEN:12;
	unsigned rsv_12:4;
	unsigned VPW_HALF_LEVEN:1;
	unsigned rsv_17:15;
};

struct DPI_REG_TGEN_VPORCH_LEVEN {
	unsigned VBP_LEVEN:12;
	unsigned rsv_12:4;
	unsigned VFP_LEVEN:12;
	unsigned rsv_28:4;
};

struct DPI_REG_TGEN_VWIDTH_RODD {
	unsigned VPW_RODD:12;
	unsigned rsv_12:4;
	unsigned VPW_HALF_RODD:1;
	unsigned rsv_17:15;
};

struct DPI_REG_TGEN_VPORCH_RODD {
	unsigned VBP_RODD:12;
	unsigned rsv_12:4;
	unsigned VFP_RODD:12;
	unsigned rsv_28:4;
};

struct DPI_REG_TGEN_VWIDTH_REVEN {
	unsigned VPW_REVEN:12;
	unsigned rsv_12:4;
	unsigned VPW_HALF_REVEN:1;
	unsigned rsv_17:15;
};

struct DPI_REG_TGEN_VPORCH_REVEN {
	unsigned VBP_REVEN:12;
	unsigned rsv_12:4;
	unsigned VFP_REVEN:12;
	unsigned rsv_28:4;
};

struct DPI_REG_ESAV_VTIM_LOAD {
	unsigned ESAV_VOFST_LODD:12;
	unsigned rsv_12:4;
	unsigned ESAV_VWID_LODD:12;
	unsigned rsv_28:4;
};

struct DPI_REG_ESAV_VTIM_LEVEN {
	unsigned ESAV_VVOFST_LEVEN:12;
	unsigned rsv_12:4;
	unsigned ESAV_VWID_LEVEN:12;
	unsigned rsv_28:4;
};

struct DPI_REG_ESAV_VTIM_ROAD {
	unsigned ESAV_VOFST_RODD:12;
	unsigned rsv_12:4;
	unsigned ESAV_VWID_RODD:12;
	unsigned rsv_28:4;
};

struct DPI_REG_ESAV_VTIM_REVEN {
	unsigned ESAV_VOFST_REVEN:12;
	unsigned rsv_12:4;
	unsigned ESAV_VWID_REVEN:12;
	unsigned rsv_28:4;
};


struct DPI_REG_ESAV_FTIM {
	unsigned ESAV_FOFST_ODD:12;
	unsigned rsv_12:4;
	unsigned ESAV_FOFST_EVEN:12;
	unsigned rsv_28:4;
};

struct DPI_REG_CLPF_SETTING {
	unsigned CLPF_TYPE:2;
	unsigned rsv2:2;
	unsigned ROUND_EN:1;
	unsigned rsv5:27;
};

struct DPI_REG_Y_LIMIT {
	unsigned Y_LIMIT_BOT:12;
	unsigned rsv12:4;
	unsigned Y_LIMIT_TOP:12;
	unsigned rsv28:4;
};

struct DPI_REG_C_LIMIT {
	unsigned C_LIMIT_BOT:12;
	unsigned rsv12:4;
	unsigned C_LIMIT_TOP:12;
	unsigned rsv28:4;
};

struct DPI_REG_YUV422_SETTING {
	unsigned UV_SWAP:1;
	unsigned rsv1:3;
	unsigned CR_DELSEL:1;
	unsigned CB_DELSEL:1;
	unsigned Y_DELSEL:1;
	unsigned DE_DELSEL:1;
	unsigned rsv8:24;
};

struct DPI_REG_EMBSYNC_SETTING {
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
	unsigned EMBSYNC_OPT:1;
	unsigned rsv_17:15;
};

struct DPI_REG_ESAV_CODE_SET0 {
	unsigned ESAV_CODE0:12;
	unsigned rsv_12:4;
	unsigned ESAV_CODE1:12;
	unsigned rsv_28:4;
};

struct DPI_REG_ESAV_CODE_SET1 {
	unsigned ESAV_CODE2:12;
	unsigned rsv_12:4;
	unsigned ESAV_CODE3_MSB:1;
	unsigned rsv_17:15;
};

struct DPI_REG_PATTERN {
	unsigned PAT_EN:1;
	unsigned rsv_1:3;
	unsigned PAT_SEL:3;
	unsigned rsv_6:1;
	unsigned PAT_B_MAN:8;
	unsigned PAT_G_MAN:8;
	unsigned PAT_R_MAN:8;
};

/*not be used*/
struct DPI_REG_TGEN_HCNTL {
	unsigned HPW:8;
	unsigned HBP:8;
	unsigned HFP:8;
	unsigned HSYNC_POL:1;
	unsigned DE_POL:1;
	unsigned rsv_26:6;
};

struct DPI_REG_TGEN_VCNTL {
	unsigned VPW:8;
	unsigned VBP:8;
	unsigned VFP:8;
	unsigned VSYNC_POL:1;
	unsigned rsv_25:7;
};
/*not be used end*/

struct DPI_REG_MATRIX_COEFF_SET0 {
	unsigned MATRIX_C00:13;
	unsigned rsv_13:3;
	unsigned MATRIX_C01:13;
	unsigned rsv_29:3;
};

struct DPI_REG_MATRIX_COEFF_SET1 {
	unsigned MATRIX_C02:13;
	unsigned rsv_13:3;
	unsigned MATRIX_C10:13;
	unsigned rsv_29:3;
};

struct DPI_REG_MATRIX_COEFF_SET2 {
	unsigned MATRIX_C11:13;
	unsigned rsv_13:3;
	unsigned MATRIX_C12:13;
	unsigned rsv_29:3;
};

struct DPI_REG_MATRIX_COEFF_SET3 {
	unsigned MATRIX_C20:13;
	unsigned rsv_13:3;
	unsigned MATRIX_C21:13;
	unsigned rsv_29:3;
};

struct DPI_REG_MATRIX_COEFF_SET4 {
	unsigned MATRIX_C22:13;
	unsigned rsv_13:19;
};

struct DPI_REG_MATRIX_PREADD_SET0 {
	unsigned MATRIX_PRE_ADD_0:9;
	unsigned rsv_9:7;
	unsigned MATRIX_PRE_ADD_1:9;
	unsigned rsv_24:7;
};

struct DPI_REG_MATRIX_PREADD_SET1 {
	unsigned MATRIX_PRE_ADD_2:9;
	unsigned rsv_9:23;
};

struct DPI_REG_MATRIX_POSTADD_SET0 {
	unsigned MATRIX_POST_ADD_0:13;
	unsigned rsv_13:3;
	unsigned MATRIX_POST_ADD_1:13;
	unsigned rsv_24:3;
};

struct DPI_REG_MATRIX_POSTADD_SET1 {
	unsigned MATRIX_POST_ADD_2:13;
	unsigned rsv_13:19;
};


struct DPI_REGS {
	struct DPI_REG_EN DPI_EN;	/*0000*/
	struct DPI_REG_RST DPI_RST;	/*0004*/
	struct DPI_REG_INTERRUPT INT_ENABLE;	/* 0008*/
	struct DPI_REG_INTERRUPT INT_STATUS;	/*000C*/
	struct DPI_REG_CNTL CNTL;	/*0010*/
	struct DPI_REG_OUTPUT_SETTING OUTPUT_SETTING;	/*0014*/
	struct DPI_REG_SIZE SIZE;	/*0018*/
	struct DPI_REG_DDR_SETTING DDR_SETTING;	/*001c*/

	unsigned int TGEN_HWIDTH;	/*0020*/
	struct DPI_REG_TGEN_HPORCH TGEN_HPORCH;	/*0024*/
	struct DPI_REG_TGEN_VWIDTH_LODD TGEN_VWIDTH_LODD;	/* 0028*/
	struct DPI_REG_TGEN_VPORCH_LODD TGEN_VPORCH_LODD;	/*002C*/

	struct DPI_REG_BG_HCNTL BG_HCNTL;	/*0030*/
	struct DPI_REG_BG_VCNTL BG_VCNTL;	/* 0034*/
	struct DPI_REG_BG_COLOR BG_COLOR;	/*0038*/
	struct DPI_REG_FIFO_CTL FIFO_CTL;	/*003C*/

	struct DPI_REG_STATUS STATUS;	/*0040*/
	struct DPI_REG_TMODE TMODE;	/*0044*/
	struct DPI_REG_CHKSUM CHKSUM;	/*0048*/
	unsigned int rsv_4C;
	unsigned int DUMMY;	/*0050*/
	unsigned int rsv_54[5];

	struct DPI_REG_TGEN_VWIDTH_LEVEN TGEN_VWIDTH_LEVEN;	/*0068*/
	struct DPI_REG_TGEN_VPORCH_LEVEN TGEN_VPORCH_LEVEN;	/* 006C*/

	struct DPI_REG_TGEN_VWIDTH_RODD TGEN_VWIDTH_RODD;	/*0070*/
	struct DPI_REG_TGEN_VPORCH_RODD TGEN_VPORCH_RODD;	/*0074*/
	struct DPI_REG_TGEN_VWIDTH_REVEN TGEN_VWIDTH_REVEN;	/*0078*/
	struct DPI_REG_TGEN_VPORCH_REVEN TGEN_VPORCH_REVEN;	/*007C*/
	struct DPI_REG_ESAV_VTIM_LOAD ESAV_VTIM_LOAD;	/*0080*/
	struct DPI_REG_ESAV_VTIM_LEVEN ESAV_VTIM_LEVEN;	/*0084*/
	struct DPI_REG_ESAV_VTIM_ROAD ESAV_VTIM_ROAD;	/*0088*/
	struct DPI_REG_ESAV_VTIM_REVEN ESAV_VTIM_REVEN;	/*008C*/

	struct DPI_REG_ESAV_FTIM ESAV_FTIM;	/*0090*/
	struct DPI_REG_CLPF_SETTING CLPF_SETTING;	/*0094*/
	struct DPI_REG_Y_LIMIT Y_LIMIT;	/*0098*/
	struct DPI_REG_C_LIMIT C_LIMIT;	/*009C*/
	struct DPI_REG_YUV422_SETTING YUV422_SETTING;	/*00A0*/
	struct DPI_REG_EMBSYNC_SETTING EMBSYNC_SETTING;	/*00A4*/
	struct DPI_REG_ESAV_CODE_SET0 ESAV_CODE_SET0;	/*00A8*/
	struct DPI_REG_ESAV_CODE_SET1 ESAV_CODE_SET1;	/*00AC*/

};
extern struct DPI_REGS *DPI_REG;

/*STATIC_ASSERT((offsetof(struct DPI_REGS, SIZE) == 0x0018));*/
/*STATIC_ASSERT((offsetof(struct DPI_REGS, BG_COLOR) == 0x0038));*/
/*STATIC_ASSERT((offsetof(struct DPI_REGS, TGEN_VWIDTH_RODD) == 0x0070));*/
/*STATIC_ASSERT((offsetof(struct DPI_REGS, ESAV_CODE_SET1) == 0x00AC));*/

#ifdef __cplusplus
}
#endif
#endif				 /*__DPI_REG_H__*/
