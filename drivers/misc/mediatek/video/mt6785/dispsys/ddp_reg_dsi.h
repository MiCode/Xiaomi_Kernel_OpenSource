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

#define DISP_REG_DSI_MMCLK_STALL_DBG1 (0x1C0UL)

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
	unsigned rsv_1:3;
	unsigned ESC_ENTRY_ERR:1;
	unsigned ESC_SYNC_ERR:1;
	unsigned CTRL_ERR:1;
	unsigned CONTENT_ERR:1;
	unsigned rsv_8:24;
};

#define FLD_RD_RDY_INT_EN			REG_FLD_MSB_LSB(0, 0)
#define FLD_CMD_DONE_INT_EN			REG_FLD_MSB_LSB(1, 1)
#define FLD_TE_RDY_INT_EN			REG_FLD_MSB_LSB(2, 2)
#define FLD_VM_DONE_INT_EN			REG_FLD_MSB_LSB(3, 3)
#define FLD_FRAME_DONE_INT_EN			REG_FLD_MSB_LSB(4, 4)
#define FLD_VM_CMD_DONE_INT_EN			REG_FLD_MSB_LSB(5, 5)
#define FLD_SLEEPOUT_DONE_INT_EN		REG_FLD_MSB_LSB(6, 6)
#define FLD_TE_TIMEOUT_INT_EN			REG_FLD_MSB_LSB(7, 7)
#define FLD_VM_VBP_STR_INT_EN			REG_FLD_MSB_LSB(8, 8)
#define FLD_VM_VACT_STR_INT_EN			REG_FLD_MSB_LSB(9, 9)
#define FLD_VM_VFP_STR_INT_EN			REG_FLD_MSB_LSB(10, 10)
#define FLD_SKEWCAL_DONE_INT_EN			REG_FLD_MSB_LSB(11, 11)
#define FLD_BUFFER_UNDERRUN_INT_EN		REG_FLD_MSB_LSB(12, 12)
#define FLD_BTA_TIMEOUT_INT_EN			REG_FLD_MSB_LSB(13, 13)
#define FLD_INP_UNFINISH_INT_EN			REG_FLD_MSB_LSB(14, 14)
#define FLD_SLEEPIN_ULPS_INT_EN			REG_FLD_MSB_LSB(15, 15)
#define FLD_LPRX_RD_RDY_EVENT_INT_EN		REG_FLD_MSB_LSB(16, 16)
#define FLD_CMD_DONE_EVENT_INT_EN		REG_FLD_MSB_LSB(17, 17)
#define FLD_TE_RDY_EVENT_INT_EN			REG_FLD_MSB_LSB(18, 18)
#define FLD_VM_DONE_EVENT_INT_EN		REG_FLD_MSB_LSB(19, 19)
#define FLD_FRAME_DONE_EVENT_INT_EN		REG_FLD_MSB_LSB(20, 20)
#define FLD_VM_CMD_DONE_EVENT_INT_EN		REG_FLD_MSB_LSB(21, 21)
#define FLD_SLEEPOUT_DONE_EVENT_INT_EN		REG_FLD_MSB_LSB(22, 22)
#define FLD_TE_TIMEOUT_EVENT_INT_EN		REG_FLD_MSB_LSB(23, 23)
#define FLD_VM_VBP_STR_EVENT_INT_EN		REG_FLD_MSB_LSB(24, 24)
#define FLD_VM_VACT_STR_EVENT_INT_EN		REG_FLD_MSB_LSB(25, 25)
#define FLD_VM_VFP_STR_EVENT_INT_EN		REG_FLD_MSB_LSB(26, 26)
#define FLD_SKEWCAL_DONE_EVENT_INT_EN		REG_FLD_MSB_LSB(27, 27)
#define FLD_BUFFER_UNDERRUN_EVENT_INT_EN	REG_FLD_MSB_LSB(28, 28)

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
	unsigned BTA_TIMEOUT_EVENT_EN:1;
	unsigned INP_UNFINISH_EVENT_EN:1;
	unsigned SLEEPIN_ULPS_EVENT_EN:1;
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
	unsigned rsv_5:11;
	unsigned RG_DSI_DUMMY_BYTE_REMOVE_MANUAL_EN:1;
	unsigned rsv_17:3;
	unsigned RG_DSI_DUMMY_BYTE_REMOVE_MANUAL_NUM:2;
	unsigned rsv_22:2;
	unsigned RG_DSI_CM_MODE_WAIT_DATA_EVERY_LINE_EN:1;
	unsigned RG_DSI_CM_MODE_WAIT_DATA_RESERVE_BIT:1;
	unsigned rsv_26:6;
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

#define FLD_VC_NUM			REG_FLD_MSB_LSB(1, 0)
#define FLD_LANE_NUM			REG_FLD_MSB_LSB(5, 2)
#define FLD_DIS_EOT			REG_FLD_MSB_LSB(6, 6)
#define FLD_BLLP_EN			REG_FLD_MSB_LSB(7, 7)
#define FLD_TE_FREERUN			REG_FLD_MSB_LSB(8, 8)
#define FLD_EXT_TE_EN			REG_FLD_MSB_LSB(9, 9)
#define FLD_EXT_TE_EDGE			REG_FLD_MSB_LSB(10, 10)
#define FLD_TE_AUTO_SYNC		REG_FLD_MSB_LSB(11, 11)
#define FLD_MAX_RTN_SIZE		REG_FLD_MSB_LSB(15, 12)
#define FLD_HSTX_CKLP_EN		REG_FLD_MSB_LSB(16, 16)
#define FLD_TYPE1_BTA_SEL		REG_FLD_MSB_LSB(17, 17)
#define FLD_TE_WITH_CMD_EN		REG_FLD_MSB_LSB(18, 18)
#define FLD_TE_TIMEOUT_CHK_EN		REG_FLD_MSB_LSB(19, 19)
#define FLD_EXT_TE_TIME_VM		REG_FLD_MSB_LSB(23, 20)
#define FLD_RGB_PKT_CNT			REG_FLD_MSB_LSB(27, 24)
#define FLD_LP_ONLY_VBLK		REG_FLD_MSB_LSB(28, 28)

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
	unsigned BTA_TIMEOUT_CHK_EN:1;
	unsigned rsv_30:2;
};

enum DSI_PS_TYPE {
	PACKED_PS_16BIT_RGB565 = 0,
	LOOSELY_PS_18BIT_RGB666 = 1,
	PACKED_PS_24BIT_RGB888 = 2,
	PACKED_PS_18BIT_RGB666 = 3
};

#define FLD_DSI_PS_WC			REG_FLD_MSB_LSB(14, 0)
#define FLD_DSI_PS_SEL			REG_FLD_MSB_LSB(18, 16)

struct DSI_PSCTRL_REG {
	unsigned DSI_PS_WC:15;
	unsigned rsv_15:1;
	unsigned DSI_PS_SEL:3;
	unsigned rsv_19:1;
	unsigned RG_DSI_DCS_30BIT_FORMAT:1;
	unsigned rsv_21:3;
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
	unsigned VFP_EARLY_SOP_EN:1;
	unsigned VFP_EARLY_SOP_SKIP_VSA_EN:1;
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
	unsigned rsv_2:30;
};

struct DSI_TRIG_STA_REG {
	unsigned TRIG0:1;	/* remote rst */
	unsigned TRIG1:1;	/* TE */
	unsigned TRIG2:1;	/* ack */
	unsigned TRIG3:1;	/* rsv */
	unsigned RX_ULPS:1;
	unsigned DIRECTION:1;
	unsigned RX_LPDT:1;
	unsigned rsv_7:1;
	unsigned RX_POINTER:4;
	unsigned rsv_12:20;
};

struct DSI_MEM_CONTI_REG {
	unsigned RWMEM_CONTI:16;
	unsigned rsv_16:16;
};

struct DSI_FRM_BC_REG {
	unsigned FRM_BC:21;
	unsigned rsv_21:11;
};

struct DSI_3D_CON_REG {
	unsigned _3D_MODE:2;
	unsigned _3D_FMT:2;
	unsigned _3D_VSYNC:1;
	unsigned _3D_LR:1;
	unsigned rsv_6:2;
	unsigned _3D_EN:1;
	unsigned rsv_9:23;
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

struct DSI_TIME_CON2_REG {
	unsigned BTA_TIMEOUT_PRD:16;
	unsigned rsv_16:16;
};

struct DSI_PHY_LCPAT_REG {
	unsigned LC_HSTX_CK_PAT:8;
	unsigned rsv_8:24;
};

struct DSI_PHY_LCCON_REG {
	unsigned LC_HS_TX_EN:1;
	unsigned LC_ULPM_EN:1;
	unsigned LC_WAKEUP_EN:1;
	unsigned TRAIL_FIX:1;
	unsigned rsv_4:4;
	unsigned EARLY_DRDY:5;
	unsigned rsv_13:3;
	unsigned EARLY_HS_POE:5;
	unsigned rsv_21:11;
};

struct DSI_PHY_LD0CON_REG {
	unsigned L0_RM_TRIG_EN:1;
	unsigned L0_ULPM_EN:1;
	unsigned L0_WAKEUP_EN:1;
	unsigned Lx_ULPM_AS_L0:1;
	unsigned L0_RX_FILTER_EN:1;
	unsigned rsv_5:27;
};

struct DSI_PHY_SYNCON_REG {
	unsigned HS_SYNC_CODE:8;
	unsigned HS_SYNC_CODE2:8;
	unsigned HS_SKEWCAL_PAT:8;
	unsigned HS_DB_SYNC_EN:1;
	unsigned rsv_25:7;
};

#define FLD_LPX			REG_FLD_MSB_LSB(7, 0)
#define FLD_HS_PRPR		REG_FLD_MSB_LSB(15, 8)
#define FLD_HS_ZERO		REG_FLD_MSB_LSB(23, 16)
#define FLD_HS_TRAIL		REG_FLD_MSB_LSB(31, 24)

struct DSI_PHY_TIMCON0_REG {
	unsigned char LPX;
	unsigned char HS_PRPR;
	unsigned char HS_ZERO;
	unsigned char HS_TRAIL;
};

#define FLD_TA_GO		REG_FLD_MSB_LSB(7, 0)
#define FLD_TA_SURE		REG_FLD_MSB_LSB(15, 8)
#define FLD_TA_GET		REG_FLD_MSB_LSB(23, 16)
#define FLD_DA_HS_EXIT		REG_FLD_MSB_LSB(31, 24)

struct DSI_PHY_TIMCON1_REG {
	unsigned char TA_GO;
	unsigned char TA_SURE;
	unsigned char TA_GET;
	unsigned char DA_HS_EXIT;
};

#define FLD_CONT_DET		REG_FLD_MSB_LSB(7, 0)
#define FLD_DA_HS_SYNC		REG_FLD_MSB_LSB(15, 8)
#define FLD_CLK_ZERO		REG_FLD_MSB_LSB(23, 16)
#define FLD_CLK_TRAIL		REG_FLD_MSB_LSB(31, 24)

struct DSI_PHY_TIMCON2_REG {
	unsigned char CONT_DET;
	unsigned char DA_HS_SYNC;
	unsigned char CLK_ZERO;
	unsigned char CLK_TRAIL;
};

#define FLD_CLK_HS_PRPR		REG_FLD_MSB_LSB(7, 0)
#define FLD_CLK_HS_POST		REG_FLD_MSB_LSB(15, 8)
#define FLD_CLK_HS_EXIT		REG_FLD_MSB_LSB(23, 16)

struct DSI_PHY_TIMCON3_REG {
	unsigned char CLK_HS_PRPR;
	unsigned char CLK_HS_POST;
	unsigned char CLK_HS_EXIT;
	unsigned rsv_24:8;
};

struct DSI_CPHY_CON0_REG {
	unsigned CPHY_EN:1;
	unsigned SETTLE_SKIP_EN:1;
	unsigned PROGSEQ_SKIP_EN:1;
	unsigned rsv_3:1;
	unsigned CPHY_PROGSEQMSB:10;
	unsigned RG_CPHY_LP_RX_B_IGNORE_EN:1;
	unsigned rsv_15:1;
	unsigned CPHY_INIT_STATE:9;
	unsigned rsv_25:3;
	unsigned CPHY_CONTI_CLK:4;
};

struct DSI_CPHY_CON1_REG {
	unsigned CPHY_PROGSEQLSB:32;
};

struct DSI_CPHY_DBG0_REG {
	unsigned CPHYHS_STATE_DA0:9;
	unsigned rsv_9:7;
	unsigned CPHYHS_STATE_DA1:9;
	unsigned rsv_25:7;
};

struct DSI_CPHY_DBG1_REG {
	unsigned CPHYHS_STATE_DA2:9;
	unsigned rsv_9:23;
};

struct DSI_VM_CMD_CON_REG {
	unsigned VM_CMD_EN:1;
	unsigned LONG_PKT:1;
	unsigned TIME_SEL:1;
	unsigned TS_VSA_EN:1;
	unsigned TS_VBP_EN:1;
	unsigned TS_VFP_EN:1;
	unsigned rsv_6:2;
	unsigned CM_DATA_ID:8;
	unsigned CM_DATA_0:8;
	unsigned CM_DATA_1:8;
};

struct DSI_CKSM_OUT_REG {
	unsigned PKT_CHECK_SUM:16;
	unsigned ACC_CHECK_SUM:16;
};

struct DSI_STATE_DBG0_REG {
	unsigned DPHY_CTL_STATE_C:9;
	unsigned rsv_9:7;
	unsigned DPHY_HS_TX_STATE_C:5;
	unsigned rsv_21:11;
};

struct DSI_STATE_DBG1_REG {
	unsigned CTL_STATE_C:15;
	unsigned rsv_15:1;
	unsigned HS_TX_STATE_0:5;
	unsigned rsv_21:3;
	unsigned ESC_STATE_0:8;
};

struct DSI_STATE_DBG2_REG {
	unsigned RX_ESC_STATE:10;
	unsigned rsv_10:6;
	unsigned TA_T2R_STATE:5;
	unsigned rsv_21:3;
	unsigned TA_R2T_STATE:5;
	unsigned rsv_29:3;
};

struct DSI_STATE_DBG3_REG {
	unsigned CTL_STATE_1:5;
	unsigned rsv_5:3;
	unsigned HS_TX_STATE_1:5;
	unsigned rsv_13:3;
	unsigned CTL_STATE_2:5;
	unsigned rsv_21:3;
	unsigned HS_TX_STATE_2:5;
	unsigned rsv_29:3;
};

struct DSI_STATE_DBG4_REG {
	unsigned CTL_STATE_3:5;
	unsigned rsv_5:3;
	unsigned HS_TX_STATE_3:5;
	unsigned rsv_13:19;
};

struct DSI_STATE_DBG5_REG {
	unsigned TIMER_COUNTER:16;
	unsigned TIMER_BUSY:1;
	unsigned rsv_17:11;
	unsigned WAKEUP_STATE:4;
};

struct DSI_STATE_DBG6_REG {
	unsigned CMTRL_STATE:15;
	unsigned rsv_15:1;
	unsigned CMDQ_STATE:7;
	unsigned rsv_23:9;
};

struct DSI_STATE_DBG7_REG {
	unsigned VMCTL_STATE:11;
	unsigned rsv_11:1;
	unsigned VFP_PERIOD:1;
	unsigned VACT_PERIOD:1;
	unsigned VBP_PERIOD:1;
	unsigned VSA_PERIOD:1;
	unsigned EVEN_FIELD:1;
	unsigned rsv_17:15;
};

struct DSI_STATE_DBG8_REG {
	unsigned WORD_COUNTER:14;
	unsigned rsv_14:2;
	unsigned PREFETCH_CNT:15;
	unsigned DSI_PREFETCH_MUTEX:1;
};

struct DSI_STATE_DBG9_REG {
	unsigned LINE_COUNTER:22;
	unsigned rsv_22:10;
};

struct DSI_DEBUG_SEL_REG {
	unsigned DEBUG_OUT_SEL:5;
	unsigned rsv_5:3;
	unsigned CHKSUM_REC_EN:1;
	unsigned C2V_START_CON:1;
	unsigned MM_RST_SEL:1;
	unsigned rsv_11:21;
};

struct DSI_STATE_DBG10_REG {
	unsigned LIMIT_W:15;
	unsigned rsv_15:1;
	unsigned LIMIT_H:15;
	unsigned rsv_31:1;
};

struct DSI_BIST_CON_REG {
	unsigned rsv_0:6;
	unsigned SELF_PAT_PRE_MODE:1;
	unsigned SELF_PAT_POST_MODE:1;
	unsigned SELF_PAT_SEL:4;
	unsigned rsv_12:20;
};

struct DSI_SHADOW_DEBUG_REG {
	unsigned FORCE_COMMIT:1;
	unsigned BYPASS_SHADOW:1;
	unsigned READ_WORKING:1;
	unsigned rsv_3:29;
};

struct DSI_SHADOW_STA_REG {
	unsigned VACT_UPDATE_ERR:1;
	unsigned VFP_UPDATE_ERR:1;
	unsigned rsv_2:30;
};

struct DSI_CG_CONFIG_REG {
	unsigned DYNAMIC_CG_CON:24;
	unsigned DYNAMIC_MM_CG_CON:8;
};

struct DSI_MMCLK_STALL_DBG1_REG {
	unsigned MMCLK_STALL_CHECKER_EN:1;
	unsigned MMCLK_STALL_CHECKER_MODE:1;
	unsigned rsv_2:6;
	unsigned MMCLK_STALL_CHECKER_THRESHOLD:7;
	unsigned rsv_15:17;
};

struct DSI_MMCLK_STALL_DBG2_REG {
	unsigned ASYNC_FIFO_RD_PTR_BINARY_MM_DOMAIN:6;
	unsigned rsv_6:2;
	unsigned ASYNC_FIFO_WR_PTR_BINARY_MM_DOMAIN:6;
	unsigned rsv_14:2;
	unsigned TIMES_OF_MMCLK_STALL_END:2;
	unsigned rsv_18:2;
	unsigned TIMES_OF_MMCLK_STALL_START:2;
	unsigned rsv_22:2;
	unsigned TIMES_OF_MMCLK_STALL_BUFFERUNDER_RUN:2;
	unsigned rsv_26:6;
};

struct DSI_MMCLK_STALL_DBG3_REG {
	unsigned ASYNC_FIFO_RD_PTR_BINARY_DSI_DOMAIN_AT_STALL_START:6;
	unsigned rsv_6:2;
	unsigned ASYNC_FIFO_WR_PTR_BINARY_DSI_DOMAIN_AT_STALL_START:6;
	unsigned rsv_14:2;
	unsigned ASYNC_FIFO_RD_PTR_BINARY_DSI_DOMAIN_AT_STALL_END:6;
	unsigned rsv_22:2;
	unsigned ASYNC_FIFO_WR_PTR_BINARY_DSI_DOMAIN_AT_STALL_END:6;
	unsigned rsv_30:2;
};

struct DSI_MMCLK_STALL_DBG4_REG {
	unsigned ASYNC_FIFO_RD_PTR_BINARY_DSI_DOMAIN:6;
	unsigned rsv_6:2;
	unsigned ASYNC_FIFO_WR_PTR_BINARY_DSI_DOMAIN:6;
	unsigned rsv_14:2;
	unsigned COUNTER_AT_MMCLK_BUFFER_RUN:7;
	unsigned rsv_23:1;
	unsigned COUNTER_AT_MMCLK_STALL_END:7;
	unsigned rsv_30:1;
};

struct DSI_INPUT_SETTING_REG {
	unsigned INP_REDUNDANT_PROCESSING_EN:1;
	unsigned rsv_1:31;
};

struct DSI_INPUT_DEBUG_REG {
	unsigned INP_PIXEL_COUNT:13;
	unsigned rsv_13:3;
	unsigned INP_LINE_COUNT:13;
	unsigned rsv_29:1;
	unsigned INP_END_SYNC_TO_DSI:1;
	unsigned INP_REDUNDANT_REGION:1;
};

struct DSI_REGS {
	struct DSI_START_REG DSI_START;			/* 0x000 */
	struct DSI_STATUS_REG DSI_STA;			/* 0x004 */
	struct DSI_INT_ENABLE_REG DSI_INTEN;		/* 0x008 */
	struct DSI_INT_STATUS_REG DSI_INTSTA;		/* 0x00C */
	struct DSI_COM_CTRL_REG DSI_COM_CTRL;		/* 0x010 */
	struct DSI_MODE_CTRL_REG DSI_MODE_CTRL;		/* 0x014 */
	struct DSI_TXRX_CTRL_REG DSI_TXRX_CTRL;		/* 0x018 */
	struct DSI_PSCTRL_REG DSI_PSCTRL;		/* 0x01C */
	struct DSI_VSA_NL_REG DSI_VSA_NL;		/* 0x020 */
	struct DSI_VBP_NL_REG DSI_VBP_NL;		/* 0x024 */
	struct DSI_VFP_NL_REG DSI_VFP_NL;		/* 0x028 */
	struct DSI_VACT_NL_REG DSI_VACT_NL;		/* 0x02C */
	struct DSI_LFR_CON_REG DSI_LFR_CON;		/* 0x030 */
	struct DSI_LFR_STA_REG DSI_LFR_STA;		/* 0x034 */
	struct DSI_SIZE_CON_REG DSI_SIZE_CON;		/* 0x038 */
	struct DSI_VFP_EARLY_STOP_REG DSI_VFP_EARLY_STOP; /* 0x3C */
	uint32_t rsv_40[4];				/* 0x040..0x04C */
	struct DSI_HSA_WC_REG DSI_HSA_WC;		/* 0x050 */
	struct DSI_HBP_WC_REG DSI_HBP_WC;		/* 0x054 */
	struct DSI_HFP_WC_REG DSI_HFP_WC;		/* 0x058 */
	struct DSI_BLLP_WC_REG DSI_BLLP_WC;		/* 0x05C */
	struct DSI_CMDQ_CTRL_REG DSI_CMDQ_SIZE;		/* 0x060 */
	struct DSI_HSTX_CKLP_REG DSI_HSTX_CKL_WC;	/* 0x064 */
	struct DSI_HSTX_CKLP_WC_AUTO_RESULT_REG
		DSI_HSTX_CKL_WC_AUTO_RESULT;		/* 0x068 */
	uint32_t rsv_006C[2];				/* 0x06c..0070 */
	struct DSI_RX_DATA_REG DSI_RX_DATA0;		/* 0x074 */
	struct DSI_RX_DATA_REG DSI_RX_DATA1;		/* 0x078 */
	struct DSI_RX_DATA_REG DSI_RX_DATA2;		/* 0x07c */
	struct DSI_RX_DATA_REG DSI_RX_DATA3;		/* 0x080 */
	struct DSI_RACK_REG DSI_RACK;			/* 0x084 */
	struct DSI_TRIG_STA_REG DSI_TRIG_STA;		/* 0x088 */
	uint32_t rsv_008C;				/* 0x08C */
	struct DSI_MEM_CONTI_REG DSI_MEM_CONTI;		/* 0x090 */
	struct DSI_FRM_BC_REG DSI_FRM_BC;		/* 0x094 */
	struct DSI_3D_CON_REG DSI_3D_CON;		/* 0x098 */
	uint32_t rsv_009C;				/* 0x09c */
	struct DSI_TIME_CON0_REG DSI_TIME_CON0;		/* 0x0A0 */
	struct DSI_TIME_CON1_REG DSI_TIME_CON1;		/* 0x0A4 */
	struct DSI_TIME_CON2_REG DSI_TIME_CON2;		/* 0x0A8 */
	uint32_t rsv_00AC[21];				/* 0x0AC..0x0FC */
	struct DSI_PHY_LCPAT_REG DSI_PHY_LCPAT;		/* 0x100 */
	struct DSI_PHY_LCCON_REG DSI_PHY_LCCON;		/* 0x104 */
	struct DSI_PHY_LD0CON_REG DSI_PHY_LD0CON;	/* 0x108 */
	struct DSI_PHY_SYNCON_REG DSI_PHY_SYNCON;	/* 0x10C */
	struct DSI_PHY_TIMCON0_REG DSI_PHY_TIMECON0;	/* 0x110 */
	struct DSI_PHY_TIMCON1_REG DSI_PHY_TIMECON1;	/* 0x114 */
	struct DSI_PHY_TIMCON2_REG DSI_PHY_TIMECON2;	/* 0x118 */
	struct DSI_PHY_TIMCON3_REG DSI_PHY_TIMECON3;	/* 0x11C */
	struct DSI_CPHY_CON0_REG DSI_CPHY_CON0;		/* 0x120 */
	struct DSI_CPHY_CON1_REG DSI_CPHY_CON1;		/* 0x124 */
	struct DSI_CPHY_DBG0_REG DSI_CPHY_DBG0;		/* 0x128 */
	struct DSI_CPHY_DBG1_REG DSI_CPHY_DBG1;		/* 0x12C */
	struct DSI_VM_CMD_CON_REG DSI_VM_CMD_CON;	/* 0x130 */
	uint32_t DSI_VM_CMD_DATA0;			/* 0x134 */
	uint32_t DSI_VM_CMD_DATA4;			/* 0x138 */
	uint32_t DSI_VM_CMD_DATA8;			/* 0x13C */
	uint32_t DSI_VM_CMD_DATAC;			/* 0x140 */
	struct DSI_CKSM_OUT_REG DSI_CKSM_OUT;		/* 0x144 */
	struct DSI_STATE_DBG0_REG DSI_STATE_DBG0;	/* 0x148 */
	struct DSI_STATE_DBG1_REG DSI_STATE_DBG1;	/* 0x14C */
	struct DSI_STATE_DBG2_REG DSI_STATE_DBG2;	/* 0x150 */
	struct DSI_STATE_DBG3_REG DSI_STATE_DBG3;	/* 0x154 */
	struct DSI_STATE_DBG4_REG DSI_STATE_DBG4;	/* 0x158 */
	struct DSI_STATE_DBG5_REG DSI_STATE_DBG5;	/* 0x15C */
	struct DSI_STATE_DBG6_REG DSI_STATE_DBG6;	/* 0x160 */
	struct DSI_STATE_DBG7_REG DSI_STATE_DBG7;	/* 0x164 */
	struct DSI_STATE_DBG8_REG DSI_STATE_DBG8;	/* 0x168 */
	struct DSI_STATE_DBG9_REG DSI_STATE_DBG9;	/* 0x16C */
	struct DSI_DEBUG_SEL_REG DSI_DEBUG_SEL;		/* 0x170 */
	struct DSI_STATE_DBG10_REG DSI_STATE_DBG10;	/* 0x174 */
	uint32_t DSI_BIST_PATTERN;			/* 0x178 */
	struct DSI_BIST_CON_REG DSI_BIST_CON;		/* 0x17C */
	uint32_t DSI_VM_CMD_DATA10;			/* 0x180 */
	uint32_t DSI_VM_CMD_DATA14;			/* 0x184 */
	uint32_t DSI_VM_CMD_DATA18;			/* 0x188 */
	uint32_t DSI_VM_CMD_DATA1C;			/* 0x18C */
	struct DSI_SHADOW_DEBUG_REG DSI_SHADOW_DEBUG;	/* 0x190 */
	struct DSI_SHADOW_STA_REG DSI_SHADOW_STA;	/* 0x194 */
	struct DSI_CG_CONFIG_REG DSI_CG_CONFIG;		/* 0x198 */
	uint32_t rsv_019C;				/* 0x19C */
	uint32_t DSI_VM_CMD_DATA20;			/* 0x1A0 */
	uint32_t DSI_VM_CMD_DATA24;			/* 0x1A4 */
	uint32_t DSI_VM_CMD_DATA28;			/* 0x1A8 */
	uint32_t DSI_VM_CMD_DATA2C;			/* 0x1AC */
	uint32_t DSI_VM_CMD_DATA30;			/* 0x1B0 */
	uint32_t DSI_VM_CMD_DATA34;			/* 0x1B4 */
	uint32_t DSI_VM_CMD_DATA38;			/* 0x1B8 */
	uint32_t DSI_VM_CMD_DATA3C;			/* 0x1BC */
	struct DSI_MMCLK_STALL_DBG1_REG DSI_MMCLK_STALL_DBG1;	/* 0x1C0 */
	struct DSI_MMCLK_STALL_DBG2_REG DSI_MMCLK_STALL_DBG2;	/* 0x1C4 */
	struct DSI_MMCLK_STALL_DBG3_REG DSI_MMCLK_STALL_DBG3;	/* 0x1C8 */
	struct DSI_MMCLK_STALL_DBG4_REG DSI_MMCLK_STALL_DBG4;	/* 0x1CC */
	struct DSI_INPUT_SETTING_REG DSI_INPUT_SETTING;		/* 0x1D0 */
	struct DSI_INPUT_DEBUG_REG DSI_INPUT_DEBUG;		/* 0x1D4 */
};

/**
 * 0~1 TYPE, 2 BTA, 3 HS, 4 CL, 5 TE, 6~7 RESV,
 * 8~15 DATA_ID, 16~23 DATA_0, 24~31 DATA_1
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
