/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _CAMSV_REGS_H
#define _CAMSV_REGS_H

/*  */
#define REG_CAMSV_TOP_FBC_CNT_SET				0x0014

union CAMSV_TOP_FBC_CNT_SET {
	struct {
		unsigned int  RCNT_INC1					:	1;
		unsigned int  RCNT_INC2					:	1;
		unsigned int  rsv_2						:	30;
	} Bits;
	unsigned int Raw;
};
/* Module EN */
#define REG_CAMSV_MODULE_EN						0x0040
union CAMSV_MODULE_EN {
	struct {
		unsigned int  TG_EN						:	1;
		unsigned int  rsv_1						:	1;
		unsigned int  PAK_EN					:	1;
		unsigned int  PAK_SEL					:	1;
		unsigned int  IMGO_EN					:	1;
		unsigned int  rsv_5						:	1;
		unsigned int  UFE_EN					:	1;
		unsigned int  QBN_EN					:	1;
		unsigned int  rsv_8						:	8;
		unsigned int  DOWN_SAMPLE_PERIOD		:	8;
		unsigned int  DOWN_SAMPLE_EN			:	1;
		unsigned int  DB_LOAD_HOLD				:	1;
		unsigned int  DB_LOAD_FORCE				:	1;
		unsigned int  rsv_27					:	1;
		unsigned int  DB_LOAD_SRC				:	2;
		unsigned int  DB_EN						:	1;
		unsigned int  DB_LOCK					:	1;
	} Bits;
	unsigned int Raw;
};


/* FMT SEL */
#define REG_CAMSV_FMT_SEL						0x0044
union CAMSV_FMT_SEL {
	struct {
		unsigned int  TG1_FMT					:	3;
		unsigned int  rsv_3						:	2;
		unsigned int  TG1_SW					:	2;
		unsigned int  rsv_7						:	1;
		unsigned int  LP_MODE					:	1;
		unsigned int  HLR_MODE					:	1;
		unsigned int  rsv_10					:	22;
	} Bits;
	unsigned int Raw;
};

/* INT EN */
#define REG_CAMSV_INT_EN						0x0048
union CAMSV_INT_EN {
	struct {
		unsigned int  VS_INT_EN					:	1;
		unsigned int  TG_INT1_EN				:	1;
		unsigned int  TG_INT2_EN				:	1;
		unsigned int  EXPDON1_INT_EN			:	1;
		unsigned int  TG_ERR_INT_EN				:	1;
		unsigned int  TG_GBERR_INT_EN			:	1;
		unsigned int  TG_SOF_INT_EN				:	1;
		unsigned int  TG_SOF_WAIT_INT_EN		:	1;
		unsigned int  TG_SOF_DROP_INT_EN		:	1;
		unsigned int  VS_INT_ORG_EN				:	1;
		unsigned int  DB_LOAD_ERR_EN			:	1;
		unsigned int  PASS1_DON_INT_EN			:	1;
		unsigned int  SW_PASS1_DON_INT_EN		:	1;
		unsigned int  SUB_PASS1_DON_INT_EN		:	1;
		unsigned int  rsv_14					:	1;
		unsigned int  UFEO_OVERR_INT_EN			:	1;
		unsigned int  DMA_ERR_INT_EN			:	1;
		unsigned int  IMGO_OVERR_INT_EN			:	1;
		unsigned int  UFEO_DROP_INT_EN			:	1;
		unsigned int  IMGO_DROP_INT_EN			:	1;
		unsigned int  IMGO_DONE_INT_EN			:	1;
		unsigned int  UFEO_DONE_INT_EN			:	1;
		unsigned int  TG_INT3_EN				:	1;
		unsigned int  TG_INT4_EN				:	1;
		unsigned int  SW_ENQUE_ERR_EN			:	1;
		unsigned int  rsv_25					:	6;
		unsigned int  INT_WCLR_EN				:	1;
	} Bits;
	unsigned int Raw;
};


/* INT Status */
#define REG_CAMSV_INT_STATUS					0x004C
#define CAMSV_INT_VS_ST							BIT(0)
#define CAMSV_INT_TG_ST1						BIT(1)
#define CAMSV_INT_TG_ST2						BIT(2)
#define CAMSV_INT_EXPDON1_ST					BIT(3)
#define CAMSV_INT_TG_ERR_ST						BIT(4)
#define CAMSV_INT_TG_GBERR_ST					BIT(5)
#define CAMSV_INT_TG_SOF_INT_ST					BIT(6)
#define CAMSV_INT_DB_LOAD_ERR_ST				BIT(10)
#define CAMSV_INT_PASS1_DON_ST					BIT(11)
#define CAMSV_INT_SW_PASS1_DON_ST				BIT(12)
#define CAMSV_INT_SUB_PASS1_DON_ST				BIT(13)
#define CAMSV_INT_UFEO_OVERR_ST					BIT(15)
#define CAMSV_INT_DMA_ERR_ST					BIT(16)
#define CAMSV_INT_IMGO_OVERR_ST					BIT(17)
#define CAMSV_INT_UFEO_DROP_ST					BIT(18)
#define CAMSV_INT_IMGO_DROP_ST					BIT(19)
#define CAMSV_INT_IMGO_DONE_ST					BIT(20)
#define CAMSV_INT_UFEO_DONE_ST					BIT(21)
#define CAMSV_INT_TG_ST3						BIT(22)
#define CAMSV_INT_TG_ST4						BIT(23)
#define CAMSV_INT_SW_ENQUE_ERR					BIT(24)
union CAMSV_INT_STATUS {
	struct {
		unsigned int  VS_ST						:	1;
		unsigned int  TG_ST1					:	1;
		unsigned int  TG_ST2					:	1;
		unsigned int  EXPDON1_ST				:	1;
		unsigned int  SV_TG_ERR_ST				:	1;
		unsigned int  SV_TG_GBERR_ST			:	1;
		unsigned int  TG_SOF_INT_ST				:	1;
		unsigned int  TG_SOF_WAIT_ST			:	1;
		unsigned int  TG_SOF_DROP_ST			:	1;
		unsigned int  VS_ORG_ST					:	1;
		unsigned int  DB_LOAD_ERR_ST			:	1;
		unsigned int  PASS1_DON_ST				:	1;
		unsigned int  SV_SW_PASS1_DON_ST		:	1;
		unsigned int  SUB_PASS1_DON_ST			:	1;
		unsigned int  rsv_14					:	1;
		unsigned int  SV_UFEO_OVERR_ST			:	1;
		unsigned int  SV_DMA_ERR_ST				:	1;
		unsigned int  SV_IMGO_OVERR_ST			:	1;
		unsigned int  SV_UFEO_DROP_ST			:	1;
		unsigned int  SV_IMGO_DROP_ST			:	1;
		unsigned int  SV_IMGO_DONE_ST			:	1;
		unsigned int  SV_UFEO_DONE_ST			:	1;
		unsigned int  TG_ST3					:	1;
		unsigned int  TG_ST4					:	1;
		unsigned int  SW_ENQUE_ERR				:	1;
		unsigned int  rsv_20					:	7;
	} Bits;
	unsigned int Raw;
};

/* SW CTL */
#define REG_CAMSV_SW_CTL						0x0050

/* IMGO FBC */
#define REG_CAMSV_IMGO_FBC						0x005C
union CAMSV_IMGO_FBC {
	struct {
		unsigned int  rsv_0						:	1;
		unsigned int  rsv_1						:	1;
		unsigned int  rsv_2						:	1;
		unsigned int  rsv_3						:	1;
		unsigned int  IMGO_FIFO_FULL_EN			:	1;
		unsigned int  UFEO_FIFO_FULL_EN			:	1;
		unsigned int  IMGO_FBC_FIFO_FULL_SEL	:	1;
		unsigned int  UFEO_FBC_FIFO_FULL_SEL	:	1;
		unsigned int  rsv_8						:	24;
	} Bits;
	unsigned int Raw;
};

/* CLK EN */
#define REG_CAMSV_CLK_EN						0x0060
union CAMSV_CLK_EN {
	struct {
		unsigned int  TG_DP_CK_EN				:	1;
		unsigned int  QBN_DP_CK_EN				:	1;
		unsigned int  PAK_DP_CK_EN				:	1;
		unsigned int  rsv_3						:	1;
		unsigned int  UFEO_DP_CK_EN				:	1;
		unsigned int  rsv_5						:	10;
		unsigned int  IMGO_DP_CK_EN				:	1;
		unsigned int  rsv_16					:	16;
	} Bits;
	unsigned int Raw;
};

/* DCIF Set */
#define REG_CAMSV_DCIF_SET						0x0074
union CAMSV_DCIF_SET {
	struct {
		unsigned int  rsv_0						:	6;
		unsigned int  POSTPONE_PASS1_DONE		:	1;
		unsigned int  MASK_DB_LOAD				:	1;
		unsigned int  ENABLE_OUTPUT_CQ_START_SIGNAL	:	1;
		unsigned int  ENABLE_REFERENCE_COUPLE_CAMSV_PASS1_ENABLE	:	1;
		unsigned int  SEL_THE_SOF_SIGNAL_FROM	:	2;
		unsigned int  rsv_12					:	3;
		unsigned int  FOR_DCIF_SUBSAMPLE_EN		:	1;
		unsigned int  FOR_DCIF_SUBSAMPLE_NUMBER	:	8;
		unsigned int  FOR_DCIF_CQ_START_TIMING	:	8;
	} Bits;
	unsigned int Raw;
};

/* SUB CTRL */
#define REG_CAMSV_SUB_CTRL						0x0078
union CAMSV_SUB_CTRL {
	struct {
		unsigned int  CENTRAL_SUB_EN			:	1;
		unsigned int  rsv_1						:	31;
	} Bits;
	unsigned int Raw;
};

/* PAK */
#define REG_CAMSV_PAK							0x007C
union CAMSV_PAK {
	struct {
		unsigned int  PAK_MODE					:	8;
		unsigned int  PAK_DBL_MODE				:	2;
		unsigned int  rsv_10					:	22;
	} Bits;
	unsigned int Raw;
};

/* MISC */
#define REG_CAMSV_MISC							0x0088
union CAMSV_MISC {
	struct {
		unsigned int  VF_SRC					:	1;
		unsigned int  rsv_1						:	31;
	} Bits;
	unsigned int Raw;
};

/* QBIN */
#define REG_CAMSV_QBN_SET						0x009C
union CAMSV_CAMSV_QBN_SET {
	struct {
		unsigned int  CSR_QBN_CAM_PIX_BUS		:	2;
		unsigned int  rsv_1						:	30;
	} Bits;
	unsigned int Raw;
};

/* TG SEN Mode */
#define REG_CAMSV_TG_SEN_MODE					0x0100
#define CAMSV_TG_SEN_MODE_CMOS_EN				BIT(0)
#define CAMSV_TG_SEN_MODE_CMOS_RDY_SEL			BIT(14)

union CAMSV_TG_SEN_MODE {
	struct {
		unsigned int  CMOS_EN					:	1;
		unsigned int  rsv_1						:	1;
		unsigned int  SOT_MODE					:	1;
		unsigned int  SOT_CLR_MODE				:	1;
		unsigned int  DBL_DATA_BUS				:	2;
		unsigned int  rsv_6						:	2;
		unsigned int  SOF_SRC					:	2;
		unsigned int  EOF_SRC					:	2;
		unsigned int  PXL_CNT_RST_SRC			:	1;
		unsigned int  rsv_13					:	1;
		unsigned int  CMOS_RDY_SEL				:	1;
		unsigned int  FIFO_FULL_CTL_EN			:	1;
		unsigned int  TIME_STP_EN				:	1;
		unsigned int  VS_SUB_EN					:	1;
		unsigned int  SOF_SUB_EN				:	1;
		unsigned int  VSYNC_INT_POL				:	1;
		unsigned int  EOF_ALS_RDY_EN			:	1;
		unsigned int  rsv_21					:	1;
		unsigned int  STAGGER_EN				:	1;
		unsigned int  HDR_EN					:	1;
		unsigned int  HDR_SEL					:	1;
		unsigned int  SOT_DLY_EN				:	1;
		unsigned int  VS_IGNORE_STALL_EN		:	1;
		unsigned int  LINE_OK_CHECK				:	1;
		unsigned int  SOF_SUB_CNT_EXCEPTION_DIS :	1;
		unsigned int  rsv_29					:	3;
	} Bits;
	unsigned int Raw;
};


/* TG VF CON */
#define REG_CAMSV_TG_VF_CON						0x0104
#define CAMSV_TG_VF_CON_VFDATA_EN				BIT(0)
union CAMSV_TG_VF_CON {
	struct {
		unsigned int  VFDATA_EN					:	1;
		unsigned int  SINGLE_MODE				:	1;
		unsigned int  rsv_2						:	2;
		unsigned int  FR_CON					:	3;
		unsigned int  rsv_7						:	1;
		unsigned int  SP_DELAY					:	3;
		unsigned int  rsv_11					:	1;
		unsigned int  SPDELAY_MODE				:	1;
		unsigned int  VFDATA_EN_MUX_0_SEL		:	1;
		unsigned int  VFDATA_EN_MUX_1_SEL		:	1;
		unsigned int  rsv_15					:	17;
	} Bits;
	unsigned int Raw;
};

/* TG GRAB PXL */
#define REG_CAMSV_TG_SEN_GRAB_PXL				0x0108

/* TG_GRAB LIN */
#define REG_CAMSV_TG_SEN_GRAB_LIN				0x010C

/* TG Path CFG */
#define REG_CAMSV_TG_PATH_CFG					0x0110

#define CAMSV_TG_PATH_TG_FULL_SEL				BIT(15)
union CAMSV_TG_PATH_CFG {
	struct {
		unsigned int  SEN_IN_LSB				:	3;
		unsigned int  rsv_3						:	1;
		unsigned int  JPGINF_EN					:	1;
		unsigned int  MEMIN_EN					:	1;
		unsigned int  rsv_6						:	1;
		unsigned int  JPG_LINEND_EN				:	1;
		unsigned int  DB_LOAD_DIS				:	1;
		unsigned int  DB_LOAD_SRC				:	1;
		unsigned int  DB_LOAD_VSPOL				:	1;
		unsigned int  DB_LOAD_HOLD				:	1;
		unsigned int  YUV_U2S_DIS				:	1;
		unsigned int  YUV_BIN_EN				:	1;
		unsigned int  TG_ERR_SEL				:	1;
		unsigned int  TG_FULL_SEL				:	1;
		unsigned int  TG_FULL_SEL2				:	1;
		unsigned int  FLUSH_DISABLE				:	1;
		unsigned int  INT_BANK_DISABLE			:	1;
		unsigned int  EXP_ESC					:	1;
		unsigned int  SUB_SOF_SRC_SEL			:	2;
		unsigned int  rsv_22					:	10;
	} Bits;
	unsigned int Raw;
};

/* TG FRMSIZE ST */
#define REG_CAMSV_TG_FRMSIZE_ST					0x0138

/* TG INTER ST */
#define REG_CAMSV_TG_INTER_ST					0x013C
#define CAMSV_TG_CS_MASK						0x3F00
#define CAMSV_TG_IDLE_ST						BIT(8)
union CAMSV_TG_INTER_ST {
	struct {
		unsigned int  SYN_VF_DATA_EN			:	1;
		unsigned int  OUT_RDY					:	1;
		unsigned int  OUT_REQ					:	1;
		unsigned int  rsv_3						:	5;
		unsigned int  TG_CAM_CS					:	6;
		unsigned int  rsv_14					:	2;
		unsigned int  CAM_FRM_CNT				:	8;
		unsigned int  CAM_TOTAL_FRM				:	8;
	} Bits;
	unsigned int Raw;
};

//TG_DCIF_CTL?

/* TG SUB PERIOD */
#define REG_CAMSV_TG_SUB_PERIOD					0x0164
union CAMSV_TG_SUB_PERIOD {
	struct {
		unsigned int  VS_PERIOD					:	8;
		unsigned int  SOF_PERIOD				:	8;
		unsigned int  rsv_16					:	16;
	} Bits;
	unsigned int Raw;
};

/* TG FRMSIZE ST R */
#define REG_CAMSV_TG_FRMSIZE_ST_R				0x016C

/* TG TIME STAMP */
#define REG_CAMSV_TG_TIME_STAMP					0x0178

/* PAK CON */
#define REG_CAMSV_PAK_CON						0x01C0
union CAMSV_PAK_CON {
	struct {
		unsigned int  PAK_SWAP_ODR				:	2;
		unsigned int  rsv_2						:	2;
		unsigned int  PAK_UV_SIGN				:	1;
		unsigned int  rsv_5						:	11;
		unsigned int  PAK_IN_BIT				:	5;
		unsigned int  rsv_21					:	11;
	} Bits;
	unsigned int Raw;
};

/* FBC IMGO CTL1 */
#define REG_CAMSV_FBC_IMGO_CTL1					0x0240
union CAMSV_FBC_IMGO_CTL1 {
	struct {
		unsigned int  rsv_0						:	8;
		unsigned int  FBC_RESET					:	1;
		unsigned int  FBC_DB_EN					:	1;
		unsigned int  rsv_10					:	1;
		unsigned int  rsv_11					:	1;
		unsigned int  LOCK_EN					:	1;
		unsigned int  DROP_TIMING				:	1;
		unsigned int  FBC_SUB_EN				:	1;
		unsigned int  FBC_EN					:	1;
		unsigned int  VALID_NUM					:	8;
		unsigned int  SUB_RATIO					:	8;
	} Bits;
	unsigned int Raw;
};

/* FBC IMGO CTL2 */
#define REG_CAMSV_FBC_IMGO_CTL2					0x0244
union CAMSV_FBC_IMGO_CTL2 {
	struct {
		unsigned int  IMGO_RCNT					:	8;
		unsigned int  IMGO_WCNT					:	8;
		unsigned int  IMGO_FBC_CNT				:	8;
		unsigned int  IMGO_DROP_CNT				:	8;
	} Bits;
	unsigned int Raw;
};

/* QBIN */
#define REG_CAMSV_QBN							0x0400
union CAMSV_QBN_CTL {
	struct {
		unsigned int  CAMSVQBN_ACC				:	2;
		unsigned int  rsv_2						:	2;
		unsigned int  CAMSVQBN_ACC_MODE			:	2;
		unsigned int  rsv_6						:	26;
	} Bits;
	unsigned int Raw;
};

/* Special FUN EN */
#define REG_CAMSV_SPECIAL_FUN_EN				0x0600
union CAMSV_SPECIAL_FUN_EN {
	struct {
		unsigned int  rsv_0						:	20;
		unsigned int  CONTINUOUS_COM_CON		:	2;
		unsigned int  rsv_22					:	1;
		unsigned int  CONTINUOUS_COM_EN			:	1;
		unsigned int  LPDVT_EN					:	1;
		unsigned int  GCLAST_EN					:	1;
		unsigned int  DCM_MODE					:	1;
		unsigned int  rsv_27					:	5;
	} Bits;
	unsigned int Raw;
};

/* DMA DEBUG SEL */
#define REG_CAMSV_DMATOP_DMA_DEBUG_SEL			0x0670

/* DMA DEBUG PORT */
#define REG_CAMSV_DMATOP_DMA_DEBUG_PORT			0x0674

/* IMGO BASE ADDR */
#define REG_CAMSV_IMGO_BASE_ADDR				0x0700

/* IMGO BASE ADDR MSB */
#define REG_CAMSV_IMGO_BASE_ADDR_MSB			0x0704

/* IMGO OFST ADDR */
#define REG_CAMSV_IMGO_OFST_ADDR				0x0708

/* IMGO OFST ADDR */
#define REG_CAMSV_IMGO_OFST_ADDR_MSB			0x070C

/* IMGO XSIZE */
#define REG_CAMSV_IMGO_XSIZE					0x0710
union CAMSV_IMGO_XSIZE {
	struct {
		unsigned int  XSIZE						:	16;
		unsigned int  rsv_16					:	16;
	} Bits;
	unsigned int Raw;
};

/* IMGO YSIZE */
#define REG_CAMSV_IMGO_YSIZE					0x0714
union CAMSV_IMGO_YSIZE {
	struct {
		unsigned int  YSIZE						:	16;
		unsigned int  rsv_16					:	16;
	} Bits;
	unsigned int Raw;
};

/* IMGO STRIDE */
#define REG_CAMSV_IMGO_STRIDE					0x0718
union CAMSV_IMGO_STRIDE {
	struct {
		unsigned int  STRIDE					:	16;
		unsigned int  rsv_16					:	16;
	} Bits;
	unsigned int Raw;
};

#define REG_CAMSV_IMGO_BASIC					0x071C
union CAMSV_IMGO_BASIC {
	struct {
		unsigned int  BUS_SIZE					:	4;
		unsigned int  rsv_4						:	4;
		unsigned int  FORMAT					:	6;
		unsigned int  rsv_14					:	9;
		unsigned int  EVEN_ODD_EN				:	1;
		unsigned int  rsv_24					:	4;
		unsigned int  BUS_SIZE_EN				:	1;
		unsigned int  FORMAT_EN					:	1;
		unsigned int  rsv_30					:	1;
		unsigned int  EVEN_ODD_EN_EN			:	1;
	} Bits;
	unsigned int Raw;
};

/* IMGO CON0 */
#define REG_CAMSV_IMGO_CON0						0x0720
union CAMSV_IMGO_CON0 {
	struct {
		unsigned int  FIFO_SIZE					:	12;
		unsigned int  rsv_12					:	12;
		unsigned int  MAX_BURST_LEN				:	8;
	} Bits;
	unsigned int Raw;
};

/* IMGO CON1 */
#define REG_CAMSV_IMGO_CON1						0x0724
union CAMSV_IMGO_CON1 {
	struct {
		unsigned int  FIFO_PRE_PRI_THRL			:	12;
		unsigned int  rsv_12					:	4;
		unsigned int  FIFO_PRE_PRI_THRH			:	12;
		unsigned int  LAST_PRE_ULTRA_EN			:	1;
		unsigned int  rsv_29					:	3;
	} Bits;
	unsigned int Raw;
};

/* IMGO CON2 */
#define REG_CAMSV_IMGO_CON2						0x0728
union CAMSV_IMGO_CON2 {
	struct {
		unsigned int  FIFO_PRI_THRL				:	12;
		unsigned int  rsv_12					:	4;
		unsigned int  FIFO_PRI_THRH				:	12;
		unsigned int  LAST_ULTRA_EN				:	1;
		unsigned int  rsv_29					:	3;
	} Bits;
	unsigned int Raw;
};

/* IMGO CON3 */
#define REG_CAMSV_IMGO_CON3						0x072C
union CAMSV_IMGO_CON3 {
	struct {
		unsigned int  FIFO_URGENT_THRL			:	12;
		unsigned int  rsv_12					:	4;
		unsigned int  FIFO_URGENT_THRH			:	12;
		unsigned int  rsv_28					:	3;
		unsigned int  FIFO_URGENT_EN			:	1;
	} Bits;
	unsigned int Raw;
};

/* IMGO CON4 */
#define REG_CAMSV_IMGO_CON4						0x0730
union CAMSV_IMGO_CON4 {
	struct {
		unsigned int  FIFO_DVFS_THRL			:	12;
		unsigned int  rsv_12					:	4;
		unsigned int  FIFO_DVFS_THRH			:	12;
		unsigned int  rsv_28					:	3;
		unsigned int  FIFO_DVFS_EN				:	1;
	} Bits;
	unsigned int Raw;
};

/* IMGO ERR STAT */
#define REG_CAMSV_IMGO_ERR_STAT					0x0734

/* DMA SPECIAL EN */
#define REG_CAMSV_DMA_SPECIAL_EN				0x0738
union CAMSV_DMA_SPECIAL_EN {
	struct {
		unsigned int  UFO_EN					:	1;
		unsigned int  rsv_1						:	2;
		unsigned int  UFO_XSIZE_QUEUE_FULL		:	1;
		unsigned int  FH_EN						:	1;
		unsigned int  rsv_5						:	2;
		unsigned int  N_UFE_XSIZE_QUEUE_FULL	:	1; //error flag
		unsigned int  IPU_RING_EN				:	1;
		unsigned int  RING_EN					:	1;
		unsigned int  rsv_10					:	2;
		unsigned int  BW_SELF_TEST_EN			:	1;
		unsigned int  V_FLIP_EN					:	1;
		unsigned int  V_FLIP_X2_EN				:	1;
		unsigned int  rsv_15					:	17;
	} Bits;
	unsigned int Raw;
};

/* IMGO CROP */
#define REG_CAMSV_IMGO_CROP						0x074C
union CAMSV_IMGO_CROP {
	struct {
		unsigned int  XOFFSET					:	16;
		unsigned int  YOFFSET					:	16;
	} Bits;
	unsigned int Raw;
};

/* IMGO FH SPARE 3 */
#define REG_CAMSV_FRAME_SEQ_NO					0x075C

/* IRQ Error Mask */
#define INT_ST_MASK_CAMSV_ERR (\
					CAMSV_INT_TG_ERR_ST |\
					CAMSV_INT_TG_GBERR_ST |\
					CAMSV_INT_DB_LOAD_ERR_ST |\
					CAMSV_INT_DMA_ERR_ST |\
					CAMSV_INT_IMGO_OVERR_ST)

/* Dma Error Mask */
#define DMA_ST_MASK_CAMSV_ERR (\
					CAMSV_INT_DMA_ERR_ST)

#endif	/* _CAMSV_REGS_H */
