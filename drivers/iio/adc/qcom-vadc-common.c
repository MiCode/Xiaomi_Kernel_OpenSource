// SPDX-License-Identifier: GPL-2.0
#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/log2.h>
#include <linux/err.h>
#include <linux/module.h>

#include "qcom-vadc-common.h"

/* Voltage to temperature */
static const struct vadc_map_pt adcmap_100k_104ef_104fb[] = {
	{1758,	-40},
	{1742,	-35},
	{1719,	-30},
	{1691,	-25},
	{1654,	-20},
	{1608,	-15},
	{1551,	-10},
	{1483,	-5},
	{1404,	0},
	{1315,	5},
	{1218,	10},
	{1114,	15},
	{1007,	20},
	{900,	25},
	{795,	30},
	{696,	35},
	{605,	40},
	{522,	45},
	{448,	50},
	{383,	55},
	{327,	60},
	{278,	65},
	{237,	70},
	{202,	75},
	{172,	80},
	{146,	85},
	{125,	90},
	{107,	95},
	{92,	100},
	{79,	105},
	{68,	110},
	{59,	115},
	{51,	120},
	{44,	125}
};

/*
 * Voltage to temperature table for 100k pull up for NTCG104EF104 with
 * 1.875V reference.
 */
static const struct vadc_map_pt adcmap_100k_104ef_104fb_1875_vref[] = {
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
 * Voltage to temperature table for 100k pull up for bat_therm with
 * Alium.
 */
static const struct vadc_map_pt adcmap_batt_therm_100k[] = {
	{1840,	-400},
	{1835,	-380},
	{1828,	-360},
	{1821,	-340},
	{1813,	-320},
	{1803,	-300},
	{1793,	-280},
	{1781,	-260},
	{1768,	-240},
	{1753,	-220},
	{1737,	-200},
	{1719,	-180},
	{1700,	-160},
	{1679,	-140},
	{1655,	-120},
	{1630,	-100},
	{1603,	-80},
	{1574,	-60},
	{1543,	-40},
	{1510,	-20},
	{1475,	0},
	{1438,	20},
	{1400,	40},
	{1360,	60},
	{1318,	80},
	{1276,	100},
	{1232,	120},
	{1187,	140},
	{1142,	160},
	{1097,	180},
	{1051,	200},
	{1005,	220},
	{960,	240},
	{915,	260},
	{871,	280},
	{828,	300},
	{786,	320},
	{745,	340},
	{705,	360},
	{666,	380},
	{629,	400},
	{594,	420},
	{560,	440},
	{527,	460},
	{497,	480},
	{467,	500},
	{439,	520},
	{413,	540},
	{388,	560},
	{365,	580},
	{343,	600},
	{322,	620},
	{302,	640},
	{284,	660},
	{267,	680},
	{251,	700},
	{235,	720},
	{221,	740},
	{208,	760},
	{195,	780},
	{184,	800},
	{173,	820},
	{163,	840},
	{153,	860},
	{144,	880},
	{136,	900},
	{128,	920},
	{120,	940},
	{114,	960},
	{107,	980}
};

/*
 * Voltage to temperature table for 30k pull up for bat_therm with
 * Alium.
 */
static const struct vadc_map_pt adcmap_batt_therm_30k[] = {
	{1864,	-400},
	{1863,	-380},
	{1861,	-360},
	{1858,	-340},
	{1856,	-320},
	{1853,	-300},
	{1850,	-280},
	{1846,	-260},
	{1842,	-240},
	{1837,	-220},
	{1831,	-200},
	{1825,	-180},
	{1819,	-160},
	{1811,	-140},
	{1803,	-120},
	{1794,	-100},
	{1784,	-80},
	{1773,	-60},
	{1761,	-40},
	{1748,	-20},
	{1734,	0},
	{1718,	20},
	{1702,	40},
	{1684,	60},
	{1664,	80},
	{1643,	100},
	{1621,	120},
	{1597,	140},
	{1572,	160},
	{1546,	180},
	{1518,	200},
	{1489,	220},
	{1458,	240},
	{1426,	260},
	{1393,	280},
	{1359,	300},
	{1324,	320},
	{1288,	340},
	{1252,	360},
	{1214,	380},
	{1176,	400},
	{1138,	420},
	{1100,	440},
	{1061,	460},
	{1023,	480},
	{985,	500},
	{947,	520},
	{910,	540},
	{873,	560},
	{836,	580},
	{801,	600},
	{766,	620},
	{732,	640},
	{699,	660},
	{668,	680},
	{637,	700},
	{607,	720},
	{578,	740},
	{550,	760},
	{524,	780},
	{498,	800},
	{474,	820},
	{451,	840},
	{428,	860},
	{407,	880},
	{387,	900},
	{367,	920},
	{349,	940},
	{332,	960},
	{315,	980}
};

/*
 * Voltage to temperature table for 400k pull up for bat_therm with
 * Alium.
 */
static const struct vadc_map_pt adcmap_batt_therm_400k[] = {
	{1744,	-400},
	{1724,	-380},
	{1701,	-360},
	{1676,	-340},
	{1648,	-320},
	{1618,	-300},
	{1584,	-280},
	{1548,	-260},
	{1509,	-240},
	{1468,	-220},
	{1423,	-200},
	{1377,	-180},
	{1328,	-160},
	{1277,	-140},
	{1225,	-120},
	{1171,	-100},
	{1117,	-80},
	{1062,	-60},
	{1007,	-40},
	{953,	-20},
	{899,	0},
	{847,	20},
	{795,	40},
	{745,	60},
	{697,	80},
	{651,	100},
	{607,	120},
	{565,	140},
	{526,	160},
	{488,	180},
	{453,	200},
	{420,	220},
	{390,	240},
	{361,	260},
	{334,	280},
	{309,	300},
	{286,	320},
	{265,	340},
	{245,	360},
	{227,	380},
	{210,	400},
	{195,	420},
	{180,	440},
	{167,	460},
	{155,	480},
	{144,	500},
	{133,	520},
	{124,	540},
	{115,	560},
	{107,	580},
	{99,	600},
	{92,	620},
	{86,	640},
	{80,	660},
	{75,	680},
	{70,	700},
	{65,	720},
	{61,	740},
	{57,	760},
	{53,	780},
	{50,	800},
	{46,	820},
	{43,	840},
	{41,	860},
	{38,	880},
	{36,	900},
	{34,	920},
	{32,	940},
	{30,	960},
	{28,	980}
};

static int qcom_vadc_map_voltage_temp(const struct vadc_map_pt *pts,
				      u32 tablesize, s32 input, s64 *output)
{
	bool descending = 1;
	u32 i = 0;

	if (!pts)
		return -EINVAL;

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
		*output = (((s32)((pts[i].y - pts[i - 1].y) *
			(input - pts[i - 1].x)) /
			(pts[i].x - pts[i - 1].x)) +
			pts[i - 1].y);
	}

	return 0;
}

static void qcom_vadc_scale_calib(const struct vadc_linear_graph *calib_graph,
				  u16 adc_code,
				  bool absolute,
				  s64 *scale_voltage)
{
	*scale_voltage = (adc_code - calib_graph->gnd);
	*scale_voltage *= calib_graph->dx;
	*scale_voltage = div64_s64(*scale_voltage, calib_graph->dy);
	if (absolute)
		*scale_voltage += calib_graph->dx;

	if (*scale_voltage < 0)
		*scale_voltage = 0;
}

static int qcom_vadc_scale_volt(const struct vadc_linear_graph *calib_graph,
				const struct vadc_prescale_ratio *prescale,
				bool absolute, u16 adc_code,
				int *result_uv)
{
	s64 voltage = 0, result = 0;

	qcom_vadc_scale_calib(calib_graph, adc_code, absolute, &voltage);

	voltage = voltage * prescale->den;
	result = div64_s64(voltage, prescale->num);
	*result_uv = result;

	return 0;
}

static int qcom_vadc_scale_therm(const struct vadc_linear_graph *calib_graph,
				 const struct vadc_prescale_ratio *prescale,
				 bool absolute, u16 adc_code,
				 int *result_mdec)
{
	s64 voltage = 0, result = 0;
	int ret;

	qcom_vadc_scale_calib(calib_graph, adc_code, absolute, &voltage);

	if (absolute)
		voltage = div64_s64(voltage, 1000);

	ret = qcom_vadc_map_voltage_temp(adcmap_100k_104ef_104fb,
					 ARRAY_SIZE(adcmap_100k_104ef_104fb),
					 voltage, &result);
	if (ret)
		return ret;

	result *= 1000;
	*result_mdec = result;

	return 0;
}

static int qcom_vadc_scale_die_temp(const struct vadc_linear_graph *calib_graph,
				    const struct vadc_prescale_ratio *prescale,
				    bool absolute,
				    u16 adc_code, int *result_mdec)
{
	s64 voltage = 0;
	u64 temp; /* Temporary variable for do_div */

	qcom_vadc_scale_calib(calib_graph, adc_code, absolute, &voltage);

	if (voltage > 0) {
		temp = voltage * prescale->den;
		do_div(temp, prescale->num * 2);
		voltage = temp;
	} else {
		voltage = 0;
	}

	voltage -= KELVINMIL_CELSIUSMIL;
	*result_mdec = voltage;

	return 0;
}

static int qcom_vadc_scale_chg_temp(const struct vadc_linear_graph *calib_graph,
				    const struct vadc_prescale_ratio *prescale,
				    bool absolute,
				    u16 adc_code, int *result_mdec)
{
	s64 voltage = 0, result = 0;

	qcom_vadc_scale_calib(calib_graph, adc_code, absolute, &voltage);

	voltage = voltage * prescale->den;
	voltage = div64_s64(voltage, prescale->num);
	voltage = ((PMI_CHG_SCALE_1) * (voltage * 2));
	voltage = (voltage + PMI_CHG_SCALE_2);
	result =  div64_s64(voltage, 1000000);
	*result_mdec = result;

	return 0;
}

static int qcom_vadc_scale_hw_calib_volt(
				const struct vadc_prescale_ratio *prescale,
				const struct adc_data *data,
				u16 adc_code, int *result_uv)
{
	s64 voltage = 0, result = 0, adc_vdd_ref_mv = 1875;

	if (adc_code > VADC5_MAX_CODE)
		adc_code = 0;

	/* (ADC code * vref_vadc (1.875V)) / full_scale_code */
	voltage = (s64) adc_code * adc_vdd_ref_mv * 1000;
	voltage = div64_s64(voltage, data->full_scale_code_volt);
	voltage = voltage * prescale->den;
	result = div64_s64(voltage, prescale->num);
	*result_uv = result;

	return 0;
}

static int qcom_vadc_scale_hw_calib_therm(
				const struct vadc_prescale_ratio *prescale,
				const struct adc_data *data,
				u16 adc_code, int *result_mdec)
{
	s64 voltage = 0, result = 0;
	int ret;

	if (adc_code > VADC5_MAX_CODE)
		adc_code = 0;

	/* (ADC code * vref_vadc (1.875V)) / full_scale_code */
	voltage = (s64) adc_code * ADC_HC_VDD_REF * 1000;
	voltage = div64_s64(voltage, (data->full_scale_code_volt
								* 1000));
	ret = qcom_vadc_map_voltage_temp(adcmap_100k_104ef_104fb_1875_vref,
				 ARRAY_SIZE(adcmap_100k_104ef_104fb_1875_vref),
				 voltage, &result);
	if (ret)
		return ret;

	*result_mdec = result;

	return 0;
}

static int qcom_vadc_scale_hw_calib_batt_therm_100(
				const struct vadc_prescale_ratio *prescale,
				const struct adc_data *data,
				u16 adc_code, int *result_mdec)
{
	s64 voltage = 0, result = 0, adc_vdd_ref_mv = 1875;
	int ret;

	if (adc_code > VADC5_MAX_CODE)
		adc_code = 0;

	/* (ADC code * vref_vadc (1.875V)) / full_scale_code */
	voltage = (s64) adc_code * adc_vdd_ref_mv * 1000;
	voltage = div64_s64(voltage, (data->full_scale_code_volt
								* 1000));
	ret = qcom_vadc_map_voltage_temp(adcmap_batt_therm_100k,
				 ARRAY_SIZE(adcmap_batt_therm_100k),
				 voltage, &result);
	if (ret)
		return ret;

	*result_mdec = result;

	return 0;
}

static int qcom_vadc_scale_hw_calib_batt_therm_30(
				const struct vadc_prescale_ratio *prescale,
				const struct adc_data *data,
				u16 adc_code, int *result_mdec)
{
	s64 voltage = 0, result = 0, adc_vdd_ref_mv = 1875;
	int ret;

	if (adc_code > VADC5_MAX_CODE)
		adc_code = 0;

	/* (ADC code * vref_vadc (1.875V)) / full_scale_code */
	voltage = (s64) adc_code * adc_vdd_ref_mv * 1000;
	voltage = div64_s64(voltage, (data->full_scale_code_volt
								* 1000));
	ret = qcom_vadc_map_voltage_temp(adcmap_batt_therm_30k,
				 ARRAY_SIZE(adcmap_batt_therm_30k),
				 voltage, &result);
	if (ret)
		return ret;

	*result_mdec = result;

	return 0;
}

static int qcom_vadc_scale_hw_calib_batt_therm_400(
				const struct vadc_prescale_ratio *prescale,
				const struct adc_data *data,
				u16 adc_code, int *result_mdec)
{
	s64 voltage = 0, result = 0, adc_vdd_ref_mv = 1875;
	int ret;

	if (adc_code > VADC5_MAX_CODE)
		adc_code = 0;

	/* (ADC code * vref_vadc (1.875V)) / full_scale_code */
	voltage = (s64) adc_code * adc_vdd_ref_mv * 1000;
	voltage = div64_s64(voltage, (data->full_scale_code_volt
								* 1000));
	ret = qcom_vadc_map_voltage_temp(adcmap_batt_therm_400k,
				 ARRAY_SIZE(adcmap_batt_therm_400k),
				 voltage, &result);
	if (ret)
		return ret;

	*result_mdec = result;

	return 0;
}

static int qcom_vadc_scale_hw_calib_die_temp(
				const struct vadc_prescale_ratio *prescale,
				const struct adc_data *data,
				u16 adc_code, int *result_mdec)
{
	s64 voltage = 0, adc_vdd_ref_mv = 1875;
	u64 temp; /* Temporary variable for do_div */

	if (adc_code > VADC5_MAX_CODE)
		adc_code = 0;

	/* (ADC code * vref_vadc (1.875V)) / full_scale_code */
	voltage = (s64) adc_code * adc_vdd_ref_mv * 1000;
	voltage = div64_s64(voltage, data->full_scale_code_volt);
	if (voltage > 0) {
		temp = voltage * prescale->den;
		do_div(temp, prescale->num * 2);
		voltage = temp;
	} else {
		voltage = 0;
	}

	voltage -= KELVINMIL_CELSIUSMIL;
	*result_mdec = voltage;

	return 0;
}

static int qcom_vadc_scale_hw_smb_temp(
				const struct vadc_prescale_ratio *prescale,
				const struct adc_data *data,
				u16 adc_code, int *result_mdec)
{
	s64 voltage = 0, adc_vdd_ref_mv = 1875;
	u64 temp;

	if (adc_code > VADC5_MAX_CODE)
		adc_code = 0;

	/* (ADC code * vref_vadc (1.875V)) / full_scale_code */
	voltage = (s64) adc_code * adc_vdd_ref_mv * 1000;
	voltage = div64_s64(voltage, data->full_scale_code_volt);
	if (voltage > 0) {
		temp = voltage * prescale->den;
		temp *= 100;
		do_div(temp, prescale->num * PMIC5_SMB_TEMP_SCALE_FACTOR);
		voltage = temp;
	} else {
		voltage = 0;
	}

	voltage = PMIC5_SMB_TEMP_CONSTANT - voltage;
	*result_mdec = voltage;

	return 0;
}

static int qcom_vadc_scale_hw_chg5_temp(
				const struct vadc_prescale_ratio *prescale,
				const struct adc_data *data,
				u16 adc_code, int *result_mdec)
{
	s64 voltage = 0, adc_vdd_ref_mv = 1875;
	u64 temp;

	if (adc_code > VADC5_MAX_CODE)
		adc_code = 0;

	/* (ADC code * vref_vadc (1.875V)) / full_scale_code */
	voltage = (s64) adc_code * adc_vdd_ref_mv * 1000;
	voltage = div64_s64(voltage, data->full_scale_code_volt);
	if (voltage > 0) {
		temp = voltage * prescale->den;
		do_div(temp, prescale->num * 4);
		voltage = temp;
	} else {
		voltage = 0;
	}

	voltage = PMIC5_CHG_TEMP_SCALE_FACTOR - voltage;
	*result_mdec = voltage;

	return 0;
}

static int qcom_adc_scale_hw_calib_cur(
				const struct vadc_prescale_ratio *prescale,
				const struct adc_data *data,
				u16 adc_code, int *result_uamps)
{
	s64 voltage = 0, result = 0;

	if ((adc_code & ADC_USR_DATA_CHECK) == 0) {
		voltage = (s64) adc_code * data->full_scale_code_cur * 1000;
		voltage = div64_s64(voltage, VADC5_MAX_CODE);
		voltage = voltage * prescale->den;
		result = div64_s64(voltage, prescale->num);
		*result_uamps = result;
	} else {
		adc_code = ~adc_code + 1;
		voltage = (s64) adc_code;
		voltage = (s64) adc_code * data->full_scale_code_cur * 1000;
		voltage = div64_s64(voltage, VADC5_MAX_CODE);
		voltage = voltage * prescale->den;
		result = div64_s64(voltage, prescale->num);
		*result_uamps = -result;
	}

	return 0;
}

int qcom_vadc_scale(enum vadc_scale_fn_type scaletype,
		    const struct vadc_linear_graph *calib_graph,
		    const struct vadc_prescale_ratio *prescale,
		    bool absolute,
		    u16 adc_code, int *result)
{
	switch (scaletype) {
	case SCALE_DEFAULT:
		return qcom_vadc_scale_volt(calib_graph, prescale,
					    absolute, adc_code,
					    result);
	case SCALE_THERM_100K_PULLUP:
	case SCALE_XOTHERM:
		return qcom_vadc_scale_therm(calib_graph, prescale,
					     absolute, adc_code,
					     result);
	case SCALE_PMIC_THERM:
		return qcom_vadc_scale_die_temp(calib_graph, prescale,
						absolute, adc_code,
						result);
	case SCALE_PMI_CHG_TEMP:
		return qcom_vadc_scale_chg_temp(calib_graph, prescale,
						absolute, adc_code,
						result);
	default:
		return -EINVAL;
	}
}
EXPORT_SYMBOL(qcom_vadc_scale);

int qcom_vadc_hw_scale(enum vadc_scale_fn_type scaletype,
		    const struct vadc_prescale_ratio *prescale,
		    const struct adc_data *data,
		    u16 adc_code, int *result)
{
	switch (scaletype) {
	case SCALE_HW_CALIB_DEFAULT:
		return qcom_vadc_scale_hw_calib_volt(prescale, data,
						adc_code, result);
	case SCALE_HW_CALIB_THERM_100K_PULLUP:
	case SCALE_HW_CALIB_XOTHERM:
		return qcom_vadc_scale_hw_calib_therm(prescale, data,
						adc_code, result);
	case SCALE_HW_CALIB_BATT_THERM_100K:
		return qcom_vadc_scale_hw_calib_batt_therm_100(prescale,
						data, adc_code, result);
	case SCALE_HW_CALIB_BATT_THERM_30K:
		return qcom_vadc_scale_hw_calib_batt_therm_30(prescale,
						data, adc_code, result);
	case SCALE_HW_CALIB_BATT_THERM_400K:
		return qcom_vadc_scale_hw_calib_batt_therm_400(prescale,
						data, adc_code, result);
	case SCALE_HW_CALIB_PMIC_THERM:
		return qcom_vadc_scale_hw_calib_die_temp(prescale, data,
						adc_code, result);
	case SCALE_HW_CALIB_CUR:
		return qcom_adc_scale_hw_calib_cur(prescale, data,
						adc_code, result);
	case SCALE_HW_CALIB_PM5_CHG_TEMP:
		return qcom_vadc_scale_hw_chg5_temp(prescale, data,
						adc_code, result);
	case SCALE_HW_CALIB_PM5_SMB_TEMP:
		return qcom_vadc_scale_hw_smb_temp(prescale, data,
						adc_code, result);
	default:
		return -EINVAL;
	}
}
EXPORT_SYMBOL(qcom_vadc_hw_scale);

int qcom_vadc_decimation_from_dt(u32 value)
{
	if (!is_power_of_2(value) || value < VADC_DECIMATION_MIN ||
	    value > VADC_DECIMATION_MAX)
		return -EINVAL;

	return __ffs64(value / VADC_DECIMATION_MIN);
}
EXPORT_SYMBOL(qcom_vadc_decimation_from_dt);

int qcom_adc5_decimation_from_dt(u32 value, const unsigned int *decimation)
{
	uint32_t i;

	for (i = 0; i < ADC_DECIMATION_SAMPLES_MAX; i++) {
		if (value == decimation[i])
			return i;
	}

	return -EINVAL;
}
EXPORT_SYMBOL(qcom_adc5_decimation_from_dt);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm ADC common functionality");
