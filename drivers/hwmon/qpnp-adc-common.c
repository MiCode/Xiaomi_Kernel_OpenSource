/* Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/hwmon.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/spmi.h>
#include <linux/platform_device.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/qpnp/qpnp-adc.h>

#define KELVINMIL_DEGMIL	273160
#define QPNP_VADC_LDO_VOLTAGE_MIN	1800000
#define QPNP_VADC_LDO_VOLTAGE_MAX	1800000
#define QPNP_VADC_OK_VOLTAGE_MIN	1000000
#define QPNP_VADC_OK_VOLTAGE_MAX	1000000
#define PMI_CHG_SCALE_1		-138890
#define PMI_CHG_SCALE_2		391750000000
#define QPNP_VADC_HC_VREF_CODE		0x4000
#define QPNP_VADC_HC_VDD_REFERENCE_MV	1875
#define CHRG_SCALE_1 -250
#define CHRG_SCALE_2 377500000
#define DIE_SCALE_1 500
#define DIE_SCALE_2 -273150000

/* Clamp negative ADC code to 0 */
#define QPNP_VADC_HC_MAX_CODE		0x7FFF

/*Invalid current reading*/
#define QPNP_IADC_INV		0x8000

#define IADC_SCALE_1 0xffff
#define IADC_SCALE_2 152593

#define USBIN_I_SCALE 25

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

static const struct qpnp_vadc_map_pt adcmap_qrd_btm_threshold[] = {
	{-200,	1540},
	{-180,	1517},
	{-160,	1492},
	{-140,	1467},
	{-120,	1440},
	{-100,	1412},
	{-80,	1383},
	{-60,	1353},
	{-40,	1323},
	{-20,	1292},
	{0,	1260},
	{20,	1228},
	{40,	1196},
	{60,	1163},
	{80,	1131},
	{100,	1098},
	{120,	1066},
	{140,	1034},
	{160,	1002},
	{180,	971},
	{200,	941},
	{220,	911},
	{240,	882},
	{260,	854},
	{280,	826},
	{300,	800},
	{320,	774},
	{340,	749},
	{360,	726},
	{380,	703},
	{400,	681},
	{420,	660},
	{440,	640},
	{460,	621},
	{480,	602},
	{500,	585},
	{520,	568},
	{540,	552},
	{560,	537},
	{580,	523},
	{600,	510},
	{620,	497},
	{640,	485},
	{660,	473},
	{680,	462},
	{700,	452},
	{720,	442},
	{740,	433},
	{760,	424},
	{780,	416},
	{800,	408},
};

static const struct qpnp_vadc_map_pt adcmap_qrd_skuaa_btm_threshold[] = {
	{-200,	1476},
	{-180,	1450},
	{-160,	1422},
	{-140,	1394},
	{-120,	1365},
	{-100,	1336},
	{-80,	1306},
	{-60,	1276},
	{-40,	1246},
	{-20,	1216},
	{0,	1185},
	{20,	1155},
	{40,	1126},
	{60,	1096},
	{80,	1068},
	{100,	1040},
	{120,	1012},
	{140,	986},
	{160,	960},
	{180,	935},
	{200,	911},
	{220,	888},
	{240,	866},
	{260,	844},
	{280,	824},
	{300,	805},
	{320,	786},
	{340,	769},
	{360,	752},
	{380,	737},
	{400,	722},
	{420,	707},
	{440,	694},
	{460,	681},
	{480,	669},
	{500,	658},
	{520,	648},
	{540,	637},
	{560,	628},
	{580,	619},
	{600,	611},
	{620,	603},
	{640,	595},
	{660,	588},
	{680,	582},
	{700,	575},
	{720,	569},
	{740,	564},
	{760,	559},
	{780,	554},
	{800,	549},
};

static const struct qpnp_vadc_map_pt adcmap_qrd_skug_btm_threshold[] = {
	{-200,	1338},
	{-180,	1307},
	{-160,	1276},
	{-140,	1244},
	{-120,	1213},
	{-100,	1182},
	{-80,	1151},
	{-60,	1121},
	{-40,	1092},
	{-20,	1063},
	{0,	1035},
	{20,	1008},
	{40,	982},
	{60,	957},
	{80,	933},
	{100,	910},
	{120,	889},
	{140,	868},
	{160,	848},
	{180,	830},
	{200,	812},
	{220,	795},
	{240,	780},
	{260,	765},
	{280,	751},
	{300,	738},
	{320,	726},
	{340,	714},
	{360,	704},
	{380,	694},
	{400,	684},
	{420,	675},
	{440,	667},
	{460,	659},
	{480,	652},
	{500,	645},
	{520,	639},
	{540,	633},
	{560,	627},
	{580,	622},
	{600,	617},
	{620,	613},
	{640,	608},
	{660,	604},
	{680,	600},
	{700,	597},
	{720,	593},
	{740,	590},
	{760,	587},
	{780,	585},
	{800,	582},
};

static const struct qpnp_vadc_map_pt adcmap_qrd_skuh_btm_threshold[] = {
	{-200,	1531},
	{-180,	1508},
	{-160,	1483},
	{-140,	1458},
	{-120,	1432},
	{-100,	1404},
	{-80,	1377},
	{-60,	1348},
	{-40,	1319},
	{-20,	1290},
	{0,	1260},
	{20,	1230},
	{40,	1200},
	{60,	1171},
	{80,	1141},
	{100,	1112},
	{120,	1083},
	{140,	1055},
	{160,	1027},
	{180,	1000},
	{200,	973},
	{220,	948},
	{240,	923},
	{260,	899},
	{280,	876},
	{300,	854},
	{320,	832},
	{340,	812},
	{360,	792},
	{380,	774},
	{400,	756},
	{420,	739},
	{440,	723},
	{460,	707},
	{480,	692},
	{500,	679},
	{520,	665},
	{540,	653},
	{560,	641},
	{580,	630},
	{600,	619},
	{620,	609},
	{640,	600},
	{660,	591},
	{680,	583},
	{700,	575},
	{720,	567},
	{740,	560},
	{760,	553},
	{780,	547},
	{800,	541},
	{820,	535},
	{840,	530},
	{860,	524},
	{880,	520},
};

static const struct qpnp_vadc_map_pt adcmap_qrd_skut1_btm_threshold[] = {
	{-400,	1759},
	{-350,	1742},
	{-300,	1720},
	{-250,	1691},
	{-200,	1654},
	{-150,	1619},
	{-100,	1556},
	{-50,	1493},
	{0,	1422},
	{50,	1345},
	{100,	1264},
	{150,	1180},
	{200,	1097},
	{250,	1017},
	{300,	942},
	{350,	873},
	{400,	810},
	{450,	754},
	{500,	706},
	{550,	664},
	{600,	627},
	{650,	596},
	{700,	570},
	{750,	547},
	{800,	528},
	{850,	512},
	{900,	499},
	{950,	487},
	{1000,	477},
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

/* Voltage to temperature */
static const struct qpnp_vadc_map_pt adcmap_150k_104ef_104fb[] = {
	{1738,	-40000},
	{1714,	-35000},
	{1682,	-30000},
	{1641,	-25000},
	{1589,	-20000},
	{1526,	-15000},
	{1451,	-10000},
	{1363,	-5000},
	{1266,	0},
	{1159,	5000},
	{1048,	10000},
	{936,	15000},
	{825,	20000},
	{720,	25000},
	{622,	30000},
	{533,	35000},
	{454,	40000},
	{385,	45000},
	{326,	50000},
	{275,	55000},
	{232,	60000},
	{195,	65000},
	{165,	70000},
	{139,	75000},
	{118,	80000},
	{100,	85000},
	{85,	90000},
	{73,	95000},
	{62,	100000},
	{53,	105000},
	{46,	110000},
	{40,	115000},
	{34,	120000},
	{30,	125000}
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
static const struct qpnp_vadc_map_pt adcmap_ncp03wf683[] = {
	{1742,	-40},
	{1718,	-35},
	{1687,	-30},
	{1647,	-25},
	{1596,	-20},
	{1534,	-15},
	{1459,	-10},
	{1372,	-5},
	{1275,	0},
	{1169,	5},
	{1058,	10},
	{945,	15},
	{834,	20},
	{729,	25},
	{630,	30},
	{541,	35},
	{461,	40},
	{392,	45},
	{332,	50},
	{280,	55},
	{236,	60},
	{199,	65},
	{169,	70},
	{142,	75},
	{121,	80},
	{102,	85},
	{87,	90},
	{74,	95},
	{64,	100},
	{55,	105},
	{47,	110},
	{40,	115},
	{35,	120},
	{30,	125}
};

/* Voltage to temperature */
static const struct qpnp_vadc_map_pt adcmap_batt_therm[] = {
	{1770,	-400},
	{1757,	-380},
	{1743,	-360},
	{1727,	-340},
	{1710,	-320},
	{1691,	-300},
	{1671,	-280},
	{1650,	-260},
	{1627,	-240},
	{1602,	-220},
	{1576,	-200},
	{1548,	-180},
	{1519,	-160},
	{1488,	-140},
	{1456,	-120},
	{1423,	-100},
	{1388,	-80},
	{1353,	-60},
	{1316,	-40},
	{1278,	-20},
	{1240,	0},
	{1201,	20},
	{1162,	40},
	{1122,	60},
	{1082,	80},
	{1042,	100},
	{1003,	120},
	{964,	140},
	{925,	160},
	{887,	180},
	{849,	200},
	{812,	220},
	{777,	240},
	{742,	260},
	{708,	280},
	{675,	300},
	{643,	320},
	{613,	340},
	{583,	360},
	{555,	380},
	{528,	400},
	{502,	420},
	{477,	440},
	{453,	460},
	{430,	480},
	{409,	500},
	{388,	520},
	{369,	540},
	{350,	560},
	{333,	580},
	{316,	600},
	{300,	620},
	{285,	640},
	{271,	660},
	{257,	680},
	{245,	700},
	{233,	720},
	{221,	740},
	{210,	760},
	{200,	780},
	{190,	800},
	{181,	820},
	{173,	840},
	{164,	860},
	{157,	880},
	{149,	900},
	{142,	920},
	{136,	940},
	{129,	960},
	{124,	980}
};

/* Voltage to temperature */
static const struct qpnp_vadc_map_pt adcmap_batt_therm_qrd[] = {
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
	{1475,	00},
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

/* Voltage to temperature */
static const struct qpnp_vadc_map_pt adcmap_batt_therm_pu30[] = {
	{1842,	-400},
	{1838,	-380},
	{1833,	-360},
	{1828,	-340},
	{1822,	-320},
	{1816,	-300},
	{1809,	-280},
	{1801,	-260},
	{1793,	-240},
	{1784,	-220},
	{1774,	-200},
	{1763,	-180},
	{1752,	-160},
	{1739,	-140},
	{1726,	-120},
	{1712,	-100},
	{1697,	-80},
	{1680,	-60},
	{1663,	-40},
	{1645,	-20},
	{1625,	0},
	{1605,	20},
	{1583,	40},
	{1561,	60},
	{1537,	80},
	{1513,	100},
	{1487,	120},
	{1461,	140},
	{1433,	160},
	{1405,	180},
	{1376,	200},
	{1347,	220},
	{1316,	240},
	{1286,	260},
	{1254,	280},
	{1223,	300},
	{1191,	320},
	{1159,	340},
	{1126,	360},
	{1094,	380},
	{1062,	400},
	{1029,	420},
	{997,	440},
	{966,	460},
	{934,	480},
	{903,	500},
	{873,	520},
	{843,	540},
	{813,	560},
	{784,	580},
	{756,	600},
	{728,	620},
	{702,	640},
	{675,	660},
	{650,	680},
	{625,	700},
	{601,	720},
	{578,	740},
	{556,	760},
	{534,	780},
	{513,	800},
	{493,	820},
	{474,	840},
	{455,	860},
	{437,	880},
	{420,	900},
	{403,	920},
	{387,	940},
	{372,	960},
	{357,	980}
};

/* Voltage to temp0erature */
static const struct qpnp_vadc_map_pt adcmap_batt_therm_pu400[] = {
	{1516,	-400},
	{1478,	-380},
	{1438,	-360},
	{1396,	-340},
	{1353,	-320},
	{1307,	-300},
	{1261,	-280},
	{1213,	-260},
	{1164,	-240},
	{1115,	-220},
	{1066,	-200},
	{1017,	-180},
	{968,	-160},
	{920,	-140},
	{872,	-120},
	{826,	-100},
	{781,	-80},
	{737,	-60},
	{694,	-40},
	{654,	-20},
	{615,	0},
	{578,	20},
	{542,	40},
	{509,	60},
	{477,	80},
	{447,	100},
	{419,	120},
	{392,	140},
	{367,	160},
	{343,	180},
	{321,	200},
	{301,	220},
	{282,	240},
	{264,	260},
	{247,	280},
	{231,	300},
	{216,	320},
	{203,	340},
	{190,	360},
	{178,	380},
	{167,	400},
	{157,	420},
	{147,	440},
	{138,	460},
	{130,	480},
	{122,	500},
	{115,	520},
	{108,	540},
	{102,	560},
	{96,	580},
	{90,	600},
	{85,	620},
	{80,	640},
	{76,	660},
	{72,	680},
	{68,	700},
	{64,	720},
	{61,	740},
	{57,	760},
	{54,	780},
	{52,	800},
	{49,	820},
	{46,	840},
	{44,	860},
	{42,	880},
	{40,	900},
	{38,	920},
	{36,	940},
	{34,	960},
	{32,	980}
};

/*
 * Voltage to temperature table for 100k pull up for NTCG104EF104 with
 * 1.875V reference.
 */
static const struct qpnp_vadc_map_pt adcmap_100k_104ef_104fb_1875_vref[] = {
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

static int32_t qpnp_adc_map_voltage_temp(const struct qpnp_vadc_map_pt *pts,
		uint32_t tablesize, int32_t input, int64_t *output)
{
	bool descending = 1;
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
	bool descending = 1;
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

static void qpnp_adc_scale_with_calib_param(int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		int64_t *scale_voltage)
{
	*scale_voltage = (adc_code -
		chan_properties->adc_graph[chan_properties->calib_type].adc_gnd)
		* chan_properties->adc_graph[chan_properties->calib_type].dx;
	*scale_voltage = div64_s64(*scale_voltage,
		chan_properties->adc_graph[chan_properties->calib_type].dy);

	if (chan_properties->calib_type == CALIB_ABSOLUTE)
		*scale_voltage +=
		chan_properties->adc_graph[chan_properties->calib_type].dx;

	if (*scale_voltage < 0)
		*scale_voltage = 0;
}

int32_t qpnp_adc_scale_pmic_therm(struct qpnp_vadc_chip *vadc,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int64_t pmic_voltage = 0;

	if (!chan_properties || !chan_properties->offset_gain_numerator ||
		!chan_properties->offset_gain_denominator || !adc_properties
		|| !adc_chan_result)
		return -EINVAL;

	if (adc_properties->adc_hc) {
		/* (ADC code * vref_vadc (1.875V)) / scale_code */
		if (adc_code > QPNP_VADC_HC_MAX_CODE)
			adc_code = 0;
		pmic_voltage = (int64_t) adc_code;
		pmic_voltage *= (int64_t) (adc_properties->adc_vdd_reference
							* 1000);
		pmic_voltage = div64_s64(pmic_voltage,
					adc_properties->full_scale_code);
	} else {
		if (!chan_properties->adc_graph[CALIB_ABSOLUTE].dy)
			return -EINVAL;
		qpnp_adc_scale_with_calib_param(adc_code, adc_properties,
					chan_properties, &pmic_voltage);
	}

	if (pmic_voltage > 0) {
		/* 2mV/K */
		adc_chan_result->measurement = pmic_voltage*
			chan_properties->offset_gain_denominator;

		adc_chan_result->measurement =
			div64_s64(adc_chan_result->measurement,
			chan_properties->offset_gain_numerator * 2);
	} else
		adc_chan_result->measurement = 0;

	/* Change to .001 deg C */
	adc_chan_result->measurement -= KELVINMIL_DEGMIL;
	adc_chan_result->physical = (int32_t) adc_chan_result->measurement;

	return 0;
}
EXPORT_SYMBOL(qpnp_adc_scale_pmic_therm);

int32_t qpnp_adc_scale_millidegc_pmic_voltage_thr(struct qpnp_vadc_chip *chip,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold)
{
	struct qpnp_vadc_linear_graph btm_param;
	int64_t low_output = 0, high_output = 0;
	int rc = 0, sign = 0;

	/* Convert to Kelvin and account for voltage to be written as 2mV/K */
	low_output = (param->low_temp + KELVINMIL_DEGMIL) * 2;
	/* Convert to Kelvin and account for voltage to be written as 2mV/K */
	high_output = (param->high_temp + KELVINMIL_DEGMIL) * 2;

	if (param->adc_tm_hc) {
		low_output *= param->full_scale_code;
		low_output = div64_s64(low_output,
				(QPNP_VADC_HC_VDD_REFERENCE_MV * 1000));
		high_output *= param->full_scale_code;
		high_output = div64_s64(high_output,
				(QPNP_VADC_HC_VDD_REFERENCE_MV * 1000));
	} else {
		rc = qpnp_get_vadc_gain_and_offset(chip, &btm_param,
							CALIB_ABSOLUTE);
		if (rc < 0) {
		pr_err("Could not acquire gain and offset\n");
		return rc;
		}

		/* Convert to voltage threshold */
		low_output = (low_output - QPNP_ADC_625_UV) * btm_param.dy;
		if (low_output < 0) {
			sign = 1;
			low_output = -low_output;
		}
		low_output = div64_s64(low_output, QPNP_ADC_625_UV);
		if (sign)
			low_output = -low_output;
		low_output += btm_param.adc_gnd;

		sign = 0;
		/* Convert to voltage threshold */
		high_output = (high_output - QPNP_ADC_625_UV) * btm_param.dy;
		if (high_output < 0) {
			sign = 1;
			high_output = -high_output;
		}
		high_output = div64_s64(high_output, QPNP_ADC_625_UV);
		if (sign)
			high_output = -high_output;
		high_output += btm_param.adc_gnd;
	}

	*low_threshold = (uint32_t) low_output;
	*high_threshold = (uint32_t) high_output;

	pr_debug("high_temp:%d, low_temp:%d\n", param->high_temp,
				param->low_temp);
	pr_debug("adc_code_high:%x, adc_code_low:%x\n", *high_threshold,
				*low_threshold);

	return 0;
}
EXPORT_SYMBOL(qpnp_adc_scale_millidegc_pmic_voltage_thr);

/* Scales the ADC code to degC using the mapping
 * table for the XO thermistor.
 */
int32_t qpnp_adc_tdkntcg_therm(struct qpnp_vadc_chip *chip,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int64_t xo_thm_voltage = 0;

	if (!chan_properties || !chan_properties->offset_gain_numerator ||
		!chan_properties->offset_gain_denominator || !adc_properties
		|| !adc_chan_result)
		return -EINVAL;

	if (adc_properties->adc_hc) {
		/* (code * vref_vadc (1.875V) * 1000) / (scale_code * 1000) */
		if (adc_code > QPNP_VADC_HC_MAX_CODE)
			adc_code = 0;
		xo_thm_voltage = (int64_t) adc_code;
		xo_thm_voltage *= (int64_t) (adc_properties->adc_vdd_reference
							* 1000);
		xo_thm_voltage = div64_s64(xo_thm_voltage,
				adc_properties->full_scale_code * 1000);
		qpnp_adc_map_voltage_temp(adcmap_100k_104ef_104fb_1875_vref,
			ARRAY_SIZE(adcmap_100k_104ef_104fb_1875_vref),
			xo_thm_voltage, &adc_chan_result->physical);
	} else {
		qpnp_adc_scale_with_calib_param(adc_code,
			adc_properties, chan_properties, &xo_thm_voltage);

		if (chan_properties->calib_type == CALIB_ABSOLUTE)
			xo_thm_voltage = div64_s64(xo_thm_voltage, 1000);

		qpnp_adc_map_voltage_temp(adcmap_100k_104ef_104fb,
			ARRAY_SIZE(adcmap_100k_104ef_104fb),
			xo_thm_voltage, &adc_chan_result->physical);
	}

	return 0;
}
EXPORT_SYMBOL(qpnp_adc_tdkntcg_therm);

int32_t qpnp_adc_batt_therm(struct qpnp_vadc_chip *chip,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int64_t batt_thm_voltage = 0;

	if (!chan_properties || !chan_properties->offset_gain_numerator ||
		!chan_properties->offset_gain_denominator || !adc_properties
		|| !adc_chan_result)
		return -EINVAL;

	if (adc_properties->adc_hc) {
		/* (code * vref_vadc (1.875V) * 1000) / (scale_code * 1000) */
		if (adc_code > QPNP_VADC_HC_MAX_CODE)
			adc_code = 0;
		batt_thm_voltage = (int64_t) adc_code;
		batt_thm_voltage *= (adc_properties->adc_vdd_reference
							* 1000);
		batt_thm_voltage = div64_s64(batt_thm_voltage,
				adc_properties->full_scale_code * 1000);
		qpnp_adc_map_voltage_temp(adcmap_batt_therm,
			ARRAY_SIZE(adcmap_batt_therm),
			batt_thm_voltage, &adc_chan_result->physical);
	}
	return 0;
}
EXPORT_SYMBOL(qpnp_adc_batt_therm);

int32_t qpnp_adc_batt_therm_qrd(struct qpnp_vadc_chip *chip,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int64_t batt_thm_voltage = 0;

	if (!chan_properties || !chan_properties->offset_gain_numerator ||
		!chan_properties->offset_gain_denominator || !adc_properties
		|| !adc_chan_result)
		return -EINVAL;

	if (adc_properties->adc_hc) {
		/* (code * vref_vadc (1.875V) * 1000) / (scale_code * 1000) */
		if (adc_code > QPNP_VADC_HC_MAX_CODE)
			adc_code = 0;
		batt_thm_voltage = (int64_t) adc_code;
		batt_thm_voltage *= (adc_properties->adc_vdd_reference
							* 1000);
		batt_thm_voltage = div64_s64(batt_thm_voltage,
				adc_properties->full_scale_code * 1000);
		qpnp_adc_map_voltage_temp(adcmap_batt_therm_qrd,
			ARRAY_SIZE(adcmap_batt_therm_qrd),
			batt_thm_voltage, &adc_chan_result->physical);
	}
	return 0;
}
EXPORT_SYMBOL(qpnp_adc_batt_therm_qrd);

int32_t qpnp_adc_batt_therm_pu30(struct qpnp_vadc_chip *chip,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int64_t batt_thm_voltage = 0;

	if (!chan_properties || !chan_properties->offset_gain_numerator ||
		!chan_properties->offset_gain_denominator || !adc_properties
		|| !adc_chan_result)
		return -EINVAL;

	/* (code * vref_vadc (1.875V) * 1000) / (scale_code * 1000) */
	if (adc_code > QPNP_VADC_HC_MAX_CODE)
		adc_code = 0;
	batt_thm_voltage = (int64_t) adc_code;
	batt_thm_voltage *= (adc_properties->adc_vdd_reference
						* 1000);
	batt_thm_voltage = div64_s64(batt_thm_voltage,
			adc_properties->full_scale_code * 1000);
	qpnp_adc_map_voltage_temp(adcmap_batt_therm_pu30,
		ARRAY_SIZE(adcmap_batt_therm_pu30),
		batt_thm_voltage, &adc_chan_result->physical);
	return 0;
}
EXPORT_SYMBOL(qpnp_adc_batt_therm_pu30);

int32_t qpnp_adc_batt_therm_pu400(struct qpnp_vadc_chip *chip,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int64_t batt_thm_voltage = 0;

	if (!chan_properties || !chan_properties->offset_gain_numerator ||
		!chan_properties->offset_gain_denominator || !adc_properties
		|| !adc_chan_result)
		return -EINVAL;

	/* (code * vref_vadc (1.875V) * 1000) / (scale_code * 1000) */
	if (adc_code > QPNP_VADC_HC_MAX_CODE)
		adc_code = 0;
	batt_thm_voltage = (int64_t) adc_code;
	batt_thm_voltage *= (adc_properties->adc_vdd_reference
						* 1000);
	batt_thm_voltage = div64_s64(batt_thm_voltage,
			adc_properties->full_scale_code * 1000);
	qpnp_adc_map_voltage_temp(adcmap_batt_therm_pu400,
		ARRAY_SIZE(adcmap_batt_therm_pu400),
		batt_thm_voltage, &adc_chan_result->physical);
	return 0;
}
EXPORT_SYMBOL(qpnp_adc_batt_therm_pu400);

int32_t qpnp_adc_batt_therm_qrd_215(struct qpnp_vadc_chip *chip,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int64_t batt_thm_voltage = 0;

	if (!chan_properties || !chan_properties->offset_gain_numerator ||
		!chan_properties->offset_gain_denominator || !adc_properties
		|| !adc_chan_result)
		return -EINVAL;

	qpnp_adc_scale_with_calib_param(adc_code,
		adc_properties, chan_properties, &batt_thm_voltage);

	adc_chan_result->measurement = batt_thm_voltage;

	return qpnp_adc_map_voltage_temp(
			adcmap_batt_therm_qrd_215,
			ARRAY_SIZE(adcmap_batt_therm_qrd_215),
			batt_thm_voltage,
			&adc_chan_result->physical);

	return 0;
}
EXPORT_SYMBOL(qpnp_adc_batt_therm_qrd_215);

int32_t qpnp_adc_scale_batt_therm(struct qpnp_vadc_chip *chip,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int64_t bat_voltage = 0;

	qpnp_adc_scale_with_calib_param(adc_code,
			adc_properties, chan_properties, &bat_voltage);

	adc_chan_result->measurement = bat_voltage;

	return qpnp_adc_map_temp_voltage(
			adcmap_btm_threshold,
			ARRAY_SIZE(adcmap_btm_threshold),
			bat_voltage,
			&adc_chan_result->physical);
}
EXPORT_SYMBOL(qpnp_adc_scale_batt_therm);

int32_t qpnp_adc_scale_chrg_temp(struct qpnp_vadc_chip *vadc,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int rc = 0;

	if (!chan_properties || !chan_properties->offset_gain_numerator ||
		!chan_properties->offset_gain_denominator || !adc_properties
		|| !adc_chan_result)
		return -EINVAL;

	rc = qpnp_adc_scale_default(vadc, adc_code, adc_properties,
			chan_properties, adc_chan_result);
	if (rc < 0)
		return rc;

	pr_debug("raw_code:%x, v_adc:%lld\n", adc_code,
						adc_chan_result->physical);
	adc_chan_result->physical = (int64_t) ((CHRG_SCALE_1) *
					(adc_chan_result->physical));
	adc_chan_result->physical = (int64_t) (adc_chan_result->physical +
							CHRG_SCALE_2);
	adc_chan_result->physical = (int64_t) adc_chan_result->physical;
	adc_chan_result->physical = div64_s64(adc_chan_result->physical,
								1000000);

	return 0;
}
EXPORT_SYMBOL(qpnp_adc_scale_chrg_temp);

int32_t qpnp_adc_scale_die_temp(struct qpnp_vadc_chip *vadc,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int rc = 0;

	if (!chan_properties || !chan_properties->offset_gain_numerator ||
		!chan_properties->offset_gain_denominator || !adc_properties
		|| !adc_chan_result)
		return -EINVAL;

	rc = qpnp_adc_scale_default(vadc, adc_code, adc_properties,
			chan_properties, adc_chan_result);
	if (rc < 0)
		return rc;

	pr_debug("raw_code:%x, v_adc:%lld\n", adc_code,
						adc_chan_result->physical);
	adc_chan_result->physical = (int64_t) ((DIE_SCALE_1) *
					(adc_chan_result->physical));
	adc_chan_result->physical = (int64_t) (adc_chan_result->physical +
							DIE_SCALE_2);
	adc_chan_result->physical = (int64_t) adc_chan_result->physical;
	adc_chan_result->physical = div64_s64(adc_chan_result->physical,
								1000000);

	return 0;
}
EXPORT_SYMBOL(qpnp_adc_scale_die_temp);

int32_t qpnp_adc_scale_qrd_batt_therm(struct qpnp_vadc_chip *chip,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int64_t bat_voltage = 0;

	qpnp_adc_scale_with_calib_param(adc_code,
			adc_properties, chan_properties, &bat_voltage);

	adc_chan_result->measurement = bat_voltage;

	return qpnp_adc_map_temp_voltage(
			adcmap_qrd_btm_threshold,
			ARRAY_SIZE(adcmap_qrd_btm_threshold),
			bat_voltage,
			&adc_chan_result->physical);
}
EXPORT_SYMBOL(qpnp_adc_scale_qrd_batt_therm);

int32_t qpnp_adc_scale_qrd_skuaa_batt_therm(struct qpnp_vadc_chip *chip,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int64_t bat_voltage = 0;

	qpnp_adc_scale_with_calib_param(adc_code,
			adc_properties, chan_properties, &bat_voltage);

	adc_chan_result->measurement = bat_voltage;

	return qpnp_adc_map_temp_voltage(
			adcmap_qrd_skuaa_btm_threshold,
			ARRAY_SIZE(adcmap_qrd_skuaa_btm_threshold),
			bat_voltage,
			&adc_chan_result->physical);
}
EXPORT_SYMBOL(qpnp_adc_scale_qrd_skuaa_batt_therm);

int32_t qpnp_adc_scale_qrd_skug_batt_therm(struct qpnp_vadc_chip *chip,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int64_t bat_voltage = 0;

	qpnp_adc_scale_with_calib_param(adc_code,
			adc_properties, chan_properties, &bat_voltage);
	adc_chan_result->measurement = bat_voltage;

	return qpnp_adc_map_temp_voltage(
			adcmap_qrd_skug_btm_threshold,
			ARRAY_SIZE(adcmap_qrd_skug_btm_threshold),
			bat_voltage,
			&adc_chan_result->physical);
}
EXPORT_SYMBOL(qpnp_adc_scale_qrd_skug_batt_therm);

int32_t qpnp_adc_scale_qrd_skuh_batt_therm(struct qpnp_vadc_chip *chip,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int64_t bat_voltage = 0;

	qpnp_adc_scale_with_calib_param(adc_code,
			adc_properties, chan_properties, &bat_voltage);

	return qpnp_adc_map_temp_voltage(
			adcmap_qrd_skuh_btm_threshold,
			ARRAY_SIZE(adcmap_qrd_skuh_btm_threshold),
			bat_voltage,
			&adc_chan_result->physical);
}
EXPORT_SYMBOL(qpnp_adc_scale_qrd_skuh_batt_therm);

int32_t qpnp_adc_scale_qrd_skut1_batt_therm(struct qpnp_vadc_chip *chip,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int64_t bat_voltage = 0;

	qpnp_adc_scale_with_calib_param(adc_code,
			adc_properties, chan_properties, &bat_voltage);

	return qpnp_adc_map_temp_voltage(
			adcmap_qrd_skut1_btm_threshold,
			ARRAY_SIZE(adcmap_qrd_skut1_btm_threshold),
			bat_voltage,
			&adc_chan_result->physical);
}
EXPORT_SYMBOL(qpnp_adc_scale_qrd_skut1_batt_therm);

int32_t qpnp_adc_scale_smb_batt_therm(struct qpnp_vadc_chip *chip,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int64_t bat_voltage = 0;

	qpnp_adc_scale_with_calib_param(adc_code,
			adc_properties, chan_properties, &bat_voltage);

	return qpnp_adc_map_temp_voltage(
			adcmap_smb_batt_therm,
			ARRAY_SIZE(adcmap_smb_batt_therm),
			bat_voltage,
			&adc_chan_result->physical);
}
EXPORT_SYMBOL(qpnp_adc_scale_smb_batt_therm);

int32_t qpnp_adc_scale_therm_pu1(struct qpnp_vadc_chip *chip,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int64_t therm_voltage = 0;

	qpnp_adc_scale_with_calib_param(adc_code,
			adc_properties, chan_properties, &therm_voltage);

	qpnp_adc_map_voltage_temp(adcmap_150k_104ef_104fb,
		ARRAY_SIZE(adcmap_150k_104ef_104fb),
		therm_voltage, &adc_chan_result->physical);

	return 0;
}
EXPORT_SYMBOL(qpnp_adc_scale_therm_pu1);

int32_t qpnp_adc_scale_therm_pu2(struct qpnp_vadc_chip *chip,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int64_t therm_voltage = 0;

	if (!chan_properties || !chan_properties->offset_gain_numerator ||
		!chan_properties->offset_gain_denominator || !adc_properties)
		return -EINVAL;

	if (adc_properties->adc_hc) {
		/* (code * vref_vadc (1.875V) * 1000) / (scale code * 1000) */
		if (adc_code > QPNP_VADC_HC_MAX_CODE)
			adc_code = 0;
		therm_voltage = (int64_t) adc_code;
		therm_voltage *= (int64_t) (adc_properties->adc_vdd_reference
							* 1000);
		therm_voltage = div64_s64(therm_voltage,
				(adc_properties->full_scale_code * 1000));

		qpnp_adc_map_voltage_temp(adcmap_100k_104ef_104fb_1875_vref,
			ARRAY_SIZE(adcmap_100k_104ef_104fb_1875_vref),
			therm_voltage, &adc_chan_result->physical);
	} else {
		qpnp_adc_scale_with_calib_param(adc_code,
			adc_properties, chan_properties, &therm_voltage);

		if (chan_properties->calib_type == CALIB_ABSOLUTE)
			therm_voltage = div64_s64(therm_voltage, 1000);

		qpnp_adc_map_voltage_temp(adcmap_100k_104ef_104fb,
			ARRAY_SIZE(adcmap_100k_104ef_104fb),
			therm_voltage, &adc_chan_result->physical);
	}

	return 0;
}
EXPORT_SYMBOL(qpnp_adc_scale_therm_pu2);

int32_t qpnp_adc_tm_scale_voltage_therm_pu2(struct qpnp_vadc_chip *chip,
		const struct qpnp_adc_properties *adc_properties,
					uint32_t reg, int64_t *result)
{
	int64_t adc_voltage = 0;
	struct qpnp_vadc_linear_graph param1;
	int negative_offset = 0;

	if (adc_properties->adc_hc) {
		/* (ADC code * vref_vadc (1.875V)) / full_scale_code */
		if (reg > QPNP_VADC_HC_MAX_CODE)
			reg = 0;
		adc_voltage = (int64_t) reg;
		adc_voltage *= QPNP_VADC_HC_VDD_REFERENCE_MV;
		adc_voltage = div64_s64(adc_voltage,
				adc_properties->full_scale_code);
		qpnp_adc_map_voltage_temp(adcmap_100k_104ef_104fb_1875_vref,
			ARRAY_SIZE(adcmap_100k_104ef_104fb_1875_vref),
			adc_voltage, result);
	} else {
		qpnp_get_vadc_gain_and_offset(chip, &param1, CALIB_RATIOMETRIC);

		adc_voltage = (reg - param1.adc_gnd) * param1.adc_vref;
		if (adc_voltage < 0) {
			negative_offset = 1;
			adc_voltage = -adc_voltage;
		}

		adc_voltage = div64_s64(adc_voltage, param1.dy);

		qpnp_adc_map_voltage_temp(adcmap_100k_104ef_104fb,
			ARRAY_SIZE(adcmap_100k_104ef_104fb),
			adc_voltage, result);
		if (negative_offset)
			adc_voltage = -adc_voltage;
	}

	return 0;
}
EXPORT_SYMBOL(qpnp_adc_tm_scale_voltage_therm_pu2);

int32_t qpnp_adc_tm_scale_therm_voltage_pu2(struct qpnp_vadc_chip *chip,
			const struct qpnp_adc_properties *adc_properties,
				struct qpnp_adc_tm_config *param)
{
	struct qpnp_vadc_linear_graph param1;
	int rc;

	if (adc_properties->adc_hc) {
		rc = qpnp_adc_map_temp_voltage(
			adcmap_100k_104ef_104fb_1875_vref,
			ARRAY_SIZE(adcmap_100k_104ef_104fb_1875_vref),
			param->low_thr_temp, &param->low_thr_voltage);
		if (rc)
			return rc;
		param->low_thr_voltage *= adc_properties->full_scale_code;
		param->low_thr_voltage = div64_s64(param->low_thr_voltage,
						QPNP_VADC_HC_VDD_REFERENCE_MV);

		rc = qpnp_adc_map_temp_voltage(
			adcmap_100k_104ef_104fb_1875_vref,
			ARRAY_SIZE(adcmap_100k_104ef_104fb_1875_vref),
			param->high_thr_temp, &param->high_thr_voltage);
		if (rc)
			return rc;
		param->high_thr_voltage *= adc_properties->full_scale_code;
		param->high_thr_voltage = div64_s64(param->high_thr_voltage,
						QPNP_VADC_HC_VDD_REFERENCE_MV);
	} else {
		qpnp_get_vadc_gain_and_offset(chip, &param1, CALIB_RATIOMETRIC);

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
	}

	return 0;
}
EXPORT_SYMBOL(qpnp_adc_tm_scale_therm_voltage_pu2);

int32_t qpnp_adc_scale_therm_ncp03(struct qpnp_vadc_chip *chip,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int64_t therm_voltage = 0;

	qpnp_adc_scale_with_calib_param(adc_code,
			adc_properties, chan_properties, &therm_voltage);

	qpnp_adc_map_voltage_temp(adcmap_ncp03wf683,
		ARRAY_SIZE(adcmap_ncp03wf683),
		therm_voltage, &adc_chan_result->physical);

	return 0;
}
EXPORT_SYMBOL(qpnp_adc_scale_therm_ncp03);

int32_t qpnp_adc_scale_batt_id(struct qpnp_vadc_chip *chip,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int64_t batt_id_voltage = 0;

	qpnp_adc_scale_with_calib_param(adc_code,
			adc_properties, chan_properties, &batt_id_voltage);

	adc_chan_result->physical = batt_id_voltage;
	adc_chan_result->physical = adc_chan_result->measurement;

	return 0;
}
EXPORT_SYMBOL(qpnp_adc_scale_batt_id);

int32_t qpnp_adc_scale_default(struct qpnp_vadc_chip *vadc,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int64_t scale_voltage = 0;

	if (!chan_properties || !chan_properties->offset_gain_numerator ||
		!chan_properties->offset_gain_denominator || !adc_properties
		|| !adc_chan_result)
		return -EINVAL;

	if (adc_properties->adc_hc) {
		/* (ADC code * vref_vadc (1.875V)) / full_scale_code */
		if (adc_code > QPNP_VADC_HC_MAX_CODE)
			adc_code = 0;
		scale_voltage = (int64_t) adc_code;
		scale_voltage *= (adc_properties->adc_vdd_reference * 1000);
		scale_voltage = div64_s64(scale_voltage,
				adc_properties->full_scale_code);
	} else {
		qpnp_adc_scale_with_calib_param(adc_code, adc_properties,
					chan_properties, &scale_voltage);
		if (!(chan_properties->calib_type == CALIB_ABSOLUTE))
			scale_voltage *= 1000;
	}


	scale_voltage *= chan_properties->offset_gain_denominator;
	scale_voltage = div64_s64(scale_voltage,
				chan_properties->offset_gain_numerator);
	adc_chan_result->measurement = scale_voltage;
	/*
	 * Note: adc_chan_result->measurement is in the unit of
	 * adc_properties.adc_reference. For generic channel processing,
	 * channel measurement is a scale/ratio relative to the adc
	 * reference input
	 */
	adc_chan_result->physical = adc_chan_result->measurement;

	return 0;
}
EXPORT_SYMBOL(qpnp_adc_scale_default);

int32_t qpnp_iadc_scale_default(struct qpnp_vadc_chip *vadc,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int64_t scale_current = 0;

	if (!chan_properties || !chan_properties->offset_gain_numerator ||
		!chan_properties->offset_gain_denominator || !adc_properties
		|| !adc_chan_result)
		return -EINVAL;

	if (adc_properties->adc_hc) {

		if (adc_code == QPNP_IADC_INV)
			return -EINVAL;

		scale_current = (int64_t) adc_code;

		if (adc_code > QPNP_IADC_INV) {
		scale_current = ((~scale_current) & IADC_SCALE_1);
		scale_current++;
		scale_current = -scale_current;
		}
	}

	scale_current *= IADC_SCALE_2;
	scale_current = div64_s64(scale_current,
				1000);
	scale_current *= chan_properties->offset_gain_denominator;
	scale_current = div64_s64(scale_current,
				chan_properties->offset_gain_numerator);
	adc_chan_result->measurement = scale_current;
	/*
	 * Note: adc_chan_result->measurement is in uA.
	 */
	adc_chan_result->physical = adc_chan_result->measurement;

	return 0;
}
EXPORT_SYMBOL(qpnp_iadc_scale_default);

int qpnp_adc_scale_usbin_curr(struct qpnp_vadc_chip *vadc,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int rc = 0;

	rc = qpnp_adc_scale_default(vadc, adc_code, adc_properties,
			chan_properties, adc_chan_result);
	if (rc < 0)
		return rc;

	pr_debug("raw_code:%x, v_adc:%lld\n", adc_code,
						adc_chan_result->physical);
	adc_chan_result->physical = (int64_t) ((USBIN_I_SCALE) *
					adc_chan_result->physical);
	adc_chan_result->physical = div64_s64(adc_chan_result->physical,
								10);

	return 0;
}
EXPORT_SYMBOL(qpnp_adc_scale_usbin_curr);

int32_t qpnp_adc_usb_scaler(struct qpnp_vadc_chip *chip,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold)
{
	struct qpnp_vadc_linear_graph usb_param;

	qpnp_get_vadc_gain_and_offset(chip, &usb_param, CALIB_RATIOMETRIC);

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

int32_t qpnp_adc_absolute_rthr(struct qpnp_vadc_chip *chip,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold)
{
	struct qpnp_vadc_linear_graph vbatt_param;
	int rc = 0, sign = 0;
	int64_t low_thr = 0, high_thr = 0;

	if (param->adc_tm_hc) {
		low_thr = (param->low_thr/param->gain_den);
		low_thr *= param->gain_num;
		low_thr *= param->full_scale_code;
		low_thr = div64_s64(low_thr,
				(QPNP_VADC_HC_VDD_REFERENCE_MV * 1000));
		*low_threshold = low_thr;

		high_thr = (param->high_thr/param->gain_den);
		high_thr *= param->gain_num;
		high_thr *= param->full_scale_code;
		high_thr = div64_s64(high_thr,
				(QPNP_VADC_HC_VDD_REFERENCE_MV * 1000));
		*high_threshold = high_thr;
	} else {
		rc = qpnp_get_vadc_gain_and_offset(chip, &vbatt_param,
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
	}

	pr_debug("high_volt:%d, low_volt:%d\n", param->high_thr,
				param->low_thr);
	pr_debug("adc_code_high:%x, adc_code_low:%x\n", *high_threshold,
				*low_threshold);
	return 0;
}
EXPORT_SYMBOL(qpnp_adc_absolute_rthr);

int32_t qpnp_adc_vbatt_rscaler(struct qpnp_vadc_chip *chip,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold)
{
	return qpnp_adc_absolute_rthr(chip, param, low_threshold,
							high_threshold);
}
EXPORT_SYMBOL(qpnp_adc_vbatt_rscaler);

int32_t qpnp_vadc_absolute_rthr(struct qpnp_vadc_chip *chip,
		const struct qpnp_vadc_chan_properties *chan_prop,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold)
{
	struct qpnp_vadc_linear_graph vbatt_param;
	int rc = 0, sign = 0;
	int64_t low_thr = 0, high_thr = 0;

	if (!chan_prop || !chan_prop->offset_gain_numerator ||
		!chan_prop->offset_gain_denominator)
		return -EINVAL;

	rc = qpnp_get_vadc_gain_and_offset(chip, &vbatt_param, CALIB_ABSOLUTE);
	if (rc < 0)
		return rc;

	low_thr = (((param->low_thr)/(int)chan_prop->offset_gain_denominator
					- QPNP_ADC_625_UV) * vbatt_param.dy);
	if (low_thr < 0) {
		sign = 1;
		low_thr = -low_thr;
	}
	low_thr = low_thr * chan_prop->offset_gain_numerator;
	low_thr = div64_s64(low_thr, QPNP_ADC_625_UV);
	if (sign)
		low_thr = -low_thr;
	*low_threshold = low_thr + vbatt_param.adc_gnd;

	sign = 0;
	high_thr = (((param->high_thr)/(int)chan_prop->offset_gain_denominator
					- QPNP_ADC_625_UV) * vbatt_param.dy);
	if (high_thr < 0) {
		sign = 1;
		high_thr = -high_thr;
	}
	high_thr = high_thr * chan_prop->offset_gain_numerator;
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
EXPORT_SYMBOL(qpnp_vadc_absolute_rthr);

int32_t qpnp_adc_btm_scaler(struct qpnp_vadc_chip *chip,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold)
{
	struct qpnp_vadc_linear_graph btm_param;
	int64_t low_output = 0, high_output = 0;
	int rc = 0;

	if (param->adc_tm_hc) {
		pr_err("Update scaling for VADC_TM_HC\n");
		return -EINVAL;
	}

	qpnp_get_vadc_gain_and_offset(chip, &btm_param, CALIB_RATIOMETRIC);

	pr_debug("warm_temp:%d and cool_temp:%d\n", param->high_temp,
				param->low_temp);
	rc = qpnp_adc_map_voltage_temp(
		adcmap_btm_threshold,
		ARRAY_SIZE(adcmap_btm_threshold),
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
		adcmap_btm_threshold,
		ARRAY_SIZE(adcmap_btm_threshold),
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
EXPORT_SYMBOL(qpnp_adc_btm_scaler);

int32_t qpnp_adc_qrd_skuh_btm_scaler(struct qpnp_vadc_chip *chip,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold)
{
	struct qpnp_vadc_linear_graph btm_param;
	int64_t low_output = 0, high_output = 0;
	int rc = 0;

	if (param->adc_tm_hc) {
		pr_err("Update scaling for VADC_TM_HC\n");
		return -EINVAL;
	}

	qpnp_get_vadc_gain_and_offset(chip, &btm_param, CALIB_RATIOMETRIC);

	pr_debug("warm_temp:%d and cool_temp:%d\n", param->high_temp,
				param->low_temp);
	rc = qpnp_adc_map_voltage_temp(
		adcmap_qrd_skuh_btm_threshold,
		ARRAY_SIZE(adcmap_qrd_skuh_btm_threshold),
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
		adcmap_qrd_skuh_btm_threshold,
		ARRAY_SIZE(adcmap_qrd_skuh_btm_threshold),
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
EXPORT_SYMBOL(qpnp_adc_qrd_skuh_btm_scaler);

int32_t qpnp_adc_qrd_skut1_btm_scaler(struct qpnp_vadc_chip *chip,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold)
{
	struct qpnp_vadc_linear_graph btm_param;
	int64_t low_output = 0, high_output = 0;
	int rc = 0;

	if (param->adc_tm_hc) {
		pr_err("Update scaling for VADC_TM_HC\n");
		return -EINVAL;
	}

	qpnp_get_vadc_gain_and_offset(chip, &btm_param, CALIB_RATIOMETRIC);

	pr_debug("warm_temp:%d and cool_temp:%d\n", param->high_temp,
				param->low_temp);
	rc = qpnp_adc_map_voltage_temp(
		adcmap_qrd_skut1_btm_threshold,
		ARRAY_SIZE(adcmap_qrd_skut1_btm_threshold),
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
		adcmap_qrd_skut1_btm_threshold,
		ARRAY_SIZE(adcmap_qrd_skut1_btm_threshold),
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
EXPORT_SYMBOL(qpnp_adc_qrd_skut1_btm_scaler);

int32_t qpnp_adc_qrd_215_btm_scaler(struct qpnp_vadc_chip *chip,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold)
{
	struct qpnp_vadc_linear_graph btm_param;
	int64_t low_output = 0, high_output = 0;
	int rc = 0;

	if (param->adc_tm_hc) {
		pr_debug("Update scaling for VADC_TM_HC\n");
		return -EINVAL;
	}

	qpnp_get_vadc_gain_and_offset(chip, &btm_param, CALIB_RATIOMETRIC);

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

int32_t qpnp_adc_smb_btm_rscaler(struct qpnp_vadc_chip *chip,
		struct qpnp_adc_tm_btm_param *param,
		uint32_t *low_threshold, uint32_t *high_threshold)
{
	struct qpnp_vadc_linear_graph btm_param;
	int64_t low_output = 0, high_output = 0;
	int rc = 0;

	if (param->adc_tm_hc) {
		pr_err("Update scaling for VADC_TM_HC\n");
		return -EINVAL;
	}

	qpnp_get_vadc_gain_and_offset(chip, &btm_param, CALIB_RATIOMETRIC);

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

int32_t qpnp_adc_scale_pmi_chg_temp(struct qpnp_vadc_chip *vadc,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int rc = 0;

	rc = qpnp_adc_scale_default(vadc, adc_code, adc_properties,
			chan_properties, adc_chan_result);
	if (rc < 0)
		return rc;

	pr_debug("raw_code:%x, v_adc:%lld\n", adc_code,
						adc_chan_result->physical);
	adc_chan_result->physical = (int64_t) ((PMI_CHG_SCALE_1) *
					(adc_chan_result->physical * 2));
	adc_chan_result->physical = (int64_t) (adc_chan_result->physical +
							PMI_CHG_SCALE_2);
	adc_chan_result->physical = (int64_t) adc_chan_result->physical;
	adc_chan_result->physical = div64_s64(adc_chan_result->physical,
								1000000);

	return 0;
}
EXPORT_SYMBOL(qpnp_adc_scale_pmi_chg_temp);

int32_t qpnp_adc_scale_die_temp_1390(struct qpnp_vadc_chip *chip,
		int32_t adc_code,
		const struct qpnp_adc_properties *adc_properties,
		const struct qpnp_vadc_chan_properties *chan_properties,
		struct qpnp_vadc_result *adc_chan_result)
{
	int rc = 0;

	if (!chan_properties || !chan_properties->offset_gain_numerator ||
		!chan_properties->offset_gain_denominator || !adc_properties
		|| !adc_chan_result)
		return -EINVAL;

	rc = qpnp_adc_scale_default(chip, adc_code, adc_properties,
			chan_properties, adc_chan_result);
	if (rc < 0)
		return rc;

	pr_debug("raw_code:%x, v_adc:%lld\n", adc_code,
						adc_chan_result->physical);
	/* T = (1.49322 – V) / 0.00356 */
	adc_chan_result->physical = 1493220 - adc_chan_result->physical;
	adc_chan_result->physical = div64_s64(adc_chan_result->physical, 356);

	return 0;
}
EXPORT_SYMBOL(qpnp_adc_scale_die_temp_1390);

int32_t qpnp_adc_enable_voltage(struct qpnp_adc_drv *adc)
{
	int rc = 0;

	if (adc->hkadc_ldo) {
		rc = regulator_enable(adc->hkadc_ldo);
		if (rc < 0) {
			pr_err("Failed to enable hkadc ldo\n");
			return rc;
		}
	}

	if (adc->hkadc_ldo_ok) {
		rc = regulator_enable(adc->hkadc_ldo_ok);
		if (rc < 0) {
			pr_err("Failed to enable hkadc ok signal\n");
			return rc;
		}
	}

	return rc;
}
EXPORT_SYMBOL(qpnp_adc_enable_voltage);

void qpnp_adc_disable_voltage(struct qpnp_adc_drv *adc)
{
	if (adc->hkadc_ldo)
		regulator_disable(adc->hkadc_ldo);

	if (adc->hkadc_ldo_ok)
		regulator_disable(adc->hkadc_ldo_ok);

}
EXPORT_SYMBOL(qpnp_adc_disable_voltage);

void qpnp_adc_free_voltage_resource(struct qpnp_adc_drv *adc)
{
	if (adc->hkadc_ldo)
		regulator_put(adc->hkadc_ldo);

	if (adc->hkadc_ldo_ok)
		regulator_put(adc->hkadc_ldo_ok);
}
EXPORT_SYMBOL(qpnp_adc_free_voltage_resource);

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
	int decimation_tm_hc = 0, fast_avg_setup_tm_hc = 0, cal_val_hc = 0;
	bool adc_hc;

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
	adc_hc = adc_qpnp->adc_hc;
	adc_prop->adc_hc = adc_hc;

	if (of_device_is_compatible(node, "qcom,qpnp-adc-tm-hc")) {
		rc = of_property_read_u32(node, "qcom,decimation",
						&decimation_tm_hc);
		if (rc) {
			pr_err("Invalid decimation property\n");
			return -EINVAL;
		}

		rc = of_property_read_u32(node,
			"qcom,fast-avg-setup", &fast_avg_setup_tm_hc);
		if (rc) {
			pr_err("Invalid fast average setup with %d\n", rc);
			return -EINVAL;
		}

		if ((fast_avg_setup_tm_hc) > ADC_FAST_AVG_SAMPLE_16) {
			pr_err("Max average support is 2^16\n");
			return -EINVAL;
		}
	}

	if (of_device_is_compatible(node, "qcom,qpnp-adc-hc-pm5") ||
		of_device_is_compatible(node, "qcom,qpnp-adc-tm-hc-pm5"))
		adc_prop->is_pmic_5 = true;
	else
		adc_prop->is_pmic_5 = false;

	for_each_child_of_node(node, child) {
		int channel_num, scaling = 0, post_scaling = 0;
		int fast_avg_setup, calib_type = 0, rc, hw_settle_time = 0;
		const char *calibration_param, *channel_name;

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

		if (!of_device_is_compatible(node, "qcom,qpnp-iadc")) {
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
				"qcom,scale-function", &post_scaling);
			if (rc) {
				pr_err("Invalid channel post scaling property\n");
				return -EINVAL;
			}
			rc = of_property_read_string(child,
				"qcom,calibration-type", &calibration_param);
			if (rc) {
				pr_err("Invalid calibration type\n");
				return -EINVAL;
			}

			if (!strcmp(calibration_param, "absolute")) {
				if (adc_hc)
					calib_type = ADC_HC_ABS_CAL;
				else
					calib_type = CALIB_ABSOLUTE;
			} else if (!strcmp(calibration_param, "ratiometric")) {
				if (adc_hc)
					calib_type = ADC_HC_RATIO_CAL;
				else
					calib_type = CALIB_RATIOMETRIC;
			} else if (!strcmp(calibration_param, "no_cal")) {
				if (adc_hc)
					calib_type = ADC_HC_NO_CAL;
				else {
					pr_err("%s: Invalid calibration property\n",
						__func__);
					return -EINVAL;
				}
			} else {
				pr_err("%s: Invalid calibration property\n",
						__func__);
				return -EINVAL;
			}
		}

		/* ADC_TM_HC fast avg setting is common across channels */
		if (!of_device_is_compatible(node, "qcom,qpnp-adc-tm-hc")) {
			rc = of_property_read_u32(child,
				"qcom,fast-avg-setup", &fast_avg_setup);
			if (rc) {
				pr_err("Invalid channel fast average setup\n");
				return -EINVAL;
			}
		} else {
			fast_avg_setup = fast_avg_setup_tm_hc;
		}

		/* ADC_TM_HC decimation setting is common across channels */
		if (!of_device_is_compatible(node, "qcom,qpnp-adc-tm-hc")) {
			rc = of_property_read_u32(child,
				"qcom,decimation", &decimation);
			if (rc) {
				pr_err("Invalid decimation\n");
				return -EINVAL;
			}
		} else {
			decimation = decimation_tm_hc;
		}

		if (of_device_is_compatible(node, "qcom,qpnp-vadc-hc")) {
			rc = of_property_read_u32(child, "qcom,cal-val",
							&cal_val_hc);
			if (rc) {
				pr_debug("Use calibration value from timer\n");
				adc_channel_list[i].cal_val = ADC_TIMER_CAL;
			} else {
				adc_channel_list[i].cal_val = cal_val_hc;
			}
		}

		/* Individual channel properties */
		adc_channel_list[i].name = (char *)channel_name;
		adc_channel_list[i].channel_num = channel_num;
		adc_channel_list[i].adc_decimation = decimation;
		adc_channel_list[i].fast_avg_setup = fast_avg_setup;
		if (!of_device_is_compatible(node, "qcom,qpnp-iadc")) {
			adc_channel_list[i].chan_path_prescaling = scaling;
			adc_channel_list[i].adc_scale_fn = post_scaling;
			adc_channel_list[i].hw_settle_time = hw_settle_time;
			adc_channel_list[i].calib_type = calib_type;
		}
		i++;
	}

	/* Get the ADC VDD reference voltage and ADC bit resolution */
	rc = of_property_read_u32(node, "qcom,adc-vdd-reference",
			&adc_prop->adc_vdd_reference);
	if (rc) {
		pr_err("Invalid adc vdd reference property\n");
		return -EINVAL;
	}
	rc = of_property_read_u32(node, "qcom,adc-full-scale-code",
			&adc_prop->full_scale_code);
	if (rc) {
		pr_debug("Use default value of 0x4000 for full scale\n");
		adc_prop->full_scale_code = QPNP_VADC_HC_VREF_CODE;
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

	adc_qpnp->slave = to_spmi_device(pdev->dev.parent)->usid;
	adc_qpnp->offset = base;

	/* Register the ADC peripheral interrupt */
	adc_qpnp->adc_irq_eoc = platform_get_irq_byname(pdev,
							"eoc-int-en-set");
	if (adc_qpnp->adc_irq_eoc < 0) {
		pr_err("Invalid irq\n");
		return -ENXIO;
	}

	init_completion(&adc_qpnp->adc_rslt_completion);

	if (of_get_property(node, "hkadc_ldo-supply", NULL)) {
		adc_qpnp->hkadc_ldo = regulator_get(&pdev->dev, "hkadc_ldo");
		if (IS_ERR(adc_qpnp->hkadc_ldo)) {
			pr_err("hkadc_ldo-supply node not found\n");
			return -EINVAL;
		}

		rc = regulator_set_voltage(adc_qpnp->hkadc_ldo,
				QPNP_VADC_LDO_VOLTAGE_MIN,
				QPNP_VADC_LDO_VOLTAGE_MAX);
		if (rc < 0) {
			pr_err("setting voltage for hkadc_ldo failed\n");
			return rc;
		}

		rc = regulator_set_load(adc_qpnp->hkadc_ldo, 100000);
		if (rc < 0) {
			pr_err("hkadc_ldo optimum mode failed%d\n", rc);
			return rc;
		}
	}

	if (of_get_property(node, "hkadc_ok-supply", NULL)) {
		adc_qpnp->hkadc_ldo_ok = regulator_get(&pdev->dev,
				"hkadc_ok");
		if (IS_ERR(adc_qpnp->hkadc_ldo_ok)) {
			pr_err("hkadc_ok node not found\n");
			return -EINVAL;
		}

		rc = regulator_set_voltage(adc_qpnp->hkadc_ldo_ok,
				QPNP_VADC_OK_VOLTAGE_MIN,
				QPNP_VADC_OK_VOLTAGE_MAX);
		if (rc < 0) {
			pr_err("setting voltage for hkadc-ldo-ok failed\n");
			return rc;
		}
	}

	return 0;
}
EXPORT_SYMBOL(qpnp_adc_get_devicetree_data);
