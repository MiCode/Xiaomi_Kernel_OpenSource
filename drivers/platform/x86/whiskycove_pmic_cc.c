/*
 * whiskycove_pmic_cc.c - Whiskey Cove PMIC Coulomb Counter Driver
 *
 * Copyright (C) 2014 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author:	Ramakrishna Pallala <ramakrishna.pallala@intel.com>
 *			Srinidhi Rao <srinidhi.rao@intel.com>
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/time.h>
#include <linux/wakelock.h>
#include <linux/mfd/intel_soc_pmic.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/io.h>

#include <linux/iio/consumer.h>
#include <linux/power/intel_fuel_gauge.h>
#include <linux/completion.h>

#define DRIVER_NAME					"whiskey_cove_cc"

/* Base address of coulomb counter block. */
#define PMU_BASE_ADR					(0x04F0)

/* Address of coulomb counter UP register. */
#define PMU_CC_UP_CNT					(0xF2)
/* Address of coulomb counter DOWN register. */
#define PMU_CC_DOWN_CNT					(0xEE)
/* Bitfield mask for positive part of 13 bit battery current field. */
#define PMU_CC_CBAT_POS_MASK				(0xFFF)
/* Bitfield mask for negative part of 13 bit battery current field. */
#define PMU_CC_CBAT_NEG_MASK				(0x1000)

/* Address of short term battery current register. */
#define PMU_ST_BAT_CURRENT				(0xE8)
/* Address of latched short term battery current register. */
#define PMU_LATCHED_VBAT_MAX				(0xF6)
/* Address of latched short term battery current register. */
#define PMU_LATCHED_ST_BAT_CURRENT			(0xF8)
/* Address of long term battery current register. */
#define PMU_LT_BAT_CURRENT				(0xEA)
/* Address of latched long term battery current register. */
#define PMU_LATCHED_LT_BAT_CURRENT			(0xFA)

/* Address of coulomb counter THR register. */
#define PMU_CC_THR					(0xE6)
/* Maximum value of CC_THR register */
#define PMU_CC_THR_LIMIT				(0xFF)

/* WC: Address of coulomb counter CTRL register. */
#define PMU_CC_CTRL0_REG					(0xEC)
#define PMU_CC_CTRL1_REG					(0xED)

/*WC: CC_CNTRL Settings to clear CC */
#define CTRL0_CC_DEF_SET		(0x04)
#define CTRL1_CC_DEF_SET		(0x09)

/* WC: Coulomb counter CTRL register setting. */
#define SC_CC_CTRL_ENABLE				(0x00)

#define PMU_CLR_VBATMAX					(0xF6)

#define BATT_OCV_CONV_FCTR				(1250)

/* Rsense Resistance for Whiskeycove pmic */
#define SENSE_RESISTOR_MOHM				(10)

#define	BATT_RESERVED_CAP				(28)

/*
 * Maximum value for charge remaining (mAh).
 * Used when no battery is fitted to reduce.
 * capacity depletion due to the coulomb counter
 * between calibration points when powered by
 * a PSU.
 */
#define POWER_SUPPLY_CHARGE_MAX_MAH			(222600)

/* Theoretical trigger level for coulomb counter increment in mV. */
#define CC_INC_THRES_THEORETICAL_MV     (500)
/* Average error in trigger level for coulomb counter increment in mV. */
#define CC_INC_THRES_COMP_MV    (15)

/* Sampling frequency of coulomb counter integrator in Hz. */
#define CC_CLK_FREQ_HZ                          (8192)
/* Delta coulomb counter threshold scaling factor */
#define CC_DELTA_THRES_SCALING                  (12500)

/* Accumulated CC Error Values */
#define OFFSET_ERR_UC_PER_S			(0)
#define GAIN_ERR_1_UC_PER_MC			(20)
#define GAIN_ERR_2_UC_PER_MC			(20)


/* WCove CCTICK IRQ defnitions */
#define CC_TICK_ADCIRQ_REG	(0x6E08)
#define CC_CHRGR_IRQ_REG	(0x6E0A)
#define CC_MADCIRQ_REG		(0x6E15)
#define CC_MIRQLVL1_REG		(0x6E0E)
#define CC_MIRQ_CLR_ADC		(1 << 3)
#define CC_MADC_CLR_IRQ		(0)
#define CC_TICK_ADCIRQ		(1 << 7)

/* Multiplier / Divisor for milli-micro calculations. */
#define SCALE_MILLI                             (1000)

/* Coulomb Scaling factor  from C to uC value. */
#define SCALING_C_TO_UC						(1000000)
/* Coulomb Scaling factor from uC to mC value. */
#define SCALING_UC_TO_MC					(1000)
/* Scaling factor from uA to mA. */
#define SCALING_UA_TO_MA					(1000)
/* Scaling factor  from uS to S. */
#define SCALING_US_TO_S						(1000000)

/* Error compensated trigger level for coulomb counter increment in mV. */
#define CC_INC_THRES_MV \
		(CC_INC_THRES_THEORETICAL_MV + \
			CC_INC_THRES_COMP_MV)

#define CC_SCALING_UC						3050

#define THRES_CNT_SCALING_MC \
		((CC_SCALING_UC * CC_DELTA_THRES_SCALING) \
			/ SCALING_UC_TO_MC)

/* Conversion factor for mDeg to Deg. */
#define SCALE_TEMP_MDEG_TO_DEG			(1000)

#define BATT_VOLT_CONV_FCTR			(1250)

/* Minimum period required for a long term average Ibat measurement. */
#define IBAT_LONG_TERM_AVG_MIN_PERIOD_SECS			(300)
#if !defined(SWFG_HAL_WA_IIR_FILTER_BUG)
/* long term average Ibat period error margin in percent. */
#define IBAT_LONG_TERM_AVG_ERR_MARGIN_PERCENT		(10)
/* Timer polling period for long term average Ibat in Secs. */
#define IBAT_LTERM_AVG_JIFFIES \
		(IBAT_LONG_TERM_AVG_MIN_PERIOD_SECS+ \
		((IBAT_LONG_TERM_AVG_MIN_PERIOD_SECS\
		 * IBAT_LONG_TERM_AVG_ERR_MARGIN_PERCENT) / 100))
#else
#define IBAT_LTERM_AVG_JIFFIES		(HZ * 10) /* 10sec */
#endif

/* Conversion factor for mAh to mC */
#define SCALE_MAH_TO_MC				(3600)

/* Multiplier / Divisor for milli-micro calculations. */
#define SCALE_MILLI				(1000)

/**
 * enum swfg_hal_get_key - Key used to get information
 * from the SW Fuel Gauge HAL driver.
 * See struct swfg_hal_interface::get()
.*
 * @SWFG_HAL_GET_CC_POSITIVE_COUNT
.*	Key for the amount of charge into the battery.
.* Parameters for ::get are then defined as:
.*	::union swfg_hal_get_params.cc_positive_mc
.*		Amount of charge into the battery (mC)
.*
.*.@SWFG_HAL_GET_CC_NEGATIVE_COUNT
.*	Key for the amount of charge out of the battery.
.* Parameters for ::get are then defined as:
.*	::union swfg_hal_get_params.cc_negative_mc
.*		Amount of charge out of the battery (mC)
.*
.*.@SWFG_HAL_GET_CC_BALANCED_COUNT
.*	Key for the amount of charge in/out of the battery
.* since the last reference point.
.*	Parameters for ::get are then defined as:
.*	::union swfg_hal_get_params.cc_balanced_mc
.*		Amount of charge into, less the charge out of the battery (mC)
.*
.*.@SWFG_HAL_GET_CC_ACCUMULATED_ERR
.*	Key for the accumulated error since the last error reset.
.* Parameters for ::get are then defined as:
.*	::union swfg_hal_get_params.cc_acc_err_mc
.*		Accumulated error since the last error reset (mC)
.*
.*.@SWFG_HAL_GET_COULOMB_IND_DELTA_THRES,
.*	Key for definition of the coulomb indication delta threshold.
.* Parameters for ::get are then defined as:
.*	::union swfg_hal_get_params.delta_thres_mc
.*		Threshold set for delta notification (mC)
.*
.*.@SWFG_HAL_GET_IBAT_LOAD_SHORT_TERM_AVG,
.*	Key to read the short term average battery current.
.* Parameters for ::get are then defined as:
.*	::union swfg_hal_get_params.ibat_load_short_ma
.*		Short term average battery current (mA).
.*
.*.@SWFG_HAL_GET_IBAT_LOAD_SHORT_TERM_AVG_AT_OCV,
.*	Key to read the short term average battery current at OCV measurement.
.* Parameters for ::get are then defined as:
.*	::union swfg_hal_get_params.ibat_load_short_at_ocv_ma
.*		Short term average battery current at OCV (mA).
.*
.*.@SWFG_HAL_GET_IBAT_LOAD_LONG_TERM_AVG_AT_OCV
.*	Key to read the long term average battery current at OCV measurement.
.* Parameters for ::get are then defined as:
.*	::union swfg_hal_get_params.ibat_load_long_at_ocv_ma
.*		Long term average battery current (mA)
.*
 */
enum swfg_hal_get_key {
	SWFG_HAL_GET_CC_POSITIVE_COUNT,
	SWFG_HAL_GET_CC_NEGATIVE_COUNT,
	SWFG_HAL_GET_CC_BALANCED_COUNT,
	SWFG_HAL_GET_CC_ACCUMULATED_ERR,
	SWFG_HAL_GET_COULOMB_IND_DELTA_THRES,
	SWFG_HAL_GET_IBAT_LOAD_SHORT_TERM_AVG,
	SWFG_HAL_GET_IBAT_LOAD_SHORT_TERM_AVG_AT_OCV,
	SWFG_HAL_GET_IBAT_LOAD_LONG_TERM_AVG,
	SWFG_HAL_GET_IBAT_LOAD_LONG_TERM_AVG_AT_OCV,
};

/**
 * union swfg_hal_get_params - Union type for get function parameters
 *
 * @cc_positive_mc
 * See enum swfg_hal_get_key::SWFG_HAL_GET_CC_POSITIVE_COUNT
 * @cc_negative_mc See enum key::SWFG_HAL_GET_CC_NEGATIVE_COUNT
 * @cc_balanced_mc See enum key::SWFG_HAL_GET_CC_BALANCED_COUNT
 * @cc_acc_err_mc See enum key:: SWFG_HAL_GET_CC_ACC_ERROR_COUNT
 * @delta_thres_mc See enum key
 *	::SWFG_HAL_GET_COULOMB_IND_DELTA_THRES
 * @ibat_load_short_ma See enum key
 *	::SWFG_HAL_GET_IBAT_LOAD_SHORT_TERM_AVG
 * @ibat_load_short_at_ocv_ma See
 *	enum key::SWFG_HAL_GET_IBAT_LOAD_SHORT_TERM_AVG_AT_OCV
 * @ibat_load_long_ma See enum key
 *	::SWFG_HAL_GET_IBAT_LOAD_LONG_TERM_AVG
 * @ibat_load_long_at_ocv_ma See enum key
 *	::SWFG_HAL_GET_IBAT_LOAD_LONG_TERM_AVG_AT_OCV
 */
union swfg_hal_get_params {
	int cc_positive_mc;
	int cc_negative_mc;
	int cc_balanced_mc;
	int cc_acc_err_mc;
	int delta_thres_mc;
	int ibat_load_short_ma;
	int ibat_load_short_at_ocv_ma;
	int ibat_load_long_ma;
	int ibat_load_long_at_ocv_ma;
};

/**
 * key used to set the configuration of the SW Fuel Gauge HAL driver.
 * See struct swfg_hal_interface::set()
 *
 * @SWFG_HAL_SET_COULOMB_IND_DELTA_THRES
 *	Key for definition of the coulomb indication delta threshold.
 *	The parameters for ::set are then defined as:
 *	::union swfg_hal_set_params.delta_thres_mc
 *	Delta threshold for notification (mC)
 *
 * @SWFG_HAL_SET_ZERO_ACCUMULATED_CC_ERROR
 *	Key to reset the accumulated coulomb counter error.
 *	The parameters for ::set are then defined as:
 *	None. Performs error reset without further parameters.
 * @SWFG_HAL_SET_CLEAR_LATCHED_IBAT_AVGS_AT_OCV
 *	Key to reset the ibat averages latched at OCV point.
 *	This might be dummy if not supported in HW.
 *	The parameters for ::set are then defined as:
 *	None. Performs error reset without further parameters.
 */
enum swfg_hal_set_key {
	SWFG_HAL_SET_COULOMB_IND_DELTA_THRES,
	SWFG_HAL_SET_ZERO_ACC_CC_ERR,
	SWFG_HAL_SET_CLEAR_LATCHED_IBAT_AVGS_AT_OCV,
};

/**
 * union swfg_hal_set_params - Union type for set function params
 * @delta_thres_mc
 * see enum swfg_hal_set_key::SWFG_HAL_SET_DELTA_THRES
 * @dummy_value
 * For functions that do not require an argument, a dummy parameter is passed
 */
union swfg_hal_set_params {
	int delta_thres_mc;
	int dummy_value;
};

struct wcove_cc_info {
	struct platform_device *pdev;
	struct delayed_work	init_work;
	struct completion cc_tick_completion;

	/* ADC IRQ interrupt request number */
	int			cc_irq;
	/* Virtual address of PMU Coulomb Counter registers. */
	u16			pmu_cc_base;
	/* Base timestamp for accumulated error. */
	time_t			err_base_rtc_sec;
	/* Base count for accumulated error in charge IN to battery. */
	u32			err_base_cc_up;
	/* Base count for accumulated error in charge OUT of battery. */
	u32			err_base_cc_down;
	/* Coulomb delta threshold currently set (mC) */
	int			delta_thres_mc;
	/* true after coulomb delta threshold has been set. */
	bool			delta_thres_set;
	/* Scaling factor from counts to mC for delta threshold */
	int			thres_count_scaling_mc;
	/* Scaling factor from UP and DOWN counts to uC */
	int			cc_scaling_uc;
};

static struct wcove_cc_info *info_ptr;

/**
 * swfg_hal_get_coulomb_counts - Read the raw couloumb counter values
 *
 * @cc_up_counts	[out] Raw value of coulomb UP counter in counts.
 * @cc_down_counts	[out] Raw value of coulomb DOWN counter in counts.
 */
static void swfg_hal_get_coulomb_counts(u32 *cc_up_counts,
						u32 *cc_down_counts)
{
	u16 addr;
	u8 data0, data1, data2, data3, dummy;
	/*
	 * Read registers and return values to the caller.
	 */
	addr = info_ptr->pmu_cc_base + PMU_CC_UP_CNT;

	/* Read the LSB register 1st, as this read is used to latch MSBs */
	data0 = intel_soc_pmic_readb(addr+3);
	if (data0 < 0) {
		pr_err("SWFG: Error in reading 0x%04x register\n", addr+3);
		goto out;
	}
	/* Dummy read to workaround register double-buffer bug - START
	 * MSB registers are incorrectly doubled-buffered and a dummy
	 * read after the normal LSB read is required
	 * to get latest contents of MSBs
	 */
	dummy = intel_soc_pmic_readb(addr+3);
	if (dummy < 0)
		pr_err("SWFG: Error in reading 0x%04x register\n", addr+3);

	/* Dummy read required to workaround
			register double-buffer bug - END */
	data1 = intel_soc_pmic_readb(addr+2);
	if (data1 < 0) {
		pr_err("SWFG: Error in reading 0x%04x register\n", addr+2);
		goto out;
	}

	data2 = intel_soc_pmic_readb(addr+1);
	if (data2 < 0) {
		pr_err("SWFG: Error in reading 0x%04x register\n", addr+1);
		goto out;
	}

	data3 = intel_soc_pmic_readb(addr);
	if (data3 < 0) {
		pr_err("SWFG: Error in reading 0x%04x register\n", addr);
		goto out;
	}

	*cc_up_counts =
	(((u32)data3 << 24)+((u32)data2 << 16)+((u32)data1 << 8)+data0);

	addr = info_ptr->pmu_cc_base + PMU_CC_DOWN_CNT;

	/* Read the LSB register 1st, as this read is used to latch MSBs */
	data0 = intel_soc_pmic_readb(addr+3);
	if (data0 < 0) {
		pr_err("SWFG: Error in reading 0x%04x register\n", addr+3);
		goto out;
	}
	/* Dummy read to workaround register double-buffer bug - START
	 * MSB registers are incorrectly doubled-buffered and a dummy
	 * read after the normal LSB read is
	 * required to get latest contents of MSBs
	 */
	dummy = intel_soc_pmic_readb(addr+3);
	if (dummy < 0)
		pr_err("SWFG: Error in reading 0x%04x register\n", addr+3);

	/* Dummy read required to workaround
		register double-buffer bug - END */
	data1 = intel_soc_pmic_readb(addr+2);
	if (data1 < 0) {
		pr_err("SWFG: Error in reading 0x%04x register\n", addr+2);
		goto out;
	}

	data2 = intel_soc_pmic_readb(addr+1);
	if (data2 < 0) {
		pr_err("SWFG: Error in reading 0x%04x register\n", addr+1);
		goto out;
	}

	data3 = intel_soc_pmic_readb(addr);
	if (data3 < 0) {
		pr_err("SWFG: Error in reading 0x%04x register\n", addr);
		goto out;
	}

	*cc_down_counts =
	(((u32)data3 << 24)+((u32)data2 << 16)+((u32)data1 << 8)+data0);

out:
	return;

}

/**
 * swfg_hal_counts_to_mc - Convert raw couloumb counter values to mC
 *
 * NOTE: The HW counters are unsigned 32-bit values. Due to the possible
 * non-integer nature of the scaling factor when expressed in mC, uC are
 * used internally to maintain accuracy. This then requires 64-bit
 * arithmetic to prevent arithmetic overflow.
 *
 * Returning a signed 32-bit quantity (which may not represent the full
 * range of the HW) is deemed acceptable.
 *
 * Example:
 * Each count corresponds to 3.051 mC, giving a maximum value
 * of (2^32) x 3.051 mC, or approximately 13,000,000,000 mC.
 * In mAh: over 3,600,00 - which is currently almost
 * 1000 x the size of a large battery.
 *
 * The signed 32 bit return still allows for peak values
 * of over +/- 2x10^9 or nearly 600000 mAh.
 *
 * @cc_counts	[in] Raw value of coulomb count.
 * Returns:	Coulomb count in mC.
 */
static int swfg_hal_counts_to_mc(int cc_counts)
{
	/* Scale the HW count value to uC using 64-bit
		arithmetic to maintain accuracy. */
	s64 result_uc_64 = (s64)cc_counts *
			(s64)info_ptr->cc_scaling_uc;
	s32 remainder;
	/*
	 * Truncate to 32-bit signed mC value for result.
	 * NOTE: 64 Division is not supported with the standard C operator
	 */
	return (int)div_s64_rem(result_uc_64, SCALING_UC_TO_MC, &remainder);
}

/**
 * swfg_hal_read_coulomb_counter - Read the HW and return the
 *  requested coulomb counter
 * value converted to mC.
 * @value_to_read	[in] Specifies which coulomb count to read.
 * Returns:		Signed coulomb count in mC.
 */
static int swfg_hal_read_coulomb_counter(
			enum swfg_hal_get_key value_to_read)
{
	u32 cc_up;
	u32 cc_down;
	int result_mc = 0;

	/* Get the raw coulomb counter values from the HW . */
	swfg_hal_get_coulomb_counts(&cc_up, &cc_down);

	switch (value_to_read) {
	case SWFG_HAL_GET_CC_BALANCED_COUNT:
		/* Calculate balanced value.
		 * In SC, the coulomb counter assumes positive IBAT
		 * when charging (according to the datasheet, this is correct)
		 * Our design works on the premise that the IBAT is negative
		 * when charging, so the sign is inverted by subtracting the
		 * negative minus the positive.
		 */
		result_mc = swfg_hal_counts_to_mc(cc_down - cc_up);
		break;

	case SWFG_HAL_GET_CC_POSITIVE_COUNT:
		/* Number of counts into the battery. */
		result_mc = swfg_hal_counts_to_mc(cc_up);
		break;

	case SWFG_HAL_GET_CC_NEGATIVE_COUNT:
		/* Number of counts out of the battery. */
		result_mc = swfg_hal_counts_to_mc(cc_down);
		break;

	default:
		/* Invalid read parameter. */
		pr_warn("SWFG: file:%s, func:%s, line:%d\n",
			__FILE__, __func__, __LINE__);
		break;
	}
	return result_mc;
}

/**
 * swfg_hal_read_batt_current - Read the HW register
 * and return the 1 second average
 * battery current measured by the coulomb counter.
 * @longterm [in]	Specifies whether long (TRUE) or
 * short (FALSE) average current is to be read.
 * @latched [in]	Specifies whether latched (TRUE) or
 * running (FALSE) measurement is to be read.
 * Returns:		Battery current in mA.
 */
static int swfg_hal_read_batt_current(bool longterm, bool latched)
{
	int ibat_positive;
	int ibat_negative;
	int ibat_count_signed;
	int ibat_ma = INT_MAX;
	u16 addr, offset;
	u8 data0, data1, dummy;
	u32 ibat_reg;
	bool data_valid = true;
	int ret = 0;

	/* Determine the address offset based on the type of
		measurement to be read */
	if (longterm) {
		if (latched)
			offset = PMU_LATCHED_LT_BAT_CURRENT;
		else
			offset = PMU_LT_BAT_CURRENT;
	} else {
		if (latched)
			offset = PMU_LATCHED_ST_BAT_CURRENT;
		else
			offset = PMU_ST_BAT_CURRENT;
	}

	if (latched) {
		u32 max_vbat;

		/* Check whether latched values are valid by checking whether
			 Max Voltage value is not zero (reset value) */
		addr = info_ptr->pmu_cc_base + PMU_LATCHED_VBAT_MAX;

		data0 = intel_soc_pmic_readb(addr+1);
		if (data0 < 0) {
			pr_err("SWFG: Err in reading 0x%04x register\n",
				addr+1);
			goto out;
		}

		data1 = intel_soc_pmic_readb(addr);
		if (data1 < 0) {
			pr_err("SWFG: Err in reading 0x%04x register\n",
				addr);
			goto out;
		}

		max_vbat = (((u32)data1 << 8)+data0);
		data_valid = (max_vbat != 0);
	}

	if (data_valid)	{
		addr = info_ptr->pmu_cc_base + offset;

		/* Read the LSB register 1st, as this read
			is used to latch MSB */
		data0 = intel_soc_pmic_readb(addr+1);
		if (data0 < 0) {
			pr_err("SWFG: Err in reading 0x%04x register\n",
				addr+1);
			goto out;
		}

		/* Dummy read to workaround register double-buffer bug - START
		 * MSB registers are incorrectly doubled-buffered and a
		 * dummy read after the normal LSB read is required to get
		 * latest contents of MSB
		 */
		dummy = intel_soc_pmic_readb(addr+1);
		if (dummy < 0)
			pr_err("SWFG: Err in reading 0x%04x register\n",
				addr+1);

		/* Dummy read required to workaround
			register double-buffer bug - END */
		data1 = intel_soc_pmic_readb(addr);
		if (data1 < 0) {
			pr_err("SWFG: Err in reading 0x%04x register\n",
				addr);
			goto out;
		}

		ibat_reg = (((u32)data1 << 8)+data0);
		/* Extract two's complement fields from register value. */
		ibat_positive = ibat_reg & PMU_CC_CBAT_POS_MASK;
		ibat_negative = ibat_reg & PMU_CC_CBAT_NEG_MASK;

		/* Calculate signed count value.
		 * The coulomb counter assumes positive IBAT when charging
		 * (according to the datasheet, this is correct).
		 * Our design works on the premise that the IBAT
		 * is negative when charging, so the sign is inverted by
		 * subtracting the negative minus the positive.
		 */
		ibat_count_signed = ibat_negative - ibat_positive;

		/* Return the HW count value scaled to mA. */
		ibat_ma = (int)((ibat_count_signed *
			info_ptr->cc_scaling_uc) / SCALING_UA_TO_MA);
	} else {
		pr_info("SWFG: longterm=%d, latched=%d, Invalid\n",
						longterm, latched);
	}

	pr_info("SWFG: ibat_ma:%d\n", ibat_ma);
	return ibat_ma;
out:
	return ret;
}

/**
 * swfg_hal_init_accumulated_error
 * Initialise the accumulated error data.
 */
static void swfg_hal_init_accumulated_error(void)
{
	u32 cc_up;
	u32 cc_down;
	struct timespec time_now;

	/* Get timestamp for error and IBAT calculation */
	ktime_get_ts(&time_now);

	/* Read raw coulomb counters. */
	swfg_hal_get_coulomb_counts(&cc_up, &cc_down);

	/* Set the reference base for accumulated error calculations. */
	info_ptr->err_base_cc_up	= cc_up;
	info_ptr->err_base_cc_down	= cc_down;
	info_ptr->err_base_rtc_sec	= time_now.tv_sec;
}

/**
 * swfg_hal_calc_accumulated_err -
 * Perform calculation of accumulated error in coulomb
 * counter since last error reset.
 * Returns: Accumulated coulomb count error in mC.
 */
static int swfg_hal_calc_accumulated_err(void)
{
	u32	cc_up;
	u32	cc_down;
	int	cc_delta_up_mc;
	int	cc_delta_down_mc;
	/* 64 Bit arithmetic used to maintain accuracy in uC calculations. */
	s64	error_uc;
	int	error_mc;
	time_t	err_period_sec;
	struct	timeval	rtc_time;
	int	remainder;

	/* Get timestamp. */
	do_gettimeofday(&rtc_time);

	/* Calculate the time elapsed since the last error reset. */
	err_period_sec =
		rtc_time.tv_sec - info_ptr->err_base_rtc_sec;

	/* Read the raw coulomb counter values. */
	swfg_hal_get_coulomb_counts(&cc_up, &cc_down);

	/* Calculate coulomb counter differences in mC over
		the period since last error reset. */
	cc_delta_up_mc = swfg_hal_counts_to_mc(
		(u32)(cc_up   - info_ptr->err_base_cc_up));
	cc_delta_down_mc = swfg_hal_counts_to_mc(
		(u32)(cc_down - info_ptr->err_base_cc_down));

	/* Calculate the offset error. */
	error_uc  = (s64)OFFSET_ERR_UC_PER_S * (s64)err_period_sec;
	/* Add in gain the error for current IN to battery. */
	error_uc += (s64)GAIN_ERR_1_UC_PER_MC * (s64)cc_delta_up_mc;
	/* Add in the gain error for current OUT of the battery. */
	error_uc += (s64)GAIN_ERR_2_UC_PER_MC * (s64)cc_delta_down_mc;
	/*
	 * Convert error to mC for return value.
	 * NOTE: Standard C divide is not supported fot 64 bit values.
	 */
	error_mc = (int)div_s64_rem(error_uc, SCALING_UC_TO_MC, &remainder);
	return error_mc;
}

/**
 * swfg_hal_set_delta_thres -
 * Sets the delta reporting threshold in the HW.
 * @delta_thres_mc		[in] Delta threshold to set. Unit mC.
 */
static void swfg_hal_set_delta_thres(int delta_thres_mc)
{
	u32 delta_thres_cc_thr;
	int ret = 0;

	/* Negative or 0 deltas are not allowed. */
	if (delta_thres_mc <= 0) {
		pr_err("SWFG: Error in reading delta_thres_mc:%d\n",
			delta_thres_mc);
		return;
	}

	/* Calculate threshold value for CC_THR register. */
	delta_thres_cc_thr = (u32)(delta_thres_mc
		/info_ptr->thres_count_scaling_mc);

	/* CC_THR register field is only 6 bits, ensure the
		calculated value fits. */
	if (delta_thres_cc_thr > PMU_CC_THR_LIMIT)
		delta_thres_cc_thr = PMU_CC_THR_LIMIT;

	/* Subtract one as zero counts as one LSB in CC_THR */
	delta_thres_cc_thr -= 1;

	/* Write the calculate value into the CC_THR register */
	ret = intel_soc_pmic_writeb((info_ptr->pmu_cc_base +
			PMU_CC_THR), delta_thres_cc_thr);
	if (ret) {
		pr_err("SWFG: Error in writing 0x%04x register\n",
			PMU_CC_THR);
		return;
	}

	/* Calculate and store the real delta threshold value set */
	info_ptr->delta_thres_mc = (delta_thres_cc_thr + 1) *
			info_ptr->thres_count_scaling_mc;

	if (!info_ptr->delta_thres_set)
		info_ptr->delta_thres_set = true;
}

/**
 * swfg_hal_reset_accumulated_err -
 * Resets the calculated accumulated error for the
 * coulomb counter by storing new baseline values for the counts and rtc.
 */
static void swfg_hal_reset_accumulated_err(void)
{
	struct timeval rtc_time;

	/* Reset base values for accumulated errors. */
	swfg_hal_get_coulomb_counts(&info_ptr->err_base_cc_up,
				&info_ptr->err_base_cc_down);

	/* Timestamp the new base values. */
	do_gettimeofday(&rtc_time);
	info_ptr->err_base_rtc_sec = rtc_time.tv_sec;
}

/**
 * swfg_hal_clear_latched_ibat_avgs -
 * Resets ibat averages latched at OCV.
 */
static void swfg_hal_clear_latched_ibat_avgs(void)
{
	u16 addr;
	int ret = 0;

	addr = info_ptr->pmu_cc_base + PMU_LATCHED_VBAT_MAX;
	/* Clear latched ibat averages */
	ret = intel_soc_pmic_writeb(addr, (u8)(PMU_CLR_VBATMAX));
	if (ret)
		pr_err("SWFG: Error in reading 0x%04x register\n", addr);
}

static int swfg_hal_set(enum swfg_hal_set_key key,
				union swfg_hal_set_params params)
{
	int ret = 0;

	switch (key) {
	case SWFG_HAL_SET_COULOMB_IND_DELTA_THRES:
		swfg_hal_set_delta_thres(params.delta_thres_mc);
		break;

	case SWFG_HAL_SET_ZERO_ACC_CC_ERR:
		/* No parameters. */
		swfg_hal_reset_accumulated_err();
		break;

	case SWFG_HAL_SET_CLEAR_LATCHED_IBAT_AVGS_AT_OCV:
		/* No parameters. */
		swfg_hal_clear_latched_ibat_avgs();
		break;

	default:
		/* Invalid Set key */
		pr_warn("SWFG: file:%s, func:%s, line:%d\n",
			__FILE__, __func__, __LINE__);
		break;
	}
	/* There is no set key that can fail. */
	return ret;
}

static int swfg_hal_get(enum swfg_hal_get_key key,
			union swfg_hal_get_params *p_params)
{
	int error = 0;

	/* Check pointer to return parameters. */
	if (NULL == p_params) {
		pr_err("SWFG: Error in reading params, in func:%s\n",
			__func__);
		error = -EFAULT;
		goto out;
	}

	switch (key) {
	case SWFG_HAL_GET_CC_ACCUMULATED_ERR:
		/* Calculate the accumumated error. */
		p_params->cc_acc_err_mc =
			swfg_hal_calc_accumulated_err();
		break;

	case SWFG_HAL_GET_CC_BALANCED_COUNT:
		/* Call physical layer to read the value from the HW here. */
		p_params->cc_balanced_mc =
			swfg_hal_read_coulomb_counter(key);
		break;
	case SWFG_HAL_GET_CC_POSITIVE_COUNT:
		/* Call physical layer to read the value from the HW here. */
		p_params->cc_positive_mc =
			swfg_hal_read_coulomb_counter(key);
		break;
	case SWFG_HAL_GET_CC_NEGATIVE_COUNT:
		/* Call physical layer to read the value from the HW here. */
		p_params->cc_negative_mc =
			swfg_hal_read_coulomb_counter(key);
		break;

	case SWFG_HAL_GET_IBAT_LOAD_SHORT_TERM_AVG:
		/* Return value of HW Ibat 1 second average current in mA. */
		p_params->ibat_load_short_ma =
			swfg_hal_read_batt_current(false, false);
		break;

	case SWFG_HAL_GET_IBAT_LOAD_SHORT_TERM_AVG_AT_OCV:
		/* Return value of HW Ibat 1 second average current in mA at
			OCV measurement point, ie. the latched version. */
		p_params->ibat_load_short_at_ocv_ma =
			swfg_hal_read_batt_current(false, true);
		break;

	case SWFG_HAL_GET_IBAT_LOAD_LONG_TERM_AVG:
		/* Return value of HW Ibat 5 min average current in mA. */
		p_params->ibat_load_long_at_ocv_ma =
			swfg_hal_read_batt_current(true, false);
		break;

	case SWFG_HAL_GET_IBAT_LOAD_LONG_TERM_AVG_AT_OCV:
		/* Return value of HW Ibat 5 min average current in mA. */
		p_params->ibat_load_long_at_ocv_ma =
			swfg_hal_read_batt_current(true, true);
		break;

	case SWFG_HAL_GET_COULOMB_IND_DELTA_THRES:
		/* If the delta threshold has been set, return it. */
		if (info_ptr->delta_thres_set)
			p_params->delta_thres_mc =
				info_ptr->delta_thres_mc;
		else
			error = -EINVAL;
		break;

	default:
		/* Invalid Get key */
		pr_warn("SWFG: file:%s, func:%s, line:%d\n",
			__FILE__, __func__, __LINE__);
		break;
	}
out:
	return error;
}

/**
 * swfg_hal_init_hw_regs
 * Initialise coulomb counter registers.
 */
static void swfg_hal_init_hw_regs(void)
{
	int ret;

	ret = intel_soc_pmic_writeb((info_ptr->pmu_cc_base + PMU_CC_CTRL0_REG),
							CTRL0_CC_DEF_SET);
	if (ret) {
		pr_err("SWFG: Error in writing 0x%04x register\n",
			PMU_CC_CTRL0_REG);
		return;
	}

	ret = intel_soc_pmic_writeb((info_ptr->pmu_cc_base + PMU_CC_CTRL1_REG),
							CTRL1_CC_DEF_SET);
	if (ret) {
		pr_err("SWFG: Error in writing 0x%04x register\n",
			PMU_CC_CTRL1_REG);
		return;
	}
}

static int intel_wcove_read_adc_val(const char *name, int *raw_val)
{
	int ret, val;
	struct iio_channel *indio_chan;

	indio_chan = iio_channel_get(NULL, name);
	if (IS_ERR_OR_NULL(indio_chan)) {
		ret = PTR_ERR(indio_chan);
		goto exit;
	}
	ret = iio_read_channel_raw(indio_chan, &val);
	if (ret) {
		pr_err("SWFG: IIO channel read error\n");
		goto err_exit;
	}

	*raw_val = val;

err_exit:
	iio_channel_release(indio_chan);
exit:
	return ret;
}

static int intel_wcove_cc_get_vbatt(int *vbatt)
{
	int ret, raw_val;

	ret = intel_wcove_read_adc_val("VBAT", &raw_val);
	if (ret < 0)
		goto vbatt_read_fail;

	*vbatt = raw_val * BATT_VOLT_CONV_FCTR;

vbatt_read_fail:
	return ret;
}

static int intel_wcove_cc_get_peak(int *vpeak)
{
	int ret = 0, raw_val, addr;
	u8 data0, data1;

	/* read lower byte */
	addr = info_ptr->pmu_cc_base;
	data0 = intel_soc_pmic_readb(0x258);
	if (data0 < 0)
		goto vbatt_read_fail;
	/* read higher byte */
	data1 = intel_soc_pmic_readb(0x257);
	if (data1 < 0)
		goto vbatt_read_fail;

	raw_val = data0 | ((data1 & 0x0f) << 8);

	*vpeak = raw_val * BATT_VOLT_CONV_FCTR;
	swfg_hal_clear_latched_ibat_avgs();

vbatt_read_fail:
	return ret;
}

static int intel_wcove_cc_get_btemp(int *btemp)
{
	int ret, raw_val;

	ret = intel_wcove_read_adc_val("SBATTEMP0", &raw_val);
	if (ret < 0)
		goto btemp_read_fail;

	*btemp = (raw_val / SCALE_TEMP_MDEG_TO_DEG) * 10;

btemp_read_fail:
	return ret;
}
static int intel_wcove_cc_get_ibatt(int *ibatt)
{
	union swfg_hal_get_params params;
	int ret;

	ret = swfg_hal_get(SWFG_HAL_GET_IBAT_LOAD_SHORT_TERM_AVG, &params);

	*ibatt = -(params.ibat_load_short_ma * 1000);

	return ret;
}

static int intel_wcove_cc_get_batt_params(int *vbat, int *ibat, int *btemp)
{
	int ret;

	ret = intel_wcove_cc_get_vbatt(vbat);
	if (ret < 0)
		pr_err("SWFG: vbat read error\n");

	ret = intel_wcove_cc_get_ibatt(ibat);
	if (ret < 0)
		pr_err("SWFG: ibatt read error\n");

	ret = intel_wcove_cc_get_btemp(btemp);
	if (ret < 0)
		pr_err("SWFG: bat temp read error\n");

	return ret;
}

static int intel_wcove_cc_get_vocv(int *vocv)
{
	int ret;

	ret = intel_wcove_cc_get_peak(vocv);
	if (ret < 0)
		pr_err("SWFG: vocv read error\n");

	return ret;
}

static int intel_wcove_cc_get_vocv_bootup(int *vocv_bootup)
{
	static int vocv;
	int ret = 0;

	if (!vocv) {
		ret = intel_wcove_cc_get_peak(&vocv);
		if (ret < 0)
			pr_err("SWFG: vocv boot read error\n");
	}
	*vocv_bootup = vocv;

	return ret;
}

static int intel_wcove_cc_get_ibat_bootup(int *ibat_bootup)
{
	union swfg_hal_get_params params;
	static int iboot;
	int ret = 0;

	if (!iboot) {
		ret = swfg_hal_get(SWFG_HAL_GET_IBAT_LOAD_SHORT_TERM_AVG,
				&params);
		if (ret >= 0)
			iboot = params.ibat_load_short_ma;
		else
			pr_err("SWFG: ibatt boot read error\n");
	}
	*ibat_bootup = iboot;

	return ret;
}

static int intel_wcove_cc_get_vavg(int *vavg)
{
	int ret, i, vbat;

	*vavg = 0;
	for (i = 0; i < 3; i++) {
		ret = intel_wcove_cc_get_vbatt(&vbat);
		if (ret < 0)
			pr_err("SWFG: vbat read error\n");
		else
			*vavg += vbat;
	}
	*vavg /= 3;

	return ret;
}

static int intel_wcove_cc_get_iavg(int *iavg)
{
	union swfg_hal_get_params params;
	int ret;

	ret = swfg_hal_get(SWFG_HAL_GET_IBAT_LOAD_LONG_TERM_AVG, &params);
	if (ret < 0) {
		/*
		 * if avg current read fails
		 * return ibatt current itslef.
		 */
		return intel_wcove_cc_get_ibatt(iavg);
	}

	*iavg = -(params.ibat_load_long_at_ocv_ma * 1000);

	return ret;
}

static int intel_wcove_cc_get_deltaq(int *deltaq)
{
	union swfg_hal_get_params params;
	static int delta_charge;
	int ret, tmp_q;

	ret = swfg_hal_get(SWFG_HAL_GET_CC_BALANCED_COUNT, &params);
	if (ret < 0)
		goto out;
	tmp_q = params.cc_balanced_mc - delta_charge;

	/* convert milli coulombs to uAh */
	if (delta_charge == 0)
		*deltaq = 0;
	else
		*deltaq = (tmp_q * (-1) * SCALE_MILLI) / SCALE_MAH_TO_MC;

	delta_charge = params.cc_balanced_mc;
out:
	return ret;
}

static int intel_wcove_cc_calibrate(void)
{
	union swfg_hal_get_params params;
	params.cc_positive_mc = 0;
	/* TODO: Implement calibration */
	return 0;
}

/* Whiskey Cove specific interface wrappers */
int intel_wcove_get_up_cc(int *up_cc)
{
	union swfg_hal_get_params params;
	int ret;

	ret = swfg_hal_get(SWFG_HAL_GET_CC_POSITIVE_COUNT, &params);
	if (ret < 0)
		goto out;

	*up_cc = params.cc_positive_mc;

out:
	return ret;
}

int intel_wcove_get_down_cc(int *down_cc)
{
	union swfg_hal_get_params params;
	int ret;

	ret = swfg_hal_get(SWFG_HAL_GET_CC_POSITIVE_COUNT, &params);
	if (ret < 0)
		goto out;

	*down_cc = params.cc_negative_mc;

out:
	return ret;
}

int intel_wcove_get_acc_err(int *acc_err)
{
	union swfg_hal_get_params params;
	int ret;

	ret = swfg_hal_get(SWFG_HAL_GET_CC_ACCUMULATED_ERR, &params);
	if (ret < 0)
		goto out;

	*acc_err = params.cc_acc_err_mc;

out:
	return ret;
}

int intel_wcove_get_delta_thr(int *delta_thr)
{
	union swfg_hal_get_params params;
	int ret;

	ret = swfg_hal_get(SWFG_HAL_GET_COULOMB_IND_DELTA_THRES,
				&params);
	if (ret < 0)
		goto out;

	*delta_thr = params.delta_thres_mc;

out:
	return ret;
}

int intel_wcove_get_long_avg(int *long_avg)
{
	union swfg_hal_get_params params;
	int ret;

	ret = swfg_hal_get(SWFG_HAL_GET_IBAT_LOAD_LONG_TERM_AVG,
				&params);
	if (ret < 0)
		goto out;

	*long_avg = params.ibat_load_long_at_ocv_ma;

out:
	return ret;
}

int intel_wcove_get_long_avg_ocv(int *long_avg_ocv)
{
	union swfg_hal_get_params params;
	int ret;

	ret = swfg_hal_get(SWFG_HAL_GET_IBAT_LOAD_LONG_TERM_AVG_AT_OCV,
				&params);
	if (ret < 0)
		goto out;

	*long_avg_ocv = params.ibat_load_long_at_ocv_ma;

out:
	return ret;
}

int intel_wcove_get_ocv_accuracy(int *ocv_accuracy)
{
	union swfg_hal_get_params params;
	int ret;

	ret = swfg_hal_get(SWFG_HAL_GET_CC_ACCUMULATED_ERR,
				&params);
	if (ret < 0)
		goto out;

	*ocv_accuracy = params.cc_acc_err_mc;

out:
	return ret;
}

int intel_wcove_reset_acc_err(int acc_err)
{
	union swfg_hal_set_params params;
	int ret;

	/* No parameters required */
	ret = swfg_hal_set(SWFG_HAL_SET_ZERO_ACC_CC_ERR,
				params);
	if (ret < 0)
		goto out;

out:
	return ret;
}

int intel_wcove_set_delta_thr(int delta_thr)
{
	union swfg_hal_set_params params;
	int ret;

	params.delta_thres_mc = delta_thr;
	ret = swfg_hal_set(SWFG_HAL_SET_COULOMB_IND_DELTA_THRES,
				params);
	if (ret < 0)
		goto out;

out:
	return ret;
}

int intel_wcove_clr_latched_ibat_avg(int ibat_avg_at_ocv)
{
	union swfg_hal_set_params params;
	int ret;

	/* No parameters required */
	ret = swfg_hal_set(SWFG_HAL_SET_CLEAR_LATCHED_IBAT_AVGS_AT_OCV,
				params);
	if (ret < 0)
		goto out;

out:
	return ret;
}

bool intel_wcove_wait_for_coulombs_change(void)
{
	int ret;

	ret = wait_for_completion_interruptible(
				&info_ptr->cc_tick_completion);
	if (ret) {
		dev_err(&info_ptr->pdev->dev,
			"Err in wait for completion\n");
		return false;
	}
	return true;
}

static struct intel_fg_input fg_input = {
	.wait_for_cc = &intel_wcove_wait_for_coulombs_change,
	.get_batt_params = &intel_wcove_cc_get_batt_params,
	.get_v_ocv = &intel_wcove_cc_get_vocv,
	.get_v_ocv_bootup = &intel_wcove_cc_get_vocv_bootup,
	.get_i_bat_bootup = &intel_wcove_cc_get_ibat_bootup,
	.get_v_avg = &intel_wcove_cc_get_vavg,
	.get_i_avg = &intel_wcove_cc_get_iavg,
	.get_delta_q = &intel_wcove_cc_get_deltaq,
	.calibrate_cc = &intel_wcove_cc_calibrate,

	.get_up_cc = &intel_wcove_get_up_cc,
	.get_down_cc = &intel_wcove_get_down_cc,
	.get_acc_err = &intel_wcove_get_acc_err,
	.get_delta_thr = &intel_wcove_get_delta_thr,
	.get_long_avg = &intel_wcove_get_long_avg,
	.get_long_avg_ocv = &intel_wcove_get_long_avg_ocv,
	.get_ocv_accuracy = &intel_wcove_get_ocv_accuracy,
	.reset_acc_err = &intel_wcove_reset_acc_err,
	.set_delta_thr = &intel_wcove_set_delta_thr,
	.clr_latched_ibat_avg = &intel_wcove_clr_latched_ibat_avg,
};

static irqreturn_t wcove_cc_threaded_isr(int irq, void *data)
{
	u8 cc_tick;
	/*
	 * TODO: Read second level interrupt register and check if
	 * IRQ that was triggered was for CCTICK interrupt.
	 */

	/*
	 * Read the 2nd Level IRQ register to determine if CCTICK
	 * Interrupt has been fired
	 */
	cc_tick = intel_soc_pmic_readb(CC_TICK_ADCIRQ_REG);
	if (cc_tick < 0) {
		dev_err(&info_ptr->pdev->dev, "Err in reading ADCIRQ REG\n");
		goto out;
	}
	if (cc_tick & CC_TICK_ADCIRQ) {
		/* TODO: Send the notification to SW FG driver */
		complete(&info_ptr->cc_tick_completion);
		/* CLear the IRQ registers */
		intel_soc_pmic_setb(CC_MADCIRQ_REG, CC_MADC_CLR_IRQ);
		intel_soc_pmic_setb(CC_MIRQLVL1_REG, CC_MIRQ_CLR_ADC);
	} else
		reinit_completion(&info_ptr->cc_tick_completion);
out:
	return IRQ_HANDLED;
}
static irqreturn_t wcove_cc_isr(int irq, void *data)
{
	return IRQ_WAKE_THREAD;
}

static void wcove_cc_init_worker(struct work_struct *work)
{
	struct wcove_cc_info *info =
	    container_of(to_delayed_work(work), struct wcove_cc_info,
					init_work.work);
	int ret;

	ret = intel_fg_register_input(&fg_input);
	if (ret < 0)
		dev_err(&info->pdev->dev, "intel FG registration failed\n");
}

static int wcove_cc_probe(struct platform_device *pdev)
{
	struct wcove_cc_info *info;
	int ret;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "mem alloc failed\n");
		return -ENOMEM;
	}

	info->pdev = pdev;

	platform_set_drvdata(pdev, info);
	info_ptr = info;

	info->cc_irq = platform_get_irq(pdev, 0);

	info_ptr->pmu_cc_base = PMU_BASE_ADR;
	info_ptr->cc_scaling_uc = CC_SCALING_UC;
	info_ptr->thres_count_scaling_mc = THRES_CNT_SCALING_MC;

	/* initialize HW registers */
	swfg_hal_init_hw_regs();

	/* Read the initial values of both coulomb counters. */
	swfg_hal_init_accumulated_error();

	/* Register a threaded IRQ handler to hande CCTICK Interrupt */
	ret = request_threaded_irq(info->cc_irq, wcove_cc_isr,
		wcove_cc_threaded_isr, IRQF_SHARED | IRQF_ONESHOT,
		DRIVER_NAME, pdev);

	if (ret) {
		dev_err(&pdev->dev, "Failed to request IRQ for wcove CC\n");
		return ret;
	}

	INIT_DELAYED_WORK(&info->init_work, wcove_cc_init_worker);
	init_completion(&info->cc_tick_completion);

	/*
	 * scheduling the init worker to reduce
	 * delays during boot time. Also delayed
	 * worker is being used to time the
	 * OCV measurment later if neccessary.
	 */
	schedule_delayed_work(&info->init_work, 0);

	return 0;
}

static int wcove_cc_remove(struct platform_device *pdev)
{
	intel_fg_unregister_input(&fg_input);
	return 0;
}

static int wcove_cc_suspend(struct device *dev)
{
	return 0;
}

static int wcove_cc_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops wcove_cc_driver_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(wcove_cc_suspend,
			wcove_cc_resume)
};

static const struct platform_device_id wcove_cc_id[] = {
	{DRIVER_NAME, },
	{ },
};
MODULE_DEVICE_TABLE(platform, &wcove_cc_id);

static struct platform_driver wcove_cc_driver = {
	.probe = wcove_cc_probe,
	.remove = wcove_cc_remove,
	.driver = {
		.name = DRIVER_NAME,
		.pm = &wcove_cc_driver_pm_ops,
		.owner = THIS_MODULE,
	},
	.id_table = wcove_cc_id,
};

module_platform_driver(wcove_cc_driver);

MODULE_AUTHOR("Ramakrishna Pallala <ramakrishna.pallala@intel.com>");
MODULE_AUTHOR("Srinidhi Rao <srinidhi.rao@intel.com>");
MODULE_DESCRIPTION("Whiskey Cove PMIC CC driver");
MODULE_LICENSE("GPL");
