/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _CAM_REGS_H
#define _CAM_REGS_H

/* normal signal bit */
#define VS_INT_ST			BIT(0)
#define HW_PASS1_DON_ST			BIT(14)
#define SOF_INT_ST			BIT(6)
#define SW_PASS1_DON_ST			BIT(16)

/* FIXME: ISP 6S as no this field */
#define DMA_ERR_INT_EN			BIT(29)

/* err status bit */
#define TG_ERR_ST			BIT(4)
#define TG_GBERR_ST			BIT(5)
#define CQ_CODE_ERR_ST			BIT(10)
#define CQ_DB_LOAD_ERR_ST		BIT(11)
#define CQ_VS_ERR_ST			BIT(12)
#define IMGO_ERR_ST			BIT(0)
#define RRZO_ERR_ST			BIT(2)
#define AAO_ERR_ST			BIT(8)
#define AFO_ERR_ST			BIT(10)
#define LCSO_ERR_ST			BIT(13)
#define DMA_ERR_ST			BIT(23)

/* CAM DMA done status */
#define AFO_DONE_ST			BIT(10)
#define AAO_DONE_ST			BIT(8)

/* IRQ signal mask */
#define INT_ST_MASK_CAM			( \
					VS_INT_ST |\
					SOF_INT_ST |\
					HW_PASS1_DON_ST |\
					SW_PASS1_DON_ST)

/* IRQ Error Mask */
#define INT_ST_MASK_CAM_ERR		( \
					TG_ERR_ST |\
					TG_GBERR_ST |\
					CQ_CODE_ERR_ST |\
					CQ_VS_ERR_ST |\
					DMA_ERR_ST)

/* IRQ Signal Log Mask */
#define INT_ST_LOG_MASK_CAM		( \
					SOF_INT_ST |\
					SW_PASS1_DON_ST |\
					HW_PASS1_DON_ST |\
					VS_INT_ST |\
					TG_ERR_ST |\
					TG_GBERR_ST |\
					RRZO_ERR_ST |\
					AFO_ERR_ST |\
					IMGO_ERR_ST |\
					AAO_ERR_ST |\
					DMA_ERR_ST)

/* DMA Event Notification Mask */
#define DMA_ST_MASK_CAM			( \
					AAO_DONE_ST |\
					AFO_DONE_ST)

/* Status check */
#define REG_CTL_EN			0x0004
#define REG_CTL_EN2			0x0008
#define REG_CTL_DMA_EN			0x0014
#define REG_CTL_SW_CTL			0x007C
#define REG_CTL_START			0x0074
#define REG_CTL_RAW_INT_STAT		0x0104
#define REG_CTL_RAW_INT2_EN         0x0110
#define REG_CTL_RAW_INT2_STAT		0x0114
#define REG_CTL_RAW_INT3_STAT		0x0124
#define REG_CTL_RAW_INT4_STAT		0x0134
#define REG_CTL_RAW_INT5_STAT		0x0144
#define REG_CTL_RAW_INT6_EN		0x0150
#define REG_CTL_RAW_INT6_STAT		0x0154

#define CAMCTL_CQ_THR0_DONE_ST		BIT(0)
#define REG_CTL_START_ST		0x0078
#define CQ_THR0_START			BIT(0)

#define REG_CAMCTL_FBC_RCNT_INC	0x0064
#define CAMCTL_IMGO_R1_RCNT_INC	BIT(0)

#define REG_CQ_EN			0x0200
#define REG_SCQ_START_PERIOD		0x0204
#define REG_SCQ_CQ_TRIG_TIME		0x0208
#define REG_TG_TIME_STAMP		0x1f70
#define REG_TG_TIME_STAMP_CTL		0x1f80

#define REG_CQ_THR0_CTL		0x0210
#define SCQ_EN				BIT(20)
#define CQ_DB_EN			BIT(4)
#define CQ_DB_LOAD_MODE		BIT(8)
#define CQ_RESET			BIT(16)
#define CTL_CQ_THR0_START		BIT(0)
#define CQ_THR0_MODE_IMMEDIATE		BIT(4)
#define CQ_THR0_MODE_CONTINUOUS	BIT(5)
#define CQ_THR0_EN			BIT(0)

#define REG_TG_SEN_MODE		0x1F00
#define TG_SEN_MODE_CMOS_EN		BIT(0)
#define TG_CMOS_RDY_SEL		BIT(14)

#define REG_TG_VF_CON			0x1F04
#define REG_TG_SEN_GRAB_PXL		0x1F08
#define REG_TG_SEN_GRAB_LIN		0x1F0c

#define TG_VF_CON_VFDATA_EN		BIT(0)
#define REG_TG_PATH_CFG		0x1F10
#define TG_TG_FULL_SEL			BIT(15)

#define REG_TG_FRMSIZE_ST		0x1F38
#define REG_TG_FRMSIZE_ST_R		0x1F7C
#define REG_TG_INTER_ST		0x1F3C
#define REG_TG_TIME_STAMP_CNT		0x1F64

#define TG_CS_MASK			0x3f00
#define TG_IDLE_ST			BIT(8)

#define REG_IMGO_BASE_ADDR		0x4820

#define REG_RRZO_BASE_ADDR		0x4900

/* Error status log */
#define REG_IMGO_ERR_STAT		0x4068
#define REG_RRZO_ERR_STAT		0x4070
#define REG_AAO_ERR_STAT		0x4048
#define REG_AFO_ERR_STAT		0x404C
#define REG_LCSO_ERR_STAT		0x4058
#define REG_UFEO_ERR_STAT		0x406C
#define REG_PDO_ERR_STAT		0x4028
#define REG_BPCI_ERR_STAT		0x402C
#define REG_LSCI_ERR_STAT		0x403C
#define REG_PDI_ERR_STAT		0x4024
#define REG_LMVO_ERR_STAT		0x4064
#define REG_FLKO_ERR_STAT		0x4050

/* ISP command */
#define REG_CQ_THR0_BASEADDR		0x0214
#define REG_CQ_THR0_DESC_SIZE		0x0218
#define REG_FRAME_SEQ_NUM		0x4850

/* META */
#define REG_META0_VB2_INDEX		0x44F8
#define REG_META1_VB2_INDEX		0x4568

/* FBC */
#define REG_AAO_FBC_STATUS		0x0E3C
#define REG_AFO_FBC_STATUS		0x0E34

/*DMA fifo*/
#define REG_IMGO_BASE_ADDR	0x4820
#define REG_IMGO_CON		0x4838
#define REG_IMGO_CON2		0x483c
#define REG_IMGO_CON3		0x4840
#define REG_IMGO_DRS		0x4828

#define REG_RRZO_BASE_ADDR	0x4900
#define REG_RRZO_CON		0x4918
#define REG_RRZO_CON2		0x491c
#define REG_RRZO_CON3		0x4920
#define REG_RRZO_DRS		0x4908

#define REG_PDO_DRS         0x4268
#define REG_PDO_CON         0x4278
#define REG_PDO_CON2        0x427C
#define REG_PDO_CON3        0x4280

#define REG_TSFSO_DRS       0x43C8
#define REG_TSFSO_CON       0x43D8
#define REG_TSFSO_CON2      0x43DC
#define REG_TSFSO_CON3      0x43E0

#define REG_AAO_DRS         0x44A8
#define REG_AAO_CON         0x44B8
#define REG_AAO_CON2        0x44BC
#define REG_AAO_CON3        0x44C0

#define REG_AAHO_DRS        0x4438
#define REG_AAHO_CON        0x4448
#define REG_AAHO_CON2       0x444C
#define REG_AAHO_CON3       0x4450

#define REG_AFO_DRS         0x4518
#define REG_AFO_CON         0x4528
#define REG_AFO_CON2        0x452C
#define REG_AFO_CON3        0x4530

#define REG_FLKO_DRS        0x4588
#define REG_FLKO_CON        0x4598
#define REG_FLKO_CON2       0x459C
#define REG_FLKO_CON3       0x45A0

#define REG_LTMSO_DRS       0x45F8
#define REG_LTMSO_CON       0x4608
#define REG_LTMSO_CON2      0x460C
#define REG_LTMSO_CON3      0x4610

#define REG_LCESO_DRS       0x4668
#define REG_LCESO_CON       0x4678
#define REG_LCESO_CON2      0x467C
#define REG_LCESO_CON3      0x4680

#define REG_LCESHO_DRS      0x46D8
#define REG_LCESHO_CON      0x46E8
#define REG_LCESHO_CON2     0x46EC
#define REG_LCESHO_CON3     0x46F0

#define REG_RSSO_DRS        0x4748
#define REG_RSSO_CON        0x4758
#define REG_RSSO_CON2       0x475C
#define REG_RSSO_CON3       0x4760

#define REG_LMVO_DRS        0x47B8
#define REG_LMVO_CON        0x47C8
#define REG_LMVO_CON2       0x47CC
#define REG_LMVO_CON3       0x47D0

#define REG_UFEO_DRS        0x4898
#define REG_UFEO_CON        0x48A8
#define REG_UFEO_CON2       0x48AC
#define REG_UFEO_CON3       0x48B0

#define REG_UFGO_DRS        0x4978
#define REG_UFGO_CON        0x4988
#define REG_UFGO_CON2       0x498C
#define REG_UFGO_CON3       0x4990

#define REG_YUVO_DRS        0x49E8
#define REG_YUVO_CON        0x49F8
#define REG_YUVO_CON2       0x49FC
#define REG_YUVO_CON3       0x4A00

#define REG_YUVBO_DRS       0x4A58
#define REG_YUVBO_CON       0x4A68
#define REG_YUVBO_CON2      0x4A6C
#define REG_YUVBO_CON3      0x4A70

#define REG_YUVCO_DRS       0x4AC8
#define REG_YUVCO_CON       0x4AD8
#define REG_YUVCO_CON2      0x4ADC
#define REG_YUVCO_CON3      0x4AE0

#define REG_CRZO_DRS        0x4B38
#define REG_CRZO_CON        0x4B48
#define REG_CRZO_CON2       0x4B4C
#define REG_CRZO_CON3       0x4B50

#define REG_CRZBO_DRS       0x4BA8
#define REG_CRZBO_CON       0x4BB8
#define REG_CRZBO_CON2      0x4BBC
#define REG_CRZBO_CON3      0x4BC0

#define REG_CRZO_R2_DRS     0x4C18
#define REG_CRZO_R2_CON     0x4C28
#define REG_CRZO_R2_CON2    0x4C2C
#define REG_CRZO_R2_CON3    0x4C30

#define REG_RSSO_R2_DRS     0x4CF8
#define REG_RSSO_R2_CON     0x4D08
#define REG_RSSO_R2_CON2    0x4D0C
#define REG_RSSO_R2_CON3    0x4D10

#define REG_RAWI_R2_DRS     0x4208
#define REG_RAWI_R2_CON     0x4218
#define REG_RAWI_R2_CON2    0x421C
#define REG_RAWI_R2_CON3    0x4220
#define REG_RAWI_R2_CON4    0x4228

#define REG_RAWI_R3_DRS     0x5208
#define REG_RAWI_R3_CON     0x5218
#define REG_RAWI_R3_CON2    0x521C
#define REG_RAWI_R3_CON3    0x5220
#define REG_RAWI_R3_CON4    0x5228

#define REG_UFDI_R2_DRS     0x4368
#define REG_UFDI_R2_CON     0x4378
#define REG_UFDI_R2_CON2    0x437C
#define REG_UFDI_R2_CON3    0x4380
#define REG_UFDI_R2_CON4    0x4388

#define REG_PDI_DRS         0x4238
#define REG_PDI_CON         0x4248
#define REG_PDI_CON2        0x424C
#define REG_PDI_CON3        0x4250
#define REG_PDI_CON4        0x4258

#define REG_BPCI_ADDR       0x42D0
#define REG_BPCI_DRS        0x42D8
#define REG_BPCI_CON        0x42E8
#define REG_BPCI_CON2       0x42EC
#define REG_BPCI_CON3       0x42F0
#define REG_BPCI_CON4       0x42F8

#define REG_BPCI_R2_DRS     0x4308
#define REG_BPCI_R2_CON     0x4318
#define REG_BPCI_R2_CON2    0x431C
#define REG_BPCI_R2_CON3    0x4320
#define REG_BPCI_R2_CON4    0x4328

#define REG_BPCI_R3_DRS     0x4338
#define REG_BPCI_R3_CON     0x4348
#define REG_BPCI_R3_CON2    0x434C
#define REG_BPCI_R3_CON3    0x4350
#define REG_BPCI_R3_CON4    0x4358

#define REG_LSCI_DRS        0x4398
#define REG_LSCI_CON        0x43A8
#define REG_LSCI_CON2       0x43AC
#define REG_LSCI_CON3       0x43B0
#define REG_LSCI_CON4       0x43B8

#define REG_CQI_R1_DRS		0x5448
#define REG_CQI_R1_CON		0x5458
#define REG_CQI_R1_CON2	0x545C
#define REG_CQI_R1_CON3	0x5460
#define REG_CQI_R1_CON4	0x5468

#define REG_CQI_R2_DRS		0x5478
#define REG_CQI_R2_CON		0x5488
#define REG_CQI_R2_CON2	0x548C
#define REG_CQI_R2_CON3	0x5490
#define REG_CQI_R2_CON4	0x5498

#define REG_HALT1_EN		0x0350
#define REG_HALT2_EN		0x0354
#define REG_HALT3_EN		0x0358
#define REG_HALT4_EN		0x035C
#define REG_HALT1_SEC_EN	0x0360
#define REG_HALT2_SEC_EN	0x0364
#define REG_HALT3_SEC_EN	0x0368
#define REG_HALT4_SEC_EN	0x036C

#endif	/* _CAM_REGS_H */
