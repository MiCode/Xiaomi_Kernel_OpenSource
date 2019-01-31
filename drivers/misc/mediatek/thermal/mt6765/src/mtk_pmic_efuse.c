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
#include <linux/seq_file.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mach/mtk_thermal.h"
#include <mt-plat/upmu_common.h>
#include <mt-plat/mtk_auxadc_intf.h>
#include <mach/mtk_pmic.h>
#include <tspmic_settings.h>
/*=============================================================
 *Local variable definition
 *=============================================================
 */
int mtktspmic_debug_log;
/* Cali */
static __s32 g_o_vts;
static __s32 g_o_vts_2;
static __s32 g_o_vts_3;
static __s32 g_degc_cali;
static __s32 g_adc_cali_en;
static __s32 g_o_slope;
static __s32 g_o_slope_sign;
static __s32 g_id;
static __s32 g_slope1 = 1;
static __s32 g_slope2 = 1;
static __s32 g_intercept;
static __s32 g_tsbuck1_slope1 = 1;
static __s32 g_tsbuck1_slope2 = 1;
static __s32 g_tsbuck1_intercept;
static __s32 g_tsbuck2_slope1 = 1;
static __s32 g_tsbuck2_slope2 = 1;
static __s32 g_tsbuck2_intercept;

static DEFINE_MUTEX(TSPMIC_lock);
static int pre_temp1 = 0, PMIC_counter;
static int pre_tsbuck1_temp1 = 0, tsbuck1_cnt;
static int pre_tsbuck2_temp1 = 0, tsbuck2_cnt;
/*=============================================================*/

static __s32 pmic_raw_to_temp(__u32 ret)
{
	__s32 t_current;

	__s32 y_curr = ret;

	t_current = g_intercept + ((g_slope1 * y_curr) / (g_slope2));

	mtktspmic_dprintk("[pmic_raw_to_temp] t_current=%d\n", t_current);
	return t_current;
}

static __s32 tsbuck1_raw_to_temp(__u32 ret)
{
	__s32 t_current;

	__s32 y_curr = ret;

	t_current = g_tsbuck1_intercept +
		((g_tsbuck1_slope1 * y_curr) / (g_tsbuck1_slope2));

	mtktspmic_dprintk("[tsbuck1_raw_to_temp] %d, %d, %d, %d\n",
		y_curr, g_tsbuck1_intercept, g_tsbuck1_slope1,
		g_tsbuck1_slope2);

	mtktspmic_dprintk("[tsbuck1_raw_to_temp] t_current=%d\n", t_current);
	return t_current;
}

static __s32 tsbuck2_raw_to_temp(__u32 ret)
{
	__s32 t_current;

	__s32 y_curr = ret;

	t_current = g_tsbuck2_intercept +
		((g_tsbuck2_slope1 * y_curr) / (g_tsbuck2_slope2));

	mtktspmic_dprintk("[tsbuck2_raw_to_temp] %d, %d, %d, %d\n",
		y_curr, g_tsbuck2_intercept, g_tsbuck2_slope1,
		g_tsbuck2_slope2);

	mtktspmic_dprintk("[tsbuck2_raw_to_temp] t_current=%d\n", t_current);
	return t_current;
}

static void mtktspmic_read_efuse(void)
{
	mtktspmic_info("[pmic_debug]  start\n");
	g_adc_cali_en = pmic_get_register_value(PMIC_AUXADC_EFUSE_ADC_CALI_EN);
	g_degc_cali = pmic_get_register_value(PMIC_AUXADC_EFUSE_DEGC_CALI);
	g_o_vts = pmic_get_register_value(PMIC_AUXADC_EFUSE_O_VTS);
	g_o_vts_2 = pmic_get_register_value(PMIC_AUXADC_EFUSE_O_VTS_2);
	g_o_vts_3 = pmic_get_register_value(PMIC_AUXADC_EFUSE_O_VTS_3);

	g_o_slope_sign = pmic_get_register_value(
				PMIC_AUXADC_EFUSE_O_SLOPE_SIGN);
	g_o_slope = pmic_get_register_value(PMIC_AUXADC_EFUSE_O_SLOPE);
	g_id = pmic_get_register_value(PMIC_AUXADC_EFUSE_ID);

	mtktspmic_info("[pmic_debug] 6357_efuse: g_o_vts = %d\n",
								g_o_vts);
	mtktspmic_info("[pmic_debug] 6357_efuse: g_o_vts_2 = %d\n",
								g_o_vts_2);
	mtktspmic_info("[pmic_debug] 6358_efuse: g_o_vts_3 = %d\n",
								g_o_vts_3);
	mtktspmic_info("[pmic_debug] 6357_efuse: g_degc_cali = %d\n",
								g_degc_cali);
	mtktspmic_info("[pmic_debug] 6357_efuse: g_adc_cali_en = %d\n",
								g_adc_cali_en);
	mtktspmic_info("[pmic_debug] 6357_efuse: g_o_slope = %d\n",
								g_o_slope);
	mtktspmic_info("[pmic_debug] 6357_efuse: g_o_slope_sign = %d\n",
								g_o_slope_sign);
	mtktspmic_info("[pmic_debug] 6357_efuse: g_id = %d\n",
								g_id);
	mtktspmic_info("[pmic_debug]  end\n");
}

void mtktspmic_cali_prepare(void)
{
	mtktspmic_read_efuse();

	if (g_id == 0)
		g_o_slope = 0;

	/* g_adc_cali_en=0;//FIX ME */

	if (g_adc_cali_en == 0) {	/* no calibration */
		mtktspmic_info("[pmic_debug]  It isn't calibration values\n");
		g_o_vts = 1600;
		g_o_vts_2 = 1600;
		g_o_vts_3 = 1600;
		g_degc_cali = 50;
		g_o_slope = 0;
		g_o_slope_sign = 0;
	}

	/*SW workaround patch for mt6755 E2*/
	if (g_degc_cali < 38 || g_degc_cali > 60)
		g_degc_cali = 53;

	mtktspmic_info("[pmic_debug] g_o_vts        = 0x%x\n", g_o_vts);
	mtktspmic_info("[pmic_debug] g_o_vts_2      = 0x%x\n", g_o_vts_2);
	mtktspmic_info("[pmic_debug] g_o_vts_3      = 0x%x\n", g_o_vts_3);
	mtktspmic_info("[pmic_debug] g_degc_cali    = 0x%x\n", g_degc_cali);
	mtktspmic_info("[pmic_debug] g_adc_cali_en  = 0x%x\n", g_adc_cali_en);
	mtktspmic_info("[pmic_debug] g_o_slope      = 0x%x\n", g_o_slope);
	mtktspmic_info("[pmic_debug] g_o_slope_sign = 0x%x\n", g_o_slope_sign);
	mtktspmic_info("[pmic_debug] g_id           = 0x%x\n", g_id);

}

void mtktspmic_cali_prepare2(void)
{
	__s32 vbe_t;
	int factor;

	factor = 1681;

	g_slope1 = (100 * 1000 * 10);	/* 1000 is for 0.001 degree */

	if (g_o_slope_sign == 0)
		g_slope2 = -(factor + g_o_slope);
	else
		g_slope2 = -(factor - g_o_slope);

	vbe_t = (-1) * ((((g_o_vts) * 1800)) / 4096) * 1000;

	if (g_o_slope_sign == 0)
		/*0.001 degree */
		g_intercept = (vbe_t * 1000) / (-(factor + g_o_slope * 10));
	else
		/*0.001 degree */
		g_intercept = (vbe_t * 1000) / (-(factor - g_o_slope * 10));

	/* 1000 is for 0.1 degree */
	g_intercept = g_intercept + (g_degc_cali * (1000 / 2));

	mtktspmic_info("[Thermal calibration] SLOPE1=%d SLOPE2=%d INTERCEPT=%d, Vbe = %d\n",
			g_slope1, g_slope2, g_intercept, vbe_t);

	factor = 1863;

	g_tsbuck1_slope1 = (100 * 1000 * 10);	/* 1000 is for 0.001 degree */

	if (g_o_slope_sign == 0)
		g_tsbuck1_slope2 = -(factor + g_o_slope);
	else
		g_tsbuck1_slope2 = -(factor - g_o_slope);

	vbe_t = (-1) * ((((g_o_vts_2) * 1800)) / 4096) * 1000;

	if (g_o_slope_sign == 0)
		g_tsbuck1_intercept =
		(vbe_t * 1000) / (-(factor + g_o_slope * 10));
	else
		g_tsbuck1_intercept =
		(vbe_t * 1000) / (-(factor - g_o_slope * 10));

	g_tsbuck1_intercept = g_tsbuck1_intercept +
		(g_degc_cali * (1000 / 2)); /* 1000 is for 0.1 degree */

	mtktspmic_info(
	"[Thermal calibration] SLOPE1=%d SLOPE2=%d INTERCEPT=%d, Vbe = %d\n",
	g_tsbuck1_slope1, g_tsbuck1_slope2,	g_tsbuck1_intercept, vbe_t);
	factor = 1863;

	g_tsbuck2_slope1 = (100 * 1000 * 10);	/* 1000 is for 0.001 degree */

	if (g_o_slope_sign == 0)
		g_tsbuck2_slope2 = -(factor + g_o_slope);
	else
		g_tsbuck2_slope2 = -(factor - g_o_slope);

	vbe_t = (-1) * ((((g_o_vts_3) * 1800)) / 4096) * 1000;

	if (g_o_slope_sign == 0)
		g_tsbuck2_intercept =
		(vbe_t * 1000) / (-(factor + g_o_slope * 10));
		/*0.001 degree */
	else
		g_tsbuck2_intercept =
		(vbe_t * 1000) / (-(factor - g_o_slope * 10));
		/*0.001 degree */

	g_tsbuck2_intercept = g_tsbuck2_intercept + (g_degc_cali * (1000 / 2));
		/* 1000 is for 0.1 degree */

	mtktspmic_info(
	"[Thermal calibration] SLOPE1=%d SLOPE2=%d INTERCEPT=%d, Vbe = %d\n",
	g_tsbuck2_slope1, g_tsbuck2_slope2, g_tsbuck2_intercept, vbe_t);
}

int mtktspmic_get_hw_temp(void)
{
	int temp = 0, temp1 = 0;

	mutex_lock(&TSPMIC_lock);

	temp = pmic_get_auxadc_value(AUXADC_LIST_MT6357_CHIP_TEMP);

	temp1 = pmic_raw_to_temp(temp);

	mtktspmic_dprintk("[pmic_debug] Raw=%d, T=%d\n", temp, temp1);

	if ((temp1 > 100000) || (temp1 < -30000))
		mtktspmic_info("[mtktspmic_get_hw_temp] raw=%d, PMIC T=%d",
								temp, temp1);

	if ((temp1 > 150000) || (temp1 < -50000)) {
		mtktspmic_info("[mtktspmic_get_hw_temp] temp(%d) too high, drop this data!\n",
				temp1);
		temp1 = pre_temp1;
	} else if ((PMIC_counter != 0)
			&& (((pre_temp1 - temp1) > 30000)
			|| ((temp1 - pre_temp1) > 30000))) {
		mtktspmic_info("[mtktspmic_get_hw_temp] temp diff too large, drop this data\n");
		temp1 = pre_temp1;
	} else {
		/* update previous temp */
		pre_temp1 = temp1;
		mtktspmic_dprintk("[mtktspmic_get_hw_temp] pre_temp1=%d\n",
								pre_temp1);

		if (PMIC_counter == 0)
			PMIC_counter++;
	}

	mutex_unlock(&TSPMIC_lock);

	return temp1;
}

int mt6357tsbuck1_get_hw_temp(void)
{
	int temp = 0, temp1 = 0;

	mutex_lock(&TSPMIC_lock);

	temp = pmic_get_auxadc_value(AUXADC_LIST_MT6357_BUCK1_TEMP);

	temp1 = tsbuck1_raw_to_temp(temp);

	mtktspmic_dprintk("%s raw=%d T=%d\n", __func__, temp, temp1);

	if ((temp1 > 100000) || (temp1 < -30000))
		mtktspmic_info("%s raw=%d T=%d\n", __func__, temp, temp1);

	if ((temp1 > 150000) || (temp1 < -50000)) {
		mtktspmic_info("%s T=%d too high, drop it!\n", __func__,
				   temp1);
		temp1 = pre_tsbuck1_temp1;
	} else if ((tsbuck1_cnt != 0)
		   && (((pre_tsbuck1_temp1 - temp1) > 30000) ||
		   ((temp1 - pre_tsbuck1_temp1) > 30000))) {
		mtktspmic_info("%s delta temp too large, drop it!\n", __func__);
		temp1 = pre_tsbuck1_temp1;
	} else {
		/* update previous temp */
		pre_tsbuck1_temp1 = temp1;
		mtktspmic_dprintk("%s pre_tsbuck1_temp1=%d\n",
			__func__, pre_tsbuck1_temp1);

		if (tsbuck1_cnt == 0)
			tsbuck1_cnt++;
	}

	mutex_unlock(&TSPMIC_lock);

	return temp1;
}

int mt6357tsbuck2_get_hw_temp(void)
{
	int temp = 0, temp1 = 0;

	mutex_lock(&TSPMIC_lock);

	temp = pmic_get_auxadc_value(AUXADC_LIST_MT6357_BUCK2_TEMP);

	temp1 = tsbuck2_raw_to_temp(temp);

	mtktspmic_dprintk("%s raw=%d T=%d\n", __func__, temp, temp1);

	if ((temp1 > 100000) || (temp1 < -30000))
		mtktspmic_info("%s raw=%d T=%d\n", __func__, temp, temp1);

	if ((temp1 > 150000) || (temp1 < -50000)) {
		mtktspmic_info("%s T=%d too high, drop it!\n", __func__,
				   temp1);
		temp1 = pre_tsbuck2_temp1;
	} else if ((tsbuck2_cnt != 0)
			&& (((pre_tsbuck2_temp1 - temp1) > 30000) ||
			((temp1 - pre_tsbuck2_temp1) > 30000))) {
		mtktspmic_info("%s delta temp too large, drop it!\n", __func__);
		temp1 = pre_tsbuck2_temp1;
	} else {
		/* update previous temp */
		pre_tsbuck2_temp1 = temp1;
		mtktspmic_dprintk("%s pre_tsbuck2_temp1=%d\n",
			__func__, pre_tsbuck2_temp1);

		if (tsbuck2_cnt == 0)
			tsbuck2_cnt++;
	}

	mutex_unlock(&TSPMIC_lock);

	return temp1;
}

