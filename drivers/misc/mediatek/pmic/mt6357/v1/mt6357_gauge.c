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

#include <asm/div64.h>

#include <linux/platform_device.h>


#include <mt-plat/upmu_common.h>

#include <mach/mtk_battery_property.h>
#include <mach/mtk_pmic.h>
#include <mt-plat/mtk_battery.h>
#include <mt-plat/upmu_common.h>
#include <mt-plat/mtk_rtc_hal_common.h>
#include <mt-plat/mtk_rtc.h>
#include "include/pmic_throttling_dlpt.h"
#include <linux/proc_fs.h>
#include <linux/math64.h>
#include <linux/of.h>
#include <mtk_gauge_class.h>
#include <mtk_charger.h>
#include <mtk_battery_internal.h>


static signed int g_hw_ocv_tune_value;
static bool g_fg_is_charger_exist;

struct mt6357_gauge {
	const char *gauge_dev_name;
	struct gauge_device *gauge_dev;
	struct gauge_properties gauge_prop;
};

/*********************** MT6357 setting *********************/
/* mt6357 314.331 uA */
#define UNIT_FGCURRENT     (314331)
/* charge_lsb 19646 * 2^11 / 3600 */
#define UNIT_FGCAR         (11176)
/* MT6335 use 3, old chip use 4 */
#define R_VAL_TEMP_2         (1)
/* MT6335 use 3, old chip use 4 */
#define R_VAL_TEMP_3         (3)
/* mt6357 0.0625 , need to * 10000 and / 10000 */
#define UNIT_TIME          (50)
/* mt6357: 19.646 * 1000, need to divide 1000 */
#define UNIT_FGCAR_ZCV     (19646)
#define UNIT_FG_IAVG		(157166)
/* 3600 * 1000 * 1000 / 157166 , for coulomb interrupt */
#define CAR_TO_REG_FACTOR  (0x5c2a)
/*coulomb interrupt lsb might be different with coulomb lsb */
#define CAR_TO_REG_SHIFT (3)


#define VOLTAGE_FULL_RANGE    1800
#define ADC_PRECISE           32768	/* 12 bits */

enum {
	FROM_SW_OCV = 1,
	FROM_6357_PLUG_IN,
	FROM_6357_PON_ON,
	FROM_6336_CHR_IN
};


int MV_to_REG_12_value(signed int _reg)
{
	int ret = (_reg * 4096) / (VOLTAGE_FULL_RANGE * 10 * R_VAL_TEMP_3);

	bm_trace("[%s] %d => %d\n", __func__, _reg, ret);
	return ret;
}

int MV_to_REG_12_temp_value(signed int _reg)
{
	int ret = (_reg * 4096) / (VOLTAGE_FULL_RANGE * 10 * R_VAL_TEMP_2);

	bm_trace("[%s] %d => %d\n", __func__, _reg, ret);
	return ret;
}

static signed int REG_to_MV_value(signed int _reg)
{
	long long _reg64 = _reg;
	int ret;

#if defined(__LP64__) || defined(_LP64)
	_reg64 = (_reg64 * VOLTAGE_FULL_RANGE * 10
		* R_VAL_TEMP_3) / ADC_PRECISE;
#else
	_reg64 = div_s64(_reg64 * VOLTAGE_FULL_RANGE
		* 10 * R_VAL_TEMP_3, ADC_PRECISE);
#endif
	ret = _reg64;

	bm_trace("[%s] %lld => %d\n", __func__, _reg64, ret);
	return ret;
}

static signed int MV_to_REG_value(signed int _mv)
{
	int ret;
	long long _reg64 = _mv;
#if defined(__LP64__) || defined(_LP64)
	_reg64 = (_reg64 * ADC_PRECISE) /
		(VOLTAGE_FULL_RANGE * 10 * R_VAL_TEMP_3);
#else
	_reg64 = div_s64((_reg64 * ADC_PRECISE),
		(VOLTAGE_FULL_RANGE * 10 * R_VAL_TEMP_3));
#endif
	ret = _reg64;

	if (ret <= 0) {
		bm_err(
			"[fg_bat_nafg][%s] mv=%d,%lld => %d,\n",
			__func__, _mv, _reg64, ret);
		return ret;
	}

	bm_trace("[%s] mv=%d,%lld => %d,\n", __func__, _mv, _reg64, ret);
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

static int fgauge_set_info(
	struct gauge_device *gauge_dev,
	enum gauge_info ginfo, int value)
{
	int ret = 0;
	int value_mask = 0;
	int sign_bit = 0;


	if (ginfo == GAUGE_2SEC_REBOOT)
		pmic_config_interface(
		PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x0);
	else if (ginfo == GAUGE_PL_CHARGING_STATUS)
		pmic_config_interface(
		PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x1);
	else if (ginfo == GAUGE_MONITER_PLCHG_STATUS)
		pmic_config_interface(
		PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x2);
	else if (ginfo == GAUGE_BAT_PLUG_STATUS)
		pmic_config_interface(
		PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x3);
	else if (ginfo == GAUGE_IS_NVRAM_FAIL_MODE)
		pmic_config_interface(
		PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x4);
	else if (ginfo == GAUGE_CON0_SOC) {
		value = value / 100;
		pmic_config_interface(
			PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x007F, 0x9);
	} else if (ginfo == GAUGE_SHUTDOWN_CAR) {
		if (value == -99999) {
			/* write invalid */
			ret = pmic_config_interface(
				PMIC_RG_SYSTEM_INFO_CON1_ADDR,
				0x1FF,
				0x01FF,
				0x7);

			bm_err("[%s]: write invalid value to GAUGE_SHUTDOWN_CAR ret:%d\n",
				__func__, ret);

			return 0;
		}

		if (value < 0)
			sign_bit = 1;

		value_mask = abs(value);
		value_mask = value_mask & 0x00ff;

		pmic_config_interface(
			PMIC_RG_SYSTEM_INFO_CON1_ADDR, value_mask, 0x00FF, 0x7);
		pmic_config_interface(
			PMIC_RG_SYSTEM_INFO_CON1_ADDR, sign_bit, 0x0001, 0xf);

		bm_err(
			"[%s]: GAUGE_SHUTDOWN_CAR:%d,0x%x,sign:%d, 0x%x,0x%x\n",
			__func__, value, value, sign_bit, value_mask,
			pmic_get_register_value(PMIC_RG_SYSTEM_INFO_CON1));
	} else
		ret = -1;

	return 0;
}

static int fgauge_get_info(
	struct gauge_device *gauge_dev,
	enum gauge_info ginfo,
	int *value)
{
	int ret = 0;
	int sign_bit = 0;

	if (ginfo == GAUGE_2SEC_REBOOT)
		pmic_read_interface(
			PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x0);
	else if (ginfo == GAUGE_PL_CHARGING_STATUS)
		pmic_read_interface(
			PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x1);
	else if (ginfo == GAUGE_MONITER_PLCHG_STATUS)
		pmic_read_interface(
			PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x2);
	else if (ginfo == GAUGE_BAT_PLUG_STATUS)
		pmic_read_interface(
			PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x3);
	else if (ginfo == GAUGE_IS_NVRAM_FAIL_MODE)
		pmic_read_interface(
			PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x4);
	else if (ginfo == GAUGE_CON0_SOC)
		pmic_read_interface(
			PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x007F, 0x9);
	else if (ginfo == GAUGE_SHUTDOWN_CAR) {
		pmic_read_interface(
			PMIC_RG_SYSTEM_INFO_CON1_ADDR, &sign_bit, 0x1, 0xf);

		pmic_read_interface(
			PMIC_RG_SYSTEM_INFO_CON1_ADDR, value, 0xff, 0x7);

		if (sign_bit == 1 && *value == 0xff) {
			bm_err("[%s]: GAUGE_SHUTDOWN_CAR: invalid, sign:%d value:0x%x\n",
				__func__, sign_bit, *value);
			sign_bit = 0;
			*value = 0;
		} else if (sign_bit == 1)
			*value = 0 - *value;
	} else
		ret = -1;

	return 0;
}

static int gspare3_reg;
static int rtc_invalid;
static int is_bat_plugout;
static int bat_plug_out_time;

static void fgauge_read_RTC_boot_status(struct gauge_device *gauge_dev)
{
	int spare3_reg = 0;
	int spare3_reg_valid = 0;

	spare3_reg = get_rtc_spare_fg_value();
	gspare3_reg = spare3_reg;
	spare3_reg_valid = (spare3_reg & 0x80) >> 7;

	if (spare3_reg_valid == 0)
		rtc_invalid = 1;
	else
		rtc_invalid = 0;

	bm_err(
		"[%s] rtc_invalid %d plugout %d plugout_time %d spare3 0x%x\n",
		__func__, rtc_invalid, is_bat_plugout, bat_plug_out_time,
		spare3_reg);
}


static int fgauge_initial(struct gauge_device *gauge_dev)
{
	int bat_flag = 0;
	int is_charger_exist;

#if defined(CONFIG_MTK_DISABLE_GAUGE)
#else
	pmic_set_register_value(PMIC_FG_SON_SLP_EN, 0);
#endif

	pmic_set_register_value(PMIC_AUXADC_NAG_PRD, 10);
	fgauge_get_info(gauge_dev, GAUGE_BAT_PLUG_STATUS, &bat_flag);
	fgauge_get_info(gauge_dev, GAUGE_PL_CHARGING_STATUS, &is_charger_exist);

	bm_err("bat_plug:%d chr:%d info:0x%x\n",
		bat_flag, is_charger_exist,
		upmu_get_reg_value(PMIC_RG_SYSTEM_INFO_CON0_ADDR));

	get_mtk_battery()->hw_status.pl_charger_status = is_charger_exist;

	if (is_charger_exist == 1) {
		is_bat_plugout = 1;
		fgauge_set_info(gauge_dev, GAUGE_2SEC_REBOOT, 0);
	} else {
		if (bat_flag == 0)
			is_bat_plugout = 1;
		else
			is_bat_plugout = 0;
	}

	fgauge_set_info(gauge_dev, GAUGE_BAT_PLUG_STATUS, 1);
	bat_plug_out_time = 31;	/*[12:8], 5 bits*/

	fgauge_read_RTC_boot_status(gauge_dev);
	gauge_dev->fg_hw_info.iavg_valid = 1;
	get_mtk_battery()->log.fg_reset = 0;

	return 0;
}

static int fgauge_read_current(
	struct gauge_device *gauge_dev, bool *fg_is_charging, int *data)
{
	unsigned short uvalue16 = 0;
	signed int dvalue = 0;
	int m = 0;
	unsigned long long Temp_Value = 0;
	unsigned int ret = 0;

	/* HW Init
	 *(1)    i2c_write (0x60, 0xC8, 0x01); // Enable VA2
	 *(2)    i2c_write (0x61, 0x15, 0x00); // Enable FGADC clock for digital
	 *(3)    i2c_write (0x61, 0x69, 0x28);
	 * // Set current mode, auto-calibration mode and 32KHz clock source
	 *(4)    i2c_write (0x61, 0x69, 0x29); // Enable FGADC
	 */

	/* Read HW Raw Data
	 *(1)    Set READ command
	 */
	ret = pmic_config_interface(MT6357_FGADC_CON1, 0x0001, 0x000F, 0x0);
	/*(2)     Keep i2c read when status = 1 (0x06) */
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
	 *(3)    Read FG_CURRENT_OUT[15:08]
	 *(4)    Read FG_CURRENT_OUT[07:00]
	 */
	uvalue16 = pmic_get_register_value(PMIC_FG_CURRENT_OUT);
	bm_trace("[%s] : FG_CURRENT = %x\r\n", __func__, uvalue16);
	/*
	 *(5)    (Read other data)
	 *(6)    Clear status to 0
	 */
	ret = pmic_config_interface(MT6357_FGADC_CON1, 0x0008, 0x000F, 0x0);
	/*
	 *(7)    Keep i2c read when status = 0 (0x08)
	 * while ( fg_get_sw_clear_status() != 0 )
	 */
	m = 0;
		while (fg_get_data_ready_status() != 0) {
			m++;
			if (m > 1000) {
				bm_err(
					"[%s] fg_get_data_ready_status timeout 2 !\r\n",
					__func__);
				break;
			}
		}
	/*(8)    Recover original settings */
	ret = pmic_config_interface(MT6357_FGADC_CON1, 0x0000, 0x000F, 0x0);

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
	do_div(Temp_Value, 100000);
	dvalue = (unsigned int) Temp_Value;

	if (*fg_is_charging == true)
		bm_trace(
		"[%s] current(charging) = %d mA\r\n", __func__,
			 dvalue);
	else
		bm_trace(
		"[%s] current(discharging) = %d mA\r\n", __func__,
			 dvalue);

		/* Auto adjust value */
		if (gauge_dev->fg_cust_data->r_fg_value != 100) {
			bm_trace(
			"[%s] Auto adjust value due to the Rfg is %d\n Ori current=%d, ",
			__func__, gauge_dev->fg_cust_data->r_fg_value, dvalue);

			dvalue =
				(dvalue * 100) /
				gauge_dev->fg_cust_data->r_fg_value;

			bm_trace(
				"[%s] new current=%d\n", __func__,
				dvalue);
		}

		bm_trace("[%s] ori current=%d\n", __func__, dvalue);

		dvalue =
			((dvalue * gauge_dev->fg_cust_data->car_tune_value)
			/ 1000);

		bm_debug("[%s] final current=%d (ratio=%d)\n",
			__func__, dvalue,
			gauge_dev->fg_cust_data->car_tune_value);

		*data = dvalue;

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
	unsigned long long Temp_Value = 0;
	unsigned int ret = 0;
	int reset = 0;

/*
 * HW Init
 *(1)    i2c_write (0x60, 0xC8, 0x01); // Enable VA2
 *(2)    i2c_write (0x61, 0x15, 0x00); // Enable FGADC clock for digital
 *(3)    i2c_write (0x61, 0x69, 0x28);
 * // Set current mode, auto-calibration mode and 32KHz clock source
 *(4)    i2c_write (0x61, 0x69, 0x29); // Enable FGADC
 *
 * Read HW Raw Data
 *(1)    Set READ command
 */

	/*fg_dump_register();*/

	if (reset == 0)
		ret = pmic_config_interface(
			MT6357_FGADC_CON1, 0x0001, 0x1F05, 0x0);
	else {
		ret = pmic_config_interface(
			MT6357_FGADC_CON1, 0x0705, 0x1F05, 0x0);
		bm_err("[fgauge_read_columb_internal] reset fgadc 0x0705\n");
	}

	/*(2)    Keep i2c read when status = 1 (0x06) */
	m = 0;
	while (fg_get_data_ready_status() == 0) {
		m++;
		if (m > 1000) {
			bm_err(
				 "[fgauge_read_columb_internal] fg_get_data_ready_status timeout 1 !\r\n");
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

	bm_trace(
		"[fgauge_read_columb_internal] temp_CAR_15_0 = 0x%x  temp_CAR_31_16 = 0x%x\n",
		 temp_CAR_15_0, temp_CAR_31_16);

	bm_trace(
		"[fgauge_read_columb_internal] FG_CAR = 0x%x\r\n",
		 uvalue32_CAR);
	bm_trace(
		 "[fgauge_read_columb_internal] uvalue32_CAR_MSB = 0x%x\r\n",
		 uvalue32_CAR_MSB);

/*
 *(5)    (Read other data)
 *(6)    Clear status to 0
 */
	ret = pmic_config_interface(MT6357_FGADC_CON1, 0x0008, 0x000F, 0x0);
/*
 *(7)    Keep i2c read when status = 0 (0x08)
 * while ( fg_get_sw_clear_status() != 0 )
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
	ret = pmic_config_interface(MT6357_FGADC_CON1, 0x0000, 0x000F, 0x0);

/*calculate the real world data    */
	dvalue_CAR = (signed int) uvalue32_CAR;

	if (uvalue32_CAR == 0) {
		Temp_Value = 0;
	} else if (uvalue32_CAR == 0xfffff) {
		Temp_Value = 0;
	} else if (uvalue32_CAR_MSB == 0x1) {
		/* dis-charging */
		/* keep negative value */
		Temp_Value = (long long) (dvalue_CAR - 0xfffff);
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
	do_div(Temp_Value, 10);
	Temp_Value = Temp_Value + 5;
	do_div(Temp_Value, 10);

	if (uvalue32_CAR_MSB == 0x1)
		dvalue_CAR = (signed int) (Temp_Value - (Temp_Value * 2));
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

		dvalue_CAR =
			(dvalue_CAR * 100) /
			gauge_dev->fg_cust_data->r_fg_value;

		bm_trace("[fgauge_read_columb_internal] new CAR=%d\n",
			 dvalue_CAR);
	}

	dvalue_CAR =
		((dvalue_CAR *
		gauge_dev->fg_cust_data->car_tune_value) / 1000);

	bm_debug("[%s] CAR=%d r_fg_value=%d car_tune_value=%d\n",
		__func__,
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

	bm_trace("[%s] : Start \r\n", __func__);

	while (val_car != 0x0) {
		ret = pmic_config_interface(
			MT6357_FGADC_CON1, 0x0600, 0x1F00, 0x0);
		bm_err("[%s] reset fgadc 0x0600\n", __func__);

		fgauge_get_coulomb(gauge_dev, &val_car_temp);
		val_car = val_car_temp;
	}

	bm_trace("[%s] : End \r\n", __func__);

	return 0;
}

static int read_hw_ocv_6357_plug_in(void)
{
	signed int adc_rdy = 0;
	signed int adc_result_reg = 0;
	signed int adc_result = 0;


	if (is_power_path_supported()) {
		adc_rdy = pmic_get_register_value(
				PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_SWCHR);
		adc_result_reg =
			pmic_get_register_value(
				PMIC_AUXADC_ADC_OUT_BAT_PLUGIN_SWCHR);
		adc_result = REG_to_MV_value(adc_result_reg);
		bm_debug("[oam] %s (swchr) : adc_result_reg=%d, adc_result=%d, start_sel=%d, rdy=%d\n",
			__func__, adc_result_reg, adc_result,
			pmic_get_register_value(
			PMIC_RG_STRUP_AUXADC_START_SEL),
			adc_rdy);
	} else {
		adc_rdy = pmic_get_register_value(
			PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_PCHR);
		adc_result_reg =
			pmic_get_register_value(
				PMIC_AUXADC_ADC_OUT_BAT_PLUGIN_PCHR);
		adc_result = REG_to_MV_value(adc_result_reg);
		bm_debug("[oam] %s (pchr) : adc_result_reg=%d, adc_result=%d, start_sel=%d, rdy=%d\n",
			 __func__, adc_result_reg, adc_result,
			 pmic_get_register_value(
				PMIC_RG_STRUP_AUXADC_START_SEL),
				adc_rdy);
	}

	if (adc_rdy == 1) {
		pmic_set_register_value(PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_CLR, 1);
		mdelay(1);
		pmic_set_register_value(PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_CLR, 0);
	}

	adc_result += g_hw_ocv_tune_value;
	return adc_result;
}


static int read_hw_ocv_6357_power_on(void)
{
	signed int adc_result_rdy = 0;
	signed int adc_result_reg = 0;
	signed int adc_result = 0;

	if (is_power_path_supported()) {

		adc_result_rdy =
			pmic_get_register_value(
				PMIC_AUXADC_ADC_RDY_PWRON_SWCHR);
		adc_result_reg =
			pmic_get_register_value(
				PMIC_AUXADC_ADC_OUT_PWRON_SWCHR);
		adc_result = REG_to_MV_value(adc_result_reg);
		bm_debug("[oam] %s (swchr) : adc_result_reg=%d, adc_result=%d, start_sel=%d, rdy=%d\n",
			 __func__, adc_result_reg, adc_result,
			 pmic_get_register_value(
				PMIC_RG_STRUP_AUXADC_START_SEL),
				adc_result_rdy);

		if (adc_result_rdy == 1) {
			pmic_set_register_value(
				PMIC_AUXADC_ADC_RDY_PWRON_CLR, 1);
			mdelay(1);
			pmic_set_register_value(
				PMIC_AUXADC_ADC_RDY_PWRON_CLR, 0);
		}
	} else {
		adc_result_rdy =
			pmic_get_register_value(PMIC_AUXADC_ADC_RDY_PWRON_PCHR);
		adc_result_reg =
			pmic_get_register_value(PMIC_AUXADC_ADC_OUT_PWRON_PCHR);
		adc_result = REG_to_MV_value(adc_result_reg);
		bm_debug("[oam] %s (pchr) : adc_result_reg=%d, adc_result=%d, start_sel=%d, rdy=%d\n",
			 __func__, adc_result_reg, adc_result,
			 pmic_get_register_value(
			 PMIC_RG_STRUP_AUXADC_START_SEL),
			 adc_result_rdy);

		if (adc_result_rdy == 1) {
			pmic_set_register_value(
				PMIC_AUXADC_ADC_RDY_PWRON_CLR, 1);
			mdelay(1);
			pmic_set_register_value(
				PMIC_AUXADC_ADC_RDY_PWRON_CLR, 0);
		}
	}
	adc_result += g_hw_ocv_tune_value;
	return adc_result;
}

static int read_hw_ocv_6357_power_on_rdy(void)
{
	signed int pon_rdy = 0;
	int hw_id = pmic_get_register_value(PMIC_HWCID);

	if (hw_id == 0x3510)
		pon_rdy =
			pmic_get_register_value(
				PMIC_AUXADC_ADC_RDY_WAKEUP_PCHR);
	else
		pon_rdy =
			pmic_get_register_value(PMIC_AUXADC_ADC_RDY_PWRON_PCHR);

	bm_err(
		"[%s] 0x%x pon_rdy %d\n",
		__func__, hw_id, pon_rdy);

	return pon_rdy;
}

static int charger_zcv;
static int pmic_in_zcv;
static int pmic_zcv;
static int pmic_rdy;
static int swocv;
static int zcv_from;
static int zcv_tmp;

static bool zcv_1st_read;
static int charger_zcv_1st;
static int pmic_in_zcv_1st;
static int pmic_zcv_1st;
static int pmic_rdy_1st;
static int swocv_1st;
static int zcv_from_1st;
static int zcv_tmp_1st;
static int moniter_plchg_bit;
static int pl_charging_status;

int read_hw_ocv(struct gauge_device *gauge_dev, int *data)
{
	int _hw_ocv, _sw_ocv;
	int _hw_ocv_src;
	int _prev_hw_ocv, _prev_hw_ocv_src;
	int _hw_ocv_rdy;
	int _flag_unreliable;
	int _hw_ocv_35_pon;
	int _hw_ocv_35_plugin;
	int _hw_ocv_35_pon_rdy;
	int _hw_ocv_chgin;
	int _hw_ocv_chgin_rdy;
	int now_temp;
	int now_thr;


	_hw_ocv_35_pon_rdy = read_hw_ocv_6357_power_on_rdy();
	_hw_ocv_35_pon = read_hw_ocv_6357_power_on();
	_hw_ocv_35_plugin = read_hw_ocv_6357_plug_in();
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

	/* if preloader records charge in, need to using subpmic as hwocv */
	fgauge_get_info(gauge_dev, GAUGE_PL_CHARGING_STATUS,
		&pl_charging_status);
	fgauge_set_info(gauge_dev, GAUGE_PL_CHARGING_STATUS, 0);
	fgauge_get_info(gauge_dev, GAUGE_MONITER_PLCHG_STATUS,
		&moniter_plchg_bit);
	fgauge_set_info(gauge_dev, GAUGE_MONITER_PLCHG_STATUS, 0);

	if (pl_charging_status == 1)
		g_fg_is_charger_exist = 1;
	else
		g_fg_is_charger_exist = 0;

	_hw_ocv = _hw_ocv_35_pon;
	_sw_ocv = get_mtk_battery()->hw_status.sw_ocv;
	_hw_ocv_src = FROM_6357_PON_ON;
	_prev_hw_ocv = _hw_ocv;
	_prev_hw_ocv_src = FROM_6357_PON_ON;
	_flag_unreliable = 0;

	if (g_fg_is_charger_exist) {
		_hw_ocv_rdy = _hw_ocv_35_pon_rdy;
		if (_hw_ocv_rdy == 1) {
			if (_hw_ocv_chgin_rdy == 1) {
				_hw_ocv = _hw_ocv_chgin;
				_hw_ocv_src = FROM_6336_CHR_IN;
			} else {
				_hw_ocv = _hw_ocv_35_pon;
				_hw_ocv_src = FROM_6357_PON_ON;
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
			/*_hw_ocv = _hw_ocv_35_plugin;*/
			/*_hw_ocv_src = FROM_6357_PLUG_IN;*/
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
		if (_hw_ocv_35_pon_rdy == 0) {
			_hw_ocv = _sw_ocv;
			_hw_ocv_src = FROM_SW_OCV;
		}
	}

	/* final chance to check hwocv */
	if (_hw_ocv < 30000) {
		bm_err(
			"[%s] ERROR, _hw_ocv=%d, force use swocv\n",
			__func__, _hw_ocv);
		_hw_ocv = _sw_ocv;
		_hw_ocv_src = FROM_SW_OCV;
	}

	*data = _hw_ocv;

	charger_zcv = _hw_ocv_chgin;
	pmic_rdy = _hw_ocv_35_pon_rdy;
	pmic_zcv = _hw_ocv_35_pon;
	pmic_in_zcv = _hw_ocv_35_plugin;
	swocv = _sw_ocv;
	zcv_from = _hw_ocv_src;
	zcv_tmp = now_temp;

	if (zcv_1st_read == false) {
		charger_zcv_1st = charger_zcv;
		pmic_rdy_1st = pmic_rdy;
		pmic_zcv_1st = pmic_zcv;
		pmic_in_zcv_1st = pmic_in_zcv;
		swocv_1st = swocv;
		zcv_from_1st = zcv_from;
		zcv_tmp_1st = zcv_tmp;
		zcv_1st_read = true;
	}

	gauge_dev->fg_hw_info.pmic_zcv = _hw_ocv_35_pon;
	gauge_dev->fg_hw_info.pmic_zcv_rdy = _hw_ocv_35_pon_rdy;
	gauge_dev->fg_hw_info.charger_zcv = _hw_ocv_chgin;
	gauge_dev->fg_hw_info.hw_zcv = _hw_ocv;

	bm_err("[%s] g_fg_is_charger_exist %d _hw_ocv_chgin_rdy %d\n",
		__func__, g_fg_is_charger_exist, _hw_ocv_chgin_rdy);
	bm_err("[%s] _hw_ocv %d _sw_ocv %d now_thr %d\n",
		__func__, _prev_hw_ocv, _sw_ocv, now_thr);
	bm_err("[%s] _hw_ocv %d _hw_ocv_src %d _prev_hw_ocv %d _prev_hw_ocv_src %d _flag_unreliable %d\n",
		__func__, _hw_ocv, _hw_ocv_src,
		_prev_hw_ocv, _prev_hw_ocv_src,
		_flag_unreliable);
	bm_debug("[%s] _hw_ocv_35_pon_rdy %d _hw_ocv_35_pon %d _hw_ocv_35_plugin %d _hw_ocv_chgin %d _sw_ocv %d now_temp %d now_thr %d\n",
		__func__, _hw_ocv_35_pon_rdy, _hw_ocv_35_pon,
		_hw_ocv_35_plugin, _hw_ocv_chgin,
		_sw_ocv, now_temp, now_thr);

	return 0;
}


int fgauge_set_coulomb_interrupt1_ht(
	struct gauge_device *gauge_dev, int car_value)
{

	unsigned int uvalue32_CAR_MSB = 0;
	signed int upperbound = 0;
	signed int upperbound_31_16 = 0, upperbound_15_00 = 0;
	int reset = 0;
	signed short m;
	unsigned int ret = 0;
	signed int value32_CAR;
	long long car = car_value;

	bm_trace("%s car=%d\n", __func__, car_value);
	if (car == 0) {
		gauge_enable_interrupt(FG_BAT1_INT_H_NO, 0);
		return 0;
	}
/*
 * HW Init
 *(1)    i2c_write (0x60, 0xC8, 0x01); // Enable VA2
 *(2)    i2c_write (0x61, 0x15, 0x00); // Enable FGADC clock for digital
 *(3)    i2c_write (0x61, 0x69, 0x28);
 * // Set current mode, auto-calibration mode and 32KHz clock source
 *(4)    i2c_write (0x61, 0x69, 0x29); // Enable FGADC
 *
 *Read HW Raw Data
 *(1)    Set READ command
 */
	if (reset == 0) {
		ret = pmic_config_interface(
				MT6357_FGADC_CON1, 0x0001, 0x1F0F, 0x0);
	} else {
		ret = pmic_config_interface(
				MT6357_FGADC_CON1, 0x1F05, 0xFF0F, 0x0);
		bm_err("[%s] reset fgadc 0x1F05\n", __func__);
	}

	/*(2)    Keep i2c read when status = 1 (0x06) */
	m = 0;
	while (fg_get_data_ready_status() == 0) {
		m++;
		if (m > 1000) {
			bm_err(
				"[%s] fg_get_data_ready_status timeout 1 !",
				__func__);
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
		"[%s] FG_CAR = 0x%x:%d   uvalue32_CAR_MSB:0x%x 0x%x 0x%x\r\n",
		__func__, value32_CAR,
		value32_CAR,
		uvalue32_CAR_MSB,
		(pmic_get_register_value(PMIC_FG_CAR_15_00)),
		(pmic_get_register_value(PMIC_FG_CAR_31_16)));

	/* recovery */
	ret = pmic_config_interface(MT6357_FGADC_CON1, 0x0008, 0x000F, 0x0);
	m = 0;
	while (fg_get_data_ready_status() != 0) {
		m++;
		if (m > 1000) {
			bm_err(
				"[%s] fg_get_data_ready_status timeout 2 !",
				__func__);
			break;
		}
	}
	ret = pmic_config_interface(MT6357_FGADC_CON1, 0x0000, 0x000F, 0x0);
	/* recovery done */


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

	upperbound = value32_CAR >> CAR_TO_REG_SHIFT;

	bm_trace(
		"[%s] upper = 0x%x:%d diff_car=0x%llx:%lld\r\n",
		__func__, upperbound, upperbound, car, car);

	upperbound = upperbound + car;

	upperbound_31_16 = (upperbound & 0xffff0000) >> 16;
	upperbound_15_00 = (upperbound & 0xffff);


	bm_trace(
		"[%s]final upper = 0x%x:%d car=0x%llx:%lld\r\n",
		__func__, upperbound, upperbound, car, car);

	bm_trace(
		"[%s]final upper 0x%x 0x%x 0x%x car=0x%llx\n",
		__func__, upperbound, upperbound_31_16, upperbound_15_00, car);

	gauge_enable_interrupt(FG_BAT1_INT_H_NO, 0);

	pmic_set_register_value(PMIC_FG_BAT0_HTH_15_00, upperbound_15_00);
	pmic_set_register_value(PMIC_FG_BAT0_HTH_31_16, upperbound_31_16);
	mdelay(1);

	gauge_enable_interrupt(FG_BAT1_INT_H_NO, 1);

	bm_debug(
		"[%s] high:0x%x 0x%x car_value:%d car:%d\r\n",
		__func__, pmic_get_register_value(PMIC_FG_BAT0_HTH_15_00),
		pmic_get_register_value(PMIC_FG_BAT0_HTH_31_16),
		car_value, value32_CAR);

	return 0;

}

int fgauge_set_coulomb_interrupt1_lt(
	struct gauge_device *gauge_dev, int car_value)
{
	unsigned int uvalue32_CAR_MSB = 0;
	signed int lowbound = 0;
	signed int lowbound_31_16 = 0, lowbound_15_00 = 0;
	int reset = 0;
	signed short m;
	unsigned int ret = 0;
	signed int value32_CAR;
	long long car = car_value;

	bm_trace("%s car=%d\n", __func__, car_value);
	if (car == 0) {
		gauge_enable_interrupt(FG_BAT1_INT_L_NO, 0);
		return 0;
	}
/*
 * HW Init
 *(1)    i2c_write (0x60, 0xC8, 0x01); // Enable VA2
 *(2)    i2c_write (0x61, 0x15, 0x00); // Enable FGADC clock for digital
 *(3)    i2c_write (0x61, 0x69, 0x28);
 *  //Set current mode, auto-calibration mode and 32KHz clock source
 *(4)    i2c_write (0x61, 0x69, 0x29); // Enable FGADC
 *
 *Read HW Raw Data
 *(1)    Set READ command
 */
	if (reset == 0) {
		ret =
			pmic_config_interface(
				MT6357_FGADC_CON1, 0x0001, 0x1F0F, 0x0);
	} else {
		ret =
			pmic_config_interface(
				MT6357_FGADC_CON1, 0x1F05, 0xFF0F, 0x0);
		bm_err("[%s] reset fgadc 0x1F05\n", __func__);
	}

	/*(2)    Keep i2c read when status = 1 (0x06) */
	m = 0;
	while (fg_get_data_ready_status() == 0) {
		m++;
		if (m > 1000) {
			bm_err(
				"[%s] fg_get_data_ready_status timeout 1 !",
				__func__);
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
		"[%s] FG_CAR = 0x%x:%d   uvalue32_CAR_MSB:0x%x 0x%x 0x%x\r\n",
		__func__, value32_CAR, value32_CAR, uvalue32_CAR_MSB,
		(pmic_get_register_value(PMIC_FG_CAR_15_00)),
		(pmic_get_register_value(PMIC_FG_CAR_31_16)));

	/* recovery */
	ret = pmic_config_interface(MT6357_FGADC_CON1, 0x0008, 0x000F, 0x0);
	m = 0;
	while (fg_get_data_ready_status() != 0) {
		m++;
		if (m > 1000) {
			bm_err(
				"[%s] fg_get_data_ready_status timeout 2 !",
				__func__);
			break;
		}
	}
	ret = pmic_config_interface(MT6357_FGADC_CON1, 0x0000, 0x000F, 0x0);
	/* recovery done */


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

	lowbound = value32_CAR >> CAR_TO_REG_SHIFT;

	bm_trace(
		"[%s]low=0x%x:%d  diff_car=0x%llx:%lld\r\n",
		__func__, lowbound, lowbound, car, car);

	lowbound = lowbound - car;

	lowbound_31_16 = (lowbound & 0xffff0000) >> 16;
	lowbound_15_00 = (lowbound & 0xffff);

	bm_trace(
		"[%s]final low=0x%x:%d  car=0x%llx:%lld\r\n",
		__func__, lowbound, lowbound, car, car);

	bm_trace(
		"[%s]final low 0x%x 0x%x 0x%x car=0x%llx\n",
		__func__, lowbound, lowbound_31_16, lowbound_15_00, car);

	gauge_enable_interrupt(FG_BAT1_INT_L_NO, 0);
	pmic_set_register_value(PMIC_FG_BAT0_LTH_15_00, lowbound_15_00);
	pmic_set_register_value(PMIC_FG_BAT0_LTH_31_16, lowbound_31_16);
	mdelay(1);

	gauge_enable_interrupt(FG_BAT1_INT_L_NO, 1);

	bm_debug(
		"[%s] low:0x%x 0x%x car_value:%d car:%d\r\n",
		__func__, pmic_get_register_value(PMIC_FG_BAT0_LTH_15_00),
		pmic_get_register_value(PMIC_FG_BAT0_LTH_31_16),
		car_value, value32_CAR);

	return 0;
}

static int fgauge_read_boot_battery_plug_out_status(
	struct gauge_device *gauge_dev, int *is_plugout, int *plutout_time)
{
	*is_plugout = is_bat_plugout;
	*plutout_time = bat_plug_out_time;
	bm_err(
		"[read_boot_battery_plug_out_status] rtc_invalid %d plugout %d bat_plug_out_time %d sp3:0x%x pl:%d %d\n",
		rtc_invalid, is_bat_plugout,
		bat_plug_out_time, gspare3_reg,
		moniter_plchg_bit, pl_charging_status);

	return 0;
}

static int fgauge_get_ptim_current(
	struct gauge_device *gauge_dev, int *ptim_current, bool *is_charging)
{
		unsigned short uvalue16 = 0;
		signed int dvalue = 0;
		/*int m = 0;*/
		unsigned long long Temp_Value = 0;
		/*unsigned int ret = 0;*/

		uvalue16 = pmic_get_register_value(PMIC_FG_R_CURR);
		bm_trace("[%s] : FG_CURRENT = %x\r\n", __func__,
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
		do_div(Temp_Value, 100000);
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

			dvalue =
				(dvalue * 100) /
				gauge_dev->fg_cust_data->r_fg_value;

			bm_trace("[fgauge_read_IM_current] new current=%d\n",
			dvalue);
		}

		bm_trace("[fgauge_read_IM_current] ori current=%d\n", dvalue);

		dvalue =
			((dvalue * gauge_dev->fg_cust_data->car_tune_value)
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
	unsigned long long Temp_Value = 0;

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
	do_div(Temp_Value, 100000);
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

	if (is_power_path_supported()) {
		adc_result_reg =
			pmic_get_register_value(
				PMIC_AUXADC_ADC_OUT_FGADC_SWCHR);
		adc_result = REG_to_MV_value(adc_result_reg);
		bm_debug("[oam] %s ISENSE (swchr) : adc_result_reg=%d, adc_result=%d\n",
			 __func__, adc_result_reg, adc_result);
	} else {
		adc_result_reg =
			pmic_get_register_value(PMIC_AUXADC_ADC_OUT_FGADC_PCHR);
		adc_result = REG_to_MV_value(adc_result_reg);
		bm_debug("[oam] %s BATSNS  (pchr) : adc_result_reg=%d, adc_result=%d\n",
			 __func__, adc_result_reg, adc_result);
	}
	adc_result += g_hw_ocv_tune_value;
	*zcv = adc_result;
	return 0;
}

static int fgauge_is_gauge_initialized(
	struct gauge_device *gauge_dev, int *init)
{
	*init = 0;

	return 0;
}

static int fgauge_set_gauge_initialized(
	struct gauge_device *gauge_dev, int init)
{
	return -ENOTSUPP;
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

	pmic_set_register_value(
		PMIC_AUXADC_NAG_C_DLTV_TH_26_16,
		NAG_C_DLTV_Threashold_26_16);
	pmic_set_register_value(
		PMIC_AUXADC_NAG_C_DLTV_TH_15_0,
		NAG_C_DLTV_Threashold_15_0);

	pmic_set_register_value(PMIC_AUXADC_NAG_PRD, _prd);
	if (is_power_path_supported()) {
		pmic_set_register_value(
			PMIC_AUXADC_NAG_VBAT1_SEL, 1);	/* use Isense */
	} else {
		pmic_set_register_value(
			PMIC_AUXADC_NAG_VBAT1_SEL, 0);	/* use Batsns */
	}

	bm_debug("[fg_bat_nafg][%s] time[%d] zcv[%d:%d] thr[%d:%d] 26_16[0x%x] 15_00[0x%x] %d\n",
		__func__, _prd, _zcv_mv,
		_zcv_reg, _thr_mv, _thr_reg,
		NAG_C_DLTV_Threashold_26_16,
		NAG_C_DLTV_Threashold_15_0,
		pmic_get_register_value(PMIC_AUXADC_NAG_VBAT1_SEL));

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
		nag_zcv_mv,
		nag_c_dltv_mv);
	return 0;
}

static int fgauge_enable_nag_interrupt(struct gauge_device *gauge_dev, int en)
{
	if (en != 0)
		en = 1;
	gauge_enable_interrupt(FG_RG_INT_EN_NAG_C_DLTV, en);
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
	*nag_cnt =
		(NAG_C_DLTV_CNT & 0xffff) +
		((NAG_C_DLTV_CNT_H & 0x3ff) << 16);
	bm_debug("[fg_bat_nafg][%s] %d [25_16 %d 15_0 %d]\n",
			__func__, *nag_cnt, NAG_C_DLTV_CNT_H, NAG_C_DLTV_CNT);

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

	bm_debug("[fg_bat_nafg][%s] mV:Reg [%d:%d]\n",
		__func__, NAG_DLTV_mV_value, NAG_DLTV_reg_value);

	return 0;
}

static int fgauge_get_nag_c_dltv(
	struct gauge_device *gauge_dev,
	int *nag_c_dltv)
{
	signed int NAG_C_DLTV_value;
	signed int NAG_C_DLTV_value_H;
	signed int NAG_C_DLTV_reg_value;
	signed int NAG_C_DLTV_mV_value;
	bool bcheckbit10;

	/*AUXADC_NAG_7*/
	NAG_C_DLTV_value = pmic_get_register_value(PMIC_AUXADC_NAG_C_DLTV_15_0);

	/*AUXADC_NAG_8*/
	NAG_C_DLTV_value_H =
	pmic_get_register_value(PMIC_AUXADC_NAG_C_DLTV_26_16);
	bcheckbit10 = NAG_C_DLTV_value_H & 0x0400;

	if (bcheckbit10 == 0)
		NAG_C_DLTV_reg_value =
		(NAG_C_DLTV_value & 0xffff) +
		((NAG_C_DLTV_value_H & 0x07ff) << 16);
	else
		NAG_C_DLTV_reg_value =
		(NAG_C_DLTV_value & 0xffff) +
		(((NAG_C_DLTV_value_H | 0xf800) & 0xffff) << 16);

	NAG_C_DLTV_mV_value = REG_to_MV_value(NAG_C_DLTV_reg_value);
	*nag_c_dltv = NAG_C_DLTV_mV_value;

	bm_debug("[fg_bat_nafg][%s] mV:Reg[%d:%d][b10:%d][26_16(0x%04x) 15_00(0x%04x)]\n",
		__func__, NAG_C_DLTV_mV_value, NAG_C_DLTV_reg_value,
		bcheckbit10, NAG_C_DLTV_value_H,
		NAG_C_DLTV_value);

	return 0;

}

static void fgauge_set_zcv_intr_internal(
	struct gauge_device *gauge_dev,
	int fg_zcv_det_time,
	int fg_zcv_car_th)
{
	int fg_zcv_car_thr_h_reg, fg_zcv_car_thr_l_reg;
	int slepp_cur_avg = gauge_dev->fg_cust_data->sleep_current_avg;
	long long fg_zcv_car_th_reg = 0;

	/* calculate n+1 mins car , 0.1mAh */
	fg_zcv_car_th = (fg_zcv_det_time + 1) * slepp_cur_avg / 60;
	fg_zcv_car_th_reg = (long long)fg_zcv_car_th;

	/* 0.1mAh * 3600 -> 0.1mAs * 100 -> 1uAs * 1000 */
	fg_zcv_car_th_reg = (fg_zcv_car_th_reg * 100 * 3600 * 1000);

	/* fg_zcv_car_th_reg request uAs, 19.646 * 1000 = 19646 */
	/* mt6357 set UNIT_FGCAR_ZCV to 19646 */

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

	pmic_set_register_value(PMIC_FG_ZCV_DET_TIME, fg_zcv_det_time);
	pmic_set_register_value(PMIC_FG_ZCV_CAR_TH_15_00,
				fg_zcv_car_thr_l_reg);
	pmic_set_register_value(PMIC_FG_ZCV_CAR_TH_31_16,
				fg_zcv_car_thr_h_reg);

	bm_debug("[FG_ZCV_INT][%s] det_time %d mv %d reg %lld 31_16 0x%x 15_00 0x%x UNIT_FGCAR_ZCV:%d\n",
		__func__, fg_zcv_det_time, fg_zcv_car_th, fg_zcv_car_th_reg,
		fg_zcv_car_thr_h_reg, fg_zcv_car_thr_l_reg,
		UNIT_FGCAR_ZCV);
}

static int fgauge_enable_zcv_interrupt(struct gauge_device *gauge_dev, int en)
{
	pmic_set_register_value(PMIC_FG_ZCV_DET_EN, en);
	gauge_enable_interrupt(FG_ZCV_NO, en);
	mdelay(3);
	return 0;
}

static int fgauge_set_zcv_interrupt_threshold(
	struct gauge_device *gauge_dev, int threshold)
{
	int fg_zcv_det_time = gauge_dev->fg_cust_data->zcv_suspend_time;
	int fg_zcv_car_th = threshold;

	fgauge_set_zcv_intr_internal(
		gauge_dev, fg_zcv_det_time, fg_zcv_car_th);

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
		i, nag_vbat_reg, nag_vbat_mv, vbat_val
		);

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
	battery_dump_nag();
	return 0;
}

void Intr_Number_to_Name(char *intr_name, int intr_no)
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
		bm_err("[%s] unknown intr %d\n", __func__, intr_no);
		break;
	}
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


int fgauge_get_hw_status(
	struct gauge_device *gauge_dev,
	struct gauge_hw_status *gauge_status,
	int intr_no)
{
	int ret, m;

	/* Set Read Latchdata */
	ret = pmic_config_interface(MT6357_FGADC_CON1, 0x0001, 0x000F, 0x0);
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

	/* recover read */
	ret = pmic_config_interface(MT6357_FGADC_CON1, 0x0008, 0x000F, 0x0);
	m = 0;
		while (fg_get_data_ready_status() != 0) {
			m++;
			if (m > 1000) {
				bm_err(
					"[read_fg_hw_info] fg_get_data_ready_status timeout 2 !\r\n");
				break;
			}
		}
	ret = pmic_config_interface(MT6357_FGADC_CON1, 0x0000, 0x000F, 0x0);

	fgauge_get_coulomb(gauge_dev, &gauge_dev->fg_hw_info.car);

	bm_debug("[read_fg_hw_info] curr_1 %d curr_2 %d car %d\n",
		gauge_dev->fg_hw_info.current_1,
		gauge_dev->fg_hw_info.current_2,
		gauge_dev->fg_hw_info.car);
	return 0;
}

static signed int fgauge_get_AUXADC_current_rawdata(unsigned short *uvalue16)
{
	int m;
	int ret;
	/* (1)    Set READ command */
	ret = pmic_config_interface(MT6357_FGADC_CON1, 0x0001, 0x000F, 0x0);

	/*(2)     Keep i2c read when status = 1 (0x06) */
	m = 0;
	while (fg_get_data_ready_status() == 0) {
		m++;
		if (m > 1000) {
			bm_err(
			"[%s] fg_get_data_ready_status timeout 1!\n",
			__func__);
			break;
		}
	}

	/* (3)    Read FG_CURRENT_OUT[15:08] */
	/* (4)    Read FG_CURRENT_OUT[07:00] */
	*uvalue16 = pmic_get_register_value(PMIC_FG_CURRENT_OUT);

	/* (5)    (Read other data) */
	/* (6)    Clear status to 0 */
	ret = pmic_config_interface(MT6357_FGADC_CON1, 0x0008, 0x000F, 0x0);

	/* (7)    Keep i2c read when status = 0 (0x08) */
	m = 0;
	while (fg_get_data_ready_status() != 0) {
		m++;
		if (m > 1000) {
			bm_err(
			"[%s] fg_get_data_ready_status timeout 2!\n",
			__func__);
			break;
		}
	}

	/*(8)    Recover original settings */
	ret = pmic_config_interface(MT6357_FGADC_CON1, 0x0000, 0x000F, 0x0);

	return ret;
}

static int fgauge_enable_car_tune_value_calibration(
	struct gauge_device *gauge_dev,
	int meta_input_cali_current, int *car_tune_value)
{
	int cali_car_tune;
	long long sum_all = 0;
	unsigned long long temp_sum = 0;
	int	avg_cnt = 0;
	int i;
	unsigned short uvalue16;
	unsigned int uvalue32;
	signed int dvalue = 0;
	long long Temp_Value1 = 0;
	unsigned long long Temp_Value2 = 0;
	long long current_from_ADC = 0;

	if (meta_input_cali_current != 0) {
		for (i = 0; i < CALI_CAR_TUNE_AVG_NUM; i++) {
			if (!fgauge_get_AUXADC_current_rawdata(&uvalue16)) {
				uvalue32 = (unsigned int) uvalue16;
				if (uvalue32 <= 0x8000) {
					Temp_Value1 = (long long)uvalue32;
					bm_err("[111]uvalue16 %d uvalue32 %d Temp_Value1 %lld\n",
						uvalue16,
						uvalue32,
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
				/*****************/
				bm_err("[333]uvalue16 %d uvalue32 %d Temp_Value1 %lld sum_all %lld\n",
						uvalue16, uvalue32,
						Temp_Value1, sum_all);
				/*****************/
			}
			mdelay(30);
		}
		/*calculate the real world data    */
		/*current_from_ADC = sum_all / avg_cnt;*/
		temp_sum = sum_all;
		bm_err("[444]sum_all %lld temp_sum %lld avg_cnt %d current_from_ADC %lld\n",
			sum_all, temp_sum, avg_cnt, current_from_ADC);

		if (avg_cnt != 0)
			do_div(temp_sum, avg_cnt);
		current_from_ADC = temp_sum;

		bm_err("[555]sum_all %lld temp_sum %lld avg_cnt %d current_from_ADC %lld\n",
			sum_all, temp_sum, avg_cnt, current_from_ADC);

		Temp_Value2 = current_from_ADC * UNIT_FGCURRENT;

		bm_err("[555]Temp_Value2 %lld current_from_ADC %lld UNIT_FGCURRENT %d\n",
			Temp_Value2, current_from_ADC, UNIT_FGCURRENT);

		/* Move 100 from denominator to cali_car_tune's numerator */
		/*do_div(Temp_Value2, 1000000);*/
		do_div(Temp_Value2, 10000);

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

		if (dvalue != 0) {
			cali_car_tune =
				meta_input_cali_current *
				1000 * 100 / dvalue;

			bm_err("[777]dvalue %d fg_cust_data.r_fg_value %d cali_car_tune %d\n",
				dvalue,
				gauge_dev->fg_cust_data->r_fg_value,
				cali_car_tune);
			*car_tune_value = cali_car_tune;

			bm_err(
				"[fgauge_meta_cali_car_tune_value][%d] meta:%d, adc:%lld, UNI_FGCUR:%d, r_fg_value:%d\n",
				cali_car_tune, meta_input_cali_current,
				current_from_ADC, UNIT_FGCURRENT,
				gauge_dev->fg_cust_data->r_fg_value);
		}

		return 0;
	}

	return 0;
}

static int fgauge_set_rtc_ui_soc(struct gauge_device *gauge_dev, int rtc_ui_soc)
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

static int fgauge_get_rtc_ui_soc(struct gauge_device *gauge_dev, int *ui_soc)
{
	int spare3_reg = get_rtc_spare_fg_value();
	int rtc_ui_soc;

	rtc_ui_soc = (spare3_reg & 0x7f);

	*ui_soc = rtc_ui_soc;
	bm_notice("[%s] rtc_ui_soc %d spare3_reg 0x%x\n",
		__func__, rtc_ui_soc, spare3_reg);

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
	int spare3_reg, after_rst_spare3_reg;

	/* read spare3 */
	spare3_reg = get_rtc_spare_fg_value();

	/* set spare3 0x7f */
	set_rtc_spare_fg_value(spare3_reg | 0x80);

	/* read spare3 again */
	after_rst_spare3_reg = get_rtc_spare_fg_value();

	bm_err("[fgauge_read_RTC_boot_status] spare3 0x%x 0x%x\n",
		spare3_reg, after_rst_spare3_reg);

	return 0;

}

static void fgauge_dump_type0(struct seq_file *m)
{
	if (m != NULL) {
		seq_puts(m, "fgauge dump\n");
		seq_printf(m, "AUXADC_ADC_RDY_LBAT2 :%x\n",
			pmic_get_register_value(PMIC_AUXADC_ADC_RDY_LBAT));
		seq_printf(m, "AUXADC_ADC_OUT_LBAT2  :%x\n",
			pmic_get_register_value(PMIC_AUXADC_ADC_OUT_LBAT));

		seq_printf(m, "AUXADC_LBAT2_DEBT_MIN  :%x\n",
			pmic_get_register_value(PMIC_AUXADC_LBAT_DEBT_MIN));
		seq_printf(m, "AUXADC_LBAT2_DEBT_MAX  :%x\n",
			pmic_get_register_value(PMIC_AUXADC_LBAT_DEBT_MAX));

		seq_printf(m, "AUXADC_LBAT2_DET_PRD_15_0  :%x\n",
			pmic_get_register_value(PMIC_AUXADC_LBAT_DET_PRD_15_0));
		seq_printf(m, "AUXADC_LBAT2_DET_PRD_19_16   :%x\n",
			pmic_get_register_value(
			PMIC_AUXADC_LBAT_DET_PRD_19_16));

		seq_printf(m, "AUXADC_LBAT2_MAX_IRQ_B  :%x\n",
			pmic_get_register_value(PMIC_AUXADC_LBAT_MAX_IRQ_B));
		seq_printf(m, "AUXADC_LBAT2_EN_MAX   :%x\n",
			pmic_get_register_value(PMIC_AUXADC_LBAT_EN_MAX));

		seq_printf(m, "AUXADC_LBAT2_IRQ_EN_MAX  :%x\n",
			pmic_get_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MAX));
		seq_printf(m, "AUXADC_LBAT2_VOLT_MAX   :%x\n",
			pmic_get_register_value(PMIC_AUXADC_LBAT_VOLT_MAX));

		seq_printf(m, "AUXADC_LBAT2_MIN_IRQ_B  :%x\n",
			pmic_get_register_value(PMIC_AUXADC_LBAT_MIN_IRQ_B));
		seq_printf(m, "AUXADC_LBAT2_EN_MIN   :%x\n",
			pmic_get_register_value(PMIC_AUXADC_LBAT_EN_MIN));

		seq_printf(m, "AUXADC_LBAT2_IRQ_EN_MIN  :%x\n",
			pmic_get_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MIN));
		seq_printf(m, "AUXADC_LBAT2_VOLT_MIN   :%x\n",
			pmic_get_register_value(PMIC_AUXADC_LBAT_VOLT_MIN));

		seq_printf(m, "AUXADC_LBAT2_DEBOUNCE_COUNT_MAX  :%x\n",
			pmic_get_register_value(
				PMIC_AUXADC_LBAT_DEBOUNCE_COUNT_MAX));
		seq_printf(m, "AUXADC_LBAT2_DEBOUNCE_COUNT_MIN   :%x\n",
			pmic_get_register_value(
				PMIC_AUXADC_LBAT_DEBOUNCE_COUNT_MIN));

		seq_printf(m, "RG_INT_EN_BAT2_H  :%x\n",
			pmic_get_register_value(PMIC_RG_INT_EN_BAT_H));
		seq_printf(m, "RG_INT_EN_BAT2_L   :%x\n",
			pmic_get_register_value(PMIC_RG_INT_EN_BAT_L));

		seq_printf(m, "RG_INT_STATUS_BAT2_H  :%x\n",
			pmic_get_register_value(PMIC_RG_INT_STATUS_BAT_H));
		seq_printf(m, "RG_INT_STATUS_BAT2_L   :%x\n",
			pmic_get_register_value(PMIC_RG_INT_STATUS_BAT_L));

		seq_printf(m, "AUXADC_SOURCE_LBAT2_SEL   :%x\n",
			pmic_get_register_value(PMIC_AUXADC_SOURCE_LBAT2_SEL));

		seq_printf(m,
			"1st chr_zcv:%d pmic_zcv:%d %d pmic_in_zcv:%d swocv:%d zcv_from:%d tmp:%d\n",
			charger_zcv_1st, pmic_rdy_1st, pmic_zcv_1st,
			pmic_in_zcv_1st, swocv_1st, zcv_from_1st,
			zcv_tmp_1st);

		seq_printf(m,
			"chr_zcv:%d pmic_zcv:%d %d pmic_in_zcv:%d swocv:%d zcv_from:%d tmp:%d\n",
			charger_zcv, pmic_rdy, pmic_zcv,
			pmic_in_zcv, swocv, zcv_from, zcv_tmp);
	}

	bm_debug(
		"1st chr_zcv:%d pmic_zcv:%d %d pmic_in_zcv:%d swocv:%d zcv_from:%d tmp:%d\n",
		charger_zcv_1st, pmic_rdy_1st, pmic_zcv_1st, pmic_in_zcv_1st,
		swocv_1st, zcv_from_1st, zcv_tmp_1st);

	bm_debug(
		"chr_zcv:%d pmic_zcv:%d %d pmic_in_zcv:%d swocv:%d zcv_from:%d tmp:%d\n",
		charger_zcv, pmic_rdy,
		pmic_zcv, pmic_in_zcv,
		swocv, zcv_from, zcv_tmp);
}

static int fgauge_dump(
	struct gauge_device *gauge_dev, struct seq_file *m, int type)
{

	if (type == 0)
		fgauge_dump_type0(m);
	else if (type == 1)
		battery_dump_nag();

	return 0;
}

static int fgauge_get_hw_version(struct gauge_device *gauge_dev)
{
	return GAUGE_HW_V1000;
}

int fgauge_notify_event(
	struct gauge_device *gauge_dev,
	enum gauge_event evt, int value)
{
	return 0;
}

static struct gauge_ops mt6357_gauge_ops = {
	.gauge_initial = fgauge_initial,
	.gauge_read_current = fgauge_read_current,
	.gauge_get_average_current = NULL,
	.gauge_get_coulomb = fgauge_get_coulomb,
	.gauge_reset_hw = fgauge_reset_hw,
	.gauge_get_hwocv = read_hw_ocv,/* check */
	.gauge_set_coulomb_interrupt1_ht = fgauge_set_coulomb_interrupt1_ht,
	.gauge_set_coulomb_interrupt1_lt = fgauge_set_coulomb_interrupt1_lt,
	.gauge_get_boot_battery_plug_out_status =
		fgauge_read_boot_battery_plug_out_status,
	.gauge_get_ptim_current = fgauge_get_ptim_current,
	.gauge_get_zcv_current = fgauge_get_zcv_current,
	.gauge_get_zcv = fgauge_get_zcv,
	.gauge_is_gauge_initialized = fgauge_is_gauge_initialized,
	.gauge_set_gauge_initialized = fgauge_set_gauge_initialized,
	.gauge_set_battery_cycle_interrupt = NULL,
	.gauge_reset_shutdown_time = NULL,
	.gauge_reset_ncar = NULL,
	.gauge_set_nag_zcv = fgauge_set_nag_zcv,
	.gauge_set_nag_c_dltv = fgauge_set_nag_c_dltv,
	.gauge_enable_nag_interrupt = fgauge_enable_nag_interrupt,
	.gauge_get_nag_cnt = fgauge_get_nag_cnt,
	.gauge_get_nag_dltv = fgauge_get_nag_dltv,
	.gauge_get_nag_c_dltv = fgauge_get_nag_c_dltv,
	.gauge_get_nag_vbat = fgauge_get_nag_vbat,
	.gauge_enable_zcv_interrupt = fgauge_enable_zcv_interrupt,
	.gauge_set_zcv_interrupt_threshold = fgauge_set_zcv_interrupt_threshold,
	.gauge_get_nag_vbat = fgauge_get_nag_vbat,
	.gauge_enable_battery_tmp_lt_interrupt = NULL,
	.gauge_enable_battery_tmp_ht_interrupt = NULL,
	.gauge_get_time = NULL,
	.gauge_set_time_interrupt = NULL,
	.gauge_get_hw_status = fgauge_get_hw_status,
	.gauge_enable_bat_plugout_interrupt = NULL,
	.gauge_enable_iavg_interrupt = NULL,
	.gauge_enable_vbat_low_interrupt = NULL,
	.gauge_enable_vbat_high_interrupt = NULL,
	.gauge_set_vbat_low_threshold = NULL,
	.gauge_set_vbat_high_threshold = NULL,
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
	.gauge_notify_event = fgauge_notify_event,
};

static int mt6357_parse_dt(struct mt6357_gauge *info, struct device *dev)
{
	struct device_node *np = dev->of_node;

	bm_debug("%s: starts\n", __func__);

	if (!np) {
		bm_debug("%s: no device node\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_string(np, "gauge_name",
		&info->gauge_dev_name) < 0) {
		bm_debug("%s: no charger name\n", __func__);
		info->gauge_dev_name = "gauge";
	}

	return 0;
}


static int mt6357_gauge_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mt6357_gauge *info;

	bm_debug("%s: starts\n", __func__);

	info = devm_kzalloc(
		&pdev->dev,
		sizeof(struct mt6357_gauge),
		GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	mt6357_parse_dt(info, &pdev->dev);
	platform_set_drvdata(pdev, info);

	/* Register charger device */
	info->gauge_dev = gauge_device_register(info->gauge_dev_name,
		&pdev->dev, info, &mt6357_gauge_ops, &info->gauge_prop);
	if (IS_ERR_OR_NULL(info->gauge_dev)) {
		ret = PTR_ERR(info->gauge_dev);
		goto err_register_gauge_dev;
	}

	return 0;
err_register_gauge_dev:
	devm_kfree(&pdev->dev, info);
	return ret;

}

static int mt6357_gauge_remove(struct platform_device *pdev)
{
	struct mt6357_gauge *mt = platform_get_drvdata(pdev);

	if (mt)
		devm_kfree(&pdev->dev, mt);
	return 0;
}

static void mt6357_gauge_shutdown(struct platform_device *dev)
{
}


static const struct of_device_id mt6357_gauge_of_match[] = {
	{.compatible = "mediatek,mt6357_gauge",},
	{},
};

MODULE_DEVICE_TABLE(of, mt6357_gauge_of_match);

static struct platform_driver mt6357_gauge_driver = {
	.probe = mt6357_gauge_probe,
	.remove = mt6357_gauge_remove,
	.shutdown = mt6357_gauge_shutdown,
	.driver = {
		   .name = "mt6357_gauge",
		   .of_match_table = mt6357_gauge_of_match,
		   },
};

static int __init mt6357_gauge_init(void)
{
	return platform_driver_register(&mt6357_gauge_driver);
}
device_initcall(mt6357_gauge_init);

static void __exit mt6357_gauge_exit(void)
{
	platform_driver_unregister(&mt6357_gauge_driver);
}
module_exit(mt6357_gauge_exit);

MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK Gauge Device Driver");
MODULE_LICENSE("GPL");
