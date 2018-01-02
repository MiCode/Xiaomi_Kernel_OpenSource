/* Copyright (c) 2018 The Linux Foundation. All rights reserved.
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

#ifndef __QG_REG_H__
#define __QG_REG_H__

#define PERPH_TYPE_REG				0x04
#define QG_TYPE					0x0D

#define QG_STATUS1_REG				0x08
#define BATTERY_PRESENT_BIT			BIT(0)

#define QG_STATUS2_REG				0x09
#define GOOD_OCV_BIT				BIT(1)

#define QG_STATUS3_REG				0x0A
#define COUNT_FIFO_RT_MASK			GENMASK(3, 0)

#define QG_INT_RT_STS_REG			0x10
#define FIFO_UPDATE_DONE_RT_STS_BIT		BIT(3)
#define VBAT_LOW_INT_RT_STS_BIT			BIT(1)

#define QG_INT_LATCHED_STS_REG			0x18
#define FIFO_UPDATE_DONE_INT_LAT_STS_BIT	BIT(3)

#define QG_DATA_CTL1_REG			0x41
#define MASTER_HOLD_OR_CLR_BIT			BIT(0)

#define QG_MODE_CTL1_REG			0x43
#define PARALLEL_IBAT_SENSE_EN_BIT		BIT(7)

#define QG_VBAT_EMPTY_THRESHOLD_REG		0x4B
#define QG_VBAT_LOW_THRESHOLD_REG		0x4C

#define QG_S2_NORMAL_MEAS_CTL2_REG		0x51
#define FIFO_LENGTH_MASK			GENMASK(5, 3)
#define FIFO_LENGTH_SHIFT			3
#define NUM_OF_ACCUM_MASK			GENMASK(2, 0)

#define QG_S2_NORMAL_MEAS_CTL3_REG		0x52

#define QG_S3_SLEEP_OCV_MEAS_CTL4_REG		0x59
#define S3_SLEEP_OCV_TIMER_MASK			GENMASK(2, 0)

#define QG_S3_SLEEP_OCV_TREND_CTL2_REG		0x5C
#define TREND_TOL_MASK				GENMASK(5, 0)

#define QG_S3_SLEEP_OCV_IBAT_CTL1_REG		0x5D
#define SLEEP_IBAT_QUALIFIED_LENGTH_MASK	GENMASK(2, 0)

#define QG_S3_ENTRY_IBAT_THRESHOLD_REG		0x5E
#define QG_S3_EXIT_IBAT_THRESHOLD_REG		0x5F

#define QG_S7_PON_OCV_V_DATA0_REG		0x70
#define QG_S7_PON_OCV_I_DATA0_REG		0x72
#define QG_S3_GOOD_OCV_V_DATA0_REG		0x74
#define QG_S3_GOOD_OCV_I_DATA0_REG		0x76

#define QG_V_ACCUM_DATA0_RT_REG			0x88
#define QG_I_ACCUM_DATA0_RT_REG			0x8B
#define QG_ACCUM_CNT_RT_REG			0x8E

#define QG_V_FIFO0_DATA0_REG			0x90
#define QG_I_FIFO0_DATA0_REG			0xA0

#define QG_SOC_MONOTONIC_REG			0xBF

#define QG_LAST_ADC_V_DATA0_REG			0xC0
#define QG_LAST_ADC_I_DATA0_REG			0xC2

/* SDAM offsets */
#define QG_SDAM_VALID_OFFSET			0x46
#define QG_SDAM_SOC_OFFSET			0x47
#define QG_SDAM_TEMP_OFFSET			0x48
#define QG_SDAM_RBAT_OFFSET			0x4A
#define QG_SDAM_OCV_OFFSET			0x4C
#define QG_SDAM_IBAT_OFFSET			0x50
#define QG_SDAM_TIME_OFFSET			0x54

#endif
