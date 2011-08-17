/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/mfd/pm8921-adc.h>
#define KELVINMIL_DEGMIL	273160
#define PM8921_ADC_SLOPE	10
#define PM8921_ADC_CODE_SCALE	24576

static const struct pm8921_adc_map_pt adcmap_batttherm[] = {
	{2020,	-30},
	{1923,	-20},
	{1796,	-10},
	{1640,	  0},
	{1459,	 10},
	{1260,	 20},
	{1159,	 25},
	{1059,	 30},
	{871,	 40},
	{706,	 50},
	{567,	 60},
	{453,	 70},
	{364,	 80}
};

static const struct pm8921_adc_map_pt adcmap_ntcg_104ef_104fb[] = {
	{696483,	-40960},
	{649148,	-39936},
	{605368,	-38912},
	{564809,	-37888},
	{527215,	-36864},
	{492322,	-35840},
	{460007,	-34816},
	{429982,	-33792},
	{402099,	-32768},
	{376192,	-31744},
	{352075,	-30720},
	{329714,	-29696},
	{308876,	-28672},
	{289480,	-27648},
	{271417,	-26624},
	{254574,	-25600},
	{238903,	-24576},
	{224276,	-23552},
	{210631,	-22528},
	{197896,	-21504},
	{186007,	-20480},
	{174899,	-19456},
	{164521,	-18432},
	{154818,	-17408},
	{145744,	-16384},
	{137265,	-15360},
	{129307,	-14336},
	{121866,	-13312},
	{114896,	-12288},
	{108365,	-11264},
	{102252,	-10240},
	{96499,		-9216},
	{91111,		-8192},
	{86055,		-7168},
	{81308,		-6144},
	{76857,		-5120},
	{72660,		-4096},
	{68722,		-3072},
	{65020,		-2048},
	{61538,		-1024},
	{58261,		0},
	{55177,		1024},
	{52274,		2048},
	{49538,		3072},
	{46962,		4096},
	{44531,		5120},
	{42243,		6144},
	{40083,		7168},
	{38045,		8192},
	{36122,		9216},
	{34308,		10240},
	{32592,		11264},
	{30972,		12288},
	{29442,		13312},
	{27995,		14336},
	{26624,		15360},
	{25333,		16384},
	{24109,		17408},
	{22951,		18432},
	{21854,		19456},
	{20807,		20480},
	{19831,		21504},
	{18899,		22528},
	{18016,		23552},
	{17178,		24576},
	{16384,		25600},
	{15631,		26624},
	{14916,		27648},
	{14237,		28672},
	{13593,		29696},
	{12976,		30720},
	{12400,		31744},
	{11848,		32768},
	{11324,		33792},
	{10825,		34816},
	{10354,		35840},
	{9900,		36864},
	{9471,		37888},
	{9062,		38912},
	{8674,		39936},
	{8306,		40960},
	{7951,		41984},
	{7616,		43008},
	{7296,		44032},
	{6991,		45056},
	{6701,		46080},
	{6424,		47104},
	{6160,		48128},
	{5908,		49152},
	{5667,		50176},
	{5439,		51200},
	{5219,		52224},
	{5010,		53248},
	{4810,		54272},
	{4619,		55296},
	{4440,		56320},
	{4263,		57344},
	{4097,		58368},
	{3938,		59392},
	{3785,		60416},
	{3637,		61440},
	{3501,		62464},
	{3368,		63488},
	{3240,		64512},
	{3118,		65536},
	{2998,		66560},
	{2889,		67584},
	{2782,		68608},
	{2680,		69632},
	{2581,		70656},
	{2490,		71680},
	{2397,		72704},
	{2310,		73728},
	{2227,		74752},
	{2147,		75776},
	{2064,		76800},
	{1998,		77824},
	{1927,		78848},
	{1860,		79872},
	{1795,		80896},
	{1736,		81920},
	{1673,		82944},
	{1615,		83968},
	{1560,		84992},
	{1507,		86016},
	{1456,		87040},
	{1407,		88064},
	{1360,		89088},
	{1314,		90112},
	{1271,		91136},
	{1228,		92160},
	{1189,		93184},
	{1150,		94208},
	{1112,		95232},
	{1076,		96256},
	{1042,		97280},
	{1008,		98304},
	{976,		99328},
	{945,		100352},
	{915,		101376},
	{886,		102400},
	{859,		103424},
	{832,		104448},
	{807,		105472},
	{782,		106496},
	{756,		107520},
	{735,		108544},
	{712,		109568},
	{691,		110592},
	{670,		111616},
	{650,		112640},
	{631,		113664},
	{612,		114688},
	{594,		115712},
	{577,		116736},
	{560,		117760},
	{544,		118784},
	{528,		119808},
	{513,		120832},
	{498,		121856},
	{483,		122880},
	{470,		123904},
	{457,		124928},
	{444,		125952},
	{431,		126976},
	{419,		128000}
};

static int32_t pm8921_adc_map_linear(const struct pm8921_adc_map_pt *pts,
		uint32_t tablesize, int32_t input, int64_t *output)
{
	bool descending = 1;
	uint32_t i = 0;

	if ((pts == NULL) || (output == NULL))
		return -EINVAL;

	/* Check if table is descending or ascending */
	if (tablesize > 1) {
		if (pts[0].x < pts[1].x)
			descending = 0;
	}

	while (i < tablesize) {
		if ((descending == 1) && (pts[i].x < input)) {
			/* table entry is less than measured
				value and table is descending, stop */
			break;
		} else if ((descending == 0) &&
				(pts[i].x > input)) {
			/* table entry is greater than measured
				value and table is ascending, stop */
			break;
		} else {
			i++;
		}
	}

	if (i == 0)
		*output = pts[0].y;
	else if (i == tablesize)
		*output = pts[tablesize-1].y;
	else {
		/* result is between search_index and search_index-1 */
		/* interpolate linearly */
		*output = (((int32_t) ((pts[i].y - pts[i-1].y)*
			(input - pts[i-1].x))/
			(pts[i].x - pts[i-1].x))+
			pts[i-1].y);
	}

	return 0;
}

int32_t pm8921_adc_scale_default(int32_t adc_code,
		const struct pm8921_adc_properties *adc_properties,
		const struct pm8921_adc_chan_properties *chan_properties,
		struct pm8921_adc_chan_result *adc_chan_result)
{
	bool negative_rawfromoffset = 0;
	int32_t rawfromoffset = 0;

	if (!chan_properties || !chan_properties->offset_gain_numerator ||
		!chan_properties->offset_gain_denominator || !adc_properties
		|| !adc_chan_result)
		return -EINVAL;

	rawfromoffset = adc_code -
			chan_properties->adc_graph[ADC_CALIB_ABSOLUTE].offset;

	adc_chan_result->adc_code = adc_code;
	if (rawfromoffset < 0) {
		if (adc_properties->bipolar) {
			rawfromoffset = -rawfromoffset;
			negative_rawfromoffset = 1;
		} else {
			rawfromoffset = 0;
		}
	}

	if (rawfromoffset >= 1 << adc_properties->bitresolution)
		rawfromoffset = (1 << adc_properties->bitresolution) - 1;

	adc_chan_result->measurement = (int64_t)rawfromoffset *
		chan_properties->adc_graph[ADC_CALIB_ABSOLUTE].dx *
				chan_properties->offset_gain_denominator;

	/* do_div only perform positive integer division! */
	do_div(adc_chan_result->measurement,
		chan_properties->adc_graph[ADC_CALIB_ABSOLUTE].dy *
				chan_properties->offset_gain_numerator);

	if (negative_rawfromoffset)
		adc_chan_result->measurement = -adc_chan_result->measurement;

	/* Note: adc_chan_result->measurement is in the unit of
	 * adc_properties.adc_reference. For generic channel processing,
	 * channel measurement is a scale/ratio relative to the adc
	 * reference input */
	adc_chan_result->physical = (int32_t) adc_chan_result->measurement;

	return 0;
}
EXPORT_SYMBOL_GPL(pm8921_adc_scale_default);

int32_t pm8921_adc_scale_batt_therm(int32_t adc_code,
		const struct pm8921_adc_properties *adc_properties,
		const struct pm8921_adc_chan_properties *chan_properties,
		struct pm8921_adc_chan_result *adc_chan_result)
{
	int rc;

	rc = pm8921_adc_scale_default(adc_code, adc_properties, chan_properties,
			adc_chan_result);
	if (rc < 0) {
		pr_debug("PM8921 ADC scale default error with %d\n", rc);
		return rc;
	}
	/* convert mV ---> degC using the table */
	return pm8921_adc_map_linear(
			adcmap_batttherm,
			sizeof(adcmap_batttherm)/sizeof(adcmap_batttherm[0]),
			adc_chan_result->physical,
			&adc_chan_result->physical);
}
EXPORT_SYMBOL_GPL(pm8921_adc_scale_batt_therm);

int32_t pm8921_adc_scale_pmic_therm(int32_t adc_code,
		const struct pm8921_adc_properties *adc_properties,
		const struct pm8921_adc_chan_properties *chan_properties,
		struct pm8921_adc_chan_result *adc_chan_result)
{
	int32_t rawfromoffset;

	if (!chan_properties || !chan_properties->offset_gain_numerator ||
		!chan_properties->offset_gain_denominator || !adc_properties
		|| !adc_chan_result)
		return -EINVAL;

	adc_chan_result->adc_code = adc_code;
	rawfromoffset = adc_code -
			chan_properties->adc_graph[ADC_CALIB_ABSOLUTE].offset;
	if (rawfromoffset > 0) {
		if (rawfromoffset >= 1 << adc_properties->bitresolution)
			rawfromoffset = (1 << adc_properties->bitresolution)
									- 1;
		/* 2mV/K */
		adc_chan_result->measurement = (int64_t)rawfromoffset*
			chan_properties->adc_graph[ADC_CALIB_ABSOLUTE].dx *
			chan_properties->offset_gain_denominator * 1000;

		do_div(adc_chan_result->measurement,
			chan_properties->adc_graph[ADC_CALIB_ABSOLUTE].dy *
			chan_properties->offset_gain_numerator*2);
	} else {
		adc_chan_result->measurement = 0;
	}
	/* Note: adc_chan_result->measurement is in the unit of
		adc_properties.adc_reference */
	adc_chan_result->physical = (int32_t)adc_chan_result->measurement;
	/* Change to .001 deg C */
	adc_chan_result->physical -= KELVINMIL_DEGMIL;
	adc_chan_result->measurement <<= 1;

	return 0;
}
EXPORT_SYMBOL_GPL(pm8921_adc_scale_pmic_therm);

/* Scales the ADC code to 0.001 degrees C using the map
 * table for the XO thermistor.
 */
int32_t pm8921_adc_tdkntcg_therm(int32_t adc_code,
		const struct pm8921_adc_properties *adc_properties,
		const struct pm8921_adc_chan_properties *chan_properties,
		struct pm8921_adc_chan_result *adc_chan_result)
{
	uint32_t num1, num2, denom, rt_r25;
	int32_t offset = chan_properties->adc_graph->offset,
		dy = chan_properties->adc_graph->dy,
		dx = chan_properties->adc_graph->dx,
		fullscale_calibrated_adc_code;

	adc_chan_result->adc_code = adc_code;
	fullscale_calibrated_adc_code = dy + offset;
	/* The above is a short cut in math that would reduce a lot of
	   computation whereas the below expression
		(adc_properties->adc_reference*dy+dx*offset+(dx>>1))/dx
	   is a more generic formula when the 2 reference voltages are
	   different than 0 and full scale voltage. */

	if ((dy == 0) || (dx == 0) ||
			(offset >= fullscale_calibrated_adc_code)) {
		return -EINVAL;
	} else {
		if (adc_code >= fullscale_calibrated_adc_code) {
			rt_r25 = (uint32_t)-1;
		} else if (adc_code <= offset) {
			rt_r25 = 0;
		} else {
		/* The formula used is (adc_code of current reading - offset)/
		 * (the calibrated fullscale adc code - adc_code of current
		 * reading). For this channel, at this time, chan_properties->
		 * offset_gain_numerator = chan_properties->
		 * offset_gain_denominator = 1, so no need to incorporate into
		 * the formula even though it could be multiplied/divided by 1
		 * which yields the same result but
		 * expensive on computation. */
		num1 = (adc_code - offset) << 14;
		num2 = (fullscale_calibrated_adc_code - adc_code) >> 1;
		denom = fullscale_calibrated_adc_code - adc_code;

			if ((int)denom <= 0)
				rt_r25 = 0x7FFFFFFF;
			else
				rt_r25 = (num1 + num2) / denom;
		}

		if (rt_r25 > 0x7FFFFFFF)
			rt_r25 = 0x7FFFFFFF;

		pm8921_adc_map_linear(adcmap_ntcg_104ef_104fb,
		sizeof(adcmap_ntcg_104ef_104fb)/
			sizeof(adcmap_ntcg_104ef_104fb[0]),
		(int32_t)rt_r25, &adc_chan_result->physical);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(pm8921_adc_tdkntcg_therm);

int32_t pm8921_adc_scale_xtern_chgr_cur(int32_t adc_code,
		const struct pm8921_adc_properties *adc_properties,
		const struct pm8921_adc_chan_properties *chan_properties,
		struct pm8921_adc_chan_result *adc_chan_result)
{
	int32_t rawfromoffset = (adc_code - PM8921_ADC_CODE_SCALE)
						/PM8921_ADC_SLOPE;

	if (!chan_properties || !chan_properties->offset_gain_numerator ||
		!chan_properties->offset_gain_denominator || !adc_properties
		|| !adc_chan_result)
		return -EINVAL;

	adc_chan_result->adc_code = adc_code;
	if (rawfromoffset > 0) {
		if (rawfromoffset >= 1 << adc_properties->bitresolution)
			rawfromoffset = (1 << adc_properties->bitresolution)
									- 1;
		adc_chan_result->measurement = ((int64_t)rawfromoffset * 5)*
				chan_properties->offset_gain_denominator;
		do_div(adc_chan_result->measurement,
					chan_properties->offset_gain_numerator);
	} else {
		adc_chan_result->measurement = 0;
	}
	adc_chan_result->physical = (int32_t) adc_chan_result->measurement;

	return 0;
}
EXPORT_SYMBOL_GPL(pm8921_adc_scale_xtern_chgr_cur);

int32_t pm8921_adc_batt_scaler(struct pm8921_adc_arb_btm_param *btm_param)
{
	/* TODO based on the schematics for the batt thermistor
	parameters and the HW/SW doc for the device. This is the
	external batt therm */

	return 0;
}
EXPORT_SYMBOL_GPL(pm8921_adc_batt_scaler);
