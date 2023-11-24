// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
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
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/uaccess.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mach/mtk_thermal.h"
#include "mtk_thermal_timer.h"
#include "mt-plat/mtk_thermal_platform.h"
#include <linux/uidgid.h>
#include <tmp_bts.h>
#include <linux/slab.h>
#if IS_ENABLED(CONFIG_MEDIATEK_MT6577_AUXADC)
#include <linux/iio/consumer.h>
#endif

/* N17 code for HQ-296383 by liunianliang at 2023/05/17 */
/*
 * This file has been modified for thermal NTC about CPU
 * the AUXADC is 0
 */

/*=============================================================
 *Weak functions
 *=============================================================
 */
#if !IS_ENABLED(CONFIG_MEDIATEK_MT6577_AUXADC)
int __attribute__ ((weak))
IMM_IsAdcInitReady(void)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
	return 0;
}
int __attribute__ ((weak))
IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
	return -1;
}

#endif
int __attribute__ ((weak))
tsdctm_thermal_get_ttj_on(void)
{
	return 0;
}

/*=============================================================*/
static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
static DEFINE_SEMAPHORE(sem_mutex);

static unsigned int interval = 1;	/* seconds, 0 : no auto polling */
static int trip_temp[10] = { 120000, 96000, 95000, 90000, 80000,
				70000, 65000, 60000, 55000, 50000 };

static struct thermal_zone_device *thz_dev;
static int mtkcs_cpu_debug_log;
static int kernelmode;
static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static int num_trip = 1;
static char g_bind0[20] = {"mtkcsCPU-sysrst"};
static char g_bind1[20] = { 0 };
static char g_bind2[20] = { 0 };
static char g_bind3[20] = { 0 };
static char g_bind4[20] = { 0 };
static char g_bind5[20] = { 0 };
static char g_bind6[20] = { 0 };
static char g_bind7[20] = { 0 };
static char g_bind8[20] = { 0 };
static char g_bind9[20] = { 0 };

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

int cscpu_cur_temp = 1;


#define MTKCS_CPU_TEMP_CRIT 60000	/* 60.000 degree Celsius */

#define mtkcs_cpu_dprintk(fmt, args...)   \
do {                                    \
	if (mtkcs_cpu_debug_log) {                \
		pr_notice("[Thermal/TZ/CSCPU]" fmt, ##args); \
	}                                   \
} while (0)


#define mtkcs_cpu_printk(fmt, args...) \
pr_debug("[Thermal/TZ/CSCPU]" fmt, ##args)

#if IS_ENABLED(CONFIG_MEDIATEK_MT6577_AUXADC)
struct iio_channel *thermistor_ch0;
#endif

struct CSCPU_TEMPERATURE {
	__s32 CSCPU_Temp;
	__s32 TemperatureR;
};

static int g_RAP_pull_up_R = CSCPU_RAP_PULL_UP_R;
static int g_TAP_over_critical_low = CSCPU_TAP_OVER_CRITICAL_LOW;
static int g_RAP_pull_up_voltage = CSCPU_RAP_PULL_UP_VOLTAGE;
static int g_RAP_ntc_table = CSCPU_RAP_NTC_TABLE;
static int g_RAP_ADC_channel = CSCPU_RAP_ADC_CHANNEL;
static int g_AP_TemperatureR;

static struct CSCPU_TEMPERATURE *CSCPU_Temperature_Table;
static int ntc_tbl_size;

/* AP_NTC_BL197 */
static struct CSCPU_TEMPERATURE CSCPU_Temperature_Table1[] = {
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
static struct CSCPU_TEMPERATURE CSCPU_Temperature_Table2[] = {
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
static struct CSCPU_TEMPERATURE CSCPU_Temperature_Table3[] = {
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
static struct CSCPU_TEMPERATURE CSCPU_Temperature_Table4[] = {
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
static struct CSCPU_TEMPERATURE CSCPU_Temperature_Table5[] = {
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
static struct CSCPU_TEMPERATURE CSCPU_Temperature_Table6[] = {
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

/* NCP03WF104F05RL- secondary supply(100K) */
static struct CSCPU_TEMPERATURE CSCPU_Temperature_Table7[] = {
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

/* SDNT0603C104F4250FTF- main supply (100K) */
static struct CSCPU_TEMPERATURE CSCPU_Temperature_Table8[] = {
	{-40, 4429000},
	{-35, 3132000},
	{-30, 2236000},
	{-25, 1614000},
	{-20, 1177000},
	{-15, 865400},
	{-10, 642900},
	{-5, 485200},
	{0, 365600},
	{5, 278500},
	{10, 214500},
	{15, 165400},
	{20, 129800},
	{25, 100000},		/* 100K */
	{30, 79400},
	{35, 63400},
#if defined(APPLY_PRECISE_NTC_TABLE)
	{40, 50770},
	{41, 48680},
	{42, 46790},
	{43, 44830},
	{44, 42720},
	{45, 40930},
	{46, 39220},
	{47, 37620},
	{48, 36060},
	{49, 34590},
	{50, 33190},
	{51, 31850},
	{52, 30560},
	{53, 29340},
	{54, 28170},
	{55, 27050},
	{56, 25980},
	{57, 24970},
	{58, 24000},
	{59, 23060},
	{60, 22160},
	{61, 21310},
	{62, 20490},
	{63, 19730},
	{64, 18970},
	{65, 18250},
	{66, 17570},
	{67, 16920},
	{68, 16290},
	{69, 15700},
	{70, 15110},
	{71, 14560},
	{72, 14030},
	{73, 13520},
	{74, 13040},
	{75, 12590},
	{76, 12130},
	{77, 11720},
	{78, 11290},
	{79, 10900},
	{80, 10510},
	{81, 10140},
	{82,  9779},
	{83,  9454},
	{84,  9134},
	{85,  8814},
	{86,  8521},
	{87,  8224},
	{88,  7952},
	{89,  7689},
	{90,  7421},
#else
	{40, 50677},
	{45, 40930},
	{50, 33190},
	{55, 27050},
	{60, 22160},
	{65, 18250},
	{70, 15110},
	{75, 12590},
	{80, 10510},
	{85,  8814},
	{90,  7421},
#endif
	{95, 6283},
	{100, 5335},
	{105, 4546},
	{110, 3887},
	{115, 3339},
	{120, 2878},
	{125, 2493}
};

/* convert register to temperature  */
static __s32 mtkcs_cpu_thermistor_conver_temp(__s32 Res)
{
	int i = 0;
	int asize = 0;
	__s32 RES1 = 0, RES2 = 0;
	__s32 TAP_Value = -200, TMP1 = 0, TMP2 = 0;
#ifdef APPLY_PRECISE_BTS_TEMP
	TAP_Value = TAP_Value * 1000;
#endif
	asize = (ntc_tbl_size / sizeof(struct CSCPU_TEMPERATURE));

	/* mtkcs_cpu_dprintk("mtkcs_cpu_thermistor_conver_temp() :
	 * asize = %d, Res = %d\n",asize,Res);
	 */
	if (Res >= CSCPU_Temperature_Table[0].TemperatureR) {
		TAP_Value = -40;	/* min */
#ifdef APPLY_PRECISE_BTS_TEMP
		TAP_Value = TAP_Value * 1000;
#endif
	} else if (Res <= CSCPU_Temperature_Table[asize - 1].TemperatureR) {
		TAP_Value = 125;	/* max */
#ifdef APPLY_PRECISE_BTS_TEMP
		TAP_Value = TAP_Value * 1000;
#endif
	} else {
		RES1 = CSCPU_Temperature_Table[0].TemperatureR;
		TMP1 = CSCPU_Temperature_Table[0].CSCPU_Temp;
		/* mtkcs_cpu_dprintk("%d : RES1 = %d,TMP1 = %d\n",
		 * __LINE__,RES1,TMP1);
		 */

		for (i = 0; i < asize; i++) {
			if (Res >= CSCPU_Temperature_Table[i].TemperatureR) {
				RES2 = CSCPU_Temperature_Table[i].TemperatureR;
				TMP2 = CSCPU_Temperature_Table[i].CSCPU_Temp;
				/* mtkcs_cpu_dprintk("%d :i=%d, RES2 = %d,
				 * TMP2 = %d\n",__LINE__,i,RES2,TMP2);
				 */
				break;
			}
			RES1 = CSCPU_Temperature_Table[i].TemperatureR;
			TMP1 = CSCPU_Temperature_Table[i].CSCPU_Temp;
			/* mtkcs_cpu_dprintk("%d :i=%d, RES1 = %d,TMP1 = %d\n",
			 * __LINE__,i,RES1,TMP1);
			 */
		}
#ifdef APPLY_PRECISE_BTS_TEMP
		TAP_Value = mult_frac((((Res - RES2) * TMP1) +
			((RES1 - Res) * TMP2)), 1000, (RES1 - RES2));
#else
		TAP_Value = (((Res - RES2) * TMP1) + ((RES1 - Res) * TMP2))
								/ (RES1 - RES2);
#endif
	}


	return TAP_Value;
}

/* convert ADC_AP_temp_volt to register */
/*Volt to Temp formula same with 6589*/
static __s32 mtk_ts_cscpu_volt_to_temp(__u32 dwVolt)
{
	__s32 TRes;
	__u64 dwVCriAP = 0;
	__s32 CSCPU_TMP = -100;
	__u64 dwVCriAP2 = 0;
	/* SW workaround-----------------------------------------------------
	 * dwVCriAP = (TAP_OVER_CRITICAL_LOW * 1800) /
	 *		(TAP_OVER_CRITICAL_LOW + 39000);
	 * dwVCriAP = (TAP_OVER_CRITICAL_LOW * RAP_PULL_UP_VOLT) /
	 *		(TAP_OVER_CRITICAL_LOW + RAP_PULL_UP_R);
	 */

	dwVCriAP = ((__u64)g_TAP_over_critical_low *
		(__u64)g_RAP_pull_up_voltage);
	dwVCriAP2 = (g_TAP_over_critical_low + g_RAP_pull_up_R);
	do_div(dwVCriAP, dwVCriAP2);

#ifdef APPLY_PRECISE_BTS_TEMP
	if ((dwVolt / 100) > ((__u32)dwVCriAP)) {
		TRes = g_TAP_over_critical_low;
	} else {
		/* TRes = (39000*dwVolt) / (1800-dwVolt); */
		/* TRes = (RAP_PULL_UP_R*dwVolt) / (RAP_PULL_UP_VOLT-dwVolt); */
		TRes = ((long long)g_RAP_pull_up_R * dwVolt) /
					(g_RAP_pull_up_voltage * 100 - dwVolt);
	}
#else
	if (dwVolt > ((__u32)dwVCriAP)) {
		TRes = g_TAP_over_critical_low;
	} else {
		/* TRes = (39000*dwVolt) / (1800-dwVolt); */
		/* TRes = (RAP_PULL_UP_R*dwVolt) / (RAP_PULL_UP_VOLT-dwVolt); */
		TRes = (g_RAP_pull_up_R * dwVolt) /
					(g_RAP_pull_up_voltage - dwVolt);
	}
#endif
	/* ------------------------------------------------------------------ */
	g_AP_TemperatureR = TRes;

	/* convert register to temperature */
	CSCPU_TMP = mtkcs_cpu_thermistor_conver_temp(TRes);

	return CSCPU_TMP;
}

static int get_hw_cscpu_temp(void)
{


#if IS_ENABLED(CONFIG_MEDIATEK_MT6577_AUXADC)
	int val = 0;
	int ret = 0, output;
#else
	int ret = 0, data[4], i, ret_value = 0, ret_temp = 0, output;
	int times = 1, Channel = g_RAP_ADC_channel; /* 6752=0(AUX_IN0_NTC) */
	static int valid_temp;
#endif

#if IS_ENABLED(CONFIG_MEDIATEK_MT6577_AUXADC)
	ret = iio_read_channel_processed(thermistor_ch0, &val);
	if (ret < 0) {
		mtkcs_cpu_printk("Busy/Timeout, IIO ch read failed %d\n", ret);
		return ret;
	}

#ifdef APPLY_PRECISE_BTS_TEMP
	ret = val * 100;
#else
	ret = val;
#endif

#else
#if defined(APPLY_AUXADC_CALI_DATA)
	int auxadc_cali_temp;
#endif

	if (IMM_IsAdcInitReady() == 0) {
		mtkcs_cpu_printk(
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
		 *      data[0] = (rawdata * 1500 / (4096 + cali_ge)) / 1000;
		 *
		 * convert to mv, need multiply 10:
		 *      data[1] = (rawdata * 150 / (4096 + cali_ge)) % 100;
		 *
		 * provide high precision mv:
		 *      data[2] = (rawdata * 1500 / (4096 + cali_ge)) % 1000;
		 */
			auxadc_cali_temp = data[0]*1000+data[2];
			valid_temp = auxadc_cali_temp;
#else
			valid_temp = ret_temp;
#endif
		}

#if defined(APPLY_AUXADC_CALI_DATA)
		ret += auxadc_cali_temp;
		mtkcs_cpu_dprintk(
			"[thermal_auxadc_get_data(AUX_IN0_NTC)]: ret_temp=%d\n",
			auxadc_cali_temp);
#else
		ret += ret_temp;
		mtkcs_cpu_dprintk(
			"[thermal_auxadc_get_data(AUX_IN0_NTC)]: ret_temp=%d\n",
			ret_temp);
#endif
	}

	/* Mt_auxadc_hal.c */
	/* #define VOLTAGE_FULL_RANGE  1500 // VA voltage */
	/* #define AUXADC_PRECISE      4096 // 12 bits */
#if defined(APPLY_AUXADC_CALI_DATA)
#else

#ifdef APPLY_PRECISE_BTS_TEMP
	ret = (val * 9375) >>  8;
#else
	ret = ret * 1500 / 4096;
#endif
#endif
#endif /*CONFIG_MEDIATEK_MT6577_AUXADC*/

	/* ret = ret*1800/4096;//82's ADC power */
	mtkcs_cpu_dprintk("CSCPUtery output mV = %d\n", ret);
	output = mtk_ts_cscpu_volt_to_temp(ret);
	mtkcs_cpu_dprintk("CSCPU output temperature = %d\n", output);
	return output;
}

static DEFINE_MUTEX(CSCPU_lock);
/*int ts_AP_at_boot_time = 0;*/
int mtkcs_cpu_get_hw_temp(void)
{
	int t_ret = 0;
	int t_ret2 = 0;

	mutex_lock(&CSCPU_lock);

	/* get HW AP temp (TSAP) */
	/* cat /sys/class/power_supply/AP/AP_temp */
	t_ret = get_hw_cscpu_temp();
#ifndef APPLY_PRECISE_BTS_TEMP
	t_ret = t_ret * 1000;
#endif
	mutex_unlock(&CSCPU_lock);


	if ((tsatm_thermal_get_catm_type() == 2) &&
		(tsdctm_thermal_get_ttj_on() == 0))
		t_ret2 = wakeup_ta_algo(TA_CATMPLUS_TTJ);

	if (t_ret2 < 0)
		pr_notice("[Thermal/TZ/CSCPU]wakeup_ta_algo out of memory\n");

	cscpu_cur_temp = t_ret;

	if (t_ret > 40000)	/* abnormal high temp */
		mtkcs_cpu_printk("T_CSCPU=%d\n", t_ret);

	mtkcs_cpu_dprintk("[%s] T_CSCPU, %d\n", __func__, t_ret);
	return t_ret;
}

static int mtkcs_cpu_get_temp(struct thermal_zone_device *thermal, int *t)
{
	*t = mtkcs_cpu_get_hw_temp();

	/* if ((int) *t > 52000) */
	/* mtkcs_cpu_dprintk("T=%d\n", (int) *t); */

#if IS_ENABLED(CONFIG_LVTS_DYNAMIC_ENABLE_REBOOT)
	if (*t > DYNAMIC_REBOOT_TRIP_TEMP)
		lvts_enable_all_hw_protect();
	else if (*t < DYNAMIC_REBOOT_EXIT_TEMP)
		lvts_disable_all_hw_protect();
#endif

	if ((int)*t >= polling_trip_temp1)
		thermal->polling_delay = interval * 1000;
	else if ((int)*t < polling_trip_temp2)
		thermal->polling_delay = interval * polling_factor2;
	else
		thermal->polling_delay = interval * polling_factor1;

	return 0;
}

static int mtkcs_cpu_bind(
struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtkcs_cpu_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtkcs_cpu_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtkcs_cpu_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtkcs_cpu_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtkcs_cpu_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtkcs_cpu_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtkcs_cpu_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtkcs_cpu_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtkcs_cpu_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtkcs_cpu_dprintk("[%s] %s\n", __func__, cdev->type);
	} else {
		return 0;
	}

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		mtkcs_cpu_dprintk(
			"[%s] error binding cooling dev\n", __func__);
		return -EINVAL;
	}

	mtkcs_cpu_dprintk("[%s] binding OK, %d\n", __func__, table_val);
	return 0;
}

static int mtkcs_cpu_unbind(struct thermal_zone_device *thermal,
			    struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtkcs_cpu_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtkcs_cpu_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtkcs_cpu_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtkcs_cpu_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtkcs_cpu_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtkcs_cpu_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtkcs_cpu_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtkcs_cpu_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtkcs_cpu_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtkcs_cpu_dprintk("[%s] %s\n", __func__, cdev->type);
	} else
		return 0;

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		mtkcs_cpu_dprintk(
			"[%s] error unbinding cooling dev\n", __func__);
		return -EINVAL;
	}

	mtkcs_cpu_dprintk("[%s] unbinding OK\n", __func__);
	return 0;
}


static int mtkcs_cpu_change_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}
static int mtkcs_cpu_get_trip_type(
struct thermal_zone_device *thermal, int trip, enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtkcs_cpu_get_trip_temp(
struct thermal_zone_device *thermal, int trip, int *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtkcs_cpu_get_crit_temp(
struct thermal_zone_device *thermal, int *temperature)
{
	*temperature = MTKCS_CPU_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtkcs_CPU_dev_ops = {
	.bind = mtkcs_cpu_bind,
	.unbind = mtkcs_cpu_unbind,
	.get_temp = mtkcs_cpu_get_temp,
	.change_mode = mtkcs_cpu_change_mode,
	.get_trip_type = mtkcs_cpu_get_trip_type,
	.get_trip_temp = mtkcs_cpu_get_trip_temp,
	.get_crit_temp = mtkcs_cpu_get_crit_temp,
};



static int mtkcs_cpu_read(struct seq_file *m, void *v)
{

	seq_printf(m,
		"[%s] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,trip_4_temp=%d\n",
		__func__,
		trip_temp[0], trip_temp[1], trip_temp[2],
		trip_temp[3], trip_temp[4]);

	seq_printf(m,
		"trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,trip_9_temp=%d,\n",
		trip_temp[5], trip_temp[6], trip_temp[7],
		trip_temp[8], trip_temp[9]);

	seq_printf(m,
		"g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,g_THERMAL_TRIP_3=%d,\n",
		g_THERMAL_TRIP[0], g_THERMAL_TRIP[1],
		g_THERMAL_TRIP[2], g_THERMAL_TRIP[3]);

	seq_printf(m,
		"g_THERMAL_TRIP_4=%d,g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,g_THERMAL_TRIP_7=%d,\n",
		g_THERMAL_TRIP[4], g_THERMAL_TRIP[5],
		g_THERMAL_TRIP[6], g_THERMAL_TRIP[7]);

	seq_printf(m, "g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
		g_THERMAL_TRIP[8], g_THERMAL_TRIP[9]);

	seq_printf(m,
		"cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,\n",
		g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);

	seq_printf(m,
		"cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s,time_ms=%d\n",
		g_bind5, g_bind6, g_bind7, g_bind8, g_bind9, interval * 1000);

	return 0;
}

static int mtkcs_cpu_register_thermal(void);
static void mtkcs_cpu_unregister_thermal(void);

static ssize_t mtkcs_cpu_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	int len = 0, i;

	struct mtkcscpu_data {
		int trip[10];
		int t_type[10];
		char bind0[20], bind1[20], bind2[20], bind3[20], bind4[20];
		char bind5[20], bind6[20], bind7[20], bind8[20], bind9[20];
		int time_msec;
		char desc[512];
	};

	struct mtkcscpu_data *ptr_mtkcscpu_data = kmalloc(
					sizeof(*ptr_mtkcscpu_data), GFP_KERNEL);

	if (ptr_mtkcscpu_data == NULL)
		return -ENOMEM;


	len = (count < (sizeof(ptr_mtkcscpu_data->desc) - 1)) ?
				count : (sizeof(ptr_mtkcscpu_data->desc) - 1);

	if (copy_from_user(ptr_mtkcscpu_data->desc, buffer, len)) {
		kfree(ptr_mtkcscpu_data);
		return 0;
	}

	ptr_mtkcscpu_data->desc[len] = '\0';

	if (sscanf
	    (ptr_mtkcscpu_data->desc,
	     "%d %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d",
		&num_trip,
		&ptr_mtkcscpu_data->trip[0], &ptr_mtkcscpu_data->t_type[0],
		ptr_mtkcscpu_data->bind0,
		&ptr_mtkcscpu_data->trip[1], &ptr_mtkcscpu_data->t_type[1],
		ptr_mtkcscpu_data->bind1,
		&ptr_mtkcscpu_data->trip[2], &ptr_mtkcscpu_data->t_type[2],
		ptr_mtkcscpu_data->bind2,
		&ptr_mtkcscpu_data->trip[3], &ptr_mtkcscpu_data->t_type[3],
		ptr_mtkcscpu_data->bind3,
		&ptr_mtkcscpu_data->trip[4], &ptr_mtkcscpu_data->t_type[4],
		ptr_mtkcscpu_data->bind4,
		&ptr_mtkcscpu_data->trip[5], &ptr_mtkcscpu_data->t_type[5],
		ptr_mtkcscpu_data->bind5,
		&ptr_mtkcscpu_data->trip[6], &ptr_mtkcscpu_data->t_type[6],
		ptr_mtkcscpu_data->bind6,
		&ptr_mtkcscpu_data->trip[7], &ptr_mtkcscpu_data->t_type[7],
		ptr_mtkcscpu_data->bind7,
		&ptr_mtkcscpu_data->trip[8], &ptr_mtkcscpu_data->t_type[8],
		ptr_mtkcscpu_data->bind8,
		&ptr_mtkcscpu_data->trip[9], &ptr_mtkcscpu_data->t_type[9],
		ptr_mtkcscpu_data->bind9,
		&ptr_mtkcscpu_data->time_msec) == 32) {

		down(&sem_mutex);
		mtkcs_cpu_dprintk(
			"[%s] mtkcs_cpu_unregister_thermal\n", __func__);

		mtkcs_cpu_unregister_thermal();

		if (num_trip < 0 || num_trip > 10) {
			#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
			aee_kernel_warning_api(__FILE__, __LINE__,
					DB_OPT_DEFAULT, "mtkcs_cpu_write",
					"Bad argument");
			#endif
			mtkcs_cpu_dprintk("[%s] bad argument\n", __func__);
			kfree(ptr_mtkcscpu_data);
			up(&sem_mutex);
			return -EINVAL;
		}

		for (i = 0; i < num_trip; i++)
			g_THERMAL_TRIP[i] = ptr_mtkcscpu_data->t_type[i];

		g_bind0[0] = g_bind1[0] = g_bind2[0] = g_bind3[0]
			= g_bind4[0] = g_bind5[0] = g_bind6[0]
			= g_bind7[0] = g_bind8[0] = g_bind9[0] = '\0';

		for (i = 0; i < 20; i++) {
			g_bind0[i] = ptr_mtkcscpu_data->bind0[i];
			g_bind1[i] = ptr_mtkcscpu_data->bind1[i];
			g_bind2[i] = ptr_mtkcscpu_data->bind2[i];
			g_bind3[i] = ptr_mtkcscpu_data->bind3[i];
			g_bind4[i] = ptr_mtkcscpu_data->bind4[i];
			g_bind5[i] = ptr_mtkcscpu_data->bind5[i];
			g_bind6[i] = ptr_mtkcscpu_data->bind6[i];
			g_bind7[i] = ptr_mtkcscpu_data->bind7[i];
			g_bind8[i] = ptr_mtkcscpu_data->bind8[i];
			g_bind9[i] = ptr_mtkcscpu_data->bind9[i];
		}

		mtkcs_cpu_dprintk(
			"[%s] g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,",
			__func__,
			g_THERMAL_TRIP[0], g_THERMAL_TRIP[1],
			g_THERMAL_TRIP[2]);

		mtkcs_cpu_dprintk(
			"g_THERMAL_TRIP_3=%d,g_THERMAL_TRIP_4=%d,g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,",
			g_THERMAL_TRIP[3], g_THERMAL_TRIP[4],
			g_THERMAL_TRIP[5], g_THERMAL_TRIP[6]);

		mtkcs_cpu_dprintk(
			"g_THERMAL_TRIP_7=%d,g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
			g_THERMAL_TRIP[7], g_THERMAL_TRIP[8],
			g_THERMAL_TRIP[9]);

		mtkcs_cpu_dprintk(
			"[%s] cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,",
			__func__,
			g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);

		mtkcs_cpu_dprintk(
			"cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
			g_bind5, g_bind6, g_bind7, g_bind8, g_bind9);

		for (i = 0; i < num_trip; i++)
			trip_temp[i] = ptr_mtkcscpu_data->trip[i];

		interval = ptr_mtkcscpu_data->time_msec / 1000;

		mtkcs_cpu_dprintk(
			"[%s] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,",
			__func__,
			trip_temp[0], trip_temp[1], trip_temp[2], trip_temp[3]);

		mtkcs_cpu_dprintk(
			"trip_4_temp=%d,trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,",
			trip_temp[4], trip_temp[5], trip_temp[6],
			trip_temp[7], trip_temp[8]);

		mtkcs_cpu_dprintk("trip_9_temp=%d,time_ms=%d\n",
			trip_temp[9], interval * 1000);

		mtkcs_cpu_dprintk(
			"[%s] mtkcs_cpu_register_thermal\n", __func__);

		mtkcs_cpu_register_thermal();
		up(&sem_mutex);
		kfree(ptr_mtkcscpu_data);
		/* AP_write_flag=1; */
		return count;
	}

	mtkcs_cpu_dprintk("[%s] bad argument\n", __func__);
    #if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
	aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT,
							"mtkcs_cpu_write",
							"Bad argument");
    #endif
	kfree(ptr_mtkcscpu_data);
	return -EINVAL;
}

void mtkcs_cpu_prepare_table(int table_num)
{

	switch (table_num) {
	case 1:		/* AP_NTC_BL197 */
		CSCPU_Temperature_Table = CSCPU_Temperature_Table1;
		ntc_tbl_size = sizeof(CSCPU_Temperature_Table1);
		break;
	case 2:		/* AP_NTC_TSM_1 */
		CSCPU_Temperature_Table = CSCPU_Temperature_Table2;
		ntc_tbl_size = sizeof(CSCPU_Temperature_Table2);
		break;
	case 3:		/* AP_NTC_10_SEN_1 */
		CSCPU_Temperature_Table = CSCPU_Temperature_Table3;
		ntc_tbl_size = sizeof(CSCPU_Temperature_Table3);
		break;
	case 4:		/* AP_NTC_10 */
		CSCPU_Temperature_Table = CSCPU_Temperature_Table4;
		ntc_tbl_size = sizeof(CSCPU_Temperature_Table4);
		break;
	case 5:		/* AP_NTC_47 */
		CSCPU_Temperature_Table = CSCPU_Temperature_Table5;
		ntc_tbl_size = sizeof(CSCPU_Temperature_Table5);
		break;
	case 6:		/* NTCG104EF104F */
		CSCPU_Temperature_Table = CSCPU_Temperature_Table6;
		ntc_tbl_size = sizeof(CSCPU_Temperature_Table6);
		break;
	case 7:		/* NCP03WF104F05RL- secondary supply */
		CSCPU_Temperature_Table = CSCPU_Temperature_Table7;
		ntc_tbl_size = sizeof(CSCPU_Temperature_Table7);
		break;
	case 8:		/* SDNT0603C104F4250FTF- main supply */
		CSCPU_Temperature_Table = CSCPU_Temperature_Table8;
		ntc_tbl_size = sizeof(CSCPU_Temperature_Table8);
		break;
	default:		/* AP_NTC_10 */
		CSCPU_Temperature_Table = CSCPU_Temperature_Table4;
		ntc_tbl_size = sizeof(CSCPU_Temperature_Table4);
		break;
	}

	pr_notice("[Thermal/TZ/BTS] %s table_num=%d\n", __func__, table_num);

}

static int mtkcs_cpu_param_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", g_RAP_pull_up_R);
	seq_printf(m, "%d\n", g_RAP_pull_up_voltage);
	seq_printf(m, "%d\n", g_TAP_over_critical_low);
	seq_printf(m, "%d\n", g_RAP_ntc_table);
	seq_printf(m, "%d\n", g_RAP_ADC_channel);
	return 0;
}


static ssize_t mtkcs_cpu_param_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	int len = 0;
	struct mtkcscpu_param_data {
		char desc[512];
		char pull_R[10], pull_V[10];
		char overcrilow[16];
		char NTC_TABLE[10];
		unsigned int valR, valV, over_cri_low, ntc_table;
		unsigned int adc_channel;
	};

	struct mtkcscpu_param_data *ptr_mtkcscpu_parm_data;

	ptr_mtkcscpu_parm_data = kmalloc(
				sizeof(*ptr_mtkcscpu_parm_data), GFP_KERNEL);

	if (ptr_mtkcscpu_parm_data == NULL)
		return -ENOMEM;

	/* external pin: 0/1/12/13/14/15, can't use pin:2/3/4/5/6/7/8/9/10/11,
	 *choose "adc_channel=11" to check if there is any param input
	 */
	ptr_mtkcscpu_parm_data->adc_channel = 11;

	len = (count < (sizeof(ptr_mtkcscpu_parm_data->desc) - 1)) ?
			count : (sizeof(ptr_mtkcscpu_parm_data->desc) - 1);

	if (copy_from_user(ptr_mtkcscpu_parm_data->desc, buffer, len)) {
		kfree(ptr_mtkcscpu_parm_data);
		return 0;
	}

	ptr_mtkcscpu_parm_data->desc[len] = '\0';

	mtkcs_cpu_dprintk("[%s]\n", __func__);

	if (sscanf
	    (ptr_mtkcscpu_parm_data->desc, "%9s %d %9s %d %15s %d %9s %d %d",
		ptr_mtkcscpu_parm_data->pull_R, &ptr_mtkcscpu_parm_data->valR,
		ptr_mtkcscpu_parm_data->pull_V, &ptr_mtkcscpu_parm_data->valV,
		ptr_mtkcscpu_parm_data->overcrilow,
		&ptr_mtkcscpu_parm_data->over_cri_low,
		ptr_mtkcscpu_parm_data->NTC_TABLE,
		&ptr_mtkcscpu_parm_data->ntc_table,
		&ptr_mtkcscpu_parm_data->adc_channel) >= 8) {

		if (!strcmp(ptr_mtkcscpu_parm_data->pull_R, "PUP_R")) {
			g_RAP_pull_up_R = ptr_mtkcscpu_parm_data->valR;
			mtkcs_cpu_dprintk("g_RAP_pull_up_R=%d\n",
							g_RAP_pull_up_R);
		} else {
			mtkcs_cpu_printk(
				"[%s] bad PUP_R argument\n", __func__);
			kfree(ptr_mtkcscpu_parm_data);
			return -EINVAL;
		}

		if (!strcmp(ptr_mtkcscpu_parm_data->pull_V, "PUP_VOLT")) {
			g_RAP_pull_up_voltage = ptr_mtkcscpu_parm_data->valV;
			mtkcs_cpu_dprintk("g_Rat_pull_up_voltage=%d\n",
							g_RAP_pull_up_voltage);
		} else {
			mtkcs_cpu_printk(
				"[%s] bad PUP_VOLT argument\n", __func__);
			kfree(ptr_mtkcscpu_parm_data);
			return -EINVAL;
		}

		if (!strcmp(ptr_mtkcscpu_parm_data->overcrilow,
			"OVER_CRITICAL_L")) {
			g_TAP_over_critical_low =
					ptr_mtkcscpu_parm_data->over_cri_low;
			mtkcs_cpu_dprintk("g_TAP_over_critical_low=%d\n",
						g_TAP_over_critical_low);
		} else {
			mtkcs_cpu_printk(
				"[%s] bad OVERCRIT_L argument\n", __func__);
			kfree(ptr_mtkcscpu_parm_data);
			return -EINVAL;
		}

		if (!strcmp(ptr_mtkcscpu_parm_data->NTC_TABLE, "NTC_TABLE")) {
			g_RAP_ntc_table = ptr_mtkcscpu_parm_data->ntc_table;
			mtkcs_cpu_dprintk("g_RAP_ntc_table=%d\n",
							g_RAP_ntc_table);
		} else {
			mtkcs_cpu_printk(
				"[%s] bad NTC_TABLE argument\n", __func__);
			kfree(ptr_mtkcscpu_parm_data);
			return -EINVAL;
		}

		/* external pin: 0/1/12/13/14/15,
		 * can't use pin:2/3/4/5/6/7/8/9/10/11,
		 * choose "adc_channel=11" to check if there is any param input
		 */
		if ((ptr_mtkcscpu_parm_data->adc_channel >= 2)
		&& (ptr_mtkcscpu_parm_data->adc_channel <= 11))
			/* check unsupport pin value, if unsupport,
			 * set channel = 1 as default setting.
			 */
			g_RAP_ADC_channel = AUX_IN0_NTC;
		else {
			/* check if there is any param input,
			 * if not using default g_RAP_ADC_channel:1
			 */
			if (ptr_mtkcscpu_parm_data->adc_channel != 11)
				g_RAP_ADC_channel =
					ptr_mtkcscpu_parm_data->adc_channel;
			else
				g_RAP_ADC_channel = AUX_IN0_NTC;
		}
		mtkcs_cpu_dprintk("adc_channel=%d\n",
					ptr_mtkcscpu_parm_data->adc_channel);
		mtkcs_cpu_dprintk("g_RAP_ADC_channel=%d\n",
						g_RAP_ADC_channel);

		mtkcs_cpu_prepare_table(g_RAP_ntc_table);

		kfree(ptr_mtkcscpu_parm_data);
		return count;
	}

	mtkcs_cpu_printk("[%s] bad argument\n", __func__);
	kfree(ptr_mtkcscpu_parm_data);
	return -EINVAL;
}

static int mtkcs_cpu_register_thermal(void)
{
	mtkcs_cpu_dprintk("[%s]\n", __func__);

	/* trips : trip 0~1 */
	thz_dev = mtk_thermal_zone_device_register("mtkcsCPU", num_trip, NULL,
						&mtkcs_CPU_dev_ops, 0, 0, 0,
						interval * 1000);

	return 0;
}

static void mtkcs_cpu_unregister_thermal(void)
{
	mtkcs_cpu_dprintk("[%s]\n", __func__);

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static int mtkcs_cpu_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtkcs_cpu_read, NULL);
}

static const struct proc_ops mtkts_AP_fops = {
	.proc_open = mtkcs_cpu_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_write = mtkcs_cpu_write,
	.proc_release = single_release,
};


static int mtkcs_cpu_param_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtkcs_cpu_param_read, NULL);
}

static const struct proc_ops mtkts_AP_param_fops = {
	.proc_open = mtkcs_cpu_param_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_write = mtkcs_cpu_param_write,
	.proc_release = single_release,
};

#if IS_ENABLED(CONFIG_MEDIATEK_MT6577_AUXADC)
static int mtkcs_cpu_probe(struct platform_device *pdev)
{
	int err = 0;
	int ret = 0;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtkts_CPU_dir = NULL;

	mtkcs_cpu_dprintk("[%s]\n", __func__);


	if (!pdev->dev.of_node) {
		mtkcs_cpu_printk("[%s] Only DT based supported\n",
			__func__);
		return -ENODEV;
	}

	thermistor_ch0 = devm_kzalloc(&pdev->dev, sizeof(*thermistor_ch0),
		GFP_KERNEL);
	if (!thermistor_ch0)
		return -ENOMEM;


	thermistor_ch0 = iio_channel_get(&pdev->dev, "thermistor-ch0");
	ret = IS_ERR(thermistor_ch0);
	if (ret) {
		mtkcs_cpu_printk("[%s] fail to get auxadc iio ch0: %d\n",
			__func__, ret);
		return ret;
	}

	/* setup default table */
	mtkcs_cpu_prepare_table(g_RAP_ntc_table);

	mtkts_CPU_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtkts_CPU_dir) {
		mtkcs_cpu_dprintk(
			"[%s]: mkdir /proc/driver/thermal failed\n",
				__func__);
	} else {
		entry = proc_create("cscpu", 0664, mtkts_CPU_dir,
							&mtkts_AP_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

	entry = proc_create("cscpu_param", 0664, mtkts_CPU_dir,
				&mtkts_AP_param_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

	}

	mtkcs_cpu_register_thermal();

	return err;
}

#if IS_ENABLED(CONFIG_OF)
const struct of_device_id mt_thermistor_of_match0[2] = {
	{.compatible = "mediatek,mtboard-thermistor0",},
	{},
};
#endif

#define THERMAL_THERMISTOR_NAME    "mtboard-thermistor0"
static struct platform_driver mtk_thermal_cscpu_driver = {
	.remove = NULL,
	.shutdown = NULL,
	.probe = mtkcs_cpu_probe,
	.suspend = NULL,
	.resume = NULL,
	.driver = {
		.name = THERMAL_THERMISTOR_NAME,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = mt_thermistor_of_match0,
#endif
	},
};
#endif /*CONFIG_MEDIATEK_MT6577_AUXADC*/


int mtkcs_cpu_init(void)
{

#if IS_ENABLED(CONFIG_MEDIATEK_MT6577_AUXADC)
	int err = 0;
#else
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtkts_CPU_dir = NULL;
#endif

	if (get_supply_rank() == MAIN_SUPPLY) {
		g_RAP_ntc_table = MAIN_SUPPLY_RAP_NTC_TABLE;
		g_TAP_over_critical_low = 4429000;
	} else {
		g_RAP_ntc_table = CSCPU_RAP_NTC_TABLE;
	}

	printk("[%s], g_RAP_ntc_table = %d\n", __func__, g_RAP_ntc_table);

#if IS_ENABLED(CONFIG_MEDIATEK_MT6577_AUXADC)
	err = platform_driver_register(&mtk_thermal_cscpu_driver);
	if (err) {
		mtkcs_cpu_printk("thermal driver callback register failed.\n");
		return err;
	}
#else
	/* setup default table */
	mtkcs_cpu_prepare_table(g_RAP_ntc_table);

	mtkts_CPU_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtkts_CPU_dir) {
		mtkcs_cpu_dprintk("[%s]: mkdir /proc/driver/thermal failed\n",
								__func__);
	} else {
		entry = proc_create("cscpu", 0664, mtkts_CPU_dir,
							&mtkts_AP_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

		entry = proc_create("cscpu_param", 0664, mtkts_CPU_dir,
							&mtkts_AP_param_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

	}

	mtkcs_cpu_register_thermal();
#endif
	return 0;
}

void mtkcs_cpu_exit(void)
{
	mtkcs_cpu_dprintk("[%s]\n", __func__);
	mtkcs_cpu_unregister_thermal();
}


MODULE_LICENSE("GPL");
MODULE_AUTHOR("MediaTek Inc.");
