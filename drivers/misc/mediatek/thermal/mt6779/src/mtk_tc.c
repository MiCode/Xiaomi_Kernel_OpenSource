// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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
#include "mt-plat/mtk_thermal_monitor.h"
#include <linux/seq_file.h>
#include <linux/slab.h>
#include "mach/mtk_thermal.h"
#include <linux/bug.h>
#include <linux/regmap.h>

#if defined(CONFIG_MTK_CLKMGR)
#include <mach/mtk_clkmgr.h>
#else
#include <linux/clk.h>
#endif

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

#ifdef CONFIG_MTK_EFUSE
#include <linux/nvmem-consumer.h>
#endif

/*=============================================================
 * Local variable definition
 *=============================================================
 */

/*
 * PTP#	module		TSMCU Plan
 *  0	MCU_LITTLE	TSMCU-5,6,7
 *  1	MCU_BIG		TSMCU-8,9
 *  2	MCU_CCI		TSMCU-5,6,7
 *  3	MFG(GPU)	TSMCU-4
 *  4	MDLA		TSMCU-1
 *  5	VPU		TSMCU-2
 *  6	TOP		TSMCU-2,3,4
 *  7	MD		TSMCU-0
 */

/* chip dependent */
/*
 * TO-DO: I assume AHB bus frequecy is 78MHz.
 * Please confirm it.
 */
/*
 * The tscpu_g_tc structure controls the polling rates and sensor mapping table
 * of all thermal controllers.  If HW thermal controllers are more than you
 * actually needed, you should pay attention to default setting of unneeded
 * thermal controllers.  Otherwise, these unneeded thermal controllers will be
 * initialized and work unexpectedly.
 */
struct thermal_controller tscpu_g_tc[THERMAL_CONTROLLER_NUM] = {
	[0] = {
		.ts = {L_TS_MCU1, L_TS_MCU2, L_TS_MCU3, L_TS_MCU4},
		.ts_number = 4,
		.dominator_ts_idx = 2, //hw protection ref TS (idx of .ts arr)
		.tc_offset = 0x0,
		.tc_speed = {
			0x0000000C,
			0x0001003B,
			0x0000030D
		} /* 4.9ms */
	},
	[1] = {
		.ts = {L_TS_MCU5, L_TS_MCU6, L_TS_MCU7},
		.ts_number = 3,
		.dominator_ts_idx = 0, //hw protection ref TS (idx of .ts arr)
		.tc_offset = 0x100,
		.tc_speed = {
			0x0000000C,
			0x0001003B,
			0x0000030D
		} /* 4.9ms */
	},
	[2] = {
		.ts = {L_TS_MCU8, L_TS_MCU9, L_TS_ABB, L_TS_MCU0},
		.ts_number = 4,
		.dominator_ts_idx = 1, //hw protection ref TS (idx of .ts arr)
		.tc_offset = 0x200,
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

static struct regmap *map;
static void __iomem *toprgu_base;
static unsigned int en_offset;
static unsigned int en_bit;
static unsigned int en_key;
static unsigned int irq_offset;
static unsigned int irq_bit;
static unsigned int irq_key;
static const struct regmap_config toprgu_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

int tscpu_debug_log;

#if MTK_TS_CPU_RT
static struct task_struct *ktp_thread_handle;
#endif

static __s32 g_adc_ge_t;
static __s32 g_adc_oe_t;
static __s32 g_o_vtsmcu0;
static __s32 g_o_vtsmcu1;
static __s32 g_o_vtsmcu2;
static __s32 g_o_vtsmcu3;
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
 * If curr_temp >= tscpu_polling_trip_temp1, use interval else if cur_temp >=
 * tscpu_polling_trip_temp2 && curr_temp < tscpu_polling_trip_temp1, use
 * interval*tscpu_polling_factor1 else, use interval*tscpu_polling_factor2
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

/*=============================================================*/

/* TODO: FIXME */
void get_thermal_slope_intercept(struct TS_PTPOD *ts_info, enum
thermal_bank_name ts_bank)
{
	unsigned int temp0, temp1, temp2;
	struct TS_PTPOD ts_ptpod;
	__s32 x_roomt;

	tscpu_dprintk("get_thermal_slope_intercept\n");

	/* chip dependent */

	/*
	 *   If there are two or more sensors in a bank, choose the sensor
	 *   calibration value of the dominant sensor. You can observe it in the
	 *   thermal doc provided by Thermal DE.  For example, Bank 1 is for SOC
	 *   + GPU. Observe all scenarios related to GPU tests to determine
	 *   which sensor is the highest temperature in all tests.  Then, It is
	 *   the dominant sensor.  (Confirmed by Thermal DE Alfred Tsai)
	 */
	/*
	 * PTP#	module		TSMCU Plan
	 *  0	MCU_LITTLE	TSMCU-5,6,7
	 *  1	MCU_BIG		TSMCU-8,9
	 *  2	MCU_CCI		TSMCU-5,6,7
	 *  3	MFG(GPU)	TSMCU-4
	 *  4	MDLA		TSMCU-1
	 *  5	VPU		TSMCU-2
	 *  6	TOP		TSMCU-2,3,4
	 *  7	MD		TSMCU-0
	 */

	switch (ts_bank) {
	case THERMAL_BANK0:
		x_roomt = g_x_roomt[L_TS_MCU5]; //TS_MCU5
		break;
	case THERMAL_BANK1:
		x_roomt = g_x_roomt[L_TS_MCU9]; //TS_MCU9
		break;
	case THERMAL_BANK2:
		x_roomt = g_x_roomt[L_TS_MCU5]; //TS_MCU5
		break;
	case THERMAL_BANK3:
		x_roomt = g_x_roomt[L_TS_MCU4]; //TS_MCU4
		break;
	case THERMAL_BANK4:
		x_roomt = g_x_roomt[L_TS_MCU1]; //TS_MCU1
		break;
	case THERMAL_BANK5:
		x_roomt = g_x_roomt[L_TS_MCU2]; //TS_MCU2
		break;
	case THERMAL_BANK6:
		x_roomt = g_x_roomt[L_TS_MCU2]; //TS_MCU2
		break;
	case THERMAL_BANK7:
		x_roomt = g_x_roomt[L_TS_MCU0]; //TS_MCU0
		break;
	default:                /*choose high temp */
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
static void mtkts_dump_cali_info(void)
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
	tscpu_printk("[cal] g_o_vtsmcu3     = %d\n", g_o_vtsmcu3);
	tscpu_printk("[cal] g_o_vtsmcu4     = %d\n", g_o_vtsmcu4);
	tscpu_printk("[cal] g_o_vtsmcu5     = %d\n", g_o_vtsmcu5);
	tscpu_printk("[cal] g_o_vtsmcu6     = %d\n", g_o_vtsmcu6);
	tscpu_printk("[cal] g_o_vtsmcu7     = %d\n", g_o_vtsmcu7);
	tscpu_printk("[cal] g_o_vtsmcu8     = %d\n", g_o_vtsmcu8);
	tscpu_printk("[cal] g_o_vtsmcu9     = %d\n", g_o_vtsmcu9);
	tscpu_printk("[cal] g_o_vtsabb      = %d\n", g_o_vtsabb);
}

#ifdef CONFIG_MTK_EFUSE
static void eDataCorrector(void)
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
	if (g_o_vtsmcu3 < -8 || g_o_vtsmcu3 > 484) {
		tscpu_warn("[thermal] Bad efuse data, g_o_vtsmcu3\n");
		g_o_vtsmcu3 = 260;
	}
	if (g_o_vtsmcu4 < -8 || g_o_vtsmcu4 > 484) {
		tscpu_warn("[thermal] Bad efuse data, g_o_vtsmcu4\n");
		g_o_vtsmcu4 = 260;
	}
	if (g_o_vtsmcu5 < -8 || g_o_vtsmcu5 > 484) { /* TODO */
		tscpu_warn("[thermal] Bad efuse data, g_o_vtsmcu5\n");
		g_o_vtsmcu5 = 260;
	}
	if (g_o_vtsmcu6 < -8 || g_o_vtsmcu6 > 484) { /* TODO */
		tscpu_warn("[thermal] Bad efuse data, g_o_vtsmcu6\n");
		g_o_vtsmcu6 = 260;
	}
	if (g_o_vtsmcu7 < -8 || g_o_vtsmcu7 > 484) { /* TODO */
		tscpu_warn("[thermal] Bad efuse data, g_o_vtsmcu7\n");
		g_o_vtsmcu7 = 260;
	}
	if (g_o_vtsmcu8 < -8 || g_o_vtsmcu8 > 484) { /* TODO */
		tscpu_warn("[thermal] Bad efuse data, g_o_vtsmcu8\n");
		g_o_vtsmcu8 = 260;
	}
	if (g_o_vtsmcu9 < -8 || g_o_vtsmcu9 > 484) { /* TODO */
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
#endif
void tscpu_thermal_cal_prepare(void)
{
#ifdef CONFIG_MTK_EFUSE
	struct device *dev = &tscpu_pdev->dev;
	struct device_node *node = tscpu_pdev->dev.of_node;
	struct nvmem_device *nvmem_dev;
	__u32 *efuse_val, *efuse_offset;
	__u32 efuse_num;
	int ret = 0, i;

	ret = of_property_read_u32(node, "therm_ctrl,efuse_num", &efuse_num);
	if (ret) {
		tscpu_warn("[thermal] Fail to get efuse_num error! ret= %d\n",
			ret);
		return;
	}

	if (!efuse_num) {
		tscpu_warn("[thermal] efuse_num cannot be 0\n");
		return;
	}

	efuse_offset = devm_kzalloc(dev,
		efuse_num * sizeof(__u32), GFP_KERNEL);
	if (IS_ERR(efuse_offset)) {
		tscpu_warn("[thermal] Alloc buf for efuse_offset failed!\n");
		return;
	}

	ret = of_property_read_u32_array(node, "therm_ctrl,efuse_offset",
			efuse_offset, efuse_num);
	if (ret) {
		tscpu_warn("[thermal] get efuse offset error! ret=%d\n", ret);
		devm_kfree(dev, (void *)efuse_offset);
		return;
	}

	efuse_val = devm_kzalloc(dev,
		efuse_num * sizeof(__u32), GFP_KERNEL);
	if (IS_ERR(efuse_val)) {
		tscpu_warn("[thermal] Alloc buf for efuse_val failed!\n");
		devm_kfree(dev, (void *)efuse_offset);
		return;
	}

	nvmem_dev = nvmem_device_get(dev, "mtk_efuse");
	if (IS_ERR(nvmem_dev)) {
		tscpu_warn("[thermal] failed to get mtk_efuse device\n");
		goto end;
	}
	for (i = 0; i < efuse_num; i++) {
		ret = nvmem_device_read(nvmem_dev,
			efuse_offset[i], sizeof(__u32), &efuse_val[i]);
		if (ret != sizeof(__u32)) {
			tscpu_warn(
				"[thermal] read efuse 0x%x failed, ret=%d\n",
				efuse_offset[i], ret);
			nvmem_device_put(nvmem_dev);
			goto end;
		} else {
			tscpu_printk("[calibration] efuse%d = 0x%x\n",
				efuse_offset[i], efuse_val[i]);
		}
	}
	nvmem_device_put(nvmem_dev);

	g_adc_ge_t = ((efuse_val[0] & _BITMASK_(31:22)) >> 22);
	g_adc_oe_t = ((efuse_val[0] & _BITMASK_(21:12)) >> 12);

	g_o_vtsmcu0 = ((efuse_val[1] & _BITMASK_(25:17)) >> 17);
	g_o_vtsmcu1 = ((efuse_val[1] & _BITMASK_(16:8)) >> 8);
	g_o_vtsmcu2 = (efuse_val[0] & _BITMASK_(8:0));
	g_o_vtsmcu3 = ((efuse_val[2] & _BITMASK_(31:23)) >> 23);
	g_o_vtsmcu4 = ((efuse_val[2] & _BITMASK_(13:5)) >> 5);
	g_o_vtsmcu5 = ((efuse_val[3] & _BITMASK_(31:23)) >> 23);
	g_o_vtsmcu6 = ((efuse_val[3] & _BITMASK_(22:14)) >> 14);
	g_o_vtsmcu7 = ((efuse_val[3] & _BITMASK_(13:5)) >> 5);
	g_o_vtsmcu8 = ((efuse_val[4] & _BITMASK_(31:23)) >> 23);
	g_o_vtsmcu9 = ((efuse_val[5] & _BITMASK_(31:23)) >> 23);
	g_o_vtsabb = ((efuse_val[2] & _BITMASK_(22:14)) >> 14);

	g_degc_cali = ((efuse_val[1] & _BITMASK_(6:1)) >> 1);
	g_adc_cali_en_t = (efuse_val[1] & _BIT_(0));
	g_o_slope_sign = ((efuse_val[1] & _BIT_(7)) >> 7);
	g_o_slope = ((efuse_val[1] & _BITMASK_(31:26)) >> 26);

	g_id = ((efuse_val[0] & _BIT_(9)) >> 9);

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
		g_o_vtsmcu3 = 260;
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

end:
	devm_kfree(dev, (void *)efuse_val);
	devm_kfree(dev, (void *)efuse_offset);
#endif
	mtkts_dump_cali_info();
}

void tscpu_thermal_cal_prepare_2(__u32 ret)
{
	__s32 format[L_TS_MCU_NUM] = { 0 };
	int i = 0;

	/* tscpu_printk("tscpu_thermal_cal_prepare_2\n"); */
	g_ge = ((g_adc_ge_t - 512) * 10000) / 4096;	/* ge * 10000 */
	g_oe = (g_adc_oe_t - 512);

	g_gain = (10000 + g_ge);

	format[0] = (g_o_vtsmcu0 + 3350 - g_oe);
	format[1] = (g_o_vtsmcu1 + 3350 - g_oe);
	format[2] = (g_o_vtsmcu2 + 3350 - g_oe);
	format[3] = (g_o_vtsmcu3 + 3350 - g_oe);
	format[4] = (g_o_vtsmcu4 + 3350 - g_oe);
	format[5] = (g_o_vtsmcu5 + 3350 - g_oe);
	format[6] = (g_o_vtsmcu6 + 3350 - g_oe);
	format[7] = (g_o_vtsmcu7 + 3350 - g_oe);
	format[8] = (g_o_vtsmcu8 + 3350 - g_oe);
	format[9] = (g_o_vtsmcu9 + 3350 - g_oe);
	format[10] = (g_o_vtsabb + 3350 - g_oe);

	for (i = 0; i < L_TS_MCU_NUM; i++) {
		/* x_roomt * 10000 */
		g_x_roomt[i] = (((format[i] * 10000) / 4096) * 10000) / g_gain;
	}

	tscpu_printk("[T_De][cal] g_ge         = %d\n", g_ge);
	tscpu_printk("[T_De][cal] g_gain       = %d\n", g_gain);

	for (i = 0; i < L_TS_MCU_NUM; i++)
		tscpu_printk("[T_De][cal] g_x_roomt%d   = %d\n", i,
							g_x_roomt[i]);
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

static void thermal_interrupt_handler(int tc_num)
{
	__u32 ret = 0, offset;

	offset = tscpu_g_tc[tc_num].tc_offset;

	ret = readl(offset + TEMPMONINTSTS);
	/* write to clear interrupt status */
	writel(ret, (void *)(offset + TEMPMONINTSTS));
	tscpu_dprintk(
		"[tIRQ] thermal_interrupt_handler,tc_num=0x%08x,ret=0x%08x\n",
		tc_num, ret);

	if (ret & THERMAL_MON_CINTSTS0)
		tscpu_dprintk(
		"[thermal_isr]: thermal sensor point 0 - cold interrupt trigger\n");

	if (ret & THERMAL_MON_HINTSTS0)
		tscpu_dprintk(
		"[thermal_isr]: thermal sensor point 0 - hot interrupt trigger\n");

	if (ret & THERMAL_MON_HINTSTS1)
		tscpu_dprintk(
		"[thermal_isr]: thermal sensor point 1 - hot interrupt trigger\n");

	if (ret & THERMAL_MON_HINTSTS2)
		tscpu_dprintk(
		"[thermal_isr]: thermal sensor point 2 - hot interrupt trigger\n");

	if (ret & THERMAL_tri_SPM_State0)
		tscpu_dprintk(
		"[thermal_isr]: Thermal state0 to trigger SPM state0\n");

	if (ret & THERMAL_tri_SPM_State1) {
		tscpu_dprintk(
		"[thermal_isr]: Thermal state1 to trigger SPM state1\n");
#if MTK_TS_CPU_RT
		wake_up_process(ktp_thread_handle);
#endif
	}

	if (ret & THERMAL_tri_SPM_State2)
		tscpu_printk(
			"[thermal_isr]: Thermal state2 to trigger SPM state2\n");
}

irqreturn_t tscpu_thermal_all_tc_interrupt_handler(int irq, void *dev_id)
{
	__u32 ret = 0, i, mask = 1;

	ret = readl(THERMINTST);

	/* MSB LSB NAME
	 * 6   6   LVTSINT3
	 * 5   5   LVTSINT2
	 * 4   4   LVTSINT1
	 * 3   3   LVTSINT0
	 * 2   2   THERMINT2
	 * 1   1   THERMINT1
	 * 0   0   THERMINT0
	 */

	ret = ret & 0x7F;

	tscpu_dprintk("thermal_interrupt_handler : THERMINTST = 0x%x\n", ret);

	for (i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++) {
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

	/* Calculating period unit in Module clock x 256,
	 * and the Module clock
	 * will be changed to 26M when Infrasys enters Sleep mode.
	 */

	/* bus clock 66M counting unit is 12 * 1/66M * 256 =
	 * 12 * 3.879us = 46.545 us
	 */
	writel(tempMonCtl1, (void *)(offset + TEMPMONCTL1));
	/*
	 * filt interval is 1 * 46.545us = 46.545us,
	 * sen interval is 429 * 46.545us = 19.968ms
	 */
	writel(tempMonCtl2, (void *)(offset + TEMPMONCTL2));
	/* AHB polling is 781* 1/66M = 11.833us */
	writel(tempAhbPoll, (void *)(offset + TEMPAHBPOLL));

#if THERMAL_CONTROLLER_HW_FILTER == 2
	/* temperature sampling control, 2 out of 4 samples */
	writel(0x00000492, (void *)(offset + TEMPMSRCTL0));
#elif THERMAL_CONTROLLER_HW_FILTER == 4
	/* temperature sampling control, 4 out of 6 samples */
	writel(0x000006DB, (void *)(offset + TEMPMSRCTL0));
#elif THERMAL_CONTROLLER_HW_FILTER == 8
	/* temperature sampling control, 8 out of 10 samples */
	writel(0x00000924, (void *)(offset + TEMPMSRCTL0));
#elif THERMAL_CONTROLLER_HW_FILTER == 16
	/* temperature sampling control, 16 out of 18 samples */
	writel(0x00000B6D, (void *)(offset + TEMPMSRCTL0));
#else				/* default 1 */
	/* temperature sampling control, 1 sample */
	writel(0x00000000, (void *)(offset + TEMPMSRCTL0));
#endif
	/* exceed this polling time, IRQ would be inserted */
	writel(0xFFFFFFFF, (void *)(offset + TEMPAHBTO));

	/* times for interrupt occurrance for SP0*/
	writel(0x00000000, (void *)(offset + TEMPMONIDET0));
	/* times for interrupt occurrance for SP1*/
	writel(0x00000000, (void *)(offset + TEMPMONIDET1));
	/* times for interrupt occurrance for SP2*/
	writel(0x00000000, (void *)(offset + TEMPMONIDET2));

	writel(0x800, (void *)(offset + TEMPADCMUX));
	/* AHB address for auxadc mux selection */
	writel((__u32) AUXADC_CON1_CLR_P,
			(void *)(offset + TEMPADCMUXADDR));

	/* AHB value for auxadc enable */
	writel(0x800, (void *)(offset + TEMPADCEN));

	/*
	 * AHB address for auxadc enable (channel 0 immediate mode selected)
	 * this value will be stored to TEMPADCENADDR automatically by hw
	 */
	writel((__u32) AUXADC_CON1_SET_P, (void *)(offset + TEMPADCENADDR));

	/* AHB address for auxadc valid bit */
	writel((__u32) AUXADC_DAT11_P,
			(void *)(offset + TEMPADCVALIDADDR));

	/* AHB address for auxadc voltage output */
	writel((__u32) AUXADC_DAT11_P,
			(void *)(offset + TEMPADCVOLTADDR));

	/* read valid & voltage are at the same register */
	writel(0x0, (void *)(offset + TEMPRDCTRL));

	/* indicate where the valid bit is (the 12th bit
	 * is valid bit and 1 is valid)
	 */
	writel(0x0000002C, (void *)(offset + TEMPADCVALIDMASK));

	/* do not need to shift */
	writel(0x0, (void *)(offset + TEMPADCVOLTAGESHIFT));
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
	tscpu_dprintk("set_tc_trigger_hw_protect t1=%d t2=%d\n",
					temperature, temperature2);

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
	raw_high = temperature_to_raw_room(temperature, ts_name);

	temp = readl(offset + TEMPMONINT);
	/* disable trigger SPM interrupt */
	writel(temp & 0x00000000, (void *)(offset + TEMPMONINT));

	/* Select protection sensor */
	config = ((d_index << 2) + 0x2) << 16;
	writel(config, (void *)(offset + TEMPPROTCTL));

	/* set hot to HOT wakeup event */
	writel(raw_high, (void *)(offset + TEMPPROTTC));

	/* enable trigger Hot SPM interrupt */
	writel(temp | 0x80000000, (void *)(offset + TEMPMONINT));
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
	raw = raw & 0x0fff;
#else
	raw = readl(tempmsr_name) & 0x0fff;
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
	writel((0x00001000 + temp), (void *)PTPSPARE2);

	/* counting unit is 320 * 31.25us = 10ms */
	writel(1, (void *)(offset + TEMPMONCTL1));

	/* sensing interval is 200 * 10ms = 2000ms */
	writel(1, (void *)(offset + TEMPMONCTL2));

	/* polling interval to check if temperature sense is ready */
	writel(1, (void *)(offset + TEMPAHBPOLL));

	/* exceed this polling time, IRQ would be inserted */
	writel(0x000000FF, (void *)(offset + TEMPAHBTO));

	/* times for interrupt occurrance */
	writel(0x00000000, (void *)(offset + TEMPMONIDET0));

	/* times for interrupt occurrance */
	writel(0x00000000, (void *)(offset + TEMPMONIDET1));

	/* temperature measurement sampling control */
	writel(0x0000000, (void *)(offset + TEMPMSRCTL0));

	/* this value will be stored to TEMPPNPMUXADDR (TEMPSPARE0)
	 * automatically by hw
	 */
	writel(0x1, (void *)(offset + TEMPADCPNP0));
	writel(0x2, (void *)(offset + TEMPADCPNP1));
	writel(0x3, (void *)(offset + TEMPADCPNP2));
	writel(0x4, (void *)(offset + TEMPADCPNP3));

	/* AHB address for pnp sensor mux selection */
	writel((__u32) PTPSPARE0_P, (void *)(offset + TEMPPNPMUXADDR));

	/* AHB address for auxadc mux selection */
	writel((__u32) PTPSPARE0_P, (void *)(offset + TEMPADCMUXADDR));

	/* AHB address for auxadc enable */
	writel((__u32) PTPSPARE1_P, (void *)(offset + TEMPADCENADDR));

	/* AHB address for auxadc valid bit */
	writel((__u32) PTPSPARE2_P, (void *)(offset + TEMPADCVALIDADDR));

	/* AHB address for auxadc voltage output */
	writel((__u32) PTPSPARE2_P, (void *)(offset + TEMPADCVOLTADDR));

	/* read valid & voltage are at the same register */
	writel(0x0, (void *)(offset + TEMPRDCTRL));

	/* indicate where the valid bit is (the 12th bit
	 * is valid bit and 1 is valid)
	 */
	writel(0x0000002C, (void *)(offset + TEMPADCVALIDMASK));

	/* do not need to shift */
	writel(0x0, (void *)(offset + TEMPADCVOLTAGESHIFT));

	/* enable auxadc mux & pnp write transaction */
	writel(0x3, (void *)(offset + TEMPADCWRITECTRL));


	/* enable all interrupt except filter sense and
	 * immediate sense interrupt
	 */
	writel(0x00000000, (void *)(offset + TEMPMONINT));

	/* enable all sensing point(sensing point 2 is unused)*/
	writel(0x0000000F, (void *)(offset + TEMPMONCTL0));


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
	case L_TS_MCU3:
		return TEMPADC_MCU3;
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
	/* tscpu_printk("thermal_pause_all_periodoc_temp_sensing\n"); */

	/* config bank0,1,2 */
	for (i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++) {
		if (tscpu_g_tc[i].ts_number == 0)
			continue;

		offset = tscpu_g_tc[i].tc_offset;
		temp = readl(offset + TEMPMSRCTL1);
		/* set bit8=bit1=bit2=bit3=1 to pause sensing point 0,1,2,3 */
		writel((temp | 0x10E), (void *)(offset + TEMPMSRCTL1));
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
		writel(((temp & (~0x10E))), (void *)(offset + TEMPMSRCTL1));
	}
}

static void tscpu_thermal_enable_all_periodoc_sensing_point(int tc_num)
{
	__u32 offset;

	offset = tscpu_g_tc[tc_num].tc_offset;

	switch (tscpu_g_tc[tc_num].ts_number) {
	case 1:
		/* enable periodoc temperature sensing point 0 */
		writel(0x00000001, (void *)(offset + TEMPMONCTL0));
		break;
	case 2:
		/* enable periodoc temperature sensing point 0,1 */
		writel(0x00000003, (void *)(offset + TEMPMONCTL0));
		break;
	case 3:
		/* enable periodoc temperature sensing point 0,1,2 */
		writel(0x00000007, (void *)(offset + TEMPMONCTL0));
		break;
	case 4:
		/* enable periodoc temperature sensing point 0,1,2,3*/
		writel(0x0000000F, (void *)(offset + TEMPMONCTL0));
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
		/* tscpu_printk("thermal_disable_all_periodoc_temp_sensing:"
		 *					"Bank_%d\n",i);
		 */
		writel(0x00000000, (void *)(offset + TEMPMONCTL0));
	}
}

static void tscpu_thermal_tempADCPNP(int tc_num, int adc, int order)
{
	__u32 offset;

	offset = tscpu_g_tc[tc_num].tc_offset;

	tscpu_dprintk("%s adc %x, order %d\n", __func__, adc, order);

	switch (order) {
	case 0:
		writel(adc, (void *)(offset + TEMPADCPNP0));
		break;
	case 1:
		writel(adc, (void *)(offset + TEMPADCPNP1));
		break;
	case 2:
		writel(adc, (void *)(offset + TEMPADCPNP2));
		break;
	case 3:
		writel(adc, (void *)(offset + TEMPADCPNP3));
		break;
	default:
		writel(adc, (void *)(offset + TEMPADCPNP0));
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
	writel(temp, (void *)AUXADC_CON0_V);

	/* disable auxadc channel 11 immediate mode */
	writel(0x800, (void *)AUXADC_CON1_CLR_V);

	for (i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++) {
		if (tscpu_g_tc[i].ts_number == 0)
			continue;

		offset = tscpu_g_tc[i].tc_offset;
		thermal_reset_and_initial(i);

		for (j = 0; j < tscpu_g_tc[i].ts_number; j++)
			tscpu_thermal_tempADCPNP(i, tscpu_thermal_ADCValueOfMcu
					(tscpu_g_tc[i].ts[j]), j);

		writel(TS_CONFIGURE_P, (void *)(offset + TEMPPNPMUXADDR));
		writel(0x3, (void *)(offset + TEMPADCWRITECTRL));
	}

	writel(0x800, (void *)AUXADC_CON1_SET_V);

	for (i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++) {
		if (tscpu_g_tc[i].ts_number == 0)
			continue;

		tscpu_thermal_enable_all_periodoc_sensing_point(i);
	}
}

void tscpu_config_all_tc_hw_protect(int temperature, int temperature2)
{
	int i = 0, ret;
	struct device_node *toprgu_np;
	struct device_node *np = tscpu_pdev->dev.of_node;
	unsigned int val = 0;

	tscpu_dprintk(
	"tscpu_config_all_tc_hw_protect,temperature=%d,temperature2=%d,\n",
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
	if (map == NULL) {
		toprgu_np = of_parse_phandle(np, "regmap", 0);
		if (!toprgu_np) {
			tscpu_warn("Unable to get troprgu_np device node\n");
			return;
		}

		if (!of_device_is_compatible(toprgu_np, "mediatek,toprgu")) {
			tscpu_warn("[thermal] get toprgu base failed!\n");
			return;
		}

		if (of_property_read_u32(np, "therm_ctrl,toprgu_en_offset",
			&en_offset)) {
			tscpu_warn("[thermal] get en_offset failed!\n");
			return;
		}

		if (of_property_read_u32(np, "therm_ctrl,toprgu_en_bit",
			&en_bit)) {
			tscpu_warn("[thermal] get en_mask failed!\n");
			return;
		}

		if (of_property_read_u32(np, "therm_ctrl,toprgu_en_key",
			&en_key)) {
			tscpu_warn("[thermal] get en_key failed!\n");
			return;
		}

		if (of_property_read_u32(np, "therm_ctrl,toprgu_irq_offset",
			&irq_offset)) {
			tscpu_warn("[thermal] get irq_offset failed!\n");
			return;
		}

		if (of_property_read_u32(np, "therm_ctrl,toprgu_irq_bit",
			&irq_bit)) {
			tscpu_warn("[thermal] get irq_bit failed!\n");
			return;
		}

		if (of_property_read_u32(np, "therm_ctrl,toprgu_irq_key",
			&irq_key)) {
			tscpu_warn("[thermal] get irq_key failed!\n");
			return;
		}

		toprgu_base = of_iomap(toprgu_np, 0);
		if (IS_ERR(toprgu_base)) {
			tscpu_warn("[thermal] iomap toprgu base failed!\n");
			return;
		}

		map = devm_regmap_init_mmio(&tscpu_pdev->dev, toprgu_base,
						     &toprgu_regmap_config);
		if (IS_ERR(map)) {
			tscpu_warn("[thermal] init mmio failed!\n");
			return;
		}
	}

	/* disable reset */
	ret = regmap_read(map, en_offset, &val);
	if (ret) {
		tscpu_warn("[thermal] Failed to read value of en_offset\n");
		return;
	}

	val &= ~(1 << en_bit);
	val |= en_key;
	regmap_write(map, en_offset, val);

	/* set to trigger reset instead of irq */
	regmap_read(map, irq_offset, &val);
	val &= ~(1 << irq_bit);
	val |= irq_key;
	regmap_write(map, irq_offset, val);

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

	/* enable reset */
	regmap_read(map, en_offset, &val);
	val |= 1 << en_bit;
	val |= en_key;
	regmap_write(map, en_offset, val);
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

	writel(temp, (void *)INFRA_GLOBALCON_RST_0_SET);

	/* TODO: How long to set the reset bit? */

	/* un reset */
	/* TODO: Is it necessary to read INFRA_GLOBALCON_RST_0_CLR? */
	temp = readl(INFRA_GLOBALCON_RST_0_CLR);
	/* 1: Enable reset Disables thermal control software reset */
	temp |= 0x00000001;
	writel(temp, (void *)INFRA_GLOBALCON_RST_0_CLR);
}

int tscpu_dump_cali_info(struct seq_file *m, void *v)
{
	int i;

	seq_printf(m, "[cal] g_adc_ge_t      = %d\n", g_adc_ge_t);
	seq_printf(m, "[cal] g_adc_oe_t      = %d\n", g_adc_oe_t);

	seq_printf(m, "[cal] g_o_vtsmcu0     = %d\n", g_o_vtsmcu0);
	seq_printf(m, "[cal] g_o_vtsmcu1     = %d\n", g_o_vtsmcu1);
	seq_printf(m, "[cal] g_o_vtsmcu2     = %d\n", g_o_vtsmcu2);
	seq_printf(m, "[cal] g_o_vtsmcu3     = %d\n", g_o_vtsmcu3);
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

