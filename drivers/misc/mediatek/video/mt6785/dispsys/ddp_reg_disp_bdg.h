/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _DDP_REG_DISP_BDG_H_
#define _DDP_REG_DISP_BDG_H_


/* field definition */
/* ------------------------------------------------------------- */
/* DSI */

struct DSI_TX_START_REG {
	unsigned DSI_TX_START : 1;
	unsigned RSV_01 : 1;
	unsigned SLEEPOUT_START : 1;
	unsigned RSV_03 : 1;
	unsigned SKEWCAL_START : 1;
	unsigned RSV_05 : 11;
	unsigned VM_CMD_START : 1;
	unsigned RSV_17 : 15;
};

struct DSI_TX_STATUS_REG {
	unsigned RSV_00 : 32;
};

struct DSI_TX_INTEN_REG {
	unsigned LPRX_RD_RDY_INT_EN : 1;
	unsigned CMD_DONE_INT_EN : 1;
	unsigned TE_RDY_INT_EN : 1;
	unsigned VM_DONE_INT_EN : 1;
	unsigned FRAME_DONE_INT_EN : 1;
	unsigned VM_CMD_DONE_INT_EN : 1;
	unsigned SLEEPOUT_DONE_INT_EN : 1;
	unsigned TE_TIMEOUT_INT_EN : 1;
	unsigned VM_VBP_STR_INT_EN : 1;
	unsigned VM_VACT_STR_INT_EN : 1;
	unsigned VM_VFP_STR_INT_EN : 1;
	unsigned SKEWCAL_DONE_INT_EN : 1;
	unsigned BUFFER_UNDERRUN_INT_EN : 1;
	unsigned BTA_TIMEOUT_INT_EN : 1;
	unsigned INP_UNFINISH_INT_EN : 1;
	unsigned SLEEPIN_ULPS_INT_EN : 1;
	unsigned LPRX_RD_RDY_EVENT_EN : 1;
	unsigned CMD_DONE_EVENT_EN : 1;
	unsigned TE_RDY_EVENT_EN : 1;
	unsigned VM_DONE_EVENT_EN : 1;
	unsigned FRAME_DONE_EVENT_EN : 1;
	unsigned VM_CMD_DONE_EVENT_EN : 1;
	unsigned SLEEPOUT_DONE_EVENT_EN : 1;
	unsigned TE_TIMEOUT_EVENT_EN : 1;
	unsigned VM_VBP_STR_EVENT_EN : 1;
	unsigned VM_VACT_STR_EVENT_EN : 1;
	unsigned VM_VFP_STR_EVENT_EN : 1;
	unsigned SKEWCAL_DONE_EVENT_EN : 1;
	unsigned BUFFER_UNDERRUN_EVENT_EN : 1;
	unsigned BTA_TIMEOUT_EVENT_EN : 1;
	unsigned INP_UNFINISH_EVENT_EN : 1;
	unsigned SLEEPIN_ULPS_EVENT_EN : 1;
};

struct DSI_TX_INTSTA_REG {
	unsigned LPRX_RD_RDY_INT_FLAG : 1;
	unsigned CMD_DONE_INT_FLAG : 1;
	unsigned TE_RDY_INT_FLAG : 1;
	unsigned VM_DONE_INT_FLAG : 1;
	unsigned FRAME_DONE_INT_FLAG : 1;
	unsigned VM_CMD_DONE_INT_FLAG : 1;
	unsigned SLEEPOUT_DONE_INT_FLAG : 1;
	unsigned TE_TIMEOUT_INT_FLAG : 1;
	unsigned VM_VBP_STR_INT_FLAG : 1;
	unsigned VM_VACT_STR_INT_FLAG : 1;
	unsigned VM_VFP_STR_INT_FLAG : 1;
	unsigned SKEWCAL_DONE_INT_FLAG : 1;
	unsigned BUFFER_UNDERRUN_INT_FLAG : 1;
	unsigned BTA_TIMEOUT_INT_FLAG : 1;
	unsigned INP_UNFINISH_INT_FLAG : 1;
	unsigned SLEEPIN_ULPS_INT_FLAG : 1;
	unsigned HWMUTE_ON_INT_FLAG : 1;
	unsigned HWMUTE_OFF_INT_FLAG : 1;
	unsigned RSV_18 : 13;
	unsigned BUSY : 1;
};

struct DSI_TX_COM_CON_REG {
	unsigned DSI_RESET : 1;
	unsigned RSV_01 : 1;
	unsigned DPHY_RESET : 1;
	unsigned RSV_03 : 1;
	unsigned DSI_DUAL_EN : 1;
	unsigned RSV_05 : 19;
	unsigned RG_DSI_CM_MODE_WAIT_DATA_EVERY_LINE_EN : 1;
	unsigned RSV_25 : 2;
	unsigned RG_DSI_CM_WAIT_FIFO_FULL_EN : 1;
	unsigned RSV_28 : 4;
};

enum DSI_TX_MODE_CON {
	DSI_TX_CMD_MODE = 0,
	DSI_TX_SYNC_PULSE_VDO_MODE = 1,
	DSI_TX_SYNC_EVENT_VDO_MODE = 2,
	DSI_TX_BURST_VDO_MODE = 3
};

struct DSI_TX_MODE_CON_REG {
	unsigned MODE : 2;
	unsigned RSV_02 : 14;
	unsigned FRM_MODE : 1;
	unsigned RSV_17 : 3;
	unsigned SLEEP_MODE : 1;
	unsigned RSV_21 : 11;
};

struct DSI_TX_TXRX_CON_REG {
	unsigned VC_NUM : 2;
	unsigned LANE_NUM : 4;
	unsigned DIS_EOT : 1;
	unsigned BLLP_EN : 1;
	unsigned TE_FREERUN : 1;
	unsigned EXT_TE_EN : 1;
	unsigned EXT_TE_EDGE_SEL : 1;
	unsigned TE_AUTO_SYNC : 1;
	unsigned MAX_RTN_SIZE : 4;
	unsigned HSTX_CKLP_EN : 1;
	unsigned RSV_17 : 12;
	unsigned BTA_TIMEOUT_CHK_EN : 1;
	unsigned RSV_30 : 2;
};

enum DSI_TX_PS_TYPE {
	PACKED_PS_16_BIT_RGB565 = 0,
	PACKED_PS_18_BIT_RGB666 = 1,
	LOOSELY_PS_24_BIT_RGB666 = 2,
	PACKED_PS_24_BIT_RGB888 = 3
};

struct DSI_TX_PSCON_REG {
	unsigned DSI_PS_WC : 15;
	unsigned RSV_15 : 1;
	unsigned DSI_PS_SEL : 4;
	unsigned RSV_20 : 3;
	unsigned CUSTOM_HEADER_EN : 1;
	unsigned RGB_SWAP : 1;
	unsigned RSV_25 : 1;
	unsigned CUSTOM_HEADER : 6;
};

struct DSI_TX_VSA_NL_REG {
	unsigned VSA_NL : 12;
	unsigned RSV_12 : 20;
};

struct DSI_TX_VBP_NL_REG {
	unsigned VBP_NL : 12;
	unsigned RSV_12 : 20;
};

struct DSI_TX_VFP_NL_REG {
	unsigned VFP_NL : 15;
	unsigned RSV_15 : 17;
};

struct DSI_TX_VACT_NL_REG {
	unsigned VACT_NL : 15;
	unsigned RSV_15 : 17;
};

struct DSI_TX_LFR_CON_REG {
	unsigned RSV_00 : 32;
};

struct DSI_TX_LFR_STA_REG {
	unsigned RSV_00 : 32;
};

struct DSI_TX_SIZE_CON_REG {
	unsigned DSI_WIDTH : 15;
	unsigned RSV_15 : 1;
	unsigned DSI_HEIGHT : 15;
	unsigned RSV_31 : 1;
};

struct DSI_TX_HSA_WC_REG {
	unsigned HSA_WC : 12;
	unsigned RSV_12 : 20;
};

struct DSI_TX_HBP_WC_REG {
	unsigned HBP_WC : 12;
	unsigned RSV_12 : 20;
};

struct DSI_TX_HFP_WC_REG {
	unsigned HFP_WC : 15;
	unsigned RSV_15 : 17;
};

struct DSI_TX_BLLP_WC_REG {
	unsigned BLLP_WC : 12;
	unsigned RSV_12 : 20;
};

struct DSI_TX_CMDQ_CON_REG {
	unsigned CMDQ_SIZE : 8;
	unsigned RSV_08 : 7;
	unsigned BIT_15 : 1;
	unsigned RSV_16 : 16;
};

struct DSI_TX_HSTX_CKLP_WC_REG {
	unsigned RSV_00 : 32;
};

struct DSI_TX_RX_DATA_REG {
	unsigned char byte0;
	unsigned char byte1;
	unsigned char byte2;
	unsigned char byte3;
};

struct DSI_TX_RACK_REG {
	unsigned DSI_TX_RACK : 1;
	unsigned DSI_TX_RACK_BYPASS : 1;
	unsigned RSV_02 : 30;
};

struct DSI_TX_TRIG_STA_REG {
	unsigned TRIG0 : 1;	/* remote rst */
	unsigned TRIG1 : 1;	/* TE */
	unsigned TRIG2 : 1;	/* ack */
	unsigned TRIG3 : 1;	/* rsv */
	unsigned RX_ULPS : 1;
	unsigned DIRECTION : 1;
	unsigned RX_LPDT : 1;
	unsigned RSV_07 : 1;
	unsigned RX_POINTER : 4;
	unsigned RSV_12 : 20;
};

struct DSI_TX_MEM_CONTI_REG {
	unsigned RWMEM_CONTI : 16;
	unsigned RSV_16 : 16;
};

struct DSI_TX_FRM_BC_REG {
	unsigned FRM_BC : 21;
	unsigned RSV_21 : 11;
};

struct DSI_TX_V3D_CON_REG {
	unsigned RSV_00 : 32;
};

struct DSI_TX_TIME_CON0_REG {
	unsigned UPLS_WAKEUP_PRD : 16;
	unsigned SKEWCALL_PRD : 16;
};

struct DSI_TX_TIME_CON1_REG {
	unsigned RSV_00 : 32;
};

struct DSI_TX_TIME_CON2_REG {
	unsigned BTA_TIMEOUT_PRD : 16;
	unsigned RSV_16 : 16;
};

struct DSI_TX_PHY_LCPAT_REG {
	unsigned LC_HSTX_CK_PAT : 8;
	unsigned RSV_08 : 24;
};

struct DSI_TX_PHY_LCCON_REG {
	unsigned LC_HS_TX_EN : 1;
	unsigned LC_ULPM_EN : 1;
	unsigned RSV_02 : 30;
};

struct DSI_TX_PHY_LD0CON_REG {
	unsigned L0_RM_TRIG_EN : 1;
	unsigned L0_ULPM_EN : 1;
	unsigned RSV_02 : 1;
	unsigned LX_ULPM_AS_L0 : 1;
	unsigned L0_RX_FILTER_EN : 1;
	unsigned RSV_05 : 27;
};

struct DSI_TX_PHY_SYNCON_REG {
	unsigned HS_SYNC_CODE : 8;
	unsigned HS_SYNC_CODE2 : 8;
	unsigned HS_SKEWCAL_PAT : 8;
	unsigned HS_DB_SYNC_EN : 1;
	unsigned RSV_25 : 7;
};

struct DSI_TX_PHY_TIMCON0_REG {
	unsigned char LPX;
	unsigned char HS_PRPR;
	unsigned char HS_ZERO;
	unsigned char HS_TRAIL;
};

struct DSI_TX_PHY_TIMCON1_REG {
	unsigned char TA_GO;
	unsigned char TA_SURE;
	unsigned char TA_GET;
	unsigned char DA_HS_EXIT;
};

struct DSI_TX_PHY_TIMCON2_REG {
	unsigned RSV_00 : 8;
	unsigned char DA_HS_SYNC;
	unsigned char CLK_ZERO;
	unsigned char CLK_TRAIL;
};

struct DSI_TX_PHY_TIMCON3_REG {
	unsigned char CLK_HS_PRPR;
	unsigned char CLK_HS_POST;
	unsigned char CLK_HS_EXIT;
	unsigned RSV_24 : 8;
};

struct DSI_TX_VM_CMD_CON_REG {
	unsigned VM_CMD_EN : 1;
	unsigned LONG_PKT : 1;
	unsigned TIME_SEL : 1;
	unsigned TS_VSA_EN : 1;
	unsigned TS_VBP_EN : 1;
	unsigned TS_VFP_EN : 1;
	unsigned RSV_06 : 2;
	unsigned CM_DATA_ID : 8;
	unsigned CM_DATA_0 : 8;
	unsigned CM_DATA_1 : 8;
};

struct DSI_TX_PHY_TIMCON_REG {
	struct DSI_TX_PHY_TIMCON0_REG CTRL0;
	struct DSI_TX_PHY_TIMCON1_REG CTRL1;
	struct DSI_TX_PHY_TIMCON2_REG CTRL2;
	struct DSI_TX_PHY_TIMCON3_REG CTRL3;
};

struct DSI_TX_CKSM_OUT_REG {
	unsigned PKT_CHECK_SUM : 16;
	unsigned ACC_CHECK_SUM : 16;
};

struct DSI_TX_DEBUG_SEL_REG {
	unsigned DEBUG_OUT_SEL : 5;
	unsigned RSV_05 : 3;
	unsigned CHKSUM_REC_EN : 1;
	unsigned C2V_START_CON : 1;
	unsigned RSV_10 : 2;
	unsigned HSTXOE_OUT_VALUE : 1;
	unsigned RSV_13 : 19;
};

struct DSI_TX_STATE_DBG10_REG {
	unsigned LIMIT_W : 15;
	unsigned RSV_15 : 1;
	unsigned LIMIT_H : 15;
	unsigned RSV_31 : 1;
};

struct DSI_TX_SELF_PAT_CON0_REG {
	unsigned SELF_PAT_PRE_MODE : 1;
	unsigned SELF_PAT_POST_MODE : 1;
	unsigned SW_MUTE_EN : 1;
	unsigned HW_MUTE_EN : 1;
	unsigned SELF_PAT_SEL : 4;
	unsigned SELF_PAT_LEVEL : 2;
	unsigned SLEF_PAT_VSYNC_ON : 1;
	unsigned SELF_PAT_READY_ON : 1;
	unsigned RSV_12 : 4;
	unsigned SELF_PAT_R : 12;
	unsigned RSV_28 : 4;
};

struct DSI_TX_SELF_PAT_CON1_REG {
	unsigned SELF_PAT_G : 12;
	unsigned RSV_12 : 4;
	unsigned SELF_PAT_B : 12;
	unsigned RSV_28 : 4;
};

struct DSI_TX_SHADOW_DEBUG_REG {
	unsigned FORCE_COMMIT : 1;
	unsigned BYPASS_SHADOW : 1;
	unsigned READ_WORKING : 1;
	unsigned RSV_03 : 29;
};

struct DSI_TX_SHADOW_STA_REG {
	unsigned VACT_UPDATE_ERR : 1;
	unsigned VFP_UPDATE_ERR : 1;
	unsigned RSV_02 : 30;
};
struct DSI_VM_CMD_CON0_REG {
	unsigned VM_CMD_EN : 1;
	unsigned LONG_PKT : 1;
	unsigned TIME_SEL : 1;
	unsigned TS_VSA_EN : 1;
	unsigned TS_VBP_EN : 1;
	unsigned TS_VFP_EN : 1;
	unsigned RSV_07 : 2;
	unsigned CM_DATA_ID : 8;
	unsigned CM_DATA_0 : 8;
	unsigned CM_DATA_1 : 8;
};

struct DSI_VM_CMD_CON1_REG {
	unsigned VM_CMD_NEW_WC_SEL : 1;
	unsigned RSV_01 : 15;
	unsigned VM_CMD_NEW_WC : 16;
};

struct DSI_TARGET_NL_REG {
	unsigned TARGET_NL : 15;
	unsigned RSV_15 : 1;
	unsigned TARGET_NL_EN : 1;
};

struct DSI_TX_BUF_CON0_REG {
	unsigned ANTI_LATENCY_BUF_EN : 1;
	unsigned RSV_01 : 3;
	unsigned ANTI_LATENCY_BUF_FIFO_UNDERFLOW_DONT_BLOCK : 1;
	unsigned RSV_05 : 27;
};

struct DSI_TX_BUF_CON1_REG {
	unsigned ANTI_LATENCY_BUF_OUTPUT_VALID_THESHOLD : 15;
	unsigned RSV_15 : 1;
	unsigned ANTI_LATENCY_BUF_PSEUDO_SIZE : 15;
	unsigned RSV_31 : 1;
};

struct DSI_TX_BUF_RW_TIMES_REG {
	unsigned ANTI_LATENCY_RW_TIMES : 32;
};

struct DSI_TX_DBG_CON_REG {
	unsigned OUTPUT_CHKSUM_EN : 1;
	unsigned RSV_01 : 1;
	unsigned OUTPUT_CHKSUM_RST : 1;
	unsigned RSV_03 : 1;
	unsigned INPUT_CHKSUM_EN : 1;
	unsigned RSV_05 : 1;
	unsigned INPUT_CHKSUM_RST : 1;
	unsigned RSV_07 : 1;
	unsigned VM_FRAME_CKSM_RESET : 1;
	unsigned RSV_09 : 3;
	unsigned FRAME_SOFEOF_EN : 1;
	unsigned TX_VM_FRAME_CKSM : 2;
	unsigned RSV_15 : 1;
	unsigned SOFEOF_NUM_RESET : 1;
	unsigned RSV_17 : 1;
	unsigned FRAME_RATE_RESET : 1;
	unsigned RSV_19 : 1;
	unsigned MUTE_COLOR_DBG : 1;
	unsigned RSV_21 : 11;
};

struct DSI_TX_CRC_CKSM_REG {
	unsigned TX_VM_FRAME_CKSM : 16;
	unsigned TX_ACC_CKSM_FRAME_UPDATE : 16;
};

struct DSI_TX_IN_CKSM_REG {
	unsigned INPUT_CHKSUM : 30;
	unsigned RSV_30 : 2;
};

struct DSI_TX_O_CKSM_REG {
	unsigned LANE_CHKSUM : 32;
};

struct DSI_RESYNC_CON_REG {
	unsigned RSV_32 : 32;
};

struct DSI_TX_CMDQ_REG {
	unsigned TYPE : 2;
	unsigned BTA : 1;
	unsigned HS : 1;
	unsigned CL : 1;
	unsigned TE : 1;
	unsigned RESV : 2;
	unsigned DATA_ID : 8;
	unsigned DATA_0 : 8;
	unsigned DATA_1 : 8;
};

struct BDG_TX_REGS {
	struct DSI_TX_START_REG DSI_TX_START;			/* 0000 */
	struct DSI_TX_STATUS_REG DSI_TX_STA;			/* 0004 */
	struct DSI_TX_INTEN_REG DSI_TX_INTEN;			/* 0008 */
	struct DSI_TX_INTSTA_REG DSI_TX_INTSTA;			/* 000C */
	struct DSI_TX_COM_CON_REG DSI_TX_COM_CON;		/* 0010 */
	struct DSI_TX_MODE_CON_REG DSI_TX_MODE_CON;		/* 0014 */
	struct DSI_TX_TXRX_CON_REG DSI_TX_TXRX_CON;		/* 0018 */
	struct DSI_TX_PSCON_REG DSI_TX_PSCON;			/* 001C */
	struct DSI_TX_VSA_NL_REG DSI_TX_VSA_NL;			/* 0020 */
	struct DSI_TX_VBP_NL_REG DSI_TX_VBP_NL;			/* 0024 */
	struct DSI_TX_VFP_NL_REG DSI_TX_VFP_NL;			/* 0028 */
	struct DSI_TX_VACT_NL_REG DSI_TX_VACT_NL;		/* 002C */
	struct DSI_TX_LFR_CON_REG DSI_TX_LFR_CON;		/* 0030 */
	struct DSI_TX_LFR_STA_REG DSI_TX_LFR_STA;		/* 0034 */
	struct DSI_TX_SIZE_CON_REG DSI_TX_SIZE_CON;		/* 0038 */
	unsigned int RSV_003C[5];				/* 003C..004C */
	struct DSI_TX_HSA_WC_REG DSI_TX_HSA_WC;			/* 0050 */
	struct DSI_TX_HBP_WC_REG DSI_TX_HBP_WC;			/* 0054 */
	struct DSI_TX_HFP_WC_REG DSI_TX_HFP_WC;			/* 0058 */
	struct DSI_TX_BLLP_WC_REG DSI_TX_BLLP_WC;		/* 005C */
	struct DSI_TX_CMDQ_CON_REG DSI_TX_CMDQ_CON;		/* 0060 */
	struct DSI_TX_HSTX_CKLP_WC_REG DSI_TX_HSTX_CKL_WC;	/* 0064 */
	unsigned int RSV_0068[3];				/* 0068..0070 */
	struct DSI_TX_RX_DATA_REG DSI_TX_RX_DATA03;		/* 0074 */
	struct DSI_TX_RX_DATA_REG DSI_TX_RX_DATA47;		/* 0078 */
	struct DSI_TX_RX_DATA_REG DSI_TX_RX_DATA8B;		/* 007C */
	struct DSI_TX_RX_DATA_REG DSI_TX_RX_DATAC;		/* 0080 */
	struct DSI_TX_RACK_REG DSI_TX_RACK;			/* 0084 */
	struct DSI_TX_TRIG_STA_REG DSI_TX_TRIG_STA;		/* 0088 */
	unsigned int RSV_008C;					/* 008C */
	struct DSI_TX_MEM_CONTI_REG DSI_TX_MEM_CONTI;		/* 0090 */
	struct DSI_TX_FRM_BC_REG DSI_TX_FRM_BC;			/* 0094 */
	struct DSI_TX_V3D_CON_REG DSI_TX_V3D_CON;		/* 0098 */
	unsigned int RSV_009C;					/* 009C */
	struct DSI_TX_TIME_CON0_REG DSI_TX_TIME_CON0;		/* 00A0 */
	struct DSI_TX_TIME_CON1_REG DSI_TX_TIME_CON1;		/* 00A4 */
	struct DSI_TX_TIME_CON2_REG DSI_TX_TIME_CON2;		/* 00A8 */
	unsigned int RSV_00AC[21];				/* 00AC..00FC */
	struct DSI_TX_PHY_LCPAT_REG DSI_TX_PHY_LCPAT;		/* 0100 */
	struct DSI_TX_PHY_LCCON_REG DSI_TX_PHY_LCCON;		/* 0104 */
	struct DSI_TX_PHY_LD0CON_REG DSI_TX_PHY_LD0CON;		/* 0108 */
	struct DSI_TX_PHY_SYNCON_REG DSI_TX_PHY_SYNCON;		/* 010C */
	struct DSI_TX_PHY_TIMCON0_REG DSI_TX_PHY_TIMECON0;	/* 0110 */
	struct DSI_TX_PHY_TIMCON1_REG DSI_TX_PHY_TIMECON1;	/* 0114 */
	struct DSI_TX_PHY_TIMCON2_REG DSI_TX_PHY_TIMECON2;	/* 0118 */
	struct DSI_TX_PHY_TIMCON3_REG DSI_TX_PHY_TIMECON3;	/* 011C */
	unsigned int RSV_0120[4];				/* 0120..012C */
	struct DSI_TX_VM_CMD_CON_REG DSI_TX_VM_CMD_CON;		/* 0130 */
	unsigned int DSI_TX_VM_CMD_DATA0;			/* 0134 */
	unsigned int DSI_TX_VM_CMD_DATA4;			/* 0138 */
	unsigned int DSI_TX_VM_CMD_DATA8;			/* 013C */
	unsigned int DSI_TX_VM_CMD_DATAC;			/* 0140 */
	struct DSI_TX_CKSM_OUT_REG DSI_TX_CKSM_OUT;		/* 0144 */
	unsigned int RSV_0148[7];				/* 0148..0160 */
	struct DSI_TX_TIME_CON1_REG DSI_TX_STATE_DBG7;		/* 0164 */
	unsigned int RSV_0168[2];				/* 0168..016C */
	struct DSI_TX_DEBUG_SEL_REG DSI_TX_DEBUG_SEL;		/* 0170 */
	unsigned int RSV_0174;					/* 0174 */
	struct DSI_TX_SELF_PAT_CON0_REG DSI_TX_SELF_PAT_CON0;	/* 0178 */
	struct DSI_TX_SELF_PAT_CON1_REG DSI_TX_SELF_PAT_CON1;	/* 017C */
	unsigned int DSI_TX_VM_CMD_DATA10;			/* 0180 */
	unsigned int DSI_TX_VM_CMD_DATA14;			/* 0184 */
	unsigned int DSI_TX_VM_CMD_DATA18;			/* 0188 */
	unsigned int DSI_TX_VM_CMD_DATA1C;			/* 018C */
	struct DSI_TX_SHADOW_DEBUG_REG DSI_TX_SHADOW_DEBUG;	/* 0190 */
	struct DSI_TX_SHADOW_STA_REG DSI_TX_SHADOW_STA;		/* 0194 */
	unsigned int RSV_0198[2];				/* 0198..019C */
	unsigned int DSI_TX_VM_CMD_DATA20;			/* 01A0 */
	unsigned int DSI_TX_VM_CMD_DATA24;			/* 01A4 */
	unsigned int DSI_TX_VM_CMD_DATA28;			/* 01A8 */
	unsigned int DSI_TX_VM_CMD_DATA2C;			/* 01AC */
	unsigned int DSI_TX_VM_CMD_DATA30;			/* 01B0 */
	unsigned int DSI_TX_VM_CMD_DATA34;			/* 01B4 */
	unsigned int DSI_TX_VM_CMD_DATA38;			/* 01B8 */
	unsigned int DSI_TX_VM_CMD_DATA3C;			/* 01BC */
	unsigned int RSV_01C0[12];				/* 01C0..01EC */
	struct DSI_RESYNC_CON_REG DSI_RESYNC_CON;		/* 01F0 */
	unsigned int RSV_01F4[3];				/* 01F4..01FC */
	struct DSI_VM_CMD_CON0_REG DSI_VM_CMD_CON0;		/* 0200 */
	struct DSI_VM_CMD_CON1_REG DSI_VM_CMD_CON1;		/* 0204 */
	unsigned int RSV_208[62];				/* 0208..02FC */
	struct DSI_TARGET_NL_REG DSI_TARGET_NL;			/* 0300 */
	unsigned int RSV_304[63];				/* 0304..03FC */
	struct DSI_TX_BUF_CON0_REG DSI_TX_BUF_CON0;		/* 0400 */
	struct DSI_TX_BUF_CON1_REG DSI_TX_BUF_CON1;		/* 0404 */
	unsigned int RSV_0408[2];				/* 0408..040C */
	struct DSI_TX_BUF_RW_TIMES_REG DSI_TX_BUF_RW_TIMES;	/* 0410 */
	unsigned int RSV_0414[61];				/* 0414..0504 */
	struct DSI_TX_DBG_CON_REG DSI_TX_DBG_CON;		/* 0508 */
	struct DSI_TX_CRC_CKSM_REG DSI_TX_CRC_CKSM;		/* 050C */
	struct DSI_TX_IN_CKSM_REG DSI_TX_IN_CKSM;		/* 0510 */
	unsigned int RSV_0514[12];				/* 0514..0540 */
	struct DSI_TX_O_CKSM_REG DSI_TX_O_CKSM;			/* 0544 */
	unsigned int RSV_0548[494];				/* 0548..0CCC */
	struct DSI_TX_CMDQ_REG DSI_TX_CMDQ;			/* 0D00 */
};

struct MMSYS_CON_REG {
	unsigned RSV_00 : 32;
};

struct MIPI_RX_POST_CTRL_REG {
	unsigned MIPI_RX_MODE_SEL : 1;
	unsigned RSV_01 : 3;
	unsigned IPI_POST_SW_RESET : 1;
	unsigned RSV_05 : 3;
	unsigned DDI_POST_REDAY_FORCE_HIGH : 1;
	unsigned IPI_POST_REDAY_FORCE_HIGH : 1;
	unsigned RSV_10 : 22;
};

struct TE_OUT_CON_REG {
	unsigned TE_OUT_SEL : 6;
	unsigned RSV_01 : 2;
	unsigned TE_OUT_INV : 1;
	unsigned TE_OUT_MASK : 1;
};

struct BDG_DISPSYS_CONFIG_REGS {
	unsigned int RSV_0000[64];				/* 0000..00FC */
	struct MMSYS_CON_REG MMSYS_CG_CON0;			/* 0100 */
	unsigned int RSV_0104[7];				/* 0104..011C */
	struct MMSYS_CON_REG MMSYS_HW_DCM_1ST_DIS0;		/* 0120 */
	unsigned int RSV_0124[7];				/* 0124..013C */
	struct MMSYS_CON_REG MMSYS_HW_DCM_2ND_DIS0;		/* 0140 */
	unsigned int RSV_0144[11];				/* 0144..016C */
	struct MIPI_RX_POST_CTRL_REG MIPI_RX_POST_CTRL;		/* 0170 */
	struct MMSYS_CON_REG DDI_POST_CTRL;			/* 0174 */
	unsigned int RSV_0178[12];				/* 0178..01a4 */
	struct TE_OUT_CON_REG TE_OUT_CON;			/* 01a8 */
};

struct DISP_DSC_CON_REG {
	unsigned DSC_EN : 1;
	unsigned RSV_01 : 1;
	unsigned DSC_DUAL_INOUT : 1;
	unsigned DSC_IN_SRC_SEL : 1;
	unsigned DSC_BYPASS : 1;
	unsigned DSC_RELAY : 1;
	unsigned DSC_ALL_BYPASS : 1;
	unsigned DSC_PT_MEM_EN : 1;
	unsigned DSC_SW_RESET : 1;
	unsigned STALL_CLK_GATE_EN : 1;
	unsigned RSV_10 : 4;
	unsigned DSC_EMPTY_FLAG_SEL : 2;
	unsigned DSC_UFOE_SEL : 1;
	unsigned DSC_IN_SWAP : 1;
	unsigned DSC_OUT_SWAP : 1;
	unsigned RSV_19 : 13;
};

struct DISP_DSC_PIC_W_REG {
	unsigned PIC_WIDTH : 16;
	unsigned PIC_GROUP_WIDTH_M1 : 16;
};

struct DISP_DSC_PIC_H_REG {
	unsigned PIC_HEIGHT_M1 : 16;
	unsigned PIC_HEIGHT_EXT_M1 : 16;
};

struct DISP_DSC_SLICE_W_REG {
	unsigned SLICE_WIDTH : 16;
	unsigned SLICE_GROUP_WIDTH_M1 : 16;
};

struct DISP_DSC_SLICE_H_REG {
	unsigned SLICE_HEIGHT_M1 : 16;
	unsigned SLICE_NUM_M1 : 14;
	unsigned SLICE_WIDTH_MOD3 : 2;
};

struct DISP_DSC_CHUNK_SIZE_REG {
	unsigned CHUNK_SIZE : 16;
	unsigned RSV_16 : 16;
};

struct DISP_DSC_BUF_SIZE_REG {
	unsigned BUF_SIZE : 24;
	unsigned RSV_24 : 8;
};

struct DISP_DSC_MODE_REG {
	unsigned SLICE_MODE : 1;
	unsigned PIX_TYPE : 1;
	unsigned RGB_SWAP : 1;
	unsigned RSV_03 : 5;
	unsigned INIT_DELAY_HEIGHT : 4;
	unsigned RSV_12 : 4;
	unsigned OBUF_STR_IF_BUF_FULL : 1;
	unsigned RSV_17 : 15;
};

struct DISP_DSC_CFG_REG {
	unsigned DSC_CFG : 32;
};

struct DISP_DSC_PAD_REG {
	unsigned PAD_NUM : 3;
	unsigned RSV_03 : 29;
};

struct DISP_DSC_OBUF_REG {
	unsigned OBUF_SIZE : 12;
	unsigned RSV_12 : 19;
	unsigned OBUF_SW : 1;
};

struct DISP_DSC_PPS_REG {
	unsigned RSV_00 : 32;
};

struct DISP_DSC_SHADOW_REG {
	unsigned RSV_00 : 3;
	unsigned MUTE_HL_SOF_SYNC : 1;
	unsigned MUTE_LH_SOF_SYNC : 1;
	unsigned DSC_VERSION_MINOR : 4;
	unsigned DSC_RELAY_FIFO_OFF : 1;
	unsigned RESERVE_BIT : 22;
};

struct BDG_DISP_DSC_REGS {
	struct DISP_DSC_CON_REG DISP_DSC_CON;			/* 0000 */
	unsigned int RSV_0004[5];				/* 0004..0014 */
	struct DISP_DSC_PIC_W_REG DISP_DSC_PIC_W;		/* 0018 */
	struct DISP_DSC_PIC_H_REG DISP_DSC_PIC_H;		/* 001C */
	struct DISP_DSC_SLICE_W_REG DISP_DSC_SLICE_W;		/* 0020 */
	struct DISP_DSC_SLICE_H_REG DISP_DSC_SLICE_H;		/* 0024 */
	struct DISP_DSC_CHUNK_SIZE_REG DISP_DSC_CHUNK_SIZE;	/* 0028 */
	struct DISP_DSC_BUF_SIZE_REG DISP_DSC_BUF_SIZE;		/* 002C */
	struct DISP_DSC_MODE_REG DISP_DSC_MODE;			/* 0030 */
	struct DISP_DSC_CFG_REG DISP_DSC_CFG;			/* 0034 */
	struct DISP_DSC_PAD_REG DISP_DSC_PAD;			/* 0038 */
	unsigned int RSV_003C[13];				/* 003C..006C */
	struct DISP_DSC_OBUF_REG DISP_DSC_OBUF;			/* 0070 */
	unsigned int RSV_0074[3];				/* 0074..007C */
	struct DISP_DSC_PPS_REG DISP_DSC_PPS[20];		/* 0080..00CC */
	unsigned int RSV_00D0[76];				/* 00D0..01FC */
	struct DISP_DSC_SHADOW_REG DISP_DSC_SHADOW;		/* 0200 */
};

struct SYSREG_PWR_CTRL_REG {
	unsigned REG_PISO_EN  : 1;
	unsigned REG_01 : 3;
	unsigned REG_DISP_PWR_CLK_DIS : 1;
	unsigned REG_05 : 3;
	unsigned REG_PWR_ON : 1;
	unsigned REG_PWR_ACK : 1;
	unsigned REG_PWR_ON_2ND : 1;			//10
	unsigned REG_PWR_ACK_2ND : 1;
	unsigned REG_DISP_SRAM_PDN : 1;			//12
	unsigned REG_DISP_SRAM_PDN_ACK : 1;
	unsigned REG_SRAM_PDN_MODE : 1;
	unsigned REG_15 : 1;
	unsigned REG_GCE_SRAM_PDN : 1;			//16
	unsigned REG_GCE_SRAM_SLEEP_B : 1;
	unsigned REG_GCE_SRAM_CKISO : 1;
	unsigned REG_GCE_SRAM_ISOINT_B : 1;		//19
	unsigned REG_SYSBUF_SRAM_PDN : 1;
	unsigned REG_SYSBUF_SRAM_SLEEP_B : 1;
	unsigned REG_SYSBUF_SRAM_CKISO : 1;		//22
	unsigned REG_SYSBUF_SRAM_ISOINT_B : 1;
	unsigned REG_24 : 8;
};

struct SYSREG_RST_CTRL_REG {
	unsigned REG_00 : 15;
	unsigned REG_SW_RST_EN_DISP_PWR_WRAP : 1;
	unsigned REG_16 : 16;
};

struct SYSREG_IRQ_CTRL_REG {
	unsigned REG_00 : 31;
	unsigned RG_IRQ_STATUS31_MTCMOS_PWR_ACK : 1;
};

struct RST_CLR_SET_REG {
	unsigned REG_00 : 7;
	unsigned REG_07 : 1;
	unsigned REG_08 : 24;
};

struct IRQ_MSK_CLR_SET_REG {
	unsigned REG_00 : 4;
	unsigned REG_04 : 1;
	unsigned REG_05 : 5;
	unsigned REG_10 : 1;
	unsigned REG_11 : 20;
	unsigned REG_31 : 1;
};

struct DISP_SYSREG_REG {
	unsigned REG_00 : 32;
};

struct SYSREG_LDO_CTRL0_REG {
	unsigned REG_00 : 30;
	unsigned RG_LDO_TRIM_BY_EFUSE : 1;
	unsigned RG_PHYLDO_MASKB : 1;
};

struct SYSREG_LDO_CTRL1_REG {
	unsigned RG_PHYLDO2_EN : 1;
	unsigned REG_01 : 28;
	unsigned RG_PHYLDO1_LP_EN : 1;
	unsigned REG_30 : 2;
};

struct CKBUF_CTRL_REG {
	unsigned NFC_CKBUF_TERM : 4;
	unsigned EXT_CLK_REQ_EN : 1;
	unsigned NFC_CK_OUT_EN : 1;
	unsigned REG_06 : 26;
};

struct BDG_SYSREG_CTRL_REGS {
	unsigned int RSV_0000[2];				/* 0000..0004 */
	struct SYSREG_PWR_CTRL_REG SYSREG_PWR_CTRL;		/* 0008 */
	unsigned int RSV_000C;					/* 000C */
	struct SYSREG_RST_CTRL_REG SYSREG_RST_CTRL;		/* 0010 */
	unsigned int RSV_0014[1];				/* 0014 */
	struct SYSREG_IRQ_CTRL_REG SYSREG_IRQ_CTRL1;		/* 0018 */
	struct SYSREG_IRQ_CTRL_REG SYSREG_IRQ_CTRL2;	/* 001C */
	struct SYSREG_IRQ_CTRL_REG SYSREG_IRQ_CTRL3; /* 0020 */
	unsigned int RSV_0024[16];				/* 0024..0060 */
	struct DISP_SYSREG_REG RST_DG_CTRL;			/* 0064 */
	struct RST_CLR_SET_REG RST_SET;				/* 0068 */
	struct RST_CLR_SET_REG RST_CLR;				/* 006C */
	struct IRQ_MSK_CLR_SET_REG IRQ_MSK_SET;			/* 0070 */
	struct IRQ_MSK_CLR_SET_REG IRQ_MSK_CLR;			/* 0074 */
	unsigned int RSV_0070[2];				/* 0078..007C */
	struct DISP_SYSREG_REG DISP_MISC0;			/* 0080 */
	struct DISP_SYSREG_REG DISP_MISC1;			/* 0084 */
	unsigned int RSV_0088[6];				/* 0088..009C */
	struct CKBUF_CTRL_REG CKBUF_CTRL;			/* 00A0 */
	struct SYSREG_LDO_CTRL0_REG SYSREG_LDO_CTRL0;		/* 00A4 */
	struct SYSREG_LDO_CTRL1_REG SYSREG_LDO_CTRL1;		/* 00A8 */
	struct DISP_SYSREG_REG LDO_STATUS;			/* 00AC */
};

struct CLK_CFG_REG {
	unsigned RSV_00 : 32;
};

struct BDG_TOPCKGEN_REGS {
	struct CLK_CFG_REG CLK_MODE;				/* 0000 */
	struct CLK_CFG_REG CLK_CFG_UPDATE;			/* 0004 */
	unsigned int RSV_0008[2];				/* 0008..000C */
	struct CLK_CFG_REG CLK_CFG_0;				/* 0010 */
	struct CLK_CFG_REG CLK_CFG_0_SET;			/* 0014 */
	struct CLK_CFG_REG CLK_CFG_0_CLR;			/* 0018 */
};

struct AP_PLL_CON_REG {
	unsigned RSV_00 : 32;
};

struct PLLON_CON_REG {
	unsigned RSV_00 : 32;
};

struct MAINPLL_CON_REG {
	unsigned RSV_00 : 32;
};

struct BDG_APMIXEDSYS_REGS {
	unsigned int RSV_0000[3];				/* 0000..0008 */
	struct AP_PLL_CON_REG AP_PLL_CON3;			/* 00c */
	struct AP_PLL_CON_REG AP_PLL_CON4;			/* 010 */
	struct AP_PLL_CON_REG AP_PLL_CON5;			/* 014 */
	unsigned int RSV_0018[14];				/* 0018..004c */
	struct PLLON_CON_REG PLLON_CON0;			/* 050 */
	struct PLLON_CON_REG PLLON_CON1;			/* 054 */
	unsigned int RSV_0058[112];				/* 0058..0214 */
	struct MAINPLL_CON_REG MAINPLL_CON0;		/* 0218 */
	struct MAINPLL_CON_REG MAINPLL_CON1;		/* 021C */
	unsigned int RSV_0220;						/* 0220 */
	struct MAINPLL_CON_REG MAINPLL_CON3;		/* 0224 */
};

struct GPIO_MODE1_REG {
	unsigned RSV_00 : 16;
	unsigned GPIO12 : 3;
	unsigned RSV_19 : 5;
	unsigned RSV_24 : 8;
};

struct BDG_GPIO_REGS {
	unsigned int RSV_0000[196];				/* 0000..030C */
	struct GPIO_MODE1_REG GPIO_MODE1;			/* 0310 */
};

struct EFUSE_REG {
	unsigned RSV_00 : 32;
};

struct BDG_EFUSE_REGS {
	unsigned int RSV_0000[74];				/* 0000..0124 */
	struct EFUSE_REG STATUS;				/* 0128 */
	unsigned int RSV_012C[213];				/* 012C..047C */
	struct EFUSE_REG DCM_ON;				/* 0480 */
	unsigned int RSV_0484[279];				/* 0484..08DC */
	struct EFUSE_REG TRIM1;					/* 08E0 */
	struct EFUSE_REG TRIM2;					/* 08E4 */
	struct EFUSE_REG TRIM3;					/* 08E8 */
};

struct DISP_RDMA_GLOBAL_CON_REG {
	unsigned ENGINE_EN : 1;
	unsigned MODE_SEL : 1;
	unsigned RG_UFOD_EN : 1;
	unsigned RG_PIXEL_10_BIT : 1;
	unsigned SOFT_RESET : 1;
	unsigned RG_CONST_CLR_EN : 1;
	unsigned RG_LINE_PACKAGE_CTL_EN : 1;
	unsigned RSV_07 : 25;
};

struct DISP_RDMA_SIZE_CON_0_REG {
	unsigned OUTPUT_FRAME_WIDTH : 13;
	unsigned RSV_13 : 19;
};

struct DISP_RDMA_SIZE_CON_1_REG {
	unsigned OUTPUT_FRAME_HEIGHT : 13;
	unsigned RSV_13 : 19;
};

struct DISP_RDMA_FIFO_CON_REG {
	unsigned OUTPUT_VALID_FIFO_THRESHOLD : 14;
	unsigned RSV_14 : 1;
	unsigned OUTPUT_VALID_THRESHOLD_PER_LINE : 1;
	unsigned FIFO_PSEUDO_SIZE : 14;
	unsigned RSV_30 : 1;
	unsigned FIFO_UNDERFLOW_EN : 1;
};

struct DISP_RDMA_SRAM_SEL_REG {
	unsigned RDMA_SRAM_SEL : 3;
	unsigned RSV_03 : 29;
};

struct DISP_RDMA_STALL_CG_CON_REG {
	unsigned RDMA_CG_CON : 32;
};

struct BDG_RDMA0_REGS {
	unsigned int RSV_0000[4];				/* 0000..000C */
	struct DISP_RDMA_GLOBAL_CON_REG DISP_RDMA_GLOBAL_CON;	/* 0010 */
	struct DISP_RDMA_SIZE_CON_0_REG DISP_RDMA_SIZE_CON_0;	/* 0014 */
	struct DISP_RDMA_SIZE_CON_1_REG DISP_RDMA_SIZE_CON_1;	/* 0018 */
	unsigned int RSV_001C[9];				/* 001C..003C */
	struct DISP_RDMA_FIFO_CON_REG DISP_RDMA_FIFO_CON;	/* 0040 */
	unsigned int RSV_0044[27];				/* 0044..00AC */
	struct DISP_RDMA_SRAM_SEL_REG DISP_RDMA_SRAM_SEL;	/* 00B0 */
	struct DISP_RDMA_STALL_CG_CON_REG DISP_RDMA_STALL_CG_CON;/* 00B4 */
};

struct DISP_MUTEX_INTEN_REG {
	unsigned MUTEX_INTEN : 32;
};

struct DISP_MUTEX0_EN_REG {
	unsigned MUTEX0_EN : 1;
	unsigned RSV_01 : 31;
};

struct DISP_MUTEX0_CTL_REG {
	unsigned MUTEX_SOF : 3;
	unsigned MUTEX_SOF_TIMING : 2;
	unsigned MUTEX_SOF_WAIT : 1;
	unsigned MUTEX_EOF : 3;
	unsigned MUTEX_EOF_TIMING : 2;
	unsigned MUTEX_EOF_WAIT : 1;
	unsigned RSV_12 : 20;
};

struct DISP_MUTEX0_MOD0_REG {
	unsigned MUTEX_MOD0 : 32;
};

struct BDG_MUTEX_REGS {
	struct DISP_MUTEX_INTEN_REG DISP_MUTEX_INTEN;		/* 0000 */
	unsigned int RSV_0004[7];				/* 0004..001C */
	struct DISP_MUTEX0_EN_REG DISP_MUTEX0_EN;		/* 0020 */
	unsigned int RSV_0024[2];				/* 0024..0028 */
	struct DISP_MUTEX0_CTL_REG DISP_MUTEX0_CTL;		/* 002C */
	struct DISP_MUTEX0_MOD0_REG DISP_MUTEX0_MOD0;		/* 0030 */
};

struct DSI2_DEVICE_RSV32_OS_REG {
	unsigned RSV_00 : 32;
};

struct BDG_MIPIDSI2_REGS {
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_VERSION_OS;		/* 0000 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_SOFT_RSTN_OS;		/* 0004 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_N_LANES_OS;		/* 0008 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_ST_MAIN_OS;	/* 000C */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_EOTP_CFG_OS;		/* 0010 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_CTRL_CFG_OS;		/* 0014 */
	unsigned int RSV_0018[2];				/* 0018..001C */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_FIFO_STATUS_OS;	/* 0020 */
	unsigned int RSV_0024[55];				/* 0024..00FC */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_PHY_SHUTDOWNZ_OS;	/* 0100 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_PHY_RSTZ_OS;		/* 0104 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_PHY_CAL_OS;		/* 0108 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_PHY_MODE_OS;		/* 010C */
	unsigned int RSV_0110;					/* 0110 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_PHY_TEST_SEL_OS;	/* 0114 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_PHY_TEST_CTRL0_OS;	/* 0118 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_PHY_TEST_CTRL1_OS;	/* 011C */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_PHY_CLK_STATUS_OS;	/* 0120 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_PHY_DATA_STATUS_OS;	/* 0124 */
	unsigned int RSV_0128[6];				/* 0128..013C */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_LPTXRDY_TO_CNT_OS;	/* 0140 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_LPTX_TO_CNT_OS;	/* 0144 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_HSRX_TO_CNT_OS;	/* 0148 */
	unsigned int RSV_014C[45];				/* 014C..01FC */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_ST_PHY_FATAL_OS;	/* 0200 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_MASK_N_PHY_FATAL_OS;	/* 0204 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_FORCE_PHY_FATAL_OS;	/* 0208 */
	unsigned int RSV_020C;					/* 020C */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_ST_PHY_OS;	/* 0210 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_MASK_N_PHY_OS;	/* 0214 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_FORCE_PHY_OS;	/* 0218 */
	unsigned int RSV_021C;					/* 021C */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_ST_DSI_FATAL_OS;	/* 0220 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_MASK_N_DSI_FATAL_OS;	/* 0224 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_FORCE_DSI_FATAL_OS;	/* 0228 */
	unsigned int RSV_022C;					/* 022C */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_ST_DSI_OS;	/* 0230 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_MASK_N_DSI_OS;	/* 0234 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_FORCE_DSI_OS;	/* 0238 */
	unsigned int RSV_023C;						/* 023C */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_ST_DDI_FATAL_OS;	/* 0240 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_MASK_N_DDI_FATAL_OS;	/* 0244 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_FORCE_DDI_FATAL_OS;	/* 0248 */
	unsigned int RSV_024C;							/* 024C */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_ST_DDI_OS;	/* 0250 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_MASK_N_DDI_OS;	/* 0254 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_FORCE_DDI_OS;	/* 0258 */
	unsigned int RSV_025C;							/* 025C */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_ST_IPI_FATAL_OS;	/* 0260 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_MASK_N_IPI_FATAL_OS;	/* 0264 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_FORCE_IPI_FATAL_OS;	/* 0268 */
	unsigned int RSV_026C;							/* 026C */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_ST_IPI_OS;	/* 0270 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_MASK_N_IPI_OS;	/* 0274 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_FORCE_IPI_OS;	/* 0278 */
	unsigned int RSV_027C;							/* 027C */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_ST_FIFO_FATAL_OS;	/* 0280 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_MASK_N_FIFO_FATAL_OS;	/* 0284 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_FORCE_FIFO_FATAL_OS;	/* 0288 */
	unsigned int RSV_028C[9];						/* 028C..02AC */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_ST_ERR_RPT_OS;		/* 02B0 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_MASK_N_ERR_RPT_OS;	/* 02B4 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_FORCE_ERR_RPT_OS;	/* 02B8 */
	unsigned int RSV_02BC[5];						/* 02BC..02CC */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_ST_RX_TRIGGERS_OS;	/* 02D0 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_MASK_N_RX_TRIGGERS_OS;	/* 02D4 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_INT_FORCE_RX_TRIGGERS_OS;	/* 02D8 */
	unsigned int RSV_02DC[9];				/* 02DC..02FC */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_DDI_RDY_TO_CNT_OS;	/* 0300 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_DDI_RESP_TO_CNT_OS;	/* 0304 */
	unsigned int RSV_0308[2];				/* 0308..030C */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_DDI_VALID_VC_CFG_OS;/* 0310 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_DDI_CLK_MGR_CFG_OS;	/* 0314 */
	unsigned int RSV_0318[58];				/* 0318..03FC */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_IPI_MODE_CFG_OS;	/* 0400 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_IPI_VALID_VC_CFG_OS;	/* 0404 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_IPI_TX_DELAY_OS;	/* 0408 */
	unsigned int RSV_040C[45];				/* 040C..04BC */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_IPI_PG_EN_OS;	/* 04C0 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_IPI_PG_ACTIVE_OS;	/* 04C4 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_IPI_PG_CFG_OS;	/* 04C8 */
	unsigned int RSV_04CC;						/* 04CC */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_IPI_PG_PIXEL_NUM_OS;	/* 04D0 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_IPI_PG_HSA_TIME_OS;	/* 04D4 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_IPI_PG_HBP_TIME_OS;	/* 04D8 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_IPI_PG_HLINE_TIME_OS;	/* 04DC */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_IPI_PG_VSA_LINES_OS;	/* 04E0 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_IPI_PG_VBP_LINES_OS;	/* 04E4 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_IPI_PG_VFP_LINES_OS;	/* 04E8 */
	struct DSI2_DEVICE_RSV32_OS_REG DSI2_DEVICE_IPI_PG_VACTIVE_LINES_OS;	/* 04EC */

};

struct GCE_RSV32_REG {
	unsigned RSV_00 : 32;
};

struct BDG_GCE_REGS {
	unsigned int RSV_0000[60];				/* 0000..00EC */
	struct GCE_RSV32_REG GCE_CTL_INT0;			/* 00F0 */
};

struct OCLA_RSV32_REG {
	unsigned RSV_00 : 32;
};

struct OCLA_LANE_CON_REG {
	unsigned ENABLE_SEL : 1;
	unsigned SW_ENABLE : 1;
	unsigned FORCE_RX_MODE : 1;
	unsigned FORCE_TX_STOP_MODE : 1;
	unsigned RSV_04 : 28;
};

struct BDG_OCLA_REGS {
	unsigned int RSV_0000[4];				/* 0000..000C */
	struct OCLA_RSV32_REG OCLA_PHY_READY;			/* 0010 */
	unsigned int RSV_0014[3];				/* 0014..001C */
	struct OCLA_RSV32_REG OCLA_LANE_SWAP;			/* 0020 */
	unsigned int RSV_0024[55];				/* 0024..00FC */
	struct OCLA_LANE_CON_REG OCLA_LANE0_CON;		/* 0100 */
	unsigned int RSV_0104[10];				/* 0104..0128 */
	struct OCLA_RSV32_REG OCLA_LANE0_STOPSTATE;		/* 012C */
	unsigned int RSV_0130[52];				/* 0130..01FC */
	struct OCLA_LANE_CON_REG OCLA_LANE1_CON;		/* 0200 */
	unsigned int RSV_0204[63];				/* 0204..02FC */
	struct OCLA_LANE_CON_REG OCLA_LANE2_CON;		/* 0300 */
	unsigned int RSV_0304[63];				/* 0304..03FC */
	struct OCLA_LANE_CON_REG OCLA_LANE3_CON;		/* 0400 */
	unsigned int RSV_0404[63];				/* 0404..04FC */
	struct OCLA_LANE_CON_REG OCLA_LANEC_CON;		/* 0500 */
};

struct MIPI_TX_LANE_CON_REG {
	unsigned RG_DSI_CPHY_T1DRV_EN : 1;
	unsigned RG_DSI_ANA_CK_SEL : 1;
	unsigned RG_DSI_PHY_CK_SEL : 1;
	unsigned RG_DSI_CPHY_EN : 1;
	unsigned RG_DSI_PHYCK_INV_EN : 1;
	unsigned RG_DSI_PWR04_EN : 1;
	unsigned RG_DSI_BG_LPF_EN : 1;
	unsigned RG_DSI_BG_CORE_EN : 1;
	unsigned RG_DSI_PAD_TIEL_SEL : 1;
	unsigned RSV_09 : 23;
};

struct MIPI_TX_PLL_PWR_REG {
	unsigned AD_DSI_PLL_SDM_PWR_ON : 1;
	unsigned AD_DSI_PLL_SDM_ISO_EN : 1;
	unsigned RSV_02 : 30;
};

struct MIPI_TX_PLL_CON0_REG {
	unsigned RG_DSI_PLL_SDM_PCW : 32;
};

struct MIPI_TX_PLL_CON1_REG {
	unsigned RG_DSI_PLL_SDM_PCW_CHG : 1;
	unsigned RSV_01 : 3;
	unsigned RG_DSI_PLL_EN : 1;
	unsigned RSV_05 : 3;
	unsigned RG_DSI_PLL_POSDIV : 3;
	unsigned RG_DSI_PLL_PREDIV : 2;
	unsigned RG_DSI_PLL_SDM_FRA_EN : 1;
	unsigned RG_DSI_PLL_SDM_HREN : 1;
	unsigned RG_DSI_PLL_LVROD_EN : 1;
	unsigned RG_DSI_PLL_BP : 1;
	unsigned RG_DSI_PLL_BR : 1;
	unsigned RG_DSI_PLL_BLP : 1;
	unsigned RSV_19 : 1;
	unsigned RG_DSI_PLL_RST_DLY : 2;
	unsigned RSV_22 : 10;
};

struct MIPI_TX_PLL_CON2_REG {
	unsigned RG_DSI_PLL_SDM_SSC_PH_INIT : 1;
	unsigned RG_DSI_PLL_SDM_SSC_EN : 1;
	unsigned RSV_02 : 14;
	unsigned RG_DSI_PLL_SDM_SSC_PRD : 16;
};

struct MIPI_TX_PLL_CON3_REG {
	unsigned RG_DSI_PLL_SDM_SSC_DELTA1 : 16;
	unsigned RG_DSI_PLL_SDM_SSC_DELTA : 16;
};

struct MIPI_TX_PLL_CON4_REG {
	unsigned RG_DSI_PLL_MONCK_EN : 1;
	unsigned RSV_01 : 1;
	unsigned RG_DSI_PLL_MONVC_EN : 2;
	unsigned RG_DSI_PLL_MONREF_EN : 1;
	unsigned RG_DSI_PLL_BW : 3;
	unsigned RG_DSI_PLL_FS : 2;
	unsigned RG_DSI_PLL_IBIAS : 2;
	unsigned RG_DSI_PLL_ICHP : 2;
	unsigned RSV_14 : 2;
	unsigned RG_DSI_PLL_RESERVED : 16;
};

struct MIPI_TX_PHY_SEL0_REG {
	unsigned MIPI_TX_CPHY_EN : 2;
	unsigned RSV_02 : 2;
	unsigned MIPI_TX_PHY2_SEL : 4;
	unsigned MIPI_TX_CPHY0BC_SEL : 4;
	unsigned MIPI_TX_PHY0_SEL : 4;
	unsigned MIPI_TX_PHY1AB_SEL : 4;
	unsigned MIPI_TX_PHYC_SEL : 4;
	unsigned MIPI_TX_CPHY1CA_SEL : 4;
	unsigned MIPI_TX_PHY1_SEL : 4;
};

struct MIPI_TX_PHY_SEL1_REG {
	unsigned MIPI_TX_PHY2BC_SEL : 4;
	unsigned MIPI_TX_PHY3_SEL : 4;
	unsigned MIPI_TX_CPHYXXX_SEL : 4;
	unsigned MIPI_TX_LPRX0AB_SEL : 4;
	unsigned MIPI_TX_LPRX0BC_SEL : 4;
	unsigned MIPI_TX_LPRX0CA_SEL : 4;
	unsigned MIPI_TX_CPHY0_HS_SEL : 2;
	unsigned MIPI_TX_CPHY1_HS_SEL : 2;
	unsigned MIPI_TX_CPHY2_HS_SEL : 2;
	unsigned RSV_30 : 2;
};

struct MIPI_TX_PHY_SEL2_REG {
	unsigned MIPI_TX_PHY2_HSDATA_SEL : 4;
	unsigned MIPI_TX_CPHY0BC_HSDATA_SEL : 4;
	unsigned MIPI_TX_PHY0_HSDATA_SEL : 4;
	unsigned MIPI_TX_PHY1AB_HSDATA_SEL : 4;
	unsigned MIPI_TX_PHYC_HSDATA_SEL : 4;
	unsigned MIPI_TX_CPHY1CA_HSDATA_SEL : 4;
	unsigned MIPI_TX_PHY1_HSDATA_SEL : 4;
	unsigned MIPI_TX_PHY2BC_HSDATA_SEL : 4;
};

struct MIPI_TX_PHY_SEL3_REG {
	unsigned MIPI_TX_PHY3_HSDATA_SEL : 4;
	unsigned RSV_04 : 28;
};

struct MIPI_TX_SW_CTRL_CON4_REG {
	unsigned RSV_00 : 8;
	unsigned MIPI_TX_SW_ANA_CK_EN : 1;
	unsigned RSV_09 : 23;
};

struct MIPI_TX_CKMODE_EN_REG {
	unsigned DSI_CKMODE_EN : 1;
	unsigned RSV_01 : 31;
};

struct MIPI_TX_SW_CTL_EN_REG {
	unsigned DSI_SW_CTL_EN : 1;
	unsigned RSV_01 : 31;
};

struct MIPI_TX_SW_LPTX_PRE_OE_REG {
	unsigned DSI_SW_LPTX_PRE_OE : 1;
	unsigned RSV_01 : 31;
};

struct MIPI_TX_SW_LPTX_OE_REG {
	unsigned DSI_SW_LPTX_OE : 1;
	unsigned RSV_01 : 31;
};

struct MIPI_TX_SW_LPTX_DP_REG {
	unsigned DSI_SW_LPTX_DP : 1;
	unsigned RSV_01 : 31;
};

struct MIPI_TX_SW_LPTX_DN_REG {
	unsigned DSI_SW_LPTX_DN : 1;
	unsigned RSV_01 : 31;
};

struct BDG_MIPI_TX_REGS {
	unsigned int RSV_0000[3];				/* 0000..0008 */
	struct MIPI_TX_LANE_CON_REG MIPI_TX_LANE_CON;		/* 000C */
	unsigned int RSV_0010[6];				/* 0010..0024 */
	struct MIPI_TX_PLL_PWR_REG MIPI_TX_PLL_PWR;		/* 0028 */
	struct MIPI_TX_PLL_CON0_REG MIPI_TX_PLL_CON0;		/* 002C */
	struct MIPI_TX_PLL_CON1_REG MIPI_TX_PLL_CON1;		/* 0030 */
	struct MIPI_TX_PLL_CON2_REG MIPI_TX_PLL_CON2;		/* 0034 */
	struct MIPI_TX_PLL_CON3_REG MIPI_TX_PLL_CON3;		/* 0038 */
	struct MIPI_TX_PLL_CON4_REG MIPI_TX_PLL_CON4;		/* 003C */
	struct MIPI_TX_PHY_SEL0_REG MIPI_TX_PHY_SEL0;		/* 0040 */
	struct MIPI_TX_PHY_SEL1_REG MIPI_TX_PHY_SEL1;		/* 0044 */
	struct MIPI_TX_PHY_SEL2_REG MIPI_TX_PHY_SEL2;		/* 0048 */
	struct MIPI_TX_PHY_SEL3_REG MIPI_TX_PHY_SEL3;		/* 004C */
	unsigned int RSV_0050[4];				/* 0050..005C */
	struct MIPI_TX_SW_CTRL_CON4_REG MIPI_TX_SW_CTRL_CON4;	/* 0060 */
	unsigned int RSV_0064[49];				/* 0064..0124 */
	struct MIPI_TX_CKMODE_EN_REG MIPI_TX_D2_CKMODE_EN;	/* 0128 */
	unsigned int RSV_012C[6];				/* 012C..0140 */
	struct MIPI_TX_SW_CTL_EN_REG MIPI_TX_D2_SW_CTL_EN;	/* 0144 */
	struct MIPI_TX_SW_LPTX_PRE_OE_REG MIPI_TX_D2_SW_LPTX_PRE_OE;	/* 0148 */
	struct MIPI_TX_SW_LPTX_OE_REG MIPI_TX_D2_SW_LPTX_OE;	/* 014c */
	struct MIPI_TX_SW_LPTX_DP_REG MIPI_TX_D2_SW_LPTX_DP;	/* 0150 */
	struct MIPI_TX_SW_LPTX_DN_REG MIPI_TX_D2_SW_LPTX_DN;	/* 0154 */
	unsigned int RSV_0158[52];				/* 0158..0224 */
	struct MIPI_TX_CKMODE_EN_REG MIPI_TX_D0_CKMODE_EN;	/* 0228 */
	unsigned int RSV_022C[6];				/* 022C..0240 */
	struct MIPI_TX_SW_CTL_EN_REG MIPI_TX_D0_SW_CTL_EN;	/* 0244 */
	struct MIPI_TX_SW_LPTX_PRE_OE_REG MIPI_TX_D0_SW_LPTX_PRE_OE;	/* 0248 */
	struct MIPI_TX_SW_LPTX_OE_REG MIPI_TX_D0_SW_LPTX_OE;	/* 024c */
	struct MIPI_TX_SW_LPTX_DP_REG MIPI_TX_D0_SW_LPTX_DP;	/* 0250 */
	struct MIPI_TX_SW_LPTX_DN_REG MIPI_TX_D0_SW_LPTX_DN;	/* 0254 */
	unsigned int RSV_0258[52];				/* 0258..0324 */
	struct MIPI_TX_CKMODE_EN_REG MIPI_TX_CK_CKMODE_EN;	/* 0328 */
	unsigned int RSV_032C[6];				/* 032C..0340 */
	struct MIPI_TX_SW_CTL_EN_REG MIPI_TX_CK_SW_CTL_EN;	/* 0344 */
	struct MIPI_TX_SW_LPTX_PRE_OE_REG MIPI_TX_CK_SW_LPTX_PRE_OE;	/* 0348 */
	struct MIPI_TX_SW_LPTX_OE_REG MIPI_TX_CK_SW_LPTX_OE;	/* 034c */
	struct MIPI_TX_SW_LPTX_DP_REG MIPI_TX_CK_SW_LPTX_DP;	/* 0350 */
	struct MIPI_TX_SW_LPTX_DN_REG MIPI_TX_CK_SW_LPTX_DN;	/* 0354 */
	unsigned int RSV_0358[52];				/* 0358..0424 */
	struct MIPI_TX_CKMODE_EN_REG MIPI_TX_D1_CKMODE_EN;	/* 0428 */
	unsigned int RSV_042C[6];				/* 042C..0440 */
	struct MIPI_TX_SW_CTL_EN_REG MIPI_TX_D1_SW_CTL_EN;	/* 0444 */
	struct MIPI_TX_SW_LPTX_PRE_OE_REG MIPI_TX_D1_SW_LPTX_PRE_OE;	/* 0448 */
	struct MIPI_TX_SW_LPTX_OE_REG MIPI_TX_D1_SW_LPTX_OE;	/* 044c */
	struct MIPI_TX_SW_LPTX_DP_REG MIPI_TX_D1_SW_LPTX_DP;	/* 0450 */
	struct MIPI_TX_SW_LPTX_DN_REG MIPI_TX_D1_SW_LPTX_DN;	/* 0454 */
	unsigned int RSV_0458[52];				/* 0458..0524 */
	struct MIPI_TX_CKMODE_EN_REG MIPI_TX_D3_CKMODE_EN;	/* 0528 */
	unsigned int RSV_052C[6];				/* 052C..0540 */
	struct MIPI_TX_SW_CTL_EN_REG MIPI_TX_D3_SW_CTL_EN;	/* 0544 */
	struct MIPI_TX_SW_LPTX_PRE_OE_REG MIPI_TX_D3_SW_LPTX_PRE_OE;	/* 0548 */
	struct MIPI_TX_SW_LPTX_OE_REG MIPI_TX_D3_SW_LPTX_OE;	/* 054c */
	struct MIPI_TX_SW_LPTX_DP_REG MIPI_TX_D3_SW_LPTX_DP;	/* 0550 */
	struct MIPI_TX_SW_LPTX_DN_REG MIPI_TX_D3_SW_LPTX_DN;	/* 0554 */
};

/**
 * 0~1 TYPE, 2 BTA, 3 HS, 4 CL, 5 TE, 6~7 RESV,
 * 8~15 DATA_ID, 16~23 DATA_0, 24~31 DATA_1
 */
struct DSI_TX_CMDQ {
	unsigned char byte0;
	unsigned char byte1;
	unsigned char byte2;
	unsigned char byte3;
};

struct DSI_TX_CMDQ_REGS {
	struct DSI_TX_CMDQ data[128]; /* only support 128 cmdq */
};

struct DSI_TX_VM_CMDQ {
	unsigned char byte0;
	unsigned char byte1;
	unsigned char byte2;
	unsigned char byte3;
};

struct DSI_TX_VM_CMDQ_REGS {
	struct DSI_TX_VM_CMDQ data[4];
};

struct DSI_TX_T0_INS {
	unsigned CONFG:8;
	unsigned Data_ID:8;
	unsigned Data0:8;
	unsigned Data1:8;
};

struct DSI_TX_T1_INS {
	unsigned CONFG:8;
	unsigned Data_ID:8;
	unsigned mem_start0:8;
	unsigned mem_start1:8;
};

struct DSI_TX_T2_INS {
	unsigned CONFG:8;
	unsigned Data_ID:8;
	unsigned WC16:16;
	unsigned int *pdata;
};

struct DSI_TX_T3_INS {
	unsigned CONFG:8;
	unsigned Data_ID:8;
	unsigned mem_start0:8;
	unsigned mem_start1:8;
};
#endif /* _DDP_REG_DSI_TX_H_ */
