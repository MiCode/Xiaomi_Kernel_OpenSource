/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _CAMSV_REGS_H
#define _CAMSV_REGS_H

#define REG_CAMSV_M1_CQI_ORIRDMA_CON0			0x0520
#define REG_CAMSV_M1_CQI_ORIRDMA_CON1			0x0524
#define REG_CAMSV_M1_CQI_ORIRDMA_CON2			0x0528
#define REG_CAMSV_M1_CQI_ORIRDMA_CON3			0x052C
#define REG_CAMSV_M1_CQI_ORIRDMA_CON4			0x0530

#define REG_CAMSV_M2_CQI_ORIRDMA_CON0			0x05A0
#define REG_CAMSV_M2_CQI_ORIRDMA_CON1			0x05A4
#define REG_CAMSV_M2_CQI_ORIRDMA_CON2			0x05A8
#define REG_CAMSV_M2_CQI_ORIRDMA_CON3			0x05AC
#define REG_CAMSV_M2_CQI_ORIRDMA_CON4			0x05A0

#define REG_CAMSVCQ_CQ_SUB_THR0_CTL				0x0170
union CAMSVCQ_CQ_SUB_THR0_CTL {
	struct {
		unsigned int CAMSVCQ_CQ_SUB_THR0_EN			:  1;
		unsigned int rsv_1							:  3;
		unsigned int CAMSVCQ_CQ_SUB_THR0_MODE		:  2;
		unsigned int rsv_6							:  2;
		unsigned int CAMSVCQ_CQ_SUB_THR0_DONE_SEL	:  1;
		unsigned int rsv_9							: 23;
	} Bits;
	unsigned int Raw;
};

#define REG_CAMSVCQ_SCQ_START_PERIOD			0x0108
union CAMSVCQ_SCQ_START_PERIOD {
	struct {
		unsigned int CAMSVCQ_SCQ_START_PERIOD		: 32;
	} Bits;
	unsigned int Raw;
};

#define REG_CAMSVCQ_CQ_EN						0x0100
union CAMSVCQ_CQ_EN {
	struct {
		unsigned int CAMSVCQ_CQ_APB_2T				:  1;
		unsigned int CAMSVCQ_CQ_DROP_FRAME_EN		:  1;
		unsigned int CAMSVCQ_CQ_SOF_SEL				:  1;
		unsigned int rsv_3							:  1;
		unsigned int CAMSVCQ_CQ_DB_EN				:  1;
		unsigned int rsv_5							:  3;
		unsigned int CAMSVCQ_CQ_DB_LOAD_MODE		:  1;
		unsigned int rsv_9							:  3;
		unsigned int CAMSVCQ_SCQ_STAGGER_MODE		:  1;
		unsigned int rsv_13							:  3;
		unsigned int CAMSVCQ_CQ_RESET				:  1;
		unsigned int rsv_17							:  4;
		unsigned int CAMSVCQ_SCQ_SUBSAMPLE_EN		:  1;
		unsigned int rsv_22							:  6;
		unsigned int CAMSVCQ_CQ_DBG_SEL				:  1;
		unsigned int CAMSVCQ_CQ_DBG_MAIN_SUB_SEL	:  1;
		unsigned int rsv_30							:  2;
	} Bits;
	unsigned int Raw;
};
#define REG_CAMSVCQ_CQ_SUB_EN						0x0160
union CAMSVCQ_CQ_SUB_EN {
	struct {
		unsigned int CAMSVCQ_CQ_SUB_APB_2T           :  1;
		unsigned int CAMSVCQ_CQ_SUB_DROP_FRAME_EN    :  1;
		unsigned int CAMSVCQ_CQ_SUB_SOF_SEL          :  1;
		unsigned int rsv_3                           :  1;
		unsigned int CAMSVCQ_CQ_SUB_DB_EN            :  1;
		unsigned int rsv_5                           :  3;
		unsigned int CAMSVCQ_CQ_SUB_DB_LOAD_MODE     :  1;
		unsigned int rsv_9                           :  3;
		unsigned int CAMSVCQ_SCQ_SUB_STAGGER_MODE    :  1;
		unsigned int rsv_13                          :  3;
		unsigned int CAMSVCQ_CQ_SUB_RESET            :  1;
		unsigned int rsv_17                          : 11;
		unsigned int CAMSVCQ_CQ_SUB_DBG_SEL          :  1;
		unsigned int rsv_29                          :  3;
	} Bits;
	unsigned int Raw;
};
#define REG_CAMSVCQ_CQ_SUB_THR0_DESC_SIZE_2			0x0188
union CAMSVCQ_CQ_SUB_THR0_DESC_SIZE_2 {
	struct {
		unsigned int CAMSVCQ_CQ_SUB_THR0_DESC_SIZE_2	: 16;
		unsigned int rsv_16					: 16;
	} Bits;
	unsigned int Raw;
};

#define REG_CAMSVCQ_CQ_SUB_THR0_BASEADDR_2_MSB		0x0180
union CAMSVCQ_CQ_SUB_THR0_BASEADDR_2_MSB {
	struct {
		unsigned int CAMSVCQ_CQ_SUB_THR0_BASEADDR_2_MSB	:  3;
		unsigned int rsv_3					: 29;
	} Bits;
	unsigned int Raw;
};

#define REG_CAMSVCQ_CQ_SUB_THR0_BASEADDR_2			0x017c
union CAMSVCQ_CQ_SUB_THR0_BASEADDR_2 {
	struct {
		unsigned int CAMSVCQ_CQ_SUB_THR0_BASEADDR_2		: 32;
	} Bits;
	unsigned int Raw;
};

#define REG_CAMSVCQTOP_THR_START					0x0014
union CAMSVCQTOP_THR_START {
	struct {
		unsigned int CAMSVCQTOP_CSR_CQ_THR0_START		:  1;
		unsigned int rsv_1					: 31;
	} Bits;
	unsigned int Raw;
};

#define REG_CAMSVCQTOP_INT_0_EN						0x0018
union CAMSVCQTOP_INT_0_EN {
	struct {
		unsigned int CAMSVCQTOP_CSR_SCQ_SUB_THR_DONE_INT_EN			:  1;
		unsigned int CAMSVCQTOP_CSR_SCQ_MAX_START_DLY_ERR_INT_EN	:  1;
		unsigned int CAMSVCQTOP_CSR_SCQ_SUB_CODE_ERR_INT_EN			:  1;
		unsigned int CAMSVCQTOP_CSR_SCQ_SUB_VB_ERR_INT_EN			:  1;
		unsigned int CAMSVCQTOP_CSR_SCQ_TRIG_DLY_INT_EN				:  1;
		unsigned int CAMSVCQTOP_CSR_DMA_ERR_INT_EN			:  1;
		unsigned int CAMSVCQTOP_CSR_CQI_E1_DONE_INT_EN				:  1;
		unsigned int CAMSVCQTOP_CSR_CQI_E2_DONE_INT_EN				:  1;
		unsigned int CAMSVCQTOP_CSR_SCQ_MAX_START_DLY_SMALL_INT_EN	:  1;
		unsigned int rsv_9					: 22;
		unsigned int CAMSVCQTOP_CSR_INT_0_WCLR_EN			:  1;
	} Bits;
	unsigned int Raw;
};

/* GROUPS */
#define REG_CAMSVCENTRAL_GROUP_TAG0				0x01B8
#define REG_CAMSVCENTRAL_GROUP_TAG_SHIFT		0x4

/* INT EN */
#define REG_CAMSVCENTRAL_DONE_STATUS_EN			0x0344
#define REG_CAMSVCENTRAL_ERR_STATUS_EN			0x034C
#define REG_CAMSVCENTRAL_SOF_STATUS_EN			0x035C

union CAMSVCENTRAL_DONE_STATUS_EN {
	struct {
		unsigned int GP_PASS1_DONE_0_ST_EN		:  1;
		unsigned int GP_PASS1_DONE_1_ST_EN		:  1;
		unsigned int GP_PASS1_DONE_2_ST_EN		:  1;
		unsigned int GP_PASS1_DONE_3_ST_EN		:  1;
		unsigned int rsv_4						: 12;
		unsigned int SW_PASS1_DONE_0_ST_EN		:  1;
		unsigned int SW_PASS1_DONE_1_ST_EN		:  1;
		unsigned int SW_PASS1_DONE_2_ST_EN		:  1;
		unsigned int SW_PASS1_DONE_3_ST_EN		:  1;
		unsigned int rsv_20						: 12;
	} Bits;
	unsigned int Raw;
};

#define REG_CAMSVCENTRAL_ERR_CTL				0x0170
union CAMSVCENTRAL_ERR_CTL {
	struct {
		unsigned int GRAB_ERR_FLIMIT_NO			:  4;
		unsigned int GRAB_ERR_FLIMIT_EN			:  1;
		unsigned int GRAB_ERR_EN				:  1;
		unsigned int rsv_6						:  2;
		unsigned int REZ_OVRUN_FLIMIT_NO		:  4;
		unsigned int REZ_OVRUN_FLIMIT_EN		:  1;
		unsigned int rsv_13						:  3;
		unsigned int GROUP_PXL_ERR_EN			:  1;
		unsigned int rsv_17						: 15;
	} Bits;
	unsigned int  Raw;
};

/* DONE Status */
#define REG_CAMSVCENTRAL_DONE_STATUS			0x0348
#define CAMSVCENTRAL_SW_GP_PASS1_DONE_0_ST			BIT(16)
#define CAMSVCENTRAL_SW_GP_PASS1_DONE_1_ST			BIT(17)
#define CAMSVCENTRAL_SW_GP_PASS1_DONE_2_ST			BIT(18)
#define CAMSVCENTRAL_SW_GP_PASS1_DONE_3_ST			BIT(19)


/* INT Status */
#define REG_CAMSVCENTRAL_SOF_STATUS			0x0360
#define CAMSVCENTRAL_VS_ST_TAG1					BIT(1)
#define CAMSVCENTRAL_VS_ST_TAG2					BIT(5)
#define CAMSVCENTRAL_VS_ST_TAG3					BIT(9)
#define CAMSVCENTRAL_VS_ST_TAG4					BIT(13)
#define CAMSVCENTRAL_VS_ST_TAG5					BIT(17)
#define CAMSVCENTRAL_VS_ST_TAG6					BIT(21)
#define CAMSVCENTRAL_VS_ST_TAG7					BIT(25)
#define CAMSVCENTRAL_VS_ST_TAG8					BIT(29)
#define CAMSVCENTRAL_SOF_ST_TAG1				BIT(2)
#define CAMSVCENTRAL_SOF_ST_TAG2				BIT(6)
#define CAMSVCENTRAL_SOF_ST_TAG3				BIT(10)
#define CAMSVCENTRAL_SOF_ST_TAG4				BIT(14)
#define CAMSVCENTRAL_SOF_ST_TAG5				BIT(18)
#define CAMSVCENTRAL_SOF_ST_TAG6				BIT(22)
#define CAMSVCENTRAL_SOF_ST_TAG7				BIT(26)
#define CAMSVCENTRAL_SOF_ST_TAG8				BIT(30)

/* ERROR Status */
#define REG_CAMSVCENTRAL_ERR_STATUS			0x0350
#define CAMSVCENTRAL_DMA_SRAM_FULL_ST			BIT(0)
#define CAMSVCENTRAL_CQ_BUSY_DROP_ST			BIT(1)
#define CAMSVCENTRAL_GRAB_ERR_TAG1				BIT(8)
#define CAMSVCENTRAL_OVRUN_ERR_TAG1				BIT(9)
#define CAMSVCENTRAL_DB_LOAD_ERR_TAG1			BIT(10)
#define CAMSVCENTRAL_GRAB_ERR_TAG2				BIT(11)
#define CAMSVCENTRAL_OVRUN_ERR_TAG2				BIT(12)
#define CAMSVCENTRAL_DB_LOAD_ERR_TAG2			BIT(13)
#define CAMSVCENTRAL_GRAB_ERR_TAG3				BIT(14)
#define CAMSVCENTRAL_OVRUN_ERR_TAG3				BIT(15)
#define CAMSVCENTRAL_DB_LOAD_ERR_TAG3			BIT(16)
#define CAMSVCENTRAL_GRAB_ERR_TAG4				BIT(17)
#define CAMSVCENTRAL_OVRUN_ERR_TAG4				BIT(18)
#define CAMSVCENTRAL_DB_LOAD_ERR_TAG4			BIT(19)
#define CAMSVCENTRAL_GRAB_ERR_TAG5				BIT(20)
#define CAMSVCENTRAL_OVRUN_ERR_TAG5				BIT(21)
#define CAMSVCENTRAL_DB_LOAD_ERR_TAG5			BIT(22)
#define CAMSVCENTRAL_GRAB_ERR_TAG6				BIT(23)
#define CAMSVCENTRAL_OVRUN_ERR_TAG6				BIT(24)
#define CAMSVCENTRAL_DB_LOAD_ERR_TAG6			BIT(25)
#define CAMSVCENTRAL_GRAB_ERR_TAG7				BIT(26)
#define CAMSVCENTRAL_OVRUN_ERR_TAG7				BIT(27)
#define CAMSVCENTRAL_DB_LOAD_ERR_TAG7			BIT(28)
#define CAMSVCENTRAL_GRAB_ERR_TAG8				BIT(29)
#define CAMSVCENTRAL_OVRUN_ERR_TAG8				BIT(30)
#define CAMSVCENTRAL_DB_LOAD_ERR_TAG8			BIT(31)

/* SW CTL */
#define REG_CAMSVCENTRAL_SW_CTL					0x01D0
#define REG_CAMSVDMATOP_SW_RST_CTL              0x0020
#define REG_CAMSVCENTRAL_DCM_DIS				0x007C
#define REG_CAMSVCQTOP_SW_RST_CTL				0x0010
#define REG_CAM_MAIN_SW_RST_1					0x0058

/* TAGS */
#define REG_CAMSVCENTRAL_LAST_TAG				0x01C8
#define REG_CAMSVCENTRAL_FIRST_TAG				0x01CC

/* CLK EN */
#define REG_CAMSVCENTRAL_DMA_EN_IMG				0x0040

/* TG SEN Mode */
#define REG_CAMSVCENTRAL_SEN_MODE				0x0140
#define CAMSVCENTRAL_SEN_MODE_CMOS_EN			BIT(0)
#define CAMSVCENTRAL_SEN_MODE_CMOS_RDY_SEL		BIT(14)
#define CAMSVCENTRAL_SEN_MODE_FIFO_FULL_SEL		BIT(15)
union CAMSVCENTRAL_SEN_MODE {
	struct {
		unsigned int CMOS_EN				:	1;
		unsigned int SAT_SWITCH_GROUP		:	1;
		unsigned int rsv_2					:	9;
		unsigned int TG_MODE_OFF			:	1;
		unsigned int PXL_CNT_RST_SRC		:	1;
		unsigned int FLUSH_ALL_SRC_DIS		:	1;
		unsigned int CMOS_RDY_SEL			:	1;
		unsigned int FIFO_FULL_SEL			:	1;
		unsigned int CAM_SUB_EN				:	1;
		unsigned int VS_SUB_EN				:	1;
		unsigned int SOF_SUB_EN				:	1;
		unsigned int DOWN_SAMPLE_EN			:	1;
		unsigned int rsv_20					:	4;
		unsigned int VSYNC_INT_POL			:	1;
		unsigned int rsv_25					:	1;
		unsigned int RGBW_EN				:	1;
		unsigned int LINE_OK_CHECK			:	1;
		unsigned int rsv_28					:	4;
	} Bits;
	unsigned int Raw;
};

/* SUB_PERIOD */
#define REG_CAMSVCENTRAL_SUB_PERIOD				0x0144
union CAMSVCENTRAL_SUB_PERIOD {
	struct {
		unsigned int VS_PERIOD				:  8;
		unsigned int SOF_PERIOD				:  8;
		unsigned int DOWN_SAMPLE_PERIOD		:  8;
		unsigned int rsv_24					:  8;
	} Bits;
	unsigned int Raw;
};

/* TG TIMESTAMP */
#define REG_CAMSVCENTRAL_TIME_STAMP_CTL			0x018C
union CAMSVCENTRAL_TIME_STAMP_CTL {
	struct /* 0x1A10018C */
	{
		unsigned int TIME_STAMP_SEL				:  1;
		unsigned int TIME_STP_EN				:  1;
		unsigned int rsv_2						:  2;
		unsigned int TIME_STAMP_LOCK			:  1;
		unsigned int rsv_5						:  3;
		unsigned int TIME_STAMP_TIE_SPARE0		:  1;
		unsigned int rsv_9						: 23;
	} Bits;
	unsigned int Raw;
};

/* TG VF CON */
#define REG_CAMSVCENTRAL_VF_CON					0x0148
#define CAMSVCENTRAL_VF_CON_VFDATA_EN			BIT(0)
union CAMSVCENTRAL_VF_CON {
	struct {
		unsigned int VFDATA_EN					:  1;
		unsigned int SINGLE_MODE				:  1;
		unsigned int rsv_2						:  2;
		unsigned int FR_CON						:  3;
		unsigned int rsv_7						:  1;
		unsigned int SP_DELAY					:  3;
		unsigned int rsv_11						:  1;
		unsigned int SPDELAY_MODE				:  1;
		unsigned int VFDATA_EN_MUX_0_SEL		:  1;
		unsigned int VFDATA_EN_MUX_1_SEL		:  1;
		unsigned int rsv_15						: 17;
	} Bits;
	unsigned int Raw;
};

/* TG SIZE */
#define REG_CAMSVCENTRAL_FRMSIZE_ST				0x0188
#define REG_CAMSVCENTRAL_FRMSIZE_ST_R			0x0180

/* TG GRAB PXL */
#define REG_CAMSVCENTRAL_GRAB_PXL_TAG1			0x0548
#define CAMSVCENTRAL_GRAB_PXL_TAG_SHIFT			0x40

/* TG_GRAB LIN */
#define REG_CAMSVCENTRAL_GRAB_LIN_TAG1			0x054C
#define CAMSVCENTRAL_GRAB_LIN_TAG_SHIFT			0x40

/* VF_ST */
#define REG_CAMSVCENTRAL_VF_ST_TAG1				0x0550
#define CAMSVCENTRAL_VF_ST_TAG_SHIFT			0x40

/* FMT SEL */
#define REG_CAMSVCENTRAL_FORMAT_TAG1			0x0554
#define CAMSVCENTRAL_FORMAT_TAG_SHIFT			0x40

union CAMSVCENTRAL_FORMAT_TAG1 {
	struct {
		unsigned int  FMT_TAG1					:  4;
		unsigned int  UNPACK_MODE_TAG1			:  1;
		unsigned int  rsv_5						:  2;
		unsigned int  MQE_MODE_TAG1				:  4;
		unsigned int  MQE_EN_TAG1				:  1;
		unsigned int  rsv_12					:  1;
		unsigned int  YUV_U2S_DIS_TAG1			:  1;
		unsigned int  DATA_SWAP_TAG1			:  2;
		unsigned int  UFE_IMGO_FMT_TAG1			:  8;
		unsigned int  UFE_FORCE_PCM_TAG1		:  1;
		unsigned int  UFE_TCCT_BYP_TAG1			:  1;
		unsigned int  UFE_EN_TAG1				:  1;
		unsigned int  QBN_RATIO_TAG1			:  2;
		unsigned int  QBN_MODE_TAG1				:  2;
		unsigned int  QBN_EN_TAG1				:  1;
	} Bits;
	unsigned int  Raw;
};


/* TG Path CFG */
#define REG_CAMSVCENTRAL_PATH_CFG				0x014C
union CAMSVCENTRAL_PATH_CFG {
	struct {
		unsigned int rsv_0						:  4;
		unsigned int JPGINF_EN					:  1;
		unsigned int rsv_5						:  2;
		unsigned int JPG_LINEND_EN				:  1;
		unsigned int SYNC_VF_EN_DB_LOAD_DIS		:  1;
		unsigned int rsv_9						: 10;
		unsigned int EXP_ESC					:  1;
		unsigned int rsv_20						: 12;
	} Bits;
	unsigned int Raw;
};

/* TG DB */
#define REG_CAMSVCENTRAL_MODULE_DB				0x01B4
union CAMSVCENTRAL_MODULE_DB {
	struct {
		unsigned int rsv_0						: 26;
		unsigned int CAM_DB_LOAD_FORCE			:  1;
		unsigned int rsv_27						:  3;
		unsigned int CAM_DB_EN					:  1;
		unsigned int CAM_DB_LOCK				:  1;
	} Bits;
	unsigned int Raw;
};

/* IMGO FBC0 */
#define REG_CAMSVCENTRAL_FBC0_TAG1				0x0540
#define CAMSVCENTRAL_FBC0_TAG_SHIFT				0x40
union CAMSVCENTRAL_FBC0_TAG1 {
	struct {
		unsigned int rsv_0						:  7;
		unsigned int RCNT_INC_TAG1				:  1;
		unsigned int FBC_RESET_TAG1				:  1;
		unsigned int rsv_9						:  3;
		unsigned int FBC_LOCK_EN_TAG1			:  1;
		unsigned int rsv_13						:  1;
		unsigned int FBC_SUB_EN_TAG1			:  1;
		unsigned int FBC_EN_TAG1				:  1;
		unsigned int FBC_VALID_NUM_TAG1			:  8;
		unsigned int FBC_SUB_RATIO_TAG1			:  8;
	} Bits;
	unsigned int Raw;
};

/* IMGO FBC1 */
#define REG_CAMSVCENTRAL_FBC1_TAG1				0x0544
#define CAMSVCENTRAL_FBC1_TAG_SHIFT				0x40

union CAMSVCENTRAL_FBC1_TAG1 {
	/* 0x1A100544 */
	struct {
		unsigned int RCNT_TAG1					:  8;
		unsigned int WCNT_TAG1					:  8;
		unsigned int FBC_CNT_TAG1				:  4;
		unsigned int rsv_20						:  5;
		unsigned int DROP_CNT_TAG1				:  7;
	} Bits;
	unsigned int Raw;
};

#define REG_CAMSVCENTRAL_FBC0_TAG1				0x0540
#define REG_CAMSVCENTRAL_FBC1_TAG1				0x0544

/* CAMSV TAG SEL */
#define REG_E_CAMSVCENTRAL_TAG_R_SEL			0x0174

/* CAMSV DCIF SET */
#define REG_E_CAMSVCENTRAL_DCIF_SET				0x01D4

/* CAMSV DCIF SEL */
#define REG_E_CAMSVCENTRAL_DCIF_SEL				0x01D8

/* DMATOP DEBUG SEL */
#define REG_CAMSV_DMATOP_DMA_DEBUG_SEL			0x0090

/* DMATOP DEBUG PORT */
#define REG_CAMSV_DMATOP_DMA_DEBUG_PORT			0x0094

/* DMATOP BASE ADDR  */
#define REG_CAMSVDMATOP_WDMA_BASE_ADDR_IMG1		0x0200
#define CAMSVDMATOP_WDMA_BASE_ADDR_IMG_SHIFT	0x40

/* DMATOP BASE ADDR MSB  */
#define REG_CAMSVDMATOP_WDMA_BASE_ADDR_MSB_IMG1	0x0204
#define CAMSVDMATOP_WDMA_BASE_ADDR_MSB_IMG_SHIFT	0x40


/* IMGO BASE ADDR SHIFT */
#define REG_CAMSVDMATOP_WDMA_ADDR_SHIFT		0x40

/* DMATOP STRIDE */
#define REG_CAMSVDMATOP_WDMA_BASIC_IMG1			0x0210
#define CAMSVDMATOP_WDMA_BASIC_IMG_SHIFT		0x40

union CAMSVDMATOP_WDMA_BASIC_IMG1 {
	struct {
		unsigned int MAX_BURST_LEN_IMG1		:  5;
		unsigned int rsv_5					:  3;
		unsigned int WDMA_RSV_IMG1			:  8;
		unsigned int STRIDE_IMG1			: 16;
	} Bits;
	unsigned int Raw;
};

/* CAMSV CON */
#define REG_CAMSVDMATOP_CON1_IMG				0x0000
#define REG_CAMSVDMATOP_CON2_IMG				0x0004
#define REG_CAMSVDMATOP_CON3_IMG				0x0008
#define REG_CAMSVDMATOP_CON4_IMG				0x000C

/* IMGO FH SPARE 3 */
#define REG_CAMSVCENTRAL_FRAME_SEQ_NO			0x0068

/* IRQ Error Mask */
#define ERR_ST_MASK_TAG1_ERR (\
					CAMSVCENTRAL_GRAB_ERR_TAG1 |\
					CAMSVCENTRAL_OVRUN_ERR_TAG1 |\
					CAMSVCENTRAL_DB_LOAD_ERR_TAG1)
#define ERR_ST_MASK_TAG2_ERR (\
					CAMSVCENTRAL_GRAB_ERR_TAG2 |\
					CAMSVCENTRAL_OVRUN_ERR_TAG2 |\
					CAMSVCENTRAL_DB_LOAD_ERR_TAG2)
#define ERR_ST_MASK_TAG3_ERR (\
					CAMSVCENTRAL_GRAB_ERR_TAG3 |\
					CAMSVCENTRAL_OVRUN_ERR_TAG3 |\
					CAMSVCENTRAL_DB_LOAD_ERR_TAG3)
#define ERR_ST_MASK_TAG4_ERR (\
					CAMSVCENTRAL_GRAB_ERR_TAG4 |\
					CAMSVCENTRAL_OVRUN_ERR_TAG4 |\
					CAMSVCENTRAL_DB_LOAD_ERR_TAG4)
#define ERR_ST_MASK_TAG5_ERR (\
					CAMSVCENTRAL_GRAB_ERR_TAG5 |\
					CAMSVCENTRAL_OVRUN_ERR_TAG5 |\
					CAMSVCENTRAL_DB_LOAD_ERR_TAG5)
#define ERR_ST_MASK_TAG6_ERR (\
					CAMSVCENTRAL_GRAB_ERR_TAG6 |\
					CAMSVCENTRAL_OVRUN_ERR_TAG6 |\
					CAMSVCENTRAL_DB_LOAD_ERR_TAG6)
#define ERR_ST_MASK_TAG7_ERR (\
					CAMSVCENTRAL_GRAB_ERR_TAG7 |\
					CAMSVCENTRAL_OVRUN_ERR_TAG7 |\
					CAMSVCENTRAL_DB_LOAD_ERR_TAG7)
#define ERR_ST_MASK_TAG8_ERR (\
					CAMSVCENTRAL_GRAB_ERR_TAG8 |\
					CAMSVCENTRAL_OVRUN_ERR_TAG8 |\
					CAMSVCENTRAL_DB_LOAD_ERR_TAG8)
/* camsv CQ irq status */
#define REG_CAMSVCQTOP_INT_0_STATUS				0x001c
#define CAMSVCQTOP_SCQ_SUB_THR_DONE				BIT(0)

#endif	/* _CAMSV_REGS_H */
