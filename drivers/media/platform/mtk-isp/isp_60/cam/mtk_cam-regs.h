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

#endif	/* _CAM_REGS_H */
