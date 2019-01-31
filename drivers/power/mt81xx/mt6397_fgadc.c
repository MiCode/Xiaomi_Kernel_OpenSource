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
#include <linux/errno.h>

#include <mt-plat/upmu_common.h>

#include "mt_battery_custom_data.h"
#include "mt_battery_meter_hal.h"

#define VOLTAGE_FULL_RANGE 1200
#define ADC_PRECISE 1024	/* 10 bits */
#define UNIT_FGCURRENT (158122) /* 158.122 uA */

static s32 g_hw_ocv_tune_value = 8; /* hwocv chip calibration value */
static bool g_fg_is_charging;

static struct mt_battery_meter_custom_data *bat_meter_data;

s32 use_chip_trim_value(s32 not_trim_val)
{
	pr_debug("skip trim value for mt6397.\n");
	return not_trim_val;
}

int get_hw_ocv(void)
{
#if defined(CONFIG_POWER_EXT)
	return 4001 + g_hw_ocv_tune_value;
#else
	s32 adc_result_reg = 0;
	s32 adc_result = 0;
	s32 r_val_temp = 4;

#if defined(CONFIG_SWCHR_POWER_PATH)
	adc_result_reg = upmu_get_rg_adc_out_wakeup_swchr_trim();
	adc_result = (adc_result_reg * r_val_temp * VOLTAGE_FULL_RANGE) /
		     ADC_PRECISE;
	pr_debug(
		"[oam] get_hw_ocv (swchr) : adc_result_reg=%d, adc_result=%d\n",
		adc_result_reg, adc_result);
#else
	adc_result_reg = upmu_get_rg_adc_out_wakeup_pchr();
	adc_result = (adc_result_reg * r_val_temp * VOLTAGE_FULL_RANGE) /
		ADC_PRECISE;
	pr_debug("[oam] get_hw_ocv (pchr) : adc_result_reg=%d, adc_result=%d\n",
		adc_result_reg, adc_result);
#endif

	adc_result += g_hw_ocv_tune_value;
	return adc_result;
#endif
}

/* ============================================================// */

static u32 fg_get_data_ready_status(void)
{
	u32 ret = 0;
	u32 temp_val = 0;

	ret = pmic_read_interface(FGADC_CON0, &temp_val, 0xFFFF, 0x0);

	pr_debug("[fg_get_data_ready_status] Reg[0x%x]=0x%x\r\n", FGADC_CON0,
		temp_val);

	temp_val = (temp_val &
	(PMIC_FG_LATCHDATA_ST_MASK << PMIC_FG_LATCHDATA_ST_SHIFT)) >>
		   PMIC_FG_LATCHDATA_ST_SHIFT;

	return temp_val;
}

static s32 fgauge_read_current(void *data);
static s32 fgauge_initialization(void *data)
{
	u32 ret = 0;
	s32 current_temp = 0;
	int m = 0;

	bat_meter_data = (struct mt_battery_meter_custom_data *)data;

	/* 1. HW initialization */
	/* FGADC clock is 32768Hz from RTC */
	/* Enable FGADC in current mode at 32768Hz with auto-calibration */

	/* (1)    Enable VA2 */
	/* (2)    Enable FGADC clock for digital */
	upmu_set_rg_fgadc_ana_ck_pdn(0);
	upmu_set_rg_fgadc_ck_pdn(0);
	/* (3)    Set current mode, auto-calibration mode and 32KHz clock source
	 */
	ret = pmic_config_interface(FGADC_CON0, 0x0028, 0x00FF, 0x0);
	/* (4)    Enable FGADC */
	ret = pmic_config_interface(FGADC_CON0, 0x0029, 0x00FF, 0x0);

	/* reset HW FG */
	ret = pmic_config_interface(FGADC_CON0, 0x7100, 0xFF00, 0x0);
	pr_debug("******** [fgauge_initialization] reset HW FG!\n");

	/* make sure init finish */
	m = 0;
	while (current_temp == 0) {
		fgauge_read_current(&current_temp);
		m++;
		if (m > 1000) {
			pr_debug("[fgauge_initialization] timeout!\r\n");
			break;
		}
	}

	pr_debug("******** [fgauge_initialization] Done!\n");

	return 0;
}

static s32 fgauge_read_current(void *data)
{
	u16 uvalue16 = 0;
	s32 dvalue = 0;
	int m = 0;
	s64 Temp_Value = 0;
	s32 Current_Compensate_Value = 0;
	u32 ret = 0;
	s32 r_fg_board_slope = bat_meter_data->r_fg_board_slope;
	s32 r_fg_board_base = bat_meter_data->r_fg_board_base;
	s32 car_tune_value = bat_meter_data->car_tune_value;
	s32 r_fg_value = bat_meter_data->r_fg_value;

	/* HW Init
	 * (1)    i2c_write (0x60, 0xC8, 0x01); // Enable VA2
	 * (2)    i2c_write (0x61, 0x15, 0x00); // Enable FGADC clock for
	 * digital
	 * (3)    i2c_write (0x61, 0x69, 0x28); // Set current mode,
	 * auto-calibration mode and 32KHz clock source
	 * (4)    i2c_write (0x61, 0x69, 0x29); // Enable FGADC
	 */

	/* Read HW Raw Data */
	/* (1)    Set READ command */
	ret = pmic_config_interface(FGADC_CON0, 0x0200, 0xFF00, 0x0);
	/* (2)     Keep i2c read when status = 1 (0x06) */
	m = 0;
	while (fg_get_data_ready_status() == 0) {
		m++;
		if (m > 1000) {
			pr_debug("[fgauge_read_current] fg_get_data_ready_status timeout 1 !\r\n");
			break;
		}
	}
	/* (3)    Read FG_CURRENT_OUT[15:08] */
	/* (4)    Read FG_CURRENT_OUT[07:00] */
	uvalue16 = upmu_get_fg_current_out();
	pr_debug("[fgauge_read_current] : FG_CURRENT = %x\r\n", uvalue16);
	/* (5)    (Read other data) */
	/* (6)    Clear status to 0 */
	ret = pmic_config_interface(FGADC_CON0, 0x0800, 0xFF00, 0x0);
	/* (7)    Keep i2c read when status = 0 (0x08) */
	/* while ( fg_get_sw_clear_status() != 0 ) */
	m = 0;
	while (fg_get_data_ready_status() != 0) {
		m++;
		if (m > 1000) {
			pr_debug("[fgauge_read_current] fg_get_data_ready_status timeout 2 !\r\n");
			break;
		}
	}
	/* (8)    Recover original settings */
	ret = pmic_config_interface(FGADC_CON0, 0x0000, 0xFF00, 0x0);

	/* calculate the real world data */
	dvalue = (u32)uvalue16;
	if (dvalue == 0) {
		Temp_Value = (s64)dvalue;
		g_fg_is_charging = false;
	} else if (dvalue > 32767) {
		/* > 0x8000 */
		Temp_Value = (s64)(dvalue - 65535);
		Temp_Value = Temp_Value - (Temp_Value * 2);
		g_fg_is_charging = false;
	} else {
		Temp_Value = (s64)dvalue;
		g_fg_is_charging = true;
	}

	Temp_Value = Temp_Value * UNIT_FGCURRENT;
	do_div(Temp_Value, 100000);
	dvalue = (u32)Temp_Value;

	if (g_fg_is_charging == true)
		pr_debug("[fgauge_read_current] current(charging) = %d mA\r\n",
			dvalue);
	else
		pr_debug("[fgauge_read_current] current(discharging) = %d mA\r\n",
			dvalue);

	/* Auto adjust value */
	if (r_fg_value != 20) {
		pr_debug("[fgauge_read_current] Auto adjust value due to the Rfg is %d\n Ori current=%d, ",
			r_fg_value, dvalue);

		dvalue = (dvalue * 20) / r_fg_value;

		pr_debug("[fgauge_read_current] new current=%d\n", dvalue);
	}
	/* K current */
	if (r_fg_board_slope != r_fg_board_base)
		dvalue = ((dvalue * r_fg_board_base) + (r_fg_board_slope / 2)) /
			r_fg_board_slope;

	/* current compensate */
	if (g_fg_is_charging == true)
		dvalue = dvalue + Current_Compensate_Value;
	else
		dvalue = dvalue - Current_Compensate_Value;

	pr_debug("[fgauge_read_current] ori current=%d\n", dvalue);

	dvalue = ((dvalue * car_tune_value) / 100);

	dvalue = use_chip_trim_value(dvalue);

	pr_debug("[fgauge_read_current] final current=%d (ratio=%d)\n", dvalue,
		car_tune_value);

	*(s32 *)(data) = dvalue;

	return 0;
}

static s32 fgauge_read_current_sign(void *data)
{
	*(bool *)(data) = g_fg_is_charging;

	return 0;
}

static s32 fgauge_read_columb_internal(void *data, int reset)
{
	u32 uvalue32_CAR = 0;
	u32 uvalue32_CAR_MSB = 0;
	s32 dvalue_CAR = 0;
	int m = 0;
	int Temp_Value = 0;
	u32 ret = 0;
	s32 r_fg_value = bat_meter_data->r_fg_value;
	s32 car_tune_value = bat_meter_data->car_tune_value;

	/* HW Init
	 * (1)    i2c_write (0x60, 0xC8, 0x01); // Enable VA2
	 * (2)    i2c_write (0x61, 0x15, 0x00); // Enable FGADC clock for
	 * digital
	 * (3)    i2c_write (0x61, 0x69, 0x28); // Set current mode,
	 * auto-calibration mode and 32KHz clock source
	 * (4)    i2c_write (0x61, 0x69, 0x29); // Enable FGADC
	 */

	/* Read HW Raw Data */
	/* (1)    Set READ command */
	if (reset == 0)
		ret = pmic_config_interface(FGADC_CON0, 0x0200, 0xFF00, 0x0);
	else
		ret = pmic_config_interface(FGADC_CON0, 0x7300, 0xFF00, 0x0);

	/* (2)    Keep i2c read when status = 1 (0x06) */
	m = 0;
	while (fg_get_data_ready_status() == 0) {
		m++;
		if (m > 1000) {
			pr_debug("[fgauge_read_columb_internal] fg_get_data_ready_status timeout 1 !\r\n");
			break;
		}
	}
	/* (3)    Read FG_CURRENT_OUT[28:14] */
	/* (4)    Read FG_CURRENT_OUT[35] */
	uvalue32_CAR = (upmu_get_fg_car_15_00()) >> 14;
	uvalue32_CAR |= ((upmu_get_fg_car_31_16()) & 0x3FFF) << 2;

	uvalue32_CAR_MSB = (upmu_get_fg_car_35_32() & 0x0F) >> 3;

	pr_debug("[fgauge_read_columb_internal] FG_CAR = 0x%x\r\n",
		 uvalue32_CAR);
	pr_debug("[fgauge_read_columb_internal] uvalue32_CAR_MSB = 0x%x\r\n",
		 uvalue32_CAR_MSB);

	/* (5)    (Read other data) */
	/* (6)    Clear status to 0 */
	ret = pmic_config_interface(FGADC_CON0, 0x0800, 0xFF00, 0x0);
	/* (7)    Keep i2c read when status = 0 (0x08) */
	/* while ( fg_get_sw_clear_status() != 0 ) */
	m = 0;
	while (fg_get_data_ready_status() != 0) {
		m++;
		if (m > 1000) {
			pr_debug("[fgauge_read_columb_internal] fg_get_data_ready_status timeout 2 !\r\n");
			break;
		}
	}
	/* (8)    Recover original settings */
	ret = pmic_config_interface(FGADC_CON0, 0x0000, 0xFF00, 0x0);

	/* calculate the real world data */
	dvalue_CAR = (s32)uvalue32_CAR;

	if (uvalue32_CAR == 0) {
		Temp_Value = 0;
	} else if (uvalue32_CAR == 65535) {
		/* 0xffff */
		Temp_Value = 0;
	} else if (uvalue32_CAR_MSB == 0x1) {
		/* dis-charging */
		Temp_Value = dvalue_CAR - 65535; /* keep negative value */
	} else {
		/* charging */
		Temp_Value = (int)dvalue_CAR;
	}
	/* [28:14]'s LSB=359.86 uAh */
	Temp_Value = (((Temp_Value * 35986) / 10) + (5)) / 10;
	/* mAh */
	dvalue_CAR = Temp_Value / 1000;

	pr_debug("[fgauge_read_columb_internal] dvalue_CAR = %d\r\n",
		dvalue_CAR);

	/* Auto adjust value */
	if (r_fg_value != 20) {
		pr_debug(
			"[fgauge_read_columb_internal] Auto adjust value deu to the Rfg is %d\n Ori CAR=%d, ",
			r_fg_value, dvalue_CAR);

		dvalue_CAR = (dvalue_CAR * 20) / r_fg_value;

		pr_debug("[fgauge_read_columb_internal] new CAR=%d\n",
			dvalue_CAR);
	}

	dvalue_CAR = ((dvalue_CAR * car_tune_value) / 100);

	dvalue_CAR = use_chip_trim_value(dvalue_CAR);

	pr_debug("[fgauge_read_columb_internal] final dvalue_CAR = %d\r\n",
		dvalue_CAR);

	*(s32 *)(data) = dvalue_CAR;

	return 0;
}

static s32 fgauge_read_columb(void *data)
{
	return fgauge_read_columb_internal(data, 0);
}

static s32 fgauge_hw_reset(void *data)
{
	u32 val_car = 1;
	u32 ret = 0;

	pr_debug("[fgauge_hw_reset] : Start \r\n");

	while (val_car != 0x0) {
		ret = pmic_config_interface(FGADC_CON0, 0x7100, 0xFF00, 0x0);
		fgauge_read_columb_internal(&val_car, 1);
		pr_debug("#");
	}

	pr_debug("[fgauge_hw_reset] : End \r\n");

	return 0;
}

static s32 read_adc_v_bat_sense(void *data)
{
#if defined(CONFIG_POWER_EXT)
	*(s32 *)(data) = 4201;
#else
	*(s32 *)(data) = PMIC_IMM_GetOneChannelValue(
		bat_meter_data->vbat_channel_number, *(s32 *)(data), 1);
#endif

	return 0;
}

static s32 read_adc_v_i_sense(void *data)
{
#if defined(CONFIG_POWER_EXT)
	*(s32 *)(data) = 4202;
#else
	*(s32 *)(data) = PMIC_IMM_GetOneChannelValue(
		bat_meter_data->isense_channel_number, *(s32 *)(data), 1);
#endif

	return 0;
}

static s32 read_adc_v_bat_temp(void *data)
{
#if defined(CONFIG_POWER_EXT)
	*(s32 *)(data) = 0;
#else
	pr_debug("[read_adc_v_bat_temp] return PMIC_IMM_GetOneChannelValue(4,times,1);\n");
	*(s32 *)(data) = PMIC_IMM_GetOneChannelValue(
		bat_meter_data->vbattemp_channel_number, *(s32 *)(data), 1);
#endif

	return 0;
}

static s32 read_adc_v_charger(void *data)
{
#if defined(CONFIG_POWER_EXT)
	*(s32 *)(data) = 5001;
#else
	s32 val;

	val = PMIC_IMM_GetOneChannelValue(
		bat_meter_data->vcharger_channel_number, *(s32 *)(data), 1);
	val = val / 100;

	*(s32 *)(data) = val;
#endif

	return 0;
}

static s32 read_hw_ocv(void *data)
{
#if defined(CONFIG_POWER_EXT)
	*(s32 *)(data) = 3999;
#else
	*(s32 *)(data) = get_hw_ocv();
#endif
	return 0;
}

static s32 dump_register_fgadc(void *data)
{
	return 0;
}

static s32 (*const bm_func[BATTERY_METER_CMD_NUMBER])(void *data) = {
	fgauge_initialization, fgauge_read_current, fgauge_read_current_sign,
	fgauge_read_columb,    fgauge_hw_reset,     read_adc_v_bat_sense,
	read_adc_v_i_sense,    read_adc_v_bat_temp, read_adc_v_charger,
	read_hw_ocv,	   dump_register_fgadc};

s32 bm_ctrl_cmd(int cmd, void *data)
{
	s32 status;

	if ((cmd < BATTERY_METER_CMD_NUMBER) && (bm_func[cmd] != NULL))
		status = bm_func[cmd](data);
	else
		return -1;

	return status;
}
