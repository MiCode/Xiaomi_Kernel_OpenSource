// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _DDP_REG_DBI_H_
#define _DDP_REG_DBI_H_

/* field definition */
/* ------------------------------------------------------------- */

struct DBI_REG_STATUS {
	unsigned RUN:1;
	unsigned rsv_1:1;
	unsigned WAIT_HTT:1;
	unsigned WAIT_SYNC:1;
	unsigned BUSY:1;
	unsigned rsv_5:27;
};

struct DBI_REG_INTERRUPT {
	unsigned COMPLETED:1;
	unsigned rsv_1:1;
	unsigned HTT:1;
	unsigned SYNC:1;
	unsigned TE:1;
	unsigned TIMEOUT:1;
	unsigned rsv_6:26;
};

struct DBI_REG_START {
	unsigned RESET:1;
	unsigned rsv_1:14;
	unsigned START:1;
	unsigned rsv_16:16;
};

struct DBI_SIF_PIX_CON_REG {
	unsigned SIF0_2PIN_SIZE:3;
	unsigned rsv_3:1;
	unsigned SIF0_PIX_2PIN:1;
	unsigned SIF0_PARA_2PIN:1;
	unsigned SIF0_SINGLE_A0:1;
	unsigned SIF0_CS_STAY_LOW:1;
	unsigned SIF1_2PIN_SIZE:3;
	unsigned rsv_11:1;
	unsigned SIF1_PIX_2PIN:1;
	unsigned SIF1_PARA_2PIN:1;
	unsigned SIF1_SINGLE_A0:1;
	unsigned SIF1_CS_STAY_LOW:1;
	unsigned rsv_16:16;
};

struct DBI_SIF_TIMING_REG {
	unsigned WR_2ND:4;
	unsigned WR_1ST:4;
	unsigned RD_2ND:4;
	unsigned RD_1ST:4;
	unsigned CSH:4;
	unsigned CSS:4;
	unsigned rsv_24:8;
};

struct DBI_SCNF_REG {
	unsigned SIZE_0:3;
	unsigned THREE_WIRE_0:1;
	unsigned SDI_0:1;
	unsigned FIRST_POL_0:1;
	unsigned SCK_DEF_0:1;
	unsigned DIV2_0:1;
	unsigned SIZE_1:3;
	unsigned THREE_WIRE_1:1;
	unsigned SDI_1:1;
	unsigned FIRST_POL_1:1;
	unsigned SCK_DEF_1:1;
	unsigned DIV2_1:1;
	unsigned rsv_16:8;
	unsigned HW_CS:1;
	unsigned VDO_MODE:1;
	unsigned CMD_LOCK:1;
	unsigned VDO_AUTO:1;
	unsigned SYNC_ALIGN:1;
	unsigned DDR_EN_CONFIG:1;
	unsigned rsv_30:2;
};

struct DBI_REG_PCNF {
	unsigned WST:6;
	unsigned rsv_6:2;
	unsigned C2WS:4;
	unsigned C2WH:4;
	unsigned RLT:6;
	unsigned rsv_22:2;
	unsigned C2RS:4;
	unsigned C2RH:4;
};

struct DBI_REG_PCNFDW {
	unsigned PCNF0_DW:3;
	unsigned rsv_3:1;
	unsigned PCNF1_DW:3;
	unsigned rsv_7:1;
	unsigned PCNF2_DW:3;
	unsigned rsv_11:5;
	unsigned PCNF0_CHW:4;
	unsigned PCNF1_CHW:4;
	unsigned PCNF2_CHW:4;
	unsigned rsv_28:4;
};

struct DBI_TECON_REG {
	unsigned ENABLE:1;
	unsigned EDGE_SEL:1;
	unsigned MODE:1;
	unsigned TE_REPEAT:1;
	unsigned rsv_4:11;
	unsigned SW_TE:1;
	unsigned rsv_16:16;
};

struct DBI_ROI_SIZE_REG {
	unsigned WIDTH:11;
	unsigned rsv_11:5;
	unsigned HEIGHT:11;
	unsigned rsv_27:5;
};

struct DBI_ROI_CADD_REG {
	unsigned rsv_0:4;
	unsigned addr:4;
	unsigned rsv_8:24;
};

struct DBI_ROI_DADD_REG {
	unsigned rsv_0:4;
	unsigned addr:4;
	unsigned rsv_8:24;
};

struct DBI_ROICON_REG {
	unsigned RGB_ORDER:1;
	unsigned SIGNIFICANCE:1;
	unsigned PADDING:1;
	unsigned DATA_FMT:3;
	unsigned IF_SIZE:2;
	unsigned rsv_8:16;
	unsigned SEND_RES_MODE:1;
	unsigned IF_24:1;
	unsigned rsv_26:6;
};

struct DBI_REG_SMICON {
	unsigned MAX_BURST:3;
	unsigned rsv_3:1;
	unsigned THROTTLE_EN:1;
	unsigned rsv_5:11;
	unsigned THROTTLE_PERIOD:16;
};

struct DBI_REG_DITHER_CON {
	unsigned DB_B:2;
	unsigned rsv_2:2;
	unsigned DB_G:2;
	unsigned rsv_6:2;
	unsigned DB_R:2;
	unsigned rsv_10:2;
	unsigned LFSR_B_SEED:4;
	unsigned LFSR_G_SEED:4;
	unsigned LFSR_R_SEED:4;
	unsigned rsv_24:8;
};

struct DBI_REG_SCNF_CS {
	unsigned CS0:1;
	unsigned CS1:1;
	unsigned CS2:1;
	unsigned rsv_3:29;
};

struct DBI_REG_CALC_HTT {
	unsigned TIME_OUT:12;
	unsigned rsv_12:4;
	unsigned COUNT:12;
	unsigned rsv_28:4;
};

struct DBI_REG_SYNC_LCM_SIZE {
	unsigned HTT:10;
	unsigned rsv_10:6;
	unsigned VTT:12;
	unsigned rsv_28:4;
};

struct DBI_REG_SYNC_CNT {
	unsigned WAITLINE:12;
	unsigned rsv_12:4;
	unsigned SCANLINE:12;
	unsigned rsv_28:4;
};

struct DBI_STALL_CG_CON {
	unsigned GEN_CG:1;
	unsigned DBI_SCK_CG:1;
	unsigned DBIP_PCK_CG:1;
	unsigned CHKSUM_CG:1;
	unsigned MAINCON_CG:1;
	unsigned SYNC_CG:1;
	unsigned SYNC_TE_CG:1;
	unsigned SRCMUX_CG:1;
	unsigned DITHER_CG:1;
	unsigned SOF_MASK_CG:1;
	unsigned rsv_10:22;
};

struct DBI_REG_CONSUME_RATE {
	unsigned CONSUME_PXLSRW:10;
	unsigned rsv_10:22;
};

struct DBI_REG_DBI_ULTRA_TH {
	unsigned DBI_TH_LOW:16;
	unsigned DBI_TH_HIGH:16;
};

struct DBI_REG_DBI_DB {
	unsigned XOFF:11;
	unsigned rsv_11:5;
	unsigned YOFF:11;
	unsigned rsv_27:5;
};

struct DBI_REG_DBIS_CHKSUM {
	unsigned DBIS_CHEKSUM:24;
	unsigned rsv_24:7;
	unsigned CHK_ENABLE:1;
};

struct DBI_REG_DBIP_CHKSUM {
	unsigned DBIP_CHEKSUM:24;
	unsigned rsv_24:8;
};

struct DBI_REG_INT_PATTERN {
	unsigned PAT_EN:1;
	unsigned MASK_B:1;
	unsigned MASK_G:1;
	unsigned MASK_R:1;
	unsigned PAT_SEL:3;
	unsigned rsv_7:1;
	unsigned B_MAN:8;
	unsigned G_MAN:8;
	unsigned R_MAN:8;
};

struct DBI_REG_PRDY_PROT {
	unsigned TIMEOUT_VAL:16;
	unsigned TIMEOUT_EN:1;
	unsigned rsv_17:15;
};

struct DBI_REG_STR_BYTE_CON {
	unsigned SIF0_DATA_SIZE:3;
	unsigned rsv_3:3;
	unsigned SIF0_BYTE_SWITCH:1;
	unsigned SIF0_BYTE_MODE:1;

	unsigned SIF1_DATA_SIZE:3;
	unsigned rsv_11:3;
	unsigned SIF1_BYTE_SWITCH:1;
	unsigned SIF1_BYTE_MODE:1;
	unsigned rsv_16:15;
	unsigned SW_RST_EVEN:1;
};

struct DBI_REG_WR_STR_BYTE {
	unsigned SIF0_BYTE:8;
	unsigned SIF1_BYTE:8;
	unsigned SIF0_BYTE2:8;
	unsigned SIF1_BYT2:8;
};

struct DBI_REG_RD_STR_BYTE {
	unsigned SIF0_RD_BYTE:8;
	unsigned SIF1_RD_BYTE:8;
	unsigned rsv_16:16;
};

struct DBI_REG_VDO_SYNC_CON0 {
	unsigned CYCLE_PER_PIX:8;
	unsigned rsv_8:8;
	unsigned VBP_PIX_NUM:8;
	unsigned VBP_LN_NUM:8;
};

struct DBI_REG_VDO_SYNC_CON1 {
	unsigned HFP_PIX_NUM:8;
	unsigned HFP_LN_NUM:8;
	unsigned HBP_PIX_NUM:8;
	unsigned HBP_LN_NUM:8;
};

struct DBI_REG_VDO_HEADER {
	unsigned VDO_LN_STR_HDR:16;
	unsigned VDO_FR_STR_HDR:16;
};

struct DBI_REG_PAD_SEL {
	unsigned LSDI_SEL:3;
	unsigned rsv_3:13;
	unsigned LSDA_SEL:3;
	unsigned rsv_19:13;
};

struct DBI_REG_PAD_DELAY_SEL {
	unsigned GP0_DELAY:2;
	unsigned GP1_DELAY:2;
	unsigned GP2_DELAY:2;
	unsigned GP3_DELAY:2;
	unsigned rsv_8:24;
};

struct DBI_REG_PAD_CON {
	unsigned SW_CON:1;
	unsigned SW_SEL:1;
	unsigned rsv_2:30;
};

struct DBI_REG_DITHER0 {
	unsigned START:1;
	unsigned rsv_1:3;
	unsigned OUT_SEL:1;
	unsigned rsv_5:3;
	unsigned FRAME_DONE:8;
	unsigned CRC_CEN:1;
	unsigned rsv_17:3;
	unsigned CRC_START:1;
	unsigned rsv_21:3;
	unsigned CRC_CLR:1;
	unsigned rsv_25:7;
};

struct DBI_REG_DITHER5 {
	unsigned W_DEMO:16;
	unsigned rsv_16:16;
};

struct DBI_REG_GMC_ULTRA_TH {
	unsigned GMC_TH_LOW:16;
	unsigned GMC_TH_HIGH:16;
};



struct DBI_REGS {
	struct DBI_REG_STATUS DBI_STA;	/* 3000 */
	struct DBI_REG_INTERRUPT INT_ENABLE;	/* 3004 */
	struct DBI_REG_INTERRUPT INT_STATUS;	/* 3008 */
	struct DBI_REG_START DBI_START;	/* 300C */
	UINT32 RESET;		/* 3010 */
	UINT32 rsv_3014;	/* 3014 */
	struct DBI_SIF_PIX_CON_REG DBI_SIF_PIX_CON;	/* 3018 */
	struct DBI_SIF_TIMING_REG SIF_TIMING[2];	/* 301C..3020 */
	UINT32 rsv_3024;	/* 3024 */
	struct DBI_SCNF_REG DBI_SCNF;	/* 3028 */
	struct DBI_REG_SCNF_CS SCNF_CS;	/* 302C */
	struct DBI_REG_PCNF PARALLEL_CFG[3];	/* 3030..3038 */
	struct DBI_REG_PCNFDW PARALLEL_DW;	/* 303C */
	struct DBI_TECON_REG DBI_TECON;	/* 3040 */
	struct DBI_REG_CALC_HTT CALC_HTT;	/* 3044 */
	struct DBI_REG_SYNC_LCM_SIZE SYNC_LCM_SIZE;	/* 3048 */
	struct DBI_REG_SYNC_CNT SYNC_CNT;	/* 304C */
	UINT32 rsv_0054[2];	/* 3050 3054 */
	UINT32 DBI_PCFG;	/* 3058 */
	UINT32 rsv_005C;	/* 305C */
	struct DBI_ROICON_REG DBI_ROICON;	/* 3060 */
	struct DBI_ROI_CADD_REG DBI_ROI_CADD;	/* 3064 */
	struct DBI_ROI_DADD_REG DBI_ROI_DADD;	/* 3068 */
	struct DBI_ROI_SIZE_REG DBI_ROI_SIZE;	/* 306C */
	struct DBI_STALL_CG_CON STALL_CG_CON;	/* 3070 */
	UINT32 rsv_3074[3];	/* 3074..307C */
	struct DBI_REG_DITHER_CON DITHER_CON;	/* 3080 */
	UINT32 DITHER_CFG;	/* 3084 */
	UINT32 FRAME_DONE;	/* 3088 */
	UINT32 rsv_308C;	/* 308C */
	UINT32 ULTRA_CON;	/* 3090 */
	struct DBI_REG_CONSUME_RATE CONSUME_RATE;	/* 3094 */
	struct DBI_REG_DBI_ULTRA_TH DBI_ULTRA_TH;	/* 3098 */
	UINT32 rsv_309C[3];	/* 309C,A0,A4 */
	struct DBI_REG_DBI_DB DBI_DB;	/* 30A8 */
	UINT32 rsv_30AC[13];	/* 30AC..30DC */
	struct DBI_REG_DBIS_CHKSUM DBIS_CHKSUM;	/* 30E0 */
	struct DBI_REG_DBIP_CHKSUM DBIP_CHKSUM;	/* 30E4 */
	struct DBI_REG_INT_PATTERN INT_PATTERN;	/* 30E8 */
	UINT32 rsv_30EC[93];	/* 30EC~325C */
	struct DBI_REG_PRDY_PROT PRDY_PROT;	/* 3260 */
	UINT32 rsv_3264[3];	/* 3264~326C */
	struct DBI_REG_STR_BYTE_CON STR_BYTE_CON;	/* 3270 */
	UINT32 rsv_3274;	/* 3274 */
	struct DBI_REG_WR_STR_BYTE WR_STR_BYTE;	/* 3278 */
	struct DBI_REG_RD_STR_BYTE RD_STR_BYTE;	/* 327C */
	UINT32 rsv_3280[4];	/* 3280~328C */
	struct DBI_REG_VDO_SYNC_CON0 VDO_SYNC_CON0;	/* 3290 */
	struct DBI_REG_VDO_SYNC_CON1 VDO_SYNC_CON1;	/* 3294 */
	UINT32 FR_DURATION;	/* 3298 */
	struct DBI_REG_VDO_HEADER VDO_HEADER;	/* 329C */
	UINT32 rsv_32a0[24];	/* 32a0~32FC */
	struct DBI_REG_PAD_SEL PAD_SEL;	/* 3300 */
	struct DBI_REG_PAD_DELAY_SEL PAD_DELAY_SEL;	/* 3304 */
	UINT32 rsv_3308[2];	/* 3308,330C */
	struct DBI_REG_PAD_CON PAD_CON;	/* 3310 */
	UINT32 rsv_3314[699];	/* 3314~,3DFC */
	struct DBI_REG_DITHER0 DITHER0;	/* 3E00 */
	UINT32 rsv_3E04[4];	/* 3E04 ~3E10 */
	struct DBI_REG_DITHER5 DITHER5;	/* 3E14 */
	UINT32 DITHER6;		/* 3E18 */
	UINT32 DITHER7;		/* 3E1C */
	UINT32 DITHER8;		/* 3E20 */
	UINT32 DITHER9;		/* 3E24 */
	UINT32 DITHER10;	/* 3E28 */
	UINT32 DITHE11;		/* 3E2C */
	UINT32 DITHER12;	/* 3E30 */
	UINT32 DITHER13;	/* 3E34 */
	UINT32 DITHER14;	/* 3E38 */
	UINT32 DITHE15;		/* 3E3C */
	UINT32 DITHER16;	/* 3E40 */
	UINT32 DITHER17;	/* 3E44 */
	UINT32 rsv_3E48[46];	/* 3E48 ~3EFC */
	UINT32 DBI_PCMD0;	/* 3F00 */
	UINT32 rsv_3F04[3];	/* 3F04 3F0C */
	UINT32 DBI_PDAT0;	/* 3F10 */
	UINT32 rsv_3F14[3];	/* 3F14 3F1C */
	UINT32 DBI_PCMD1;	/* 3F20 */
	UINT32 rsv_3F24[3];	/* 3F24 3F2C */
	UINT32 DBI_PDAT1;	/* 3F30 */
	UINT32 rsv_3F34[3];	/* 3F34 3F3C */
	UINT32 DBI_PCMD2;	/* 3F40 */
	UINT32 rsv_3F44[3];	/* 3F44 3F4C */
	UINT32 DBI_PDAT2;	/* 3F50 */
	UINT32 rsv_3F54[11];	/* 3F54 3F7C */
	UINT32 DBI_SCMD0;	/* 3F80 */
	UINT32 rsv_3F84;	/* 3F84 */
	UINT32 DBI_SPE_SCMD0;	/* 3F88 */
	UINT32 rsv_3F8C;	/* 3F8C */
	UINT32 DBI_SDAT0;	/* 3F90 */
	UINT32 rsv_3F94;	/* 3F94 */
	UINT32 DBI_SPE_SDAT0;	/* 3F98 */
	UINT32 rsv_3F9C;	/* 3F9C */
	UINT32 DBI_SCMD1;	/* 3FA0 */
	UINT32 rsv_3FA4;	/* 3FA4 */
	UINT32 DBI_SPE_SCMD1;	/* 3FA8 */
	UINT32 rsv_3FAC;	/* 3FAC */
	UINT32 DBI_SDAT1;	/* 3FB0 */
	UINT32 rsv_3FB4;	/* 3FB4 */
	UINT32 DBI_SPE_SDAT1;	/* 3FB8 */
};


#endif				/* _DDP_REG_DBI_H_ */
