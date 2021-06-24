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

#ifndef _DDP_REG_DSI_H_
#define _DDP_REG_DSI_H_


/* field definition */
/* ------------------------------------------------------------- */
/* DSI */

struct DSI_START_REG {
	unsigned DSI_START:1;
	unsigned rsv_1:1;
	unsigned SLEEPOUT_START:1;
	unsigned rsv_3:1;
	unsigned SKEWCAL_START:1;
	unsigned rsv_5:11;
	unsigned VM_CMD_START:1;
	unsigned rsv_17:15;
};

struct DSI_STATUS_REG {
	unsigned rsv_0:1;
	unsigned BUF_UNDERRUN:1; /* rsv */
	unsigned rsv_2:2;
	unsigned ESC_ENTRY_ERR:1;
	unsigned ESC_SYNC_ERR:1;
	unsigned CTRL_ERR:1;
	unsigned CONTENT_ERR:1;
	unsigned rsv_8:24;
};

struct DSI_INT_ENABLE_REG {
	unsigned RD_RDY:1;
	unsigned CMD_DONE:1;
	unsigned TE_RDY:1;
	unsigned VM_DONE:1;
	unsigned FRAME_DONE:1;
	unsigned VM_CMD_DONE:1;
	unsigned SLEEPOUT_DONE:1;
	unsigned TE_TIMEOUT_INT_EN:1;
	unsigned VM_VBP_STR_INT_EN:1;
	unsigned VM_VACT_STR_INT_EN:1;
	unsigned VM_VFP_STR_INT_EN:1;
	unsigned SKEWCAL_DONE_INT_EN:1;
	unsigned BUFFER_UNDERRUN_INT_EN:1;
	unsigned BTA_TIMEOUT_INT_EN:1;
	unsigned INP_UNFINISH_INT_EN:1;
	unsigned SLEEPIN_ULPS_INT_EN:1;
	unsigned LPRX_RD_RDY_EVENT_EN:1;
	unsigned CMD_DONE_EVENT_EN:1;
	unsigned TE_RDY_EVENT_EN:1;
	unsigned VM_DONE_EVENT_EN:1;
	unsigned FRAME_DONE_EVENT_EN:1;
	unsigned VM_CMD_DONE_EVENT_EN:1;
	unsigned SLEEPOUT_DONE_EVENT_EN:1;
	unsigned TE_TIMEOUT_EVENT_EN:1;
	unsigned VM_VBP_STR_EVENT_EN:1;
	unsigned VM_VACT_STR_EVENT_EN:1;
	unsigned VM_VFP_STR_EVENT_EN:1;
	unsigned SKEWCAL_DONE_EVENT_EN:1;
	unsigned BUFFER_UNDERRUN_EVENT_EN:1;
	unsigned res_29:3; /* 3 events */
};

struct DSI_INT_STATUS_REG {
	unsigned RD_RDY:1;
	unsigned CMD_DONE:1;
	unsigned TE_RDY:1;
	unsigned VM_DONE:1;
	unsigned FRAME_DONE_INT_EN:1;
	unsigned VM_CMD_DONE:1;
	unsigned SLEEPOUT_DONE:1;
	unsigned TE_TIMEOUT_INT_EN:1;
	unsigned VM_VBP_STR_INT_EN:1;
	unsigned VM_VACT_STR_INT_EN:1;
	unsigned VM_VFP_STR_INT_EN:1;
	unsigned SKEWCAL_DONE_INT_EN:1;
	unsigned BUFFER_UNDERRUN_INT_EN:1;
	unsigned BTA_TIMEOUT_INT_EN:1;
	unsigned INP_UNFINISH_INT_EN:1;
	unsigned SLEEPIN_DONE:1;
	unsigned rsv_16:15;
	unsigned BUSY:1;
};

struct DSI_COM_CTRL_REG {
	unsigned DSI_RESET:1;
	unsigned rsv_1:1;
	unsigned DPHY_RESET:1;
	unsigned rsv_3:1;
	unsigned DSI_DUAL_EN:1;
	unsigned rsv_5:27;
};

enum DSI_MODE_CTRL {
	DSI_CMD_MODE = 0,
	DSI_SYNC_PULSE_VDO_MODE = 1,
	DSI_SYNC_EVENT_VDO_MODE = 2,
	DSI_BURST_VDO_MODE = 3
};

struct DSI_MODE_CTRL_REG {
	unsigned MODE:2;
	unsigned rsv_2:6;
	unsigned INTERLACE_MODE:2;
	unsigned rsv_10:6;
	unsigned FRM_MODE:1;
	unsigned MIX_MODE:1;
	unsigned V2C_SWITCH_ON:1;
	unsigned C2V_SWITCH_ON:1;
	unsigned SLEEP_MODE:1;
	unsigned rsv_21:11;
};

enum DSI_LANE_NUM {
	ONE_LANE = 1,
	TWO_LANE = 2,
	THREE_LANE = 3,
	FOUR_LANE = 4
};

struct DSI_TXRX_CTRL_REG {
	unsigned VC_NUM:2;
	unsigned LANE_NUM:4;
	unsigned DIS_EOT:1;
	unsigned BLLP_EN:1;
	unsigned TE_FREERUN:1;
	unsigned EXT_TE_EN:1;
	unsigned EXT_TE_EDGE:1;
	unsigned TE_AUTO_SYNC:1;
	unsigned MAX_RTN_SIZE:4;
	unsigned HSTX_CKLP_EN:1;
	unsigned TYPE1_BTA_SEL:1;
	unsigned TE_WITH_CMD_EN:1;
	unsigned TE_TIMEOUT_CHK_EN:1;
	unsigned EXT_TE_TIME_VM:4;
	unsigned RGB_PKT_CNT:4;
	unsigned LP_ONLY_VBLK:1; /* bta timeout en */
	unsigned rsv_29:3;
};

enum DSI_PS_TYPE {
	PACKED_PS_16BIT_RGB565 = 0,
	LOOSELY_PS_24BIT_RGB666 = 1,
	PACKED_PS_18BIT_RGB666 = 2,
	PACKED_PS_24BIT_RGB888 = 3,
	PACKED_PS_30BIT_RGB101010 = 4,
	PACKED_COMPRESSION = 5
};

struct DSI_PSCTRL_REG {
	unsigned DSI_PS_WC:15;
	unsigned rsv_15:1;
	unsigned DSI_PS_SEL:3;
	unsigned rsv_19:5;
	unsigned RGB_SWAP:1;
	unsigned BYTE_SWAP:1;
	unsigned CUSTOM_HEADER:6;
};

struct DSI_VSA_NL_REG {
	unsigned VSA_NL:12;
	unsigned rsv_12:20;
};

struct DSI_VBP_NL_REG {
	unsigned VBP_NL:12;
	unsigned rsv_12:20;
};

struct DSI_VFP_NL_REG {
	unsigned VFP_NL:12;
	unsigned rsv_12:20;
};

struct DSI_VACT_NL_REG {
	unsigned VACT_NL:15;
	unsigned rsv_15:17;
};

struct DSI_LFR_CON_REG {
	unsigned LFR_MODE:2;
	unsigned LFR_TYPE:2;
	unsigned LFR_EN:1;
	unsigned LFR_UPDATE:1;
	unsigned LFR_VSE_DIS:1;
	unsigned rsv_7:1;
	unsigned LFR_SKIP_NUM:6;
	unsigned rsv_14:18;
};

struct DSI_LFR_STA_REG {
	unsigned LFR_SKIP_CNT:6;
	unsigned rsv_6:2;
	unsigned LFR_SKIP_STA:1;
	unsigned rsv_9:23;
};

struct DSI_SIZE_CON_REG {
	unsigned DSI_WIDTH:15;
	unsigned rsv_15:1;
	unsigned DSI_HEIGHT:15;
	unsigned rsv_31:1;
};

struct DSI_VFP_EARLY_STOP_REG {
	unsigned VFP_EARLY_STOP_EN:1;
	unsigned VFP_EARLY_STOP_SKIP_VSA_EN:1;
	unsigned rsv_2:2;
	unsigned VFP_UNLIMITED_MODE:1;
	unsigned rsv_5:3;
	unsigned VFP_EARLY_STOP:1;
	unsigned rsv_9:7;
	unsigned VFP_EARLY_STOP_MIN_NL:12;
	unsigned rsv_28:4;
};

struct DSI_HSA_WC_REG {
	unsigned HSA_WC:12;
	unsigned rsv_12:20;
};

struct DSI_HBP_WC_REG {
	unsigned HBP_WC:12;
	unsigned rsv_12:20;
};

struct DSI_HFP_WC_REG {
	unsigned HFP_WC:12;
	unsigned rsv_12:20;
};

struct DSI_BLLP_WC_REG {
	unsigned BLLP_WC:12;
	unsigned rsv_12:20;
};

struct DSI_CMDQ_CTRL_REG {
	unsigned CMDQ_SIZE:8;
	unsigned rsv_8:24;
};

struct DSI_HSTX_CKLP_REG {
	unsigned rsv_0:2;
	unsigned HSTX_CKLP_WC:14;
	unsigned HSTX_CKLP_WC_AUTO:1;
	unsigned rsv_17:15;
};

struct DSI_HSTX_CKLP_WC_AUTO_RESULT_REG {
	unsigned HSTX_CKLP_WC_AUTO_RESULT:16;
	unsigned rsv_16:16;
};

struct DSI_RX_DATA_REG {
	unsigned char byte0;
	unsigned char byte1;
	unsigned char byte2;
	unsigned char byte3;
};

struct DSI_RACK_REG {
	unsigned DSI_RACK:1;
	unsigned DSI_RACK_BYPASS:1;
	unsigned rsv2:30;
};

struct DSI_TRIG_STA_REG {
	unsigned TRIG0:1;	/* remote rst */
	unsigned TRIG1:1;	/* TE */
	unsigned TRIG2:1;	/* ack */
	unsigned TRIG3:1;	/* rsv */
	unsigned RX_ULPS:1;
	unsigned DIRECTION:1;
	unsigned RX_LPDT:1;
	unsigned rsv7:1;
	unsigned RX_POINTER:4;
	unsigned rsv12:20;
};

struct DSI_MEM_CONTI_REG {
	unsigned RWMEM_CONTI:16;
	unsigned rsv16:16;
};


struct DSI_FRM_BC_REG {
	unsigned FRM_BC:21;
	unsigned rsv21:11;
};

struct DSI_3D_CON_REG {
	unsigned _3D_MODE:2;
	unsigned _3D_FMT:2;
	unsigned _3D_VSYNC:1;
	unsigned _3D_LR:1;
	unsigned rsv6:2;
	unsigned _3D_EN:1;
	unsigned rsv9:23;
};

struct DSI_TIME_CON0_REG {
	unsigned UPLS_WAKEUP_PRD:16;
	unsigned SKEWCALL_PRD:16;
};

struct DSI_TIME_CON1_REG {
	unsigned TE_TIMEOUT_PRD:16;
	unsigned PREFETCH_TIME:15;
	unsigned PREFETCH_EN:1;
};

struct DSI_PHY_LCPAT_REG {
	unsigned LC_HSTX_CK_PAT:8;
	unsigned rsv8:24;
};

struct DSI_PHY_LCCON_REG {
	unsigned LC_HS_TX_EN:1;
	unsigned LC_ULPM_EN:1;
	unsigned LC_WAKEUP_EN:1;
	unsigned rsv3:29;
};

struct DSI_PHY_LD0CON_REG {
	unsigned L0_RM_TRIG_EN:1;
	unsigned L0_ULPM_EN:1;
	unsigned L0_WAKEUP_EN:1;
	unsigned Lx_ULPM_AS_L0:1;
	unsigned L0_RX_FILTER_EN:1;
	unsigned rsv3:27;
};

struct DSI_PHY_SYNCON_REG {
	unsigned HS_SYNC_CODE:8;
	unsigned HS_SYNC_CODE2:8;
	unsigned HS_SKEWCAL_PAT:8;
	unsigned HS_DB_SYNC_EN:1;
	unsigned rsv25:7;
};

struct DSI_PHY_TIMCON0_REG {
	unsigned char LPX;
	unsigned char HS_PRPR;
	unsigned char HS_ZERO;
	unsigned char HS_TRAIL;
};

struct DSI_PHY_TIMCON1_REG {
	unsigned char TA_GO;
	unsigned char TA_SURE;
	unsigned char TA_GET;
	unsigned char DA_HS_EXIT;
};

struct DSI_PHY_TIMCON2_REG {
	unsigned char CONT_DET;
	unsigned char DA_HS_SYNC;
	unsigned char CLK_ZERO;
	unsigned char CLK_TRAIL;
};

struct DSI_PHY_TIMCON3_REG {
	unsigned char CLK_HS_PRPR;
	unsigned char CLK_HS_POST;
	unsigned char CLK_HS_EXIT;
	unsigned rsv24:8;
};

struct DSI_VM_CMD_CON_REG {
	unsigned VM_CMD_EN:1;
	unsigned LONG_PKT:1;
	unsigned TIME_SEL:1;
	unsigned TS_VSA_EN:1;
	unsigned TS_VBP_EN:1;
	unsigned TS_VFP_EN:1;
	unsigned rsv6:2;
	unsigned CM_DATA_ID:8;
	unsigned CM_DATA_0:8;
	unsigned CM_DATA_1:8;
};

struct DSI_PHY_TIMCON_REG {
	struct DSI_PHY_TIMCON0_REG CTRL0;
	struct DSI_PHY_TIMCON1_REG CTRL1;
	struct DSI_PHY_TIMCON2_REG CTRL2;
	struct DSI_PHY_TIMCON3_REG CTRL3;
};

struct DSI_CKSM_OUT_REG {
	unsigned PKT_CHECK_SUM:16;
	unsigned ACC_CHECK_SUM:16;
};

struct DSI_STATE_DBG0_REG {
	unsigned DPHY_CTL_STATE_C:9;
	unsigned rsv9:7;
	unsigned DPHY_HS_TX_STATE_C:5;
	unsigned rsv21:11;
};

struct DSI_STATE_DBG1_REG {
	unsigned CTL_STATE_C:15;
	unsigned rsv15:1;
	unsigned HS_TX_STATE_0:5;
	unsigned rsv21:3;
	unsigned ESC_STATE_0:8;
};

struct DSI_STATE_DBG2_REG {
	unsigned RX_ESC_STATE:10;
	unsigned rsv10:6;
	unsigned TA_T2R_STATE:5;
	unsigned rsv21:3;
	unsigned TA_R2T_STATE:5;
	unsigned rsv29:3;
};

struct DSI_STATE_DBG3_REG {
	unsigned CTL_STATE_1:5;
	unsigned rsv5:3;
	unsigned HS_TX_STATE_1:5;
	unsigned rsv13:3;
	unsigned CTL_STATE_2:5;
	unsigned rsv21:3;
	unsigned HS_TX_STATE_2:5;
	unsigned rsv29:3;
};

struct DSI_STATE_DBG4_REG {
	unsigned CTL_STATE_3:5;
	unsigned rsv5:3;
	unsigned HS_TX_STATE_3:5;
	unsigned rsv13:19;
};

struct DSI_STATE_DBG5_REG {
	unsigned TIMER_COUNTER:16;
	unsigned TIMER_BUSY:1;
	unsigned rsv17:11;
	unsigned WAKEUP_STATE:4;
};

struct DSI_STATE_DBG6_REG {
	unsigned CMTRL_STATE:15;
	unsigned rsv15:1;
	unsigned CMDQ_STATE:7;
	unsigned rsv23:9;
};

struct DSI_STATE_DBG7_REG {
	unsigned VMCTL_STATE:11;
	unsigned rsv11:1;
	unsigned VFP_PERIOD:1;
	unsigned VACT_PERIOD:1;
	unsigned VBP_PERIOD:1;
	unsigned VSA_PERIOD:1;
	unsigned rsv16:16;
};

struct DSI_STATE_DBG8_REG {
	unsigned WORD_COUNTER:15;
	unsigned rsv15:1;
	unsigned PREFETCH_CNT:15;
	unsigned DSI_PREFETCH_MUTEX:1;
};

struct DSI_STATE_DBG9_REG {
	unsigned LINE_COUNTER:22;
	unsigned rsv22:10;
};

struct DSI_DEBUG_SEL_REG {
	unsigned DEBUG_OUT_SEL:5;
	unsigned DYNAMIC_MM_CG_CON:3;
	unsigned CHKSUM_REC_EN:1;
	unsigned C2V_START_CON:1;
	unsigned MM_RST_SEL:1;
	unsigned rsv11:1;
	unsigned DYNAMIC_CG_CON:20; /* 12 */
};

struct DSI_STATE_DBG10_REG {
	unsigned LIMIT_W:15;
	unsigned rsv15:1;
	unsigned LIMIT_H:15;
	unsigned rsv31:1;
};

struct DSI_BIST_CON_REG {
	unsigned BIST_MODE:1;
	unsigned BIST_ENABLE:1;
	unsigned BIST_FIX_PATTERN:1;
	unsigned BIST_SPC_PATTERN:1;
	unsigned BIST_HS_FREE:1;
	unsigned rsv_05:1;
	unsigned SELF_PAT_MODE:1;
	unsigned rsv_07:1;
	unsigned BIST_LANE_NUM:4; /* To be confirmed */
	unsigned rsv12:4;
	unsigned BIST_TIMING:8;
	unsigned rsv24:8;
};

struct DSI_SHADOW_DEBUG_REG {
	unsigned FORCE_COMMIT:1;
	unsigned BYPASS_SHADOW:1;
	unsigned READ_WORKING:1;
	unsigned rsv3:29;
};

struct DSI_SHADOW_STA_REG {
	unsigned VACT_UPDATE_ERR:1;
	unsigned VFP_UPDATE_ERR:1;
	unsigned rsv2:30;
};

struct DSI_INPUT_DEBUG_REG {
	unsigned INP_PIXEL_COUNT:13;
	unsigned rsv13:3;
	unsigned INP_LINE_COUNT:13;
	unsigned rsv29:1;
	unsigned INP_END_SYNC_TO_DSI:1;
	unsigned INP_REDUNDANT_REGION:1;
};

struct DSI_CMDQ_REG {
	unsigned rsv0:32;
};

struct DSI_REGS {
	struct DSI_START_REG DSI_START;	/* 0000 */
	struct DSI_STATUS_REG DSI_STA;	/* 0004 */
	struct DSI_INT_ENABLE_REG DSI_INTEN;	/* 0008 */
	struct DSI_INT_STATUS_REG DSI_INTSTA;	/* 000C */
	struct DSI_COM_CTRL_REG DSI_COM_CTRL;	/* 0010 */
	struct DSI_MODE_CTRL_REG DSI_MODE_CTRL;	/* 0014 */
	struct DSI_TXRX_CTRL_REG DSI_TXRX_CTRL;	/* 0018 */
	struct DSI_PSCTRL_REG DSI_PSCTRL;	/* 001C */
	struct DSI_VSA_NL_REG DSI_VSA_NL;	/* 0020 */
	struct DSI_VBP_NL_REG DSI_VBP_NL;	/* 0024 */
	struct DSI_VFP_NL_REG DSI_VFP_NL;	/* 0028 */
	struct DSI_VACT_NL_REG DSI_VACT_NL;	/* 002C */
	struct DSI_LFR_CON_REG DSI_LFR_CON;	/* 0030 */
	struct DSI_LFR_STA_REG DSI_LFR_STA;	/* 0034 */
	struct DSI_SIZE_CON_REG DSI_SIZE_CON;	/* 0038 */
	struct DSI_VFP_EARLY_STOP_REG
		DSI_VFP_EARLY_STOP;	/* 003C */
	UINT32 rsv_3c[4];	/* 0040..004C */
	struct DSI_HSA_WC_REG DSI_HSA_WC;	/* 0050 */
	struct DSI_HBP_WC_REG DSI_HBP_WC;	/* 0054 */
	struct DSI_HFP_WC_REG DSI_HFP_WC;	/* 0058 */
	struct DSI_BLLP_WC_REG DSI_BLLP_WC;	/* 005C */
	struct DSI_CMDQ_CTRL_REG DSI_CMDQ_SIZE;	/* 0060 */
	struct DSI_HSTX_CKLP_REG DSI_HSTX_CKL_WC;	/* 0064 */
	struct DSI_HSTX_CKLP_WC_AUTO_RESULT_REG
		DSI_HSTX_CKL_WC_AUTO_RESULT;	/* 0068 */
	UINT32 rsv_006C[2];	/* 006c..0070 */
	struct DSI_RX_DATA_REG DSI_RX_DATA0;	/* 0074 */
	struct DSI_RX_DATA_REG DSI_RX_DATA1;	/* 0078 */
	struct DSI_RX_DATA_REG DSI_RX_DATA2;	/* 007c */
	struct DSI_RX_DATA_REG DSI_RX_DATA3;	/* 0080 */
	struct DSI_RACK_REG DSI_RACK;	/* 0084 */
	struct DSI_TRIG_STA_REG DSI_TRIG_STA;	/* 0088 */
	UINT32 rsv_008C;	/* 008C */
	struct DSI_MEM_CONTI_REG DSI_MEM_CONTI;	/* 0090 */
	struct DSI_FRM_BC_REG DSI_FRM_BC;	/* 0094 */
	struct DSI_3D_CON_REG DSI_3D_CON;	/* 0098 */
	UINT32 rsv_009C;	/* 009c */
	struct DSI_TIME_CON0_REG DSI_TIME_CON0;	/* 00A0 */
	struct DSI_TIME_CON1_REG DSI_TIME_CON1;	/* 00A4 */

	UINT32 rsv_00A8[22];	/* 0A8..0FC */
	UINT32 DSI_PHY_PCPAT;	/* 00100 */

	struct DSI_PHY_LCCON_REG DSI_PHY_LCCON;	/* 0104 */
	struct DSI_PHY_LD0CON_REG DSI_PHY_LD0CON;	/* 0108 */
	struct DSI_PHY_SYNCON_REG DSI_PHY_SYNCON;	/* 010C */
	struct DSI_PHY_TIMCON0_REG DSI_PHY_TIMECON0;	/* 0110 */
	struct DSI_PHY_TIMCON1_REG DSI_PHY_TIMECON1;	/* 0114 */
	struct DSI_PHY_TIMCON2_REG DSI_PHY_TIMECON2;	/* 0118 */
	struct DSI_PHY_TIMCON3_REG DSI_PHY_TIMECON3;	/* 011C */
	UINT32 rsv_0120[4];	/* 0120..012c */
	struct DSI_VM_CMD_CON_REG DSI_VM_CMD_CON;	/* 0130 */
	UINT32 DSI_VM_CMD_DATA0;	/* 0134 */
	UINT32 DSI_VM_CMD_DATA4;	/* 0138 */
	UINT32 DSI_VM_CMD_DATA8;	/* 013C */
	UINT32 DSI_VM_CMD_DATAC;	/* 0140 */
	struct DSI_CKSM_OUT_REG DSI_CKSM_OUT;	/* 0144 */
	struct DSI_STATE_DBG0_REG DSI_STATE_DBG0;	/* 0148 */
	struct DSI_STATE_DBG1_REG DSI_STATE_DBG1;	/* 014C */
	struct DSI_STATE_DBG2_REG DSI_STATE_DBG2;	/* 0150 */
	struct DSI_STATE_DBG3_REG DSI_STATE_DBG3;	/* 0154 */
	struct DSI_STATE_DBG4_REG DSI_STATE_DBG4;	/* 0158 */
	struct DSI_STATE_DBG5_REG DSI_STATE_DBG5;	/* 015C */
	struct DSI_STATE_DBG6_REG DSI_STATE_DBG6;	/* 0160 */
	struct DSI_STATE_DBG7_REG DSI_STATE_DBG7;	/* 0164 */
	struct DSI_STATE_DBG8_REG DSI_STATE_DBG8;	/* 0168 */
	struct DSI_STATE_DBG9_REG DSI_STATE_DBG9;	/* 016C */
	struct DSI_DEBUG_SEL_REG DSI_DEBUG_SEL;	/* 0170 */
	struct DSI_STATE_DBG10_REG DSI_STATE_DBG10;	/* 0174 */
	UINT32 DSI_BIST_PATTERN;	/* 0178 */
	struct DSI_BIST_CON_REG DSI_BIST_CON;	/* 017C */
	UINT32 DSI_VM_CMD_DATA10;	/* 00180 */
	UINT32 DSI_VM_CMD_DATA14;	/* 00184 */
	UINT32 DSI_VM_CMD_DATA18;	/* 00188 */
	UINT32 DSI_VM_CMD_DATA1C;	/* 0018C */
	struct DSI_SHADOW_DEBUG_REG DSI_SHADOW_DEBUG;	/* 0190 */
	struct DSI_SHADOW_STA_REG DSI_SHADOW_STA;	/* 0194 */
	UINT32 rsv_0198[26];				/* 0198..01fc */
	struct DSI_CMDQ_REG DSI_CMDQ0;			/* 200 */
	struct DSI_CMDQ_REG DSI_CMDQ1;			/* 204 */
	struct DSI_CMDQ_REG DSI_CMDQ2;			/* 208 */
};

/* 0~1 TYPE ,2 BTA,3 HS, 4 CL,5 TE,6~7 RESV,
 * 8~15 DATA_ID,16~23 DATA_0,24~31 DATA_1
 */
struct DSI_CMDQ {
	unsigned char byte0;
	unsigned char byte1;
	unsigned char byte2;
	unsigned char byte3;
};

struct DSI_CMDQ_REGS {
	struct DSI_CMDQ data[128]; /* only support 128 cmdq */
};

struct DSI_VM_CMDQ {
	unsigned char byte0;
	unsigned char byte1;
	unsigned char byte2;
	unsigned char byte3;
};

struct DSI_VM_CMDQ_REGS {
	struct DSI_VM_CMDQ data[4];
};

#endif /* _DDP_REG_DSI_H_ */
