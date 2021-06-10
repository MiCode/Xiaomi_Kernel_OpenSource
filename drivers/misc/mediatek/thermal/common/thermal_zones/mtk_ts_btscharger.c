/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <mt-plat/aee.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mach/mtk_thermal.h"
#include <linux/uidgid.h>
#include <linux/slab.h>
#if (CONFIG_MTK_GAUGE_VERSION == 30)
#include <mt-plat/mtk_charger.h>
#else
#include <charging.h>
#endif
#include <tmp_btscharger.h>
#define mtktscharger_TEMP_CRIT (150000) /* 150.000 degree Celsius */

#define mtktscharger_dprintk(fmt, args...) \
do { \
	if (mtktscharger_debug_log) \
		pr_debug("[Thermal/tzcharger]" fmt, ##args); \
} while (0)

#define mtktscharger_dprintk_always(fmt, args...) \
	pr_debug("[Thermal/tzcharger]" fmt, ##args)

#define mtktscharger_pr_notice(fmt, args...) \
	pr_notice("[Thermal/tzcharger]" fmt, ##args)

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
static DEFINE_SEMAPHORE(sem_mutex);

static int kernelmode;
static unsigned int interval; /* seconds, 0 : no auto polling */
static int num_trip = 1;
static int trip_temp[10] = { 125000, 110000, 100000, 90000, 80000,
				70000, 65000, 60000, 55000, 50000 };

static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static char g_bind0[20] = "mtktscharger-sysrst";
static char g_bind1[20] = "";
static char g_bind2[20] = "";
static char g_bind3[20] = "";
static char g_bind4[20] = "";
static char g_bind5[20] = "";
static char g_bind6[20] = "";
static char g_bind7[20] = "";
static char g_bind8[20] = "";
static char g_bind9[20] = "";
static char *g_bind_a[10] = {&g_bind0[0], &g_bind1[0], &g_bind2[0]
	, &g_bind3[0], &g_bind4[0], &g_bind5[0], &g_bind6[0], &g_bind7[0]
	, &g_bind8[0], &g_bind9[0]};
static struct thermal_zone_device *thz_dev;

static unsigned int cl_dev_sysrst_state;
static struct thermal_cooling_device *cl_dev_sysrst;

static int mtktscharger_debug_log;

/* This is to preserve last temperature readings from charger driver.
 * In case mtk_ts_charger.c fails to read temperature.
 */
/**
 * If curr_temp >= polling_trip_temp1, use interval
 * else if cur_temp >= polling_trip_temp2 && curr_temp < polling_trip_temp1,
 *	use interval*polling_factor1
 * else, use interval*polling_factor2
 */
static int polling_trip_temp1 = 40000;
static int polling_trip_temp2 = 20000;
static int polling_factor1 = 5000;
static int polling_factor2 = 10000;

#if (CONFIG_MTK_GAUGE_VERSION == 30)
static struct charger_consumer *pthermal_consumer;
#endif

struct BTSCHARGER_TEMPERATURE {
	__s32 BTSCHARGER_Temp;
	__s32 TemperatureR;
};

static int g_RAP_pull_up_R = BTSCHARGER_RAP_PULL_UP_R;
static int g_TAP_over_critical_low = BTSCHARGER_TAP_OVER_CRITICAL_LOW;
static int g_RAP_pull_up_voltage = BTSCHARGER_RAP_PULL_UP_VOLTAGE;
static int g_RAP_ntc_table = BTSCHARGER_RAP_NTC_TABLE;
static int g_RAP_ADC_channel = BTSCHARGER_RAP_ADC_CHANNEL;

static int g_btscharger_TemperatureR;
/* struct BTSCHARGER_TEMPERATURE BTSCHARGER_Temperature_Table[] = {0}; */

static struct BTSCHARGER_TEMPERATURE *BTSCHARGER_Temperature_Table;
static int ntc_tbl_size;

/* AP_NTC_BL197 */
static struct BTSCHARGER_TEMPERATURE BTSCHARGER_Temperature_Table1[] = {
	{-40, 74354},		/* FIX_ME */
	{-35, 74354},		/* FIX_ME */
	{-30, 74354},		/* FIX_ME */
	{-25, 74354},		/* FIX_ME */
	{-20, 74354},
	{-15, 57626},
	{-10, 45068},
	{-5, 35548},
	{0, 28267},
	{5, 22650},
	{10, 18280},
	{15, 14855},
	{20, 12151},
	{25, 10000},		/* 10K */
	{30, 8279},
	{35, 6892},
	{40, 5768},
	{45, 4852},
	{50, 4101},
	{55, 3483},
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970}		/* FIX_ME */
};

/* AP_NTC_TSM_1 */
static struct BTSCHARGER_TEMPERATURE BTSCHARGER_Temperature_Table2[] = {
	{-40, 70603},		/* FIX_ME */
	{-35, 70603},		/* FIX_ME */
	{-30, 70603},		/* FIX_ME */
	{-25, 70603},		/* FIX_ME */
	{-20, 70603},
	{-15, 55183},
	{-10, 43499},
	{-5, 34569},
	{0, 27680},
	{5, 22316},
	{10, 18104},
	{15, 14773},
	{20, 12122},
	{25, 10000},		/* 10K */
	{30, 8294},
	{35, 6915},
	{40, 5795},
	{45, 4882},
	{50, 4133},
	{55, 3516},
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004},		/* FIX_ME */
	{60, 3004}		/* FIX_ME */
};

/* AP_NTC_10_SEN_1 */
static struct BTSCHARGER_TEMPERATURE BTSCHARGER_Temperature_Table3[] = {
	{-40, 74354},		/* FIX_ME */
	{-35, 74354},		/* FIX_ME */
	{-30, 74354},		/* FIX_ME */
	{-25, 74354},		/* FIX_ME */
	{-20, 74354},
	{-15, 57626},
	{-10, 45068},
	{-5, 35548},
	{0, 28267},
	{5, 22650},
	{10, 18280},
	{15, 14855},
	{20, 12151},
	{25, 10000},		/* 10K */
	{30, 8279},
	{35, 6892},
	{40, 5768},
	{45, 4852},
	{50, 4101},
	{55, 3483},
	{60, 2970},
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970},		/* FIX_ME */
	{60, 2970}		/* FIX_ME */
};

/* AP_NTC_10(TSM0A103F34D1RZ) */
static struct BTSCHARGER_TEMPERATURE BTSCHARGER_Temperature_Table4[] = {
	{-40, 188500},
	{-35, 144290},
	{-30, 111330},
	{-25, 86560},
	{-20, 67790},
	{-15, 53460},
	{-10, 42450},
	{-5, 33930},
	{0, 27280},
	{5, 22070},
	{10, 17960},
	{15, 14700},
	{20, 12090},
	{25, 10000},		/* 10K */
	{30, 8310},
	{35, 6940},
	{40, 5830},
	{45, 4910},
	{50, 4160},
	{55, 3540},
	{60, 3020},
	{65, 2590},
	{70, 2230},
	{75, 1920},
	{80, 1670},
	{85, 1450},
	{90, 1270},
	{95, 1110},
	{100, 975},
	{105, 860},
	{110, 760},
	{115, 674},
	{120, 599},
	{125, 534}
};

/* AP_NTC_47 */
static struct BTSCHARGER_TEMPERATURE BTSCHARGER_Temperature_Table5[] = {
	{-40, 483954},		/* FIX_ME */
	{-35, 483954},		/* FIX_ME */
	{-30, 483954},		/* FIX_ME */
	{-25, 483954},		/* FIX_ME */
	{-20, 483954},
	{-15, 360850},
	{-10, 271697},
	{-5, 206463},
	{0, 158214},
	{5, 122259},
	{10, 95227},
	{15, 74730},
	{20, 59065},
	{25, 47000},		/* 47K */
	{30, 37643},
	{35, 30334},
	{40, 24591},
	{45, 20048},
	{50, 16433},
	{55, 13539},
	{60, 11210},
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210},		/* FIX_ME */
	{60, 11210}		/* FIX_ME */
};


/* NTCG104EF104F(100K) */
static struct BTSCHARGER_TEMPERATURE BTSCHARGER_Temperature_Table6[] = {
	{-40, 4251000},
	{-35, 3005000},
	{-30, 2149000},
	{-25, 1554000},
	{-20, 1135000},
	{-15, 837800},
	{-10, 624100},
	{-5, 469100},
	{0, 355600},
	{5, 271800},
	{10, 209400},
	{15, 162500},
	{20, 127000},
	{25, 100000},		/* 100K */
	{30, 79230},
	{35, 63180},
	{40, 50680},
	{45, 40900},
	{50, 33190},
	{55, 27090},
	{60, 22220},
	{65, 18320},
	{70, 15180},
	{75, 12640},
	{80, 10580},
	{85, 8887},
	{90, 7500},
	{95, 6357},
	{100, 5410},
	{105, 4623},
	{110, 3965},
	{115, 3415},
	{120, 2951},
	{125, 2560}
};

/* NCP15WF104F03RC(100K) */
static struct BTSCHARGER_TEMPERATURE BTSCHARGER_Temperature_Table7[] = {
	{-40, 4397119},
	{-35, 3088599},
	{-30, 2197225},
	{-25, 1581881},
	{-20, 1151037},
	{-15, 846579},
	{-10, 628988},
	{-5, 471632},
	{0, 357012},
	{5, 272500},
	{10, 209710},
	{15, 162651},
	{20, 127080},
	{25, 100000},		/* 100K */
	{30, 79222},
	{35, 63167},
#if defined(APPLY_PRECISE_NTC_TABLE)
	{40, 50677},
	{41, 48528},
	{42, 46482},
	{43, 44533},
	{44, 42675},
	{45, 40904},
	{46, 39213},
	{47, 37601},
	{48, 36063},
	{49, 34595},
	{50, 33195},
	{51, 31859},
	{52, 30584},
	{53, 29366},
	{54, 28203},
	{55, 27091},
	{56, 26028},
	{57, 25013},
	{58, 24042},
	{59, 23113},
	{60, 22224},
	{61, 21374},
	{62, 20560},
	{63, 19782},
	{64, 19036},
	{65, 18322},
	{66, 17640},
	{67, 16986},
	{68, 16360},
	{69, 15759},
	{70, 15184},
	{71, 14631},
	{72, 14100},
	{73, 13591},
	{74, 13103},
	{75, 12635},
	{76, 12187},
	{77, 11756},
	{78, 11343},
	{79, 10946},
	{80, 10565},
	{81, 10199},
	{82,  9847},
	{83,  9509},
	{84,  9184},
	{85,  8872},
	{86,  8572},
	{87,  8283},
	{88,  8005},
	{89,  7738},
	{90,  7481},
#else
	{40, 50677},
	{45, 40904},
	{50, 33195},
	{55, 27091},
	{60, 22224},
	{65, 18323},
	{70, 15184},
	{75, 12635},
	{80, 10566},
	{85, 8873},
	{90, 7481},
#endif
	{95, 6337},
	{100, 5384},
	{105, 4594},
	{110, 3934},
	{115, 3380},
	{120, 2916},
	{125, 2522}
};


/* convert register to temperature  */
static __s16 mtkts_btscharger_thermistor_conver_temp(__s32 Res)
{
	int i = 0;
	int asize = 0;
	__s32 RES1 = 0, RES2 = 0;
	__s32 TAP_Value = -200, TMP1 = 0, TMP2 = 0;

	asize = (ntc_tbl_size / sizeof(struct BTSCHARGER_TEMPERATURE));
	/* mtktscharger_dprintk("btscharger() :
	 * asize = %d, Res = %d\n",asize,Res);
	 */
	if (Res >= BTSCHARGER_Temperature_Table[0].TemperatureR) {
		TAP_Value = -40;	/* min */
	} else if (Res <=
		BTSCHARGER_Temperature_Table[asize - 1].TemperatureR) {
		TAP_Value = 125;	/* max */
	} else {
		RES1 = BTSCHARGER_Temperature_Table[0].TemperatureR;
		TMP1 = BTSCHARGER_Temperature_Table[0].BTSCHARGER_Temp;
		/* mtktscharger_dprintk("%d : RES1 = %d,TMP1 = %d\n",__LINE__,
		 * RES1,TMP1);
		 */

		for (i = 0; i < asize; i++) {
			if (Res >=
				BTSCHARGER_Temperature_Table[i].TemperatureR) {
				RES2 =
				BTSCHARGER_Temperature_Table[i].TemperatureR;
				TMP2 =
				BTSCHARGER_Temperature_Table[i].BTSCHARGER_Temp;
				/* mtktscharger_dprintk("%d :i=%d, RES2 = %d,
				 * TMP2 = %d\n",__LINE__,i,RES2,TMP2);
				 */
				break;
			}
			RES1 = BTSCHARGER_Temperature_Table[i].TemperatureR;
			TMP1 = BTSCHARGER_Temperature_Table[i].BTSCHARGER_Temp;
			/* mtktscharger_dprintk("%d :i=%d, RES1 = %d,
			 * TMP1 = %d\n",__LINE__,i,RES1,TMP1);
			 */
		}

		TAP_Value = (((Res - RES2) * TMP1) + ((RES1 - Res) * TMP2))
								/ (RES1 - RES2);
	}


	return TAP_Value;
}

/* convert ADC_AP_temp_volt to register */
/*Volt to Temp formula same with 6589*/
static __s16 mtk_ts_btscharger_volt_to_temp(__u32 dwVolt)
{
	__s32 TRes;
	__u64 dwVCriAP = 0;
	__u64 dwVCriAP2 = 0;
	__s32 BTSCHARGER_TMP = -100;

	/* SW workaround-----------------------------------------------------
	 * dwVCriAP = (TAP_OVER_CRITICAL_LOW * 1800) /
	 * (TAP_OVER_CRITICAL_LOW + 39000);
	 * dwVCriAP = (TAP_OVER_CRITICAL_LOW * RAP_PULL_UP_VOLT) /
	 * (TAP_OVER_CRITICAL_LOW + RAP_PULL_UP_R);
	 */

	dwVCriAP = ((__u64)g_TAP_over_critical_low *
		(__u64)g_RAP_pull_up_voltage);
	dwVCriAP2 = (g_TAP_over_critical_low + g_RAP_pull_up_R);
	do_div(dwVCriAP, dwVCriAP2);


	if (dwVolt > ((__u32)dwVCriAP)) {
		TRes = g_TAP_over_critical_low;
	} else {
		/* TRes = (39000*dwVolt) / (1800-dwVolt);
		 * TRes = (RAP_PULL_UP_R*dwVolt) / (RAP_PULL_UP_VOLT-dwVolt);
		 */
		TRes = (g_RAP_pull_up_R * dwVolt)
				/ (g_RAP_pull_up_voltage - dwVolt);
	}
	/* ------------------------------------------------------------------ */

	g_btscharger_TemperatureR = TRes;

	/* convert register to temperature */
	BTSCHARGER_TMP = mtkts_btscharger_thermistor_conver_temp(TRes);

	return BTSCHARGER_TMP;
}

static int mtktscharger_get_hw_temp(void)
{

	int ret = 0, data[4], i, ret_value = 0, ret_temp = 0, output;
	int times = 1, Channel = g_RAP_ADC_channel; /* 6752=0(AUX_IN2_NTC) */
	static int valid_temp;
#if defined(APPLY_AUXADC_CALI_DATA)
	int auxadc_cali_temp;
#endif

	if (IMM_IsAdcInitReady() == 0) {
		mtktscharger_dprintk(
			"[thermal_auxadc_get_data]: AUXADC is not ready\n");
		return 0;
	}

	i = times;
	while (i--) {
		ret_value = IMM_GetOneChannelValue(Channel, data, &ret_temp);
		if (ret_value) {/* AUXADC is busy */
#if defined(APPLY_AUXADC_CALI_DATA)
			auxadc_cali_temp = valid_temp;
#else
			ret_temp = valid_temp;
#endif
		} else {
#if defined(APPLY_AUXADC_CALI_DATA)
			/*
			 * by reference mtk_auxadc.c
			 *
			 * convert to volt:
			 *      data[0] = (rawdata * 1500 / (4096 + cali_ge)) /
			 *                 1000;
			 *
			 * convert to mv, need multiply 10:
			 *      data[1] = (rawdata * 150 / (4096 + cali_ge)) %
			 *                 100;
			 *
			 * provide high precision mv:
			 *      data[2] = (rawdata * 1500 / (4096 + cali_ge)) %
			 *                 1000;
			 */
			auxadc_cali_temp = data[0]*1000+data[2];
			valid_temp = auxadc_cali_temp;
#else
			valid_temp = ret_temp;
#endif
		}

#if defined(APPLY_AUXADC_CALI_DATA)
		ret += auxadc_cali_temp;
		mtktscharger_dprintk(
			"[thermal_auxadc_get_data(AUX_IN2_NTC)]: ret_temp=%d\n",
			auxadc_cali_temp);
#else
		ret += ret_temp;
		mtktscharger_dprintk(
			"[thermal_auxadc_get_data(AUX_IN2_NTC)]: ret_temp=%d\n",
			ret_temp);
#endif
	}

	/* Mt_auxadc_hal.c */
	/* #define VOLTAGE_FULL_RANGE  1500 // VA voltage */
	/* #define AUXADC_PRECISE      4096 // 12 bits */
#if defined(APPLY_AUXADC_CALI_DATA)
#else
	ret = ret * 1500 / 4096;
#endif
	/* ret = ret*1800/4096;//82's ADC power */
	mtktscharger_dprintk("APtery output mV = %d\n", ret);
	output = mtk_ts_btscharger_volt_to_temp(ret);
	mtktscharger_dprintk("BTSCHARGER output temperature = %d\n", output);
	return output;
}

static int mtktscharger_get_temp(struct thermal_zone_device *thermal, int *t)
{
	*t = mtktscharger_get_hw_temp() * 1000;
	mtktscharger_dprintk("%s %d\n", __func__, *t);

	if (*t >= 85000)
		mtktscharger_dprintk_always("HT %d\n", *t);

	if ((int)*t >= polling_trip_temp1)
		thermal->polling_delay = interval * 1000;
	else if ((int)*t < polling_trip_temp2)
		thermal->polling_delay = interval * polling_factor2;
	else
		thermal->polling_delay = interval * polling_factor1;

	return 0;
}

static int mtktscharger_bind(
struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0))
		table_val = 0;
	else if (!strcmp(cdev->type, g_bind1))
		table_val = 1;
	else if (!strcmp(cdev->type, g_bind2))
		table_val = 2;
	else if (!strcmp(cdev->type, g_bind3))
		table_val = 3;
	else if (!strcmp(cdev->type, g_bind4))
		table_val = 4;
	else if (!strcmp(cdev->type, g_bind5))
		table_val = 5;
	else if (!strcmp(cdev->type, g_bind6))
		table_val = 6;
	else if (!strcmp(cdev->type, g_bind7))
		table_val = 7;
	else if (!strcmp(cdev->type, g_bind8))
		table_val = 8;
	else if (!strcmp(cdev->type, g_bind9))
		table_val = 9;
	else
		return 0;

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		mtktscharger_dprintk("%s error binding %s\n", __func__,
								cdev->type);
		return -EINVAL;
	}

	mtktscharger_dprintk("%s binding %s at %d\n", __func__, cdev->type,
								table_val);
	return 0;
}

static int mtktscharger_unbind(struct thermal_zone_device *thermal,
			    struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0))
		table_val = 0;
	else if (!strcmp(cdev->type, g_bind1))
		table_val = 1;
	else if (!strcmp(cdev->type, g_bind2))
		table_val = 2;
	else if (!strcmp(cdev->type, g_bind3))
		table_val = 3;
	else if (!strcmp(cdev->type, g_bind4))
		table_val = 4;
	else if (!strcmp(cdev->type, g_bind5))
		table_val = 5;
	else if (!strcmp(cdev->type, g_bind6))
		table_val = 6;
	else if (!strcmp(cdev->type, g_bind7))
		table_val = 7;
	else if (!strcmp(cdev->type, g_bind8))
		table_val = 8;
	else if (!strcmp(cdev->type, g_bind9))
		table_val = 9;
	else
		return 0;

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		mtktscharger_dprintk("%s error unbinding %s\n", __func__,
								cdev->type);
		return -EINVAL;
	}

	mtktscharger_dprintk("%s unbinding OK\n", __func__);
	return 0;
}

static int mtktscharger_get_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int mtktscharger_set_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int mtktscharger_get_trip_type(
struct thermal_zone_device *thermal, int trip, enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtktscharger_get_trip_temp(
struct thermal_zone_device *thermal, int trip, int *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtktscharger_get_crit_temp(
struct thermal_zone_device *thermal, int *temperature)
{
	*temperature = mtktscharger_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtktscharger_dev_ops = {
	.bind = mtktscharger_bind,
	.unbind = mtktscharger_unbind,
	.get_temp = mtktscharger_get_temp,
	.get_mode = mtktscharger_get_mode,
	.set_mode = mtktscharger_set_mode,
	.get_trip_type = mtktscharger_get_trip_type,
	.get_trip_temp = mtktscharger_get_trip_temp,
	.get_crit_temp = mtktscharger_get_crit_temp,
};

static int mtktscharger_register_thermal(void)
{
	mtktscharger_dprintk("%s\n", __func__);

	/* trips : trip 0~2 */
	thz_dev = mtk_thermal_zone_device_register("mtktscharger", num_trip,
					NULL, /* name: mtktscharger ??? */
					&mtktscharger_dev_ops, 0, 0, 0,
					interval * 1000);

	return 0;
}

static void mtktscharger_unregister_thermal(void)
{
	mtktscharger_dprintk("%s\n", __func__);

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static int mtktscharger_sysrst_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	return 0;
}

static int mtktscharger_sysrst_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = cl_dev_sysrst_state;
	return 0;
}

static int mtktscharger_sysrst_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	cl_dev_sysrst_state = state;
	if (cl_dev_sysrst_state == 1) {
		pr_notice("[Thermal/mtktscharger_sysrst] reset, reset, reset!!!\n");
		pr_notice("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
		pr_notice("*****************************************\n");
		pr_notice("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");

		/* To trigger data abort to reset the system
		 * for thermal protection.
		 */
		BUG();
	}

	return 0;
}

static struct thermal_cooling_device_ops mtktscharger_cooling_sysrst_ops = {
	.get_max_state = mtktscharger_sysrst_get_max_state,
	.get_cur_state = mtktscharger_sysrst_get_cur_state,
	.set_cur_state = mtktscharger_sysrst_set_cur_state,
};

int mtktscharger_register_cooler(void)
{
	cl_dev_sysrst = mtk_thermal_cooling_device_register(
				"mtktscharger-sysrst", NULL,
					&mtktscharger_cooling_sysrst_ops);
	return 0;
}

void mtktscharger_unregister_cooler(void)
{
	if (cl_dev_sysrst) {
		mtk_thermal_cooling_device_unregister(cl_dev_sysrst);
		cl_dev_sysrst = NULL;
	}
}
void mtkts_btscharger_prepare_table(int table_num)
{

	switch (table_num) {
	case 1:		/* AP_NTC_BL197 */
		BTSCHARGER_Temperature_Table = BTSCHARGER_Temperature_Table1;
		ntc_tbl_size = sizeof(BTSCHARGER_Temperature_Table1);
		break;
	case 2:		/* AP_NTC_TSM_1 */
		BTSCHARGER_Temperature_Table = BTSCHARGER_Temperature_Table2;
		ntc_tbl_size = sizeof(BTSCHARGER_Temperature_Table2);
		break;
	case 3:		/* AP_NTC_10_SEN_1 */
		BTSCHARGER_Temperature_Table = BTSCHARGER_Temperature_Table3;
		ntc_tbl_size = sizeof(BTSCHARGER_Temperature_Table3);
		break;
	case 4:		/* AP_NTC_10 */
		BTSCHARGER_Temperature_Table = BTSCHARGER_Temperature_Table4;
		ntc_tbl_size = sizeof(BTSCHARGER_Temperature_Table4);
		break;
	case 5:		/* AP_NTC_47 */
		BTSCHARGER_Temperature_Table = BTSCHARGER_Temperature_Table5;
		ntc_tbl_size = sizeof(BTSCHARGER_Temperature_Table5);
		break;
	case 6:		/* NTCG104EF104F */
		BTSCHARGER_Temperature_Table = BTSCHARGER_Temperature_Table6;
		ntc_tbl_size = sizeof(BTSCHARGER_Temperature_Table6);
		break;
	case 7:		/* NCP15WF104F03RC */
		BTSCHARGER_Temperature_Table = BTSCHARGER_Temperature_Table7;
		ntc_tbl_size = sizeof(BTSCHARGER_Temperature_Table7);
		break;
	default:		/* AP_NTC_10 */
		BTSCHARGER_Temperature_Table = BTSCHARGER_Temperature_Table4;
		ntc_tbl_size = sizeof(BTSCHARGER_Temperature_Table4);
		break;
	}

	pr_notice("[Thermal/TZ/BTSCHARGER] %s table_num=%d\n",
						__func__, table_num);
}
static int mtktscharger_read(struct seq_file *m, void *v)
{
	seq_printf(m, "log=%d\n", mtktscharger_debug_log);
	seq_printf(m, "polling delay=%d\n", interval * 1000);
	seq_printf(m, "no of trips=%d\n", num_trip);
	{
		int i = 0;

		for (; i < 10; i++)
			seq_printf(m, "%02d\t%d\t%d\t%s\n", i, trip_temp[i],
						g_THERMAL_TRIP[i], g_bind_a[i]);
	}

	return 0;
}

static ssize_t mtktscharger_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	int len = 0, i;
	struct mtktscharger_data {
		int trip[10];
		int t_type[10];
		char bind0[20], bind1[20], bind2[20], bind3[20], bind4[20];
		char bind5[20], bind6[20], bind7[20], bind8[20], bind9[20];
		int time_msec;
		char desc[512];
	};

	struct mtktscharger_data *ptr_mtktscharger_data = kmalloc(
				sizeof(*ptr_mtktscharger_data), GFP_KERNEL);

	if (ptr_mtktscharger_data == NULL)
		return -ENOMEM;

	len = (count < (sizeof(ptr_mtktscharger_data->desc) - 1)) ?
			count : (sizeof(ptr_mtktscharger_data->desc) - 1);

	if (copy_from_user(ptr_mtktscharger_data->desc, buffer, len)) {
		kfree(ptr_mtktscharger_data);
		return 0;
	}

	ptr_mtktscharger_data->desc[len] = '\0';

	/* TODO: Add a switch of mtktscharger_debug_log. */

	if (sscanf(ptr_mtktscharger_data->desc,
		"%d %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d",
		&num_trip,
		&ptr_mtktscharger_data->trip[0],
		&ptr_mtktscharger_data->t_type[0], ptr_mtktscharger_data->bind0,
		&ptr_mtktscharger_data->trip[1],
		&ptr_mtktscharger_data->t_type[1], ptr_mtktscharger_data->bind1,
		&ptr_mtktscharger_data->trip[2],
		&ptr_mtktscharger_data->t_type[2], ptr_mtktscharger_data->bind2,
		&ptr_mtktscharger_data->trip[3],
		&ptr_mtktscharger_data->t_type[3], ptr_mtktscharger_data->bind3,
		&ptr_mtktscharger_data->trip[4],
		&ptr_mtktscharger_data->t_type[4], ptr_mtktscharger_data->bind4,
		&ptr_mtktscharger_data->trip[5],
		&ptr_mtktscharger_data->t_type[5], ptr_mtktscharger_data->bind5,
		&ptr_mtktscharger_data->trip[6],
		&ptr_mtktscharger_data->t_type[6], ptr_mtktscharger_data->bind6,
		&ptr_mtktscharger_data->trip[7],
		&ptr_mtktscharger_data->t_type[7], ptr_mtktscharger_data->bind7,
		&ptr_mtktscharger_data->trip[8],
		&ptr_mtktscharger_data->t_type[8], ptr_mtktscharger_data->bind8,
		&ptr_mtktscharger_data->trip[9],
		&ptr_mtktscharger_data->t_type[9], ptr_mtktscharger_data->bind9,
		&ptr_mtktscharger_data->time_msec) == 32) {
		down(&sem_mutex);
		mtktscharger_dprintk("mtktscharger_unregister_thermal\n");
		mtktscharger_unregister_thermal();

		if (num_trip < 0 || num_trip > 10) {
			mtktscharger_dprintk_always("%s bad argument\n",
								__func__);
#ifdef CONFIG_MTK_AEE_FEATURE
			aee_kernel_warning_api(__FILE__, __LINE__,
					DB_OPT_DEFAULT, "%s",
					"Bad argument", __func__);
#endif
			kfree(ptr_mtktscharger_data);
			up(&sem_mutex);
			return -EINVAL;
		}

		for (i = 0; i < num_trip; i++)
			g_THERMAL_TRIP[i] = ptr_mtktscharger_data->t_type[i];

		g_bind0[0] = g_bind1[0] = g_bind2[0] = g_bind3[0]
			= g_bind4[0] = g_bind5[0] = g_bind6[0]
			= g_bind7[0] = g_bind8[0] = g_bind9[0] = '\0';

		for (i = 0; i < 20; i++) {
			g_bind0[i] = ptr_mtktscharger_data->bind0[i];
			g_bind1[i] = ptr_mtktscharger_data->bind1[i];
			g_bind2[i] = ptr_mtktscharger_data->bind2[i];
			g_bind3[i] = ptr_mtktscharger_data->bind3[i];
			g_bind4[i] = ptr_mtktscharger_data->bind4[i];
			g_bind5[i] = ptr_mtktscharger_data->bind5[i];
			g_bind6[i] = ptr_mtktscharger_data->bind6[i];
			g_bind7[i] = ptr_mtktscharger_data->bind7[i];
			g_bind8[i] = ptr_mtktscharger_data->bind8[i];
			g_bind9[i] = ptr_mtktscharger_data->bind9[i];
		}

		mtktscharger_dprintk("%s g_THERMAL_TRIP_0=%d,", __func__,
			g_THERMAL_TRIP[0]);
		mtktscharger_dprintk("g_THERMAL_TRIP_1=%d g_THERMAL_TRIP_2=%d ",
			g_THERMAL_TRIP[1], g_THERMAL_TRIP[2]);
		mtktscharger_dprintk("g_THERMAL_TRIP_3=%d g_THERMAL_TRIP_4=%d ",
			g_THERMAL_TRIP[3], g_THERMAL_TRIP[4]);
		mtktscharger_dprintk("g_THERMAL_TRIP_5=%d g_THERMAL_TRIP_6=%d ",
			g_THERMAL_TRIP[5], g_THERMAL_TRIP[6]);

		mtktscharger_dprintk(
			"g_THERMAL_TRIP_7=%d g_THERMAL_TRIP_8=%d g_THERMAL_TRIP_9=%d\n",
			g_THERMAL_TRIP[7], g_THERMAL_TRIP[8],
			g_THERMAL_TRIP[9]);

		mtktscharger_dprintk("cooldev0=%s cooldev1=%s cooldev2=%s ",
			g_bind0, g_bind1, g_bind2);

		mtktscharger_dprintk("cooldev3=%s cooldev4=%s ",
			g_bind3, g_bind4);

		mtktscharger_dprintk(
			"cooldev5=%s cooldev6=%s cooldev7=%s cooldev8=%s cooldev9=%s\n",
			g_bind5, g_bind6, g_bind7, g_bind8, g_bind9);

		for (i = 0; i < num_trip; i++)
			trip_temp[i] = ptr_mtktscharger_data->trip[i];

		interval = ptr_mtktscharger_data->time_msec / 1000;

		mtktscharger_dprintk("%s trip_0_temp=%d trip_1_temp=%d ",
					__func__, trip_temp[0], trip_temp[1]);

		mtktscharger_dprintk("trip_2_temp=%d trip_3_temp=%d ",
						trip_temp[2], trip_temp[3]);

		mtktscharger_dprintk(
				"trip_4_temp=%d trip_5_temp=%d trip_6_temp=%d ",
				trip_temp[4], trip_temp[5], trip_temp[6]);

		mtktscharger_dprintk("trip_7_temp=%d trip_8_temp=%d ",
						trip_temp[7], trip_temp[8]);

		mtktscharger_dprintk("trip_9_temp=%d time_ms=%d\n",
						trip_temp[9], interval * 1000);

		mtktscharger_dprintk("mtktscharger_register_thermal\n");
		mtktscharger_register_thermal();
		up(&sem_mutex);

		kfree(ptr_mtktscharger_data);
		return count;
	}

	mtktscharger_dprintk("%s bad argument\n", __func__);
	kfree(ptr_mtktscharger_data);

	return -EINVAL;
}

static int mtktscharger_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtktscharger_read, NULL);
}

static const struct file_operations mtktscharger_fops = {
	.owner = THIS_MODULE,
	.open = mtktscharger_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtktscharger_write,
	.release = single_release,
};

#if (CONFIG_MTK_GAUGE_VERSION == 30)
static int mtktscharger_pdrv_probe(struct platform_device *pdev)
{
	int err = 0;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtktscharger_dir = NULL;

	mtktscharger_dprintk_always("%s\n", __func__);

	pthermal_consumer = charger_manager_get_by_name(&pdev->dev, "charger");

	if (!pthermal_consumer) {
		mtktscharger_pr_notice("%s get get_by_name fails.\n", __func__);
		return -EPERM;
	}

	err = mtktscharger_register_thermal();
	if (err)
		goto err_unreg;

	mtktscharger_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtktscharger_dir) {
		mtktscharger_pr_notice("%s get /proc/driver/thermal failed\n",
								__func__);
	} else {
		entry = proc_create("tzcharger", 0664, mtktscharger_dir,
							&mtktscharger_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
	}

	return 0;

err_unreg:
	mtktscharger_unregister_cooler();

	return 0;
}

static int mtktscharger_pdrv_remove(struct platform_device *pdev)
{
	return 0;
}

struct platform_device mtktscharger_device = {
	.name = "mtktscharger",
	.id = -1,
};

static struct platform_driver mtktscharger_driver = {
	.probe = mtktscharger_pdrv_probe,
	.remove = mtktscharger_pdrv_remove,
	.driver = {
		   .name = "mtktscharger",
		   .owner  = THIS_MODULE,
		   },
};
#endif

static int __init mtktscharger_init(void)
{
	int err = 0;
#if (CONFIG_MTK_GAUGE_VERSION == 30)
	/* Move this segment to probe function
	 * in case mtktscharger reads temperature
	 * before mtk_charger allows it.
	 */
#else
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtktscharger_dir = NULL;
#endif
	mtkts_btscharger_prepare_table(g_RAP_ntc_table);
	err = mtktscharger_register_cooler();
	if (err)
		return err;

#if (CONFIG_MTK_GAUGE_VERSION == 30)
	/* Move this segment to probe function
	 * in case mtktscharger reads temperature
	 * before mtk_charger allows it.
	 */
#else
	err = mtktscharger_register_thermal();
	if (err)
		goto err_unreg;

	mtktscharger_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtktscharger_dir) {
		mtktscharger_dprintk("%s get /proc/driver/thermal failed\n",
								__func__);
	} else {
		entry = proc_create("tzcharger", 0664, mtktscharger_dir,
							&mtktscharger_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
	}
#endif

#if (CONFIG_MTK_GAUGE_VERSION == 30)
	/* TODO: consider not to register a charger thermal zone
	 * if not PEP30 or dual charger.
	 */
	/* register platform device/driver
	 */
	err = platform_device_register(&mtktscharger_device);
	if (err) {
		mtktscharger_dprintk("%s fail to reg device\n", __func__);
		goto err_unreg;
	}

	err = platform_driver_register(&mtktscharger_driver);
	if (err) {
		mtktscharger_dprintk("%s fail to reg driver\n", __func__);
		goto reg_platform_driver_fail;
	}
#endif

	return 0;

#if (CONFIG_MTK_GAUGE_VERSION == 30)
reg_platform_driver_fail:
	platform_device_unregister(&mtktscharger_device);
#endif

err_unreg:

	mtktscharger_unregister_cooler();

	return err;
}

static void __exit mtktscharger_exit(void)
{
	mtktscharger_dprintk("%s\n", __func__);
	mtktscharger_unregister_thermal();
	mtktscharger_unregister_cooler();
}

late_initcall(mtktscharger_init);
module_exit(mtktscharger_exit);
