/*
 * Copyright (C) 2015 MediaTek Inc.
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


#include <linux/delay.h>
#include <linux/time.h>
#include <linux/platform_device.h>

#include <asm/div64.h>

#include <mt-plat/upmu_common.h>

#include <mach/mtk_battery_property.h>
#include <mach/mtk_pmic.h>
#include <mt-plat/charger_type.h>
#include <mt-plat/mtk_battery.h>
#include <mt-plat/upmu_common.h>
#include <mt-plat/mtk_rtc_hal_common.h>
#include <mt-plat/mtk_rtc.h>
#include "include/pmic_throttling_dlpt.h"
#include <linux/proc_fs.h>
#include <linux/math64.h>

#include <mtk_gauge_class.h>
#include <mtk_battery_internal.h>

#define SWCHR_POWER_PATH

#define UNIT_FGCURRENT     (381470)
#define UNIT_FGCAR         (108507)
#define R_VAL_TEMP_2         (2)
#define R_VAL_TEMP_3         (3)
#define UNIT_TIME          (50)
#define UNIT_FGCAR_ZCV     (90735)
#define UNIT_FG_IAVG		(190735)
#define CAR_TO_REG_FACTOR  (0x49BA)

static signed int g_hw_ocv_tune_value;
static bool g_fg_is_charger_exist;

struct mt6355_gauge {
	const char *gauge_dev_name;
	struct gauge_device *gauge_dev;
	struct gauge_properties gauge_prop;
};

#define VOLTAGE_FULL_RANGE    1800
#define ADC_PRECISE           32768	/* 12 bits */

enum {
	FROM_SW_OCV = 1,
	FROM_6355_PLUG_IN,
	FROM_6355_PON_ON,
	FROM_6336_CHR_IN
};


int MV_to_REG_12_value(signed int _reg)
{
	int ret = (_reg * 4096) / (VOLTAGE_FULL_RANGE * 10 * R_VAL_TEMP_3);

	bm_trace("[MV_to_REG_12_value] %d => %d\n", _reg, ret);
	return ret;
}

static int MV_to_REG_12_temp_value(signed int _reg)
{
	int ret = (_reg * 4096) / (VOLTAGE_FULL_RANGE * 10 * R_VAL_TEMP_2);

	bm_trace("[MV_to_REG_12_temp_value] %d => %d\n", _reg, ret);
	return ret;
}

static signed int REG_to_MV_value(signed int _reg)
{
	long long _reg64 = _reg;
	int ret;

#if defined(__LP64__) || defined(_LP64)
	_reg64 =
	(_reg64 * VOLTAGE_FULL_RANGE * 10 * R_VAL_TEMP_3) / ADC_PRECISE;
#else
	_reg64 = div_s64(
	_reg64 * VOLTAGE_FULL_RANGE * 10 * R_VAL_TEMP_3, ADC_PRECISE);
#endif
	ret = _reg64;

	bm_trace("[REG_to_MV_value] %lld => %d\n", _reg64, ret);
	return ret;
}

static signed int MV_to_REG_value(signed int _mv)
{
	int ret =
		(_mv * ADC_PRECISE) / (VOLTAGE_FULL_RANGE * 10 * R_VAL_TEMP_3);

	bm_trace("[MV_to_REG_value] %d => %d\n", _mv, ret);
	return ret;
}

static unsigned int fg_get_data_ready_status(void)
{
	unsigned int ret = 0;
	unsigned int temp_val = 0;

	ret = pmic_read_interface(
		PMIC_FG_LATCHDATA_ST_ADDR, &temp_val, 0xFFFF, 0x0);

	temp_val =
	(temp_val & (PMIC_FG_LATCHDATA_ST_MASK << PMIC_FG_LATCHDATA_ST_SHIFT))
	>> PMIC_FG_LATCHDATA_ST_SHIFT;

	return temp_val;
}

void read_fg_hw_info_current_1(struct gauge_device *gauge_dev)
{
	long long fg_current_1_reg;
	signed int dvalue;
	long long Temp_Value;
	int sign_bit = 0;

	fg_current_1_reg = pmic_get_register_value(PMIC_FG_CURRENT_OUT);

	/*calculate the real world data    */
	dvalue = (unsigned int) fg_current_1_reg;
	if (dvalue == 0) {
		Temp_Value = (long long) dvalue;
		sign_bit = 0;
	} else if (dvalue > 32767) {
		/* > 0x8000 */
		Temp_Value = (long long) (dvalue - 65535);
		Temp_Value = Temp_Value - (Temp_Value * 2);
		sign_bit = 1;
	} else {
		Temp_Value = (long long) dvalue;
		sign_bit = 0;
	}

	Temp_Value = Temp_Value * UNIT_FGCURRENT;
#if defined(__LP64__) || defined(_LP64)
	do_div(Temp_Value, 100000);
#else
	Temp_Value = div_s64(Temp_Value, 100000);
#endif

	dvalue = (unsigned int) Temp_Value;

	if (gauge_dev->fg_cust_data->r_fg_value != 100)
		dvalue = (dvalue * 100) / gauge_dev->fg_cust_data->r_fg_value;

	if (sign_bit == 1)
		dvalue = dvalue - (dvalue * 2);

	gauge_dev->fg_hw_info.current_1 =
		((dvalue * gauge_dev->fg_cust_data->car_tune_value) / 1000);

}

void read_fg_hw_info_current_2(struct gauge_device *gauge_dev)
{
	long long fg_current_2_reg;
	signed int dvalue;
	long long Temp_Value;
	int sign_bit = 0;

	fg_current_2_reg = pmic_get_register_value(PMIC_FG_CIC2);

	/*calculate the real world data    */
	dvalue = (unsigned int) fg_current_2_reg;
	if (dvalue == 0) {
		Temp_Value = (long long) dvalue;
		sign_bit = 0;
	} else if (dvalue > 32767) {
		/* > 0x8000 */
		Temp_Value = (long long) (dvalue - 65535);
		Temp_Value = Temp_Value - (Temp_Value * 2);
		sign_bit = 1;
	} else {
		Temp_Value = (long long) dvalue;
		sign_bit = 0;
	}

	Temp_Value = Temp_Value * UNIT_FGCURRENT;
#if defined(__LP64__) || defined(_LP64)
	do_div(Temp_Value, 100000);
#else
	Temp_Value = div_s64(Temp_Value, 100000);
#endif
	dvalue = (unsigned int) Temp_Value;

	if (gauge_dev->fg_cust_data->r_fg_value != 100)
		dvalue = (dvalue * 100) / gauge_dev->fg_cust_data->r_fg_value;

	if (sign_bit == 1)
		dvalue = dvalue - (dvalue * 2);

	gauge_dev->fg_hw_info.current_2 =
		((dvalue * gauge_dev->fg_cust_data->car_tune_value) / 1000);

}


static void read_fg_hw_info_Iavg(
	struct gauge_device *gauge_dev, int *is_iavg_valid)
{
	long long fg_iavg_reg = 0;
	long long fg_iavg_reg_tmp = 0;
	long long fg_iavg_ma = 0;
	int fg_iavg_reg_27_16 = 0;
	int fg_iavg_reg_15_00 = 0;
	int sign_bit = 0;
	int is_bat_charging;
	int valid_bit;

	valid_bit = pmic_get_register_value(PMIC_FG_IAVG_VLD);
	*is_iavg_valid = valid_bit;

	if (valid_bit == 1) {
		fg_iavg_reg_27_16 = pmic_get_register_value(PMIC_FG_IAVG_27_16);
		fg_iavg_reg_15_00 = pmic_get_register_value(PMIC_FG_IAVG_15_00);
		fg_iavg_reg = (fg_iavg_reg_27_16 << 16) + fg_iavg_reg_15_00;
		sign_bit = (fg_iavg_reg_27_16 & 0x800) >> 11;

		if (sign_bit) {
			fg_iavg_reg_tmp = fg_iavg_reg;
			fg_iavg_reg = 0xfffffff - fg_iavg_reg_tmp + 1;
		}

		if (sign_bit)
			is_bat_charging = 0;	/* discharge */
		else
			is_bat_charging = 1;	/* charge */

		fg_iavg_ma = fg_iavg_reg * UNIT_FG_IAVG *
			gauge_dev->fg_cust_data->car_tune_value;

#if defined(__LP64__) || defined(_LP64)
		do_div(fg_iavg_ma, 1000000);
		do_div(fg_iavg_ma, gauge_dev->fg_cust_data->r_fg_value);
#else
		fg_iavg_ma = div_s64(fg_iavg_ma, 1000000);
		fg_iavg_ma = div_s64(fg_iavg_ma,
			gauge_dev->fg_cust_data->r_fg_value);
#endif

		if (sign_bit == 1)
			fg_iavg_ma = 0 - fg_iavg_ma;

		gauge_dev->fg_hw_info.current_avg = fg_iavg_ma;
		gauge_dev->fg_hw_info.current_avg_sign = sign_bit;
	} else {
		read_fg_hw_info_current_1(gauge_dev);
		gauge_dev->fg_hw_info.current_avg =
			gauge_dev->fg_hw_info.current_1;
		is_bat_charging = 0;	/* discharge */
	}

	bm_debug(
		"[read_fg_hw_info_Iavg] fg_iavg_reg 0x%llx fg_iavg_reg_tmp 0x%llx 27_16 0x%x 15_00 0x%x\n",
			fg_iavg_reg, fg_iavg_reg_tmp,
			fg_iavg_reg_27_16, fg_iavg_reg_15_00);
	bm_debug(
		"[read_fg_hw_info_Iavg] is_bat_charging %d fg_iavg_ma 0x%llx\n",
			is_bat_charging, fg_iavg_ma);


}

static signed int fg_get_current_iavg(struct gauge_device *gauge_dev, int *data)
{
	long long fg_iavg_reg = 0;
	long long fg_iavg_reg_tmp = 0;
	long long fg_iavg_ma = 0;
	int fg_iavg_reg_27_16 = 0;
	int fg_iavg_reg_15_00 = 0;
	int sign_bit = 0;
	int is_bat_charging;
	int ret, m;

	/* Set Read Latchdata */
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0001, 0x000F, 0x0);
	m = 0;
		while (fg_get_data_ready_status() == 0) {
			m++;
			if (m > 1000) {
				bm_err(
				"[fg_get_current_iavg] fg_get_data_ready_status timeout 1 !\r\n");
				break;
			}
		}

	if (pmic_get_register_value(PMIC_FG_IAVG_VLD) == 1) {
		fg_iavg_reg_27_16 = pmic_get_register_value(PMIC_FG_IAVG_27_16);
		fg_iavg_reg_15_00 = pmic_get_register_value(PMIC_FG_IAVG_15_00);
		fg_iavg_reg = (fg_iavg_reg_27_16 << 16) + fg_iavg_reg_15_00;
		sign_bit = (fg_iavg_reg_27_16 & 0x800) >> 11;

		if (sign_bit) {
			fg_iavg_reg_tmp = fg_iavg_reg;
			/*fg_iavg_reg = fg_iavg_reg_tmp - 0xfffffff - 1;*/
			fg_iavg_reg = 0xfffffff - fg_iavg_reg_tmp + 1;
		}

		if (sign_bit == 1)
			is_bat_charging = 0;	/* discharge */
		else
			is_bat_charging = 1;	/* charge */

		fg_iavg_ma = fg_iavg_reg * UNIT_FG_IAVG *
			gauge_dev->fg_cust_data->car_tune_value;
		bm_trace(
			"[fg_get_current_iavg] iavg_ma %lld iavg_reg %lld iavg_reg_tmp %lld\n",
			fg_iavg_ma, fg_iavg_reg, fg_iavg_reg_tmp);

#if defined(__LP64__) || defined(_LP64)
		do_div(fg_iavg_ma, 1000000);
		do_div(fg_iavg_ma, gauge_dev->fg_cust_data->r_fg_value);
#else
		fg_iavg_ma = div_s64(fg_iavg_ma, 1000000);
		fg_iavg_ma = div_s64(fg_iavg_ma,
			gauge_dev->fg_cust_data->r_fg_value);
#endif

		bm_trace("[fg_get_current_iavg] fg_iavg_ma %lld\n", fg_iavg_ma);


		if (sign_bit == 1)
			fg_iavg_ma = 0 - fg_iavg_ma;

		bm_trace(
			"[fg_get_current_iavg] fg_iavg_ma %lld fg_iavg_reg %lld r_fg_value %d 27_16 0x%x 15_00 0x%x\n",
			fg_iavg_ma, fg_iavg_reg,
			gauge_dev->fg_cust_data->r_fg_value,
			fg_iavg_reg_27_16, fg_iavg_reg_15_00);
			gauge_dev->fg_hw_info.current_avg = fg_iavg_ma;
			gauge_dev->fg_hw_info.current_avg_sign = sign_bit;
		bm_trace("[fg_get_current_iavg] PMIC_FG_IAVG_VLD == 1\n");
	} else {
		read_fg_hw_info_current_1(gauge_dev);
		gauge_dev->fg_hw_info.current_avg =
			gauge_dev->fg_hw_info.current_1;
		if (gauge_dev->fg_hw_info.current_1 < 0)
			gauge_dev->fg_hw_info.current_avg_sign = 1;
		bm_debug("[fg_get_current_iavg] PMIC_FG_IAVG_VLD != 1, avg %d, current_1 %d\n",
			gauge_dev->fg_hw_info.current_avg,
			gauge_dev->fg_hw_info.current_1);
	}

	/* recover read */
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0008, 0x000F, 0x0);
	m = 0;
		while (fg_get_data_ready_status() != 0) {
			m++;
			if (m > 1000) {
				bm_err(
					"[fg_get_current_iavg] fg_get_data_ready_status timeout 2 !\r\n");
				break;
			}
		}
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0000, 0x000F, 0x0);

	*data = gauge_dev->fg_hw_info.current_avg;

	bm_debug("[fg_get_current_iavg] %d\n", *data);

	return 0;
}


static signed int fg_set_iavg_intr(struct gauge_device *gauge_dev, void *data)
{
	int iavg_gap = *(unsigned int *) (data);
	int iavg;
	long long iavg_ht, iavg_lt;
	int ret;
	int sign_bit_ht, sign_bit_lt;
	long long fg_iavg_reg_ht, fg_iavg_reg_lt;
	int fg_iavg_lth_28_16, fg_iavg_lth_15_00;
	int fg_iavg_hth_28_16, fg_iavg_hth_15_00;

/* fg_iavg_ma = fg_iavg_reg * UNIT_FG_IAVG * fg_cust_data.car_tune_value */
/* fg_iavg_ma = fg_iavg_ma / 1000 / 1000 / fg_cust_data.r_fg_value; */

	ret = fg_get_current_iavg(gauge_dev, &iavg);

	iavg_ht = abs(iavg) + iavg_gap;
	iavg_lt = abs(iavg) - iavg_gap;
	if (iavg_lt <= 0)
		iavg_lt = 0;

	get_mtk_battery()->hw_status.iavg_ht = iavg_ht;
	get_mtk_battery()->hw_status.iavg_lt = iavg_lt;

	fg_iavg_reg_ht = iavg_ht * 1000 * 1000 *
		gauge_dev->fg_cust_data->r_fg_value;
	if (fg_iavg_reg_ht < 0) {
		sign_bit_ht = 1;
		fg_iavg_reg_ht = 0x1fffffff - fg_iavg_reg_ht + 1;
	} else
		sign_bit_ht = 0;

#if defined(__LP64__) || defined(_LP64)
	do_div(fg_iavg_reg_ht, UNIT_FG_IAVG);
	do_div(fg_iavg_reg_ht, gauge_dev->fg_cust_data->car_tune_value);
#else
	fg_iavg_reg_ht = div_s64(fg_iavg_reg_ht, UNIT_FG_IAVG);
	fg_iavg_reg_ht = div_s64(fg_iavg_reg_ht,
		gauge_dev->fg_cust_data->car_tune_value);
#endif
	if (sign_bit_ht == 1)
		fg_iavg_reg_ht = fg_iavg_reg_ht - (fg_iavg_reg_ht * 2);

	fg_iavg_reg_lt = iavg_lt * 1000 * 1000 *
		gauge_dev->fg_cust_data->r_fg_value;
	if (fg_iavg_reg_lt < 0) {
		sign_bit_lt = 1;
		fg_iavg_reg_lt = 0x1fffffff - fg_iavg_reg_lt + 1;
	} else
		sign_bit_lt = 0;
#if defined(__LP64__) || defined(_LP64)
	do_div(fg_iavg_reg_lt, UNIT_FG_IAVG);
	do_div(fg_iavg_reg_lt, gauge_dev->fg_cust_data->car_tune_value);
#else
	fg_iavg_reg_lt = div_s64(fg_iavg_reg_lt, UNIT_FG_IAVG);
	fg_iavg_reg_lt = div_s64(fg_iavg_reg_lt,
		gauge_dev->fg_cust_data->car_tune_value);
#endif
	if (sign_bit_lt == 1)
		fg_iavg_reg_lt = fg_iavg_reg_lt - (fg_iavg_reg_lt * 2);



	fg_iavg_lth_28_16 = (fg_iavg_reg_lt & 0x1fff0000) >> 16;
	fg_iavg_lth_15_00 = fg_iavg_reg_lt & 0xffff;
	fg_iavg_hth_28_16 = (fg_iavg_reg_ht & 0x1fff0000) >> 16;
	fg_iavg_hth_15_00 = fg_iavg_reg_ht & 0xffff;

	pmic_enable_interrupt(FG_IAVG_H_NO, 0, "GM30");
	pmic_enable_interrupt(FG_IAVG_L_NO, 0, "GM30");

	pmic_set_register_value(PMIC_FG_IAVG_LTH_28_16, fg_iavg_lth_28_16);
	pmic_set_register_value(PMIC_FG_IAVG_LTH_15_00, fg_iavg_lth_15_00);
	pmic_set_register_value(PMIC_FG_IAVG_HTH_28_16, fg_iavg_hth_28_16);
	pmic_set_register_value(PMIC_FG_IAVG_HTH_15_00, fg_iavg_hth_15_00);

	pmic_enable_interrupt(FG_IAVG_H_NO, 1, "GM30");
	if (iavg_lt > 0)
		pmic_enable_interrupt(FG_IAVG_L_NO, 1, "GM30");
	else
		pmic_enable_interrupt(FG_IAVG_L_NO, 0, "GM30");

	bm_debug("[FG_IAVG_INT][fg_set_iavg_intr] iavg %d iavg_gap %d iavg_ht %lld iavg_lt %lld fg_iavg_reg_ht %lld fg_iavg_reg_lt %lld\n",
			iavg, iavg_gap, iavg_ht, iavg_lt,
			fg_iavg_reg_ht, fg_iavg_reg_lt);
	bm_debug("[FG_IAVG_INT][fg_set_iavg_intr] lt_28_16 0x%x lt_15_00 0x%x ht_28_16 0x%x ht_15_00 0x%x\n",
			fg_iavg_lth_28_16, fg_iavg_lth_15_00,
			fg_iavg_hth_28_16, fg_iavg_hth_15_00);

	pmic_set_register_value(PMIC_RG_INT_EN_FG_IAVG_H, 1);
	if (iavg_lt > 0)
		pmic_set_register_value(PMIC_RG_INT_EN_FG_IAVG_L, 1);
	else
		pmic_set_register_value(PMIC_RG_INT_EN_FG_IAVG_L, 0);

	return 0;
}

void read_fg_hw_info_ncar(struct gauge_device *gauge_dev)
{
	unsigned int uvalue32_NCAR = 0;
	unsigned int uvalue32_NCAR_MSB = 0;
	unsigned int temp_NCAR_15_0 = 0;
	unsigned int temp_NCAR_31_16 = 0;
	signed int dvalue_NCAR = 0;
	long long Temp_Value = 0;

	temp_NCAR_15_0 = pmic_get_register_value(PMIC_FG_NCAR_15_00);
	temp_NCAR_31_16 = pmic_get_register_value(PMIC_FG_NCAR_31_16);

	uvalue32_NCAR = temp_NCAR_15_0 >> 11;
	uvalue32_NCAR |= ((temp_NCAR_31_16) & 0x7FFF) << 5;

	uvalue32_NCAR_MSB = (temp_NCAR_31_16 & 0x8000) >> 15;

	/*calculate the real world data    */
	dvalue_NCAR = (signed int) uvalue32_NCAR;

	if (uvalue32_NCAR == 0) {
		Temp_Value = 0;
	} else if (uvalue32_NCAR == 0xfffff) {
		Temp_Value = 0;
	} else if (uvalue32_NCAR_MSB == 0x1) {
		/* dis-charging */
		Temp_Value = (long long) (dvalue_NCAR - 0xfffff);
		Temp_Value = Temp_Value - (Temp_Value * 2);
	} else {
		/*charging */
		Temp_Value = (long long) dvalue_NCAR;
	}

	/* 0.1 mAh */
#if defined(__LP64__) || defined(_LP64)
	Temp_Value = Temp_Value * UNIT_FGCAR / 1000;
#else
	Temp_Value = div_s64(Temp_Value * UNIT_FGCAR, 1000);
#endif

#if defined(__LP64__) || defined(_LP64)
	do_div(Temp_Value, 10);
	Temp_Value = Temp_Value + 5;
	do_div(Temp_Value, 10);
#else
	Temp_Value = div_s64(Temp_Value, 10);
	Temp_Value = Temp_Value + 5;
	Temp_Value = div_s64(Temp_Value, 10);
#endif

	if (uvalue32_NCAR_MSB == 0x1)
		dvalue_NCAR =
		(signed int) (Temp_Value - (Temp_Value * 2));
	else
		dvalue_NCAR = (signed int) Temp_Value;

	/*Auto adjust value*/
	if (gauge_dev->fg_cust_data->r_fg_value != 100)
		dvalue_NCAR = (dvalue_NCAR * 100) /
		gauge_dev->fg_cust_data->r_fg_value;

	gauge_dev->fg_hw_info.ncar =
		((dvalue_NCAR *
		gauge_dev->fg_cust_data->car_tune_value) / 1000);

}


static int gspare0_reg, gspare3_reg;
static int rtc_invalid;
static int is_bat_plugout;
static int bat_plug_out_time;

static void fgauge_read_RTC_boot_status(void)
{
	int hw_id = pmic_get_register_value(PMIC_HWCID);
	unsigned int spare0_reg = 0;
	unsigned int spare0_reg_b13 = 0;
	int spare3_reg = 0;
	int spare3_reg_valid = 0;

	spare0_reg = hal_rtc_get_spare_register(RTC_FG_INIT);
	spare3_reg = get_rtc_spare_fg_value();
	gspare0_reg = spare0_reg;
	gspare3_reg = spare3_reg;
	spare3_reg_valid = (spare3_reg & 0x80) >> 7;

	if (spare3_reg_valid == 0)
		rtc_invalid = 1;
	else
		rtc_invalid = 0;

	if (rtc_invalid == 0) {
		spare0_reg_b13 = (spare0_reg & 0x20) >> 5;
		if ((hw_id & 0xff00) == 0x3500)
			is_bat_plugout = spare0_reg_b13;
		else
			is_bat_plugout = !spare0_reg_b13;

		bat_plug_out_time = spare0_reg & 0x1f;	/*[12:8], 5 bits*/
	} else {
		is_bat_plugout = 1;
		bat_plug_out_time = 31;	/*[12:8], 5 bits*/
	}

	bm_err(
	"[fgauge_read_RTC_boot_status] rtc_invalid %d plugout %d plugout_time %d spare3 0x%x spare0 0x%x hw_id 0x%x\n",
			rtc_invalid, is_bat_plugout, bat_plug_out_time,
			spare3_reg, spare0_reg, hw_id);
}


static int fgauge_initial(struct gauge_device *gauge_dev)
{
	fgauge_read_RTC_boot_status();
#if defined(CONFIG_MTK_DISABLE_GAUGE)
#else
	pmic_set_register_value(PMIC_FG_SON_SLP_EN, 0);
#endif

	return 0;
}

static int fgauge_read_current(
	struct gauge_device *gauge_dev, bool *fg_is_charging, int *data)
{
	unsigned short uvalue16 = 0;
	signed int dvalue = 0;
	int m = 0;
	long long Temp_Value = 0;
	unsigned int ret = 0;

	/* HW Init
	 *(1)    i2c_write (0x60, 0xC8, 0x01); // Enable VA2
	 *(2)    i2c_write (0x61, 0x15, 0x00); // Enable FGADC clock for digital
	 *	//Set current mode, auto-calibration mode and 32KHz clock source
	 *(3)    i2c_write (0x61, 0x69, 0x28);
	 *(4)    i2c_write (0x61, 0x69, 0x29); // Enable FGADC
	 */

	/* Read HW Raw Data
	 *(1)    Set READ command
	 */
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0001, 0x000F, 0x0);
	/*(2)     Keep i2c read when status = 1 (0x06) */
	m = 0;
		while (fg_get_data_ready_status() == 0) {
			m++;
			if (m > 1000) {
				bm_err(
				"[fgauge_read_current] fg_get_data_ready_status timeout 1 !\r\n");
				break;
			}
		}
	/*
	 *(3)    Read FG_CURRENT_OUT[15:08]
	 *(4)    Read FG_CURRENT_OUT[07:00]
	 */
	uvalue16 = pmic_get_register_value(PMIC_FG_CURRENT_OUT);
	bm_trace("[fgauge_read_current] : FG_CURRENT = %x\r\n", uvalue16);
	/*
	 *(5)    (Read other data)
	 *(6)    Clear status to 0
	 */
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0008, 0x000F, 0x0);
	/*
	 *(7)    Keep i2c read when status = 0 (0x08)
	 * while ( fg_get_sw_clear_status() != 0 )
	 */
	m = 0;
		while (fg_get_data_ready_status() != 0) {
			m++;
			if (m > 1000) {
				bm_err(
					"[fgauge_read_current] fg_get_data_ready_status timeout 2 !\r\n");
				break;
			}
		}
	/*(8)    Recover original settings */
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0000, 0x000F, 0x0);

	/*calculate the real world data    */
	dvalue = (unsigned int) uvalue16;
		if (dvalue == 0) {
			Temp_Value = (long long) dvalue;
			*fg_is_charging = false;
		} else if (dvalue > 32767) {
			/* > 0x8000 */
			Temp_Value = (long long) (dvalue - 65535);
			Temp_Value = Temp_Value - (Temp_Value * 2);
			*fg_is_charging = false;
		} else {
			Temp_Value = (long long) dvalue;
			*fg_is_charging = true;
		}

	Temp_Value = Temp_Value * UNIT_FGCURRENT;
#if defined(__LP64__) || defined(_LP64)
	do_div(Temp_Value, 100000);
#else
	Temp_Value = div_s64(Temp_Value, 100000);
#endif
	dvalue = (unsigned int) Temp_Value;

	if (*fg_is_charging == true)
		bm_trace("[%s] current(charging) = %d mA\r\n",
			 __func__, dvalue);
	else
		bm_trace("[%s] current(discharging) = %d mA\r\n",
			 __func__, dvalue);

		/* Auto adjust value */
		if (gauge_dev->fg_cust_data->r_fg_value != 100) {
			bm_trace(
			"[fgauge_read_current] Auto adjust value due to the Rfg is %d\n Ori current=%d, ",
			gauge_dev->fg_cust_data->r_fg_value, dvalue);

			dvalue = (dvalue * 100) /
				gauge_dev->fg_cust_data->r_fg_value;

			bm_trace("[fgauge_read_current] new current=%d\n",
				dvalue);
		}

		bm_trace("[fgauge_read_current] ori current=%d\n",
			dvalue);

		dvalue = ((dvalue *
			gauge_dev->fg_cust_data->car_tune_value) / 1000);

		bm_debug("[fgauge_read_current] final current=%d (ratio=%d)\n",
			 dvalue, gauge_dev->fg_cust_data->car_tune_value);

		*data = dvalue;

	return 0;
}

static int fgauge_get_average_current(
	struct gauge_device *gauge_dev, int *data, bool *valid)
{
	long long fg_iavg_reg = 0;
	long long fg_iavg_reg_tmp = 0;
	long long fg_iavg_ma = 0;
	int fg_iavg_reg_27_16 = 0;
	int fg_iavg_reg_15_00 = 0;
	int sign_bit = 0;
	int is_bat_charging;
	int ret, m;

	/* Set Read Latchdata */
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0001, 0x000F, 0x0);
	m = 0;
		while (fg_get_data_ready_status() == 0) {
			m++;
			if (m > 1000) {
				bm_err(
				"[fg_get_current_iavg] fg_get_data_ready_status timeout 1 !\r\n");
				break;
			}
		}

	if (pmic_get_register_value(PMIC_FG_IAVG_VLD) == 1) {
		fg_iavg_reg_27_16 = pmic_get_register_value(PMIC_FG_IAVG_27_16);
		fg_iavg_reg_15_00 = pmic_get_register_value(PMIC_FG_IAVG_15_00);
		fg_iavg_reg = (fg_iavg_reg_27_16 << 16) + fg_iavg_reg_15_00;
		sign_bit = (fg_iavg_reg_27_16 & 0x800) >> 11;

		if (sign_bit) {
			fg_iavg_reg_tmp = fg_iavg_reg;
			/*fg_iavg_reg = fg_iavg_reg_tmp - 0xfffffff - 1;*/
			fg_iavg_reg = 0xfffffff - fg_iavg_reg_tmp + 1;
		}

		if (sign_bit == 1)
			is_bat_charging = 0;	/* discharge */
		else
			is_bat_charging = 1;	/* charge */

		fg_iavg_ma = fg_iavg_reg * UNIT_FG_IAVG *
			gauge_dev->fg_cust_data->car_tune_value;
		bm_trace(
		"[fg_get_current_iavg] fg_iavg_ma %lld fg_iavg_reg %lld fg_iavg_reg_tmp %lld\n",
			fg_iavg_ma, fg_iavg_reg, fg_iavg_reg_tmp);
#if defined(__LP64__) || defined(_LP64)
		do_div(fg_iavg_ma, 1000000);
		do_div(fg_iavg_ma, gauge_dev->fg_cust_data->r_fg_value);
#else
		fg_iavg_ma = div_s64(fg_iavg_ma, 1000000);
		fg_iavg_ma = div_s64(fg_iavg_ma,
			gauge_dev->fg_cust_data->r_fg_value);
#endif
		bm_trace("[fg_get_current_iavg] fg_iavg_ma %lld\n", fg_iavg_ma);


		if (sign_bit == 1)
			fg_iavg_ma = 0 - fg_iavg_ma;

		bm_trace(
			"[fg_get_current_iavg] fg_iavg_ma %lld fg_iavg_reg %lld r_fg_value %d 27_16 0x%x 15_00 0x%x\n",
			fg_iavg_ma, fg_iavg_reg,
			gauge_dev->fg_cust_data->r_fg_value,
			fg_iavg_reg_27_16, fg_iavg_reg_15_00);
			gauge_dev->fg_hw_info.current_avg = fg_iavg_ma;
			gauge_dev->fg_hw_info.current_avg_sign = sign_bit;
		bm_trace("[fg_get_current_iavg] PMIC_FG_IAVG_VLD == 1\n");
	} else {
		read_fg_hw_info_current_1(gauge_dev);
		gauge_dev->fg_hw_info.current_avg =
			gauge_dev->fg_hw_info.current_1;
		if (gauge_dev->fg_hw_info.current_1 < 0)
			gauge_dev->fg_hw_info.current_avg_sign = 1;
		bm_debug("[fg_get_current_iavg] PMIC_FG_IAVG_VLD != 1, avg %d, current_1 %d\n",
			gauge_dev->fg_hw_info.current_avg,
			gauge_dev->fg_hw_info.current_1);
	}

	*data = gauge_dev->fg_hw_info.current_avg;
	*valid = pmic_get_register_value(PMIC_FG_IAVG_VLD);
	bm_debug("[fg_get_current_iavg] %d %d\n", *data, *valid);

	/* recover read */
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0008, 0x000F, 0x0);
	m = 0;
		while (fg_get_data_ready_status() != 0) {
			m++;
			if (m > 1000) {
				bm_err(
					"[fg_get_current_iavg] fg_get_data_ready_status timeout 2 !\r\n");
				break;
			}
		}
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0000, 0x000F, 0x0);

	return 0;
}

static int fgauge_get_coulomb(struct gauge_device *gauge_dev, int *data)
{
#if defined(SOC_BY_3RD_FG)
		*data = bq27531_get_remaincap();
		return 0;
#else
	unsigned int uvalue32_CAR = 0;
	unsigned int uvalue32_CAR_MSB = 0;
	unsigned int temp_CAR_15_0 = 0;
	unsigned int temp_CAR_31_16 = 0;
	signed int dvalue_CAR = 0;
	int m = 0;
	long long Temp_Value = 0;
	unsigned int ret = 0;
	int reset = 0;

/*
 * HW Init
 *(1)    i2c_write (0x60, 0xC8, 0x01); // Enable VA2
 *(2)    i2c_write (0x61, 0x15, 0x00); // Enable FGADC clock for digital
 *	//Set current mode, auto-calibration mode and 32KHz clock source
 *(3)    i2c_write (0x61, 0x69, 0x28);
 *(4)    i2c_write (0x61, 0x69, 0x29); // Enable FGADC
 *
 * Read HW Raw Data
 *(1)    Set READ command
 */

	/*fg_dump_register();*/

	if (reset == 0)
		ret = pmic_config_interface(
		MT6355_FGADC_CON1, 0x0001, 0x1F05, 0x0);
	else {
		ret = pmic_config_interface(
			MT6355_FGADC_CON1, 0x0705, 0x1F05, 0x0);
		bm_err("[fgauge_read_columb_internal] reset fgadc 0x0705\n");
	}

	/*(2)    Keep i2c read when status = 1 (0x06) */
	m = 0;
	while (fg_get_data_ready_status() == 0) {
		m++;
		if (m > 1000) {
			bm_err(
				 "[%s] fg_get_data_ready_status timeout 1 !\r\n",
				 __func__);
			break;
		}
	}
/*
 *(3)    Read FG_CURRENT_OUT[28:14]
 *(4)    Read FG_CURRENT_OUT[31]
 */

	temp_CAR_15_0 = pmic_get_register_value(PMIC_FG_CAR_15_00);
	temp_CAR_31_16 = pmic_get_register_value(PMIC_FG_CAR_31_16);

	uvalue32_CAR = temp_CAR_15_0 >> 11;
	uvalue32_CAR |= ((temp_CAR_31_16) & 0x7FFF) << 5;

	uvalue32_CAR_MSB = (temp_CAR_31_16 & 0x8000) >> 15;

	bm_trace("[%s] temp_CAR_15_0 = 0x%x  temp_CAR_31_16 = 0x%x\n",
		 __func__, temp_CAR_15_0, temp_CAR_31_16);

	bm_trace("[fgauge_read_columb_internal] FG_CAR = 0x%x\r\n",
		 uvalue32_CAR);
	bm_trace(
		 "[fgauge_read_columb_internal] uvalue32_CAR_MSB = 0x%x\r\n",
		 uvalue32_CAR_MSB);

/*
 *(5)    (Read other data)
 *(6)    Clear status to 0
 */
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0008, 0x000F, 0x0);
/*
 *(7)    Keep i2c read when status = 0 (0x08)
 */
	m = 0;
	while (fg_get_data_ready_status() != 0) {
		m++;
		if (m > 1000) {
			bm_err(
				 "[fgauge_read_columb_internal] fg_get_data_ready_status timeout 2 !\r\n");
			break;
		}
	}
	/*(8)    Recover original settings */
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0000, 0x000F, 0x0);

/*calculate the real world data    */
	dvalue_CAR = (signed int) uvalue32_CAR;

	if (uvalue32_CAR == 0) {
		Temp_Value = 0;
	} else if (uvalue32_CAR == 0xfffff) {
		Temp_Value = 0;
	} else if (uvalue32_CAR_MSB == 0x1) {
		/* dis-charging */
		Temp_Value =
			(long long) (dvalue_CAR - 0xfffff);
		Temp_Value = Temp_Value - (Temp_Value * 2);
	} else {
		/*charging */
		Temp_Value = (long long) dvalue_CAR;
	}


	/* 0.1 mAh */
#if defined(__LP64__) || defined(_LP64)
	Temp_Value = Temp_Value * UNIT_FGCAR / 1000;
#else
	Temp_Value = div_s64(Temp_Value * UNIT_FGCAR, 1000);
#endif

#if defined(__LP64__) || defined(_LP64)
	do_div(Temp_Value, 10);
	Temp_Value = Temp_Value + 5;
	do_div(Temp_Value, 10);
#else
	Temp_Value = div_s64(Temp_Value, 10);
	Temp_Value = Temp_Value + 5;
	Temp_Value = div_s64(Temp_Value, 10);
#endif

	if (uvalue32_CAR_MSB == 0x1)
		dvalue_CAR = (signed int)
			(Temp_Value - (Temp_Value * 2));
	else
		dvalue_CAR = (signed int) Temp_Value;

	bm_trace("[fgauge_read_columb_internal] dvalue_CAR = %d\r\n",
		 dvalue_CAR);

	/*#if (OSR_SELECT_7 == 1) */
/*Auto adjust value*/
	if (gauge_dev->fg_cust_data->r_fg_value != 100) {
		bm_trace(
			 "[fgauge_read_columb_internal] Auto adjust value deu to the Rfg is %d\n Ori CAR=%d, ",
			 gauge_dev->fg_cust_data->r_fg_value, dvalue_CAR);

		dvalue_CAR = (dvalue_CAR * 100) /
			gauge_dev->fg_cust_data->r_fg_value;

		bm_trace("[fgauge_read_columb_internal] new CAR=%d\n",
			 dvalue_CAR);
	}

	dvalue_CAR = ((dvalue_CAR *
		gauge_dev->fg_cust_data->car_tune_value) / 1000);

	bm_debug("[fgauge_read_columb_internal] CAR=%d r_fg_value=%d car_tune_value=%d\n",
				 dvalue_CAR,
				 gauge_dev->fg_cust_data->r_fg_value,
				 gauge_dev->fg_cust_data->car_tune_value);

	*data = dvalue_CAR;

	return 0;
#endif
}

static int fgauge_reset_hw(struct gauge_device *gauge_dev)
{
	unsigned int val_car = 1;
	unsigned int val_car_temp = 1;
	unsigned int ret = 0;

	bm_trace("[fgauge_hw_reset] : Start \r\n");

	while (val_car != 0x0) {
		ret = pmic_config_interface(
			MT6355_FGADC_CON1, 0x0600, 0x1F00, 0x0);
		bm_err("[fgauge_hw_reset] reset fgadc 0x0600\n");

		fgauge_get_coulomb(gauge_dev, &val_car_temp);
		val_car = val_car_temp;
	}

	bm_trace("[fgauge_hw_reset] : End \r\n");

	return 0;
}

static bool is_charger_exist(void)
{
	if (mt_get_charger_type() == CHARGER_UNKNOWN)
		return false;
	else
		return true;
}

static int read_hw_ocv_6355_plug_in(void)
{
	signed int adc_rdy = 0;
	signed int adc_result_reg = 0;
	signed int adc_result = 0;

	adc_rdy = pmic_get_register_value(PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_PCHR);
	adc_result_reg = pmic_get_register_value(
		PMIC_AUXADC_ADC_OUT_BAT_PLUGIN_PCHR);
	adc_result = REG_to_MV_value(adc_result_reg);
	bm_debug("[oam] read_hw_ocv_6355_plug_in (pchr) : adc_result_reg=%d, adc_result=%d, start_sel=%d, rdy=%d\n",
		 adc_result_reg, adc_result,
		 pmic_get_register_value(PMIC_RG_STRUP_AUXADC_START_SEL),
		 adc_rdy);

	if (adc_rdy == 1) {
		pmic_set_register_value(PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_CLR, 1);
		mdelay(1);
		pmic_set_register_value(PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_CLR, 0);
	}

	adc_result += g_hw_ocv_tune_value;
	return adc_result;
}


static int read_hw_ocv_6355_power_on(void)
{
	signed int adc_rdy = 0;
	signed int adc_result_reg = 0;
	signed int adc_result = 0;

	adc_rdy = pmic_get_register_value(
		PMIC_AUXADC_ADC_RDY_PWRON_PCHR);
	adc_result_reg = pmic_get_register_value(
		PMIC_AUXADC_ADC_OUT_PWRON_PCHR);

	adc_result = REG_to_MV_value(adc_result_reg);
	bm_debug("[oam] read_hw_ocv_6355_power_on : adc_result_reg=%d, adc_result=%d, start_sel=%d, rdy=%d\n",
		 adc_result_reg, adc_result,
		 pmic_get_register_value(PMIC_RG_STRUP_AUXADC_START_SEL),
		 adc_rdy);

	if (adc_rdy == 1) {
		pmic_set_register_value(PMIC_AUXADC_ADC_RDY_PWRON_CLR, 1);
		mdelay(1);
		pmic_set_register_value(PMIC_AUXADC_ADC_RDY_PWRON_CLR, 0);
	}

	adc_result += g_hw_ocv_tune_value;
	return adc_result;
}

#ifdef CONFIG_MTK_PMIC_CHIP_MT6336
int read_hw_ocv_6336_charger_in(void)
{
		int zcv_36_low, zcv_36_high, zcv_36_rdy;
		int zcv_chrgo_1_lo, zcv_chrgo_1_hi, zcv_chrgo_1_rdy;
		int zcv_fgadc1_lo, zcv_fgadc1_hi, zcv_fgadc1_rdy;
		int hw_ocv_36_reg1 = 0;
		int hw_ocv_36_reg2 = 0;
		int hw_ocv_36_reg3 = 0;
		int hw_ocv_36_1 = 0;
		int hw_ocv_36_2 = 0;
		int hw_ocv_36_3 = 0;

		zcv_36_rdy = mt6336_get_flag_register_value(
			MT6336_AUXADC_ADC_RDY_WAKEUP1);
		zcv_chrgo_1_rdy = mt6336_get_flag_register_value(
			MT6336_AUXADC_ADC_RDY_CHRGO1);
		zcv_fgadc1_rdy = mt6336_get_flag_register_value(
			MT6336_AUXADC_ADC_RDY_FGADC1);

		if (1) {
			zcv_36_low = mt6336_get_flag_register_value(
				MT6336_AUXADC_ADC_OUT_WAKEUP1_L);
			zcv_36_high = mt6336_get_flag_register_value(
				MT6336_AUXADC_ADC_OUT_WAKEUP1_H);
			hw_ocv_36_reg1 = (zcv_36_high << 8) + zcv_36_low;
			hw_ocv_36_1 = REG_to_MV_value(hw_ocv_36_reg1);
			bm_err("[read_hw_ocv_6336_charger_in] zcv_36_rdy %d hw_ocv_36_1 %d [0x%x:0x%x]\n",
				zcv_36_rdy, hw_ocv_36_1,
				zcv_36_low, zcv_36_high);

			mt6336_set_flag_register_value(
				MT6336_AUXADC_ADC_RDY_WAKEUP_CLR, 1);
			mdelay(1);
			mt6336_set_flag_register_value(
				MT6336_AUXADC_ADC_RDY_WAKEUP_CLR, 0);
		}

		if (1) {
			zcv_chrgo_1_lo = mt6336_get_flag_register_value(
				MT6336_AUXADC_ADC_OUT_CHRGO1_L);
			zcv_chrgo_1_hi = mt6336_get_flag_register_value(
				MT6336_AUXADC_ADC_OUT_CHRGO1_H);
			hw_ocv_36_reg2 = (zcv_chrgo_1_hi << 8) + zcv_chrgo_1_lo;
			hw_ocv_36_2 = REG_to_MV_value(hw_ocv_36_reg2);
			bm_err("[read_hw_ocv_6336_charger_in] zcv_chrgo_1_rdy %d hw_ocv_36_2 %d [0x%x:0x%x]\n",
				zcv_chrgo_1_rdy, hw_ocv_36_2,
				zcv_chrgo_1_lo, zcv_chrgo_1_hi);
			mt6336_set_flag_register_value(
				MT6336_AUXADC_ADC_RDY_CHRGO_CLR, 1);
			mdelay(1);
			mt6336_set_flag_register_value(
				MT6336_AUXADC_ADC_RDY_CHRGO_CLR, 0);
		}

		if (1) {
			zcv_fgadc1_lo = mt6336_get_flag_register_value(
				MT6336_AUXADC_ADC_OUT_FGADC1_L);
			zcv_fgadc1_hi = mt6336_get_flag_register_value(
				MT6336_AUXADC_ADC_OUT_FGADC1_H);
			hw_ocv_36_reg3 = (zcv_fgadc1_hi << 8) + zcv_fgadc1_lo;
			hw_ocv_36_3 = REG_to_MV_value(hw_ocv_36_reg3);
			bm_err("[read_hw_ocv_6336_charger_in] FGADC1 %d hw_ocv_36_3 %d [0x%x:0x%x]\n",
				zcv_fgadc1_rdy, hw_ocv_36_3,
				zcv_fgadc1_lo, zcv_fgadc1_hi);
			mt6336_set_flag_register_value(
				MT6336_AUXADC_ADC_RDY_FGADC_CLR, 1);
			mdelay(1);
			mt6336_set_flag_register_value(
				MT6336_AUXADC_ADC_RDY_FGADC_CLR, 0);
		}

		return hw_ocv_36_2;
}

#else
int read_hw_ocv_6336_charger_in2(void)
{
	int hw_ocv_55 = read_hw_ocv_6355_power_on();

	return hw_ocv_55;
}
#endif

static int read_hw_ocv_6355_power_on_rdy(void)
{
	signed int pon_rdy = 0;

	pon_rdy = pmic_get_register_value(PMIC_AUXADC_ADC_RDY_PWRON_PCHR);
	bm_err("[read_hw_ocv_6355_power_on_rdy] pon_rdy %d\n", pon_rdy);

	return pon_rdy;
}

static int charger_zcv;
/* static int pmic_in_zcv; */
static int pmic_zcv;
static int pmic_rdy;
static int swocv;
static int zcv_from;
static int zcv_tmp;

int read_hw_ocv(struct gauge_device *gauge_dev, int *data)
{
	int _hw_ocv, _sw_ocv;
	int _hw_ocv_src;
	int _prev_hw_ocv, _prev_hw_ocv_src;
	int _hw_ocv_rdy;
	int _flag_unreliable;
	int _hw_ocv_55_pon;
	int _hw_ocv_55_plugin;
	int _hw_ocv_55_pon_rdy;
	int _hw_ocv_chgin;
	int _hw_ocv_chgin_rdy;
	int now_temp;
	int now_thr;

	_hw_ocv_55_pon_rdy = read_hw_ocv_6355_power_on_rdy();
	_hw_ocv_55_pon = read_hw_ocv_6355_power_on();
	_hw_ocv_55_plugin = read_hw_ocv_6355_plug_in();
	_hw_ocv_chgin = battery_get_charger_zcv() / 100;
	now_temp = fg_get_battery_temperature_for_zcv();

	if (now_temp > EXT_HWOCV_SWOCV_LT_TEMP)
		now_thr = EXT_HWOCV_SWOCV;
	else
		now_thr = EXT_HWOCV_SWOCV_LT;

	if (_hw_ocv_chgin < 25000)
		_hw_ocv_chgin_rdy = 0;
	else
		_hw_ocv_chgin_rdy = 1;

	g_fg_is_charger_exist = is_charger_exist();
	_hw_ocv = _hw_ocv_55_pon;
	_sw_ocv = get_mtk_battery()->hw_status.sw_ocv;
	_hw_ocv_src = FROM_6355_PON_ON;
	_prev_hw_ocv = _hw_ocv;
	_prev_hw_ocv_src = FROM_6355_PON_ON;
	_flag_unreliable = 0;

	if (g_fg_is_charger_exist) {
		_hw_ocv_rdy = _hw_ocv_55_pon_rdy;
		if (_hw_ocv_rdy == 1) {
			if (_hw_ocv_chgin_rdy == 1) {
				_hw_ocv = _hw_ocv_chgin;
				_hw_ocv_src = FROM_6336_CHR_IN;
			} else {
				_hw_ocv = _hw_ocv_55_pon;
				_hw_ocv_src = FROM_6355_PON_ON;
			}

			if (abs(_hw_ocv - _sw_ocv) > now_thr) {
				_prev_hw_ocv = _hw_ocv;
				_prev_hw_ocv_src = _hw_ocv_src;
				_hw_ocv = _sw_ocv;
				_hw_ocv_src = FROM_SW_OCV;
				set_hw_ocv_unreliable(true);
				_flag_unreliable = 1;
			}
		} else {
			/* fixme: swocv is workaround */
			/*_hw_ocv = _hw_ocv_55_plugin;*/
			/*_hw_ocv_src = FROM_6355_PLUG_IN;*/
			_hw_ocv = _sw_ocv;
			_hw_ocv_src = FROM_SW_OCV;
			if (_hw_ocv_chgin_rdy != 1) {
				if (abs(_hw_ocv - _sw_ocv) > now_thr) {
					_prev_hw_ocv = _hw_ocv;
					_prev_hw_ocv_src = _hw_ocv_src;
					_hw_ocv = _sw_ocv;
					_hw_ocv_src = FROM_SW_OCV;
					set_hw_ocv_unreliable(true);
					_flag_unreliable = 1;
				}
			}
		}
	} else {
		if (_hw_ocv_55_pon_rdy == 0) {
			_hw_ocv = _sw_ocv;
			_hw_ocv_src = FROM_SW_OCV;
		}
	}

	*data = _hw_ocv;

	charger_zcv = _hw_ocv_chgin;
	pmic_rdy = _hw_ocv_55_pon_rdy;
	pmic_zcv = _hw_ocv_55_pon;
	/* pmic_in_zcv = _hw_ocv_55_plugin; */
	swocv = _sw_ocv;
	zcv_from = _hw_ocv_src;
	zcv_tmp = now_temp;

	bm_err("[read_hw_ocv] g_fg_is_charger_exist %d _hw_ocv_chgin_rdy %d\n",
		g_fg_is_charger_exist, _hw_ocv_chgin_rdy);
	bm_err("[read_hw_ocv] _hw_ocv %d _sw_ocv %d now_thr %d\n",
		_prev_hw_ocv, _sw_ocv, now_thr);
	bm_err("[read_hw_ocv] _hw_ocv %d _hw_ocv_src %d _prev_hw_ocv %d _prev_hw_ocv_src %d _flag_unreliable %d\n",
		_hw_ocv, _hw_ocv_src,
		_prev_hw_ocv, _prev_hw_ocv_src,
		_flag_unreliable);
	bm_debug("[read_hw_ocv] _hw_ocv_55_pon_rdy %d _hw_ocv_55_pon %d _hw_ocv_55_plugin %d _hw_ocv_chgin %d _sw_ocv %d now_temp %d now_thr %d\n",
		_hw_ocv_55_pon_rdy, _hw_ocv_55_pon,
		_hw_ocv_55_plugin, _hw_ocv_chgin,
		_sw_ocv, now_temp, now_thr);

	return 0;
}


int fgauge_set_coulomb_interrupt1_ht(
	struct gauge_device *gauge_dev, int car_value)
{
	unsigned int uvalue32_CAR = 0;
	unsigned int uvalue32_CAR_MSB = 0;
	signed int upperbound = 0;
	signed int upperbound_31_16 = 0, upperbound_15_14 = 0;
	int reset = 0;
	signed short m;
	unsigned int ret = 0;
	signed int value32_CAR;
	long long car = car_value;

	bm_trace("fgauge_set_coulomb_interrupt1_ht car=%d\n", car_value);
	if (car == 0) {
		pmic_enable_interrupt(FG_BAT1_INT_H_NO, 0, "GM30");
		return 0;
	}
/*
 * HW Init
 *(1)    i2c_write (0x60, 0xC8, 0x01); // Enable VA2
 *(2)    i2c_write (0x61, 0x15, 0x00); // Enable FGADC clock for digital
 *	// Set current mode, auto-calibration mode and 32KHz clock source
 *(3)    i2c_write (0x61, 0x69, 0x28);
 *(4)    i2c_write (0x61, 0x69, 0x29); // Enable FGADC
 *
 *Read HW Raw Data
 *(1)    Set READ command
 */
	if (reset == 0) {
		ret = pmic_config_interface(
			MT6355_FGADC_CON1, 0x0001, 0x1F0F, 0x0);
	} else {
		ret = pmic_config_interface(
			MT6355_FGADC_CON1, 0x1F05, 0xFF0F, 0x0);
		bm_err("[fgauge_set_coulomb_interrupt1_ht] reset fgadc 0x1F05\n");
	}

	/*(2)    Keep i2c read when status = 1 (0x06) */
	m = 0;
	while (fg_get_data_ready_status() == 0) {
		m++;
		if (m > 1000) {
			bm_err(
				 "[fgauge_set_coulomb_interrupt1_ht] fg_get_data_ready_status timeout 1 !");
			break;
		}
	}
/*
 *(3)    Read FG_CURRENT_OUT[28:14]
 *(4)    Read FG_CURRENT_OUT[31]
 */
	value32_CAR = (pmic_get_register_value(PMIC_FG_CAR_15_00));
	value32_CAR |= ((pmic_get_register_value(PMIC_FG_CAR_31_16))
		& 0xffff) << 16;
	uvalue32_CAR_MSB = (pmic_get_register_value(PMIC_FG_CAR_31_16)
		& 0x8000) >> 15;

	bm_trace(
		"[fgauge_set_columb_interrupt_internal1] FG_CAR = 0x%x   uvalue32_CAR_MSB:0x%x 0x%x 0x%x\r\n",
		uvalue32_CAR, uvalue32_CAR_MSB,
		(pmic_get_register_value(PMIC_FG_CAR_15_00)),
		(pmic_get_register_value(PMIC_FG_CAR_31_16)));

	/* recover read */
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0008, 0x000F, 0x0);
	m = 0;
	while (fg_get_data_ready_status() != 0) {
		m++;
		if (m > 1000) {
			bm_err(
				 "[fgauge_set_coulomb_interrupt1_ht] fg_get_data_ready_status timeout 2 !");
			break;
		}
	}
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0000, 0x000F, 0x0);
	/* recover done */

	/* gap to register-base */
#if defined(__LP64__) || defined(_LP64)
	car = car * CAR_TO_REG_FACTOR / 10;
#else
	car = div_s64(car * CAR_TO_REG_FACTOR, 10);
#endif

	if (gauge_dev->fg_cust_data->r_fg_value != 100)
#if defined(__LP64__) || defined(_LP64)
		car = (car * gauge_dev->fg_cust_data->r_fg_value) / 100;
#else
		car = div_s64(car * gauge_dev->fg_cust_data->r_fg_value, 100);
#endif

#if defined(__LP64__) || defined(_LP64)
	car = ((car * 1000) / gauge_dev->fg_cust_data->car_tune_value);
#else
	car = div_s64((car * 1000), gauge_dev->fg_cust_data->car_tune_value);
#endif

	upperbound = value32_CAR;

	bm_trace("[%s] upper = 0x%x:%d diff_car=0x%llx:%lld\r\n",
		 __func__, upperbound, upperbound, car, car);

	upperbound = upperbound + car;

	if ((upperbound & 0x3fff) > 0) {

		bm_debug("[%s]upper:%d nupper:%d\n",
			__func__, upperbound, upperbound + 0x4000);
		upperbound = upperbound + 0x4000;
	}

	upperbound_31_16 = (upperbound & 0xffff0000) >> 16;
	upperbound_15_14 = (upperbound & 0xffff) >> 14;


	bm_trace("[%s]final upper = 0x%x:%d car=0x%llx:%lld\r\n",
		 __func__, upperbound, upperbound, car, car);

	bm_trace("[%s]final upper 0x%x 0x%x 0x%x car=0x%llx\n",
		 __func__, upperbound, upperbound_31_16, upperbound_15_14, car);

	pmic_enable_interrupt(FG_BAT1_INT_H_NO, 0, "GM30");

	pmic_set_register_value(PMIC_FG_BAT1_HTH_15_14, upperbound_15_14);
	pmic_set_register_value(PMIC_FG_BAT1_HTH_31_16, upperbound_31_16);
	mdelay(1);

	pmic_enable_interrupt(FG_BAT1_INT_H_NO, 1, "GM30");

	bm_trace(
		"[fgauge_set_columb_interrupt_internal1] high:[0xcb0]=0x%x 0x%x\r\n",
		pmic_get_register_value(PMIC_FG_BAT1_HTH_15_14),
		pmic_get_register_value(PMIC_FG_BAT1_HTH_31_16));

	return 0;

}

int fgauge_set_coulomb_interrupt1_lt(
	struct gauge_device *gauge_dev, int car_value)
{
	unsigned int uvalue32_CAR = 0;
	unsigned int uvalue32_CAR_MSB = 0;
	signed int lowbound = 0;
	signed int lowbound_31_16 = 0, lowbound_15_14 = 0;
	int reset = 0;
	signed short m;
	unsigned int ret = 0;
	signed int value32_CAR;
	long long car = car_value;

	bm_trace("fgauge_set_coulomb_interrupt1_lt car=%d\n", car_value);
	if (car == 0) {
		pmic_enable_interrupt(FG_BAT1_INT_L_NO, 0, "GM30");
		return 0;
	}
/*
 * HW Init
 *(1)    i2c_write (0x60, 0xC8, 0x01); // Enable VA2
 *(2)    i2c_write (0x61, 0x15, 0x00); // Enable FGADC clock for digital
 * // Set current mode, auto-calibration mode and 32KHz clock source
 *(3)    i2c_write (0x61, 0x69, 0x28);
 *(4)    i2c_write (0x61, 0x69, 0x29); // Enable FGADC
 *
 *Read HW Raw Data
 *(1)    Set READ command
 */
	if (reset == 0) {
		ret = pmic_config_interface(
			MT6355_FGADC_CON1, 0x0001, 0x1F0F, 0x0);
	} else {
		ret = pmic_config_interface(
			MT6355_FGADC_CON1, 0x1F05, 0xFF0F, 0x0);
		bm_err("[fgauge_set_coulomb_interrupt1_lt] reset fgadc 0x1F05\n");
	}

	/*(2)    Keep i2c read when status = 1 (0x06) */
	m = 0;
	while (fg_get_data_ready_status() == 0) {
		m++;
		if (m > 1000) {
			bm_err(
				 "[fgauge_set_coulomb_interrupt1_lt] fg_get_data_ready_status timeout 1 !");
			break;
		}
	}
/*
 *(3)    Read FG_CURRENT_OUT[28:14]
 *(4)    Read FG_CURRENT_OUT[31]
 */
	value32_CAR =
		(pmic_get_register_value(PMIC_FG_CAR_15_00));
	value32_CAR |=
		((pmic_get_register_value(PMIC_FG_CAR_31_16)) & 0xffff) << 16;

	uvalue32_CAR_MSB =
		(pmic_get_register_value(PMIC_FG_CAR_31_16) & 0x8000) >> 15;

	bm_trace(
		"[fgauge_set_coulomb_interrupt1_lt] FG_CAR = 0x%x   uvalue32_CAR_MSB:0x%x 0x%x 0x%x\r\n",
		uvalue32_CAR, uvalue32_CAR_MSB,
		(pmic_get_register_value(PMIC_FG_CAR_15_00)),
		(pmic_get_register_value(PMIC_FG_CAR_31_16)));


	/* recover read */
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0008, 0x000F, 0x0);
	m = 0;
	while (fg_get_data_ready_status() != 0) {
		m++;
		if (m > 1000) {
			bm_err(
				 "[fgauge_set_coulomb_interrupt1_lt] fg_get_data_ready_status timeout 2 !");
			break;
		}
	}
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0000, 0x000F, 0x0);
	/* recover done */

	/* gap to register-base */
#if defined(__LP64__) || defined(_LP64)
	car = car * CAR_TO_REG_FACTOR / 10;
#else
	car = div_s64(car * CAR_TO_REG_FACTOR, 10);
#endif

	if (gauge_dev->fg_cust_data->r_fg_value != 100)
#if defined(__LP64__) || defined(_LP64)
		car = (car * gauge_dev->fg_cust_data->r_fg_value) / 100;
#else
		car = div_s64(car * gauge_dev->fg_cust_data->r_fg_value, 100);
#endif

#if defined(__LP64__) || defined(_LP64)
	car = ((car * 1000) / gauge_dev->fg_cust_data->car_tune_value);
#else
	car = div_s64((car * 1000), gauge_dev->fg_cust_data->car_tune_value);
#endif

	lowbound = value32_CAR;

	bm_trace("[%s]low=0x%x:%d  diff_car=0x%llx:%lld\r\n",
		 __func__, lowbound, lowbound, car, car);

	lowbound = lowbound - car;

	if ((abs(lowbound) & 0x3fff) > 0) {

		bm_debug("[%s]low:%d nlow:%d\n",
			__func__, lowbound, lowbound - 0x4000);
		lowbound = lowbound - 0x4000;
	}

	lowbound_31_16 = (lowbound & 0xffff0000) >> 16;
	lowbound_15_14 = (lowbound & 0xffff) >> 14;

	bm_trace("[%s]final low=0x%x:%d  car=0x%llx:%lld\r\n",
		 __func__, lowbound, lowbound, car, car);

	bm_trace("[%s]final low 0x%x 0x%x 0x%x car=0x%llx\n",
		 __func__, lowbound, lowbound_31_16, lowbound_15_14, car);

	pmic_enable_interrupt(FG_BAT1_INT_L_NO, 0, "GM30");
	pmic_set_register_value(PMIC_FG_BAT1_LTH_15_14, lowbound_15_14);
	pmic_set_register_value(PMIC_FG_BAT1_LTH_31_16, lowbound_31_16);
	mdelay(1);

	pmic_enable_interrupt(FG_BAT1_INT_L_NO, 1, "GM30");

	bm_trace(
		"[fgauge_set_coulomb_interrupt1_lt] low:[0xcae]=0x%x 0x%x \r\n",
		pmic_get_register_value(PMIC_FG_BAT1_LTH_15_14),
		pmic_get_register_value(PMIC_FG_BAT1_LTH_31_16));

	return 0;
}

static int fgauge_read_boot_battery_plug_out_status(
	struct gauge_device *gauge_dev, int *is_plugout, int *plutout_time)
{
	*is_plugout = is_bat_plugout;
	*plutout_time = bat_plug_out_time;
	bm_err("[read_boot_battery_plug_out_status] rtc_invalid %d plugout %d bat_plug_out_time %d sp0:0x%x sp3:0x%x\n",
			rtc_invalid, is_bat_plugout,
			bat_plug_out_time, gspare0_reg,
			gspare3_reg);

	return 0;
}

static int fgauge_get_ptim_current
	(struct gauge_device *gauge_dev, int *ptim_current, bool *is_charging)
{
		unsigned short uvalue16 = 0;
		signed int dvalue = 0;
		/*int m = 0;*/
		long long Temp_Value = 0;
		/*unsigned int ret = 0;*/

		uvalue16 = pmic_get_register_value(PMIC_FG_R_CURR);
		bm_trace("[fgauge_get_ptim_current] : FG_CURRENT = %x\r\n",
			 uvalue16);

		/*calculate the real world data    */
		dvalue = (unsigned int) uvalue16;
		if (dvalue == 0) {
			Temp_Value = (long long) dvalue;
			*is_charging = false;
		} else if (dvalue > 32767) {
			/* > 0x8000 */
			Temp_Value = (long long) (dvalue - 65535);
			Temp_Value = Temp_Value - (Temp_Value * 2);
			*is_charging = false;
		} else {
			Temp_Value = (long long) dvalue;
			*is_charging = true;
		}

		Temp_Value = Temp_Value * UNIT_FGCURRENT;
#if defined(__LP64__) || defined(_LP64)
		do_div(Temp_Value, 100000);
#else
		Temp_Value = div_s64(Temp_Value, 100000);
#endif
		dvalue = (unsigned int) Temp_Value;

		if (*is_charging == true)
			bm_trace(
			"[fgauge_read_IM_current] current(charging) = %d mA\r\n",
			dvalue);
		else
			bm_trace(
			"[fgauge_read_IM_current] current(discharging) = %d mA\r\n",
			dvalue);

		/* Auto adjust value */
		if (gauge_dev->fg_cust_data->r_fg_value != 100) {
			bm_trace(
			"[fgauge_read_IM_current] Auto adjust value due to the Rfg is %d\n Ori current=%d, ",
			gauge_dev->fg_cust_data->r_fg_value, dvalue);

			dvalue = (dvalue * 100) /
				gauge_dev->fg_cust_data->r_fg_value;

			bm_trace("[fgauge_read_IM_current] new current=%d\n",
			dvalue);
		}

		bm_trace("[fgauge_read_IM_current] ori current=%d\n", dvalue);

		dvalue = ((dvalue * gauge_dev->fg_cust_data->car_tune_value)
			/ 1000);

		bm_debug("[fgauge_read_IM_current] final current=%d (ratio=%d)\n",
			 dvalue, gauge_dev->fg_cust_data->car_tune_value);

		*ptim_current = dvalue;

		return 0;
}

static int fgauge_get_zcv_current(
	struct gauge_device *gauge_dev, int *zcv_current)
{
	unsigned short uvalue16 = 0;
	signed int dvalue = 0;
	long long Temp_Value = 0;

	uvalue16 = pmic_get_register_value(PMIC_FG_ZCV_CURR);
	dvalue = (unsigned int) uvalue16;
		if (dvalue == 0) {
			Temp_Value = (long long) dvalue;
		} else if (dvalue > 32767) {
			/* > 0x8000 */
			Temp_Value = (long long) (dvalue - 65535);
			Temp_Value = Temp_Value - (Temp_Value * 2);
		} else {
			Temp_Value = (long long) dvalue;
		}

	Temp_Value = Temp_Value * UNIT_FGCURRENT;
#if defined(__LP64__) || defined(_LP64)
	do_div(Temp_Value, 100000);
#else
	Temp_Value = div_s64(Temp_Value, 100000);
#endif
	dvalue = (unsigned int) Temp_Value;

	/* Auto adjust value */
	if (gauge_dev->fg_cust_data->r_fg_value != 100) {
		bm_trace(
		"[fgauge_read_current] Auto adjust value due to the Rfg is %d\n Ori current=%d, ",
		gauge_dev->fg_cust_data->r_fg_value, dvalue);

		dvalue = (dvalue * 100) / gauge_dev->fg_cust_data->r_fg_value;

		bm_trace("[fgauge_read_current] new current=%d\n", dvalue);
	}

	bm_trace("[fgauge_read_current] ori current=%d\n", dvalue);

	dvalue = ((dvalue * gauge_dev->fg_cust_data->car_tune_value) / 1000);

	bm_debug("[fgauge_read_current] final current=%d (ratio=%d)\n",
		 dvalue, gauge_dev->fg_cust_data->car_tune_value);

	*zcv_current = dvalue;

	return 0;

}


static int fgauge_get_zcv(struct gauge_device *gauge_dev, int *zcv)
{
	signed int adc_result_reg = 0;
	signed int adc_result = 0;

#if defined(SWCHR_POWER_PATH)
	adc_result_reg =
		pmic_get_register_value(PMIC_AUXADC_ADC_OUT_FGADC_PCHR);
	adc_result = REG_to_MV_value(adc_result_reg);
	bm_debug("[oam] fgauge_get_zcv BATSNS (swchr) : adc_result_reg=%d, adc_result=%d\n",
		 adc_result_reg, adc_result);
#else
	adc_result_reg =
		pmic_get_register_value(PMIC_AUXADC_ADC_OUT_FGADC_PCHR);
	adc_result = REG_to_MV_value(adc_result_reg);
	bm_debug("[oam] fgauge_get_zcv BATSNS  (pchr) : adc_result_reg=%d, adc_result=%d\n",
		 adc_result_reg, adc_result);
#endif
	adc_result += g_hw_ocv_tune_value;
	*zcv = adc_result;
	return 0;
}

static int fgauge_is_gauge_initialized(
	struct gauge_device *gauge_dev, int *init)
{
	int fg_reset_status = pmic_get_register_value(PMIC_FG_RSTB_STATUS);

	*init = fg_reset_status;

	return 0;
}

static int fgauge_set_gauge_initialized(
	struct gauge_device *gauge_dev, int init)
{
	int fg_reset_status = init;

	pmic_set_register_value(PMIC_FG_RSTB_STATUS, fg_reset_status);

	return 0;
}

static int fgauge_reset_shutdown_time(struct gauge_device *gauge_dev)
{
	int ret;

	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x1000, 0x1000, 0x0);
	mdelay(1);
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0000, 0x1000, 0x0);

	return 0;
}

static int fgauge_reset_ncar(struct gauge_device *gauge_dev)
{
	pmic_set_register_value(PMIC_FG_N_CHARGE_RST, 1);
	udelay(200);
	pmic_set_register_value(PMIC_FG_N_CHARGE_RST, 0);

	return 0;
}

static int nag_zcv_mv;
static int nag_c_dltv_mv;

static void fgauge_set_nafg_intr_internal(int _prd, int _zcv_mv, int _thr_mv)
{
	int NAG_C_DLTV_Threashold_26_16;
	int NAG_C_DLTV_Threashold_15_0;
	int _zcv_reg = MV_to_REG_value(_zcv_mv);
	int _thr_reg = MV_to_REG_value(_thr_mv);

	NAG_C_DLTV_Threashold_26_16 = (_thr_reg & 0xffff0000) >> 16;
	NAG_C_DLTV_Threashold_15_0 = (_thr_reg & 0x0000ffff);
	pmic_set_register_value(PMIC_AUXADC_NAG_ZCV, _zcv_reg);

	pmic_set_register_value(PMIC_AUXADC_NAG_C_DLTV_TH_26_16,
		NAG_C_DLTV_Threashold_26_16);
	pmic_set_register_value(PMIC_AUXADC_NAG_C_DLTV_TH_15_0,
		NAG_C_DLTV_Threashold_15_0);

	pmic_set_register_value(PMIC_AUXADC_NAG_PRD, _prd);

	pmic_set_register_value(PMIC_AUXADC_NAG_VBAT1_SEL, 0);	/* use Batsns */

	bm_debug("[fg_bat_nafg][fgauge_set_nafg_interrupt_internal] time[%d] zcv[%d:%d] thr[%d:%d] 26_16[0x%x] 15_00[0x%x]\n",
		_prd, _zcv_mv,
		_zcv_reg, _thr_mv,
		_thr_reg, NAG_C_DLTV_Threashold_26_16,
		NAG_C_DLTV_Threashold_15_0);

}


static int fgauge_set_nag_zcv(struct gauge_device *gauge_dev, int zcv)
{
	nag_zcv_mv = zcv;	/* 0.1 mv*/
	return 0;
}

static int fgauge_set_nag_c_dltv(struct gauge_device *gauge_dev, int c_dltv_mv)
{
	nag_c_dltv_mv = c_dltv_mv;	/* 0.1 mv*/

	fgauge_set_nafg_intr_internal(
		gauge_dev->fg_cust_data->nafg_time_setting,
		nag_zcv_mv, nag_c_dltv_mv);
	return 0;
}

static int fgauge_enable_nag_interrupt(struct gauge_device *gauge_dev, int en)
{
	if (en != 0)
		en = 1;
	pmic_set_register_value(PMIC_RG_INT_EN_NAG_C_DLTV, en);
	pmic_set_register_value(PMIC_AUXADC_NAG_IRQ_EN, en);
	pmic_set_register_value(PMIC_AUXADC_NAG_EN, en);

	return 0;
}

static int fgauge_get_nag_cnt(struct gauge_device *gauge_dev, int *nag_cnt)
{
	signed int NAG_C_DLTV_CNT;
	signed int NAG_C_DLTV_CNT_H;

	/*AUXADC_NAG_4*/
	NAG_C_DLTV_CNT = pmic_get_register_value(PMIC_AUXADC_NAG_CNT_15_0);

	/*AUXADC_NAG_5*/
	NAG_C_DLTV_CNT_H = pmic_get_register_value(PMIC_AUXADC_NAG_CNT_25_16);
	*nag_cnt = (NAG_C_DLTV_CNT & 0xffff) +
		((NAG_C_DLTV_CNT_H & 0x3ff) << 16);
	bm_debug("[fg_bat_nafg][fgauge_get_nafg_cnt] %d [25_16 %d 15_0 %d]\n",
			*nag_cnt, NAG_C_DLTV_CNT_H, NAG_C_DLTV_CNT);

	return 0;
}

static int fgauge_get_nag_dltv(struct gauge_device *gauge_dev, int *nag_dltv)
{
	signed int NAG_DLTV_reg_value;
	signed int NAG_DLTV_mV_value;

	/*AUXADC_NAG_6*/
	NAG_DLTV_reg_value = pmic_get_register_value(PMIC_AUXADC_NAG_DLTV);

	NAG_DLTV_mV_value = REG_to_MV_value(NAG_DLTV_reg_value);
	*nag_dltv = NAG_DLTV_mV_value;

	bm_debug("[fg_bat_nafg][fgauge_get_nafg_dltv] mV:Reg [%d:%d]\n",
		NAG_DLTV_mV_value, NAG_DLTV_reg_value);

	return 0;
}

static int fgauge_get_nag_c_dltv(
	struct gauge_device *gauge_dev, int *nag_c_dltv)
{
	signed int NAG_C_DLTV_value;
	signed int NAG_C_DLTV_value_H;
	signed int NAG_C_DLTV_reg_value;
	signed int NAG_C_DLTV_mV_value;
	bool bcheckbit10;

	/*AUXADC_NAG_7*/
	NAG_C_DLTV_value = pmic_get_register_value(
		PMIC_AUXADC_NAG_C_DLTV_15_0);

	/*AUXADC_NAG_8*/
	NAG_C_DLTV_value_H = pmic_get_register_value(
		PMIC_AUXADC_NAG_C_DLTV_26_16);
	bcheckbit10 = NAG_C_DLTV_value_H & 0x0400;

	if (bcheckbit10 == 0)
		NAG_C_DLTV_reg_value = (NAG_C_DLTV_value & 0xffff) +
			((NAG_C_DLTV_value_H & 0x07ff) << 16);
	else
		NAG_C_DLTV_reg_value = (NAG_C_DLTV_value & 0xffff) +
			(((NAG_C_DLTV_value_H | 0xf800) & 0xffff) << 16);

	NAG_C_DLTV_mV_value = REG_to_MV_value(NAG_C_DLTV_reg_value);
	*nag_c_dltv = NAG_C_DLTV_mV_value;

	bm_debug("[fg_bat_nafg][fgauge_get_nafg_c_dltv] mV:Reg[%d:%d][b10:%d][26_16(0x%04x) 15_00(0x%04x)]\n",
		NAG_C_DLTV_mV_value, NAG_C_DLTV_reg_value,
		bcheckbit10, NAG_C_DLTV_value_H,
		NAG_C_DLTV_value);

	return 0;

}

static void fgauge_set_zcv_intr_internal(
	struct gauge_device *gauge_dev, int fg_zcv_det_time, int fg_zcv_car_th)
{
	int fg_zcv_car_thr_h_reg, fg_zcv_car_thr_l_reg;
	long long fg_zcv_car_th_reg = fg_zcv_car_th;

	fg_zcv_car_th_reg = (fg_zcv_car_th_reg * 100 * 3600 * 1000);
#if defined(__LP64__) || defined(_LP64)
	do_div(fg_zcv_car_th_reg, UNIT_FGCAR_ZCV);
#else
	fg_zcv_car_th_reg = div_s64(fg_zcv_car_th_reg, UNIT_FGCAR_ZCV);
#endif

	if (gauge_dev->fg_cust_data->r_fg_value != 100)
#if defined(__LP64__) || defined(_LP64)
		fg_zcv_car_th_reg = (fg_zcv_car_th_reg *
		gauge_dev->fg_cust_data->r_fg_value) / 100;
#else
		fg_zcv_car_th_reg = div_s64(fg_zcv_car_th_reg *
		gauge_dev->fg_cust_data->r_fg_value, 100);
#endif

#if defined(__LP64__) || defined(_LP64)
	fg_zcv_car_th_reg = ((fg_zcv_car_th_reg * 1000) /
	gauge_dev->fg_cust_data->car_tune_value);
#else
	fg_zcv_car_th_reg = div_s64((fg_zcv_car_th_reg * 1000),
	gauge_dev->fg_cust_data->car_tune_value);
#endif

	fg_zcv_car_thr_h_reg = (fg_zcv_car_th_reg & 0xffff0000) >> 16;
	fg_zcv_car_thr_l_reg = fg_zcv_car_th_reg & 0x0000ffff;

	pmic_set_register_value(PMIC_FG_ZCV_DET_IV, fg_zcv_det_time);
	pmic_set_register_value(PMIC_FG_ZCV_CAR_TH_15_00, fg_zcv_car_thr_l_reg);
	pmic_set_register_value(PMIC_FG_ZCV_CAR_TH_30_16, fg_zcv_car_thr_h_reg);

	bm_debug("[FG_ZCV_INT][fg_set_zcv_intr_internal] det_time %d mv %d reg %lld 30_16 0x%x 15_00 0x%x\n",
			fg_zcv_det_time, fg_zcv_car_th,
			fg_zcv_car_th_reg, fg_zcv_car_thr_h_reg,
			fg_zcv_car_thr_l_reg);
}

static int fgauge_enable_zcv_interrupt(struct gauge_device *gauge_dev, int en)
{
	if (en != 0)
		en = 1;
	pmic_set_register_value(PMIC_FG_ZCV_DET_EN, en);
	pmic_set_register_value(PMIC_RG_INT_EN_FG_ZCV, en);
	bm_debug("[FG_ZCV_INT][fg_set_zcv_intr_en] En %d\n", en);

	return 0;
}

static int fgauge_set_zcv_interrupt_threshold(
	struct gauge_device *gauge_dev, int threshold)
{
	int fg_zcv_det_time = gauge_dev->fg_cust_data->zcv_suspend_time;
	int fg_zcv_car_th = threshold;

	fgauge_set_zcv_intr_internal(gauge_dev, fg_zcv_det_time, fg_zcv_car_th);

	return 0;
}

void battery_dump_nag(void)
{
	unsigned int nag_vbat_reg, vbat_val;
	int nag_vbat_mv, i = 0;

	do {
		nag_vbat_reg = upmu_get_reg_value(PMIC_AUXADC_ADC_OUT_NAG_ADDR);
		if ((nag_vbat_reg & 0x8000) != 0)
			break;
		msleep(30);
		i++;
	} while (i <= 5);

	vbat_val = nag_vbat_reg & 0x7fff;
	nag_vbat_mv = REG_to_MV_value(vbat_val);

	bm_err("[read_nafg_vbat] i:%d nag_vbat_reg 0x%x nag_vbat_mv %d:%d\n",
		i, nag_vbat_reg, nag_vbat_mv, vbat_val);

	bm_err("[read_nafg_vbat1] %d %d %d %d %d %d %d %d %d\n",
		pmic_get_register_value(PMIC_AUXADC_NAG_C_DLTV_IRQ),
		pmic_get_register_value(PMIC_AUXADC_NAG_IRQ_EN),
		pmic_get_register_value(PMIC_AUXADC_NAG_PRD),
		pmic_get_register_value(PMIC_AUXADC_NAG_VBAT1_SEL),
		pmic_get_register_value(PMIC_AUXADC_NAG_CLR),
		pmic_get_register_value(PMIC_AUXADC_NAG_EN),
		pmic_get_register_value(PMIC_AUXADC_NAG_ZCV),
		pmic_get_register_value(PMIC_AUXADC_NAG_C_DLTV_TH_15_0),
		pmic_get_register_value(PMIC_AUXADC_NAG_C_DLTV_TH_26_16)
		);

	bm_err("[read_nafg_vbat2] %d %d %d %d %d %d %d %d %d %d %d %d\n",
		pmic_get_register_value(PMIC_RG_AUXADC_CK_PDN_HWEN),
		pmic_get_register_value(PMIC_RG_AUXADC_CK_PDN),
		pmic_get_register_value(PMIC_RG_AUXADC_NAG_CK_SW_MODE),
		pmic_get_register_value(PMIC_RG_AUXADC_NAG_CK_SW_EN),
		pmic_get_register_value(PMIC_RG_AUXADC_32K_CK_PDN_HWEN),
		pmic_get_register_value(PMIC_RG_AUXADC_32K_CK_PDN),
		pmic_get_register_value(PMIC_RG_AUXADC_1M_CK_PDN_HWEN),
		pmic_get_register_value(PMIC_RG_AUXADC_1M_CK_PDN),
		pmic_get_register_value(PMIC_RG_AUXADC_RST),
		pmic_get_register_value(PMIC_RG_INT_EN_NAG_C_DLTV),
		pmic_get_register_value(PMIC_RG_INT_MASK_NAG_C_DLTV),
		pmic_get_register_value(PMIC_RG_INT_STATUS_NAG_C_DLTV)
	);
}

static int fgauge_get_nag_vbat(struct gauge_device *gauge_dev, int *vbat)
{
	unsigned int nag_vbat_reg, vbat_val;
	int nag_vbat_mv, i = 0;

	do {
		nag_vbat_reg = upmu_get_reg_value(PMIC_AUXADC_ADC_OUT_NAG_ADDR);
		if ((nag_vbat_reg & 0x8000) != 0)
			break;
		msleep(30);
		i++;
	} while (i <= 5);

	vbat_val = nag_vbat_reg & 0x7fff;
	nag_vbat_mv = REG_to_MV_value(vbat_val);
	*vbat = nag_vbat_mv;

	return 0;
}


static int fgauge_enable_battery_tmp_lt_interrupt(
	struct gauge_device *gauge_dev, bool en, int threshold)
{
	int tmp_int_lt = 0;

	if (en == 0) {
		pmic_set_register_value(PMIC_RG_INT_EN_BAT_TEMP_L, 0);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_IRQ_EN_MAX, 0);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_EN_MAX, 0);
	} else {
		tmp_int_lt = MV_to_REG_12_temp_value(threshold);

		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_VOLT_MAX,
			tmp_int_lt);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_DET_PRD_15_0,
			3333);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_DET_PRD_19_16, 0);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_DEBT_MAX, 3);

		pmic_set_register_value(PMIC_RG_INT_EN_BAT_TEMP_L, 1);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_IRQ_EN_MAX, 1);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_EN_MAX, 1);

	}


	bm_debug("[gauge_enable_battery_tmp_lt_interrupt] en:%d mv:%d reg:%d\n",
			en, threshold, tmp_int_lt);


	return 0;
}


static int fgauge_enable_battery_tmp_ht_interrupt(
	struct gauge_device *gauge_dev, bool en, int threshold)
{
	int tmp_int_ht = 0;

	if (en == 0) {
		pmic_set_register_value(PMIC_RG_INT_EN_BAT_TEMP_H, 0);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_IRQ_EN_MIN, 0);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_EN_MIN, 0);
	} else {
		tmp_int_ht = MV_to_REG_12_temp_value(threshold);
		pmic_set_register_value(PMIC_RG_INT_EN_BAT_TEMP_H, 1);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_VOLT_MIN,
			tmp_int_ht);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_DET_PRD_15_0,
			3333);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_DET_PRD_19_16, 0);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_DEBT_MIN, 3);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_IRQ_EN_MIN, 1);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_EN_MIN, 1);

	}

	bm_debug("[gauge_enable_battery_tmp_ht_interrupt] en:%d mv:%d reg:%d\n",
			en, threshold, tmp_int_ht);

	return 0;
}

int fgauge_get_time(struct gauge_device *gauge_dev, unsigned int *ptime)
{
	unsigned int time_29_16, time_15_00, ret_time;
	int m = 0;
	unsigned int ret = 0;
	long long time = 0;

	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0001, 0x1F05, 0x0);
	/*(2)	 Keep i2c read when status = 1 (0x06) */
	m = 0;
	while (fg_get_data_ready_status() == 0) {
		m++;
		if (m > 1000) {
			bm_err(
				 "[fgauge_get_time] fg_get_data_ready_status timeout 1 !\r\n");
			break;
		}
	}

	time_15_00 = pmic_get_register_value(PMIC_FG_NTER_15_00);
	time_29_16 = pmic_get_register_value(PMIC_FG_NTER_29_16);

	time = time_15_00;
	time |= time_29_16 << 16;
#if defined(__LP64__) || defined(_LP64)
	time = time * UNIT_TIME / 100;
#else
	time = div_s64(time * UNIT_TIME, 100);
#endif
	ret_time = time;

	bm_trace(
		 "[fgauge_get_time] low:0x%x high:0x%x rtime:0x%llx 0x%x!\r\n",
		 time_15_00, time_29_16, time, ret_time);


	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0008, 0x000F, 0x0);

	m = 0;
	while (fg_get_data_ready_status() != 0) {
		m++;
		if (m > 1000) {
			bm_err(
				 "[fgauge_get_time] fg_get_data_ready_status timeout 2 !\r\n");
			break;
		}
	}
	/*(8)	 Recover original settings */
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0000, 0x000F, 0x0);

	*ptime = ret_time;

	return 0;
}

int fgauge_set_time_interrupt(struct gauge_device *gauge_dev, int threshold)
{
	unsigned int time_29_16, time_15_00;
	int m = 0;
	unsigned int ret = 0;
	long long time = 0, time2;
	long long now;
	unsigned int offsetTime = threshold;

	if (offsetTime == 0) {
		pmic_enable_interrupt(FG_TIME_NO, 0, "GM30");
		return 0;
	}

	do {
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0001, 0x1F05, 0x0);
	/*(2)	 Keep i2c read when status = 1 (0x06) */
	m = 0;
	while (fg_get_data_ready_status() == 0) {
		m++;
		if (m > 1000) {
			bm_err(
				 "[fgauge_set_time_interrupt] fg_get_data_ready_status timeout 1 !\r\n");
			break;
		}
	}

	time_15_00 = pmic_get_register_value(PMIC_FG_NTER_15_00);
	time_29_16 = pmic_get_register_value(PMIC_FG_NTER_29_16);

	time = time_15_00;
	time |= time_29_16 << 16;
		now = time;
#if defined(__LP64__) || defined(_LP64)
	time = time + offsetTime * 100 / UNIT_TIME;
#else
	time = div_s64(time + offsetTime * 100, UNIT_TIME);
#endif

	bm_debug(
			 "[fgauge_set_time_interrupt] now:%lld time:%lld\r\n",
			 now/2, time/2);
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0008, 0x000F, 0x0);

	m = 0;
	while (fg_get_data_ready_status() != 0) {
		m++;
		if (m > 1000) {
			bm_err(
				 "[fgauge_set_time_interrupt] fg_get_data_ready_status timeout 2 !\r\n");
			break;
		}
	}
	/*(8)	 Recover original settings */
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0000, 0x000F, 0x0);

	pmic_enable_interrupt(FG_TIME_NO, 0, "GM30");
	pmic_set_register_value(PMIC_FG_TIME_HTH_15_00, (time & 0xffff));
	pmic_set_register_value(PMIC_FG_TIME_HTH_29_16,
		((time & 0x3fff0000) >> 16));
	pmic_enable_interrupt(FG_TIME_NO, 1, "GM30");


		/*read again to confirm */
		ret = pmic_config_interface(
			MT6355_FGADC_CON1, 0x0001, 0x1F05, 0x0);
		/*(2)	 Keep i2c read when status = 1 (0x06) */
		m = 0;
		while (fg_get_data_ready_status() == 0) {
			m++;
			if (m > 1000) {
				bm_err(
					 "[fgauge_set_time_interrupt] fg_get_data_ready_status timeout 1 !\r\n");
				break;
			}
		}

		time_15_00 = pmic_get_register_value(PMIC_FG_NTER_15_00);
		time_29_16 = pmic_get_register_value(PMIC_FG_NTER_29_16);
		time2 = time_15_00;
		time2 |= time_29_16 << 16;

		bm_debug(
			 "[fgauge_set_time_interrupt] now:%lld time:%lld\r\n",
			 time2/2, time/2);
		ret = pmic_config_interface(
				MT6355_FGADC_CON1, 0x0008, 0x000F, 0x0);

		m = 0;
		while (fg_get_data_ready_status() != 0) {
			m++;
			if (m > 1000) {
				bm_err(
					 "[fgauge_set_time_interrupt] fg_get_data_ready_status timeout 2 !\r\n");
				break;
			}
		}
		/*(8)	 Recover original settings */
		ret = pmic_config_interface(
			MT6355_FGADC_CON1, 0x0000, 0x000F, 0x0);

		bm_trace(
			 "[fgauge_set_time_interrupt] low:0x%x high:0x%x time:%lld %lld\r\n",
			 pmic_get_register_value(PMIC_FG_TIME_HTH_15_00),
			 pmic_get_register_value(PMIC_FG_TIME_HTH_29_16),
			 time, time2);
	} while (time2 >= time);
	return 0;

}

static void Intr_Number_to_Name(char *intr_name, int intr_no)
{
	switch (intr_no) {
	case FG_INTR_0:
		sprintf(intr_name, "FG_INTR_INIT");
		break;

	case FG_INTR_TIMER_UPDATE:
		sprintf(intr_name, "FG_INTR_TIMER_UPDATE");
		break;

	case FG_INTR_BAT_CYCLE:
		sprintf(intr_name, "FG_INTR_BAT_CYCLE");
		break;

	case FG_INTR_CHARGER_OUT:
		sprintf(intr_name, "FG_INTR_CHARGER_OUT");
		break;

	case FG_INTR_CHARGER_IN:
		sprintf(intr_name, "FG_INTR_CHARGER_IN");
		break;

	case FG_INTR_FG_TIME:
		sprintf(intr_name, "FG_INTR_FG_TIME");
		break;

	case FG_INTR_BAT_INT1_HT:
		sprintf(intr_name, "FG_INTR_COULOMB_HT");
		break;

	case FG_INTR_BAT_INT1_LT:
		sprintf(intr_name, "FG_INTR_COULOMB_LT");
		break;

	case FG_INTR_BAT_INT2_HT:
		sprintf(intr_name, "FG_INTR_UISOC_HT");
		break;

	case FG_INTR_BAT_INT2_LT:
		sprintf(intr_name, "FG_INTR_UISOC_LT");
		break;

	case FG_INTR_BAT_TMP_HT:
		sprintf(intr_name, "FG_INTR_BAT_TEMP_HT");
		break;

	case FG_INTR_BAT_TMP_LT:
		sprintf(intr_name, "FG_INTR_BAT_TEMP_LT");
		break;

	case FG_INTR_BAT_TIME_INT:
		/* fixme : don't know what it is */
		sprintf(intr_name, "FG_INTR_BAT_TIME_INT");
		break;

	case FG_INTR_NAG_C_DLTV:
		sprintf(intr_name, "FG_INTR_NAFG_VOLTAGE");
		break;

	case FG_INTR_FG_ZCV:
		sprintf(intr_name, "FG_INTR_FG_ZCV");
		break;

	case FG_INTR_SHUTDOWN:
		sprintf(intr_name, "FG_INTR_SHUTDOWN");
		break;

	case FG_INTR_RESET_NVRAM:
		sprintf(intr_name, "FG_INTR_RESET_NVRAM");
		break;

	case FG_INTR_BAT_PLUGOUT:
		sprintf(intr_name, "FG_INTR_BAT_PLUGOUT");
		break;

	case FG_INTR_IAVG:
		sprintf(intr_name, "FG_INTR_IAVG");
		break;

	case FG_INTR_VBAT2_L:
		sprintf(intr_name, "FG_INTR_VBAT2_L");
		break;

	case FG_INTR_VBAT2_H:
		sprintf(intr_name, "FG_INTR_VBAT2_H");
		break;

	case FG_INTR_CHR_FULL:
		sprintf(intr_name, "FG_INTR_CHR_FULL");
		break;

	case FG_INTR_DLPT_SD:
		sprintf(intr_name, "FG_INTR_DLPT_SD");
		break;

	case FG_INTR_BAT_TMP_C_HT:
		sprintf(intr_name, "FG_INTR_BAT_TMP_C_HT");
		break;

	case FG_INTR_BAT_TMP_C_LT:
		sprintf(intr_name, "FG_INTR_BAT_TMP_C_LT");
		break;

	case FG_INTR_BAT_INT1_CHECK:
		sprintf(intr_name, "FG_INTR_COULOMB_C");
		break;

	default:
		sprintf(intr_name, "FG_INTR_UNKNOWN");
		bm_err("[Intr_Number_to_Name] unknown intr %d\n", intr_no);
		break;
	}
}

int fgauge_get_hw_status(
	struct gauge_device *gauge_dev,
	struct gauge_hw_status *gauge_status, int intr_no)
{
	int ret, m;
	char intr_name[32];
	int is_iavg_valid;
	int iavg_th;
	unsigned int time;

	Intr_Number_to_Name(intr_name, intr_no);

	/* Set Read Latchdata */
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0001, 0x000F, 0x0);
	m = 0;
		while (fg_get_data_ready_status() == 0) {
			m++;
			if (m > 1000) {
				bm_err(
				"[read_fg_hw_info] fg_get_data_ready_status timeout 1 !\r\n");
				break;
			}
		}

	/* Current_1 */
	read_fg_hw_info_current_1(gauge_dev);

	/* Current_2 */
	read_fg_hw_info_current_2(gauge_dev);

	/* Iavg */
	read_fg_hw_info_Iavg(gauge_dev, &is_iavg_valid);
	if ((is_iavg_valid == 1) && (gauge_status->iavg_intr_flag == 0)) {
		bm_trace("[read_fg_hw_info] set first fg_set_iavg_intr %d %d\n",
			is_iavg_valid, gauge_status->iavg_intr_flag);
		gauge_status->iavg_intr_flag = 1;
		iavg_th = gauge_dev->fg_cust_data->diff_iavg_th;
		ret = fg_set_iavg_intr(gauge_dev, &iavg_th);
	} else if (is_iavg_valid == 0) {
		gauge_status->iavg_intr_flag = 0;
		pmic_set_register_value(PMIC_RG_INT_EN_FG_IAVG_H, 0);
		pmic_set_register_value(PMIC_RG_INT_EN_FG_IAVG_L, 0);
		pmic_enable_interrupt(FG_IAVG_H_NO, 0, "GM30");
		pmic_enable_interrupt(FG_IAVG_L_NO, 0, "GM30");
		bm_trace(
			"[read_fg_hw_info] doublecheck first fg_set_iavg_intr %d %d\n",
			is_iavg_valid, gauge_status->iavg_intr_flag);
	}
	bm_trace("[read_fg_hw_info] thirdcheck first fg_set_iavg_intr %d %d\n",
		is_iavg_valid, gauge_status->iavg_intr_flag);

	/* Ncar */
	read_fg_hw_info_ncar(gauge_dev);

	/* recover read */
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0008, 0x000F, 0x0);
	m = 0;
		while (fg_get_data_ready_status() != 0) {
			m++;
			if (m > 1000) {
				bm_err(
					"[read_fg_hw_info] fg_get_data_ready_status timeout 2 !\r\n");
				break;
			}
		}
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0000, 0x000F, 0x0);


	fgauge_get_coulomb(gauge_dev, &gauge_dev->fg_hw_info.car);
	fgauge_get_time(gauge_dev, &time);
	gauge_dev->fg_hw_info.time = time;

	bm_err("[FGADC_intr_end][%s][read_fg_hw_info] curr_1 %d curr_2 %d Iavg %d sign %d car %d ncar %d time %d\n",
		intr_name, gauge_dev->fg_hw_info.current_1,
		gauge_dev->fg_hw_info.current_2,
		gauge_dev->fg_hw_info.current_avg,
		gauge_dev->fg_hw_info.current_avg_sign,
		gauge_dev->fg_hw_info.car,
		gauge_dev->fg_hw_info.ncar,
		gauge_dev->fg_hw_info.time);
	return 0;

}

int fgauge_enable_bat_plugout_interrupt(struct gauge_device *gauge_dev, int en)
{
	if (en == 0)
		pmic_enable_interrupt(FG_BAT_PLUGOUT_NO, 0, "GM30");
	else
		pmic_enable_interrupt(FG_BAT_PLUGOUT_NO, 1, "GM30");
	return 0;
}

int fgauge_enable_iavg_interrupt(
	struct gauge_device *gauge_dev, bool ht_en, int ht_th,
	bool lt_en, int lt_th)
{
	pmic_set_register_value(PMIC_RG_INT_EN_FG_IAVG_H, ht_en);
	pmic_set_register_value(PMIC_RG_INT_EN_FG_IAVG_L, lt_en);

	return 0;
}

int fgauge_enable_vbat_low_interrupt(struct gauge_device *gauge_dev, int en)
{
	pmic_set_register_value(PMIC_RG_INT_EN_BAT2_L, en);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_IRQ_EN_MIN, en);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_EN_MIN, en);

	return 0;
}

int fgauge_enable_vbat_high_interrupt(struct gauge_device *gauge_dev, int en)
{
	pmic_set_register_value(PMIC_RG_INT_EN_BAT2_H, en);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_IRQ_EN_MAX, en);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_EN_MAX, en);

	return 0;
}

int fgauge_set_vbat_low_threshold(struct gauge_device *gauge_dev, int threshold)
{
	int vbat2_l_th_mv =  threshold;
	int vbat2_l_th_reg = MV_to_REG_12_value(vbat2_l_th_mv);
	int vbat2_det_time_15_0 = ((1000 *
		gauge_dev->fg_cust_data->vbat2_det_time) & 0xffff);
	int vbat2_det_time_19_16 = ((1000 *
		gauge_dev->fg_cust_data->vbat2_det_time) & 0xffff0000) >> 16;
	int vbat2_det_counter = gauge_dev->fg_cust_data->vbat2_det_counter;

	pmic_set_register_value(PMIC_AUXADC_LBAT2_VOLT_MIN,
		vbat2_l_th_reg);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_DET_PRD_15_0,
		vbat2_det_time_15_0);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_DET_PRD_19_16,
		vbat2_det_time_19_16);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_DEBT_MIN, vbat2_det_counter);

	bm_debug("[fg_set_vbat2_l_th] set [0x%x 0x%x 0x%x 0x%x] get [0x%x 0x%x 0x%x 0x%x]\n",
		vbat2_l_th_reg, vbat2_det_time_15_0,
		vbat2_det_time_19_16, vbat2_det_counter,
		pmic_get_register_value(PMIC_AUXADC_LBAT2_VOLT_MIN),
		pmic_get_register_value(PMIC_AUXADC_LBAT2_DET_PRD_15_0),
		pmic_get_register_value(PMIC_AUXADC_LBAT2_DET_PRD_19_16),
		pmic_get_register_value(PMIC_AUXADC_LBAT2_DEBT_MIN));

	return 0;
}


int fgauge_set_vbat_high_threshold(
	struct gauge_device *gauge_dev, int threshold)
{
	int vbat2_h_th_mv =  threshold;
	int vbat2_h_th_reg = MV_to_REG_12_value(vbat2_h_th_mv);
	int vbat2_det_time_15_0 =
		((1000 * gauge_dev->fg_cust_data->vbat2_det_time)
		& 0xffff);
	int vbat2_det_time_19_16 =
		((1000 * gauge_dev->fg_cust_data->vbat2_det_time)
		& 0xffff0000) >> 16;
	int vbat2_det_counter = gauge_dev->fg_cust_data->vbat2_det_counter;

	pmic_set_register_value(PMIC_AUXADC_LBAT2_VOLT_MAX, vbat2_h_th_reg);
	pmic_set_register_value(
		PMIC_AUXADC_LBAT2_DET_PRD_15_0, vbat2_det_time_15_0);
	pmic_set_register_value(
		PMIC_AUXADC_LBAT2_DET_PRD_19_16, vbat2_det_time_19_16);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_DEBT_MAX, vbat2_det_counter);

	bm_debug("[fg_set_vbat2_h_th] set [0x%x 0x%x 0x%x 0x%x] get [0x%x 0x%x 0x%x 0x%x]\n",
		vbat2_h_th_reg, vbat2_det_time_15_0,
		vbat2_det_time_19_16, vbat2_det_counter,
		pmic_get_register_value(PMIC_AUXADC_LBAT2_VOLT_MAX),
		pmic_get_register_value(PMIC_AUXADC_LBAT2_DET_PRD_15_0),
		pmic_get_register_value(PMIC_AUXADC_LBAT2_DET_PRD_19_16),
		pmic_get_register_value(PMIC_AUXADC_LBAT2_DEBT_MAX));

	return 0;
}

static signed int fgauge_get_AUXADC_current_rawdata(unsigned short *uvalue16)
{
	int m;
	int ret;
	/* (1)    Set READ command */
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0001, 0x000F, 0x0);

	/*(2)     Keep i2c read when status = 1 (0x06) */
	m = 0;
	while (fg_get_data_ready_status() == 0) {
		m++;
		if (m > 1000) {
			bm_err(
			"[fgauge_get_AUXADC_current_rawdata] fg_get_data_ready_status timeout 1 !\r\n");
			break;
		}
	}

	/* (3)    Read FG_CURRENT_OUT[15:08] */
	/* (4)    Read FG_CURRENT_OUT[07:00] */
	*uvalue16 = pmic_get_register_value(PMIC_FG_CURRENT_OUT);

	/* (5)    (Read other data) */
	/* (6)    Clear status to 0 */
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0008, 0x000F, 0x0);

	/* (7)    Keep i2c read when status = 0 (0x08) */
	m = 0;
	while (fg_get_data_ready_status() != 0) {
		m++;
		if (m > 1000) {
			bm_err(
			 "[fgauge_get_AUXADC_current_rawdata] fg_get_data_ready_status timeout 2 !\r\n");
			break;
		}
	}

	/*(8)    Recover original settings */
	ret = pmic_config_interface(MT6355_FGADC_CON1, 0x0000, 0x000F, 0x0);

	return ret;
}

static int fgauge_enable_car_tune_value_calibration(
	struct gauge_device *gauge_dev,
	int meta_input_cali_current, int *car_tune_value)
{
	int cali_car_tune;
	long long sum_all = 0;
	long long temp_sum = 0;
	int	avg_cnt = 0;
	int i;
	unsigned short uvalue16;
	unsigned int uvalue32;
	signed int dvalue = 0;
	long long Temp_Value1 = 0;
	long long Temp_Value2 = 0;
	long long current_from_ADC = 0;

	if (meta_input_cali_current != 0) {
		for (i = 0; i < CALI_CAR_TUNE_AVG_NUM; i++) {
			if (!fgauge_get_AUXADC_current_rawdata(&uvalue16)) {
				uvalue32 = (unsigned int) uvalue16;
				if (uvalue32 <= 0x8000) {
					Temp_Value1 = (long long)uvalue32;
					bm_err("[111]uvalue16 %d uvalue32 %d Temp_Value1 %lld\n",
						uvalue16, uvalue32,
						Temp_Value1);
				} else if (uvalue32 > 0x8000) {
					Temp_Value1 =
					(long long) (65535 - uvalue32);
					bm_err("[222]uvalue16 %d uvalue32 %d Temp_Value1 %lld\n",
						uvalue16, uvalue32,
						Temp_Value1);
				}
				sum_all += Temp_Value1;
				avg_cnt++;

				bm_err("[333]uvalue16 %d uvalue32 %d Temp_Value1 %lld sum_all %lld\n",
						uvalue16, uvalue32,
						Temp_Value1, sum_all);

			}
			mdelay(30);
		}
		/*calculate the real world data    */
		/*current_from_ADC = sum_all / avg_cnt;*/
		temp_sum = sum_all;
		bm_err("[444]sum_all %lld temp_sum %lld avg_cnt %d current_from_ADC %lld\n",
			sum_all, temp_sum, avg_cnt, current_from_ADC);
#if defined(__LP64__) || defined(_LP64)
		do_div(temp_sum, avg_cnt);
#else
		temp_sum = div_s64(temp_sum, avg_cnt);
#endif
		current_from_ADC = temp_sum;

		bm_err("[555]sum_all %lld temp_sum %lld avg_cnt %d current_from_ADC %lld\n",
			sum_all, temp_sum, avg_cnt, current_from_ADC);

		Temp_Value2 = current_from_ADC * UNIT_FGCURRENT;

		bm_err("[555]Temp_Value2 %lld current_from_ADC %lld UNIT_FGCURRENT %d\n",
			Temp_Value2, current_from_ADC, UNIT_FGCURRENT);

		/* Move 100 from denominator to cali_car_tune's numerator */
		/*do_div(Temp_Value2, 1000000);*/
#if defined(__LP64__) || defined(_LP64)
		do_div(Temp_Value2, 10000);
#else
		Temp_Value2 = div_s64(Temp_Value2, 10000);
#endif

		bm_err("[666]Temp_Value2 %lld current_from_ADC %lld UNIT_FGCURRENT %d\n",
			Temp_Value2, current_from_ADC, UNIT_FGCURRENT);

		dvalue = (unsigned int) Temp_Value2;

		/* Auto adjust value */
		if (gauge_dev->fg_cust_data->r_fg_value != 100)
			dvalue = (dvalue * 100) /
			gauge_dev->fg_cust_data->r_fg_value;

		bm_err("[666]dvalue %d fg_cust_data.r_fg_value %d\n",
			dvalue, gauge_dev->fg_cust_data->r_fg_value);

		/* Move 100 from denominator to cali_car_tune's numerator */
		/*cali_car_tune = meta_input_cali_current * 1000 / dvalue;*/
		cali_car_tune = meta_input_cali_current * 1000 * 100 / dvalue;

		bm_err("[777]dvalue %d fg_cust_data.r_fg_value %d cali_car_tune %d\n",
			dvalue, gauge_dev->fg_cust_data->r_fg_value,
			cali_car_tune);
		*car_tune_value = cali_car_tune;

		bm_err(
			"[fgauge_meta_cali_car_tune_value][%d] meta:%d, adc:%lld, UNI_FGCUR:%d, r_fg_value:%d\n",
			cali_car_tune, meta_input_cali_current,
			current_from_ADC, UNIT_FGCURRENT,
			gauge_dev->fg_cust_data->r_fg_value);

		return 0;
	}

	return 0;
}

int fgauge_set_rtc_ui_soc(struct gauge_device *gauge_dev, int rtc_ui_soc)
{
	int spare3_reg = get_rtc_spare_fg_value();
	int spare3_reg_valid;
	int new_spare3_reg;

	spare3_reg_valid = (spare3_reg & 0x80);
	new_spare3_reg = spare3_reg_valid + rtc_ui_soc;

	/* set spare3 0x7f */
	set_rtc_spare_fg_value(new_spare3_reg);

	bm_notice("[fg_set_rtc_ui_soc] rtc_ui_soc %d spare3_reg 0x%x new_spare3_reg 0x%x\n",
		rtc_ui_soc, spare3_reg, new_spare3_reg);

	return 0;
}

int fgauge_get_rtc_ui_soc(struct gauge_device *gauge_dev, int *ui_soc)
{
	int spare3_reg = get_rtc_spare_fg_value();
	int rtc_ui_soc;

	rtc_ui_soc = (spare3_reg & 0x7f);

	*ui_soc = rtc_ui_soc;
	bm_notice("[fgauge_get_rtc_ui_soc] rtc_ui_soc %d spare3_reg 0x%x\n",
		rtc_ui_soc, spare3_reg);

	return 0;
}

int fgauge_is_rtc_invalid(struct gauge_device *gauge_dev, int *invalid)
{
	/* DON'T get spare3_reg_valid here */
	/* because it has been reset by fg_set_fg_reset_rtc_status() */

	*invalid = rtc_invalid;
	bm_notice("[fg_get_rtc_invalid] rtc_invalid %d\n", rtc_invalid);

	return 0;
}

int fgauge_set_reset_status(struct gauge_device *gauge_dev, int reset)
{
	int hw_id = pmic_get_register_value(PMIC_HWCID);
	int temp_value;
	int spare0_reg, after_rst_spare0_reg;
	int spare3_reg, after_rst_spare3_reg;

	fgauge_read_RTC_boot_status();

	/* read spare0 */
	spare0_reg = hal_rtc_get_spare_register(RTC_FG_INIT);

	/* raise 15b to reset */
	if ((hw_id & 0xff00) == 0x3500) {
		temp_value = 0x80;
		hal_rtc_set_spare_register(RTC_FG_INIT, temp_value);
		mdelay(1);
		temp_value = 0x00;
		hal_rtc_set_spare_register(RTC_FG_INIT, temp_value);
	} else {
		temp_value = 0x80;
		hal_rtc_set_spare_register(RTC_FG_INIT, temp_value);
		mdelay(1);
		temp_value = 0x20;
		hal_rtc_set_spare_register(RTC_FG_INIT, temp_value);
	}

	/* read spare0 again */
	after_rst_spare0_reg = hal_rtc_get_spare_register(RTC_FG_INIT);


	/* read spare3 */
	spare3_reg = get_rtc_spare_fg_value();

	/* set spare3 0x7f */
	set_rtc_spare_fg_value(spare3_reg | 0x80);

	/* read spare3 again */
	after_rst_spare3_reg = get_rtc_spare_fg_value();

	bm_err("[fgauge_read_RTC_boot_status] spare0 0x%x 0x%x, spare3 0x%x 0x%x\n",
		spare0_reg, after_rst_spare0_reg,
		spare3_reg, after_rst_spare3_reg);

	return 0;

}

static int fgauge_dump(
	struct gauge_device *gauge_dev, struct seq_file *m, int type)
{
	if (type == 1)
		battery_dump_nag();

	return 0;
}

static int fgauge_get_hw_version(struct gauge_device *gauge_dev)
{
	return GAUGE_HW_V2000;
}

static int fgauge_set_info(
	struct gauge_device *gauge_dev, enum gauge_info ginfo, int value)
{
	int ret = 0;

	if (ginfo == GAUGE_2SEC_REBOOT)
		pmic_config_interface(
		PMIC_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x0);
	else if (ginfo == GAUGE_PL_CHARGING_STATUS)
		pmic_config_interface(
		PMIC_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x1);
	else if (ginfo == GAUGE_MONITER_PLCHG_STATUS)
		pmic_config_interface(
		PMIC_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x2);
	else if (ginfo == GAUGE_BAT_PLUG_STATUS)
		pmic_config_interface(
		PMIC_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x3);
	else if (ginfo == GAUGE_IS_NVRAM_FAIL_MODE)
		pmic_config_interface(
		PMIC_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x4);
	else if (ginfo == GAUGE_MONITOR_SOFF_VALIDTIME)
		pmic_config_interface(
		PMIC_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x5);
	else if (ginfo == GAUGE_CON0_SOC) {
		value = value / 100;
		pmic_config_interface(
			PMIC_SYSTEM_INFO_CON0_ADDR, value, 0x007F, 0x9);
	} else
		ret = -1;

	return 0;
}

static int fgauge_get_info(
	struct gauge_device *gauge_dev, enum gauge_info ginfo, int *value)
{
	int ret = 0;

	if (ginfo == GAUGE_2SEC_REBOOT)
		pmic_read_interface(
			PMIC_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x0);
	else if (ginfo == GAUGE_PL_CHARGING_STATUS)
		pmic_read_interface(
			PMIC_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x1);
	else if (ginfo == GAUGE_MONITER_PLCHG_STATUS)
		pmic_read_interface(
			PMIC_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x2);
	else if (ginfo == GAUGE_BAT_PLUG_STATUS)
		pmic_read_interface(
			PMIC_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x3);
	else if (ginfo == GAUGE_IS_NVRAM_FAIL_MODE)
		pmic_read_interface(
			PMIC_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x4);
	else if (ginfo == GAUGE_MONITOR_SOFF_VALIDTIME)
		pmic_read_interface(
			PMIC_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x5);
	else if (ginfo == GAUGE_CON0_SOC)
		pmic_read_interface(
			PMIC_SYSTEM_INFO_CON0_ADDR, value, 0x007F, 0x9);
	else
		ret = -1;

	return 0;
}

int fgauge_set_battery_cycle_interrupt(
	struct gauge_device *gauge_dev, int threshold)
{
	long long car = threshold;
	long long carReg;

	pmic_enable_interrupt(FG_N_CHARGE_L_NO, 0, "GM30");

	car = car * CAR_TO_REG_FACTOR;
	if (fg_cust_data.r_fg_value != 100) {
		car = (car * fg_cust_data.r_fg_value);
#if defined(__LP64__) || defined(_LP64)
		do_div(car, 100);
#else
		car = div_s64(car, 100);
#endif
	}

	car = car * 1000;
#if defined(__LP64__) || defined(_LP64)
	do_div(car, fg_cust_data.car_tune_value);
#else
	car = div_s64(car, fg_cust_data.car_tune_value);
#endif

	carReg = car + 5;
#if defined(__LP64__) || defined(_LP64)
	do_div(carReg, 10);
#else
	carReg = div_s64(carReg, 10);
#endif
	carReg = 0 - carReg;

	pmic_set_register_value(PMIC_FG_N_CHARGE_LTH_15_14,
		(carReg & 0xffff)>>14);
	pmic_set_register_value(PMIC_FG_N_CHARGE_LTH_31_16,
		(carReg & 0xffff0000) >> 16);

	bm_debug("car:%d carR:%lld r:%lld current:low:0x%x high:0x%x target:low:0x%x high:0x%x",
		threshold, car, carReg,
		pmic_get_register_value(PMIC_FG_NCAR_15_00),
		pmic_get_register_value(PMIC_FG_NCAR_31_16),
		pmic_get_register_value(PMIC_FG_N_CHARGE_LTH_15_14),
		pmic_get_register_value(PMIC_FG_N_CHARGE_LTH_31_16));

	pmic_enable_interrupt(FG_N_CHARGE_L_NO, 1, "GM30");

	return 0;

}

static struct gauge_ops mt6355_gauge_ops = {
	.gauge_initial = fgauge_initial,
	.gauge_read_current = fgauge_read_current,
	.gauge_get_average_current = fgauge_get_average_current,
	.gauge_get_coulomb = fgauge_get_coulomb,
	.gauge_reset_hw = fgauge_reset_hw,
	.gauge_get_hwocv = read_hw_ocv,
	.gauge_set_coulomb_interrupt1_ht = fgauge_set_coulomb_interrupt1_ht,
	.gauge_set_coulomb_interrupt1_lt = fgauge_set_coulomb_interrupt1_lt,
	.gauge_get_boot_battery_plug_out_status =
		fgauge_read_boot_battery_plug_out_status,
	.gauge_get_ptim_current = fgauge_get_ptim_current,
	.gauge_get_zcv_current = fgauge_get_zcv_current,
	.gauge_get_zcv = fgauge_get_zcv,
	.gauge_is_gauge_initialized = fgauge_is_gauge_initialized,
	.gauge_set_gauge_initialized = fgauge_set_gauge_initialized,
	.gauge_set_battery_cycle_interrupt =
		fgauge_set_battery_cycle_interrupt,
	.gauge_reset_shutdown_time = fgauge_reset_shutdown_time,
	.gauge_reset_ncar = fgauge_reset_ncar,
	.gauge_set_nag_zcv = fgauge_set_nag_zcv,
	.gauge_set_nag_c_dltv = fgauge_set_nag_c_dltv,
	.gauge_enable_nag_interrupt = fgauge_enable_nag_interrupt,
	.gauge_get_nag_cnt = fgauge_get_nag_cnt,
	.gauge_get_nag_dltv = fgauge_get_nag_dltv,
	.gauge_get_nag_c_dltv = fgauge_get_nag_c_dltv,
	.gauge_get_nag_vbat = fgauge_get_nag_vbat,
	.gauge_enable_zcv_interrupt = fgauge_enable_zcv_interrupt,
	.gauge_set_zcv_interrupt_threshold =
		fgauge_set_zcv_interrupt_threshold,
	.gauge_get_nag_vbat = fgauge_get_nag_vbat,
	.gauge_enable_battery_tmp_lt_interrupt =
		fgauge_enable_battery_tmp_lt_interrupt,
	.gauge_enable_battery_tmp_ht_interrupt =
		fgauge_enable_battery_tmp_ht_interrupt,
	.gauge_get_time = fgauge_get_time,
	.gauge_set_time_interrupt = fgauge_set_time_interrupt,
	.gauge_get_hw_status = fgauge_get_hw_status,
	.gauge_enable_bat_plugout_interrupt =
		fgauge_enable_bat_plugout_interrupt,
	.gauge_enable_iavg_interrupt = fgauge_enable_iavg_interrupt,
	.gauge_enable_vbat_low_interrupt = fgauge_enable_vbat_low_interrupt,
	.gauge_enable_vbat_high_interrupt = fgauge_enable_vbat_high_interrupt,
	.gauge_set_vbat_low_threshold = fgauge_set_vbat_low_threshold,
	.gauge_set_vbat_high_threshold = fgauge_set_vbat_high_threshold,
	.gauge_enable_car_tune_value_calibration =
		fgauge_enable_car_tune_value_calibration,
	.gauge_set_rtc_ui_soc = fgauge_set_rtc_ui_soc,
	.gauge_get_rtc_ui_soc = fgauge_get_rtc_ui_soc,
	.gauge_is_rtc_invalid = fgauge_is_rtc_invalid,
	.gauge_set_reset_status = fgauge_set_reset_status,
	.gauge_dump = fgauge_dump,
	.gauge_get_hw_version = fgauge_get_hw_version,
	.gauge_set_info = fgauge_set_info,
	.gauge_get_info = fgauge_get_info,

};

static int mt6355_parse_dt(struct mt6355_gauge *info, struct device *dev)
{
	struct device_node *np = dev->of_node;

	pr_notice("%s: starts\n", __func__);

	if (!np) {
		pr_notice("%s: no device node\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_string(np, "gauge_name",
		&info->gauge_dev_name) < 0) {
		pr_notice("%s: no charger name\n", __func__);
		info->gauge_dev_name = "gauge";
	}

	return 0;
}


static int mt6355_gauge_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mt6355_gauge *info;

	pr_notice("%s: starts\n", __func__);

	info = devm_kzalloc(&pdev->dev,
		sizeof(struct mt6355_gauge), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	mt6355_parse_dt(info, &pdev->dev);
	platform_set_drvdata(pdev, info);

	/* Register charger device */
	info->gauge_dev = gauge_device_register(info->gauge_dev_name,
		&pdev->dev, info, &mt6355_gauge_ops, &info->gauge_prop);
	if (IS_ERR_OR_NULL(info->gauge_dev)) {
		ret = PTR_ERR(info->gauge_dev);
		goto err_register_gauge_dev;
	}

	return 0;
err_register_gauge_dev:
	devm_kfree(&pdev->dev, info);
	return ret;

}

static int mt6355_gauge_remove(struct platform_device *pdev)
{
	struct mt6355_gauge *mt = platform_get_drvdata(pdev);

	if (mt)
		devm_kfree(&pdev->dev, mt);
	return 0;
}

static void mt6355_gauge_shutdown(struct platform_device *dev)
{
}


static const struct of_device_id mt6355_gauge_of_match[] = {
	{.compatible = "mediatek,mt6355_gauge",},
	{},
};

MODULE_DEVICE_TABLE(of, mt6355_gauge_of_match);

static struct platform_driver mt6355_gauge_driver = {
	.probe = mt6355_gauge_probe,
	.remove = mt6355_gauge_remove,
	.shutdown = mt6355_gauge_shutdown,
	.driver = {
		   .name = "mt6355_gauge",
		   .of_match_table = mt6355_gauge_of_match,
		   },
};

static int __init mt6355_gauge_init(void)
{
	return platform_driver_register(&mt6355_gauge_driver);
}
device_initcall(mt6355_gauge_init);

static void __exit mt6355_gauge_exit(void)
{
	platform_driver_unregister(&mt6355_gauge_driver);
}
module_exit(mt6355_gauge_exit);

MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK Gauge Device Driver");
MODULE_LICENSE("GPL");
