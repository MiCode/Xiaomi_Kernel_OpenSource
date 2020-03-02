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

/* #define DEBUG 1 */
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
#include <linux/spinlock.h>
#include <mt-plat/sync_write.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include <linux/seq_file.h>
#include <linux/slab.h>
#include "mach/mtk_thermal.h"
#include <linux/bug.h>

#if defined(CONFIG_MTK_CLKMGR)
#include <mach/mtk_clkmgr.h>
#else
#include <linux/clk.h>
#endif

#include <mach/wd_api.h>
#include <mtk_gpu_utility.h>
#include <linux/time.h>

#define __MT_MTK_TS_CPU_C__
#include <tscpu_settings.h>

/* 1: turn on RT kthread for thermal protection in this sw module; 0: turn off */
#if MTK_TS_CPU_RT
#include <linux/sched.h>
#include <linux/kthread.h>
#endif

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#define __MT_MTK_TS_CPU_C__

#include <mt-plat/mtk_devinfo.h>
/*=============================================================
 * Local variable definition
 *=============================================================
 */

/*
* Bank0: CPU		(TS_MCU1)
* Bank1: GPU+SOC+MD     (TS_MCU2)
*/

int tscpu_ts_temp[TS_ENUM_MAX];
int tscpu_ts_temp_r[TS_ENUM_MAX];

/* chip dependent */
/*
* TO-DO: I assume AHB bus frequecy is 78MHz.
* Please confirm it.
*/
/*
 * The tscpu_g_tc structure controls the polling rates and sensor mapping tables
 * of all thermal controllers.
 * If HW thermal controllers are more than you actually needed, you should pay
 * attention to default setting of unneeded thermal controllers.
 * Otherwise, these unneeded thermal controllers will be initialized and work
 * unexpectedly.
*/
struct thermal_controller tscpu_g_tc[THERMAL_CONTROLLER_NUM] = {
	[0] = {
		.ts = {TS_MCU1, TS_MCU2, TS_MCU3, TS_ABB},
		.ts_number = 4,
		.tc_offset = 0x0,
		.tc_speed = {
			0x0000000C,
			0x0001003B,
			0x0000030D
		} /* 4.9ms */
	},
	[1] = {
		.ts = {},
		.ts_number = 0,
		.tc_offset = 0x100,
		.tc_speed = {
			0x0000000C,
			0x0001003B,
			0x0000030D
		} /* 4.9ms */
	}
};

#ifdef CONFIG_OF
const struct of_device_id mt_thermal_of_match[2] = {
	{.compatible = "mediatek,therm_ctrl",},
	{},
};
#endif

int tscpu_debug_log;

#if MTK_TS_CPU_RT
static struct task_struct *ktp_thread_handle;
#endif

static __s32 g_adc_ge_t;
static __s32 g_adc_oe_t;
static __s32 g_o_vtsmcu1;
static __s32 g_o_vtsmcu2;
static __s32 g_o_vtsmcu3;
static __s32 g_o_vtsabb;
static __s32 g_degc_cali;
static __s32 g_adc_cali_en_t;
static __s32 g_o_slope;
static __s32 g_o_slope_sign;
static __s32 g_id;

static __s32 g_ge;
static __s32 g_oe;
static __s32 g_gain;

static __s32 g_x_roomt[TS_ENUM_MAX] = { 0 };

static __u32 calefuse1;
static __u32 calefuse2;
static __u32 calefuse3;

/**
 * If curr_temp >= tscpu_polling_trip_temp1, use interval
 * else if cur_temp >= tscpu_polling_trip_temp2 && curr_temp < tscpu_polling_trip_temp1,
 * use interval*tscpu_polling_factor1
 * else, use interval*tscpu_polling_factor2
 */
/* chip dependent */
int tscpu_polling_trip_temp1 = 40000;
int tscpu_polling_trip_temp2 = 20000;
int tscpu_polling_factor1 = 1;
int tscpu_polling_factor2 = 4;

#if MTKTSCPU_FAST_POLLING
/* Combined fast_polling_trip_temp and fast_polling_factor,
*it means polling_delay will be 1/5 of original interval
*after mtktscpu reports > 65C w/o exit point
*/
int fast_polling_trip_temp = 60000;
int fast_polling_trip_temp_high = 60000; /* deprecaed */
int fast_polling_factor = 2;
int tscpu_cur_fp_factor = 1;
int tscpu_next_fp_factor = 1;
#endif

#if PRECISE_HYBRID_POWER_BUDGET
/*	tscpu_prev_cpu_temp: previous CPUSYS temperature
*	tscpu_curr_cpu_temp: current CPUSYS temperature
*	tscpu_prev_gpu_temp: previous GPUSYS temperature
*	tscpu_curr_gpu_temp: current GPUSYS temperature
 */
int tscpu_prev_cpu_temp = 0, tscpu_prev_gpu_temp = 0;
int tscpu_curr_cpu_temp = 0, tscpu_curr_gpu_temp = 0;
#endif

static int tscpu_curr_max_ts_temp;

#if CFG_THERM_LVTS
__u32 lvts_count1_b30c;
__u32 lvts_count2_b30c;
__u32 lvts_count3_b30c;

__u32 lvts_golden_temp1;
__u32 lvts_golden_temp2;
__u32 lvts_golden_temp3;
#endif

/*=============================================================
 * Local function declartation
 *=============================================================
 */
static __s32 temperature_to_raw_room(__u32 ret, enum thermal_sensor ts_name);
static void set_tc_trigger_hw_protect(int temperature, int temperature2, int tc_num);

/*=============================================================
 *Weak functions
 *=============================================================
 */
void __attribute__ ((weak))
mt_ptp_lock(unsigned long *flags)
{
	pr_notice("[Power/CPU_Thermal]%s doesn't exist\n", __func__);
}

void __attribute__ ((weak))
mt_ptp_unlock(unsigned long *flags)
{
	pr_notice("[Power/CPU_Thermal]%s doesn't exist\n", __func__);
}

int __attribute__ ((weak))
get_wd_api(struct wd_api **obj)
{
	pr_notice("[Power/CPU_Thermal]%s doesn't exist\n", __func__);
	return -1;
}

/*=============================================================*/

/* chip dependent */
int tscpu_thermal_clock_on(void)
{
	int ret = -1;

#if defined(CONFIG_MTK_CLKMGR)
	tscpu_dprintk("%s\n", __func__);
	/* ret = enable_clock(MT_CG_PERI_THERM, "THERMAL"); */
#else
	/* Use CCF instead */
	tscpu_dprintk("%s CCF\n", __func__);
	ret = clk_prepare_enable(therm_main);
	if (ret)
		tscpu_printk("Cannot enable thermal clock.\n");
#endif
	return ret;
}

/* chip dependent */
int tscpu_thermal_clock_off(void)
{
	int ret = -1;

#if defined(CONFIG_MTK_CLKMGR)
	tscpu_dprintk("%s\n", __func__);
	/*ret = disable_clock(MT_CG_PERI_THERM, "THERMAL"); */
#else
	/*Use CCF instead*/
	tscpu_dprintk("%s CCF\n", __func__);
	clk_disable_unprepare(therm_main);
#endif
	return ret;
}

/* TODO: FIXME */
void get_thermal_slope_intercept(struct TS_PTPOD *ts_info, enum thermal_bank_name ts_bank)
{
	unsigned int temp0, temp1, temp2;
	struct TS_PTPOD ts_ptpod;
	__s32 x_roomt;

	tscpu_dprintk("get_thermal_slope_intercept\n");

	/* chip dependent */

	/*
	*   If there are two or more sensors in a bank, choose the sensor calibration value of
	*   the dominant sensor. You can observe it in the thermal doc provided by Thermal DE.
	*   For example,
	*   Bank 1 is for SOC + GPU. Observe all scenarios related to GPU tests to
	*   determine which sensor is the highest temperature in all tests.
	*   Then, It is the dominant sensor.
	*   (Confirmed by Thermal DE Alfred Tsai)
	*/

	switch (ts_bank) {
	case THERMAL_BANK0:
		x_roomt = g_x_roomt[0];
		break;
	case THERMAL_BANK1:
		x_roomt = g_x_roomt[1];
		break;
	default:		/*choose high temp */
		x_roomt = g_x_roomt[0];
		break;
	}

	/*
	*   The equations in this function are confirmed by Thermal DE Alfred Tsai.
	*   Don't have to change until using next generation thermal sensors.
	 */

	temp0 = (10000 * 100000 / g_gain) * 15 / 18;

	if (g_o_slope_sign == 0)
		temp1 = (temp0 * 10) / (1650 + g_o_slope * 10);
	else
		temp1 = (temp0 * 10) / (1650 - g_o_slope * 10);

	ts_ptpod.ts_MTS = temp1;

	temp0 = (g_degc_cali * 10 / 2);
	temp1 = ((10000 * 100000 / 4096 / g_gain) * g_oe + x_roomt * 10) * 15 / 18;

	if (g_o_slope_sign == 0)
		temp2 = temp1 * 100 / (1650 + g_o_slope * 10);
	else
		temp2 = temp1 * 100 / (1650 - g_o_slope * 10);

	ts_ptpod.ts_BTS = (temp0 + temp2 - 250) * 4 / 10;


	ts_info->ts_MTS = ts_ptpod.ts_MTS;
	ts_info->ts_BTS = ts_ptpod.ts_BTS;
	tscpu_dprintk("ts_MTS=%d, ts_BTS=%d\n", ts_ptpod.ts_MTS, ts_ptpod.ts_BTS);
}
EXPORT_SYMBOL(get_thermal_slope_intercept);

/* chip dependent */
void mtkts_dump_cali_info(void)
{
	tscpu_printk("[cal] g_adc_ge_t      = %d\n", g_adc_ge_t);
	tscpu_printk("[cal] g_adc_oe_t      = %d\n", g_adc_oe_t);
	tscpu_printk("[cal] g_degc_cali     = %d\n", g_degc_cali);
	tscpu_printk("[cal] g_adc_cali_en_t = %d\n", g_adc_cali_en_t);
	tscpu_printk("[cal] g_o_slope       = %d\n", g_o_slope);
	tscpu_printk("[cal] g_o_slope_sign  = %d\n", g_o_slope_sign);
	tscpu_printk("[cal] g_id            = %d\n", g_id);

	tscpu_printk("[cal] g_o_vtsmcu1     = %d\n", g_o_vtsmcu1);
	tscpu_printk("[cal] g_o_vtsmcu2     = %d\n", g_o_vtsmcu2);
	tscpu_printk("[cal] g_o_vtsmcu3     = %d\n", g_o_vtsmcu3);
	tscpu_printk("[cal] g_o_vtsabb      = %d\n", g_o_vtsabb);

#if CFG_THERM_LVTS
	tscpu_printk("[cal] lvts_count1_b30c=  %d\n", lvts_count1_b30c);
	tscpu_printk("[cal] lvts_count2_b30c=  %d\n", lvts_count2_b30c);
	tscpu_printk("[cal] lvts_count3_b30c=  %d\n", lvts_count3_b30c);
	tscpu_printk("[cal] lvts_golden_temp1= %d\n", lvts_golden_temp1);
	tscpu_printk("[cal] lvts_golden_temp2= %d\n", lvts_golden_temp2);
	tscpu_printk("[cal] lvts_golden_temp3= %d\n", lvts_golden_temp3);
#endif
}

void eDataCorrector(void)
{
	/* Confirmed with DE Kj Hsiao and DS Lin
	*   ADC_GE_T [9:0]      Default:512   265 ~ 758
	*   ADC_OE_T [9:0]      Default:512   265 ~ 758
	*   O_VTSMCU1(9b)       Default:260   -8 ~ 484
	*   O_VTSMCU2(9b)       Default:260   -8 ~ 484
	*   O_VTSMCU3(9b)       Default:260   -8 to 484
	*   O_VTSABB (9b)       Default:260   -8 ~ 484
	*   DEGC_cali  (6b)     Default:40    1 ~ 63
	*   ADC_CALI_EN_T (1b)
	*   O_SLOPE_SIGN (1b)   Default:0
	*   O_SLOPE (6b)        Default:0
	*   ID (1b)
	*/
	if (g_adc_ge_t < 265 || g_adc_ge_t > 758) {
		tscpu_warn("[thermal] Bad efuse data, g_adc_ge_t\n");
		g_adc_ge_t = 512;
	}
	if (g_adc_oe_t < 265 || g_adc_oe_t > 758) {
		tscpu_warn("[thermal] Bad efuse data, g_adc_oe_t\n");
		g_adc_oe_t = 512;
	}
	if (g_o_vtsmcu1 < -8 || g_o_vtsmcu1 > 484) {
		tscpu_warn("[thermal] Bad efuse data, g_o_vtsmcu1\n");
		g_o_vtsmcu1 = 260;
	}
	if (g_o_vtsmcu2 < -8 || g_o_vtsmcu2 > 484) {
		tscpu_warn("[thermal] Bad efuse data, g_o_vtsmcu2\n");
		g_o_vtsmcu2 = 260;
	}
	if (g_o_vtsmcu3 < -8 || g_o_vtsmcu3 > 484) {
		tscpu_warn("[thermal] Bad efuse data, g_o_vtsmcu3\n");
		g_o_vtsmcu3 = 260;
	}
	if (g_o_vtsabb < -8 || g_o_vtsabb > 484) {
		tscpu_warn("[thermal] Bad efuse data, g_o_vtsabb\n");
		g_o_vtsabb = 260;
	}
	if (g_degc_cali < 1 || g_degc_cali > 63) {
		tscpu_warn("[thermal] Bad efuse data, g_degc_cali\n");
		g_degc_cali = 40;
	}
}
void tscpu_thermal_cal_prepare(void)
{
	__u32 temp0, temp1, temp2;
#if CFG_THERM_LVTS
	__u32 lvtsdevinfo1, lvtsdevinfo2, lvtsdevinfo3;
#endif

	temp0 = get_devinfo_with_index(ADDRESS_INDEX_0);
	temp1 = get_devinfo_with_index(ADDRESS_INDEX_1);
	temp2 = get_devinfo_with_index(ADDRESS_INDEX_2);

	tscpu_printk("[calibration] temp0=0x%x, temp1=0x%x, temp2=0x%x\n", temp0, temp1, temp2);

	/*
	 * chip dependent
	 * ADC_GE_T [9:0]     *(temp0)[19:10]
	 * ADC_OE_T [9:0]     *(temp0)[9:0]
	 */
	g_adc_ge_t = ((temp0 & _BITMASK_(19:10)) >> 10);
	g_adc_oe_t = (temp0 & _BITMASK_(9:0));

	/*
	 * O_VTSMCU1 (9b)     *(temp1)[8:0]
	 * O_VTSMCU2 (9b)     *(temp1)[17:9]
	 * O_VTSMCU3 (9b)     *(temp1)[26:18]
	 * O_VTS ABB(9b)      *(temp2)[8:0]
	 */
	g_o_vtsmcu1 = (temp1 & _BITMASK_(8:0));
	g_o_vtsmcu2 = ((temp1 & _BITMASK_(17:9)) >> 9);
	g_o_vtsmcu3 = ((temp1 & _BITMASK_(26:18)) >> 18);
	g_o_vtsabb = (temp2 & _BITMASK_(8:0));

	/*
	 * DEGC_cali(6b)      *(temp0)[25:20]
	 * ADC_CALI_EN_T (1b) *(temp1)[28]
	 */
	g_degc_cali = ((temp0 & _BITMASK_(25:20)) >> 20);
	g_adc_cali_en_t = ((temp1 & _BIT_(28)) >> 28);

	/*
	 * O_SLOPE_SIGN (1b)  *(temp1)[29]
	 * O_SLOPE (6b)       *(temp0)[31:26]
	 */
	g_o_slope_sign = ((temp1 & _BIT_(29)) >> 29);
	g_o_slope = ((temp0 & _BITMASK_(31:26)) >> 26);

	/* ID (1b)            *(temp1)[27] */
	g_id = ((temp1 & _BIT_(27)) >> 27);

	/*
	*   Check ID bit
	*   If ID=0 (TSMC sample)    , ignore O_SLOPE EFuse value and set O_SLOPE=0.
	*   If ID=1 (non-TSMC sample), read O_SLOPE EFuse value for following calculation.
	*/
	if (g_id == 0)
		g_o_slope = 0;

	if (g_adc_cali_en_t == 1) {
		/*thermal_enable = true; */
		eDataCorrector();
	} else {
		tscpu_warn("This sample is not Thermal calibrated\n");
		g_adc_ge_t = 512;
		g_adc_oe_t = 512;
		g_o_vtsmcu1 = 260;
		g_o_vtsmcu2 = 260;
		g_o_vtsmcu3 = 260;
		g_o_vtsabb = 260;
		g_degc_cali = 40;
		g_o_slope_sign = 0;
		g_o_slope = 0;
	}

#if CFG_THERM_LVTS
	/*
	 * LVTS devinfo:
	 * 0x0x11F1_01C8 --> 118
	 * 0x0x11F1_01CC --> 119
	 * 0x0x11F1_07C4 --> 139
	 */
	lvtsdevinfo1 = get_devinfo_with_index(118);
	lvtsdevinfo2 = get_devinfo_with_index(119);
	lvtsdevinfo3 = get_devinfo_with_index(139);

	tscpu_printk("[lvts_cal] 0: 0x%x, 1: 0x%x, 2: 0x%x\n", lvtsdevinfo1, lvtsdevinfo2, lvtsdevinfo3);

	lvts_count1_b30c = (lvtsdevinfo1 & _BITMASK_(23:0));
	lvts_count2_b30c = (lvtsdevinfo2 & _BITMASK_(23:0));
	lvts_count3_b30c = (lvtsdevinfo3 & _BITMASK_(23:0));

	lvts_golden_temp1 = ((lvtsdevinfo1 & _BITMASK_(31:24)) >> 24);
	lvts_golden_temp2 = ((lvtsdevinfo2 & _BITMASK_(31:24)) >> 24);
	lvts_golden_temp3 = ((lvtsdevinfo3 & _BITMASK_(31:24)) >> 24);
#endif

	mtkts_dump_cali_info();
}

void tscpu_thermal_cal_prepare_2(__u32 ret)
{
	__s32 format[TS_ENUM_MAX] = { 0 };
	int i = 0;

	/* tscpu_printk("tscpu_thermal_cal_prepare_2\n"); */
	g_ge = ((g_adc_ge_t - 512) * 10000) / 4096;	/* ge * 10000 */
	g_oe = (g_adc_oe_t - 512);

	g_gain = (10000 + g_ge);

	format[0] = (g_o_vtsmcu1 + 3350 - g_oe);
	format[1] = (g_o_vtsmcu2 + 3350 - g_oe);
	format[2] = (g_o_vtsmcu3 + 3350 - g_oe);
	format[3] = (g_o_vtsabb + 3350 - g_oe);

	for (i = 0; i < TS_ENUM_MAX; i++)
		g_x_roomt[i] = (((format[i] * 10000) / 4096) * 10000) / g_gain;	/* x_roomt * 10000 */

	tscpu_printk("[T_De][cal] g_ge         = %d\n", g_ge);
	tscpu_printk("[T_De][cal] g_gain       = %d\n", g_gain);

	for (i = 0; i < TS_ENUM_MAX; i++)
		tscpu_printk("[T_De][cal] g_x_roomt%d   = %d\n", i, g_x_roomt[i]);
}

#if THERMAL_CONTROLLER_HW_TP
static __s32 temperature_to_raw_room(__u32 ret, enum thermal_sensor ts_name)
{
	/* Ycurr = [(Tcurr - DEGC_cali/2)*(1650+O_slope*10)/10*(18/15)*(1/10000)+X_roomtabb]*Gain*4096 + OE */

	__s32 t_curr = ret;
	__s32 format_1 = 0;
	__s32 format_2 = 0;
	__s32 format_3 = 0;
	__s32 format_4 = 0;

	/* tscpu_dprintk("temperature_to_raw_room\n"); */

	if (g_o_slope_sign == 0) {	/* O_SLOPE is Positive. */
		format_1 = t_curr - (g_degc_cali * 1000 / 2);
		format_2 = format_1 * (1650 + g_o_slope * 10) / 10 * 18 / 15;
		format_2 = format_2 - 2 * format_2;

		format_3 = format_2 / 1000 + g_x_roomt[ts_name] * 10;
		format_4 = (format_3 * 4096 / 10000 * g_gain) / 100000 + g_oe;
	} else {		/* O_SLOPE is Negative. */
		format_1 = t_curr - (g_degc_cali * 1000 / 2);
		format_2 = format_1 * (1650 - g_o_slope * 10) / 10 * 18 / 15;
		format_2 = format_2 - 2 * format_2;

		format_3 = format_2 / 1000 + g_x_roomt[ts_name] * 10;
		format_4 = (format_3 * 4096 / 10000 * g_gain) / 100000 + g_oe;
	}

	return format_4;
}
#endif

static __s32 raw_to_temperature_roomt(__u32 ret, enum thermal_sensor ts_name)
{
	__s32 t_current = 0;
	__s32 y_curr = ret;
	__s32 format_1 = 0;
	__s32 format_2 = 0;
	__s32 format_3 = 0;
	__s32 format_4 = 0;
	__s32 xtoomt = 0;

	xtoomt = g_x_roomt[ts_name];

	/* tscpu_dprintk("raw_to_temperature_room,ts_num=%d,xtoomt=%d\n",ts_name,xtoomt); */

	if (ret == 0)
		return 0;

	format_1 = ((g_degc_cali * 10) >> 1);
	format_2 = (y_curr - g_oe);

	format_3 = (((((format_2) * 10000) >> 12) * 10000) / g_gain) - xtoomt;
	format_3 = format_3 * 15 / 18;


	if (g_o_slope_sign == 0)
		format_4 = ((format_3 * 1000) / (1650 + g_o_slope * 10));	/* uint = 0.1 deg */
	else
		format_4 = ((format_3 * 1000) / (1650 - g_o_slope * 10));	/* uint = 0.1 deg */

	format_4 = format_4 - (format_4 << 1);

	t_current = format_1 + format_4;	/* uint = 0.1 deg */

	/* tscpu_dprintk("raw_to_temperature_room,t_current=%d\n",t_current); */
	return t_current;
}

/*
* Bank0: CPU		(TS_MCU1)
* Bank1: GPU+SOC+MD     (TS_MCU2)
*/
int get_immediate_none_wrap(void)
{
	return -127000;
}

/* chip dependent */
int get_immediate_cpu_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_MCU1];

	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_gpu_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_MCU2];

	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_soc_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_MCU2];

	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_md_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_MCU2];

	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int (*max_temperature_in_bank[THERMAL_BANK_NUM])(void) = {
	get_immediate_cpu_wrap,
	get_immediate_gpu_wrap
};

/*
* Bank0: CPU		(TS_MCU1)
* Bank1: GPU+SOC+MD     (TS_MCU2)
*/
/* chip dependent */
int get_immediate_ts1_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_MCU1];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts2_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_MCU2];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts3_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_MCU3];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tsabb_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_ABB];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int (*get_immediate_tsX[TS_ENUM_MAX])(void) = {
	get_immediate_ts1_wrap,
	get_immediate_ts2_wrap,
	get_immediate_ts3_wrap,
	get_immediate_tsabb_wrap
};

static void thermal_interrupt_handler(int tc_num)
{
	__u32 ret = 0, offset;

	offset = tscpu_g_tc[tc_num].tc_offset;

	ret = readl(offset + TEMPMONINTSTS);
	mt_reg_sync_writel(ret, offset + TEMPMONINTSTS); /* write to clear interrupt status */
	tscpu_dprintk("[tIRQ] thermal_interrupt_handler,tc_num=0x%08x,ret=0x%08x\n", tc_num, ret);

	if (ret & THERMAL_MON_CINTSTS0)
		tscpu_dprintk("[thermal_isr]: thermal sensor point 0 - cold interrupt trigger\n");

	if (ret & THERMAL_MON_HINTSTS0)
		tscpu_dprintk("[thermal_isr]: thermal sensor point 0 - hot interrupt trigger\n");

	if (ret & THERMAL_MON_HINTSTS1)
		tscpu_dprintk("[thermal_isr]: thermal sensor point 1 - hot interrupt trigger\n");

	if (ret & THERMAL_MON_HINTSTS2)
		tscpu_dprintk("[thermal_isr]: thermal sensor point 2 - hot interrupt trigger\n");

	if (ret & THERMAL_tri_SPM_State0)
		tscpu_dprintk("[thermal_isr]: Thermal state0 to trigger SPM state0\n");

	if (ret & THERMAL_tri_SPM_State1) {
		tscpu_dprintk("[thermal_isr]: Thermal state1 to trigger SPM state1\n");
#if MTK_TS_CPU_RT
		wake_up_process(ktp_thread_handle);
#endif
	}

	if (ret & THERMAL_tri_SPM_State2)
		tscpu_printk("[thermal_isr]: Thermal state2 to trigger SPM state2\n");
}

irqreturn_t tscpu_thermal_all_tc_interrupt_handler(int irq, void *dev_id)
{
	__u32 ret = 0, i, mask = 1;

	ret = readl(THERMINTST);

	ret = ret & 0xFF;

	tscpu_dprintk("thermal_interrupt_handler : THERMINTST = 0x%x\n", ret);

	for (i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++) {
		if (tscpu_g_tc[i].ts_number == 0)
			continue;

		mask = 1 << i;
		if ((ret & mask) == 0)
			thermal_interrupt_handler(i);
	}

	return IRQ_HANDLED;
}

static void thermal_reset_and_initial(int tc_num)
{
	__u32 offset, tempMonCtl1, tempMonCtl2, tempAhbPoll;

	offset = tscpu_g_tc[tc_num].tc_offset;
	tempMonCtl1 = tscpu_g_tc[tc_num].tc_speed.tempMonCtl1;
	tempMonCtl2 = tscpu_g_tc[tc_num].tc_speed.tempMonCtl2;
	tempAhbPoll = tscpu_g_tc[tc_num].tc_speed.tempAhbPoll;

	/* Calculating period unit in Module clock x 256, and the Module clock */
	/* will be changed to 26M when Infrasys enters Sleep mode. */

	/*bus clock 66M counting unit is 12 * 1/66M * 256 = 12 * 3.879us = 46.545 us */
	mt_reg_sync_writel(tempMonCtl1, offset + TEMPMONCTL1);
	/*
	*filt interval is 1 * 46.545us = 46.545us,
	*sen interval is 429 * 46.545us = 19.968ms
	*/
	mt_reg_sync_writel(tempMonCtl2, offset + TEMPMONCTL2);
	/*AHB polling is 781* 1/66M = 11.833us*/
	mt_reg_sync_writel(tempAhbPoll, offset + TEMPAHBPOLL);

#if THERMAL_CONTROLLER_HW_FILTER == 2
	mt_reg_sync_writel(0x00000492, offset + TEMPMSRCTL0);	/* temperature sampling control, 2 out of 4 samples */
#elif THERMAL_CONTROLLER_HW_FILTER == 4
	mt_reg_sync_writel(0x000006DB, offset + TEMPMSRCTL0);	/* temperature sampling control, 4 out of 6 samples */
#elif THERMAL_CONTROLLER_HW_FILTER == 8
	mt_reg_sync_writel(0x00000924, offset + TEMPMSRCTL0);	/* temperature sampling control, 8 out of 10 samples */
#elif THERMAL_CONTROLLER_HW_FILTER == 16
	mt_reg_sync_writel(0x00000B6D, offset + TEMPMSRCTL0);	/* temperature sampling control, 16 out of 18 samples */
#else				/* default 1 */
	mt_reg_sync_writel(0x00000000, offset + TEMPMSRCTL0);	/* temperature sampling control, 1 sample */
#endif

	mt_reg_sync_writel(0xFFFFFFFF, offset + TEMPAHBTO);	/* exceed this polling time, IRQ would be inserted */

	mt_reg_sync_writel(0x00000000, offset + TEMPMONIDET0);	/* times for interrupt occurrance for SP0*/
	mt_reg_sync_writel(0x00000000, offset + TEMPMONIDET1);	/* times for interrupt occurrance for SP1*/
	mt_reg_sync_writel(0x00000000, offset + TEMPMONIDET2);	/* times for interrupt occurrance for SP2*/

	mt_reg_sync_writel(0x800, offset + TEMPADCMUX);
	mt_reg_sync_writel((__u32) AUXADC_CON1_CLR_P,
		offset + TEMPADCMUXADDR);	/* AHB address for auxadc mux selection */

	mt_reg_sync_writel(0x800, offset + TEMPADCEN);	/* AHB value for auxadc enable */

	/*
	 *AHB address for auxadc enable (channel 0 immediate mode selected)
	 *this value will be stored to TEMPADCENADDR automatically by hw
	 */
	mt_reg_sync_writel((__u32) AUXADC_CON1_SET_P, offset + TEMPADCENADDR);


	mt_reg_sync_writel((__u32) AUXADC_DAT11_P,
		offset + TEMPADCVALIDADDR);	/* AHB address for auxadc valid bit */
	mt_reg_sync_writel((__u32) AUXADC_DAT11_P,
		offset + TEMPADCVOLTADDR);	/* AHB address for auxadc voltage output */


	mt_reg_sync_writel(0x0, offset + TEMPRDCTRL);	/* read valid & voltage are at the same register */
	/* indicate where the valid bit is (the 12th bit is valid bit and 1 is valid) */
	mt_reg_sync_writel(0x0000002C, offset + TEMPADCVALIDMASK);
	mt_reg_sync_writel(0x0, offset + TEMPADCVOLTAGESHIFT);	/* do not need to shift */
}

/**
 *  temperature2 to set the middle threshold for interrupting CPU. -275000 to disable it.
 */
static void set_tc_trigger_hw_protect(int temperature, int temperature2, int tc_num)
{
	int temp = 0;
	int raw_high;
	enum thermal_sensor ts_name;
	__u32 offset;

	offset = tscpu_g_tc[tc_num].tc_offset;

	/* temperature2=80000;  test only */
	tscpu_dprintk("set_tc_trigger_hw_protect t1=%d t2=%d\n", temperature, temperature2);

	ts_name = tscpu_g_tc[tc_num].ts[0];

	/* temperature to trigger SPM state2 */
	raw_high = temperature_to_raw_room(temperature, ts_name);

	temp = readl(offset + TEMPMONINT);
	mt_reg_sync_writel(temp & 0x00000000, offset + TEMPMONINT);	/* disable trigger SPM interrupt */


	mt_reg_sync_writel(0x20000, offset + TEMPPROTCTL);	/* set hot to wakeup event control */

	mt_reg_sync_writel(raw_high, offset + TEMPPROTTC);	/* set hot to HOT wakeup event */

	mt_reg_sync_writel(temp | 0x80000000, offset + TEMPMONINT);	/* enable trigger Hot SPM interrupt */
}

static int read_tc_raw_and_temp(u32 *tempmsr_name, enum thermal_sensor ts_name)
{
	int temp = 0, raw = 0;

	if (tempmsr_name == 0)
		return 0;

	raw = readl((tempmsr_name)) & 0x0fff;
	temp = raw_to_temperature_roomt(raw, ts_name);

	tscpu_dprintk("read_tc_raw_temp,ts_raw=%d,temp=%d\n", raw, temp * 100);
	tscpu_ts_temp_r[ts_name] = raw;

	return temp * 100;
}

void tscpu_thermal_read_tc_temp(int tc_num, enum thermal_sensor type, int order)
{
	__u32 offset;

	tscpu_dprintk("%s tc_num %d type %d order %d\n", __func__, tc_num, type, order);
	offset = tscpu_g_tc[tc_num].tc_offset;

	switch (order) {
	case 0:
		tscpu_ts_temp[type] =
		    read_tc_raw_and_temp((u32 *)(offset + TEMPMSR0), type);
		tscpu_dprintk("%s order %d tc_num %d type %d temp %d\n",
			      __func__, order, tc_num, type, tscpu_ts_temp[type]);
		break;
	case 1:
		tscpu_ts_temp[type] =
		    read_tc_raw_and_temp((u32 *)(offset + TEMPMSR1), type);
		tscpu_dprintk("%s order %d tc_num %d type %d temp %d\n",
			      __func__, order, tc_num, type, tscpu_ts_temp[type]);
		break;
	case 2:
		tscpu_ts_temp[type] =
		    read_tc_raw_and_temp((u32 *)(offset + TEMPMSR2), type);
		tscpu_dprintk("%s order %d tc_num %d type %d temp %d\n",
			      __func__, order, tc_num, type, tscpu_ts_temp[type]);
		break;
	case 3:
		tscpu_ts_temp[type] =
		    read_tc_raw_and_temp((u32 *)(offset + TEMPMSR3), type);
		tscpu_dprintk("%s order %d tc_num %d type %d temp %d\n",
			      __func__, order, tc_num, type, tscpu_ts_temp[type]);
		break;
	default:
		tscpu_ts_temp[type] =
		    read_tc_raw_and_temp((u32 *)(offset + TEMPMSR0), type);
		tscpu_dprintk("%s order %d tc_num %d type %d temp %d\n",
			      __func__, order, tc_num, type, tscpu_ts_temp[type]);
		break;
	}
}

int tscpu_thermal_fast_init(int tc_num)
{
	__u32 temp = 0, cunt = 0, offset = 0;

	offset = tscpu_g_tc[tc_num].tc_offset;

	temp = THERMAL_INIT_VALUE;
	mt_reg_sync_writel((0x00001000 + temp), PTPSPARE2);	/* write temp to spare register */

	mt_reg_sync_writel(1, offset + TEMPMONCTL1);	/* counting unit is 320 * 31.25us = 10ms */
	mt_reg_sync_writel(1, offset + TEMPMONCTL2);	/* sensing interval is 200 * 10ms = 2000ms */
	mt_reg_sync_writel(1, offset + TEMPAHBPOLL);	/* polling interval to check if temperature sense is ready */

	mt_reg_sync_writel(0x000000FF, offset + TEMPAHBTO);	/* exceed this polling time, IRQ would be inserted */
	mt_reg_sync_writel(0x00000000, offset + TEMPMONIDET0);	/* times for interrupt occurrance */
	mt_reg_sync_writel(0x00000000, offset + TEMPMONIDET1);	/* times for interrupt occurrance */

	mt_reg_sync_writel(0x0000000, offset + TEMPMSRCTL0);	/* temperature measurement sampling control */

	/* this value will be stored to TEMPPNPMUXADDR (TEMPSPARE0) automatically by hw */
	mt_reg_sync_writel(0x1, offset + TEMPADCPNP0);
	mt_reg_sync_writel(0x2, offset + TEMPADCPNP1);
	mt_reg_sync_writel(0x3, offset + TEMPADCPNP2);
	mt_reg_sync_writel(0x4, offset + TEMPADCPNP3);

	mt_reg_sync_writel((__u32) PTPSPARE0_P, offset + TEMPPNPMUXADDR); /* AHB address for pnp sensor mux selection */
	mt_reg_sync_writel((__u32) PTPSPARE0_P, offset + TEMPADCMUXADDR); /* AHB address for auxadc mux selection */
	mt_reg_sync_writel((__u32) PTPSPARE1_P, offset + TEMPADCENADDR); /* AHB address for auxadc enable */
	mt_reg_sync_writel((__u32) PTPSPARE2_P, offset + TEMPADCVALIDADDR); /* AHB address for auxadc valid bit */
	mt_reg_sync_writel((__u32) PTPSPARE2_P, offset + TEMPADCVOLTADDR); /* AHB address for auxadc voltage output */

	mt_reg_sync_writel(0x0, offset + TEMPRDCTRL);	/* read valid & voltage are at the same register */
	/* indicate where the valid bit is (the 12th bit is valid bit and 1 is valid) */
	mt_reg_sync_writel(0x0000002C, offset + TEMPADCVALIDMASK);
	mt_reg_sync_writel(0x0, offset + TEMPADCVOLTAGESHIFT);	/* do not need to shift */
	mt_reg_sync_writel(0x3, offset + TEMPADCWRITECTRL);	/* enable auxadc mux & pnp write transaction */


	/* enable all interrupt except filter sense and immediate sense interrupt */
	mt_reg_sync_writel(0x00000000, offset + TEMPMONINT);


	mt_reg_sync_writel(0x0000000F, offset + TEMPMONCTL0); /* enable all sensing point(sensing point 2 is unused)*/


	cunt = 0;
	temp = readl(offset + TEMPMSR0) & 0x0fff;
	while (temp != THERMAL_INIT_VALUE && cunt < 20) {
		cunt++;
		/* pr_debug("[Power/CPU_Thermal]0 temp=%d,cunt=%d\n",temp,cunt); */
		temp = readl(offset + TEMPMSR0) & 0x0fff;
	}

	cunt = 0;
	temp = readl(offset + TEMPMSR1) & 0x0fff;
	while (temp != THERMAL_INIT_VALUE && cunt < 20) {
		cunt++;
		/* pr_debug("[Power/CPU_Thermal]1 temp=%d,cunt=%d\n",temp,cunt); */
		temp = readl(offset + TEMPMSR1) & 0x0fff;
	}

	cunt = 0;
	temp = readl(offset + TEMPMSR2) & 0x0fff;
	while (temp != THERMAL_INIT_VALUE && cunt < 20) {
		cunt++;
		/* pr_debug("[Power/CPU_Thermal]2 temp=%d,cunt=%d\n",temp,cunt); */
		temp = readl(offset + TEMPMSR2) & 0x0fff;
	}

	cunt = 0;
	temp = readl(offset + TEMPMSR3) & 0x0fff;
	while (temp != THERMAL_INIT_VALUE && cunt < 20) {
		cunt++;
		/* pr_debug("[Power/CPU_Thermal]3 temp=%d,cunt=%d\n",temp,cunt); */
		temp = readl(offset + TEMPMSR3) & 0x0fff;
	}

	return 0;
}

int tscpu_thermal_ADCValueOfMcu(enum thermal_sensor type)
{
	switch (type) {
	case TS_MCU1:
		return TEMPADC_MCU1;
	case TS_MCU2:
		return TEMPADC_MCU2;
	case TS_MCU3:
		return TEMPADC_MCU3;
	case TS_ABB:
		return TEMPADC_ABB;
	default:
		return TEMPADC_MCU1;
	}
}

/* pause ALL periodoc temperature sensing point */
void thermal_pause_all_periodoc_temp_sensing(void)
{
	int i, temp, offset;
	/* tscpu_printk("thermal_pause_all_periodoc_temp_sensing\n"); */

	/*config bank0,1,2 */
	for (i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++) {
		if (tscpu_g_tc[i].ts_number == 0)
			continue;

		offset = tscpu_g_tc[i].tc_offset;
		temp = readl(offset + TEMPMSRCTL1);
		/* set bit8=bit1=bit2=bit3=1 to pause sensing point 0,1,2,3 */
		mt_reg_sync_writel((temp | 0x10E), offset + TEMPMSRCTL1);
	}
}

/* release ALL periodoc temperature sensing point */
void thermal_release_all_periodoc_temp_sensing(void)
{
	int i = 0, temp;
	__u32 offset;

	/* tscpu_printk("thermal_release_all_periodoc_temp_sensing\n"); */

	/*config bank0,1,2 */
	for (i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++) {
		if (tscpu_g_tc[i].ts_number == 0)
			continue;

		offset = tscpu_g_tc[i].tc_offset;

		temp = readl(offset + TEMPMSRCTL1);
		/* set bit1=bit2=bit3=bit8=0 to release sensing point 0,1,2,3*/
		mt_reg_sync_writel(((temp & (~0x10E))), offset + TEMPMSRCTL1);
	}
}

static void tscpu_thermal_enable_all_periodoc_sensing_point(int tc_num)
{
	__u32 offset;

	offset = tscpu_g_tc[tc_num].tc_offset;

	switch (tscpu_g_tc[tc_num].ts_number) {
	case 1:
		/* enable periodoc temperature sensing point 0 */
		mt_reg_sync_writel(0x00000001, offset + TEMPMONCTL0);
		break;
	case 2:
		/* enable periodoc temperature sensing point 0,1 */
		mt_reg_sync_writel(0x00000003, offset + TEMPMONCTL0);
		break;
	case 3:
		/* enable periodoc temperature sensing point 0,1,2 */
		mt_reg_sync_writel(0x00000007, offset + TEMPMONCTL0);
		break;
	case 4:
		/* enable periodoc temperature sensing point 0,1,2,3*/
		mt_reg_sync_writel(0x0000000F, offset + TEMPMONCTL0);
		break;
	default:
		tscpu_printk("Error at %s\n", __func__);
		break;
	}
}

/* disable ALL periodoc temperature sensing point */
void thermal_disable_all_periodoc_temp_sensing(void)
{
	int i = 0, offset;

	/* tscpu_printk("thermal_disable_all_periodoc_temp_sensing\n"); */
	for (i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++) {
		if (tscpu_g_tc[i].ts_number == 0)
			continue;

		offset = tscpu_g_tc[i].tc_offset;
		/* tscpu_printk("thermal_disable_all_periodoc_temp_sensing:Bank_%d\n",i); */
		mt_reg_sync_writel(0x00000000, offset + TEMPMONCTL0);
	}
}

static void tscpu_thermal_tempADCPNP(int tc_num, int adc, int order)
{
	__u32 offset;

	offset = tscpu_g_tc[tc_num].tc_offset;

	tscpu_dprintk("%s adc %x, order %d\n", __func__, adc, order);

	switch (order) {
	case 0:
		mt_reg_sync_writel(adc, offset + TEMPADCPNP0);
		break;
	case 1:
		mt_reg_sync_writel(adc, offset + TEMPADCPNP1);
		break;
	case 2:
		mt_reg_sync_writel(adc, offset + TEMPADCPNP2);
		break;
	case 3:
		mt_reg_sync_writel(adc, offset + TEMPADCPNP3);
		break;
	default:
		mt_reg_sync_writel(adc, offset + TEMPADCPNP0);
		break;
	}
}

void tscpu_thermal_initial_all_tc(void)
{
	int i, j = 0;
	__u32 temp = 0, offset;

	/* AuxADC Initialization,ref MT6592_AUXADC.doc  */
	temp = readl(AUXADC_CON0_V);	/* Auto set enable for CH11 */
	temp &= 0xFFFFF7FF;	/* 0: Not AUTOSET mode */
	mt_reg_sync_writel(temp, AUXADC_CON0_V);	/* disable auxadc channel 11 synchronous mode */
	mt_reg_sync_writel(0x800, AUXADC_CON1_CLR_V);	/* disable auxadc channel 11 immediate mode */

	for (i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++) {
		if (tscpu_g_tc[i].ts_number == 0)
			continue;

		offset = tscpu_g_tc[i].tc_offset;
		thermal_reset_and_initial(i);

		for (j = 0; j < tscpu_g_tc[i].ts_number; j++)
			tscpu_thermal_tempADCPNP(i, tscpu_thermal_ADCValueOfMcu
					(tscpu_g_tc[i].ts[j]), j);

		mt_reg_sync_writel(TS_CONFIGURE_P, offset + TEMPPNPMUXADDR);
		mt_reg_sync_writel(0x3, offset + TEMPADCWRITECTRL);
	}

	mt_reg_sync_writel(0x800, AUXADC_CON1_SET_V);

	for (i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++) {
		if (tscpu_g_tc[i].ts_number == 0)
			continue;

		tscpu_thermal_enable_all_periodoc_sensing_point(i);
	}
}

void tscpu_config_all_tc_hw_protect(int temperature, int temperature2)
{
	int i = 0;
	int wd_api_ret;
	struct wd_api *wd_api;

	tscpu_dprintk("tscpu_config_all_tc_hw_protect,temperature=%d,temperature2=%d,\n",
		      temperature, temperature2);

#if THERMAL_PERFORMANCE_PROFILE
	struct timeval begin, end;
	unsigned long val;

	do_gettimeofday(&begin);
#endif
	/*spend 860~1463 us */
	/*Thermal need to config to direct reset mode
	*this API provide by Weiqi Fu(RGU SW owner).
	*/

	wd_api_ret = get_wd_api(&wd_api);
	if (wd_api_ret >= 0) {
		wd_api->wd_thermal_direct_mode_config(WD_REQ_DIS, WD_REQ_RST_MODE);	/* reset mode */
	} else {
		tscpu_warn("%d FAILED TO GET WD API\n", __LINE__);
		WARN_ON_ONCE(1);
	}

#if THERMAL_PERFORMANCE_PROFILE
	do_gettimeofday(&end);

	/* Get milliseconds */
	pr_debug("resume time spent, sec : %lu , usec : %lu\n", (end.tv_sec - begin.tv_sec),
	       (end.tv_usec - begin.tv_usec));
#endif

	for (i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++) {
		if (tscpu_g_tc[i].ts_number == 0)
			continue;
		set_tc_trigger_hw_protect(temperature, temperature2, i); /* Move thermal HW protection ahead... */
	}

	/*Thermal need to config to direct reset mode
	*  this API provide by Weiqi Fu(RGU SW owner).
	*/
	if (wd_api_ret >= 0) {
		wd_api->wd_thermal_direct_mode_config(WD_REQ_EN, WD_REQ_RST_MODE);	/* reset mode */
	} else {
		tscpu_warn("%d FAILED TO GET WD API\n", __LINE__);
		WARN_ON_ONCE(1);
	}
}

void tscpu_reset_thermal(void)
{
	/* chip dependent, Have to confirm with DE */

	int temp = 0;
	/* reset thremal ctrl */
	temp = readl(INFRA_GLOBALCON_RST_0_SET); /* TODO: Is it necessary to read INFRA_GLOBALCON_RST_0_SET? */
	temp |= 0x00000001;	/* 1: Enables thermal control software reset */
	mt_reg_sync_writel(temp, INFRA_GLOBALCON_RST_0_SET);

	/* TODO: How long to set the reset bit? */

	/* un reset */
	temp = readl(INFRA_GLOBALCON_RST_0_CLR); /* TODO: Is it necessary to read INFRA_GLOBALCON_RST_0_CLR? */
	temp |= 0x00000001;	/* 1: Enable reset Disables thermal control software reset */
	mt_reg_sync_writel(temp, INFRA_GLOBALCON_RST_0_CLR);
}

int tscpu_read_temperature_info(struct seq_file *m, void *v)
{
	int i;

	seq_printf(m, "current temp:%d\n", tscpu_curr_max_ts_temp);
	seq_printf(m, "[cal] g_adc_ge_t      = %d\n", g_adc_ge_t);
	seq_printf(m, "[cal] g_adc_oe_t      = %d\n", g_adc_oe_t);
	seq_printf(m, "[cal] g_degc_cali     = %d\n", g_degc_cali);
	seq_printf(m, "[cal] g_adc_cali_en_t = %d\n", g_adc_cali_en_t);
	seq_printf(m, "[cal] g_o_slope       = %d\n", g_o_slope);
	seq_printf(m, "[cal] g_o_slope_sign  = %d\n", g_o_slope_sign);
	seq_printf(m, "[cal] g_id            = %d\n", g_id);

	seq_printf(m, "[cal] g_o_vtsmcu1     = %d\n", g_o_vtsmcu1);
	seq_printf(m, "[cal] g_o_vtsmcu2     = %d\n", g_o_vtsmcu2);
	seq_printf(m, "[cal] g_o_vtsmcu3     = %d\n", g_o_vtsmcu3);
	seq_printf(m, "[cal] g_o_vtsabb     = %d\n", g_o_vtsabb);

	seq_printf(m, "[cal] g_ge         = %d\n", g_ge);
	seq_printf(m, "[cal] g_gain       = %d\n", g_gain);

	for (i = 0; i < TS_ENUM_MAX; i++)
		seq_printf(m, "[cal] g_x_roomt%d   = %d\n", i, g_x_roomt[i]);

	seq_printf(m, "calefuse1:0x%x\n", calefuse1);
	seq_printf(m, "calefuse2:0x%x\n", calefuse2);
	seq_printf(m, "calefuse3:0x%x\n", calefuse3);

	return 0;
}

int tscpu_get_curr_temp(void)
{
	tscpu_update_tempinfo();

#if PRECISE_HYBRID_POWER_BUDGET
	/*	update CPU/GPU temp data whenever TZ times out...
	*	If the update timing is aligned to TZ polling,
	*	this segment should be moved to TZ code instead of thermal controller driver
	*/
	tscpu_prev_cpu_temp = tscpu_curr_cpu_temp;
	tscpu_prev_gpu_temp = tscpu_curr_gpu_temp;

	/* It is platform dependent which TS is better to present CPU/GPU temperature */
	tscpu_curr_cpu_temp = tscpu_ts_temp[TS_MCU1];

	tscpu_curr_gpu_temp = tscpu_ts_temp[TS_MCU2];
#endif
	/* though tscpu_max_temperature is common, put it in mtk_ts_cpu.c is weird. */
	tscpu_curr_max_ts_temp = tscpu_max_temperature();

	return tscpu_curr_max_ts_temp;
}

/**
 * this only returns latest stored max ts temp but not updated from TC.
 */
int tscpu_get_curr_max_ts_temp(void)
{
	return tscpu_curr_max_ts_temp;
}

#ifdef CONFIG_OF
int get_io_reg_base(void)
{
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,therm_ctrl");
	WARN_ON_ONCE(node == 0);
	if (node) {
		/* Setup IO addresses */
		thermal_base = of_iomap(node, 0);
		/* tscpu_printk("[THERM_CTRL] thermal_base=0x%p\n",thermal_base); */
	}

	/*get thermal irq num */
	thermal_irq_number = irq_of_parse_and_map(node, 0);
	/*tscpu_printk("[THERM_CTRL] thermal_irq_number=%d\n", thermal_irq_number);*/
	if (!thermal_irq_number) {
		/*TODO: need check "irq number"*/
		tscpu_printk("[THERM_CTRL] get irqnr failed=%d\n", thermal_irq_number);
		return 0;
	}

	if (of_property_read_u32_index(node, "reg", 1, &thermal_phy_base)) {
		tscpu_printk("[THERM_CTRL] config error thermal_phy_base\n");
		return 0;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,auxadc");
	WARN_ON_ONCE(node == 0);
	if (node) {
		/* Setup IO addresses */
		auxadc_ts_base = of_iomap(node, 0);
		/*tscpu_printk("[THERM_CTRL] auxadc_ts_base=0x%p\n",auxadc_ts_base); */
	}


	if (of_property_read_u32_index(node, "reg", 1, &auxadc_ts_phy_base)) {
		tscpu_printk("[THERM_CTRL] config error auxadc_ts_phy_base\n");
		return 0;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,infracfg_ao");
	WARN_ON_ONCE(node == 0);
	if (node) {
		/* Setup IO addresses */
		infracfg_ao_base = of_iomap(node, 0);
		/*tscpu_printk("[THERM_CTRL] infracfg_ao_base=0x%p\n",infracfg_ao_base); */
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,apmixed");
	WARN_ON_ONCE(node == 0);
	if (node) {
		/* Setup IO addresses */
		th_apmixed_base = of_iomap(node, 0);
		/*tscpu_printk("[THERM_CTRL] apmixed_base=0x%p\n", th_apmixed_base); */
	}

	if (of_property_read_u32_index(node, "reg", 1, &apmixed_phy_base)) {
		tscpu_printk("[THERM_CTRL] config error apmixed_phy_base\n");
		return 0;
	}

#if THERMAL_GET_AHB_BUS_CLOCK
	/* TODO: If this is required, it needs to confirm which node to read. */
	node = of_find_compatible_node(NULL, NULL, "mediatek,infrasys"); /**/
	if (!node) {
		pr_err("[CLK_INFRACFG_AO] find node failed\n");
		return 0;
	}

	therm_clk_infracfg_ao_base = of_iomap(node, 0);
	if (!therm_clk_infracfg_ao_base) {
		pr_err("[CLK_INFRACFG_AO] base failed\n");
		return 0;
	}
#endif
	return 1;
}
#endif

#if THERMAL_GET_AHB_BUS_CLOCK
void thermal_get_AHB_clk_info(void)
{
	int cg = 0, dcm = 0, cg_freq = 0;
	int clockSource = 136, ahbClk = 0; /*Need to confirm with DE*/

	cg = readl(THERMAL_CG);
	dcm = readl(THERMAL_DCM);

	/*The following rule may change, you need to confirm with DE.
	*  These are for mt6759, and confirmed with DE Justin Gu
	*/
	cg_freq = dcm & _BITMASK_(9:5);

	if ((cg_freq & _BIT_(4)) == 1)
		ahbClk = clockSource / 2;
	else if ((cg_freq & _BIT_(3)) == 1)
		ahbClk = clockSource / 4;
	else if ((cg_freq & _BIT_(2)) == 1)
		ahbClk = clockSource / 8;
	else if ((cg_freq & _BIT_(1)) == 1)
		ahbClk = clockSource / 16;
	else
		ahbClk = clockSource / 32;

	/*tscpu_printk("cg %d dcm %d\n", ((cg & _BIT_(10)) >> 10), dcm);*/
	/*tscpu_printk("AHB bus clock= %d MHz\n", ahbClk);*/
}
#endif

/* chip dependent */
void print_risky_temps(char *prefix, int offset, int printLevel)
{
	sprintf((prefix + offset), "cpu %d",
		get_immediate_cpu_wrap());

	switch (printLevel) {
	case 0:
		tscpu_dprintk("%s\n", prefix);
		break;
	case 1:
		tscpu_warn("%s\n", prefix);
		break;
	default:
		tscpu_dprintk("%s\n", prefix);
	}
}

