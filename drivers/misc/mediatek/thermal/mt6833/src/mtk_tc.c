/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <mt-plat/mtk_wd_api.h>

#include <linux/time.h>

#define __MT_MTK_TS_CPU_C__
#include <tscpu_settings.h>

/* 1: turn on RT kthread for thermal protection in this sw module;
 * 0: turn off
 */
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
#include "mtk_thermal_ipi.h"
/*=============================================================
 * Local variable definition
 *=============================================================
 */

/*
 * PTP#	module		TSMCU Plan
 *  0	MCU_LITTLE	TSMCU-5,6,7
 *  1	MCU_BIG		TSMCU-8,9
 *  2	MCU_CCI		TSMCU-5,6,7
 *  3	MFG(GPU)	TSMCU-1,2
 *  4	VPU		TSMCU-4
 *  No PTP bank 5
 *  6	TOP		TSMCU-1,2,4
 *  7	MD		TSMCU-0
 */

/* chip dependent */
/* TODO: I assume AHB bus frequecy is 78MHz. Please confirm it.
 */
struct thermal_controller tscpu_g_tc[THERMAL_CONTROLLER_NUM] = {
	[0] = {
		.ts = {L_TS_MCU1, L_TS_MCU2, L_TS_MCU4, L_TS_ABB},
		.ts_number = 4,
		.dominator_ts_idx = 2, //hw protection ref TS (idx of .ts arr)
		.tc_offset = 0x0,
		.tc_speed = {
			0x00C,
			0x001,
			0x03B,
			0x0000030D
		} /* 4.9ms */
	},
	[1] = {
		.ts = {L_TS_MCU5, L_TS_MCU6, L_TS_MCU7, L_TS_MCU0},
		.ts_number = 4,
		.dominator_ts_idx = 1, //hw protection ref TS (idx of .ts arr)
		.tc_offset = 0x100,
		.tc_speed = {
			0x00C,
			0x001,
			0x03B,
			0x0000030D
		} /* 4.9ms */
	},
	[2] = {
		.ts = {L_TS_MCU8, L_TS_MCU9},
		.ts_number = 2,
		.dominator_ts_idx = 1, //hw protection ref TS (idx of .ts arr)
		.tc_offset = 0x200,
		.tc_speed = {
			0x00C,
			0x001,
			0x008,
			0x0000030D
		} /* 1ms */
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
static __s32 g_o_vtsmcu0;
static __s32 g_o_vtsmcu1;
static __s32 g_o_vtsmcu2;
/* There is no g_o_vtsmcu3 in MT6785 compared with MT6779 */
static __s32 g_o_vtsmcu4;
static __s32 g_o_vtsmcu5;
static __s32 g_o_vtsmcu6;
static __s32 g_o_vtsmcu7;
static __s32 g_o_vtsmcu8;
static __s32 g_o_vtsmcu9;
static __s32 g_o_vtsabb;
static __s32 g_degc_cali;
static __s32 g_adc_cali_en_t;
static __s32 g_o_slope;
static __s32 g_o_slope_sign;
static __s32 g_id;

static __s32 g_ge;
static __s32 g_oe;
static __s32 g_gain;

static __s32 g_x_roomt[L_TS_MCU_NUM] = { 0 };

/**
 * curr_temp >= tscpu_polling_trip_temp1:
 *	polling interval = interval
 * tscpu_polling_trip_temp1 > cur_temp >= tscpu_polling_trip_temp2:
 *	polling interval = interval * tscpu_polling_factor1
 * tscpu_polling_trip_temp2 > cur_temp:
 *	polling interval = interval * tscpu_polling_factor2
 */
/* chip dependent */
int tscpu_polling_trip_temp1 = 40000;
int tscpu_polling_trip_temp2 = 20000;
int tscpu_polling_factor1 = 1;
int tscpu_polling_factor2 = 4;

#if MTKTSCPU_FAST_POLLING
/* Combined fast_polling_trip_temp and fast_polling_factor,
 * it means polling_delay will be 1/5 of original interval
 * after mtktscpu reports > 65C w/o exit point
 */
int fast_polling_trip_temp = 60000;
int fast_polling_trip_temp_high = 60000; /* deprecaed */
int fast_polling_factor = 1;
int tscpu_cur_fp_factor = 1;
int tscpu_next_fp_factor = 1;
#endif

/*=============================================================
 * Local function declartation
 *=============================================================
 */
static __s32 temperature_to_raw_room(__u32 ret, enum tsmcu_sensor_enum ts_name);
static void set_tc_trigger_hw_protect
	(int temperature, int temperature2, int tc_num);

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

void get_thermal_slope_intercept(struct TS_PTPOD *ts_info, enum
thermal_bank_name ts_bank)
{
	unsigned int temp0, temp1, temp2;
	struct TS_PTPOD ts_ptpod;
	__s32 x_roomt;

	tscpu_dprintk("%s\n", __func__);

	/* chip dependent */

	/* If there are two or more sensors in a bank, choose the sensor
	 * calibration value of the dominant sensor. You can observe it in the
	 * thermal document provided by Thermal DE.  For example, Bank 1 is
	 * for SOC + GPU. Observe all scenarios related to GPU simulation test
	 * cases to decide which sensor is the highest temperature in all cases.
	 * Then, It is the dominant sensor.(Confirmed by Thermal DE Alfred Tsai)
	 */

	/*
	 * PTP#	module		TSMCU Plan
	 *  0	MCU_LITTLE	TSMCU-5,6,7
	 *  1	MCU_BIG		TSMCU-8,9
	 *  2	MCU_CCI		TSMCU-5,6,7
	 *  3	MFG(GPU)	TSMCU-1,2
	 *  4	VPU		TSMCU-4
	 *  No PTP bank 5
	 *  6	TOP		TSMCU-1,2,4
	 *  7	MD		TSMCU-0
	 */

	switch (ts_bank) {
	case THERMAL_BANK0:
		x_roomt = g_x_roomt[L_TS_MCU6];
		break;
	case THERMAL_BANK1:
		x_roomt = g_x_roomt[L_TS_MCU9];
		break;
	case THERMAL_BANK2:
		x_roomt = g_x_roomt[L_TS_MCU6];
		break;
	case THERMAL_BANK3:
		x_roomt = g_x_roomt[L_TS_MCU2];
		break;
	case THERMAL_BANK4:
		x_roomt = g_x_roomt[L_TS_MCU4];
		break;
	/* No bank 5 */
	case THERMAL_BANK6:
		x_roomt = g_x_roomt[L_TS_MCU4];
		break;
	case THERMAL_BANK7:
		x_roomt = g_x_roomt[L_TS_MCU0];
		break;
	default: /* choose the highest simulation hot-spot */
		x_roomt = g_x_roomt[L_TS_MCU9];
		break;
	}


	/*
	 *   The equations in this function are confirmed by Thermal DE Alfred
	 *   Tsai.  Don't have to change until using next generation thermal
	 *   sensors.
	 */

	temp0 = (10000 * 100000 / g_gain) * 15 / 18;

	if (g_o_slope_sign == 0)
		temp1 = (temp0 * 10) / (1534 + g_o_slope * 10);
	else
		temp1 = (temp0 * 10) / (1534 - g_o_slope * 10);

	ts_ptpod.ts_MTS = temp1;

	temp0 = (g_degc_cali * 10 / 2);
	temp1 =
	((10000 * 100000 / 4096 / g_gain) * g_oe + x_roomt * 10) * 15 / 18;

	if (g_o_slope_sign == 0)
		temp2 = temp1 * 100 / (1534 + g_o_slope * 10);
	else
		temp2 = temp1 * 100 / (1534 - g_o_slope * 10);

	ts_ptpod.ts_BTS = (temp0 + temp2 - 250) * 4 / 10;


	ts_info->ts_MTS = ts_ptpod.ts_MTS;
	ts_info->ts_BTS = ts_ptpod.ts_BTS;
	tscpu_dprintk("ts_MTS=%d, ts_BTS=%d\n",
		ts_ptpod.ts_MTS, ts_ptpod.ts_BTS);
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

	tscpu_printk("[cal] g_o_vtsmcu0     = %d\n", g_o_vtsmcu0);
	tscpu_printk("[cal] g_o_vtsmcu1     = %d\n", g_o_vtsmcu1);
	tscpu_printk("[cal] g_o_vtsmcu2     = %d\n", g_o_vtsmcu2);
	/* There is no g_o_vtsmcu3 in MT6785 compared with MT6779 */
	tscpu_printk("[cal] g_o_vtsmcu4     = %d\n", g_o_vtsmcu4);
	tscpu_printk("[cal] g_o_vtsmcu5     = %d\n", g_o_vtsmcu5);
	tscpu_printk("[cal] g_o_vtsmcu6     = %d\n", g_o_vtsmcu6);
	tscpu_printk("[cal] g_o_vtsmcu7     = %d\n", g_o_vtsmcu7);
	tscpu_printk("[cal] g_o_vtsmcu8     = %d\n", g_o_vtsmcu8);
	tscpu_printk("[cal] g_o_vtsmcu9     = %d\n", g_o_vtsmcu9);
	tscpu_printk("[cal] g_o_vtsabb      = %d\n", g_o_vtsabb);
}

void eDataCorrector(void)
{

	if (g_adc_ge_t < 265 || g_adc_ge_t > 758) {
		tscpu_warn("[thermal] Bad efuse data, g_adc_ge_t\n");
		g_adc_ge_t = 512;
	}
	if (g_adc_oe_t < 265 || g_adc_oe_t > 758) {
		tscpu_warn("[thermal] Bad efuse data, g_adc_oe_t\n");
		g_adc_oe_t = 512;
	}
	if (g_o_vtsmcu0 < -8 || g_o_vtsmcu0 > 484) {
		tscpu_warn("[thermal] Bad efuse data, g_o_vtsmcu1\n");
		g_o_vtsmcu0 = 260;
	}
	if (g_o_vtsmcu1 < -8 || g_o_vtsmcu1 > 484) {
		tscpu_warn("[thermal] Bad efuse data, g_o_vtsmcu1\n");
		g_o_vtsmcu1 = 260;
	}
	if (g_o_vtsmcu2 < -8 || g_o_vtsmcu2 > 484) {
		tscpu_warn("[thermal] Bad efuse data, g_o_vtsmcu2\n");
		g_o_vtsmcu2 = 260;
	}
	/* There is no g_o_vtsmcu3 in MT6785 compared with MT6779 */
	if (g_o_vtsmcu4 < -8 || g_o_vtsmcu4 > 484) {
		tscpu_warn("[thermal] Bad efuse data, g_o_vtsmcu4\n");
		g_o_vtsmcu4 = 260;
	}
	if (g_o_vtsmcu5 < -8 || g_o_vtsmcu5 > 484) {
		tscpu_warn("[thermal] Bad efuse data, g_o_vtsmcu5\n");
		g_o_vtsmcu5 = 260;
	}
	if (g_o_vtsmcu6 < -8 || g_o_vtsmcu6 > 484) {
		tscpu_warn("[thermal] Bad efuse data, g_o_vtsmcu6\n");
		g_o_vtsmcu6 = 260;
	}
	if (g_o_vtsmcu7 < -8 || g_o_vtsmcu7 > 484) {
		tscpu_warn("[thermal] Bad efuse data, g_o_vtsmcu7\n");
		g_o_vtsmcu7 = 260;
	}
	if (g_o_vtsmcu8 < -8 || g_o_vtsmcu8 > 484) {
		tscpu_warn("[thermal] Bad efuse data, g_o_vtsmcu8\n");
		g_o_vtsmcu8 = 260;
	}
	if (g_o_vtsmcu9 < -8 || g_o_vtsmcu9 > 484) {
		tscpu_warn("[thermal] Bad efuse data, g_o_vtsmcu8\n");
		g_o_vtsmcu9 = 260;
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
	__u32 temp0, temp1, temp2, temp3, temp4;

	temp0 = get_devinfo_with_index(ADDRESS_INDEX_0); /* 0184 */
	temp1 = get_devinfo_with_index(ADDRESS_INDEX_1); /* 0180 */
	temp2 = get_devinfo_with_index(ADDRESS_INDEX_2); /* 0188 */
	temp3 = get_devinfo_with_index(ADDRESS_INDEX_3); /* 01AC */
	temp4 = get_devinfo_with_index(ADDRESS_INDEX_4); /* 01B0 */

	pr_notice(
		"[calibration] tmp0=0x%x, tmp1=0x%x, tmp2=0x%x\n",
		temp0, temp1, temp2);
	pr_notice(
		"[calibration] tmp3=0x%x, tmp4=0x%x\n",
		temp3, temp4);

	g_adc_ge_t = ((temp0 & _BITMASK_(31:22)) >> 22);
	g_adc_oe_t = ((temp0 & _BITMASK_(21:12)) >> 12);


	g_o_vtsmcu0 = ((temp1 & _BITMASK_(25:17)) >> 17);
	g_o_vtsmcu1 = ((temp1 & _BITMASK_(16:8)) >> 8);
	g_o_vtsmcu2 = (temp0 & _BITMASK_(8:0));
	/* There is no g_o_vtsmcu3 in MT6785 compared with MT6779 */
	g_o_vtsmcu4 = ((temp2 & _BITMASK_(31:23)) >> 23);
	g_o_vtsmcu5 = ((temp2 & _BITMASK_(13:5)) >> 5);
	g_o_vtsmcu6 = ((temp3 & _BITMASK_(31:23)) >> 23);
	g_o_vtsmcu7 = ((temp3 & _BITMASK_(22:14)) >> 14);
	g_o_vtsmcu8 = ((temp3 & _BITMASK_(13:5)) >> 5);
	g_o_vtsmcu9 = ((temp4 & _BITMASK_(31:23)) >> 23);
	g_o_vtsabb = ((temp2 & _BITMASK_(22:14)) >> 14);

	g_degc_cali = ((temp1 & _BITMASK_(6:1)) >> 1);
	g_adc_cali_en_t = (temp1 & _BIT_(0));
	g_o_slope_sign = ((temp1 & _BIT_(7)) >> 7);
	g_o_slope = ((temp1 & _BITMASK_(31:26)) >> 26);

	g_id = ((temp0 & _BIT_(9)) >> 9);

	if (g_id == 0)
		g_o_slope = 0;

	if (g_adc_cali_en_t == 1) {
		/*thermal_enable = true; */
		eDataCorrector();
	} else {
		tscpu_warn("This sample is not Thermal calibrated\n");
		g_adc_ge_t = 512;
		g_adc_oe_t = 512;
		g_o_vtsmcu0 = 260;
		g_o_vtsmcu1 = 260;
		g_o_vtsmcu2 = 260;
		/* There is no g_o_vtsmcu3 in MT6785 compared with MT6779 */
		g_o_vtsmcu4 = 260;
		g_o_vtsmcu5 = 260;
		g_o_vtsmcu6 = 260;
		g_o_vtsmcu7 = 260;
		g_o_vtsmcu8 = 260;
		g_o_vtsmcu9 = 260;
		g_o_vtsabb = 260;
		g_degc_cali = 40;
		g_o_slope_sign = 0;
		g_o_slope = 0;
	}

	mtkts_dump_cali_info();
}

void tscpu_thermal_cal_prepare_2(__u32 ret)
{
	__s32 format[L_TS_MCU_NUM] = { 0 };
	int i = 0;
#if THERMAL_ENABLE_TINYSYS_SSPM || THERMAL_ENABLE_ONLY_TZ_SSPM
	struct thermal_ipi_data thermal_data;
#endif

	/* tscpu_printk("tscpu_thermal_cal_prepare_2\n"); */
	g_ge = ((g_adc_ge_t - 512) * 10000) / 4096;	/* ge * 10000 */
	g_oe = (g_adc_oe_t - 512);

	g_gain = (10000 + g_ge);

	format[0] = (g_o_vtsmcu0 + 3350 - g_oe);
	format[1] = (g_o_vtsmcu1 + 3350 - g_oe);
	format[2] = (g_o_vtsmcu2 + 3350 - g_oe);
	/* There is no g_o_vtsmcu3 in MT6785 compared with MT6779 */
	format[3] = (g_o_vtsmcu4 + 3350 - g_oe);
	format[4] = (g_o_vtsmcu5 + 3350 - g_oe);
	format[5] = (g_o_vtsmcu6 + 3350 - g_oe);
	format[6] = (g_o_vtsmcu7 + 3350 - g_oe);
	format[7] = (g_o_vtsmcu8 + 3350 - g_oe);
	format[8] = (g_o_vtsmcu9 + 3350 - g_oe);
	format[9] = (g_o_vtsabb + 3350 - g_oe);

	for (i = 0; i < L_TS_MCU_NUM; i++) {
		/* x_roomt * 10000 */
		g_x_roomt[i] = (((format[i] * 10000) / 4096) * 10000) / g_gain;
	}

	tscpu_printk("[T_De][cal] g_ge         = %d\n", g_ge);
	tscpu_printk("[T_De][cal] g_gain       = %d\n", g_gain);

	for (i = 0; i < L_TS_MCU_NUM; i++)
		tscpu_printk("[T_De][cal] g_x_roomt%d   = %d\n", i,
							g_x_roomt[i]);
#if THERMAL_ENABLE_TINYSYS_SSPM || THERMAL_ENABLE_ONLY_TZ_SSPM
	thermal_data.u.data.arg[0] = g_degc_cali;
	thermal_data.u.data.arg[1] = g_o_slope_sign;
	thermal_data.u.data.arg[2] = g_o_slope;
	while (thermal_to_mcupm(THERMAL_IPI_INIT_GRP1, &thermal_data) != 0)
		udelay(100);

	thermal_data.u.data.arg[0] = g_oe;
	thermal_data.u.data.arg[1] = g_gain;
	thermal_data.u.data.arg[2] = g_x_roomt[L_TS_MCU8];
	while (thermal_to_mcupm(THERMAL_IPI_INIT_GRP2, &thermal_data) != 0)
		udelay(100);

	thermal_data.u.data.arg[0] = g_x_roomt[L_TS_MCU9];
	while (thermal_to_mcupm(THERMAL_IPI_INIT_GRP3, &thermal_data) != 0)
		udelay(100);
#endif
}

#if THERMAL_CONTROLLER_HW_TP
static __s32 temperature_to_raw_room(__u32 ret, enum tsmcu_sensor_enum ts_name)
{
	/* Ycurr = [(Tcurr - DEGC_cali/2)*(1534+O_slope*10)/10*(18/15)*
	 *				(1/10000)+X_roomtabb]*Gain*4096 + OE
	 */

	__s32 t_curr = ret;
	__s32 format_1 = 0;
	__s32 format_2 = 0;
	__s32 format_3 = 0;
	__s32 format_4 = 0;

	/* tscpu_dprintk("temperature_to_raw_room\n"); */

	if (g_o_slope_sign == 0) {	/* O_SLOPE is Positive. */
		format_1 = t_curr - (g_degc_cali * 1000 / 2);
		format_2 = format_1 * (1534 + g_o_slope * 10) / 10 * 18 / 15;
		format_2 = format_2 - 2 * format_2;

		format_3 = format_2 / 1000 + g_x_roomt[ts_name] * 10;
		format_4 = (format_3 * 4096 / 10000 * g_gain) / 100000 + g_oe;
	} else {		/* O_SLOPE is Negative. */
		format_1 = t_curr - (g_degc_cali * 1000 / 2);
		format_2 = format_1 * (1534 - g_o_slope * 10) / 10 * 18 / 15;
		format_2 = format_2 - 2 * format_2;

		format_3 = format_2 / 1000 + g_x_roomt[ts_name] * 10;
		format_4 = (format_3 * 4096 / 10000 * g_gain) / 100000 + g_oe;
	}

	return format_4;
}
#endif

static __s32 raw_to_temperature_roomt(__u32 ret, enum tsmcu_sensor_enum ts_name)
{
	__s32 t_current = 0;
	__s32 y_curr = ret;
	__s32 format_1 = 0;
	__s32 format_2 = 0;
	__s32 format_3 = 0;
	__s32 format_4 = 0;
	__s32 xtoomt = 0;

	xtoomt = g_x_roomt[ts_name];

	/* tscpu_dprintk("raw_to_temperature_room,ts_num=%d,xtoomt=%d\n",
	 *						ts_name,xtoomt);
	 */

	if (ret == 0)
		return 0;

	format_1 = ((g_degc_cali * 10) >> 1);
	format_2 = (y_curr - g_oe);

	format_3 = (((((format_2) * 10000) >> 12) * 10000) / g_gain) - xtoomt;
	format_3 = format_3 * 15 / 18;


	if (g_o_slope_sign == 0) /* uint = 0.1 deg */
		format_4 = ((format_3 * 1000) / (1534 + g_o_slope * 10));
	else			/* uint = 0.1 deg */
		format_4 = ((format_3 * 1000) / (1534 - g_o_slope * 10));

	format_4 = format_4 - (format_4 << 1);

	t_current = format_1 + format_4;	/* uint = 0.1 deg */

	/* tscpu_dprintk("raw_to_temperature_room,t_current=%d\n",t_current); */
	return t_current;
}
static void thermal_dump_debug_logs(int tc_num)
{
	int offset;
	unsigned int auxadc_data11, ts_con0, ts_con1;
	unsigned int tempmsr0, tempmsr1, tempmsr2, tempmsr3;

	offset = tscpu_g_tc[tc_num].tc_offset;

	auxadc_data11 = readl(AUXADC_DAT11_V);
	ts_con0 = readl(TS_CON0_TM);
	ts_con1 = readl(TS_CON1_TM);
	tempmsr0 = readl((offset + TEMPMSR0));
	tempmsr1 = readl((offset + TEMPMSR1));
	tempmsr2 = readl((offset + TEMPMSR2));
	tempmsr3 = readl((offset + TEMPMSR3));

	tscpu_printk("AUXADC_DAT11 = 0x%x, TS_CON0 = 0x%x, TS_CON1 = 0x%x\n",
		auxadc_data11, ts_con0, ts_con1);
	tscpu_printk("TEMPMSR0_%d = 0x%x, TEMPMSR1_%d = 0x%x\n",
		tc_num, tempmsr0, tc_num, tempmsr1);
	tscpu_printk("TEMPMSR2_%d = 0x%x, TEMPMSR3_%d = 0x%x\n",
		tc_num, tempmsr2, tc_num, tempmsr3);
}
static void thermal_interrupt_handler(int tc_num)
{
	unsigned int  ret = 0;
	int offset;

	offset = tscpu_g_tc[tc_num].tc_offset;

	ret = readl(offset + TEMPMONINTSTS);
	/* Write back to clear interrupt status */
	mt_reg_sync_writel(ret, offset + TEMPMONINTSTS);

	tscpu_printk(
		"[Thermal IRQ] Auxadc thermal_tc %d, TEMPMONINTSTS=0x%08x\n",
		tc_num, ret);

	if (ret & THERMAL_COLD_INTERRUPT_0)
		tscpu_dprintk(
		"[Thermal IRQ]: Cold int triggered, sensor point 0\n");

	if (ret & THERMAL_HOT_INTERRUPT_0)
		tscpu_dprintk(
		"[Thermal IRQ]: Hot int triggered, sensor point 0\n");

	if (ret & THERMAL_LOW_OFFSET_INTERRUPT_0)
		tscpu_dprintk(
		"[Thermal IRQ]: Low offset int triggered, sensor point 0\n");

	if (ret & THERMAL_HIGH_OFFSET_INTERRUPT_0)
		tscpu_dprintk(
		"[Thermal IRQ]: High offset int triggered, sensor point 0\n");

	if (ret & THERMAL_HOT2NORMAL_INTERRUPT_0)
		tscpu_dprintk(
		"[Thermal IRQ]: Hot2Normal int triggered, sensor point 0\n");

	if (ret & THERMAL_COLD_INTERRUPT_1)
		tscpu_dprintk(
		"[Thermal IRQ]: Cold int triggered, sensor point 1\n");

	if (ret & THERMAL_HOT_INTERRUPT_1)
		tscpu_dprintk(
		"[Thermal IRQ]: Hot int triggered, sensor point 1\n");

	if (ret & THERMAL_LOW_OFFSET_INTERRUPT_1)
		tscpu_dprintk(
		"[Thermal IRQ]: Low offset int triggered, sensor point 1\n");

	if (ret & THERMAL_HIGH_OFFSET_INTERRUPT_1)
		tscpu_dprintk(
		"[Thermal IRQ]: High offset int triggered, sensor point 1\n");

	if (ret & THERMAL_HOT2NORMAL_INTERRUPT_1)
		tscpu_dprintk(
		"[Thermal IRQ]: Hot2Normal int triggered, sensor point 1\n");

	if (ret & THERMAL_COLD_INTERRUPT_2)
		tscpu_dprintk(
		"[Thermal IRQ]: Cold int triggered, sensor point 2\n");

	if (ret & THERMAL_HOT_INTERRUPT_2)
		tscpu_dprintk(
		"[Thermal IRQ]: Hot int triggered, sensor point 2\n");

	if (ret & THERMAL_LOW_OFFSET_INTERRUPT_2)
		tscpu_dprintk(
		"[Thermal IRQ]: Low offset int triggered, sensor point 2\n");

	if (ret & THERMAL_HIGH_OFFSET_INTERRUPT_2)
		tscpu_dprintk(
		"[Thermal IRQ]: High offset int triggered, sensor point 2\n");

	if (ret & THERMAL_HOT2NORMAL_INTERRUPT_2)
		tscpu_dprintk(
		"[Thermal IRQ]: Hot2Normal int triggered, sensor point 2\n");

	if (ret & THERMAL_AHB_TIMEOUT_INTERRUPT)
		tscpu_dprintk(
		"[Thermal IRQ]: AHB bus timeout triggered\n");

	if (ret & THERMAL_IMMEDIATE_INTERRUPT_0)
		tscpu_dprintk(
		"[Thermal IRQ]: Immediate int triggered, sensor point 0\n");

	if (ret & THERMAL_IMMEDIATE_INTERRUPT_1)
		tscpu_dprintk(
		"[Thermal IRQ]: Immediate int triggered, sensor point 1\n");

	if (ret & THERMAL_IMMEDIATE_INTERRUPT_2)
		tscpu_dprintk(
		"[Thermal IRQ]: Immediate int triggered, sensor point 2\n");

	if (ret & THERMAL_FILTER_INTERRUPT_0)
		tscpu_dprintk(
		"[Thermal IRQ]: Filter int triggered, sensor point 0\n");

	if (ret & THERMAL_FILTER_INTERRUPT_1)
		tscpu_dprintk(
		"[Thermal IRQ]: Filter int triggered, sensor point 1\n");

	if (ret & THERMAL_FILTER_INTERRUPT_2)
		tscpu_dprintk(
		"[Thermal IRQ]: Filter int triggered, sensor point 2\n");

	if (ret & THERMAL_COLD_INTERRUPT_3)
		tscpu_dprintk(
		"[Thermal IRQ]: Cold int triggered, sensor point 3\n");

	if (ret & THERMAL_HOT_INTERRUPT_3)
		tscpu_dprintk(
		"[Thermal IRQ]: Hot int triggered, sensor point 3\n");

	if (ret & THERMAL_LOW_OFFSET_INTERRUPT_3)
		tscpu_dprintk(
		"[Thermal IRQ]: Low offset int triggered, sensor point 3\n");

	if (ret & THERMAL_HIGH_OFFSET_INTERRUPT_3)
		tscpu_dprintk(
		"[Thermal IRQ]: High offset int triggered, sensor point 3\n");

	if (ret & THERMAL_HOT2NORMAL_INTERRUPT_3)
		tscpu_dprintk(
		"[Thermal IRQ]: Hot2Normal int triggered, sensor point 3\n");

	if (ret & THERMAL_IMMEDIATE_INTERRUPT_3)
		tscpu_dprintk(
		"[Thermal IRQ]: Immediate int triggered, sensor point 3\n");

	if (ret & THERMAL_FILTER_INTERRUPT_3)
		tscpu_dprintk(
		"[Thermal IRQ]: Filter int triggered, sensor point 3\n");

	if (ret & THERMAL_PROTECTION_STAGE_1)
		tscpu_dprintk(
		"[Thermal IRQ]: Thermal protection stage 1 int triggered\n");

	if (ret & THERMAL_PROTECTION_STAGE_2) {
		tscpu_dprintk(
		"[Thermal IRQ]: Thermal protection stage 2 int triggered\n");
#if MTK_TS_CPU_RT
		wake_up_process(ktp_thread_handle);
#endif
	}

	if (ret & THERMAL_PROTECTION_STAGE_3) {
		tscpu_printk(
		"[Thermal IRQ]: Thermal protection stage 3 int triggered\n");
		tscpu_printk("[Thermal IRQ]: Thermal HW reboot!!");
		thermal_dump_debug_logs(tc_num);
	}
}

irqreturn_t tscpu_thermal_all_tc_interrupt_handler(int irq, void *dev_id)
{
	unsigned int ret = 0, i, mask = 1;

	ret = readl(THERMINTST);
	ret = ret & 0x7F;

	/* MSB LSB NAME
	 * 6   6   LVTSINT3
	 * 5   5   LVTSINT2
	 * 4   4   LVTSINT1
	 * 3   3   LVTSINT0
	 * 2   2   THERMINT2
	 * 1   1   THERMINT1
	 * 0   0   THERMINT0
	 */

	for (i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++) {
		mask = 1 << i;

		if ((ret & mask) == 0)
			thermal_interrupt_handler(i);
	}

	return IRQ_HANDLED;
}

static void thermal_reset_and_initial(int tc_num)
{
	unsigned int offset, tempMonCtl1, tempMonCtl2, tempAhbPoll;

	offset = tscpu_g_tc[tc_num].tc_offset;

	tempMonCtl1 = (tscpu_g_tc[tc_num].tc_speed.period_unit &
			_BITMASK_(9:0));

	tempMonCtl2 = (((tscpu_g_tc[tc_num].tc_speed.filter_interval_delay
			<< 16) & _BITMASK_(25:16)) |
			(tscpu_g_tc[tc_num].tc_speed.sensor_interval_delay &
			_BITMASK_(9:0)));

	tempAhbPoll = tscpu_g_tc[tc_num].tc_speed.ahb_polling_interval;

	/* Calculating period unit in Module clock x 256,
	 * and the Module clock
	 * will be changed to 26M when Infrasys enters Sleep mode.
	 */

	/* bus clock 66M counting unit is 12 * 1/66M * 256 =
	 * 12 * 3.879us = 46.545 us
	 */
	mt_reg_sync_writel(tempMonCtl1, offset + TEMPMONCTL1);
	/*
	 * filt interval is 1 * 46.545us = 46.545us,
	 * sen interval is 429 * 46.545us = 19.968ms
	 */
	mt_reg_sync_writel(tempMonCtl2, offset + TEMPMONCTL2);
	/* AHB polling is 781* 1/66M = 11.833us */
	mt_reg_sync_writel(tempAhbPoll, offset + TEMPAHBPOLL);

#if THERMAL_CONTROLLER_HW_FILTER == 2
	/* temperature sampling control, 2 out of 4 samples */
	mt_reg_sync_writel(0x00000492, offset + TEMPMSRCTL0);
#elif THERMAL_CONTROLLER_HW_FILTER == 4
	/* temperature sampling control, 4 out of 6 samples */
	mt_reg_sync_writel(0x000006DB, offset + TEMPMSRCTL0);
#elif THERMAL_CONTROLLER_HW_FILTER == 8
	/* temperature sampling control, 8 out of 10 samples */
	mt_reg_sync_writel(0x00000924, offset + TEMPMSRCTL0);
#elif THERMAL_CONTROLLER_HW_FILTER == 16
	/* temperature sampling control, 16 out of 18 samples */
	mt_reg_sync_writel(0x00000B6D, offset + TEMPMSRCTL0);
#else				/* default 1 */
	/* temperature sampling control, 1 sample */
	mt_reg_sync_writel(0x00000000, offset + TEMPMSRCTL0);
#endif
	/* exceed this polling time, IRQ would be inserted */
	mt_reg_sync_writel(0xFFFFFFFF, offset + TEMPAHBTO);

	/* times for interrupt occurrance for SP0*/
	mt_reg_sync_writel(0x00000000, offset + TEMPMONIDET0);
	/* times for interrupt occurrance for SP1*/
	mt_reg_sync_writel(0x00000000, offset + TEMPMONIDET1);
	/* times for interrupt occurrance for SP2*/
	mt_reg_sync_writel(0x00000000, offset + TEMPMONIDET2);
	/* times for interrupt occurrance for SP2*/
	mt_reg_sync_writel(0x00000000, offset + TEMPMONIDET3);

	mt_reg_sync_writel(0x800, offset + TEMPADCMUX);
	/* AHB address for auxadc mux selection */
	mt_reg_sync_writel((__u32) AUXADC_CON1_CLR_P,
			offset + TEMPADCMUXADDR);

	/* AHB value for auxadc enable */
	mt_reg_sync_writel(0x800, offset + TEMPADCEN);

	/*
	 * AHB address for auxadc enable (channel 0 immediate mode selected)
	 * this value will be stored to TEMPADCENADDR automatically by hw
	 */
	mt_reg_sync_writel((__u32) AUXADC_CON1_SET_P, offset + TEMPADCENADDR);

	/* AHB address for auxadc valid bit */
	mt_reg_sync_writel((__u32) AUXADC_DAT11_P,
			offset + TEMPADCVALIDADDR);

	/* AHB address for auxadc voltage output */
	mt_reg_sync_writel((__u32) AUXADC_DAT11_P,
			offset + TEMPADCVOLTADDR);

	/* read valid & voltage are at the same register */
	mt_reg_sync_writel(0x0, offset + TEMPRDCTRL);

	/* indicate where the valid bit is (the 12th bit
	 * is valid bit and 1 is valid)
	 */
	mt_reg_sync_writel(0x0000002C, offset + TEMPADCVALIDMASK);

	/* do not need to shift */
	mt_reg_sync_writel(0x0, offset + TEMPADCVOLTAGESHIFT);
}

/**
 *  temperature2 to set the middle threshold for interrupting CPU.
 *  -275000 to disable it.
 */
static void set_tc_trigger_hw_protect
(int temperature, int temperature2, int tc_num)
{
	int temp = 0, raw_high, d_index, config;
	enum tsmcu_sensor_enum ts_name;
	__u32 offset;

	offset = tscpu_g_tc[tc_num].tc_offset;

	/* temperature2=80000;  test only */
	tscpu_dprintk("%s t1=%d t2=%d\n", __func__, temperature,
		temperature2);

	if (tscpu_g_tc[tc_num].dominator_ts_idx <
		tscpu_g_tc[tc_num].ts_number){
		d_index = tscpu_g_tc[tc_num].dominator_ts_idx;
	} else {
		tscpu_printk("Error: Auxadc TC%d, dominator_ts_idx = %d should smaller than ts_number = %d\n",
			tc_num,
			tscpu_g_tc[tc_num].dominator_ts_idx,
			tscpu_g_tc[tc_num].ts_number);

		tscpu_printk("Use the sensor point 0 as the dominated sensor\n");
		d_index = 0;
	}


	ts_name = tscpu_g_tc[tc_num].ts[d_index];

	/* temperature to trigger SPM state2 */
	if (tc_num == THERMAL_CONTROLLER2)
		raw_high = temperature_to_raw_room(105000, ts_name);
	else
		raw_high = temperature_to_raw_room(temperature, ts_name);

	temp = readl(offset + TEMPMONINT);
	/* disable trigger SPM interrupt */
	mt_reg_sync_writel(temp & 0x00000000, offset + TEMPMONINT);

	/* Select protection sensor */
	config = ((d_index << 2) + 0x2) << 16;
	mt_reg_sync_writel(config, offset + TEMPPROTCTL);

	/* set hot to HOT wakeup event */
	mt_reg_sync_writel(raw_high, offset + TEMPPROTTC);

	/* enable trigger Hot SPM interrupt */
	mt_reg_sync_writel(temp | 0x80000000, offset + TEMPMONINT);
}

static int read_tc_raw_and_temp(
u32 *tempmsr_name, enum tsmcu_sensor_enum ts_name)
{
	int temp = 0, raw = 0;
#if CONFIG_LVTS_ERROR_AEE_WARNING
	int raw1;
#endif

	if (tempmsr_name == 0)
		return 0;

#if CONFIG_LVTS_ERROR_AEE_WARNING
	raw = readl(tempmsr_name);
	raw1 = ((raw & _BIT_(15)) >> 15);
	raw = raw & 0xFFF;
#else
	raw = readl(tempmsr_name) & 0xFFF;
#endif
	temp = raw_to_temperature_roomt(raw, ts_name);

	tscpu_dprintk("read_tc_raw_temp,ts_raw=%d,temp=%d\n", raw, temp * 100);
	tscpu_ts_mcu_temp_r[ts_name] = raw;
#if CONFIG_LVTS_ERROR_AEE_WARNING
	tscpu_ts_mcu_temp_v[ts_name] = raw1;
#endif

#if CFG_THERM_LVTS == (0)
	tscpu_ts_temp_r[ts_name] = tscpu_ts_mcu_temp_r[ts_name];
#endif
	return temp * 100;
}

void tscpu_thermal_read_tc_temp(
int tc_num, enum tsmcu_sensor_enum type, int order)
{
	__u32 offset;

	tscpu_dprintk("%s tc_num %d type %d order %d\n", __func__,
						tc_num, type, order);
	offset = tscpu_g_tc[tc_num].tc_offset;

	switch (order) {
	case 0:
		tscpu_ts_mcu_temp[type] =
			read_tc_raw_and_temp((offset + TEMPMSR0), type);

		tscpu_dprintk("%s order %d tc_num %d type %d temp %d\n",
				__func__, order, tc_num, type,
				tscpu_ts_mcu_temp[type]);
		break;
	case 1:
		tscpu_ts_mcu_temp[type] =
			read_tc_raw_and_temp((offset + TEMPMSR1), type);
		tscpu_dprintk("%s order %d tc_num %d type %d temp %d\n",
				__func__, order, tc_num, type,
				tscpu_ts_mcu_temp[type]);
		break;
	case 2:
		tscpu_ts_mcu_temp[type] =
			read_tc_raw_and_temp((offset + TEMPMSR2), type);
		tscpu_dprintk("%s order %d tc_num %d type %d temp %d\n",
				__func__, order, tc_num, type,
				tscpu_ts_mcu_temp[type]);
		break;
	case 3:
		tscpu_ts_mcu_temp[type] =
			read_tc_raw_and_temp((offset + TEMPMSR3), type);
		tscpu_dprintk("%s order %d tc_num %d type %d temp %d\n",
				__func__, order, tc_num, type,
				tscpu_ts_mcu_temp[type]);
		break;
	default:
		tscpu_ts_mcu_temp[type] =
			read_tc_raw_and_temp((offset + TEMPMSR0), type);
		tscpu_dprintk("%s order %d tc_num %d type %d temp %d\n",
				__func__, order, tc_num, type,
				tscpu_ts_mcu_temp[type]);
		break;
	}

#if CFG_THERM_LVTS == (0)
	tscpu_ts_temp[type] = tscpu_ts_mcu_temp[type];
#endif
}

void read_all_tc_tsmcu_temperature(void)
{
	int i = 0, j = 0;

	for (i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++)
		for (j = 0; j < tscpu_g_tc[i].ts_number; j++)
			tscpu_thermal_read_tc_temp(i, tscpu_g_tc[i].ts[j], j);
}

int tscpu_thermal_fast_init(int tc_num)
{
	__u32 temp = 0, cunt = 0, offset = 0;

	offset = tscpu_g_tc[tc_num].tc_offset;

	temp = THERMAL_INIT_VALUE;

	/* write temp to spare register */
	mt_reg_sync_writel((0x00001000 + temp), PTPSPARE2);

	/* counting unit is 320 * 31.25us = 10ms */
	mt_reg_sync_writel(1, offset + TEMPMONCTL1);

	/* sensing interval is 200 * 10ms = 2000ms */
	mt_reg_sync_writel(1, offset + TEMPMONCTL2);

	/* polling interval to check if temperature sense is ready */
	mt_reg_sync_writel(1, offset + TEMPAHBPOLL);

	/* exceed this polling time, IRQ would be inserted */
	mt_reg_sync_writel(0x000000FF, offset + TEMPAHBTO);

	/* times for interrupt occurrance */
	mt_reg_sync_writel(0x00000000, offset + TEMPMONIDET0);

	/* times for interrupt occurrance */
	mt_reg_sync_writel(0x00000000, offset + TEMPMONIDET1);

	/* temperature measurement sampling control */
	mt_reg_sync_writel(0x0000000, offset + TEMPMSRCTL0);

	/* this value will be stored to TEMPPNPMUXADDR (TEMPSPARE0)
	 * automatically by hw
	 */
	mt_reg_sync_writel(0x1, offset + TEMPADCPNP0);
	mt_reg_sync_writel(0x2, offset + TEMPADCPNP1);
	mt_reg_sync_writel(0x3, offset + TEMPADCPNP2);
	mt_reg_sync_writel(0x4, offset + TEMPADCPNP3);

	/* AHB address for pnp sensor mux selection */
	mt_reg_sync_writel((__u32) PTPSPARE0_P, offset + TEMPPNPMUXADDR);

	/* AHB address for auxadc mux selection */
	mt_reg_sync_writel((__u32) PTPSPARE0_P, offset + TEMPADCMUXADDR);

	/* AHB address for auxadc enable */
	mt_reg_sync_writel((__u32) PTPSPARE1_P, offset + TEMPADCENADDR);

	/* AHB address for auxadc valid bit */
	mt_reg_sync_writel((__u32) PTPSPARE2_P, offset + TEMPADCVALIDADDR);

	/* AHB address for auxadc voltage output */
	mt_reg_sync_writel((__u32) PTPSPARE2_P, offset + TEMPADCVOLTADDR);

	/* read valid & voltage are at the same register */
	mt_reg_sync_writel(0x0, offset + TEMPRDCTRL);

	/* indicate where the valid bit is (the 12th bit
	 * is valid bit and 1 is valid)
	 */
	mt_reg_sync_writel(0x0000002C, offset + TEMPADCVALIDMASK);

	/* do not need to shift */
	mt_reg_sync_writel(0x0, offset + TEMPADCVOLTAGESHIFT);

	/* enable auxadc mux & pnp write transaction */
	mt_reg_sync_writel(0x3, offset + TEMPADCWRITECTRL);


	/* enable all interrupt except filter sense and
	 * immediate sense interrupt
	 */
	mt_reg_sync_writel(0x00000000, offset + TEMPMONINT);

	/* enable all sensing point(sensing point 2 is unused)*/
	mt_reg_sync_writel(0x0000000F, offset + TEMPMONCTL0);


	cunt = 0;
	temp = readl(offset + TEMPMSR0) & 0x0fff;
	while (temp != THERMAL_INIT_VALUE && cunt < 20) {
		cunt++;
		/* pr_notice("[Power/CPU_Thermal]0 temp=%d,cunt=%d\n",
		 *					temp,cunt);
		 */
		temp = readl(offset + TEMPMSR0) & 0x0fff;
	}

	cunt = 0;
	temp = readl(offset + TEMPMSR1) & 0x0fff;
	while (temp != THERMAL_INIT_VALUE && cunt < 20) {
		cunt++;
		/* pr_notice("[Power/CPU_Thermal]1 temp=%d,cunt=%d\n",
		 *					temp,cunt);
		 */
		temp = readl(offset + TEMPMSR1) & 0x0fff;
	}

	cunt = 0;
	temp = readl(offset + TEMPMSR2) & 0x0fff;
	while (temp != THERMAL_INIT_VALUE && cunt < 20) {
		cunt++;
		/* pr_notice("[Power/CPU_Thermal]2 temp=%d,cunt=%d\n",
		 *					temp,cunt);
		 */
		temp = readl(offset + TEMPMSR2) & 0x0fff;
	}

	cunt = 0;
	temp = readl(offset + TEMPMSR3) & 0x0fff;
	while (temp != THERMAL_INIT_VALUE && cunt < 20) {
		cunt++;
		/* pr_notice("[Power/CPU_Thermal]3 temp=%d,cunt=%d\n",
		 *					temp,cunt);
		 */
		temp = readl(offset + TEMPMSR3) & 0x0fff;
	}

	return 0;
}

int tscpu_thermal_ADCValueOfMcu(enum tsmcu_sensor_enum type)
{
	switch (type) {
	case L_TS_MCU0:
		return TEMPADC_MCU0;
	case L_TS_MCU1:
		return TEMPADC_MCU1;
	case L_TS_MCU2:
		return TEMPADC_MCU2;
	/* There is no TSMCU3 in MT6785 compared with MT6779 */
	case L_TS_MCU4:
		return TEMPADC_MCU4;
	case L_TS_MCU5:
		return TEMPADC_MCU5;
	case L_TS_MCU6:
		return TEMPADC_MCU6;
	case L_TS_MCU7:
		return TEMPADC_MCU7;
	case L_TS_MCU8:
		return TEMPADC_MCU8;
	case L_TS_MCU9:
		return TEMPADC_MCU9;
	case L_TS_ABB:
		return TEMPADC_ABB;
	default:
		return TEMPADC_MCU1;
	}
}

/* pause ALL periodoc temperature sensing point */
void thermal_pause_all_periodoc_temp_sensing(void)
{
	int i, temp, offset;

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

	for (i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++) {
		if (tscpu_g_tc[i].ts_number == 0)
			continue;

		offset = tscpu_g_tc[i].tc_offset;
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

	/* disable auxadc channel 11 synchronous mode */
	mt_reg_sync_writel(temp, AUXADC_CON0_V);

	/* disable auxadc channel 11 immediate mode */
	mt_reg_sync_writel(0x800, AUXADC_CON1_CLR_V);

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

	tscpu_dprintk("%s,temperature=%d,temperature2=%d\n", __func__,
		temperature, temperature2);

#if THERMAL_PERFORMANCE_PROFILE
	struct timeval begin, end;
	unsigned long val;

	do_gettimeofday(&begin);
#endif
	/* spend 860~1463 us */
	/* Thermal need to config to direct reset mode
	 * this API provide by Weiqi Fu(RGU SW owner).
	 */
	wd_api_ret = get_wd_api(&wd_api);
	if (wd_api_ret >= 0) {
		wd_api->wd_thermal_direct_mode_config(WD_REQ_DIS,
					WD_REQ_RST_MODE);	/* reset mode */
	} else {
		tscpu_warn("%d FAILED TO GET WD API\n", __LINE__);
		WARN_ON_ONCE(1);
	}
#if THERMAL_PERFORMANCE_PROFILE
	do_gettimeofday(&end);

	/* Get milliseconds */
	pr_notice("resume time spent, sec : %lu , usec : %lu\n",
						(end.tv_sec - begin.tv_sec),
						(end.tv_usec - begin.tv_usec));
#endif

	for (i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++) {
		if (tscpu_g_tc[i].ts_number == 0)
			continue;
		/* Move thermal HW protection ahead... */
		set_tc_trigger_hw_protect(temperature, temperature2, i);
	}

	/*Thermal need to config to direct reset mode
	 *  this API provide by Weiqi Fu(RGU SW owner).
	 */
	if (wd_api_ret >= 0) {
		wd_api->wd_thermal_direct_mode_config(WD_REQ_EN,
					WD_REQ_RST_MODE);	/* reset mode */
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
	/* TODO: Is it necessary to read INFRA_GLOBALCON_RST_0_SET? */
	temp = readl(INFRA_GLOBALCON_RST_0_SET);

	/* 1: Enables thermal control software reset */
	temp |= 0x00000001;

	mt_reg_sync_writel(temp, INFRA_GLOBALCON_RST_0_SET);

	/* TODO: How long to set the reset bit? */

	/* un reset */
	/* TODO: Is it necessary to read INFRA_GLOBALCON_RST_0_CLR? */
	temp = readl(INFRA_GLOBALCON_RST_0_CLR);
	/* 1: Enable reset Disables thermal control software reset */
	temp |= 0x00000001;
	mt_reg_sync_writel(temp, INFRA_GLOBALCON_RST_0_CLR);
}

int tscpu_dump_cali_info(struct seq_file *m, void *v)
{
	int i;

	seq_printf(m, "[cal] g_adc_ge_t      = %d\n", g_adc_ge_t);
	seq_printf(m, "[cal] g_adc_oe_t      = %d\n", g_adc_oe_t);

	seq_printf(m, "[cal] g_o_vtsmcu0     = %d\n", g_o_vtsmcu0);
	seq_printf(m, "[cal] g_o_vtsmcu1     = %d\n", g_o_vtsmcu1);
	seq_printf(m, "[cal] g_o_vtsmcu2     = %d\n", g_o_vtsmcu2);
	/* There is no g_o_vtsmcu3 in MT6785 compared with MT6779 */
	seq_printf(m, "[cal] g_o_vtsmcu4     = %d\n", g_o_vtsmcu4);
	seq_printf(m, "[cal] g_o_vtsmcu5     = %d\n", g_o_vtsmcu5);
	seq_printf(m, "[cal] g_o_vtsmcu6     = %d\n", g_o_vtsmcu6);
	seq_printf(m, "[cal] g_o_vtsmcu7     = %d\n", g_o_vtsmcu7);
	seq_printf(m, "[cal] g_o_vtsmcu8     = %d\n", g_o_vtsmcu8);
	seq_printf(m, "[cal] g_o_vtsmcu9     = %d\n", g_o_vtsmcu9);
	seq_printf(m, "[cal] g_o_vtsabb     = %d\n", g_o_vtsabb);

	seq_printf(m, "[cal] g_degc_cali     = %d\n", g_degc_cali);
	seq_printf(m, "[cal] g_adc_cali_en_t = %d\n", g_adc_cali_en_t);
	seq_printf(m, "[cal] g_o_slope_sign  = %d\n", g_o_slope_sign);
	seq_printf(m, "[cal] g_o_slope       = %d\n", g_o_slope);
	seq_printf(m, "[cal] g_id            = %d\n", g_id);
	seq_printf(m, "[cal] g_ge         = %d\n", g_ge);
	seq_printf(m, "[cal] g_gain       = %d\n", g_gain);

	for (i = 0; i < L_TS_MCU_NUM; i++)
		seq_printf(m, "[cal] g_x_roomt%d   = %d\n", i, g_x_roomt[i]);

	return 0;
}

#if CONFIG_LVTS_ERROR_AEE_WARNING
int check_auxadc_mcu_efuse(void)
{
	return g_adc_cali_en_t;
}
#endif

#if THERMAL_GET_AHB_BUS_CLOCK
void thermal_get_AHB_clk_info(void)
{
	int cg = 0, dcm = 0, cg_freq = 0;
	int clockSource = 136, ahbClk = 0; /* Need to confirm with DE */

	cg = readl(THERMAL_CG);
	dcm = readl(THERMAL_DCM);

	/* The following rule may change, you need to confirm with DE.
	 * These are for mt6759, and confirmed with DE Justin Gu
	 */
	cg_freq = dcm & _BITMASK_(9:5);

	if (cg_freq & _BIT_(4))
		ahbClk = clockSource / 2;
	else if (cg_freq & _BIT_(3))
		ahbClk = clockSource / 4;
	else if (cg_freq & _BIT_(2))
		ahbClk = clockSource / 8;
	else if (cg_freq & _BIT_(1))
		ahbClk = clockSource / 16;
	else
		ahbClk = clockSource / 32;

	/* tscpu_printk("cg %d dcm %d\n", ((cg & _BIT_(10)) >> 10), dcm); */
	/* tscpu_printk("AHB bus clock= %d MHz\n", ahbClk); */
}
#endif

/* chip dependent */
void print_risky_temps(char *prefix, int offset, int printLevel)
{
	sprintf((prefix + offset), "L %d B %d",
			get_immediate_cpuL_wrap(), get_immediate_cpuB_wrap());

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

