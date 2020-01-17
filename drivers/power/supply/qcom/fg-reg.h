/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016-2018, 2020, The Linux Foundation. All rights reserved.
 */

#ifndef __FG_REG_H__
#define __FG_REG_H__

/* FG_ADC_RR register definitions used only for READ */
#define ADC_RR_FAKE_BATT_LOW_LSB(chip)		(chip->rradc_base + 0x58)
#define ADC_RR_FAKE_BATT_HIGH_LSB(chip)		(chip->rradc_base + 0x5A)

/* GEN4 FG definitions for FG_ADC_RR */
#define ADC_RR_INT_RT_STS(chip)			(chip->rradc_base + 0x10)
#define ADC_RR_BT_MISS_BIT			BIT(0)

#define ADC_RR_BATT_ID_HI_BIAS_STS(chip)	(chip->rradc_base + 0x65)
#define BIAS_STS_READY				BIT(0)

#define ADC_RR_BATT_ID_HI_BIAS_LSB(chip)	(chip->rradc_base + 0x66)
#define ADC_RR_BATT_ID_HI_BIAS_MSB(chip)	(chip->rradc_base + 0x67)

#define ADC_RR_BATT_ID_MED_BIAS_STS(chip)	(chip->rradc_base + 0x6D)
#define ADC_RR_BATT_ID_MED_BIAS_LSB(chip)	(chip->rradc_base + 0x6E)
#define ADC_RR_BATT_ID_MED_BIAS_MSB(chip)	(chip->rradc_base + 0x6F)

#define ADC_RR_BATT_ID_LO_BIAS_STS(chip)	(chip->rradc_base + 0x75)
#define ADC_RR_BATT_ID_LO_BIAS_LSB(chip)	(chip->rradc_base + 0x76)
#define ADC_RR_BATT_ID_LO_BIAS_MSB(chip)	(chip->rradc_base + 0x77)

#define ADC_RR_BATT_THERM_BASE_CFG1(chip)	(chip->rradc_base + 0x81)
#define BATT_THERM_PULL_UP_30K			1
#define BATT_THERM_PULL_UP_100K			2
#define BATT_THERM_PULL_UP_400K			3
#define BATT_THERM_PULL_UP_MASK			GENMASK(1, 0)

#define ADC_RR_BATT_THERM_FREQ(chip)		(chip->rradc_base + 0x82)

#define ADC_RR_BATT_TEMP_LSB(chip)		(chip->rradc_base + 0x88)
#define ADC_RR_BATT_TEMP_MSB(chip)		(chip->rradc_base + 0x89)
#define GEN4_BATT_TEMP_MSB_MASK			GENMASK(1, 0)

/* FG_BATT_SOC register definitions */
#define BATT_SOC_FG_ALG_STS(chip)		(chip->batt_soc_base + 0x06)
#define BATT_SOC_FG_ALG_AUX_STS0(chip)		(chip->batt_soc_base + 0x07)
#define BATT_SOC_SLEEP_SHUTDOWN_STS(chip)	(chip->batt_soc_base + 0x08)
#define BATT_SOC_FG_MONOTONIC_SOC(chip)		(chip->batt_soc_base + 0x09)
#define BATT_SOC_FG_MONOTONIC_SOC_CP(chip)	(chip->batt_soc_base + 0x0A)
#define BATT_SOC_RST_CTRL0(chip)		(chip->batt_soc_base + 0xBA)

#define BATT_SOC_INT_RT_STS(chip)		(chip->batt_soc_base + 0x10)
#define SOC_READY_BIT				BIT(1)
#define MSOC_EMPTY_BIT				BIT(5)

#define BATT_SOC_EN_CTL(chip)			(chip->batt_soc_base + 0x46)
#define FG_ALGORITHM_EN_BIT			BIT(7)

#define BATT_SOC_RESTART(chip)			(chip->batt_soc_base + 0x48)
#define RESTART_GO_BIT				BIT(0)

#define BATT_SOC_STS_CLR(chip)			(chip->batt_soc_base + 0x4A)
#define BATT_SOC_LOW_PWR_CFG(chip)		(chip->batt_soc_base + 0x52)
#define BATT_SOC_LOW_PWR_STS(chip)		(chip->batt_soc_base + 0x56)
/* BATT_SOC_RST_CTRL0 */
#define BCL_RST_BIT				BIT(2)
#define MEM_RST_BIT				BIT(1)
#define ALG_RST_BIT				BIT(0)

/* FG_BATT_INFO register definitions */
#define BATT_INFO_BATT_TEMP_STS(chip)		(chip->batt_info_base + 0x06)
#define JEITA_TOO_HOT_STS_BIT			BIT(7)
#define JEITA_HOT_STS_BIT			BIT(6)
#define JEITA_COLD_STS_BIT			BIT(5)
#define JEITA_TOO_COLD_STS_BIT			BIT(4)
#define BATT_TEMP_DELTA_BIT			BIT(1)
#define BATT_TEMP_AVAIL_BIT			BIT(0)

#define BATT_INFO_SYS_BATT(chip)		(chip->batt_info_base + 0x07)
#define BATT_REM_LATCH_STS_BIT			BIT(4)
#define BATT_MISSING_HW_BIT			BIT(2)
#define BATT_MISSING_ALG_BIT			BIT(1)
#define BATT_MISSING_CMP_BIT			BIT(0)

#define BATT_INFO_FG_STS(chip)			(chip->batt_info_base + 0x09)
#define FG_WD_RESET_BIT				BIT(7)
#define FG_CRG_TRM_BIT				BIT(0)

#define BATT_INFO_INT_RT_STS(chip)		(chip->batt_info_base + 0x10)
#define BT_TMPR_DELTA_BIT			BIT(6)
#define WDOG_EXP_BIT				BIT(5)
#define BT_ATTN_BIT				BIT(4)
#define BT_MISS_BIT				BIT(3)
#define ESR_DELTA_BIT				BIT(2)
#define VBT_LOW_BIT				BIT(1)
#define VBT_PRD_DELTA_BIT			BIT(0)

/* GEN4 bit definitions */
#define GEN4_BT_ATTN_BIT			BIT(5)
#define GEN4_WDOG_EXP_BIT			BIT(4)
#define GEN4_ESR_DELTA_BIT			BIT(3)
#define GEN4_ESR_PULSE_PRE_BIT			BIT(2)
#define GEN4_VBT_PRD_DELTA_BIT			BIT(1)
#define GEN4_VBT_LOW_BIT			BIT(0)

#define BATT_INFO_BATT_REM_LATCH(chip)		(chip->batt_info_base + 0x4F)
#define BATT_REM_LATCH_CLR_BIT			BIT(7)

#define BATT_INFO_BATT_TEMP_LSB(chip)		(chip->batt_info_base + 0x50)
#define BATT_TEMP_LSB_MASK			GENMASK(7, 0)

#define BATT_INFO_BATT_TEMP_MSB(chip)		(chip->batt_info_base + 0x51)
#define BATT_TEMP_MSB_MASK			GENMASK(2, 0)

#define BATT_INFO_BATT_TEMP_CFG(chip)		(chip->batt_info_base + 0x56)
#define JEITA_TEMP_HYST_MASK			GENMASK(5, 4)
#define JEITA_TEMP_HYST_SHIFT			4
#define JEITA_TEMP_NO_HYST			0x0
#define JEITA_TEMP_HYST_1C			0x1
#define JEITA_TEMP_HYST_2C			0x2
#define JEITA_TEMP_HYST_3C			0x3

#define BATT_INFO_BATT_TMPR_INTR(chip)		(chip->batt_info_base + 0x59)
#define CHANGE_THOLD_MASK			GENMASK(1, 0)
#define BTEMP_DELTA_2K				0x0
#define BTEMP_DELTA_4K				0x1
#define BTEMP_DELTA_6K				0x2
#define BTEMP_DELTA_10K				0x3

#define BATT_INFO_THERM_C1(chip)		(chip->batt_info_base + 0x5C)
#define BATT_INFO_THERM_COEFF_MASK		GENMASK(7, 0)

#define BATT_INFO_THERM_C2(chip)		(chip->batt_info_base + 0x5D)
#define BATT_INFO_THERM_C3(chip)		(chip->batt_info_base + 0x5E)

#define BATT_INFO_THERM_HALF_RANGE(chip)	(chip->batt_info_base + 0x5F)
#define BATT_INFO_THERM_TEMP_MASK		GENMASK(7, 0)

#define BATT_INFO_JEITA_CTLS(chip)		(chip->batt_info_base + 0x61)
#define JEITA_STS_CLEAR_BIT			BIT(0)

#define BATT_INFO_JEITA_TOO_COLD(chip)		(chip->batt_info_base + 0x62)
#define JEITA_THOLD_MASK			GENMASK(7, 0)

#define BATT_INFO_JEITA_COLD(chip)		(chip->batt_info_base + 0x63)
#define BATT_INFO_JEITA_HOT(chip)		(chip->batt_info_base + 0x64)
#define BATT_INFO_JEITA_TOO_HOT(chip)		(chip->batt_info_base + 0x65)

/* starting from v2.0 */
#define BATT_INFO_ESR_GENERAL_CFG(chip)		(chip->batt_info_base + 0x68)
#define ESR_DEEP_TAPER_EN_BIT			BIT(0)

#define BATT_INFO_ESR_PULL_DN_CFG(chip)		(chip->batt_info_base + 0x69)
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

#define BATT_INFO_ESR_FAST_CRG_CFG(chip)	(chip->batt_info_base + 0x6A)
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

/* GEN4 bit definitions */
#define GEN4_ESR_FAST_CRG_IVAL_MASK		GENMASK(7, 4)
#define GEN4_ESR_FAST_CRG_IVAL_SHIFT		4
#define GEN4_ESR_FCC_300MA			0x0
#define GEN4_ESR_FCC_600MA			0x1
#define GEN4_ESR_FCC_1A				0x2
#define GEN4_ESR_FCC_1P5_A			0x3
#define GEN4_ESR_FCC_2A				0x4
#define GEN4_ESR_FCC_2P5_A			0x5
#define GEN4_ESR_FCC_3A				0x6
#define GEN4_ESR_FCC_3P5_A			0x7
#define GEN4_ESR_FCC_4A				0x8
#define GEN4_ESR_FCC_4P5_A			0x9
#define GEN4_ESR_FCC_5A				0xA
#define GEN4_ESR_FCC_5P5_A			0xB
#define GEN4_ESR_FCC_6A				0xC
#define GEN4_ESR_FCC_6P5_A			0xD
#define GEN4_ESR_FCC_7A				0xE
#define GEN4_ESR_FCC_7P5_A			0xF

#define BATT_INFO_BATT_MISS_CFG(chip)		(chip->batt_info_base + 0x6B)
#define BM_THERM_TH_MASK			GENMASK(5, 4)
#define RES_TH_0P75_MOHM			0x0
#define RES_TH_1P00_MOHM			0x1
#define RES_TH_1P50_MOHM			0x2
#define RES_TH_3P00_MOHM			0x3
#define BM_BATT_ID_TH_MASK			GENMASK(3, 2)
#define BM_FROM_THERM_BIT			BIT(1)
#define BM_FROM_BATT_ID_BIT			BIT(0)

#define BATT_INFO_WATCHDOG_COUNT(chip)		(chip->batt_info_base + 0x70)
#define WATCHDOG_COUNTER			GENMASK(7, 0)

#define BATT_INFO_WATCHDOG_CFG(chip)		(chip->batt_info_base + 0x71)
#define RESET_CAPABLE_BIT			BIT(2)
#define PET_CTRL_BIT				BIT(1)
#define ENABLE_CTRL_BIT				BIT(0)

#define BATT_INFO_IBATT_SENSING_CFG(chip)	(chip->batt_info_base + 0x73)
#define ADC_BITSTREAM_INV_BIT			BIT(4)
#define SOURCE_SELECT_MASK			GENMASK(1, 0)
#define SRC_SEL_BATFET				0x0
#define SRC_SEL_BATFET_SMB			0x2
#define SRC_SEL_RESERVED			0x3

#define BATT_INFO_QNOVO_CFG(chip)		(chip->batt_info_base + 0x74)
#define LD_REG_FORCE_CTL_BIT			BIT(2)
#define LD_REG_CTRL_FORCE_HIGH			LD_REG_FORCE_CTL_BIT
#define LD_REG_CTRL_FORCE_LOW			0
#define LD_REG_CTRL_BIT				BIT(1)
#define LD_REG_CTRL_REGISTER			LD_REG_CTRL_BIT
#define LD_REG_CTRL_LOGIC			0
#define BIT_STREAM_CFG_BIT			BIT(0)

#define BATT_INFO_QNOVO_SCALER(chip)		(chip->batt_info_base + 0x75)
#define QNOVO_SCALER_MASK			GENMASK(7, 0)

/* starting from v2.0 */
#define BATT_INFO_CRG_SERVICES(chip)		(chip->batt_info_base + 0x90)
#define FG_CRC_TRM_EN_BIT			BIT(0)

/* Following LSB/MSB address are for v2.0 and above; v1.1 have them swapped */
#define BATT_INFO_VBATT_LSB(chip)		(chip->batt_info_base + 0xA0)
#define BATT_INFO_VBATT_MSB(chip)		(chip->batt_info_base + 0xA1)
#define VBATT_MASK				GENMASK(7, 0)

#define BATT_INFO_IBATT_LSB(chip)		(chip->batt_info_base + 0xA2)
#define BATT_INFO_IBATT_MSB(chip)		(chip->batt_info_base + 0xA3)
#define IBATT_MASK				GENMASK(7, 0)

#define BATT_INFO_ESR_LSB(chip)			(chip->batt_info_base + 0xA4)
#define BATT_INFO_ESR_MSB(chip)			(chip->batt_info_base + 0xA5)
#define ESR_LSB_MASK				GENMASK(7, 0)
#define ESR_MSB_MASK				GENMASK(5, 0)

#define BATT_INFO_VBATT_LSB_CP(chip)		(chip->batt_info_base + 0xA6)
#define BATT_INFO_VBATT_MSB_CP(chip)		(chip->batt_info_base + 0xA7)
#define BATT_INFO_IBATT_LSB_CP(chip)		(chip->batt_info_base + 0xA8)
#define BATT_INFO_IBATT_MSB_CP(chip)		(chip->batt_info_base + 0xA9)
#define BATT_INFO_ESR_LSB_CP(chip)		(chip->batt_info_base + 0xAA)
#define BATT_INFO_ESR_MSB_CP(chip)		(chip->batt_info_base + 0xAB)

#define BATT_INFO_VADC_LSB(chip)		(chip->batt_info_base + 0xAC)
#define VADC_LSB_MASK				GENMASK(7, 0)

#define BATT_INFO_VADC_MSB(chip)		(chip->batt_info_base + 0xAD)
#define VADC_MSB_MASK				GENMASK(6, 0)

#define BATT_INFO_IADC_LSB(chip)		(chip->batt_info_base + 0xAE)
#define IADC_LSB_MASK				GENMASK(7, 0)

#define BATT_INFO_IADC_MSB(chip)		(chip->batt_info_base + 0xAF)
#define IADC_MSB_MASK				GENMASK(6, 0)

#define BATT_INFO_FG_CNV_CHAR_CFG(chip)		(chip->batt_info_base + 0xB7)
#define SMB_MEASURE_EN_BIT			BIT(2)

#define BATT_INFO_TM_MISC(chip)			(chip->batt_info_base + 0xE5)
#define FORCE_SEQ_RESP_TOGGLE_BIT		BIT(6)
#define ALG_DIRECT_VALID_DATA_BIT		BIT(5)
#define ALG_DIRECT_MODE_EN_BIT			BIT(4)
#define BATT_VADC_CONV_BIT			BIT(3)
#define BATT_IADC_CONV_BIT			BIT(2)
#define ADC_ENABLE_REG_CTRL_BIT			BIT(1)
#define WDOG_FORCE_EXP_BIT			BIT(0)

#define BATT_INFO_TM_MISC1(chip)		(chip->batt_info_base + 0xE6)
/* for v2.0 and above */
#define ESR_REQ_CTL_BIT				BIT(1)
#define ESR_REQ_CTL_EN_BIT			BIT(0)

#define BATT_INFO_PEEK_MUX4(chip)		(chip->batt_info_base + 0xEE)
#define ALG_ACTIVE_PEEK_CFG			0xAC

#define BATT_INFO_PEEK_RD(chip)			(chip->batt_info_base + 0xEF)
#define ALG_ACTIVE_BIT				BIT(3)

/* FG_MEM_IF register and bit definitions */
#define MEM_IF_INT_RT_STS(chip)			((chip->mem_if_base) + 0x10)
#define MEM_XCP_BIT				BIT(1)
#define MEM_GNT_BIT				BIT(2)
#define GEN4_DMA_XCP_BIT			BIT(2)
#define GEN4_MEM_GNT_BIT			BIT(3)
#define GEN4_MEM_ATTN_BIT			BIT(4)

#define MEM_IF_MEM_ARB_CFG(chip)		((chip->mem_if_base) + 0x40)
#define MEM_CLR_LOG_BIT				BIT(2)
#define MEM_ARB_LO_LATENCY_EN_BIT		BIT(1)
#define MEM_ARB_REQ_BIT				BIT(0)

#define MEM_IF_MEM_INTF_CFG(chip)		((chip->mem_if_base) + 0x50)
#define MEM_ACCESS_REQ_BIT			BIT(7)
#define IACS_SLCT_BIT				BIT(5)

#define MEM_IF_IMA_CTL(chip)			((chip->mem_if_base) + 0x51)
#define MEM_ACS_BURST_BIT			BIT(7)
#define IMA_WR_EN_BIT				BIT(6)
#define IMA_CTL_MASK				GENMASK(7, 6)

#define MEM_IF_IMA_CFG(chip)			((chip->mem_if_base) + 0x52)
#define IACS_CLR_BIT				BIT(2)
#define IACS_INTR_SRC_SLCT_BIT			BIT(3)
#define STATIC_CLK_EN_BIT			BIT(4)

#define MEM_IF_IMA_OPR_STS(chip)		((chip->mem_if_base) + 0x54)
#define IACS_RDY_BIT				BIT(1)

#define MEM_IF_IMA_EXP_STS(chip)		((chip->mem_if_base) + 0x55)
#define IACS_ERR_BIT				BIT(0)
#define XCT_TYPE_ERR_BIT			BIT(1)
#define DATA_RD_ERR_BIT				BIT(3)
#define DATA_WR_ERR_BIT				BIT(4)
#define ADDR_BURST_WRAP_BIT			BIT(5)
#define ADDR_STABLE_ERR_BIT			BIT(7)

#define MEM_IF_IMA_HW_STS(chip)			((chip->mem_if_base) + 0x56)

#define MEM_IF_FG_BEAT_COUNT(chip)		((chip->mem_if_base) + 0x57)
#define BEAT_COUNT_MASK				GENMASK(3, 0)

#define MEM_IF_IMA_ERR_STS(chip)		((chip->mem_if_base) + 0x5F)
#define ADDR_STBL_ERR_BIT			BIT(7)
#define WR_ACS_ERR_BIT				BIT(6)
#define RD_ACS_ERR_BIT				BIT(5)

#define MEM_IF_IMA_BYTE_EN(chip)		((chip->mem_if_base) + 0x60)
#define MEM_IF_ADDR_LSB(chip)			((chip->mem_if_base) + 0x61)
#define MEM_IF_ADDR_MSB(chip)			((chip->mem_if_base) + 0x62)
#define MEM_IF_WR_DATA0(chip)			((chip->mem_if_base) + 0x63)
#define MEM_IF_WR_DATA1(chip)			((chip->mem_if_base) + 0x64)
#define MEM_IF_WR_DATA3(chip)			((chip->mem_if_base) + 0x66)
#define MEM_IF_RD_DATA0(chip)			((chip->mem_if_base) + 0x67)
#define MEM_IF_RD_DATA1(chip)			((chip->mem_if_base) + 0x68)
#define MEM_IF_RD_DATA3(chip)			((chip->mem_if_base) + 0x6A)

#define MEM_IF_DMA_STS(chip)			((chip->mem_if_base) + 0x70)
#define DMA_WRITE_ERROR_BIT			BIT(1)
#define DMA_READ_ERROR_BIT			BIT(2)

#define MEM_IF_DMA_CTL(chip)			((chip->mem_if_base) + 0x71)
#define ADDR_KIND_BIT				BIT(1)
#define DMA_CLEAR_LOG_BIT			BIT(0)

/* FG_DMAx */
#define FG_DMA0_BASE				0x4800
#define FG_DMA1_BASE				0x4900
#define FG_DMA2_BASE				0x4A00
#define FG_DMA3_BASE				0x4B00
#define SRAM_ADDR_OFFSET			0x20

/* GEN4 FG_DMAx */
#define GEN4_FG_DMA0_BASE			0x4400
#define GEN4_FG_DMA1_BASE			0x4500
#define GEN4_FG_DMA2_BASE			0x4600
#define GEN4_FG_DMA3_BASE			0x4700
#define GEN4_FG_DMA4_BASE			0x4800
#define GEN4_FG_DMA5_BASE			0x4900
#endif
