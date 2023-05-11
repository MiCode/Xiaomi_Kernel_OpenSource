// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/iio/iio.h>
#include <linux/iio/consumer.h>

#define KELVINMIL_DEGMIL	273160
/*
 * Units for temperature below (on x axis) is in 0.1DegC as
 * required by the battery driver. Note the resolution used
 * here to compute the table was done for DegC to milli-volts.
 * In consideration to limit the size of the table for the given
 * temperature range below, the result is linearly interpolated
 * and provided to the battery driver in the units desired for
 * their framework which is 0.1DegC. True resolution of 0.1DegC
 * will result in the below table size to increase by 10 times.
 */
static const struct qpnp_vadc_map_pt adcmap_btm_threshold[] = {
	{-300,	1642},
	{-200,	1544},
	{-100,	1414},
	{0,	1260},
	{10,	1244},
	{20,	1228},
	{30,	1212},
	{40,	1195},
	{50,	1179},
	{60,	1162},
	{70,	1146},
	{80,	1129},
	{90,	1113},
	{100,	1097},
	{110,	1080},
	{120,	1064},
	{130,	1048},
	{140,	1032},
	{150,	1016},
	{160,	1000},
	{170,	985},
	{180,	969},
	{190,	954},
	{200,	939},
	{210,	924},
	{220,	909},
	{230,	894},
	{240,	880},
	{250,	866},
	{260,	852},
	{270,	838},
	{280,	824},
	{290,	811},
	{300,	798},
	{310,	785},
	{320,	773},
	{330,	760},
	{340,	748},
	{350,	736},
	{360,	725},
	{370,	713},
	{380,	702},
	{390,	691},
	{400,	681},
	{410,	670},
	{420,	660},
	{430,	650},
	{440,	640},
	{450,	631},
	{460,	622},
	{470,	613},
	{480,	604},
	{490,	595},
	{500,	587},
	{510,	579},
	{520,	571},
	{530,	563},
	{540,	556},
	{550,	548},
	{560,	541},
	{570,	534},
	{580,	527},
	{590,	521},
	{600,	514},
	{610,	508},
	{620,	502},
	{630,	496},
	{640,	490},
	{650,	485},
	{660,	281},
	{670,	274},
	{680,	267},
	{690,	260},
	{700,	254},
	{710,	247},
	{720,	241},
	{730,	235},
	{740,	229},
	{750,	224},
	{760,	218},
	{770,	213},
	{780,	208},
	{790,	203}
};

/* Voltage to temperature */
static const struct qpnp_vadc_map_pt adcmap_100k_104ef_104fb[] = {
	{1758,	-40000},
	{1742,	-35000},
	{1719,	-30000},
	{1691,	-25000},
	{1654,	-20000},
	{1608,	-15000},
	{1551,	-10000},
	{1483,	-5000},
	{1404,	0},
	{1315,	5000},
	{1218,	10000},
	{1114,	15000},
	{1007,	20000},
	{900,	25000},
	{795,	30000},
	{696,	35000},
	{605,	40000},
	{522,	45000},
	{448,	50000},
	{383,	55000},
	{327,	60000},
	{278,	65000},
	{237,	70000},
	{202,	75000},
	{172,	80000},
	{146,	85000},
	{125,	90000},
	{107,	95000},
	{92,	100000},
	{79,	105000},
	{68,	110000},
	{59,	115000},
	{51,	120000},
	{44,	125000}
};

static const struct qpnp_vadc_map_pt adcmap_smb_batt_therm[] = {
	{-300,	1625},
	{-200,	1515},
	{-100,	1368},
	{0,	1192},
	{10,	1173},
	{20,	1154},
	{30,	1135},
	{40,	1116},
	{50,	1097},
	{60,	1078},
	{70,	1059},
	{80,	1040},
	{90,	1020},
	{100,	1001},
	{110,	982},
	{120,	963},
	{130,	944},
	{140,	925},
	{150,	907},
	{160,	888},
	{170,	870},
	{180,	851},
	{190,	833},
	{200,	815},
	{210,	797},
	{220,	780},
	{230,	762},
	{240,	745},
	{250,	728},
	{260,	711},
	{270,	695},
	{280,	679},
	{290,	663},
	{300,	647},
	{310,	632},
	{320,	616},
	{330,	602},
	{340,	587},
	{350,	573},
	{360,	559},
	{370,	545},
	{380,	531},
	{390,	518},
	{400,	505},
	{410,	492},
	{420,	480},
	{430,	465},
	{440,	456},
	{450,	445},
	{460,	433},
	{470,	422},
	{480,	412},
	{490,	401},
	{500,	391},
	{510,	381},
	{520,	371},
	{530,	362},
	{540,	352},
	{550,	343},
	{560,	335},
	{570,	326},
	{580,	318},
	{590,	309},
	{600,	302},
	{610,	294},
	{620,	286},
	{630,	279},
	{640,	272},
	{650,	265},
	{660,	258},
	{670,	252},
	{680,	245},
	{690,	239},
	{700,	233},
	{710,	227},
	{720,	221},
	{730,	216},
	{740,	211},
	{750,	205},
	{760,	200},
	{770,	195},
	{780,	190},
	{790,	186}
};

/* Voltage to temperature */
static const struct qpnp_vadc_map_pt adcmap_batt_therm_qrd_215[] = {
	{1575,	-200},
	{1549,	-180},
	{1522,	-160},
	{1493,	-140},
	{1463,	-120},
	{1431,	-100},
	{1398,	-80},
	{1364,	-60},
	{1329,	-40},
	{1294,	-20},
	{1258,	0},
	{1222,	20},
	{1187,	40},
	{1151,	60},
	{1116,	80},
	{1082,	100},
	{1049,	120},
	{1016,	140},
	{985,	160},
	{955,	180},
	{926,	200},
	{899,	220},
	{873,	240},
	{849,	260},
	{825,	280},
	{804,	300},
	{783,	320},
	{764,	340},
	{746,	360},
	{729,	380},
	{714,	400},
	{699,	420},
	{686,	440},
	{673,	460},
	{662,	480},
	{651,	500},
	{641,	520},
	{632,	540},
	{623,	560},
	{615,	580},
	{608,	600},
	{601,	620},
	{595,	640},
	{589,	660},
	{583,	680},
	{578,	700},
	{574,	720},
	{569,	740},
	{565,	760},
	{562,	780},
	{558,	800}
};

static int32_t qpnp_get_vadc_gain_and_offset(struct qpnp_adc_drv *adc,
				struct qpnp_vadc_linear_graph *param,
				enum qpnp_adc_calib_type calib_type)
{
	int rc = 0;

	switch (calib_type) {
	case CALIB_RATIOMETRIC:
		param->dy =
		adc->amux_prop->chan_prop->adc_graph[CALIB_RATIOMETRIC].dy;
		param->dx =
		adc->amux_prop->chan_prop->adc_graph[CALIB_RATIOMETRIC].dx;
		param->adc_vref = adc->adc_prop->adc_vdd_reference;
		param->adc_gnd =
		adc->amux_prop->chan_prop->adc_graph[CALIB_RATIOMETRIC].adc_gnd;
		break;
	case CALIB_ABSOLUTE:
		param->dy =
		adc->amux_prop->chan_prop->adc_graph[CALIB_ABSOLUTE].dy;
		param->dx =
		adc->amux_prop->chan_prop->adc_graph[CALIB_ABSOLUTE].dx;
		param->adc_vref = adc->adc_prop->adc_vdd_reference;
		param->adc_gnd =
		adc->amux_prop->chan_prop->adc_graph[CALIB_ABSOLUTE].adc_gnd;
		break;
	default:
		rc = -EINVAL;
	}
	return rc;
}

static int32_t qpnp_adc_map_voltage_temp(const struct qpnp_vadc_map_pt *pts,
		uint32_t tablesize, int32_t input, int64_t *output)
{
	unsigned int descending = 1;
	uint32_t i = 0;

	if (pts == NULL)
		return -EINVAL;

	/* Check if table is descending or ascending */
	if (tablesize > 1) {
		if (pts[0].x < pts[1].x)
			descending = 0;
	}

	while (i < tablesize) {
		if ((descending == 1) && (pts[i].x < input)) {
			/*
			 * table entry is less than measured
			 * value and table is descending, stop.
			 */
			break;
		} else if ((descending == 0) &&
				(pts[i].x > input)) {
			/*
			 * table entry is greater than measured
			 * value and table is ascending, stop.
			 */
			break;
		}
		i++;
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

static int32_t qpnp_adc_map_temp_voltage(const struct qpnp_vadc_map_pt *pts,
		uint32_t tablesize, int32_t input, int64_t *output)
{
	unsigned int descending = 1;
	uint32_t i = 0;

	if (pts == NULL)
		return -EINVAL;

	/* Check if table is descending or ascending */
	if (tablesize > 1) {
		if (pts[0].y < pts[1].y)
			descending = 0;
	}

	while (i < tablesize) {
		if ((descending == 1) && (pts[i].y < input)) {
			/* Table entry is less than measured value. */
			/* Table is descending, stop. */
			break;
		} else if ((descending == 0) && (pts[i].y > input)) {
			/* Table entry is greater than measured value. */
			/* Table is ascending, stop. */
			break;
		}
		i++;
	}

	if (i == 0) {
		*output = pts[0].x;
	} else if (i == tablesize) {
		*output = pts[tablesize-1].x;
	} else {
		/* result is between search_index and search_index-1 */
		/* interpolate linearly */
		*output = (((int32_t) ((pts[i].x - pts[i-1].x)*
			(input - pts[i-1].y))/
			(pts[i].y - pts[i-1].y))+
			pts[i-1].x);
	}

	return 0;
}

int32_t qpnp_adc_tm_scale_therm_voltage_pu2(struct qpnp_adc_drv *adc,
			const struct qpnp_adc_properties *adc_properties,
				struct qpnp_adc_tm_config *param)
{
	struct qpnp_vadc_linear_graph param1;
	int rc = 0;

	rc = qpnp_get_vadc_gain_and_offset(adc, &param1, CALIB_RATIOMETRIC);
	if (rc < 0)
		return rc;

	rc = qpnp_adc_map_temp_voltage(adcmap_100k_104ef_104fb,
		ARRAY_SIZE(adcmap_100k_104ef_104fb),
		param->low_thr_temp, &param->low_thr_voltage);
	if (rc)
		return rc;

	param->low_thr_voltage *= param1.dy;
	param->low_thr_voltage = div64_s64(param->low_thr_voltage,
						param1.adc_vref);
	param->low_thr_voltage += param1.adc_gnd;

	rc = qpnp_adc_map_temp_voltage(adcmap_100k_104ef_104fb,
		ARRAY_SIZE(adcmap_100k_104ef_104fb),
		param->high_thr_temp, &param->high_thr_voltage);
	if (rc)
		return rc;

	param->high_thr_voltage *= param1.dy;
	param->high_thr_voltage = div64_s64(param->high_thr_voltage,
						param1.adc_vref);
	param->high_thr_voltage += param1.adc_gnd;

	return 0;
}
EXPORT_SYMBOL(qpnp_adc_tm_scale_therm_voltage_pu2);

int32_t qpnp_adc_usb_scaler(struct qpnp_adc_drv *adc,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold)
{
	struct qpnp_vadc_linear_graph usb_param;
	int rc = 0;

	rc = qpnp_get_vadc_gain_and_offset(adc, &usb_param, CALIB_RATIOMETRIC);
	if (rc < 0)
		return rc;

	*low_threshold = param->low_thr * usb_param.dy;
	*low_threshold = div64_s64(*low_threshold, usb_param.adc_vref);
	*low_threshold += usb_param.adc_gnd;

	*high_threshold = param->high_thr * usb_param.dy;
	*high_threshold = div64_s64(*high_threshold, usb_param.adc_vref);
	*high_threshold += usb_param.adc_gnd;

	pr_debug("high_volt:%d, low_volt:%d\n", param->high_thr,
				param->low_thr);
	return 0;
}
EXPORT_SYMBOL(qpnp_adc_usb_scaler);

int32_t qpnp_adc_absolute_rthr(struct qpnp_adc_drv *adc,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold)
{
	struct qpnp_vadc_linear_graph vbatt_param;
	int rc = 0, sign = 0;
	int64_t low_thr = 0, high_thr = 0;

	rc = qpnp_get_vadc_gain_and_offset(adc, &vbatt_param,
						CALIB_ABSOLUTE);
	if (rc < 0)
		return rc;

	low_thr = (((param->low_thr/param->gain_den) -
			QPNP_ADC_625_UV) * vbatt_param.dy);
	if (low_thr < 0) {
		sign = 1;
		low_thr = -low_thr;
	}
	low_thr = low_thr * param->gain_num;
	low_thr = div64_s64(low_thr, QPNP_ADC_625_UV);
	if (sign)
		low_thr = -low_thr;
	*low_threshold = low_thr + vbatt_param.adc_gnd;

	sign = 0;
	high_thr = (((param->high_thr/param->gain_den) -
			QPNP_ADC_625_UV) * vbatt_param.dy);
	if (high_thr < 0) {
		sign = 1;
		high_thr = -high_thr;
	}
	high_thr = high_thr * param->gain_num;
	high_thr = div64_s64(high_thr, QPNP_ADC_625_UV);
	if (sign)
		high_thr = -high_thr;
	*high_threshold = high_thr + vbatt_param.adc_gnd;

	pr_debug("high_volt:%d, low_volt:%d\n", param->high_thr,
				param->low_thr);
	pr_debug("adc_code_high:%x, adc_code_low:%x\n", *high_threshold,
				*low_threshold);
	return 0;
}
EXPORT_SYMBOL(qpnp_adc_absolute_rthr);

int32_t qpnp_adc_vbatt_rscaler(struct qpnp_adc_drv *adc,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold)
{
	return qpnp_adc_absolute_rthr(adc, param, low_threshold,
							high_threshold);
}
EXPORT_SYMBOL(qpnp_adc_vbatt_rscaler);

int32_t qpnp_adc_qrd_215_btm_scaler(struct qpnp_adc_drv *adc,
			struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold)
{
	struct qpnp_vadc_linear_graph btm_param;
	int64_t low_output = 0, high_output = 0;
	int rc = 0;

	rc = qpnp_get_vadc_gain_and_offset(adc, &btm_param, CALIB_RATIOMETRIC);
	if (rc < 0)
		return rc;

	pr_debug("warm_temp:%d and cool_temp:%d\n", param->high_temp,
				param->low_temp);
	rc = qpnp_adc_map_temp_voltage(
		adcmap_batt_therm_qrd_215,
		ARRAY_SIZE(adcmap_batt_therm_qrd_215),
		(param->low_temp),
		&low_output);
	if (rc) {
		pr_debug("low_temp mapping failed with %d\n", rc);
		return rc;
	}

	pr_debug("low_output:%lld\n", low_output);
	low_output *= btm_param.dy;
	low_output = div64_s64(low_output, btm_param.adc_vref);
	low_output += btm_param.adc_gnd;

	rc = qpnp_adc_map_temp_voltage(
		adcmap_batt_therm_qrd_215,
		ARRAY_SIZE(adcmap_batt_therm_qrd_215),
		(param->high_temp),
		&high_output);
	if (rc) {
		pr_debug("high temp mapping failed with %d\n", rc);
		return rc;
	}

	pr_debug("high_output:%lld\n", high_output);
	high_output *= btm_param.dy;
	high_output = div64_s64(high_output, btm_param.adc_vref);
	high_output += btm_param.adc_gnd;

	/* btm low temperature correspondes to high voltage threshold */
	*low_threshold = high_output;
	/* btm high temperature correspondes to low voltage threshold */
	*high_threshold = low_output;

	pr_debug("high_volt:%d, low_volt:%d\n", *high_threshold,
				*low_threshold);
	return 0;
}
EXPORT_SYMBOL(qpnp_adc_qrd_215_btm_scaler);

int32_t qpnp_adc_smb_btm_rscaler(struct qpnp_adc_drv *adc,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold)
{
	struct qpnp_vadc_linear_graph btm_param;
	int64_t low_output = 0, high_output = 0;
	int rc = 0;

	rc = qpnp_get_vadc_gain_and_offset(adc, &btm_param, CALIB_RATIOMETRIC);
	if (rc < 0)
		return rc;

	pr_debug("warm_temp:%d and cool_temp:%d\n", param->high_temp,
				param->low_temp);
	rc = qpnp_adc_map_voltage_temp(
		adcmap_smb_batt_therm,
		ARRAY_SIZE(adcmap_smb_batt_therm),
		(param->low_temp),
		&low_output);
	if (rc) {
		pr_debug("low_temp mapping failed with %d\n", rc);
		return rc;
	}

	pr_debug("low_output:%lld\n", low_output);
	low_output *= btm_param.dy;
	low_output = div64_s64(low_output, btm_param.adc_vref);
	low_output += btm_param.adc_gnd;

	rc = qpnp_adc_map_voltage_temp(
		adcmap_smb_batt_therm,
		ARRAY_SIZE(adcmap_smb_batt_therm),
		(param->high_temp),
		&high_output);
	if (rc) {
		pr_debug("high temp mapping failed with %d\n", rc);
		return rc;
	}

	pr_debug("high_output:%lld\n", high_output);
	high_output *= btm_param.dy;
	high_output = div64_s64(high_output, btm_param.adc_vref);
	high_output += btm_param.adc_gnd;

	/* btm low temperature correspondes to high voltage threshold */
	*low_threshold = high_output;
	/* btm high temperature correspondes to low voltage threshold */
	*high_threshold = low_output;

	pr_debug("high_volt:%d, low_volt:%d\n", *high_threshold,
				*low_threshold);
	return 0;
}
EXPORT_SYMBOL(qpnp_adc_smb_btm_rscaler);

int qpnp_adc_get_revid_version(struct device *dev)
{
	struct pmic_revid_data *revid_data;
	struct device_node *revid_dev_node;

	revid_dev_node = of_parse_phandle(dev->of_node,
						"qcom,pmic-revid", 0);
	if (!revid_dev_node) {
		pr_debug("Missing qcom,pmic-revid property\n");
		return -EINVAL;
	}

	revid_data = get_revid_data(revid_dev_node);
	if (IS_ERR(revid_data)) {
		pr_debug("revid error rc = %ld\n", PTR_ERR(revid_data));
		return -EINVAL;
	}

	if (!revid_data)
		return -EINVAL;

	if ((revid_data->rev1 == PM8941_V3P1_REV1) &&
		(revid_data->rev2 == PM8941_V3P1_REV2) &&
		(revid_data->rev3 == PM8941_V3P1_REV3) &&
		(revid_data->rev4 == PM8941_V3P1_REV4) &&
		(revid_data->pmic_subtype == PM8941_SUBTYPE))
		return QPNP_REV_ID_8941_3_1;
	else if ((revid_data->rev1 == PM8941_V3P0_REV1) &&
		(revid_data->rev2 == PM8941_V3P0_REV2) &&
		(revid_data->rev3 == PM8941_V3P0_REV3) &&
		(revid_data->rev4 == PM8941_V3P0_REV4) &&
		(revid_data->pmic_subtype == PM8941_SUBTYPE))
		return QPNP_REV_ID_8941_3_0;
	else if ((revid_data->rev1 == PM8941_V2P0_REV1) &&
		(revid_data->rev2 == PM8941_V2P0_REV2) &&
		(revid_data->rev3 == PM8941_V2P0_REV3) &&
		(revid_data->rev4 == PM8941_V2P0_REV4) &&
		(revid_data->pmic_subtype == PM8941_SUBTYPE))
		return QPNP_REV_ID_8941_2_0;
	else if ((revid_data->rev1 == PM8226_V2P2_REV1) &&
		(revid_data->rev2 == PM8226_V2P2_REV2) &&
		(revid_data->rev3 == PM8226_V2P2_REV3) &&
		(revid_data->rev4 == PM8226_V2P2_REV4) &&
		(revid_data->pmic_subtype == PM8226_SUBTYPE))
		return QPNP_REV_ID_8026_2_2;
	else if ((revid_data->rev1 == PM8226_V2P1_REV1) &&
		(revid_data->rev2 == PM8226_V2P1_REV2) &&
		(revid_data->rev3 == PM8226_V2P1_REV3) &&
		(revid_data->rev4 == PM8226_V2P1_REV4) &&
		(revid_data->pmic_subtype == PM8226_SUBTYPE))
		return QPNP_REV_ID_8026_2_1;
	else if ((revid_data->rev1 == PM8226_V2P0_REV1) &&
		(revid_data->rev2 == PM8226_V2P0_REV2) &&
		(revid_data->rev3 == PM8226_V2P0_REV3) &&
		(revid_data->rev4 == PM8226_V2P0_REV4) &&
		(revid_data->pmic_subtype == PM8226_SUBTYPE))
		return QPNP_REV_ID_8026_2_0;
	else if ((revid_data->rev1 == PM8226_V1P0_REV1) &&
		(revid_data->rev2 == PM8226_V1P0_REV2) &&
		(revid_data->rev3 == PM8226_V1P0_REV3) &&
		(revid_data->rev4 == PM8226_V1P0_REV4) &&
		(revid_data->pmic_subtype == PM8226_SUBTYPE))
		return QPNP_REV_ID_8026_1_0;
	else if ((revid_data->rev1 == PM8110_V1P0_REV1) &&
		(revid_data->rev2 == PM8110_V1P0_REV2) &&
		(revid_data->rev3 == PM8110_V1P0_REV3) &&
		(revid_data->rev4 == PM8110_V1P0_REV4) &&
		(revid_data->pmic_subtype == PM8110_SUBTYPE))
		return QPNP_REV_ID_8110_1_0;
	else if ((revid_data->rev1 == PM8110_V2P0_REV1) &&
		(revid_data->rev2 == PM8110_V2P0_REV2) &&
		(revid_data->rev3 == PM8110_V2P0_REV3) &&
		(revid_data->rev4 == PM8110_V2P0_REV4) &&
		(revid_data->pmic_subtype == PM8110_SUBTYPE))
		return QPNP_REV_ID_8110_2_0;
	else if ((revid_data->rev1 == PM8916_V1P0_REV1) &&
		(revid_data->rev2 == PM8916_V1P0_REV2) &&
		(revid_data->rev3 == PM8916_V1P0_REV3) &&
		(revid_data->rev4 == PM8916_V1P0_REV4) &&
		(revid_data->pmic_subtype == PM8916_SUBTYPE))
		return QPNP_REV_ID_8916_1_0;
	else if ((revid_data->rev1 == PM8916_V1P1_REV1) &&
		(revid_data->rev2 == PM8916_V1P1_REV2) &&
		(revid_data->rev3 == PM8916_V1P1_REV3) &&
		(revid_data->rev4 == PM8916_V1P1_REV4) &&
		(revid_data->pmic_subtype == PM8916_SUBTYPE))
		return QPNP_REV_ID_8916_1_1;
	else if ((revid_data->rev1 == PM8916_V2P0_REV1) &&
		(revid_data->rev2 == PM8916_V2P0_REV2) &&
		(revid_data->rev3 == PM8916_V2P0_REV3) &&
		(revid_data->rev4 == PM8916_V2P0_REV4) &&
		(revid_data->pmic_subtype == PM8916_SUBTYPE))
		return QPNP_REV_ID_8916_2_0;
	else if ((revid_data->rev1 == PM8909_V1P0_REV1) &&
		(revid_data->rev2 == PM8909_V1P0_REV2) &&
		(revid_data->rev3 == PM8909_V1P0_REV3) &&
		(revid_data->rev4 == PM8909_V1P0_REV4) &&
		(revid_data->pmic_subtype == PM8909_SUBTYPE))
		return QPNP_REV_ID_8909_1_0;
	else if ((revid_data->rev1 == PM8909_V1P1_REV1) &&
		(revid_data->rev2 == PM8909_V1P1_REV2) &&
		(revid_data->rev3 == PM8909_V1P1_REV3) &&
		(revid_data->rev4 == PM8909_V1P1_REV4) &&
		(revid_data->pmic_subtype == PM8909_SUBTYPE))
		return QPNP_REV_ID_8909_1_1;
	else if ((revid_data->rev4 == PM8950_V1P0_REV4) &&
		(revid_data->pmic_subtype == PM8950_SUBTYPE))
		return QPNP_REV_ID_PM8950_1_0;
	else
		return -EINVAL;
}
EXPORT_SYMBOL(qpnp_adc_get_revid_version);

int32_t qpnp_adc_get_devicetree_data(struct platform_device *pdev,
			struct qpnp_adc_drv *adc_qpnp)
{
	struct device_node *node = pdev->dev.of_node;
	unsigned int base;
	struct device_node *child;
	struct qpnp_adc_amux *adc_channel_list;
	struct qpnp_adc_properties *adc_prop;
	struct qpnp_adc_amux_properties *amux_prop;
	int count_adc_channel_list = 0, decimation = 0, rc = 0, i = 0;

	if (!node)
		return -EINVAL;

	for_each_child_of_node(node, child)
		count_adc_channel_list++;

	if (!count_adc_channel_list) {
		pr_err("No channel listing\n");
		return -EINVAL;
	}

	adc_qpnp->pdev = pdev;

	adc_prop = devm_kzalloc(&pdev->dev,
				sizeof(struct qpnp_adc_properties),
					GFP_KERNEL);
	if (!adc_prop)
		return -ENOMEM;

	adc_channel_list = devm_kzalloc(&pdev->dev,
		((sizeof(struct qpnp_adc_amux)) * count_adc_channel_list),
				GFP_KERNEL);
	if (!adc_channel_list)
		return -ENOMEM;

	amux_prop = devm_kzalloc(&pdev->dev,
		sizeof(struct qpnp_adc_amux_properties) +
		sizeof(struct qpnp_vadc_chan_properties), GFP_KERNEL);
	if (!amux_prop) {
		dev_err(&pdev->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	adc_qpnp->adc_channels = adc_channel_list;
	adc_qpnp->amux_prop = amux_prop;

	for_each_child_of_node(node, child) {
		int channel_num, scaling = 0, post_scaling = 0;
		int fast_avg_setup = 0, calib_type = 0, rc, hw_settle_time = 0;
		const char *channel_name;

		channel_name = of_get_property(child,
				"label", NULL) ? : child->name;

		if (!channel_name) {
			pr_err("Invalid channel name\n");
			return -EINVAL;
		}

		rc = of_property_read_u32(child, "reg", &channel_num);
		if (rc) {
			pr_err("Invalid channel num\n");
			return -EINVAL;
		}

		rc = of_property_read_u32(child,
			"qcom,hw-settle-time", &hw_settle_time);
		if (rc) {
			pr_err("Invalid channel hw settle time property\n");
			return -EINVAL;
		}

		rc = of_property_read_u32(child,
			"qcom,pre-div-channel-scaling", &scaling);
		if (rc) {
			pr_err("Invalid channel scaling property\n");
			return -EINVAL;
		}

		rc = of_property_read_u32(child,
			"qcom,scale-fn-type", &post_scaling);
		if (rc) {
			pr_err("Invalid channel post scaling property\n");
			return -EINVAL;
		}

		if (of_property_read_bool(child, "qcom,ratiometric"))
			calib_type = CALIB_RATIOMETRIC;
		else
			calib_type = CALIB_ABSOLUTE;

		rc = of_property_read_u32(child,
			"qcom,decimation", &decimation);
		if (rc) {
			pr_err("Invalid decimation\n");
			return -EINVAL;
		}

		rc = of_property_read_u32(child,
			"qcom,fast-avg-setup", &fast_avg_setup);
		if (rc) {
			pr_err("Invalid channel fast average setup\n");
			return -EINVAL;
		}

		/* Individual channel properties */
		adc_channel_list[i].name = (char *)channel_name;
		adc_channel_list[i].channel_num = channel_num;
		adc_channel_list[i].adc_decimation = decimation;
		adc_channel_list[i].fast_avg_setup = fast_avg_setup;

		adc_channel_list[i].chan_path_prescaling = scaling;
		adc_channel_list[i].adc_scale_fn = post_scaling;
		adc_channel_list[i].hw_settle_time = hw_settle_time;
		adc_channel_list[i].calib_type = calib_type;

		i++;
	}

	/* Get the ADC VDD reference voltage and ADC bit resolution */
	rc = of_property_read_u32(node, "qcom,adc-vdd-reference",
			&adc_prop->adc_vdd_reference);
	if (rc) {
		pr_err("Invalid adc vdd reference property\n");
		return -EINVAL;
	}
	adc_qpnp->adc_prop = adc_prop;

	/* Get the peripheral address */
	rc = of_property_read_u32(pdev->dev.of_node, "reg", &base);
	if (rc < 0) {
		dev_err(&pdev->dev,
			"Couldn't find reg in node = %s rc = %d\n",
			pdev->dev.of_node->full_name, rc);
		return rc;
	}

	adc_qpnp->offset = base;

	/* Register the ADC peripheral interrupt */
	adc_qpnp->adc_irq_eoc = platform_get_irq_byname(pdev,
							"eoc-int-en-set");
	if (adc_qpnp->adc_irq_eoc < 0) {
		pr_err("Invalid irq\n");
		return -ENXIO;
	}

	return 0;
}
EXPORT_SYMBOL(qpnp_adc_get_devicetree_data);
