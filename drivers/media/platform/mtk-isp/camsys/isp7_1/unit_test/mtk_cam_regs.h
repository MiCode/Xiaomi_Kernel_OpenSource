/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Bibby Hsieh <bibby.hsieh@mediatek.com>
 */

/* normal siganl */
#define VS_INT_ST			(1L << 0)
#define TG_INT1_ST			(1L << 1)
#define TG_INT2_ST			(1L << 2)
#define EXPDON_ST			(1L << 5)
#define SOF_INT_ST			(1L << 8)
#define HW_PASS1_DON_ST			(1L << 20)
#define SW_PASS1_DON_ST			(1L << 22)

/* YUV siganl */
#define YUV_SW_PASS1_DON_ST		(1L << 0)
#define YUV_PASS1_DON_ST		(1L << 1)
#define YUV_DMA_ERR_ST			(1L << 2)

/* err status */
#define TG_OVRUN_ST			(1L << 6)
#define TG_GBERR_ST			(1L << 7)
#define CQ_DB_LOAD_ERR_ST		(1L << 12)
#define CQ_MAIN_CODE_ERR_ST		(1L << 14)
#define CQ_MAIN_VS_ERR_ST		(1L << 15)
#define CQ_MAIN_TRIG_DLY_ST		(1L << 16)
#define LSCI_ERR_ST			(1L << 24)
#define DMA_ERR_ST			(1L << 26)

/* CAM DMA done status */
#define IMGO_DONE_ST			(1L << 0)
#define CQI_R1_DONE_ST			(1L << 8)

/* IRQ signal mask */
#define INT_ST_MASK_CAM (VS_INT_ST	 |\
			 TG_INT1_ST	 |\
			 TG_INT2_ST	 |\
			 EXPDON_ST       |\
			 HW_PASS1_DON_ST |\
			 SOF_INT_ST      |\
			 SW_PASS1_DON_ST)

/* IRQ Error Mask */
#define INT_ST_MASK_CAM_ERR (TG_OVRUN_ST	 |\
			     TG_GBERR_ST	 |\
			     CQ_DB_LOAD_ERR_ST	 |\
			     CQ_MAIN_CODE_ERR_ST |\
			     CQ_MAIN_VS_ERR_ST	 |\
			     DMA_ERR_ST)

#define ISP_SENINF_CTRL(regs)				(regs + 0x0200)
#define ISP_SENINF_TSETMDL_CTRL(regs)			(regs + 0x0220)
#define ISP_SENINF_MUX_CTRL_0(regs)			(regs + 0x0D00)
#define ISP_SENINF_MUX_CTRL_1(regs)			(regs + 0x0D04)

#define ISP_SENINF_TM_CTL(regs)				(regs + 0x0608)
#define ISP_SENINF_TM_SIZE(regs)			(regs + 0x060C)
#define ISP_SENINF_TM_CLK(regs)				(regs + 0x0610)
#define ISP_SENINF_TM_DUM(regs)				(regs + 0x0618)

#define ISP_SENINF_CAM_MUX_CTRL_0(regs)         (regs + 0x0400)
#define ISP_SENINF_CAM_MUX_CTRL_1(regs)         (regs + 0x0404)
#define ISP_SENINF_CAM_MUX_CTRL_2(regs)         (regs + 0x0408)
#define ISP_SENINF_CAM_MUX_CTRL_3(regs)         (regs + 0x040C)

#define CAMSYS_MAIN_REG_HALT1_EN(regs)			(regs + 0x0350)
#define CAMSYS_MAIN_REG_HALT2_EN(regs)			(regs + 0x0354)
#define CAMSYS_MAIN_REG_HALT3_EN(regs)			(regs + 0x0358)
#define CAMSYS_MAIN_REG_HALT4_EN(regs)			(regs + 0x035C)
#define CAMSYS_MAIN_REG_HALT5_EN(regs)			(regs + 0x0360)
#define CAMSYS_MAIN_REG_HALT6_EN(regs)			(regs + 0x0364)

#define CAM_REG_CTL_RAW_INT_STATUS(regs)		(regs + 0x0104)
#define CAM_REG_CTL_RAW_INT2_STATUS(regs)		(regs + 0x0114)
#define CAM_REG_CTL_RAW_INT3_STATUS(regs)		(regs + 0x0124)
#define CAM_REG_CTL_RAW_INT4_STATUS(regs)		(regs + 0x0134)
#define CAM_REG_CTL_RAW_INT5_STATUS(regs)		(regs + 0x0144)
#define CAM_REG_CTL_RAW_INT6_EN				0x0150
#define CAM_REG_CTL_RAW_INT6_STATUS(regs)		(regs + 0x0154)
#define CAM_REG_CTL_RAW_INT7_EN				0x0160
#define CAM_REG_CTL_RAW_INT7_STATUS(regs)		(regs + 0x0164)

#define CAM_REG_CTL_RAW_INT_STATUSX(regs)		(regs + 0x0108)
#define CAM_REG_CTL_RAW_INT2_STATUSX(regs)		(regs + 0x0118)
#define CAM_REG_CTL_RAW_INT3_STATUSX(regs)		(regs + 0x0128)
#define CAM_REG_CTL_RAW_INT4_STATUSX(regs)		(regs + 0x0138)
#define CAM_REG_CTL_RAW_INT5_STATUSX(regs)		(regs + 0x0148)
#define CAM_REG_CTL_RAW_INT6_STATUSX(regs)		(regs + 0x0158)
#define CTL_CQ_THR0_DONE_ST				BIT(0)
#define CAM_REG_CTL_RAW_INT7_STATUSX(regs)		(regs + 0x0168)
#define CTL_CQ_THRSUB_DONE_ST				BIT(10)


#define CAM_REG_CTL2_RAW_INT_STATUS(regs)		(regs + 0x0104)
#define CAM_REG_CTL2_RAW_INT2_STATUS(regs)		(regs + 0x0114)
#define CAM_REG_CTL2_RAW_INT4_STATUS(regs)		(regs + 0x0134)
#define CAM_REG_CTL2_RAW_INT5_STATUS(regs)		(regs + 0x0144)

#define CAM_REG_CTL2_RAW_INT_STATUSX(regs)		(regs + 0x0108)
#define CAM_REG_CTL2_RAW_INT2_STATUSX(regs)		(regs + 0x0118)
#define CAM_REG_CTL2_RAW_INT4_STATUSX(regs)		(regs + 0x0138)
#define CAM_REG_CTL2_RAW_INT5_STATUSX(regs)		(regs + 0x0148)

#define CAM_REG_CTL_RAW_MOD5_DCM_DIS			0x0310
#define CAM_REG_CTL_RAW_MOD6_DCM_DIS			0x0314

#define CAM_REG_CQ_THR0_CTL(regs)			(regs + 0x0414)
#define CAM_REG_CQ_THR0_BASEADDR(regs)			(regs + 0x0418)
#define CAM_REG_CQ_THR0_DESC_SIZE(regs)			(regs + 0x041C)

#define CAM_REG_TG_SEN_MODE(regs)			(regs + 0x0700)
#define TG_CMOS_RDY_SEL					BIT(14)
#define CAM_REG_TG_SEN_GRAB_PXL(regs)			(regs + 0x0708)
#define CAM_REG_TG_SEN_GRAB_LIN(regs)			(regs + 0x070C)
#define CAM_REG_TG_PATH_CFG(regs)			(regs + 0x0710)
#define TG_TG_FULL_SEL					BIT(15)
#define CAM_REG_TG_FRMSIZE_ST(regs)			(regs + 0x0738)
#define CAM_REG_TG_FRMSIZE_ST_R(regs)			(regs + 0x076C)

#define CAM_REG_CQI_R1A_CON0(regs)			(regs + 0x445C)
#define CAM_REG_CQI_R1A_CON1(regs)			(regs + 0x4460)
#define CAM_REG_CQI_R1A_CON2(regs)			(regs + 0x4464)
#define CAM_REG_CQI_R1A_CON3(regs)			(regs + 0x4468)
#define CAM_REG_CQI_R1A_CON4(regs)			(regs + 0x446C)

#define CAM_REG_CQI_R2A_CON0(regs)			(regs + 0x44BC)
#define CAM_REG_CQI_R2A_CON1(regs)			(regs + 0x44C0)
#define CAM_REG_CQI_R2A_CON2(regs)			(regs + 0x44C4)
#define CAM_REG_CQI_R2A_CON3(regs)			(regs + 0x44C8)
#define CAM_REG_CQI_R2A_CON4(regs)			(regs + 0x44CC)

#define CAM_REG_CQI_R3A_CON0(regs)			(regs + 0x44BC)
#define CAM_REG_CQI_R3A_CON1(regs)			(regs + 0x44C0)
#define CAM_REG_CQI_R3A_CON2(regs)			(regs + 0x44C4)
#define CAM_REG_CQI_R3A_CON3(regs)			(regs + 0x44C8)
#define CAM_REG_CQI_R3A_CON4(regs)			(regs + 0x44CC)

#define CAM_REG_CQI_R4A_CON0(regs)			(regs + 0x44BC)
#define CAM_REG_CQI_R4A_CON1(regs)			(regs + 0x44C0)
#define CAM_REG_CQI_R4A_CON2(regs)			(regs + 0x44C4)
#define CAM_REG_CQI_R4A_CON3(regs)			(regs + 0x44C8)
#define CAM_REG_CQI_R4A_CON4(regs)			(regs + 0x44CC)

#define CAM_REG_IMGO_CON0(regs)				(regs + 0x483C)
#define CAM_REG_IMGO_CON1(regs)				(regs + 0x4840)
#define CAM_REG_IMGO_CON2(regs)				(regs + 0x4844)
#define CAM_REG_IMGO_CON3(regs)				(regs + 0x4848)
#define CAM_REG_IMGO_CON4(regs)				(regs + 0x484C)

/* error status */
#define REG_RAWI_R2_ERR_STAT				0x4230
#define REG_UFDI_R2_ERR_STAT				0x4290
#define REG_RAWI_R3_ERR_STAT				0x42F0
#define REG_UFDI_R3_ERR_STAT				0x4350
#define REG_RAWI_R6_ERR_STAT				0x4810
#define REG_CQI_R1_ERR_STAT				0x4470
#define REG_CQI_R2_ERR_STAT				0x44D0
#define REG_CQI_R3_ERR_STAT				0x4530
#define REG_CQI_R4_ERR_STAT				0x4590
#define REG_LSCI_R1_ERR_STAT				0x45F0
#define REG_BPCI_R1_ERR_STAT				0x4650
#define REG_BPCI_R2_ERR_STAT				0x4690
#define REG_BPCI_R3_ERR_STAT				0x46D0
#define REG_PDI_R1_ERR_STAT				0x4710
#define REG_AAI_R1_ERR_STAT				0x4790
#define REG_CACI_R1_ERR_STAT				0x47D0
#define REG_RAWI2_R6_ERR_STAT				0x4810
#define REG_IMGO_R1_ERR_STAT				0x4850
#define REG_FHO_R1_ERR_STAT				0x4900
#define REG_AAHO_R1_ERR_STAT				0x49B0
#define REG_PDO_R1_ERR_STAT				0x4A60
#define REG_AAO_R1_ERR_STAT				0x4B10
#define REG_AFO_R1_ERR_STAT				0x4BC0
#define REG_TSFSO_R1_ERR_STAT				0x4C70
#define REG_LTMSO_R1_ERR_STAT				0x4CB0
#define REG_FLKO_R1_ERR_STAT				0x4CF0
#define REG_UFEO_R1_ERR_STAT				0x4D30
#define REG_TSFSO_R2_ERR_STAT				0x4DB0
/* error status, yuv base */
#define REG_YUVO_R1_ERR_STAT				0x4230
#define REG_YUVBO_R1_ERR_STAT				0x42E0
#define REG_YUVCO_R1_ERR_STAT				0x4390
#define REG_YUVDO_R1_ERR_STAT				0x4440
#define REG_YUVO_R3_ERR_STAT				0x44F0
#define REG_YUVBO_R3_ERR_STAT				0x45A0
#define REG_YUVCO_R3_ERR_STAT				0x4650
#define REG_YUVDO_R3_ERR_STAT				0x4700
#define REG_YUVO_R2_ERR_STAT				0x47B0
#define REG_YUVBO_R2_ERR_STAT				0x47F0
#define REG_YUVO_R4_ERR_STAT				0x4830
#define REG_YUVBO_R4_ERR_STAT				0x4870
#define REG_RZH1N2TO_R1_ERR_STAT			0x48B0
#define REG_RZH1N2TBO_R1_ERR_STAT			0x48F0
#define REG_RZH1N2TO_R2_ERR_STAT			0x4930
#define REG_RZH1N2TO_R3_ERR_STAT			0x4970
#define REG_RZH1N2TBO_R3_ERR_STAT			0x49B0
#define REG_DRZS4NO_R1_ERR_STAT				0x49F0
#define REG_DRZS4NO_R2_ERR_STAT				0x4A30
#define REG_DRZS4NO_R3_ERR_STAT				0x4A70
#define REG_ACTSO_R1_ERR_STAT				0x4AF0
#define REG_TNCSO_R1_ERR_STAT				0x4B30
#define REG_TNCSBO_R1_ERR_STAT				0x4B70
#define REG_TNCSHO_R1_ERR_STAT				0x4BB0
#define REG_TNCSYO_R1_ERR_STAT				0x4BF0
#define REG_YUVO_R5_ERR_STAT				0x4C30
#define REG_YUVBO_R5_ERR_STAT				0x4C70

/* CQ related */
#define REG_CQ_EN					0x0400
#define REG_SCQ_START_PERIOD				0x0408
#define REG_CQ_THR0_CTL					0x0414
#define REG_CQ_THR0_BASEADDR				0x0418
#define REG_CQ_THR0_DESC_SIZE				0x041C
#define REG_CQ_SUB_CQ_EN			        0x060C
#define REG_CQ_SUB_THR0_CTL				0x061C
#define REG_CQ_SUB_THR0_BASEADDR_2			0x0624
#define REG_CQ_SUB_THR0_DESC_SIZE_2			0x062C
#define REG_CQ_SUB_THR0_BASEADDR_1			0x0620
#define REG_CQ_SUB_THR0_DESC_SIZE_1			0x0628

#define CQ_DB_EN					BIT(4)
#define CQ_DB_LOAD_MODE					BIT(8)
#define CQ_RESET					BIT(16)
#define CTL_CQ_THR0_START				BIT(0)
#define CQ_THR0_MODE_IMMEDIATE				BIT(4)
#define CQ_THR0_MODE_CONTINUOUS				BIT(5)
#define CQ_THR0_DONE_SEL				BIT(8)
#define SCQ_EN						BIT(20)
#define SCQ_SUBSAMPLE_EN                                BIT(21)
#define SCQ_SUB_RESET					BIT(16)

#define CQ_THR0_EN					BIT(0)
#define CQ_CQI_R1_EN					BIT(8)
#define CQ_CQI_R2_EN					BIT(9)
#define CAMCQ_SCQ_EN					BIT(20)
#define PASS1_DONE_SEL					BIT(31)

/* camsys */
#define REG_CAMSYS_CG_SET				0x0004
#define REG_CAMSYS_CG_CLR				0x0008
#define REG_CTL_START					0x00B0

/* camctl */
#define REG_CTL_RAWI_TRIG				0x00C0
#define REG_CTL_SW_CTL					0x00C4
#define REG_CTL_SW_PASS1_DONE				0x00C8
#define REG_CTL_SW_SUB_CTL				0x00CC

/* TG */
#define REG_TG_SEN_MODE					0x0700
#define TG_SEN_MODE_CMOS_EN				BIT(0)

#define REG_TG_VF_CON					0x0704
#define TG_VFDATA_EN					BIT(0)
#define REG_TG_INTER_ST					0x073C
#define TG_CAM_CS_MASK					0x3f00
#define TG_IDLE_ST					BIT(8)

/* DBG */
#define CAM_REG_CQ_SUB_THR0_BASEADDR_1(regs)			(regs + 0x0620)
#define CAM_REG_CQ_SUB_THR0_BASEADDR_2(regs)			(regs + 0x0624)
#define CAM_REG_SUB_THR0_DESC_SIZE_1(regs)			(regs + 0x0628)
#define CAM_REG_SUB_THR0_DESC_SIZE_2(regs)			(regs + 0x062C)
#define CAM_REG_IMGO_ORIWDMA_BASE_ADDR(regs)			(regs + 0x4820)
#define CAM_REG_AAO_ORIWDMA_BASE_ADDR(regs)			(regs + 0x4AE0)
#define CAM_REG_YUVO_R1_ORIWDMA_BASE_ADDR(regs)			(regs + 0x4200)
#define CAM_REG_YUVO_R3_ORIWDMA_BASE_ADDR(regs)			(regs + 0x44C0)

#define CAM_REG_FBC_IMGO_R1_CTL2(regs)			        (regs + 0x2C04)
#define CAM_REG_FBC_YUVO_R1_CTL2(regs)			        (regs + 0x3784)
#define CAM_REG_FBC_YUVO_R3_CTL2(regs)			        (regs + 0x37A4)

#define CAM_REG_CQ_EN(regs)				        (regs + 0x0400)
#define CAM_REG_SUB_CQ_EN(regs)				        (regs + 0x060C)

#define CAM_REG_SW_PASS1_DONE(regs)			        (regs + 0x00C8)
#define CAM_REG_SW_SUB_CTL(regs)			        (regs + 0x00CC)

#define CAM_REG_FBC_AAO_R1_CTL1(regs)			        (regs + 0x2C20)
#define CAM_REG_FBC_AAHO_R1_CTL1(regs)			        (regs + 0x2C10)
#define CAM_REG_FBC_AAO_R1_CTL2(regs)			        (regs + 0x2C24)
#define CAM_REG_FBC_AAHO_R1_CTL2(regs)			        (regs + 0x2C14)


