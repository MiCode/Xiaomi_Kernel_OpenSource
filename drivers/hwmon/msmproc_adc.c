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
#include <linux/mfd/pm8xxx/pm8921-adc.h>
#define KELVINMIL_DEGMIL	273160

static const struct pm8921_adc_map_pt adcmap_batttherm[] = {
	{41001,	-30},
	{40017,	-20},
	{38721,	-10},
	{37186,	  0},
	{35554,	 10},
	{33980,	 20},
	{33253,	 25},
	{32580,	 30},
	{31412,	 40},
	{30481,	 50},
	{29759,	 60},
	{29209,	 70},
	{28794,	 80}
};

static const struct pm8921_adc_map_pt adcmap_btm_threshold[] = {
	{-30,	41001},
	{-20,	40017},
	{-10,	38721},
	{0,	37186},
	{10,	35554},
	{11,	35392},
	{12,	35230},
	{13,	35070},
	{14,	34910},
	{15,	34751},
	{16,	34594},
	{17,	34438},
	{18,	34284},
	{19,	34131},
	{20,	33980},
	{21,	33830},
	{22,	33683},
	{23,	33538},
	{24,	33394},
	{25,	33253},
	{26,	33114},
	{27,	32977},
	{28,	32842},
	{29,	32710},
	{30,	32580},
	{31,	32452},
	{32,	32327},
	{33,	32204},
	{34,	32084},
	{35,	31966},
	{36,	31850},
	{37,	31737},
	{38,	31627},
	{39,	31518},
	{40,	31412},
	{41,	31309},
	{42,	31208},
	{43,	31109},
	{44,	31013},
	{45,	30918},
	{46,	30827},
	{47,	30737},
	{48,	30649},
	{49,	30564},
	{50,	30481},
	{51,	30400},
	{52,	30321},
	{53,	30244},
	{54,	30169},
	{55,	30096},
	{56,	30025},
	{57,	29956},
	{58,	29889},
	{59,	29823},
	{60,	29759},
	{61,	29697},
	{62,	29637},
	{63,	29578},
	{64,	29521},
	{65,	29465},
	{66,	29411},
	{67,	29359},
	{68,	29308},
	{69,	29258},
	{70,	29209},
	{71,	29162},
	{72,	29117},
	{73,	29072},
	{74,	29029},
	{75,	28987},
	{76,	28946},
	{77,	28906},
	{78,	28868},
	{79,	28830},
	{80,	28794}
};

static const struct pm8921_adc_map_pt adcmap_pa_therm[] = {
	{41350,	-30},
	{41282,	-29},
	{41211,	-28},
	{41137,	-27},
	{41060,	-26},
	{40980,	-25},
	{40897,	-24},
	{40811,	-23},
	{40721,	-22},
	{40629,	-21},
	{40533,	-20},
	{40434,	-19},
	{40331,	-18},
	{40226,	-17},
	{40116,	-16},
	{40004,	-15},
	{39888,	-14},
	{39769,	-13},
	{39647,	-12},
	{39521,	-11},
	{39392,	-10},
	{39260,	-9},
	{39124,	-8},
	{38986,	-7},
	{38845,	-6},
	{38700,	-5},
	{38553,	-4},
	{38403,	-3},
	{38250,	-2},
	{38094,	-1},
	{37936,	0},
	{37776,	1},
	{37613,	2},
	{37448,	3},
	{37281,	4},
	{37112,	5},
	{36942,	6},
	{36770,	7},
	{36596,	8},
	{36421,	9},
	{36245,	10},
	{36068,	11},
	{35890,	12},
	{35712,	13},
	{35532,	14},
	{35353,	15},
	{35173,	16},
	{34993,	17},
	{34813,	18},
	{34634,	19},
	{34455,	20},
	{34276,	21},
	{34098,	22},
	{33921,	23},
	{33745,	24},
	{33569,	25},
	{33395,	26},
	{33223,	27},
	{33051,	28},
	{32881,	29},
	{32713,	30},
	{32547,	31},
	{32382,	32},
	{32219,	33},
	{32058,	34},
	{31899,	35},
	{31743,	36},
	{31588,	37},
	{31436,	38},
	{31285,	39},
	{31138,	40},
	{30992,	41},
	{30849,	42},
	{30708,	43},
	{30570,	44},
	{30434,	45},
	{30300,	46},
	{30169,	47},
	{30041,	48},
	{29915,	49},
	{29791,	50},
	{29670,	51},
	{29551,	52},
	{29435,	53},
	{29321,	54},
	{29210,	55},
	{29101,	56},
	{28994,	57},
	{28890,	58},
	{28788,	59},
	{28688,	60},
	{28590,	61},
	{28495,	62},
	{28402,	63},
	{28311,	64},
	{28222,	65},
	{28136,	66},
	{28051,	67},
	{27968,	68},
	{27888,	69},
	{27809,	70},
	{27732,	71},
	{27658,	72},
	{27584,	73},
	{27513,	74},
	{27444,	75},
	{27376,	76},
	{27310,	77},
	{27245,	78},
	{27183,	79},
	{27121,	80},
	{27062,	81},
	{27004,	82},
	{26947,	83},
	{26892,	84},
	{26838,	85},
	{26785,	86},
	{26734,	87},
	{26684,	88},
	{26636,	89},
	{26588,	90}
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
	/* convert mV ---> degC using the table */
	return pm8921_adc_map_linear(
			adcmap_batttherm,
			ARRAY_SIZE(adcmap_batttherm),
			adc_code,
			&adc_chan_result->physical);
}
EXPORT_SYMBOL_GPL(pm8921_adc_scale_batt_therm);

int32_t pm8921_adc_scale_pa_therm(int32_t adc_code,
		const struct pm8921_adc_properties *adc_properties,
		const struct pm8921_adc_chan_properties *chan_properties,
		struct pm8921_adc_chan_result *adc_chan_result)
{
	/* convert mV ---> degC using the table */
	return pm8921_adc_map_linear(
			adcmap_pa_therm,
			ARRAY_SIZE(adcmap_pa_therm),
			adc_code,
			&adc_chan_result->physical);
}
EXPORT_SYMBOL_GPL(pm8921_adc_scale_pa_therm);

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
	int32_t rt_r25;
	int32_t offset = chan_properties->adc_graph[ADC_CALIB_ABSOLUTE].offset;

	rt_r25 = adc_code - offset;

	pm8921_adc_map_linear(adcmap_ntcg_104ef_104fb,
		ARRAY_SIZE(adcmap_ntcg_104ef_104fb),
		rt_r25, &adc_chan_result->physical);

	return 0;
}
EXPORT_SYMBOL_GPL(pm8921_adc_tdkntcg_therm);

int32_t pm8921_adc_batt_scaler(struct pm8921_adc_arb_btm_param *btm_param)
{
	int rc;

	rc = pm8921_adc_map_linear(
		adcmap_btm_threshold,
		ARRAY_SIZE(adcmap_btm_threshold),
		btm_param->low_thr_temp,
		&btm_param->low_thr_voltage);

	if (!rc) {
		rc = pm8921_adc_map_linear(
			adcmap_btm_threshold,
			ARRAY_SIZE(adcmap_btm_threshold),
			btm_param->high_thr_temp,
			&btm_param->high_thr_voltage);
	}

	return rc;
}
EXPORT_SYMBOL_GPL(pm8921_adc_batt_scaler);
