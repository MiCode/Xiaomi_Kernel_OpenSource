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
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>
#define KELVINMIL_DEGMIL	273160

static const struct pm8xxx_adc_map_pt adcmap_batttherm[] = {
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

static const struct pm8xxx_adc_map_pt adcmap_btm_threshold[] = {
	{-30,	1642},
	{-20,	1544},
	{-10,	1414},
	{0,	1260},
	{1,	1244},
	{2,	1228},
	{3,	1212},
	{4,	1195},
	{5,	1179},
	{6,	1162},
	{7,	1146},
	{8,	1129},
	{9,	1113},
	{10,	1097},
	{11,	1080},
	{12,	1064},
	{13,	1048},
	{14,	1032},
	{15,	1016},
	{16,	1000},
	{17,	985},
	{18,	969},
	{19,	954},
	{20,	939},
	{21,	924},
	{22,	909},
	{23,	894},
	{24,	880},
	{25,	866},
	{26,	852},
	{27,	838},
	{28,	824},
	{29,	811},
	{30,	798},
	{31,	785},
	{32,	773},
	{33,	760},
	{34,	748},
	{35,	736},
	{36,	725},
	{37,	713},
	{38,	702},
	{39,	691},
	{40,	681},
	{41,	670},
	{42,	660},
	{43,	650},
	{44,	640},
	{45,	631},
	{46,	622},
	{47,	613},
	{48,	604},
	{49,	595},
	{50,	587},
	{51,	579},
	{52,	571},
	{53,	563},
	{54,	556},
	{55,	548},
	{56,	541},
	{57,	534},
	{58,	527},
	{59,	521},
	{60,	514},
	{61,	508},
	{62,	502},
	{63,	496},
	{64,	490},
	{65,	485},
	{66,	281},
	{67,	274},
	{68,	267},
	{69,	260},
	{70,	254},
	{71,	247},
	{72,	241},
	{73,	235},
	{74,	229},
	{75,	224},
	{76,	218},
	{77,	213},
	{78,	208},
	{79,	203}
};

static const struct pm8xxx_adc_map_pt adcmap_pa_therm[] = {
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

static const struct pm8xxx_adc_map_pt adcmap_ntcg_104ef_104fb[] = {
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

static int32_t pm8xxx_adc_map_linear(const struct pm8xxx_adc_map_pt *pts,
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

int32_t pm8xxx_adc_scale_default(int32_t adc_code,
		const struct pm8xxx_adc_properties *adc_properties,
		const struct pm8xxx_adc_chan_properties *chan_properties,
		struct pm8xxx_adc_chan_result *adc_chan_result)
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
EXPORT_SYMBOL_GPL(pm8xxx_adc_scale_default);

int32_t pm8xxx_adc_scale_batt_therm(int32_t adc_code,
		const struct pm8xxx_adc_properties *adc_properties,
		const struct pm8xxx_adc_chan_properties *chan_properties,
		struct pm8xxx_adc_chan_result *adc_chan_result)
{
	/* Note: adc_chan_result->measurement is in the unit of
		adc_properties.adc_reference */
	adc_chan_result->measurement = adc_code;
	/* convert mV ---> degC using the table */
	return pm8xxx_adc_map_linear(
			adcmap_batttherm,
			ARRAY_SIZE(adcmap_batttherm),
			adc_code,
			&adc_chan_result->physical);
}
EXPORT_SYMBOL_GPL(pm8xxx_adc_scale_batt_therm);

int32_t pm8xxx_adc_scale_pa_therm(int32_t adc_code,
		const struct pm8xxx_adc_properties *adc_properties,
		const struct pm8xxx_adc_chan_properties *chan_properties,
		struct pm8xxx_adc_chan_result *adc_chan_result)
{
	/* Note: adc_chan_result->measurement is in the unit of
		adc_properties.adc_reference */
	adc_chan_result->measurement = adc_code;
	/* convert mV ---> degC using the table */
	return pm8xxx_adc_map_linear(
			adcmap_pa_therm,
			ARRAY_SIZE(adcmap_pa_therm),
			adc_code,
			&adc_chan_result->physical);
}
EXPORT_SYMBOL_GPL(pm8xxx_adc_scale_pa_therm);

int32_t pm8xxx_adc_scale_pmic_therm(int32_t adc_code,
		const struct pm8xxx_adc_properties *adc_properties,
		const struct pm8xxx_adc_chan_properties *chan_properties,
		struct pm8xxx_adc_chan_result *adc_chan_result)
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
EXPORT_SYMBOL_GPL(pm8xxx_adc_scale_pmic_therm);

/* Scales the ADC code to 0.001 degrees C using the map
 * table for the XO thermistor.
 */
int32_t pm8xxx_adc_tdkntcg_therm(int32_t adc_code,
		const struct pm8xxx_adc_properties *adc_properties,
		const struct pm8xxx_adc_chan_properties *chan_properties,
		struct pm8xxx_adc_chan_result *adc_chan_result)
{
	int32_t rt_r25;
	int32_t offset = chan_properties->adc_graph[ADC_CALIB_ABSOLUTE].offset;

	rt_r25 = adc_code - offset;

	pm8xxx_adc_map_linear(adcmap_ntcg_104ef_104fb,
		ARRAY_SIZE(adcmap_ntcg_104ef_104fb),
		rt_r25, &adc_chan_result->physical);

	return 0;
}
EXPORT_SYMBOL_GPL(pm8xxx_adc_tdkntcg_therm);

int32_t pm8xxx_adc_batt_scaler(struct pm8xxx_adc_arb_btm_param *btm_param,
		const struct pm8xxx_adc_properties *adc_properties,
		const struct pm8xxx_adc_chan_properties *chan_properties)
{
	int rc;

	rc = pm8xxx_adc_map_linear(
		adcmap_btm_threshold,
		ARRAY_SIZE(adcmap_btm_threshold),
		btm_param->low_thr_temp,
		&btm_param->low_thr_voltage);
	if (rc)
		return rc;

	btm_param->low_thr_voltage *=
		chan_properties->adc_graph[ADC_CALIB_RATIOMETRIC].dy;
	do_div(btm_param->low_thr_voltage, adc_properties->adc_vdd_reference);
	btm_param->low_thr_voltage +=
		chan_properties->adc_graph[ADC_CALIB_RATIOMETRIC].adc_gnd;

	rc = pm8xxx_adc_map_linear(
		adcmap_btm_threshold,
		ARRAY_SIZE(adcmap_btm_threshold),
		btm_param->high_thr_temp,
		&btm_param->high_thr_voltage);
	if (rc)
		return rc;

	btm_param->high_thr_voltage *=
		chan_properties->adc_graph[ADC_CALIB_RATIOMETRIC].dy;
	do_div(btm_param->high_thr_voltage, adc_properties->adc_vdd_reference);
	btm_param->high_thr_voltage +=
		chan_properties->adc_graph[ADC_CALIB_RATIOMETRIC].adc_gnd;


	return rc;
}
EXPORT_SYMBOL_GPL(pm8xxx_adc_batt_scaler);
