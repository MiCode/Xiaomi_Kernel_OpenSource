/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#ifndef __PM8XXX_BMS_H
#define __PM8XXX_BMS_H

#include <linux/errno.h>

#define PM8921_BMS_DEV_NAME	"pm8921-bms"

#define FCC_CC_COLS		5
#define FCC_TEMP_COLS		8

#define PC_CC_ROWS             29
#define PC_CC_COLS             13

#define PC_TEMP_ROWS		29
#define PC_TEMP_COLS		8

#define MAX_SINGLE_LUT_COLS	20

struct single_row_lut {
	int x[MAX_SINGLE_LUT_COLS];
	int y[MAX_SINGLE_LUT_COLS];
	int cols;
};

/**
 * struct sf_lut -
 * @rows:	number of percent charge entries should be <= PC_CC_ROWS
 * @cols:	number of charge cycle entries should be <= PC_CC_COLS
 * @row_entries:	the charge cycles/temperature at which sf data
 *			is available in the table.
 *		The charge cycles must be in increasing order from 0 to rows.
 * @percent:	the percent charge at which sf data is available in the table
 *		The  percentcharge must be in decreasing order from 0 to cols.
 * @sf:		the scaling factor data
 */
struct sf_lut {
	int rows;
	int cols;
	int row_entries[PC_CC_COLS];
	int percent[PC_CC_ROWS];
	int sf[PC_CC_ROWS][PC_CC_COLS];
};

/**
 * struct pc_temp_ocv_lut -
 * @rows:	number of percent charge entries should be <= PC_TEMP_ROWS
 * @cols:	number of temperature entries should be <= PC_TEMP_COLS
 * @temp:	the temperatures at which ocv data is available in the table
 *		The temperatures must be in increasing order from 0 to rows.
 * @percent:	the percent charge at which ocv data is available in the table
 *		The  percentcharge must be in decreasing order from 0 to cols.
 * @ocv:	the open circuit voltage
 */
struct pc_temp_ocv_lut {
	int rows;
	int cols;
	int temp[PC_TEMP_COLS];
	int percent[PC_TEMP_ROWS];
	int ocv[PC_TEMP_ROWS][PC_TEMP_COLS];
};

/**
 * struct pm8921_bms_battery_data -
 * @fcc:		full charge capacity (mAmpHour)
 * @fcc_temp_lut:	table to get fcc at a given temp
 * @pc_temp_ocv_lut:	table to get percent charge given batt temp and cycles
 * @pc_sf_lut:		table to get percent charge scaling factor given cycles
 *			and percent charge
 * @rbatt_sf_lut:	table to get battery resistance scaling factor given
 *			temperature and percent charge
 * @default_rbatt_mohm:	the default value of battery resistance to use when
 *			readings from bms are not available.
 * @delta_rbatt_mohm:	the resistance to be added towards lower soc to
 *			compensate for battery capacitance.
 */
struct pm8921_bms_battery_data {
	unsigned int		fcc;
	struct single_row_lut	*fcc_temp_lut;
	struct single_row_lut	*fcc_sf_lut;
	struct pc_temp_ocv_lut	*pc_temp_ocv_lut;
	struct sf_lut		*pc_sf_lut;
	struct sf_lut		*rbatt_sf_lut;
	int			default_rbatt_mohm;
	int			delta_rbatt_mohm;
};

struct pm8xxx_bms_core_data {
	unsigned int	batt_temp_channel;
	unsigned int	vbat_channel;
	unsigned int	ref625mv_channel;
	unsigned int	ref1p25v_channel;
	unsigned int	batt_id_channel;
};

enum battery_type {
	BATT_UNKNOWN = 0,
	BATT_PALLADIUM,
	BATT_DESAY,
};

/**
 * struct pm8921_bms_platform_data -
 * @batt_type:		allows to force chose battery calibration data
 * @r_sense:		sense resistor value in (mOhms)
 * @i_test:		current at which the unusable charger cutoff is to be
 *			calculated or the peak system current (mA)
 * @v_failure:		the voltage at which the battery is considered empty(mV)
 * @calib_delay_ms:	how often should the adc calculate gain and offset
 * @enable_fcc_learning:	if set the driver will learn full charge
 *				capacity of the battery upon end of charge
 */
struct pm8921_bms_platform_data {
	struct pm8xxx_bms_core_data	bms_cdata;
	enum battery_type		battery_type;
	unsigned int			r_sense;
	unsigned int			i_test;
	unsigned int			v_failure;
	unsigned int			calib_delay_ms;
	unsigned int			max_voltage_uv;
	unsigned int			rconn_mohm;
	int				enable_fcc_learning;
};

#if defined(CONFIG_PM8921_BMS) || defined(CONFIG_PM8921_BMS_MODULE)
extern struct pm8921_bms_battery_data  palladium_1500_data;
extern struct pm8921_bms_battery_data  desay_5200_data;
/**
 * pm8921_bms_get_vsense_avg - return the voltage across the sense
 *				resitor in microvolts
 * @result:	The pointer where the voltage will be updated. A -ve
 *		result means that the current is flowing in
 *		the battery - during battery charging
 *
 * RETURNS:	Error code if there was a problem reading vsense, Zero otherwise
 *		The result won't be updated in case of an error.
 *
 *
 */
int pm8921_bms_get_vsense_avg(int *result);

/**
 * pm8921_bms_get_battery_current - return the battery current based on vsense
 *				resitor in microamperes
 * @result:	The pointer where the voltage will be updated. A -ve
 *		result means that the current is flowing in
 *		the battery - during battery charging
 *
 * RETURNS:	Error code if there was a problem reading vsense, Zero otherwise
 *		The result won't be updated in case of an error.
 *
 */
int pm8921_bms_get_battery_current(int *result);

/**
 * pm8921_bms_get_percent_charge - returns the current battery charge in percent
 *
 */
int pm8921_bms_get_percent_charge(void);

/**
 * pm8921_bms_get_fcc - returns fcc in mAh of the battery depending on its age
 *			and temperature
 *
 */
int pm8921_bms_get_fcc(void);

/**
 * pm8921_bms_charging_began - function to notify the bms driver that charging
 *				has started. Used by the bms driver to keep
 *				track of chargecycles
 */
void pm8921_bms_charging_began(void);
/**
 * pm8921_bms_charging_end - function to notify the bms driver that charging
 *				has stopped. Used by the bms driver to keep
 *				track of chargecycles
 */
void pm8921_bms_charging_end(int is_battery_full);

void pm8921_bms_calibrate_hkadc(void);
/**
 * pm8921_bms_get_simultaneous_battery_voltage_and_current
 *		- function to take simultaneous vbat and vsense readings
 *		  this puts the bms in override mode but keeps coulumb couting
 *		  on. Useful when ir compensation needs to be implemented
 */
int pm8921_bms_get_simultaneous_battery_voltage_and_current(int *ibat_ua,
								int *vbat_uv);
/**
 * pm8921_bms_get_rbatt - function to get the battery resistance in mOhm.
 */
int pm8921_bms_get_rbatt(void);
#else
static inline int pm8921_bms_get_vsense_avg(int *result)
{
	return -ENXIO;
}
static inline int pm8921_bms_get_battery_current(int *result)
{
	return -ENXIO;
}
static inline int pm8921_bms_get_percent_charge(void)
{
	return -ENXIO;
}
static inline int pm8921_bms_get_fcc(void)
{
	return -ENXIO;
}
static inline void pm8921_bms_charging_began(void)
{
}
static inline void pm8921_bms_charging_end(int is_battery_full)
{
}
static inline void pm8921_bms_calibrate_hkadc(void)
{
}
static inline int pm8921_bms_get_simultaneous_battery_voltage_and_current(
						int *ibat_ua, int *vbat_uv)
{
	return -ENXIO;
}
static inline int pm8921_bms_get_rbatt(void)
{
	return -EINVAL;
}
#endif

#endif
