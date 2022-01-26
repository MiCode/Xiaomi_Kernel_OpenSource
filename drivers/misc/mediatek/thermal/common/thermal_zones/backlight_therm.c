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
#if defined(CONFIG_MEDIATEK_MT6577_AUXADC)
#include <linux/of.h>
#include <linux/iio/consumer.h>
#include <linux/iio/iio.h>
#endif
#include <tscpu_settings.h>
/*=============================================================
 *Weak functions
 *=============================================================
 */
#if !defined(CONFIG_MEDIATEK_MT6577_AUXADC)
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

static unsigned int interval;	/* seconds, 0 : no auto polling */
static int trip_temp[10] = { 120000, 64000, 61000, 58000, 55000,
				52000, 49000, 46000, 43000, 41000 };

static struct thermal_zone_device *thz_dev;
static int backlight_therm_debug_log;
static int kernelmode;
static int g_THERMAL_TRIP[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static int num_trip;
static char g_bind0[20] = {"mtk-cl-shutdown06"};
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

int backlight_therm_cur_temp = 1;


#define MTKTS_BACKLIGHT_THERM_TEMP_CRIT 120000	/* 120.000 degree Celsius */

#define backlight_therm_dprintk(fmt, args...)   \
do {                                    \
	if (backlight_therm_debug_log) {                \
		pr_debug("[Thermal/TZ/BACKLIGHT_THERM]" fmt, ##args); \
	}                                   \
} while (0)


#define backlight_therm_printk(fmt, args...) \
pr_notice("[Thermal/TZ/BACKLIGHT_THERM]" fmt, ##args)

#if defined(CONFIG_MEDIATEK_MT6577_AUXADC)
struct iio_channel *thermistor_ch5;
static int g_ADC_channel;
#endif
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

struct BACKLIGHT_THERM_TEMPERATURE {
	__s32 BACKLIGHT_THERM_Temp;
	__s32 TemperatureR;
};

static int g_RAP_pull_up_R = BACKLIGHT_THERM_RAP_PULL_UP_R;
static int g_TAP_over_critical_low = BACKLIGHT_THERM_TAP_OVER_CRITICAL_LOW;
static int g_RAP_pull_up_voltage = BACKLIGHT_THERM_RAP_PULL_UP_VOLTAGE;
static int g_RAP_ntc_table = BACKLIGHT_THERM_RAP_NTC_TABLE;
static int g_RAP_ADC_channel = BACKLIGHT_THERM_RAP_ADC_CHANNEL;
static int g_backlight_therm_TemperatureR;
/* BACKLIGHT_THERM_TEMPERATURE BACKLIGHT_THERM_Temperature_Table[] = {0}; */

static struct BACKLIGHT_THERM_TEMPERATURE *BACKLIGHT_THERM_Temperature_Table;
static int ntc_tbl_size;

/* AP_NTC_BL197 */
static struct BACKLIGHT_THERM_TEMPERATURE BACKLIGHT_THERM_Temperature_Table1[] = {
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
static struct BACKLIGHT_THERM_TEMPERATURE BACKLIGHT_THERM_Temperature_Table2[] = {
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
static struct BACKLIGHT_THERM_TEMPERATURE BACKLIGHT_THERM_Temperature_Table3[] = {
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
static struct BACKLIGHT_THERM_TEMPERATURE BACKLIGHT_THERM_Temperature_Table4[] = {
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
static struct BACKLIGHT_THERM_TEMPERATURE BACKLIGHT_THERM_Temperature_Table4[] = {
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
static struct BACKLIGHT_THERM_TEMPERATURE BACKLIGHT_THERM_Temperature_Table5[] = {
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
static struct BACKLIGHT_THERM_TEMPERATURE BACKLIGHT_THERM_Temperature_Table6[] = {
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

/* SDNT0603C104F4250FTF(100K) */
static struct BACKLIGHT_THERM_TEMPERATURE BACKLIGHT_THERM_Temperature_Table7[] = {
	{-40, 4229000},
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
	{40, 50770},
	{45, 40930},
	{50, 33190},
	{55, 27050},
	{60, 22160},
	{65, 18250},
	{70, 15110},
	{75, 12590},
	{80, 10510},
	{85, 8814},
	{90, 7421},
	{95, 6283},
	{100, 5335},
	{105, 4546},
	{110, 3887},
	{115, 3339},
	{120, 2878},
	{125, 2493}
};

/* convert register to temperature  */
static __s32 backlight_therm_thermistor_conver_temp(__s32 Res)
{
	int i = 0;
	int asize = 0;
	__s32 RES1 = 0, RES2 = 0;
	__s32 TAP_Value = -200, TMP1 = 0, TMP2 = 0;
#ifdef APPLY_PRECISE_BTS_TEMP
	TAP_Value = TAP_Value * 1000;
#endif
	asize = (ntc_tbl_size / sizeof(struct BACKLIGHT_THERM_TEMPERATURE));

	/* backlight_therm_dprintk("backlight_therm_thermistor_conver_temp() :
	 * asize = %d, Res = %d\n",asize,Res);
	 */
	if (Res >= BACKLIGHT_THERM_Temperature_Table[0].TemperatureR) {
		TAP_Value = -40;	/* min */
#ifdef APPLY_PRECISE_BTS_TEMP
		TAP_Value = TAP_Value * 1000;
#endif
	} else if (Res <= BACKLIGHT_THERM_Temperature_Table[asize - 1].TemperatureR) {
		TAP_Value = 125;	/* max */
#ifdef APPLY_PRECISE_BTS_TEMP
		TAP_Value = TAP_Value * 1000;
#endif
	} else {
		RES1 = BACKLIGHT_THERM_Temperature_Table[0].TemperatureR;
		TMP1 = BACKLIGHT_THERM_Temperature_Table[0].BACKLIGHT_THERM_Temp;
		/* backlight_therm_dprintk("%d : RES1 = %d,TMP1 = %d\n",
		 * __LINE__,RES1,TMP1);
		 */

		for (i = 0; i < asize; i++) {
			if (Res >= BACKLIGHT_THERM_Temperature_Table[i].TemperatureR) {
				RES2 = BACKLIGHT_THERM_Temperature_Table[i].TemperatureR;
				TMP2 = BACKLIGHT_THERM_Temperature_Table[i].BACKLIGHT_THERM_Temp;
				/* backlight_therm_dprintk("%d :i=%d, RES2 = %d,
				 * TMP2 = %d\n",__LINE__,i,RES2,TMP2);
				 */
				break;
			}
			RES1 = BACKLIGHT_THERM_Temperature_Table[i].TemperatureR;
			TMP1 = BACKLIGHT_THERM_Temperature_Table[i].BACKLIGHT_THERM_Temp;
			/* backlight_therm_dprintk("%d :i=%d, RES1 = %d,TMP1 = %d\n",
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
static __s32 mtk_ts_backlight_therm_volt_to_temp(__u32 dwVolt)
{
	__s32 TRes;
	__u64 dwVCriAP = 0;
	__s32 BACKLIGHT_THERM_TMP = -100;
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


	if (dwVolt > ((__u32)dwVCriAP)) {
		TRes = g_TAP_over_critical_low;
	} else {
		/* TRes = (39000*dwVolt) / (1800-dwVolt); */
		/* TRes = (RAP_PULL_UP_R*dwVolt) / (RAP_PULL_UP_VOLT-dwVolt); */
		TRes = (g_RAP_pull_up_R * dwVolt) /
					(g_RAP_pull_up_voltage - dwVolt);
	}
	/* ------------------------------------------------------------------ */
	g_backlight_therm_TemperatureR = TRes;

	/* convert register to temperature */
	BACKLIGHT_THERM_TMP = backlight_therm_thermistor_conver_temp(TRes);

	return BACKLIGHT_THERM_TMP;
}

static int get_hw_backlight_therm_temp(void)
{


#if defined(CONFIG_MEDIATEK_MT6577_AUXADC)
	int val = 0;
	int ret = 0, output;
#else
	int ret = 0, data[4], i, ret_value = 0, ret_temp = 0, output;
	int times = 1, Channel = g_RAP_ADC_channel; /* 6752=0(AUX_IN0_NTC) */
	static int valid_temp;
#endif

#if defined(CONFIG_MEDIATEK_MT6577_AUXADC)
	ret = iio_read_channel_processed(thermistor_ch5, &val);
	if (ret < 0) {
		backlight_therm_printk("Busy/Timeout, IIO ch read failed %d\n", ret);
		return ret;
	}

	/*val * 1500 / 4096*/
	ret = (val * 1500) >> 12;
#else
#if defined(APPLY_AUXADC_CALI_DATA)
	int auxadc_cali_temp;
#endif

	if (IMM_IsAdcInitReady() == 0) {
		backlight_therm_printk(
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
		backlight_therm_dprintk(
			"[thermal_auxadc_get_data(AUX_IN5_NTC)]: ret_temp=%d\n",
			auxadc_cali_temp);
#else
		ret += ret_temp;
		backlight_therm_dprintk(
			"[thermal_auxadc_get_data(AUX_IN5_NTC)]: ret_temp=%d\n",
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
#endif /*CONFIG_MEDIATEK_MT6577_AUXADC*/

	/* ret = ret*1800/4096;//82's ADC power */
	pr_info("backlight_thermtery output mV = %d\n", ret);
	output = mtk_ts_backlight_therm_volt_to_temp(ret);
	pr_info("BACKLIGHT_THERM output temperature = %d\n", output);
	return output;
}

static DEFINE_MUTEX(BACKLIGHT_THERM_lock);
/*int ts_backlight_therm_at_boot_time = 0;*/
int backlight_therm_get_hw_temp(void)
{
	int t_ret = 0;
	int t_ret2 = 0;

	mutex_lock(&BACKLIGHT_THERM_lock);

	/* get HW backlight_therm temp (TSbacklight_therm) */
	/* cat /sys/class/power_supply/backlight_therm/backlight_therm_temp */
	t_ret = get_hw_backlight_therm_temp();
#ifndef APPLY_PRECISE_BTS_TEMP
	t_ret = t_ret * 1000;
#endif
	mutex_unlock(&BACKLIGHT_THERM_lock);

	if ((tsatm_thermal_get_catm_type() == 2) &&
		(tsdctm_thermal_get_ttj_on() == 0)) {
		t_ret2 = wakeup_ta_algo(TA_CATMPLUS_TTJ);

		if (t_ret2 < 0)
			pr_notice("[Thermal/TZ/BACKLIGHT_THERM]wakeup_ta_algo %d\n",
				t_ret2);
	}

	backlight_therm_cur_temp = t_ret;

	if (t_ret > 40000)	/* abnormal high temp */
		backlight_therm_printk("T_backlight_therm=%d\n", t_ret);

	backlight_therm_dprintk("[%s] T_backlight_therm, %d\n", __func__, t_ret);
	return t_ret;
}

static int backlight_therm_get_temp(struct thermal_zone_device *thermal, int *t)
{
	*t = backlight_therm_get_hw_temp();

	/* if ((int) *t > 52000) */
	/* backlight_therm_dprintk("T=%d\n", (int) *t); */

#ifdef CONFIG_LVTS_DYNAMIC_ENABLE_REBOOT
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

static int backlight_therm_bind(
struct thermal_zone_device *thermal, struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		backlight_therm_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		backlight_therm_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		backlight_therm_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		backlight_therm_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		backlight_therm_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		backlight_therm_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		backlight_therm_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		backlight_therm_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		backlight_therm_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		backlight_therm_dprintk("[%s] %s\n", __func__, cdev->type);
	} else {
		return 0;
	}

	if (mtk_thermal_zone_bind_cooling_device(thermal, table_val, cdev)) {
		backlight_therm_dprintk(
			"[%s] error binding cooling dev\n", __func__);
		return -EINVAL;
	}

	backlight_therm_dprintk("[%s] binding OK, %d\n", __func__, table_val);
	return 0;
}

static int backlight_therm_unbind(struct thermal_zone_device *thermal,
			    struct thermal_cooling_device *cdev)
{
	int table_val = 0;

	if (!strcmp(cdev->type, g_bind0)) {
		table_val = 0;
		backlight_therm_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind1)) {
		table_val = 1;
		backlight_therm_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind2)) {
		table_val = 2;
		backlight_therm_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind3)) {
		table_val = 3;
		backlight_therm_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind4)) {
		table_val = 4;
		backlight_therm_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind5)) {
		table_val = 5;
		backlight_therm_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind6)) {
		table_val = 6;
		backlight_therm_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind7)) {
		table_val = 7;
		backlight_therm_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind8)) {
		table_val = 8;
		backlight_therm_dprintk("[%s] %s\n", __func__, cdev->type);
	} else if (!strcmp(cdev->type, g_bind9)) {
		table_val = 9;
		backlight_therm_dprintk("[%s] %s\n", __func__, cdev->type);
	} else
		return 0;

	if (thermal_zone_unbind_cooling_device(thermal, table_val, cdev)) {
		backlight_therm_dprintk(
			"[%s] error unbinding cooling dev\n", __func__);
		return -EINVAL;
	}

	backlight_therm_dprintk("[%s] unbinding OK\n", __func__);
	return 0;
}

static int backlight_therm_get_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode *mode)
{
	*mode = (kernelmode) ? THERMAL_DEVICE_ENABLED : THERMAL_DEVICE_DISABLED;
	return 0;
}

static int backlight_therm_set_mode(
struct thermal_zone_device *thermal, enum thermal_device_mode mode)
{
	kernelmode = mode;
	return 0;
}

static int backlight_therm_get_trip_type(
struct thermal_zone_device *thermal, int trip, enum thermal_trip_type *type)
{
	*type = g_THERMAL_TRIP[trip];
	return 0;
}

static int backlight_therm_get_trip_temp(
struct thermal_zone_device *thermal, int trip, int *temp)
{
	*temp = trip_temp[trip];
	return 0;
}

static int backlight_therm_get_crit_temp(
struct thermal_zone_device *thermal, int *temperature)
{
	*temperature = MTKTS_BACKLIGHT_THERM_TEMP_CRIT;
	return 0;
}

/* bind callback functions to thermalzone */
static struct thermal_zone_device_ops mtkts_BACKLIGHT_THERM_dev_ops = {
	.bind = backlight_therm_bind,
	.unbind = backlight_therm_unbind,
	.get_temp = backlight_therm_get_temp,
	.get_mode = backlight_therm_get_mode,
	.set_mode = backlight_therm_set_mode,
	.get_trip_type = backlight_therm_get_trip_type,
	.get_trip_temp = backlight_therm_get_trip_temp,
	.get_crit_temp = backlight_therm_get_crit_temp,
};



static int backlight_therm_read(struct seq_file *m, void *v)
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

static int backlight_therm_register_thermal(void);
static void backlight_therm_unregister_thermal(void);

static ssize_t backlight_therm_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	int len = 0, i;

	struct backlight_therm_data {
		int trip[10];
		int t_type[10];
		char bind0[20], bind1[20], bind2[20], bind3[20], bind4[20];
		char bind5[20], bind6[20], bind7[20], bind8[20], bind9[20];
		int time_msec;
		char desc[512];
	};

	struct backlight_therm_data *ptr_backlight_therm_data = kmalloc(
					sizeof(*ptr_backlight_therm_data), GFP_KERNEL);

	if (ptr_backlight_therm_data == NULL)
		return -ENOMEM;


	len = (count < (sizeof(ptr_backlight_therm_data->desc) - 1)) ?
				count : (sizeof(ptr_backlight_therm_data->desc) - 1);

	if (copy_from_user(ptr_backlight_therm_data->desc, buffer, len)) {
		kfree(ptr_backlight_therm_data);
		return 0;
	}

	ptr_backlight_therm_data->desc[len] = '\0';

	if (sscanf
	    (ptr_backlight_therm_data->desc,
	     "%d %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d %d %19s %d",
		&num_trip,
		&ptr_backlight_therm_data->trip[0], &ptr_backlight_therm_data->t_type[0],
		ptr_backlight_therm_data->bind0,
		&ptr_backlight_therm_data->trip[1], &ptr_backlight_therm_data->t_type[1],
		ptr_backlight_therm_data->bind1,
		&ptr_backlight_therm_data->trip[2], &ptr_backlight_therm_data->t_type[2],
		ptr_backlight_therm_data->bind2,
		&ptr_backlight_therm_data->trip[3], &ptr_backlight_therm_data->t_type[3],
		ptr_backlight_therm_data->bind3,
		&ptr_backlight_therm_data->trip[4], &ptr_backlight_therm_data->t_type[4],
		ptr_backlight_therm_data->bind4,
		&ptr_backlight_therm_data->trip[5], &ptr_backlight_therm_data->t_type[5],
		ptr_backlight_therm_data->bind5,
		&ptr_backlight_therm_data->trip[6], &ptr_backlight_therm_data->t_type[6],
		ptr_backlight_therm_data->bind6,
		&ptr_backlight_therm_data->trip[7], &ptr_backlight_therm_data->t_type[7],
		ptr_backlight_therm_data->bind7,
		&ptr_backlight_therm_data->trip[8], &ptr_backlight_therm_data->t_type[8],
		ptr_backlight_therm_data->bind8,
		&ptr_backlight_therm_data->trip[9], &ptr_backlight_therm_data->t_type[9],
		ptr_backlight_therm_data->bind9,
		&ptr_backlight_therm_data->time_msec) == 32) {

		down(&sem_mutex);
		backlight_therm_dprintk(
			"[%s] backlight_therm_unregister_thermal\n", __func__);

		backlight_therm_unregister_thermal();

		if (num_trip < 0 || num_trip > 10) {
			#ifdef CONFIG_MTK_AEE_FEATURE
			aee_kernel_warning_api(__FILE__, __LINE__,
					DB_OPT_DEFAULT, "backlight_therm_write",
					"Bad argument");
			#endif
			backlight_therm_dprintk("[%s] bad argument\n", __func__);
			kfree(ptr_backlight_therm_data);
			up(&sem_mutex);
			return -EINVAL;
		}

		for (i = 0; i < num_trip; i++)
			g_THERMAL_TRIP[i] = ptr_backlight_therm_data->t_type[i];

		g_bind0[0] = g_bind1[0] = g_bind2[0] = g_bind3[0]
			= g_bind4[0] = g_bind5[0] = g_bind6[0]
			= g_bind7[0] = g_bind8[0] = g_bind9[0] = '\0';

		for (i = 0; i < 20; i++) {
			g_bind0[i] = ptr_backlight_therm_data->bind0[i];
			g_bind1[i] = ptr_backlight_therm_data->bind1[i];
			g_bind2[i] = ptr_backlight_therm_data->bind2[i];
			g_bind3[i] = ptr_backlight_therm_data->bind3[i];
			g_bind4[i] = ptr_backlight_therm_data->bind4[i];
			g_bind5[i] = ptr_backlight_therm_data->bind5[i];
			g_bind6[i] = ptr_backlight_therm_data->bind6[i];
			g_bind7[i] = ptr_backlight_therm_data->bind7[i];
			g_bind8[i] = ptr_backlight_therm_data->bind8[i];
			g_bind9[i] = ptr_backlight_therm_data->bind9[i];
		}

		backlight_therm_dprintk(
			"[%s] g_THERMAL_TRIP_0=%d,g_THERMAL_TRIP_1=%d,g_THERMAL_TRIP_2=%d,",
			__func__,
			g_THERMAL_TRIP[0], g_THERMAL_TRIP[1],
			g_THERMAL_TRIP[2]);

		backlight_therm_dprintk(
			"g_THERMAL_TRIP_3=%d,g_THERMAL_TRIP_4=%d,g_THERMAL_TRIP_5=%d,g_THERMAL_TRIP_6=%d,",
			g_THERMAL_TRIP[3], g_THERMAL_TRIP[4],
			g_THERMAL_TRIP[5], g_THERMAL_TRIP[6]);

		backlight_therm_dprintk(
			"g_THERMAL_TRIP_7=%d,g_THERMAL_TRIP_8=%d,g_THERMAL_TRIP_9=%d,\n",
			g_THERMAL_TRIP[7], g_THERMAL_TRIP[8],
			g_THERMAL_TRIP[9]);

		backlight_therm_dprintk(
			"[%s] cooldev0=%s,cooldev1=%s,cooldev2=%s,cooldev3=%s,cooldev4=%s,",
			__func__,
			g_bind0, g_bind1, g_bind2, g_bind3, g_bind4);

		backlight_therm_dprintk(
			"cooldev5=%s,cooldev6=%s,cooldev7=%s,cooldev8=%s,cooldev9=%s\n",
			g_bind5, g_bind6, g_bind7, g_bind8, g_bind9);

		for (i = 0; i < num_trip; i++)
			trip_temp[i] = ptr_backlight_therm_data->trip[i];

		interval = ptr_backlight_therm_data->time_msec / 1000;

		backlight_therm_dprintk(
			"[%s] trip_0_temp=%d,trip_1_temp=%d,trip_2_temp=%d,trip_3_temp=%d,",
			__func__,
			trip_temp[0], trip_temp[1], trip_temp[2], trip_temp[3]);

		backlight_therm_dprintk(
			"trip_4_temp=%d,trip_5_temp=%d,trip_6_temp=%d,trip_7_temp=%d,trip_8_temp=%d,",
			trip_temp[4], trip_temp[5], trip_temp[6],
			trip_temp[7], trip_temp[8]);

		backlight_therm_dprintk("trip_9_temp=%d,time_ms=%d\n",
			trip_temp[9], interval * 1000);

		backlight_therm_dprintk(
			"[%s] backlight_therm_register_thermal\n", __func__);

		backlight_therm_register_thermal();
		up(&sem_mutex);
		kfree(ptr_backlight_therm_data);
		/* backlight_therm_write_flag=1; */
		return count;
	}

	backlight_therm_dprintk("[%s] bad argument\n", __func__);
    #ifdef CONFIG_MTK_AEE_FEATURE
	aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT,
							"backlight_therm_write",
							"Bad argument");
    #endif
	kfree(ptr_backlight_therm_data);
	return -EINVAL;
}

void backlight_therm_prepare_table(int table_num)
{

	switch (table_num) {
	case 1:		/* backlight_therm_NTC_BL197 */
		BACKLIGHT_THERM_Temperature_Table = BACKLIGHT_THERM_Temperature_Table1;
		ntc_tbl_size = sizeof(BACKLIGHT_THERM_Temperature_Table1);
		break;
	case 2:		/* backlight_therm_NTC_TSM_1 */
		BACKLIGHT_THERM_Temperature_Table = BACKLIGHT_THERM_Temperature_Table2;
		ntc_tbl_size = sizeof(BACKLIGHT_THERM_Temperature_Table2);
		break;
	case 3:		/* backlight_therm_NTC_10_SEN_1 */
		BACKLIGHT_THERM_Temperature_Table = BACKLIGHT_THERM_Temperature_Table3;
		ntc_tbl_size = sizeof(BACKLIGHT_THERM_Temperature_Table3);
		break;
	case 4:		/* backlight_therm_NTC_10 */
		BACKLIGHT_THERM_Temperature_Table = BACKLIGHT_THERM_Temperature_Table4;
		ntc_tbl_size = sizeof(BACKLIGHT_THERM_Temperature_Table4);
		break;
	case 5:		/* backlight_therm_NTC_47 */
		BACKLIGHT_THERM_Temperature_Table = BACKLIGHT_THERM_Temperature_Table5;
		ntc_tbl_size = sizeof(BACKLIGHT_THERM_Temperature_Table5);
		break;
	case 6:		/* NTCG104EF104F */
		BACKLIGHT_THERM_Temperature_Table = BACKLIGHT_THERM_Temperature_Table6;
		ntc_tbl_size = sizeof(BACKLIGHT_THERM_Temperature_Table6);
		break;
	case 7:		/* NCP15WF104F03RC */
		BACKLIGHT_THERM_Temperature_Table = BACKLIGHT_THERM_Temperature_Table7;
		ntc_tbl_size = sizeof(BACKLIGHT_THERM_Temperature_Table7);
		break;
	default:		/* backlight_therm_NTC_10 */
		BACKLIGHT_THERM_Temperature_Table = BACKLIGHT_THERM_Temperature_Table4;
		ntc_tbl_size = sizeof(BACKLIGHT_THERM_Temperature_Table4);
		break;
	}

	pr_notice("[Thermal/TZ/BACKLIGHT_THERM] %s table_num=%d\n", __func__, table_num);

#if 0
	{
		int i = 0;

		for (i = 0; i < (ntc_tbl_size
			/ sizeof(struct BACKLIGHT_THERM_TEMPERATURE)); i++) {
			pr_notice(
				"BACKLIGHT_THERM_Temperature_Table[%d].backlight_thermteryTemp =%d\n", i,
				BACKLIGHT_THERM_Temperature_Table[i].BACKLIGHT_THERM_Temp);
			pr_notice(
				"BACKLIGHT_THERM_Temperature_Table[%d].TemperatureR=%d\n",
				i, BACKLIGHT_THERM_Temperature_Table[i].TemperatureR);
		}
	}
#endif
}

static int backlight_therm_param_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", g_RAP_pull_up_R);
	seq_printf(m, "%d\n", g_RAP_pull_up_voltage);
	seq_printf(m, "%d\n", g_TAP_over_critical_low);
	seq_printf(m, "%d\n", g_RAP_ntc_table);
#if defined(CONFIG_MEDIATEK_MT6577_AUXADC)
	seq_printf(m, "%d\n", g_ADC_channel);
#else
	seq_printf(m, "%d\n", g_RAP_ADC_channel);
#endif
	return 0;
}


static ssize_t backlight_therm_param_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	int len = 0;
	struct backlight_therm_param_data {
		char desc[512];
		char pull_R[10], pull_V[10];
		char overcrilow[16];
		char NTC_TABLE[10];
		unsigned int valR, valV, over_cri_low, ntc_table;
		unsigned int adc_channel;
	};

	struct backlight_therm_param_data *ptr_backlight_therm_parm_data;

	ptr_backlight_therm_parm_data = kmalloc(
				sizeof(*ptr_backlight_therm_parm_data), GFP_KERNEL);

	if (ptr_backlight_therm_parm_data == NULL)
		return -ENOMEM;

	/* external pin: 0/1/12/13/14/15, can't use pin:2/3/4/5/6/7/8/9/10/11,
	 *choose "adc_channel=11" to check if there is any param input
	 */
	ptr_backlight_therm_parm_data->adc_channel = 11;

	len = (count < (sizeof(ptr_backlight_therm_parm_data->desc) - 1)) ?
			count : (sizeof(ptr_backlight_therm_parm_data->desc) - 1);

	if (copy_from_user(ptr_backlight_therm_parm_data->desc, buffer, len)) {
		kfree(ptr_backlight_therm_parm_data);
		return 0;
	}

	ptr_backlight_therm_parm_data->desc[len] = '\0';

	backlight_therm_dprintk("[%s]\n", __func__);

	if (sscanf
	    (ptr_backlight_therm_parm_data->desc, "%9s %d %9s %d %15s %d %9s %d %d",
		ptr_backlight_therm_parm_data->pull_R, &ptr_backlight_therm_parm_data->valR,
		ptr_backlight_therm_parm_data->pull_V, &ptr_backlight_therm_parm_data->valV,
		ptr_backlight_therm_parm_data->overcrilow,
		&ptr_backlight_therm_parm_data->over_cri_low,
		ptr_backlight_therm_parm_data->NTC_TABLE,
		&ptr_backlight_therm_parm_data->ntc_table,
		&ptr_backlight_therm_parm_data->adc_channel) >= 8) {

		if (!strcmp(ptr_backlight_therm_parm_data->pull_R, "PUP_R")) {
			g_RAP_pull_up_R = ptr_backlight_therm_parm_data->valR;
			backlight_therm_dprintk("g_RAP_pull_up_R=%d\n",
							g_RAP_pull_up_R);
		} else {
			backlight_therm_printk(
				"[%s] bad PUP_R argument\n", __func__);
			kfree(ptr_backlight_therm_parm_data);
			return -EINVAL;
		}

		if (!strcmp(ptr_backlight_therm_parm_data->pull_V, "PUP_VOLT")) {
			g_RAP_pull_up_voltage = ptr_backlight_therm_parm_data->valV;
			backlight_therm_dprintk("g_Rat_pull_up_voltage=%d\n",
							g_RAP_pull_up_voltage);
		} else {
			backlight_therm_printk(
				"[%s] bad PUP_VOLT argument\n", __func__);
			kfree(ptr_backlight_therm_parm_data);
			return -EINVAL;
		}

		if (!strcmp(ptr_backlight_therm_parm_data->overcrilow,
			"OVER_CRITICAL_L")) {
			g_TAP_over_critical_low =
					ptr_backlight_therm_parm_data->over_cri_low;
			backlight_therm_dprintk("g_TAP_over_critical_low=%d\n",
						g_TAP_over_critical_low);
		} else {
			backlight_therm_printk(
				"[%s] bad OVERCRIT_L argument\n", __func__);
			kfree(ptr_backlight_therm_parm_data);
			return -EINVAL;
		}

		if (!strcmp(ptr_backlight_therm_parm_data->NTC_TABLE, "NTC_TABLE")) {
			g_RAP_ntc_table = ptr_backlight_therm_parm_data->ntc_table;
			backlight_therm_dprintk("g_RAP_ntc_table=%d\n",
							g_RAP_ntc_table);
		} else {
			backlight_therm_printk(
				"[%s] bad NTC_TABLE argument\n", __func__);
			kfree(ptr_backlight_therm_parm_data);
			return -EINVAL;
		}

		/* external pin: 0/1/12/13/14/15,
		 * can't use pin:2/3/4/5/6/7/8/9/10/11,
		 * choose "adc_channel=11" to check if there is any param input
		 */
		if ((ptr_backlight_therm_parm_data->adc_channel >= 2)
		&& (ptr_backlight_therm_parm_data->adc_channel <= 11))
			/* check unsupport pin value, if unsupport,
			 * set channel = 1 as default setting.
			 */
			g_RAP_ADC_channel = AUX_IN5_NTC;
		else {
			/* check if there is any param input,
			 * if not using default g_RAP_ADC_channel:1
			 */
			if (ptr_backlight_therm_parm_data->adc_channel != 11)
				g_RAP_ADC_channel =
					ptr_backlight_therm_parm_data->adc_channel;
			else
				g_RAP_ADC_channel = AUX_IN5_NTC;
		}
		backlight_therm_dprintk("adc_channel=%d\n",
					ptr_backlight_therm_parm_data->adc_channel);
		backlight_therm_dprintk("g_RAP_ADC_channel=%d\n",
						g_RAP_ADC_channel);

		backlight_therm_prepare_table(g_RAP_ntc_table);

		kfree(ptr_backlight_therm_parm_data);
		return count;
	}

	backlight_therm_printk("[%s] bad argument\n", __func__);
	kfree(ptr_backlight_therm_parm_data);
	return -EINVAL;
}

/* int  backlight_therm_register_cooler(void)
 * {
 *  cooling devices
 *  cl_dev_sysrst = mtk_thermal_cooling_device_register(
 *  "backlight_thermtery-sysrst", NULL,
 *  &backlight_therm_cooling_sysrst_ops);
 *  return 0;
 * }
 */

#if 0
static void backlight_therm_cancel_thermal_timer(void)
{
	/* cancel timer
	 * backlight_therm_printk("backlight_therm_cancel_thermal_timer\n");

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


static void backlight_therm_start_thermal_timer(void)
{
	/* backlight_therm_printk("backlight_therm_start_thermal_timer\n");
	 * resume thermal framework polling when leaving deep idle
	 *
	 *if (thz_dev != NULL && interval != 0)
	 *	mod_delayed_work(system_freezable_power_efficient_wq,
	 *				&(thz_dev->poll_queue),
	 *				round_jiffies(msecs_to_jiffies(3000)));
	 */
}
#endif

static int backlight_therm_register_thermal(void)
{
	backlight_therm_dprintk("[%s]\n", __func__);

	/* trips : trip 0~1 */
	thz_dev = mtk_thermal_zone_device_register("backlight_therm", num_trip, NULL,
						&mtkts_BACKLIGHT_THERM_dev_ops, 0, 0, 0,
						interval * 1000);

	return 0;
}

/* void backlight_therm_unregister_cooler(void) */
/* { */
	/* if (cl_dev_sysrst) { */
	/* mtk_thermal_cooling_device_unregister(cl_dev_sysrst); */
	/* cl_dev_sysrst = NULL; */
	/* } */
/* } */
static void backlight_therm_unregister_thermal(void)
{
	backlight_therm_dprintk("[%s]\n", __func__);

	if (thz_dev) {
		mtk_thermal_zone_device_unregister(thz_dev);
		thz_dev = NULL;
	}
}

static int backlight_therm_open(struct inode *inode, struct file *file)
{
	return single_open(file, backlight_therm_read, NULL);
}

static const struct file_operations backlight_therm_fops = {
	.owner = THIS_MODULE,
	.open = backlight_therm_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = backlight_therm_write,
	.release = single_release,
};


static int backlight_therm_param_open(struct inode *inode, struct file *file)
{
	return single_open(file, backlight_therm_param_read, NULL);
}

static const struct file_operations backlight_therm_param_fops = {
	.owner = THIS_MODULE,
	.open = backlight_therm_param_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = backlight_therm_param_write,
	.release = single_release,
};

#if defined(CONFIG_MEDIATEK_MT6577_AUXADC)
static int backlight_therm_probe(struct platform_device *pdev)
{
	int err = 0;
	int ret = 0;

	pr_info("[%s]\n", __func__);

	if (!pdev->dev.of_node) {
		backlight_therm_printk("[%s] Only DT based supported\n",
			__func__);
		return -ENODEV;
	}

	thermistor_ch5 = devm_kzalloc(&pdev->dev, sizeof(*thermistor_ch5),
		GFP_KERNEL);
	if (!thermistor_ch5)
		return -ENOMEM;


	thermistor_ch5 = iio_channel_get(&pdev->dev, "thermistor-ch5");
	ret = IS_ERR(thermistor_ch5);
	if (ret) {
		backlight_therm_printk("[%s] fail to get auxadc iio ch5: %d\n",
			__func__, ret);
		return ret;
	}

	g_ADC_channel = thermistor_ch5->channel->channel;
	backlight_therm_printk("[%s]get auxadc iio ch: %d\n", __func__,
		thermistor_ch5->channel->channel);

	return err;
}

#ifdef CONFIG_OF
const struct of_device_id mt_thermistor_of_match6[2] = {
	{.compatible = "mediatek,mtboard-thermistor6",},
	{},
};
#endif

#define THERMAL_THERMISTOR_NAME    "mtboard-thermistor6"
static struct platform_driver mtk_thermal_backlight_therm_driver = {
	.remove = NULL,
	.shutdown = NULL,
	.probe = backlight_therm_probe,
	.suspend = NULL,
	.resume = NULL,
	.driver = {
		.name = THERMAL_THERMISTOR_NAME,
#ifdef CONFIG_OF
		.of_match_table = mt_thermistor_of_match6,
#endif
	},
};
#endif /*CONFIG_MEDIATEK_MT6577_AUXADC*/


static int __init backlight_therm_init(void)
{
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *backlight_therm_dir = NULL;
#if defined(CONFIG_MEDIATEK_MT6577_AUXADC)
	int err = 0;
#endif

	backlight_therm_dprintk("[%s]\n", __func__);


#if defined(CONFIG_MEDIATEK_MT6577_AUXADC)
	err = platform_driver_register(&mtk_thermal_backlight_therm_driver);
	if (err) {
		backlight_therm_printk("thermal driver callback register failed.\n");
		return err;
	}
#endif

	/* setup default table */
	backlight_therm_prepare_table(g_RAP_ntc_table);

	backlight_therm_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!backlight_therm_dir) {
		backlight_therm_dprintk("[%s]: mkdir /proc/driver/thermal failed\n",
								__func__);
	} else {
		entry = proc_create("tzbacklight_therm", 0664, backlight_therm_dir,
							&backlight_therm_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

		entry = proc_create("tzbacklight_therm_param", 0664, backlight_therm_dir,
							&backlight_therm_param_fops);
		if (entry)
			proc_set_user(entry, uid, gid);

	}

	backlight_therm_register_thermal();
#if 0
	mtkTTimer_register("backlight_therm", backlight_therm_start_thermal_timer,
					backlight_therm_cancel_thermal_timer);
#endif
	return 0;
}

static void __exit backlight_therm_exit(void)
{
	backlight_therm_dprintk("[%s]\n", __func__);
	backlight_therm_unregister_thermal();
#if 0
	mtkTTimer_unregister("backlight_therm");
#endif
	/* backlight_therm_unregister_cooler(); */
}

module_init(backlight_therm_init);
module_exit(backlight_therm_exit);
