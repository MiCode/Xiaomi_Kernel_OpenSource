// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/iio/iio.h>
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

/*
 * Resistance to temperature table for NTCG104EF104 thermistor with
 * 100k pull-up.
 */
static const struct adc_tm_map_pt adcmap_100k_adc7[] = {
	{ 4250657, -40960 },
	{ 3962085, -39936 },
	{ 3694875, -38912 },
	{ 3447322, -37888 },
	{ 3217867, -36864 },
	{ 3005082, -35840 },
	{ 2807660, -34816 },
	{ 2624405, -33792 },
	{ 2454218, -32768 },
	{ 2296094, -31744 },
	{ 2149108, -30720 },
	{ 2012414, -29696 },
	{ 1885232, -28672 },
	{ 1766846, -27648 },
	{ 1656598, -26624 },
	{ 1553884, -25600 },
	{ 1458147, -24576 },
	{ 1368873, -23552 },
	{ 1285590, -22528 },
	{ 1207863, -21504 },
	{ 1135290, -20480 },
	{ 1067501, -19456 },
	{ 1004155, -18432 },
	{ 944935, -17408 },
	{ 889550, -16384 },
	{ 837731, -15360 },
	{ 789229, -14336 },
	{ 743813, -13312 },
	{ 701271, -12288 },
	{ 661405, -11264 },
	{ 624032, -10240 },
	{ 588982, -9216 },
	{ 556100, -8192 },
	{ 525239, -7168 },
	{ 496264, -6144 },
	{ 469050, -5120 },
	{ 443480, -4096 },
	{ 419448, -3072 },
	{ 396851, -2048 },
	{ 375597, -1024 },
	{ 355598, 0 },
	{ 336775, 1024 },
	{ 319052, 2048 },
	{ 302359, 3072 },
	{ 286630, 4096 },
	{ 271806, 5120 },
	{ 257829, 6144 },
	{ 244646, 7168 },
	{ 232209, 8192 },
	{ 220471, 9216 },
	{ 209390, 10240 },
	{ 198926, 11264 },
	{ 189040, 12288 },
	{ 179698, 13312 },
	{ 170868, 14336 },
	{ 162519, 15360 },
	{ 154622, 16384 },
	{ 147150, 17408 },
	{ 140079, 18432 },
	{ 133385, 19456 },
	{ 127046, 20480 },
	{ 121042, 21504 },
	{ 115352, 22528 },
	{ 109960, 23552 },
	{ 104848, 24576 },
	{ 100000, 25600 },
	{ 95402, 26624 },
	{ 91038, 27648 },
	{ 86897, 28672 },
	{ 82965, 29696 },
	{ 79232, 30720 },
	{ 75686, 31744 },
	{ 72316, 32768 },
	{ 69114, 33792 },
	{ 66070, 34816 },
	{ 63176, 35840 },
	{ 60423, 36864 },
	{ 57804, 37888 },
	{ 55312, 38912 },
	{ 52940, 39936 },
	{ 50681, 40960 },
	{ 48531, 41984 },
	{ 46482, 43008 },
	{ 44530, 44032 },
	{ 42670, 45056 },
	{ 40897, 46080 },
	{ 39207, 47104 },
	{ 37595, 48128 },
	{ 36057, 49152 },
	{ 34590, 50176 },
	{ 33190, 51200 },
	{ 31853, 52224 },
	{ 30577, 53248 },
	{ 29358, 54272 },
	{ 28194, 55296 },
	{ 27082, 56320 },
	{ 26020, 57344 },
	{ 25004, 58368 },
	{ 24033, 59392 },
	{ 23104, 60416 },
	{ 22216, 61440 },
	{ 21367, 62464 },
	{ 20554, 63488 },
	{ 19776, 64512 },
	{ 19031, 65536 },
	{ 18318, 66560 },
	{ 17636, 67584 },
	{ 16982, 68608 },
	{ 16355, 69632 },
	{ 15755, 70656 },
	{ 15180, 71680 },
	{ 14628, 72704 },
	{ 14099, 73728 },
	{ 13592, 74752 },
	{ 13106, 75776 },
	{ 12640, 76800 },
	{ 12192, 77824 },
	{ 11762, 78848 },
	{ 11350, 79872 },
	{ 10954, 80896 },
	{ 10574, 81920 },
	{ 10209, 82944 },
	{ 9858, 83968 },
	{ 9521, 84992 },
	{ 9197, 86016 },
	{ 8886, 87040 },
	{ 8587, 88064 },
	{ 8299, 89088 },
	{ 8023, 90112 },
	{ 7757, 91136 },
	{ 7501, 92160 },
	{ 7254, 93184 },
	{ 7017, 94208 },
	{ 6789, 95232 },
	{ 6570, 96256 },
	{ 6358, 97280 },
	{ 6155, 98304 },
	{ 5959, 99328 },
	{ 5770, 100352 },
	{ 5588, 101376 },
	{ 5412, 102400 },
	{ 5243, 103424 },
	{ 5080, 104448 },
	{ 4923, 105472 },
	{ 4771, 106496 },
	{ 4625, 107520 },
	{ 4484, 108544 },
	{ 4348, 109568 },
	{ 4217, 110592 },
	{ 4090, 111616 },
	{ 3968, 112640 },
	{ 3850, 113664 },
	{ 3736, 114688 },
	{ 3626, 115712 },
	{ 3519, 116736 },
	{ 3417, 117760 },
	{ 3317, 118784 },
	{ 3221, 119808 },
	{ 3129, 120832 },
	{ 3039, 121856 },
	{ 2952, 122880 },
	{ 2868, 123904 },
	{ 2787, 124928 },
	{ 2709, 125952 },
	{ 2633, 126976 },
	{ 2560, 128000 },
	{ 2489, 129024 },
	{ 2420, 130048 }
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

/* Used by thermal clients to read ADC channel temperature*/
int adc_tm_get_temp_vadc(struct adc_tm_sensor *sensor, int *temp)
{
	if (!sensor || !sensor->adc)
		return -EINVAL;

	return iio_read_channel_processed(sensor->adc, temp);
}

int32_t adc_tm_read_reg(struct adc_tm_chip *chip,
					int16_t reg, u8 *data, int len)
{
	int ret;

	ret = regmap_bulk_read(chip->regmap, (chip->base + reg), data, len);
	if (ret < 0)
		pr_err("adc-tm read reg %d failed with %d\n", reg, ret);

	return ret;
}

int32_t adc_tm_write_reg(struct adc_tm_chip *chip,
					int16_t reg, u8 *data, int len)
{
	int ret;

	ret = regmap_bulk_write(chip->regmap, (chip->base + reg), data, len);
	if (ret < 0)
		pr_err("adc-tm write reg %d failed with %d\n", reg, ret);

	return ret;
}

int adc_tm_is_valid(struct adc_tm_chip *chip)
{
	struct adc_tm_chip *adc_tm_chip = NULL;

	list_for_each_entry(adc_tm_chip, chip->device_list, list)
		if (chip == adc_tm_chip)
			return 0;

	return -EINVAL;
}

int therm_fwd_scale(int64_t code, uint32_t adc_hc_vdd_ref_mv,
			const struct adc_tm_data *data)
{
	int64_t volt = 0;
	int result = 0;

	volt = (s64) code * adc_hc_vdd_ref_mv;
	volt = div64_s64(volt, (data->full_scale_code_volt));

	/* Same API can be used for resistance-temperature table */
	adc_tm_map_voltage_temp(adcmap_100k_104ef_104fb_1875_vref,
				 ARRAY_SIZE(adcmap_100k_104ef_104fb_1875_vref),
				 (int) volt, &result);

	return result;
}

int therm_fwd_scale_adc7(int64_t code)
{
	int64_t resistance = 0;
	int result = 0;

	if (code >= RATIO_MAX_ADC7)
		return -EINVAL;

	resistance = (s64) code * R_PU_100K;
	resistance = div64_s64(resistance, (RATIO_MAX_ADC7 - code));

	adc_tm_map_voltage_temp(adcmap_100k_adc7,
				 ARRAY_SIZE(adcmap_100k_adc7),
				 (int) resistance, &result);

	return result;
}

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

void adc_tm_scale_therm_voltage_100k_adc7(struct adc_tm_config *param)
{
	int temp;
	int64_t resistance = 0;

	/* High temperature maps to lower threshold voltage */
	/* Same API can be used for resistance-temperature table */
	adc_tm_map_temp_voltage(
		adcmap_100k_adc7,
		ARRAY_SIZE(adcmap_100k_adc7),
		param->high_thr_temp, &resistance);

	param->low_thr_voltage = resistance * RATIO_MAX_ADC7;
	param->low_thr_voltage = div64_s64(param->low_thr_voltage,
						(resistance + R_PU_100K));

	temp = therm_fwd_scale_adc7(param->low_thr_voltage);
	if (temp == -EINVAL)
		return;

	if (temp < param->high_thr_temp)
		param->low_thr_voltage--;

	/* Low temperature maps to higher threshold voltage */
	/* Same API can be used for resistance-temperature table */
	adc_tm_map_temp_voltage(
		adcmap_100k_adc7,
		ARRAY_SIZE(adcmap_100k_adc7),
		param->low_thr_temp, &resistance);

	param->high_thr_voltage = resistance * RATIO_MAX_ADC7;
	param->high_thr_voltage = div64_s64(param->high_thr_voltage,
						(resistance + R_PU_100K));

	temp = therm_fwd_scale_adc7(param->high_thr_voltage);

	if (temp > param->low_thr_temp)
		param->high_thr_voltage++;
}

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

int32_t adc_tm_absolute_rthr_adc7(struct adc_tm_config *tm_config)
{
	int64_t low_thr = 0, high_thr = 0;

	low_thr =  div_s64(tm_config->low_thr_voltage, tm_config->prescal);
	low_thr *= MAX_CODE_VOLT;

	low_thr = div64_s64(low_thr, ADC_HC_VDD_REF);
	tm_config->low_thr_voltage = low_thr;

	high_thr =  div_s64(tm_config->high_thr_voltage, tm_config->prescal);
	high_thr *= MAX_CODE_VOLT;

	high_thr = div64_s64(high_thr, ADC_HC_VDD_REF);
	tm_config->high_thr_voltage = high_thr;

	return 0;
}

MODULE_DESCRIPTION("Qualcomm Technologies Inc. PMIC ADC_TM common driver");
MODULE_LICENSE("GPL v2");
