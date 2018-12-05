/*
 * Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include "adc-tm.h"

/*
 * Voltage to temperature table for NTCG104EF104 thermistor with
 * 1.875V reference and 100k pull-up.
 */
static const struct adc_tm_map_pt adcmap_100k_104ef_104fb_1875_vref[] = {
	{ 1831,	-40000 },
	{ 1814,	-35000 },
	{ 1791,	-30000 },
	{ 1761,	-25000 },
	{ 1723,	-20000 },
	{ 1675,	-15000 },
	{ 1616,	-10000 },
	{ 1545,	-5000 },
	{ 1463,	0 },
	{ 1370,	5000 },
	{ 1268,	10000 },
	{ 1160,	15000 },
	{ 1049,	20000 },
	{ 937,	25000 },
	{ 828,	30000 },
	{ 726,	35000 },
	{ 630,	40000 },
	{ 544,	45000 },
	{ 467,	50000 },
	{ 399,	55000 },
	{ 340,	60000 },
	{ 290,	65000 },
	{ 247,	70000 },
	{ 209,	75000 },
	{ 179,	80000 },
	{ 153,	85000 },
	{ 130,	90000 },
	{ 112,	95000 },
	{ 96,	100000 },
	{ 82,	105000 },
	{ 71,	110000 },
	{ 62,	115000 },
	{ 53,	120000 },
	{ 46,	125000 },
};

static void adc_tm_map_temp_voltage(const struct adc_tm_map_pt *pts,
		size_t tablesize, int input, int64_t *output)
{
	bool descending = true;
	unsigned int i = 0;

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

void adc_tm_scale_therm_voltage_100k(struct adc_tm_config *param,
				const struct adc_tm_data *data)
{
	uint32_t adc_hc_vdd_ref_mv = 1875;

	/* High temperature maps to lower threshold voltage */
	adc_tm_map_temp_voltage(
		adcmap_100k_104ef_104fb_1875_vref,
		ARRAY_SIZE(adcmap_100k_104ef_104fb_1875_vref),
		param->high_thr_temp, &param->low_thr_voltage);

	param->low_thr_voltage *= data->full_scale_code_volt;
	param->low_thr_voltage = div64_s64(param->low_thr_voltage,
						adc_hc_vdd_ref_mv);

	/* Low temperature maps to higher threshold voltage */
	adc_tm_map_temp_voltage(
		adcmap_100k_104ef_104fb_1875_vref,
		ARRAY_SIZE(adcmap_100k_104ef_104fb_1875_vref),
		param->low_thr_temp, &param->high_thr_voltage);

	param->high_thr_voltage *= data->full_scale_code_volt;
	param->high_thr_voltage = div64_s64(param->high_thr_voltage,
						adc_hc_vdd_ref_mv);

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
