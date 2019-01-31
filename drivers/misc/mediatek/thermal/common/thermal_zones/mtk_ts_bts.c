/*
 * Copyright (C) 2017 MediaTek Inc.
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

/*=============================================================
 *Weak functions
 *=============================================================
 */
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
/*=============================================================*/
static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
static DEFINE_SEMAPHORE(sem_mutex);

static unsigned int interval;	/* seconds, 0 : no auto polling */
static int trip_temp[10] = { 120000, 110000, 100000, 90000, 80000,
				70000, 65000, 60000, 55000, 50000 };

static struct thermal_zone_device *thz_dev;
static int mtkts_bts_debug_log;
static int kernelmode;
static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static int num_trip;
static char g_bind0[20] = { 0 };
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

int bts_cur_temp = 1;


#define MTKTS_BTS_TEMP_CRIT 60000	/* 60.000 degree Celsius */

#define mtkts_bts_dprintk(fmt, args...)   \
do {                                    \
	if (mtkts_bts_debug_log) {                \
		pr_debug("[Thermal/TZ/BTS]" fmt, ##args); \
	}                                   \
} while (0)


#define mtkts_bts_printk(fmt, args...) \
pr_debug("[Thermal/TZ/BTS]" fmt, ##args)


/* #define INPUT_PARAM_FROM_USER_AP */

/*
 * kernel fopen/fclose
 */
/*
 *static mm_segment_t oldfs;
 *
 *static void my_close(int fd)
 *{
 *	set_fs(oldfs);
 *	sys_close(fd);
 *}
 *
 *static int my_open(char *fname, int flag)
 *{
 *	oldfs = get_fs();
 *    set_fs(KERNEL_DS);
 *    return sys_open(fname, flag, 0);
 *}
 */

struct BTS_TEMPERATURE {
	__s32 BTS_Temp;
	__s32 TemperatureR;
};

static int g_RAP_pull_up_R = BTS_RAP_PULL_UP_R;
static int g_TAP_over_critical_low = BTS_TAP_OVER_CRITICAL_LOW;
static int g_RAP_pull_up_voltage = BTS_RAP_PULL_UP_VOLTAGE;
static int g_RAP_ntc_table = BTS_RAP_NTC_TABLE;
static int g_RAP_ADC_channel = BTS_RAP_ADC_CHANNEL;
static int g_AP_TemperatureR;
/* BTS_TEMPERATURE BTS_Temperature_Table[] = {0}; */

static struct BTS_TEMPERATURE *BTS_Temperature_Table;
static int ntc_tbl_size;

/* AP_NTC_BL197 */
static struct BTS_TEMPERATURE BTS_Temperature_Table1[] = {
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
static struct BTS_TEMPERATURE BTS_Temperature_Table2[] = {
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
static struct BTS_TEMPERATURE BTS_Temperature_Table3[] = {
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

#if 0
/* AP_NTC_10 */
static struct BTS_TEMPERATURE BTS_Temperature_Table4[] = {
	{-20, 68237},
	{-15, 53650},
	{-10, 42506},
	{-5, 33892},
	{0, 27219},
	{5, 22021},
	{10, 17926},
	{15, 14674},
	{20, 12081},
	{25, 10000},
	{30, 8315},
	{35, 6948},
	{40, 5834},
	{45, 4917},
	{50, 4161},
	{55, 3535},
	{60, 3014}
};
#else
/* AP_NTC_10(TSM0A103F34D1RZ) */
static struct BTS_TEMPERATURE BTS_Temperature_Table4[] = {
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
#endif

/* AP_NTC_47 */
static struct BTS_TEMPERATURE BTS_Temperature_Table5[] = {
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
static struct BTS_TEMPERATURE BTS_Temperature_Table6[] = {
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
static struct BTS_TEMPERATURE BTS_Temperature_Table7[] = {
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
#else
	{40, 50677},
	{45, 40904},
	{50, 33195},
	{55, 27091},
	{60, 22224},
#endif
	{65, 18323},
	{70, 15184},
	{75, 12635},
	{80, 10566},
	{85, 8873},
	{90, 7481},
	{95, 6337},
	{100, 5384},
	{105, 4594},
	{110, 3934},
	{115, 3380},
	{120, 2916},
	{125, 2522}
};


/* convert register to temperature  */
static __s16 mtkts_bts_thermistor_conver_temp(__s32 Res)
{
	int i = 0;
	int asize = 0;
	__s32 RES1 = 0, RES2 = 0;
	__s32 TAP_Value = -200, TMP1 = 0, TMP2 = 0;

	asize = (ntc_tbl_size / sizeof(struct BTS_TEMPERATURE));

	/* mtkts_bts_dprintk("mtkts_bts_thermistor_conver_temp() :
	 * asize = %d, Res = %d\n",asize,Res);
	 */
	if (Res >= BTS_Temperature_Table[0].TemperatureR) {
		TAP_Value = -40;	/* min */
	} else if (Res <= BTS_Temperature_Table[asize - 1].TemperatureR) {
		TAP_Value = 125;	/* max */
	} else {
		RES1 = BTS_Temperature_Table[0].TemperatureR;
		TMP1 = BTS_Temperature_Table[0].BTS_Temp;
		/* mtkts_bts_dprintk("%d : RES1 = %d,TMP1 = %d\n",
		 * __LINE__,RES1,TMP1);
		 */

		for (i = 0; i < asize; i++) {
			if (Res >= BTS_Temperature_Table[i].TemperatureR) {
				RES2 = BTS_Temperature_Table[i].TemperatureR;
				TMP2 = BTS_Temperature_Table[i].BTS_Temp;
				/* mtkts_bts_dprintk("%d :i=%d, RES2 = %d,
				 * TMP2 = %d\n",__LINE__,i,RES2,TMP2);
				 */
				break;
			}
			RES1 = BTS_Temperature_Table[i].TemperatureR;
			TMP1 = BTS_Temperature_Table[i].BTS_Temp;
			/* mtkts_bts_dprintk("%d :i=%d, RES1 = %d,TMP1 = %d\n",
			 * __LINE__,i,RES1,TMP1);
			 */
		}

		TAP_Value = (((Res - RES2) * TMP1) + ((RES1 - Res) * TMP2))
								/ (RES1 - RES2);
	}

#if 0
	mtkts_bts_dprintk(
		"mtkts_bts_thermistor_conver_temp() : TAP_Value = %d\n",
								TAP_Value);

	mtkts_bts_dprintk("mtkts_bts_thermistor_conver_temp() : Res = %d\n",
									Res);

	mtkts_bts_dprintk("mtkts_bts_thermistor_conver_temp() : RES1 = %d\n",
									RES1);

	mtkts_bts_dprintk("mtkts_bts_thermistor_conver_temp() : RES2 = %d\n",
									RES2);

	mtkts_bts_dprintk("mtkts_bts_thermistor_conver_temp() : TMP1 = %d\n",
									TMP1);

	mtkts_bts_dprintk("mtkts_bts_thermistor_conver_temp() : TMP2 = %d\n",
									TMP2);
#endif

	return TAP_Value;
}

/* convert ADC_AP_temp_volt to register */
/*Volt to Temp formula same with 6589*/
static __s16 mtk_ts_bts_volt_to_temp(__u32 dwVolt)
{
	__s32 TRes;
	__s32 dwVCriAP = 0;
	__s32 BTS_TMP = -100;

	/* SW workaround-----------------------------------------------------
	 * dwVCriAP = (TAP_OVER_CRITICAL_LOW * 1800) /
	 *		(TAP_OVER_CRITICAL_LOW + 39000);
	 * dwVCriAP = (TAP_OVER_CRITICAL_LOW * RAP_PULL_UP_VOLT) /
	 *		(TAP_OVER_CRITICAL_LOW + RAP_PULL_UP_R);
	 */
	dwVCriAP = (g_TAP_over_critical_low * g_RAP_pull_up_voltage)
				/ (g_TAP_over_critical_low + g_RAP_pull_up_R);

	if (dwVolt > dwVCriAP) {
		TRes = g_TAP_over_critical_low;
	} else {
		/* TRes = (39000*dwVolt) / (1800-dwVolt); */
		/* TRes = (RAP_PULL_UP_R*dwVolt) / (RAP_PULL_UP_VOLT-dwVolt); */
		TRes = (g_RAP_pull_up_R * dwVolt) /
					(g_RAP_pull_up_voltage - dwVolt);
	}
	/* ------------------------------------------------------------------ */

	g_AP_TemperatureR = TRes;

	/* convert register to temperature */
	BTS_TMP = mtkts_bts_thermistor_conver_temp(TRes);

	return BTS_TMP;
}

static int get_hw_bts_temp(void)
{

	int ret = 0, data[4], i, ret_value = 0, ret_temp = 0, output;
	int times = 1, Channel = g_RAP_ADC_channel; /* 6752=0(AUX_IN0_NTC) */
	static int valid_temp;
#if defined(APPLY_AUXADC_CALI_DATA)
	int auxadc_cali_temp;
#endif

	if (IMM_IsAdcInitReady() == 0) {
		mtkts_bts_printk(
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
		mtkts_bts_dprintk(
			"[thermal_auxadc_get_data(AUX_IN0_NTC)]: ret_temp=%d\n",
			auxadc_cali_temp);
#else
		ret += ret_temp;
		mtkts_bts_dprintk(
			"[thermal_auxadc_get_data(AUX_IN0_NTC)]: ret_temp=%d\n",
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
	mtkts_bts_dprintk("APtery output mV = %d\n", ret);
	output = mtk_ts_bts_volt_to_temp(ret);
	mtkts_bts_dprintk("BTS output temperature = %d\n", output);
	return output;
}

static DEFINE_MUTEX(BTS_lock);
/*int ts_AP_at_boot_time = 0;*/
int mtkts_bts_get_hw_temp(void)
{
	int t_ret = 0;
	int t_ret2 = 0;

	mutex_lock(&BTS_lock);

	/* get HW AP temp (TSAP) */
	/* cat /sys/class/power_supply/AP/AP_temp */
	t_ret = get_hw_bts_temp();
	t_ret = t_ret * 1000;

	mutex_unlock(&BTS_lock);


	if (tsatm_thermal_get_catm_type() == 2)
		t_ret2 = wakeup_ta_algo(TA_CATMPLUS_TTJ);

	if (t_ret2 < 0)
		pr_notice("[Thermal/TZ/BTS]wakeup_ta_algo out of memory\n");

	bts_cur_temp = t_ret;

	if (t_ret > 40000)	/* abnormal high temp */
		mtkts_bts_printk("T_AP=%d\n", t_ret);

	mtkts_bts_dprintk("[mtkts_bts_get_hw_temp] T_AP, %d\n", t_ret);
	return t_ret;
}

static int mtkts_bts_get_temp(struct thermal_zone_device *thermal, int *t)
{
	*t = mtkts_bts_get_hw_temp();

	/* if ((int) *t > 52000) */
	/* mtkts_bts_dprintk("T=%d\n", (int) *t); */

	if ((int)*t >= polling_trip_temp1)
		thermal->polling_delay = interval * 1000;
	else if ((int)*t < polling_trip_temp2)
		thermal->polling_delay = interval * polling_factor2;
	else
		thermal->polling_delay = interval * polling_factor1;

	return 0;
}

static int mtkts_bts_bind(
struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtkts_bts_dprintk("[mtkts_bts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtkts_bts_dprintk("[mtkts_bts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtkts_bts_dprintk("[mtkts_bts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtkts_bts_dprintk("[mtkts_bts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtkts_bts_dprintk("[mtkts_bts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtkts_bts_dprintk("[mtkts_bts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtkts_bts_dprintk("[mtkts_bts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtkts_bts_dprintk("[mtkts_bts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtkts_bts_dprintk("[mtkts_bts_bind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtkts_bts_dprintk("[mtkts_bts_bind] %s\n", cdev->type);
	} else {
		return 0;
	}

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		mtkts_bts_dprintk(
			"[mtkts_bts_bind] error binding cooling dev\n");
		return -EINVAL;
	}

	mtkts_bts_dprintk("[mtkts_bts_bind] binding OK, %d\n", table_val);
	return 0;
}

static int mtkts_bts_unbind(struct thermal_zone_device *thermal,
			    struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		mtkts_bts_dprintk("[mtkts_bts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		mtkts_bts_dprintk("[mtkts_bts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		mtkts_bts_dprintk("[mtkts_bts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		mtkts_bts_dprintk("[mtkts_bts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		mtkts_bts_dprintk("[mtkts_bts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		mtkts_bts_dprintk("[mtkts_bts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		mtkts_bts_dprintk("[mtkts_bts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		mtkts_bts_dprintk("[mtkts_bts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		mtkts_bts_dprintk("[mtkts_bts_unbind] %s\n", cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		mtkts_bts_dprintk("[mtkts_bts_unbind] %s\n", cdev->type);
	} else
		return 0;

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		mtkts_bts_dprintk(
			"[mtkts_bts_unbind] error unbinding cooling dev\n");
		return -EINVAL;
	}

	mtkts_bts_dprintk("[mtkts_bts_unbind] unbinding OK\n");
	return 0;
}

static int mtkts_bts_get_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int mtkts_bts_set_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int mtkts_bts_get_trip_type(
struct thermal_zone_device *thermal, int trip, enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int mtkts_bts_get_trip_temp(
struct thermal_zone_device *thermal, int trip, int *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int mtkts_bts_get_crit_temp(
struct thermal_zone_device *thermal, int *temperature)
{
	*temperature = MTKTS_BTS_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtkts_BTS_dev_ops = {
	.bind = mtkts_bts_bind,
	.unbind = mtkts_bts_unbind,
	.get_temp = mtkts_bts_get_temp,
	.get_mode = mtkts_bts_get_mode,
	.set_mode = mtkts_bts_set_mode,
	.get_trip_type = mtkts_bts_get_trip_type,
	.get_trip_temp = mtkts_bts_get_trip_temp,
	.get_crit_temp = mtkts_bts_get_crit_temp,
};



static int mtkts_bts_read(struct seq_file *m, void *v)
{

	seq_printf(m,
		"[mtkts_bts_read] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,trip_4_temp=%d\n",
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

static int mtkts_bts_register_thermal(void);
static void mtkts_bts_unregister_thermal(void);

static ssize_t mtkts_bts_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	int len = 0, i;

	struct mtktsbts_data {
		int trip[10];
		int t_type[10];
		char bind0[20], bind1[20], bind2[20], bind3[20], bind4[20];
		char bind5[20], bind6[20], bind7[20], bind8[20], bind9[20];
		int time_msec;
		char desc[512];
	};

	struct mtktsbts_data *ptr_mtktsbts_data = kmalloc(
					sizeof(*ptr_mtktsbts_data), GFP_KERNEL);

	if (ptr_mtktsbts_data == NULL)
		return -ENOMEM;


	len = (count < (sizeof(ptr_mtktsbts_data->desc) - 1)) ?
				count : (sizeof(ptr_mtktsbts_data->desc) - 1);

	if (copy_from_user(ptr_mtktsbts_data->desc, buffer, len)) {
		kfree(ptr_mtktsbts_data);
		return 0;
	}

	ptr_mtktsbts_data->desc[len] = '\0';

	if (sscanf
	    (ptr_mtktsbts_data->desc,
	     "%d %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d",
		&num_trip,
		&ptr_mtktsbts_data->trip[0], &ptr_mtktsbts_data->t_type[0],
		ptr_mtktsbts_data->bind0,
		&ptr_mtktsbts_data->trip[1], &ptr_mtktsbts_data->t_type[1],
		ptr_mtktsbts_data->bind1,
		&ptr_mtktsbts_data->trip[2], &ptr_mtktsbts_data->t_type[2],
		ptr_mtktsbts_data->bind2,
		&ptr_mtktsbts_data->trip[3], &ptr_mtktsbts_data->t_type[3],
		ptr_mtktsbts_data->bind3,
		&ptr_mtktsbts_data->trip[4], &ptr_mtktsbts_data->t_type[4],
		ptr_mtktsbts_data->bind4,
		&ptr_mtktsbts_data->trip[5], &ptr_mtktsbts_data->t_type[5],
		ptr_mtktsbts_data->bind5,
		&ptr_mtktsbts_data->trip[6], &ptr_mtktsbts_data->t_type[6],
		ptr_mtktsbts_data->bind6,
		&ptr_mtktsbts_data->trip[7], &ptr_mtktsbts_data->t_type[7],
		ptr_mtktsbts_data->bind7,
		&ptr_mtktsbts_data->trip[8], &ptr_mtktsbts_data->t_type[8],
		ptr_mtktsbts_data->bind8,
		&ptr_mtktsbts_data->trip[9], &ptr_mtktsbts_data->t_type[9],
		ptr_mtktsbts_data->bind9,
		&ptr_mtktsbts_data->time_msec) == 32) {

		down(&sem_mutex);
		mtkts_bts_dprintk(
			"[mtkts_bts_write] mtkts_bts_unregister_thermal\n");

		mtkts_bts_unregister_thermal();

		if (num_trip < 0 || num_trip > 10) {
			#ifdef CONFIG_MTK_AEE_FEATURE
			aee_kernel_warning_api(__FILE__, __LINE__,
					DB_OPT_DEFAULT, "mtkts_bts_write",
					"Bad argument");
			#endif
			mtkts_bts_dprintk("[mtkts_bts_write] bad argument\n");
			kfree(ptr_mtktsbts_data);
			up(&sem_mutex);
			return -EINVAL;
		}

		for (i = 0; i < num_trip; i++)
			g_THERMAL_TRIP[i] = ptr_mtktsbts_data->t_type[i];

		g_bind0[0] = g_bind1[0] = g_bind2[0] = g_bind3[0]
			= g_bind4[0] = g_bind5[0] = g_bind6[0]
			= g_bind7[0] = g_bind8[0] = g_bind9[0] = '\0';

		for (i = 0; i < 20; i++) {
			g_bind0[i] = ptr_mtktsbts_data->bind0[i];
			g_bind1[i] = ptr_mtktsbts_data->bind1[i];
			g_bind2[i] = ptr_mtktsbts_data->bind2[i];
			g_bind3[i] = ptr_mtktsbts_data->bind3[i];
			g_bind4[i] = ptr_mtktsbts_data->bind4[i];
			g_bind5[i] = ptr_mtktsbts_data->bind5[i];
			g_bind6[i] = ptr_mtktsbts_data->bind6[i];
			g_bind7[i] = ptr_mtktsbts_data->bind7[i];
			g_bind8[i] = ptr_mtktsbts_data->bind8[i];
			g_bind9[i] = ptr_mtktsbts_data->bind9[i];
		}

		mtkts_bts_dprintk(
			"[mtkts_bts_write] g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,",
			g_THERMAL_TRIP[0], g_THERMAL_TRIP[1],
			g_THERMAL_TRIP[2]);

		mtkts_bts_dprintk(
			"g_THERMAL_TRIP_3=%d,g_THERMAL_TRIP_4=%d,g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,",
			g_THERMAL_TRIP[3], g_THERMAL_TRIP[4],
			g_THERMAL_TRIP[5], g_THERMAL_TRIP[6]);

		mtkts_bts_dprintk(
			"g_THERMAL_TRIP_7=%d,g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
			g_THERMAL_TRIP[7], g_THERMAL_TRIP[8],
			g_THERMAL_TRIP[9]);

		mtkts_bts_dprintk(
			"[mtkts_bts_write] cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,",
			g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);

		mtkts_bts_dprintk(
			"cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
			g_bind5, g_bind6, g_bind7, g_bind8, g_bind9);

		for (i = 0; i < num_trip; i++)
			trip_temp[i] = ptr_mtktsbts_data->trip[i];

		interval = ptr_mtktsbts_data->time_msec / 1000;

		mtkts_bts_dprintk(
			"[mtkts_bts_write] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,",
			trip_temp[0], trip_temp[1], trip_temp[2], trip_temp[3]);

		mtkts_bts_dprintk(
			"trip_4_temp=%d,trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,",
			trip_temp[4], trip_temp[5], trip_temp[6],
			trip_temp[7], trip_temp[8]);

		mtkts_bts_dprintk("trip_9_temp=%d,time_ms=%d\n",
			trip_temp[9], interval * 1000);

		mtkts_bts_dprintk(
			"[mtkts_bts_write] mtkts_bts_register_thermal\n");

		mtkts_bts_register_thermal();
		up(&sem_mutex);
		kfree(ptr_mtktsbts_data);
		/* AP_write_flag=1; */
		return count;
	}

	mtkts_bts_dprintk("[mtkts_bts_write] bad argument\n");
    #ifdef CONFIG_MTK_AEE_FEATURE
	aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT,
							"mtkts_bts_write",
							"Bad argument");
    #endif
	kfree(ptr_mtktsbts_data);
	return -EINVAL;
}

void mtkts_bts_prepare_table(int table_num)
{

	switch (table_num) {
	case 1:		/* AP_NTC_BL197 */
		BTS_Temperature_Table = BTS_Temperature_Table1;
		ntc_tbl_size = sizeof(BTS_Temperature_Table1);
		break;
	case 2:		/* AP_NTC_TSM_1 */
		BTS_Temperature_Table = BTS_Temperature_Table2;
		ntc_tbl_size = sizeof(BTS_Temperature_Table2);
		break;
	case 3:		/* AP_NTC_10_SEN_1 */
		BTS_Temperature_Table = BTS_Temperature_Table3;
		ntc_tbl_size = sizeof(BTS_Temperature_Table3);
		break;
	case 4:		/* AP_NTC_10 */
		BTS_Temperature_Table = BTS_Temperature_Table4;
		ntc_tbl_size = sizeof(BTS_Temperature_Table4);
		break;
	case 5:		/* AP_NTC_47 */
		BTS_Temperature_Table = BTS_Temperature_Table5;
		ntc_tbl_size = sizeof(BTS_Temperature_Table5);
		break;
	case 6:		/* NTCG104EF104F */
		BTS_Temperature_Table = BTS_Temperature_Table6;
		ntc_tbl_size = sizeof(BTS_Temperature_Table6);
		break;
	case 7:		/* NCP15WF104F03RC */
		BTS_Temperature_Table = BTS_Temperature_Table7;
		ntc_tbl_size = sizeof(BTS_Temperature_Table7);
		break;
	default:		/* AP_NTC_10 */
		BTS_Temperature_Table = BTS_Temperature_Table4;
		ntc_tbl_size = sizeof(BTS_Temperature_Table4);
		break;
	}


	pr_notice("[Thermal/TZ/BTS] %s table_num=%d, table_rows=%d\n",
			__func__, table_num,
			(int)(ntc_tbl_size / sizeof(struct BTS_TEMPERATURE)));

#if 0
	{
		int i = 0;

		for (i = 0; i < (ntc_tbl_size
			/ sizeof(struct BTS_TEMPERATURE)); i++) {
			pr_notice(
				"BTS_Temperature_Table[%d].APteryTemp =%d\n", i,
				BTS_Temperature_Table[i].BTS_Temp);
			pr_notice(
				"BTS_Temperature_Table[%d].TemperatureR=%d\n",
				i, BTS_Temperature_Table[i].TemperatureR);
		}
	}
#endif
}

static int mtkts_bts_param_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", g_RAP_pull_up_R);
	seq_printf(m, "%d\n", g_RAP_pull_up_voltage);
	seq_printf(m, "%d\n", g_TAP_over_critical_low);
	seq_printf(m, "%d\n", g_RAP_ntc_table);
	seq_printf(m, "%d\n", g_RAP_ADC_channel);

	return 0;
}


static ssize_t mtkts_bts_param_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	int len = 0;
	struct mtktsbts_param_data {
		char desc[512];
		char pull_R[10], pull_V[10];
		char overcrilow[16];
		char NTC_TABLE[10];
		unsigned int valR, valV, over_cri_low, ntc_table;
		unsigned int adc_channel;
	};

	struct mtktsbts_param_data *ptr_mtktsbts_parm_data;

	ptr_mtktsbts_parm_data = kmalloc(
				sizeof(*ptr_mtktsbts_parm_data), GFP_KERNEL);

	if (ptr_mtktsbts_parm_data == NULL)
		return -ENOMEM;

	/* external pin: 0/1/12/13/14/15, can't use pin:2/3/4/5/6/7/8/9/10/11,
	 *choose "adc_channel=11" to check if there is any param input
	 */
	ptr_mtktsbts_parm_data->adc_channel = 11;

	len = (count < (sizeof(ptr_mtktsbts_parm_data->desc) - 1)) ?
			count : (sizeof(ptr_mtktsbts_parm_data->desc) - 1);

	if (copy_from_user(ptr_mtktsbts_parm_data->desc, buffer, len)) {
		kfree(ptr_mtktsbts_parm_data);
		return 0;
	}

	ptr_mtktsbts_parm_data->desc[len] = '\0';

	mtkts_bts_dprintk("[mtkts_bts_write]\n");

	if (sscanf
	    (ptr_mtktsbts_parm_data->desc, "%9s %d %9s %d %15s %d %9s %d %d",
		ptr_mtktsbts_parm_data->pull_R, &ptr_mtktsbts_parm_data->valR,
		ptr_mtktsbts_parm_data->pull_V, &ptr_mtktsbts_parm_data->valV,
		ptr_mtktsbts_parm_data->overcrilow,
		&ptr_mtktsbts_parm_data->over_cri_low,
		ptr_mtktsbts_parm_data->NTC_TABLE,
		&ptr_mtktsbts_parm_data->ntc_table,
		&ptr_mtktsbts_parm_data->adc_channel) >= 8) {

		if (!strcmp(ptr_mtktsbts_parm_data->pull_R, "PUP_R")) {
			g_RAP_pull_up_R = ptr_mtktsbts_parm_data->valR;
			mtkts_bts_dprintk("g_RAP_pull_up_R=%d\n",
							g_RAP_pull_up_R);
		} else {
			mtkts_bts_printk(
				"[mtkts_bts_write] bad PUP_R argument\n");
			kfree(ptr_mtktsbts_parm_data);
			return -EINVAL;
		}

		if (!strcmp(ptr_mtktsbts_parm_data->pull_V, "PUP_VOLT")) {
			g_RAP_pull_up_voltage = ptr_mtktsbts_parm_data->valV;
			mtkts_bts_dprintk("g_Rat_pull_up_voltage=%d\n",
							g_RAP_pull_up_voltage);
		} else {
			mtkts_bts_printk(
				"[mtkts_bts_write] bad PUP_VOLT argument\n");
			kfree(ptr_mtktsbts_parm_data);
			return -EINVAL;
		}

		if (!strcmp(ptr_mtktsbts_parm_data->overcrilow,
			"OVER_CRITICAL_L")) {
			g_TAP_over_critical_low =
					ptr_mtktsbts_parm_data->over_cri_low;
			mtkts_bts_dprintk("g_TAP_over_critical_low=%d\n",
						g_TAP_over_critical_low);
		} else {
			mtkts_bts_printk(
				"[mtkts_bts_write] bad OVERCRIT_L argument\n");
			kfree(ptr_mtktsbts_parm_data);
			return -EINVAL;
		}

		if (!strcmp(ptr_mtktsbts_parm_data->NTC_TABLE, "NTC_TABLE")) {
			g_RAP_ntc_table = ptr_mtktsbts_parm_data->ntc_table;
			mtkts_bts_dprintk("g_RAP_ntc_table=%d\n",
							g_RAP_ntc_table);
		} else {
			mtkts_bts_printk(
				"[mtkts_bts_write] bad NTC_TABLE argument\n");
			kfree(ptr_mtktsbts_parm_data);
			return -EINVAL;
		}

		/* external pin: 0/1/12/13/14/15,
		 * can't use pin:2/3/4/5/6/7/8/9/10/11,
		 * choose "adc_channel=11" to check if there is any param input
		 */
		if ((ptr_mtktsbts_parm_data->adc_channel >= 2)
		&& (ptr_mtktsbts_parm_data->adc_channel <= 11))
			/* check unsupport pin value, if unsupport,
			 * set channel = 1 as default setting.
			 */
			g_RAP_ADC_channel = AUX_IN0_NTC;
		else {
			/* check if there is any param input,
			 * if not using default g_RAP_ADC_channel:1
			 */
			if (ptr_mtktsbts_parm_data->adc_channel != 11)
				g_RAP_ADC_channel =
					ptr_mtktsbts_parm_data->adc_channel;
			else
				g_RAP_ADC_channel = AUX_IN0_NTC;
		}
		mtkts_bts_dprintk("adc_channel=%d\n",
					ptr_mtktsbts_parm_data->adc_channel);
		mtkts_bts_dprintk("g_RAP_ADC_channel=%d\n",
						g_RAP_ADC_channel);

		mtkts_bts_prepare_table(g_RAP_ntc_table);

		kfree(ptr_mtktsbts_parm_data);
		return count;
	}

	mtkts_bts_printk("[mtkts_bts_write] bad argument\n");
	kfree(ptr_mtktsbts_parm_data);
	return -EINVAL;
}

/* int  mtkts_AP_register_cooler(void)
 * {
 *  cooling devices
 *  cl_dev_sysrst = mtk_thermal_cooling_device_register(
 *  "mtktsAPtery-sysrst", NULL,
 *  &mtkts_AP_cooling_sysrst_ops);
 *  return 0;
 * }
 */

#if 0
static void mtkts_bts_cancel_thermal_timer(void)
{
	/* cancel timer
	 * mtkts_bts_printk("mtkts_bts_cancel_thermal_timer\n");

	 * stop thermal framework polling when entering deep idle

	 *
	 *   We cannot cancel the timer during deepidle and SODI, because
	 *   the battery may suddenly heat up by 3A fast charging.
	 *
	 *
	 *if (thz_dev)
	 *	cancel_delayed_work(&(thz_dev->poll_queue));
	 */
}


static void mtkts_bts_start_thermal_timer(void)
{
	/* mtkts_bts_printk("mtkts_bts_start_thermal_timer\n");
	 * resume thermal framework polling when leaving deep idle
	 *
	 *if (thz_dev != NULL && interval != 0)
	 *	mod_delayed_work(system_freezable_power_efficient_wq,
	 *				&(thz_dev->poll_queue),
	 *				round_jiffies(msecs_to_jiffies(3000)));
	 */
}
#endif

static int mtkts_bts_register_thermal(void)
{
	mtkts_bts_dprintk("[mtkts_bts_register_thermal]\n");

	/* trips : trip 0~1 */
	thz_dev = mtk_thermal_zone_device_register("mtktsAP", num_trip, NULL,
						&mtkts_BTS_dev_ops, 0, 0, 0,
						interval * 1000);

	return 0;
}

/* void mtkts_AP_unregister_cooler(void) */
/* { */
	/* if (cl_dev_sysrst) { */
	/* mtk_thermal_cooling_device_unregister(cl_dev_sysrst); */
	/* cl_dev_sysrst = NULL; */
	/* } */
/* } */
static void mtkts_bts_unregister_thermal(void)
{
	mtkts_bts_dprintk("[mtkts_bts_unregister_thermal]\n");

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static int mtkts_bts_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtkts_bts_read, NULL);
}

static const struct file_operations mtkts_AP_fops = {
	.owner = THIS_MODULE,
	.open = mtkts_bts_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtkts_bts_write,
	.release = single_release,
};


static int mtkts_bts_param_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtkts_bts_param_read, NULL);
}

static const struct file_operations mtkts_AP_param_fops = {
	.owner = THIS_MODULE,
	.open = mtkts_bts_param_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = mtkts_bts_param_write,
	.release = single_release,
};

static int __init mtkts_bts_init(void)
{
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *mtkts_AP_dir = NULL;

	mtkts_bts_dprintk("[mtkts_bts_init]\n");

	/* setup default table */
	mtkts_bts_prepare_table(g_RAP_ntc_table);

	mtkts_AP_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!mtkts_AP_dir) {
		mtkts_bts_dprintk("[%s]: mkdir /proc/driver/thermal failed\n",
								__func__);
	} else {
		entry = proc_create("tzbts", 0664, mtkts_AP_dir,
							&mtkts_AP_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

		entry = proc_create("tzbts_param", 0664, mtkts_AP_dir,
							&mtkts_AP_param_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

	}
#if 0
	mtkTTimer_register("mtktsAP", mtkts_bts_start_thermal_timer,
					mtkts_bts_cancel_thermal_timer);
#endif
	return 0;
}

static void __exit mtkts_bts_exit(void)
{
	mtkts_bts_dprintk("[mtkts_bts_exit]\n");
	mtkts_bts_unregister_thermal();
#if 0
	mtkTTimer_unregister("mtktsAP");
#endif
	/* mtkts_AP_unregister_cooler(); */
}

module_init(mtkts_bts_init);
module_exit(mtkts_bts_exit);
