/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
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

#ifndef __PMIC8XXX_CCADC_H__
#define __PMIC8XXX_CCADC_H__

#include <linux/mfd/pm8xxx/core.h>

#define PM8XXX_CCADC_DEV_NAME "pm8xxx-ccadc"

/**
 * struct pm8xxx_ccadc_platform_data -
 * @r_sense:		sense resistor value in (mOhms)
 */
struct pm8xxx_ccadc_platform_data {
	int r_sense;
};

#define CCADC_READING_RESOLUTION_N_V1	1085069
#define CCADC_READING_RESOLUTION_D_V1	100000
#define CCADC_READING_RESOLUTION_N_V2	542535
#define CCADC_READING_RESOLUTION_D_V2	100000

static s64 pm8xxx_ccadc_reading_to_microvolt_v1(s64 cc)
{
	return div_s64(cc * CCADC_READING_RESOLUTION_N_V1,
					CCADC_READING_RESOLUTION_D_V1);
}

static s64 pm8xxx_ccadc_reading_to_microvolt_v2(s64 cc)
{
	return div_s64(cc * CCADC_READING_RESOLUTION_N_V2,
					CCADC_READING_RESOLUTION_D_V2);
}

static inline s64 pm8xxx_ccadc_reading_to_microvolt(int revision, s64 cc)
{
	/*
	 * resolution (the value of a single bit) was changed after revision 2.0
	 * for more accurate readings
	 */
	return (revision < PM8XXX_REVISION_8921_2p0) ?
				pm8xxx_ccadc_reading_to_microvolt_v1((s64)cc) :
				pm8xxx_ccadc_reading_to_microvolt_v2((s64)cc);
}

#if defined(CONFIG_PM8XXX_CCADC) || defined(CONFIG_PM8XXX_CCADC_MODULE)
/**
 * pm8xxx_cc_adjust_for_gain - the function to adjust the voltage read from
 *			ccadc for gain compensation
 * @v:	the voltage which needs to be gain compensated in microVolts
 *
 *
 * RETURNS: gain compensated voltage
 */
s64 pm8xxx_cc_adjust_for_gain(s64 uv);

/**
 * pm8xxx_calib_ccadc - calibration for ccadc. This will calculate gain
 *			and offset and reprogram them in the appropriate
 *			registers
 */
void pm8xxx_calib_ccadc(void);

/**
 * pm8xxx_ccadc_get_battery_current - return the battery current based on vsense
 *				resitor in microamperes
 * @result:	The pointer where the voltage will be updated. A -ve
 *		result means that the current is flowing in
 *		the battery - during battery charging
 *
 * RETURNS:	Error code if there was a problem reading vsense, Zero otherwise
 *		The result won't be updated in case of an error.
 *
 */
int pm8xxx_ccadc_get_battery_current(int *bat_current);
#else
static inline s64 pm8xxx_cc_adjust_for_gain(s64 uv)
{
	return -ENXIO;
}
static inline void pm8xxx_calib_ccadc(void)
{
}
static inline int pm8xxx_ccadc_get_battery_current(int *bat_current)
{
	return -ENXIO;
}
#endif

#endif /* __PMIC8XXX_CCADC_H__ */
