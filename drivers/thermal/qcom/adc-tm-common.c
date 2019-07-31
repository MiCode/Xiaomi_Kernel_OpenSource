// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include "adc-tm.h"

/*
 * Voltage to temperature table for NTCG104EF104 thermistor with
 * 1.875V reference and 100k pull-up.
 */
static const struct adc_tm_map_pt adcmap_100k_104ef_104fb_1875_vref[] = {
	{ 1831000,	-40000 },
	{ 1814000,	-35000 },
	{ 1791000,	-30000 },
	{ 1761000,	-25000 },
	{ 1723000,	-20000 },
	{ 1675000,	-15000 },
	{ 1616000,	-10000 },
	{ 1545000,	-5000 },
	{ 1463000,	0 },
	{ 1370000,	5000 },
	{ 1268000,	10000 },
	{ 1160000,	15000 },
	{ 1049000,	20000 },
	{ 937000,	25000 },
	{ 828000,	30000 },
	{ 726000,	35000 },
	{ 630000,	40000 },
	{ 544000,	45000 },
	{ 467000,	50000 },
	{ 399000,	55000 },
	{ 340000,	60000 },
	{ 290000,	65000 },
	{ 247000,	70000 },
	{ 209000,	75000 },
	{ 179000,	80000 },
	{ 153000,	85000 },
	{ 130000,	90000 },
	{ 112000,	95000 },
	{ 96000,	100000 },
	{ 82000,	105000 },
	{ 71000,	110000 },
	{ 62000,	115000 },
	{ 53000,	120000 },
	{ 46000,	125000 },
};

static void adc_tm_map_voltage_temp(const struct adc_tm_map_pt *pts,
				      size_t tablesize, int input, int *output)
{
	unsigned int descending = 1;
	u32 i = 0;

	/* Check if table is descending or ascending */
	if (tablesize > 1) {
		if (pts[0].x < pts[1].x)
			descending = 0;
	}

	while (i < tablesize) {
		if ((descending) && (pts[i].x < input)) {
			/* table entry is less than measured*/
			 /* value and table is descending, stop */
			break;
		} else if ((!descending) &&
				(pts[i].x > input)) {
			/* table entry is greater than measured*/
			/*value and table is ascending, stop */
			break;
		}
		i++;
	}

	if (i == 0) {
		*output = pts[0].y;
	} else if (i == tablesize) {
		*output = pts[tablesize - 1].y;
	} else {
		/* result is between search_index and search_index-1 */
		/* interpolate linearly */
		*output = (((int32_t)((pts[i].y - pts[i - 1].y) *
			(input - pts[i - 1].x)) /
			(pts[i].x - pts[i - 1].x)) +
			pts[i - 1].y);
	}
}

static void adc_tm_map_temp_voltage(const struct adc_tm_map_pt *pts,
		size_t tablesize, int input, int64_t *output)
{
	unsigned int i = 0, descending = 1;

	/* Check if table is descending or ascending */
	if (tablesize > 1) {
		if (pts[0].y < pts[1].y)
			descending = 0;
	}

	while (i < tablesize) {
		if (descending && (pts[i].y < input)) {
			/*
			 * Table entry is less than measured value.
			 * Table is descending, stop.
			 */
			break;
		} else if (!descending && (pts[i].y > input)) {
			/*
			 * Table entry is greater than measured value.
			 * Table is ascending, stop.
			 */
			break;
		}
		i++;
	}

	if (i == 0) {
		*output = pts[0].x;
	} else if (i == tablesize) {
		*output = pts[tablesize-1].x;
	} else {
		/*
		 * Result is between search_index and search_index-1.
		 * Interpolate linearly.
		 */
		*output = (((int32_t) ((pts[i].x - pts[i-1].x) *
			(input - pts[i-1].y)) /
			(pts[i].y - pts[i-1].y)) +
			pts[i-1].x);
	}
}

int therm_fwd_scale(int64_t code, uint32_t adc_hc_vdd_ref_mv,
			const struct adc_tm_data *data)
{
	int64_t volt = 0;
	int result = 0;

	volt = (s64) code * adc_hc_vdd_ref_mv;
	volt = div64_s64(volt, (data->full_scale_code_volt));

	adc_tm_map_voltage_temp(adcmap_100k_104ef_104fb_1875_vref,
				 ARRAY_SIZE(adcmap_100k_104ef_104fb_1875_vref),
				 (int) volt, &result);

	return result;
}
EXPORT_SYMBOL(therm_fwd_scale);

void adc_tm_scale_therm_voltage_100k(struct adc_tm_config *param,
				const struct adc_tm_data *data)
{
	int temp;

	/* High temperature maps to lower threshold voltage */
	adc_tm_map_temp_voltage(
		adcmap_100k_104ef_104fb_1875_vref,
		ARRAY_SIZE(adcmap_100k_104ef_104fb_1875_vref),
		param->high_thr_temp, &param->low_thr_voltage);

	param->low_thr_voltage *= data->full_scale_code_volt;
	param->low_thr_voltage = div64_s64(param->low_thr_voltage,
						ADC_HC_VDD_REF);

	temp = therm_fwd_scale(param->low_thr_voltage,
				ADC_HC_VDD_REF, data);

	if (temp < param->high_thr_temp)
		param->low_thr_voltage--;

	/* Low temperature maps to higher threshold voltage */
	adc_tm_map_temp_voltage(
		adcmap_100k_104ef_104fb_1875_vref,
		ARRAY_SIZE(adcmap_100k_104ef_104fb_1875_vref),
		param->low_thr_temp, &param->high_thr_voltage);

	param->high_thr_voltage *= data->full_scale_code_volt;
	param->high_thr_voltage = div64_s64(param->high_thr_voltage,
						ADC_HC_VDD_REF);

	temp = therm_fwd_scale(param->high_thr_voltage,
				ADC_HC_VDD_REF, data);

	if (temp > param->low_thr_temp)
		param->high_thr_voltage++;

}
EXPORT_SYMBOL(adc_tm_scale_therm_voltage_100k);

int32_t adc_tm_absolute_rthr(const struct adc_tm_data *data,
			struct adc_tm_config *tm_config)
{
	int64_t low_thr = 0, high_thr = 0;

	low_thr =  div_s64(tm_config->low_thr_voltage, tm_config->prescal);
	low_thr *= data->full_scale_code_volt;
	low_thr = div64_s64(low_thr, ADC_HC_VDD_REF);
	tm_config->low_thr_voltage = low_thr;

	high_thr =  div_s64(tm_config->high_thr_voltage, tm_config->prescal);
	high_thr *= data->full_scale_code_volt;
	high_thr = div64_s64(high_thr, ADC_HC_VDD_REF);
	tm_config->high_thr_voltage = high_thr;

	return 0;
}
EXPORT_SYMBOL(adc_tm_absolute_rthr);

MODULE_DESCRIPTION("Qualcomm Technologies Inc. PMIC ADC_TM common driver");
MODULE_LICENSE("GPL v2");
