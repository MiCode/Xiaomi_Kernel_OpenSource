/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __FG_REG_H__
#define __FG_REG_H__

/* FG_ADC_RR register definitions used only for READ */
#define ADC_RR_FAKE_BATT_LOW_LSB(chip)		(chip->rradc_base + 0x58)
#define ADC_RR_FAKE_BATT_HIGH_LSB(chip)		(chip->rradc_base + 0x5A)

/* FG_BATT_SOC register definitions */
#define BATT_SOC_FG_ALG_STS(chip)		(chip->batt_soc_base + 0x06)
#define BATT_SOC_FG_ALG_AUX_STS0(chip)		(chip->batt_soc_base + 0x07)
#define BATT_SOC_SLEEP_SHUTDOWN_STS(chip)	(chip->batt_soc_base + 0x08)
#define BATT_SOC_FG_MONOTONIC_SOC(chip)		(chip->batt_soc_base + 0x09)
#define BATT_SOC_FG_MONOTONIC_SOC_CP(chip)	(chip->batt_soc_base + 0x0A)
#define BATT_SOC_INT_RT_STS(chip)		(chip->batt_soc_base + 0x10)
#define BATT_SOC_EN_CTL(chip)			(chip->batt_soc_base + 0x46)
#define BATT_SOC_RESTART(chip)			(chip->batt_soc_base + 0x48)
#define BATT_SOC_STS_CLR(chip)			(chip->batt_soc_base + 0x4A)
#define BATT_SOC_LOW_PWR_CFG(chip)		(chip->batt_soc_base + 0x52)
#define BATT_SOC_LOW_PWR_STS(chip)		(chip->batt_soc_base + 0x56)
#define BATT_SOC_RST_CTRL0(chip)		(chip->batt_soc_base + 0xBA)

/* BATT_SOC_INT_RT_STS */
#define MSOC_EMPTY_BIT				BIT(5)

/* BATT_SOC_EN_CTL */
#define FG_ALGORITHM_EN_BIT			BIT(7)

/* BATT_SOC_RESTART */
#define RESTART_GO_BIT				BIT(0)

/* BCL_RESET */
#define BCL_RESET_BIT				BIT(2)

/* FG_BATT_INFO register definitions */
#define BATT_INFO_BATT_TEMP_STS(chip)		(chip->batt_info_base + 0x06)
#define BATT_INFO_SYS_BATT(chip)		(chip->batt_info_base + 0x07)
#define BATT_INFO_FG_STS(chip)			(chip->batt_info_base + 0x09)
#define BATT_INFO_INT_RT_STS(chip)		(chip->batt_info_base + 0x10)
#define BATT_INFO_BATT_REM_LATCH(chip)		(chip->batt_info_base + 0x4F)
#define BATT_INFO_BATT_TEMP_LSB(chip)		(chip->batt_info_base + 0x50)
#define BATT_INFO_BATT_TEMP_MSB(chip)		(chip->batt_info_base + 0x51)
#define BATT_INFO_BATT_TEMP_CFG(chip)		(chip->batt_info_base + 0x56)
#define BATT_INFO_BATT_TMPR_INTR(chip)		(chip->batt_info_base + 0x59)
#define BATT_INFO_THERM_C1(chip)		(chip->batt_info_base + 0x5C)
#define BATT_INFO_THERM_C2(chip)		(chip->batt_info_base + 0x5D)
#define BATT_INFO_THERM_C3(chip)		(chip->batt_info_base + 0x5E)
#define BATT_INFO_THERM_HALF_RANGE(chip)	(chip->batt_info_base + 0x5F)
#define BATT_INFO_JEITA_CTLS(chip)		(chip->batt_info_base + 0x61)
#define BATT_INFO_JEITA_TOO_COLD(chip)		(chip->batt_info_base + 0x62)
#define BATT_INFO_JEITA_COLD(chip)		(chip->batt_info_base + 0x63)
#define BATT_INFO_JEITA_HOT(chip)		(chip->batt_info_base + 0x64)
#define BATT_INFO_JEITA_TOO_HOT(chip)		(chip->batt_info_base + 0x65)

/* only for v1.1 */
#define BATT_INFO_ESR_CFG(chip)			(chip->batt_info_base + 0x69)
/* starting from v2.0 */
#define BATT_INFO_ESR_GENERAL_CFG(chip)		(chip->batt_info_base + 0x68)
#define BATT_INFO_ESR_PULL_DN_CFG(chip)		(chip->batt_info_base + 0x69)
#define BATT_INFO_ESR_FAST_CRG_CFG(chip)	(chip->batt_info_base + 0x6A)

#define BATT_INFO_BATT_MISS_CFG(chip)		(chip->batt_info_base + 0x6B)
#define BATT_INFO_WATCHDOG_COUNT(chip)		(chip->batt_info_base + 0x70)
#define BATT_INFO_WATCHDOG_CFG(chip)		(chip->batt_info_base + 0x71)
#define BATT_INFO_IBATT_SENSING_CFG(chip)	(chip->batt_info_base + 0x73)
#define BATT_INFO_QNOVO_CFG(chip)		(chip->batt_info_base + 0x74)
#define BATT_INFO_QNOVO_SCALER(chip)		(chip->batt_info_base + 0x75)

/* starting from v2.0 */
#define BATT_INFO_CRG_SERVICES(chip)		(chip->batt_info_base + 0x90)

/* Following LSB/MSB address are for v2.0 and above; v1.1 have them swapped */
#define BATT_INFO_VBATT_LSB(chip)		(chip->batt_info_base + 0xA0)
#define BATT_INFO_VBATT_MSB(chip)		(chip->batt_info_base + 0xA1)
#define BATT_INFO_IBATT_LSB(chip)		(chip->batt_info_base + 0xA2)
#define BATT_INFO_IBATT_MSB(chip)		(chip->batt_info_base + 0xA3)
#define BATT_INFO_ESR_LSB(chip)			(chip->batt_info_base + 0xA4)
#define BATT_INFO_ESR_MSB(chip)			(chip->batt_info_base + 0xA5)
#define BATT_INFO_VBATT_LSB_CP(chip)		(chip->batt_info_base + 0xA6)
#define BATT_INFO_VBATT_MSB_CP(chip)		(chip->batt_info_base + 0xA7)
#define BATT_INFO_IBATT_LSB_CP(chip)		(chip->batt_info_base + 0xA8)
#define BATT_INFO_IBATT_MSB_CP(chip)		(chip->batt_info_base + 0xA9)
#define BATT_INFO_ESR_LSB_CP(chip)		(chip->batt_info_base + 0xAA)
#define BATT_INFO_ESR_MSB_CP(chip)		(chip->batt_info_base + 0xAB)
#define BATT_INFO_VADC_LSB(chip)		(chip->batt_info_base + 0xAC)
#define BATT_INFO_VADC_MSB(chip)		(chip->batt_info_base + 0xAD)
#define BATT_INFO_IADC_LSB(chip)		(chip->batt_info_base + 0xAE)
#define BATT_INFO_IADC_MSB(chip)		(chip->batt_info_base + 0xAF)
#define BATT_INFO_TM_MISC(chip)			(chip->batt_info_base + 0xE5)
#define BATT_INFO_TM_MISC1(chip)		(chip->batt_info_base + 0xE6)
#define BATT_INFO_PEEK_MUX1(chip)		(chip->batt_info_base + 0xEB)
#define BATT_INFO_RDBACK(chip)			(chip->batt_info_base + 0xEF)

/* BATT_INFO_BATT_TEMP_STS */
#define JEITA_TOO_HOT_STS_BIT			BIT(7)
#define JEITA_HOT_STS_BIT			BIT(6)
#define JEITA_COLD_STS_BIT			BIT(5)
#define JEITA_TOO_COLD_STS_BIT			BIT(4)
#define BATT_TEMP_DELTA_BIT			BIT(1)
#define BATT_TEMP_AVAIL_BIT			BIT(0)

/* BATT_INFO_SYS_BATT */
#define BATT_REM_LATCH_STS_BIT			BIT(4)
#define BATT_MISSING_HW_BIT			BIT(2)
#define BATT_MISSING_ALG_BIT			BIT(1)
#define BATT_MISSING_CMP_BIT			BIT(0)

/* BATT_INFO_FG_STS */
#define FG_WD_RESET_BIT				BIT(7)
/* This bit is not present in v1.1 */
#define FG_CRG_TRM_BIT				BIT(0)

/* BATT_INFO_INT_RT_STS */
#define BT_TMPR_DELTA_BIT			BIT(6)
#define WDOG_EXP_BIT				BIT(5)
#define BT_ATTN_BIT				BIT(4)
#define BT_MISS_BIT				BIT(3)
#define ESR_DELTA_BIT				BIT(2)
#define VBT_LOW_BIT				BIT(1)
#define VBT_PRD_DELTA_BIT			BIT(0)

/* BATT_INFO_INT_RT_STS */
#define BATT_REM_LATCH_CLR_BIT			BIT(7)

/* BATT_INFO_BATT_TEMP_LSB/MSB */
#define BATT_TEMP_LSB_MASK			GENMASK(7, 0)
#define BATT_TEMP_MSB_MASK			GENMASK(2, 0)

/* BATT_INFO_BATT_TEMP_CFG */
#define JEITA_TEMP_HYST_MASK			GENMASK(5, 4)
#define JEITA_TEMP_HYST_SHIFT			4
#define JEITA_TEMP_NO_HYST			0x0
#define JEITA_TEMP_HYST_1C			0x1
#define JEITA_TEMP_HYST_2C			0x2
#define JEITA_TEMP_HYST_3C			0x3

/* BATT_INFO_BATT_TMPR_INTR */
#define CHANGE_THOLD_MASK			GENMASK(1, 0)
#define BTEMP_DELTA_2K				0x0
#define BTEMP_DELTA_4K				0x1
#define BTEMP_DELTA_6K				0x2
#define BTEMP_DELTA_10K				0x3

/* BATT_INFO_THERM_C1/C2/C3 */
#define BATT_INFO_THERM_COEFF_MASK		GENMASK(7, 0)

/* BATT_INFO_THERM_HALF_RANGE */
#define BATT_INFO_THERM_TEMP_MASK		GENMASK(7, 0)

/* BATT_INFO_JEITA_CTLS */
#define JEITA_STS_CLEAR_BIT			BIT(0)

/* BATT_INFO_JEITA_TOO_COLD/COLD/HOT/TOO_HOT */
#define JEITA_THOLD_MASK			GENMASK(7, 0)

/* BATT_INFO_ESR_CFG */
#define CFG_ACTIVE_PD_MASK			GENMASK(2, 1)
#define CFG_FCC_DEC_MASK			GENMASK(4, 3)

/* BATT_INFO_ESR_GENERAL_CFG */
#define ESR_DEEP_TAPER_EN_BIT			BIT(0)

/* BATT_INFO_ESR_PULL_DN_CFG */
#define ESR_PULL_DOWN_IVAL_MASK			GENMASK(3, 2)
#define ESR_PULL_DOWN_IVAL_SHIFT		2
#define ESR_MEAS_CUR_60MA			0x0
#define ESR_MEAS_CUR_120MA			0x1
#define ESR_MEAS_CUR_180MA			0x2
#define ESR_MEAS_CUR_240MA			0x3
#define ESR_PULL_DOWN_MODE_MASK			GENMASK(1, 0)
#define ESR_NO_PULL_DOWN			0x0
#define ESR_STATIC_PULL_DOWN			0x1
#define ESR_CRG_DSC_PULL_DOWN			0x2
#define ESR_DSC_PULL_DOWN			0x3

/* BATT_INFO_ESR_FAST_CRG_CFG */
#define ESR_FAST_CRG_IVAL_MASK			GENMASK(3, 1)
#define ESR_FCC_300MA				0x0
#define ESR_FCC_600MA				0x1
#define ESR_FCC_1A				0x2
#define ESR_FCC_2A				0x3
#define ESR_FCC_3A				0x4
#define ESR_FCC_4A				0x5
#define ESR_FCC_5A				0x6
#define ESR_FCC_6A				0x7
#define ESR_FAST_CRG_CTL_EN_BIT			BIT(0)

/* BATT_INFO_BATT_MISS_CFG */
#define BM_THERM_TH_MASK			GENMASK(5, 4)
#define RES_TH_0P75_MOHM			0x0
#define RES_TH_1P00_MOHM			0x1
#define RES_TH_1P50_MOHM			0x2
#define RES_TH_3P00_MOHM			0x3
#define BM_BATT_ID_TH_MASK			GENMASK(3, 2)
#define BM_FROM_THERM_BIT			BIT(1)
#define BM_FROM_BATT_ID_BIT			BIT(0)

/* BATT_INFO_WATCHDOG_COUNT */
#define WATCHDOG_COUNTER			GENMASK(7, 0)

/* BATT_INFO_WATCHDOG_CFG */
#define RESET_CAPABLE_BIT			BIT(2)
#define PET_CTRL_BIT				BIT(1)
#define ENABLE_CTRL_BIT				BIT(0)

/* BATT_INFO_IBATT_SENSING_CFG */
#define ADC_BITSTREAM_INV_BIT			BIT(4)
#define SOURCE_SELECT_MASK			GENMASK(1, 0)
#define SRC_SEL_BATFET				0x0
#define SRC_SEL_BATFET_SMB			0x2
#define SRC_SEL_RESERVED			0x3

/* BATT_INFO_QNOVO_CFG */
#define LD_REG_FORCE_CTL_BIT			BIT(2)
#define LD_REG_CTRL_FORCE_HIGH			LD_REG_FORCE_CTL_BIT
#define LD_REG_CTRL_FORCE_LOW			0
#define LD_REG_CTRL_BIT				BIT(1)
#define LD_REG_CTRL_REGISTER			LD_REG_CTRL_BIT
#define LD_REG_CTRL_LOGIC			0
#define BIT_STREAM_CFG_BIT			BIT(0)

/* BATT_INFO_QNOVO_SCALER */
#define QNOVO_SCALER_MASK			GENMASK(7, 0)

/* BATT_INFO_CRG_SERVICES */
#define FG_CRC_TRM_EN_BIT			BIT(0)

/* BATT_INFO_VBATT_LSB/MSB */
#define VBATT_MASK				GENMASK(7, 0)

/* BATT_INFO_IBATT_LSB/MSB */
#define IBATT_MASK				GENMASK(7, 0)

/* BATT_INFO_ESR_LSB/MSB */
#define ESR_LSB_MASK				GENMASK(7, 0)
#define ESR_MSB_MASK				GENMASK(5, 0)

/* BATT_INFO_VADC_LSB/MSB */
#define VADC_LSB_MASK				GENMASK(7, 0)
#define VADC_MSB_MASK				GENMASK(6, 0)

/* BATT_INFO_IADC_LSB/MSB */
#define IADC_LSB_MASK				GENMASK(7, 0)
#define IADC_MSB_MASK				GENMASK(6, 0)

/* BATT_INFO_TM_MISC */
#define FORCE_SEQ_RESP_TOGGLE_BIT		BIT(6)
#define ALG_DIRECT_VALID_DATA_BIT		BIT(5)
#define ALG_DIRECT_MODE_EN_BIT			BIT(4)
#define BATT_VADC_CONV_BIT			BIT(3)
#define BATT_IADC_CONV_BIT			BIT(2)
#define ADC_ENABLE_REG_CTRL_BIT			BIT(1)
#define WDOG_FORCE_EXP_BIT			BIT(0)
/* only for v1.1 */
#define ESR_PULSE_FORCE_CTRL_BIT		BIT(7)

/* BATT_INFO_TM_MISC1 */
/* for v2.0 and above */
#define ESR_REQ_CTL_BIT				BIT(1)
#define ESR_REQ_CTL_EN_BIT			BIT(0)

/* BATT_INFO_PEEK_MUX1 */
#define PEEK_MUX1_BIT				BIT(0)

/* FG_MEM_IF register and bit definitions */
#define MEM_IF_INT_RT_STS(chip)			((chip->mem_if_base) + 0x10)
#define MEM_IF_MEM_ARB_CFG(chip)		((chip->mem_if_base) + 0x40)
#define MEM_IF_MEM_INTF_CFG(chip)		((chip->mem_if_base) + 0x50)
#define MEM_IF_IMA_CTL(chip)			((chip->mem_if_base) + 0x51)
#define MEM_IF_IMA_CFG(chip)			((chip->mem_if_base) + 0x52)
#define MEM_IF_IMA_OPR_STS(chip)		((chip->mem_if_base) + 0x54)
#define MEM_IF_IMA_EXP_STS(chip)		((chip->mem_if_base) + 0x55)
#define MEM_IF_IMA_HW_STS(chip)			((chip->mem_if_base) + 0x56)
#define MEM_IF_FG_BEAT_COUNT(chip)		((chip->mem_if_base) + 0x57)
#define MEM_IF_IMA_ERR_STS(chip)		((chip->mem_if_base) + 0x5F)
#define MEM_IF_IMA_BYTE_EN(chip)		((chip->mem_if_base) + 0x60)
#define MEM_IF_ADDR_LSB(chip)			((chip->mem_if_base) + 0x61)
#define MEM_IF_ADDR_MSB(chip)			((chip->mem_if_base) + 0x62)
#define MEM_IF_WR_DATA0(chip)			((chip->mem_if_base) + 0x63)
#define MEM_IF_WR_DATA3(chip)			((chip->mem_if_base) + 0x66)
#define MEM_IF_RD_DATA0(chip)			((chip->mem_if_base) + 0x67)
#define MEM_IF_RD_DATA3(chip)			((chip->mem_if_base) + 0x6A)
#define MEM_IF_DMA_STS(chip)			((chip->mem_if_base) + 0x70)
#define MEM_IF_DMA_CTL(chip)			((chip->mem_if_base) + 0x71)

/* MEM_IF_INT_RT_STS */
#define MEM_XCP_BIT				BIT(1)
#define MEM_GNT_BIT				BIT(2)

/* MEM_IF_MEM_INTF_CFG */
#define MEM_ACCESS_REQ_BIT			BIT(7)
#define IACS_SLCT_BIT				BIT(5)

/* MEM_IF_IMA_CTL */
#define MEM_ACS_BURST_BIT			BIT(7)
#define IMA_WR_EN_BIT				BIT(6)
#define IMA_CTL_MASK				GENMASK(7, 6)

/* MEM_IF_IMA_CFG */
#define IACS_CLR_BIT				BIT(2)
#define IACS_INTR_SRC_SLCT_BIT			BIT(3)
#define STATIC_CLK_EN_BIT			BIT(4)

/* MEM_IF_IMA_OPR_STS */
#define IACS_RDY_BIT				BIT(1)

/* MEM_IF_IMA_EXP_STS */
#define IACS_ERR_BIT				BIT(0)
#define XCT_TYPE_ERR_BIT			BIT(1)
#define DATA_RD_ERR_BIT				BIT(3)
#define DATA_WR_ERR_BIT				BIT(4)
#define ADDR_BURST_WRAP_BIT			BIT(5)
#define ADDR_STABLE_ERR_BIT			BIT(7)

/* MEM_IF_IMA_ERR_STS */
#define ADDR_STBL_ERR_BIT			BIT(7)
#define WR_ACS_ERR_BIT				BIT(6)
#define RD_ACS_ERR_BIT				BIT(5)

/* MEM_IF_FG_BEAT_COUNT */
#define BEAT_COUNT_MASK				GENMASK(3, 0)

/* MEM_IF_DMA_STS */
#define DMA_WRITE_ERROR_BIT			BIT(1)
#define DMA_READ_ERROR_BIT			BIT(2)

/* MEM_IF_DMA_CTL */
#define DMA_CLEAR_LOG_BIT			BIT(0)
/* MEM_IF_REQ */
#define MEM_IF_ARB_REQ_BIT			BIT(0)
#endif
