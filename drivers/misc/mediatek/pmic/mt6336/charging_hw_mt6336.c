/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <mt-plat/charging.h>
#include <mt-plat/upmu_common.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <mt-plat/mt_boot.h>
#include <mt-plat/battery_meter.h>
#include <mach/mt_battery_meter.h>
#include <mach/mt_charging.h>
#include <mach/mt_pmic.h>
#include <mt_sleep.h>

#include "mt6336.h"


#define STATUS_OK    0
#define STATUS_UNSUPPORTED    -1
#define STATUS_FAIL -2
#define GETARRAYNUM(array) (sizeof(array)/sizeof(array[0]))

/* ============================================================ // */
/* Global variable */
/* ============================================================ // */

#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
#else
static CHARGER_TYPE g_charger_type = CHARGER_UNKNOWN;
#endif

unsigned char charging_type_det_done = KAL_TRUE;

/* mt6336 REG:0427 VCV_CON0[7:0] */
const unsigned int VBAT_CV_VTH[] = {
	2600000, 2612500, 2625000, 2637500,
	2650000, 2662500, 2675000, 2687500,
	2700000, 2712500, 2725000, 2737500,
	2750000, 2762500, 2775000, 2787500,
	2800000, 2812500, 2825000, 2837500,
	2850000, 2862500, 2875000, 2887500,
	2900000, 2912500, 2925000, 2937500,
	2950000, 2962500, 2975000, 2987500,
	3000000, 3012500, 3025000, 3037500,
	3050000, 3062500, 3075000, 3087500,
	3100000, 3112500, 3125000, 3137500,
	3150000, 3162500, 3175000, 3187500,
	3200000, 3212500, 3225000, 3237500,
	3250000, 3262500, 3275000, 3287500,
	3300000, 3312500, 3325000, 3337500,
	3350000, 3362500, 3375000, 3387500,
	3400000, 3412500, 3425000, 3437500,
	3450000, 3462500, 3475000, 3487500,
	3500000, 3512500, 3525000, 3537500,
	3550000, 3562500, 3575000, 3587500,
	3600000, 3612500, 3625000, 3637500,
	3650000, 3662500, 3675000, 3687500,
	3700000, 3712500, 3725000, 3737500,
	3750000, 3762500, 3775000, 3787500,
	3800000, 3812500, 3825000, 3837500,
	3850000, 3862500, 3875000, 3887500,
	3900000, 3912500, 3925000, 3937500,
	3950000, 3962500, 3975000, 3987500,
	4000000, 4012500, 4025000, 4037500,
	4050000, 4062500, 4075000, 4087500,
	4100000, 4112500, 4125000, 4137500,
	4150000, 4162500, 4175000, 4187500,
	4200000, 4212500, 4225000, 4237500,
	4250000, 4262500, 4275000, 4287500,
	4300000, 4312500, 4325000, 4337500,
	4350000, 4362500, 4375000, 4387500,
	4400000, 4412500, 4425000, 4437500,
	4450000, 4462500, 4475000, 4487500,
	4500000, 4512500, 4525000, 4537500,
	4550000, 4562500, 4575000, 4587500,
	4600000, 4612500, 4625000, 4637500,
	4650000, 4662500, 4675000, 4687500,
	4700000, 4712500, 4725000, 4737500,
	4750000, 4762500, 4775000, 4787500,
	4800000,
};

/* mt6336 REG:0426 ICC_CON0[6:0] */
const unsigned int CS_VTH[] = {
	50000, 55000, 60000, 65000,
	70000, 75000, 80000, 85000,
	90000, 95000, 100000, 105000,
	110000, 115000, 120000, 125000,
	130000, 135000, 140000, 145000,
	150000, 155000, 160000, 165000,
	170000, 175000, 180000, 185000,
	190000, 195000, 200000, 205000,
	210000, 215000, 220000, 225000,
	230000, 235000, 240000, 245000,
	250000, 255000, 260000, 265000,
	270000, 275000, 280000, 285000,
	290000, 295000, 300000, 305000,
	310000, 315000, 320000, 325000,
	330000, 335000, 340000, 345000,
	350000, 355000, 360000, 365000,
	370000, 375000, 380000, 385000,
	390000, 395000, 400000, 405000,
	410000, 415000, 420000, 425000,
	430000, 435000, 440000, 445000,
	450000, 455000, 460000, 465000,
	470000, 475000, 480000, 485000,
	490000, 495000, 500000,
};

/* mt6336 REG:0437 ICL_CON0[5:0]*/
const unsigned int INPUT_CS_VTH[] = {
	5000, 10000, 15000, 50000,
	55000, 60000, 65000, 70000,
	75000, 80000, 85000, 90000,
	95000, 100000, 105000, 110000,
	115000, 120000, 125000, 130000,
	135000, 140000, 145000, 150000,
	155000, 160000, 165000, 170000,
	175000, 180000, 185000, 190000,
	195000, 200000, 205000, 210000,
	215000, 220000, 225000, 230000,
	235000, 240000, 245000, 250000,
	255000, 260000, 265000, 270000,
	275000, 280000, 285000, 290000,
	295000, 300000, 305000, 310000,
	315000, 320000,
};

/* mt6335 REG:2602 CHR_CON1[7:4] */
const unsigned int VCDT_HV_VTH[] = {
#if 1
	4200000, 4250000, 4300000,
	4350000, 4400000, 4450000,
	6000000, 6500000, 7000000,
	7500000, 8500000, 9500000,
	10500000, 11500000, 12500000,
	14000000,
#else
	BATTERY_VOLT_04_200000_V, BATTERY_VOLT_04_250000_V, BATTERY_VOLT_04_300000_V,
	BATTERY_VOLT_04_350000_V, BATTERY_VOLT_04_400000_V, BATTERY_VOLT_04_450000_V,
	BATTERY_VOLT_06_000000_V, BATTERY_VOLT_06_500000_V, BATTERY_VOLT_07_000000_V,
	BATTERY_VOLT_07_500000_V, BATTERY_VOLT_08_500000_V, BATTERY_VOLT_09_500000_V,
	BATTERY_VOLT_10_500000_V, BATTERY_VOLT_11_500000_V, BATTERY_VOLT_12_500000_V,
	BATTERY_VOLT_14_000000_V,
#endif
};

/* ??? */
const unsigned int VINDPM_REG[] = {
	2600, 2700, 2800, 2900, 3000, 3100, 3200, 3300, 3400, 3500, 3600, 3700, 3800, 3900, 4000,
	4100, 4200, 4300, 4400, 4500, 4600, 4700, 4800, 4900, 5000, 5100, 5200, 5300, 5400, 5500,
	5600, 5700, 5800, 5900, 6000, 6100, 6200, 6300, 6400, 6500, 6600, 6700, 6800, 6900, 7000,
	7100, 7200, 7300, 7400, 7500, 7600, 7700, 7800, 7900, 8000, 8100, 8200, 8300, 8400, 8500,
	8600, 8700, 8800, 8900, 9000, 9100, 9200, 9300, 9400, 9500, 9600, 9700, 9800, 9900, 10000,
	10100, 10200, 10300, 10400, 10500, 10600, 10700, 10800, 10900, 11000, 11100, 11200, 11300,
	11400, 11500, 11600, 11700, 11800, 11900, 12000, 12100, 12200, 12300, 12400, 12500, 12600,
	12700, 12800, 12900, 13000, 13100, 13200, 13300, 13400, 13500, 13600, 13700, 13800, 13900,
	14000, 14100, 14200, 14300, 14400, 14500, 14600, 14700, 14800, 14900, 15000, 15100, 15200,
	15300
};

/* ============================================================ // */
/* function prototype */
/* ============================================================ // */


/* ============================================================ // */
/* extern variable */
/* ============================================================ // */

/* extern function */
/* extern unsigned int upmu_get_reg_value(unsigned int reg); upmu_common.h, _not_ used */
/* extern bool mt_usb_is_device(void); _not_ used */
/* extern void Charger_Detect_Init(void); _not_ used */
/* extern void Charger_Detect_Release(void); _not_ used */
/* extern int hw_charging_get_charger_type(void);  included in charging.h*/
/* extern void mt_power_off(void); _not_ used */

static unsigned int charging_error;
static unsigned int charging_set_error_state(void *data);
static unsigned int charging_set_vindpm(void *data);

/* ============================================================ // */
unsigned int charging_value_to_parameter(const unsigned int *parameter, const unsigned int array_size,
				       const unsigned int val)
{
	if (val < array_size)
		return parameter[val];

		battery_log(BAT_LOG_CRTI, "Can't find the parameter \r\n");
		return parameter[0];

}

unsigned int charging_parameter_to_value(const unsigned int *parameter, const unsigned int array_size,
				       const unsigned int val)
{
	unsigned int i;

	battery_log(BAT_LOG_FULL, "array_size = %d \r\n", array_size);

	for (i = 0; i < array_size; i++) {
		if (val == *(parameter + i))
			return i;
	}

	battery_log(BAT_LOG_CRTI, "NO register value match \r\n");
	/* TODO: ASSERT(0);    // not find the value */
	return 0;
}

static unsigned int bmt_find_closest_level(const unsigned int *pList, unsigned int number,
					 unsigned int level)
{
	unsigned int i;
	unsigned int max_value_in_last_element;

	if (pList[0] < pList[1])
		max_value_in_last_element = KAL_TRUE;
	else
		max_value_in_last_element = KAL_FALSE;

	if (max_value_in_last_element == KAL_TRUE) {
		for (i = (number - 1); i != 0; i--) {	/* max value in the last element */
			if (pList[i] <= level) {
				battery_log(2, "zzf_%d<=%d     i=%d\n", pList[i], level, i);
				return pList[i];
			}
		}

		battery_log(BAT_LOG_CRTI, "Can't find closest level \r\n");
		return pList[0];
		/* return CHARGE_CURRENT_0_00_MA; */
	} else {
		for (i = 0; i < number; i++) {	/* max value in the first element */
			if (pList[i] <= level)
				return pList[i];
		}

		battery_log(BAT_LOG_CRTI, "Can't find closest level \r\n");
		return pList[number - 1];
		/* return CHARGE_CURRENT_0_00_MA; */
	}
}
#if !defined(CONFIG_POWER_EXT)
static unsigned int is_chr_det(void)
{
	unsigned int val = 0;
	/* TODO: add charger detection here */
	val = pmic_get_register_value(PMIC_RGS_CHRDET);
	battery_log(BAT_LOG_CRTI, "[is_chr_det] %d\n", val);

	return val;
}
#endif


static unsigned int charging_hw_init(void *data)
{
	unsigned int status = STATUS_OK;


	return status;
}

static unsigned int charging_get_bif_vbat(void *data);

static unsigned int charging_dump_register(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned short icc;
	unsigned short icl;
	unsigned short chg_en;
	unsigned short vcv;
	unsigned short aicc;

	battery_log(BAT_LOG_FULL, "charging_dump_register\r\n");

#if 0
	for (i = MT6336_RG_ICC; i <= MT6336_RG_VCV; i++) {
		mt6336_get_flag_register_value(i);
	};
#endif

	icc = mt6336_get_flag_register_value(MT6336_RG_ICC);
	icl = mt6336_get_flag_register_value(MT6336_RG_ICL);
	chg_en = mt6336_get_flag_register_value(MT6336_RG_EN_CHARGE);
	vcv = mt6336_get_flag_register_value(MT6336_RG_VCV);
	aicc = mt6336_get_flag_register_value(MT6336_RG_EN_AICC);

	battery_log(BAT_LOG_CRTI,
		    "[MT6336]ICC=%d, ICL=%d, VCV=%d, CHGEN=%d, AICC=%d\n",
		    icc * 50 + 500, icl * 50 + 50, vcv, chg_en, aicc);

#if 0
	unsigned int vbat;

	charging_get_bif_vbat(&vbat);
	battery_log(BAT_LOG_CRTI, "[BIF] vbat=%d mV\n", vbat);
#endif
	return status;
}

static unsigned int charging_enable(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int enable = *(unsigned int *) (data);

	mt6336_set_flag_register_value(MT6336_RG_EN_CHARGE, enable);
	if (!enable && charging_error)
		battery_log(BAT_LOG_CRTI, "[charging_enable] under test mode: disable charging\n");

	return status;
}

static unsigned int charging_set_cv_voltage(void *data)
{
	unsigned int status;
	unsigned short int array_size;
	unsigned int set_cv_voltage;
	unsigned short int register_value;
	/*static kal_int16 pre_register_value; */

	array_size = GETARRAYNUM(VBAT_CV_VTH);
	status = STATUS_OK;
	/*pre_register_value = -1; */
	set_cv_voltage = bmt_find_closest_level(VBAT_CV_VTH, array_size, *(unsigned int *) data);
	register_value =
	    charging_parameter_to_value(VBAT_CV_VTH, GETARRAYNUM(VBAT_CV_VTH), set_cv_voltage);
	battery_log(BAT_LOG_CRTI, "charging_set_cv_voltage register_value=0x%x %d %d\n",
		    register_value, *(unsigned int *) data, set_cv_voltage);

	mt6336_set_flag_register_value(MT6336_RG_VCV, register_value);

	return status;
}


static unsigned int charging_get_current(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int array_size;
	/*unsigned char reg_value; */
	unsigned int val;

	/*Get current level */
	array_size = GETARRAYNUM(CS_VTH);
	val = mt6336_get_flag_register_value(MT6336_RG_ICC);
	*(unsigned int *)data = charging_value_to_parameter(CS_VTH, array_size, val);

	return status;
}


static unsigned int charging_set_current(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int set_chr_current;
	unsigned int array_size;
	unsigned int register_value;
	unsigned int current_value = *(unsigned int *) data;

	array_size = GETARRAYNUM(CS_VTH);
	set_chr_current = bmt_find_closest_level(CS_VTH, array_size, current_value);
	register_value = charging_parameter_to_value(CS_VTH, array_size, set_chr_current);
	mt6336_set_flag_register_value(MT6336_RG_ICC, register_value);

	return status;
}

static unsigned int charging_set_input_current(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int current_value = *(unsigned int *) data;
	unsigned int set_chr_current;
	unsigned int array_size;
	unsigned int register_value;

	array_size = GETARRAYNUM(INPUT_CS_VTH);
	set_chr_current = bmt_find_closest_level(INPUT_CS_VTH, array_size, current_value);
	register_value = charging_parameter_to_value(INPUT_CS_VTH, array_size, set_chr_current);
	mt6336_set_flag_register_value(MT6336_RG_ICL, register_value);

	/*For USB_IF compliance test only when USB is in suspend(Ibus < 2.5mA) or unconfigured(Ibus < 70mA) states*/
#ifdef CONFIG_USBIF_COMPLIANCE /* TODO: set to the right value */
	if (current_value < CHARGE_CURRENT_100_00_MA)
		register_value = 0x7f;
	else
		register_value = 0x13;
	charging_set_vindpm(&register_value);
#endif

	return status;
}

static unsigned int charging_get_charging_status(void *data)
{
	unsigned int status = STATUS_OK;
#if 0 /* TODO: */
	unsigned char reg_value;

	bq25890_read_interface(bq25890_CONB, &reg_value, 0x3, 3);	/* ICHG to BAT */

	if (reg_value == 0x3)	/* check if chrg done */
		*(unsigned int *) data = true;
	else
		*(unsigned int *) data = false;
#endif
	return status;
}

static unsigned int charging_reset_watch_dog_timer(void *data)
{
	unsigned int status = STATUS_OK;

	/* reset watchdog timer */
	mt6336_set_flag_register_value(MT6336_RG_WR_TRI, 0x1);

	return status;
}

static unsigned int charging_set_hv_threshold(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int set_hv_voltage;
	unsigned int array_size;
	unsigned short int register_value;
	unsigned int voltage = *(unsigned int *) (data);

	array_size = GETARRAYNUM(VCDT_HV_VTH);
	set_hv_voltage = bmt_find_closest_level(VCDT_HV_VTH, array_size, voltage);
	register_value = charging_parameter_to_value(VCDT_HV_VTH, array_size, set_hv_voltage);
	pmic_set_register_value(PMIC_RG_VCDT_HV_VTH, register_value);

	return status;
}


static unsigned int charging_get_hv_status(void *data)
{
	unsigned int status = STATUS_OK;

	*(kal_bool *) (data) = pmic_get_register_value(PMIC_RGS_VCDT_HV_DET);

	return status;
}


static unsigned int charging_get_battery_status(void *data)
{
	unsigned int status = STATUS_OK;

#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
	*(kal_bool *) (data) = 0;
	battery_log(BAT_LOG_CRTI, "bat exist for evb\n");
#else
	unsigned int val = 0;

	val = pmic_get_register_value(PMIC_BATON_TDET_EN);
	battery_log(BAT_LOG_FULL, "[charging_get_battery_status] BATON_TDET_EN = %d\n", val);
	if (val) {
		pmic_set_register_value(PMIC_BATON_TDET_EN, 1);
		pmic_set_register_value(PMIC_RG_BATON_EN, 1);
		*(kal_bool *) (data) = pmic_get_register_value(PMIC_RGS_BATON_UNDET);
	} else {
		*(kal_bool *) (data) = KAL_FALSE;
	}
#endif
	return status;
}


static unsigned int charging_get_charger_det_status(void *data)
{
	unsigned int status = STATUS_OK;

#if defined(CONFIG_MTK_FPGA)
	*(kal_bool *) (data) = 1;
	battery_log(BAT_LOG_CRTI, "chr exist for fpga\n");
#else
	*(kal_bool *) (data) = pmic_get_register_value_nolock(PMIC_RGS_CHRDET);
#endif
	return status;
}


kal_bool charging_type_detection_done(void)
{
	return charging_type_det_done;
}


static unsigned int charging_get_charger_type(void *data)
{
	unsigned int status = STATUS_OK;

#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
	*(CHARGER_TYPE *) (data) = STANDARD_HOST;
#else
	if (is_chr_det() == 0) {
		g_charger_type = CHARGER_UNKNOWN;
		*(CHARGER_TYPE *) (data) = CHARGER_UNKNOWN;
		battery_log(BAT_LOG_CRTI, "[charging_get_charger_type] return CHARGER_UNKNOWN\n");
		return status;
	}

	charging_type_det_done = KAL_FALSE;
	*(CHARGER_TYPE *) (data) = hw_charging_get_charger_type();
	charging_type_det_done = KAL_TRUE;
	g_charger_type = *(CHARGER_TYPE *) (data);

#endif

	return status;
}

static unsigned int charging_get_is_pcm_timer_trigger(void *data)
{
	unsigned int status = STATUS_OK;

#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA) || defined(CONFIG_ARCH_ELBRUS)
	*(kal_bool *) (data) = KAL_FALSE;
#else
	if (slp_get_wake_reason() == WR_PCM_TIMER)
		*(kal_bool *) (data) = KAL_TRUE;
	else
		*(kal_bool *) (data) = KAL_FALSE;

	battery_log(BAT_LOG_CRTI, "slp_get_wake_reason=%d\n", slp_get_wake_reason());
#endif

	return status;
}

static unsigned int charging_set_platform_reset(void *data)
{
	unsigned int status = STATUS_OK;

#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
#else
	battery_log(BAT_LOG_CRTI, "charging_set_platform_reset\n");
	kernel_restart("battery service reboot system");
#endif

	return status;
}

static unsigned int charging_get_platform_boot_mode(void *data)
{
	unsigned int status = STATUS_OK;

#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
#else
	*(unsigned int *) (data) = get_boot_mode();

	battery_log(BAT_LOG_CRTI, "get_boot_mode=%d\n", get_boot_mode());
#endif

	return status;
}

static unsigned int charging_set_power_off(void *data)
{
	unsigned int status = STATUS_OK;

#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
#else
#if 0
	/* close flashlight */
	checkAndRelease();
#endif
	/*added dump_stack to see who the caller is */
	dump_stack();
	battery_log(BAT_LOG_CRTI, "charging_set_power_off\n");
	kernel_power_off();
#endif

	return status;
}

static unsigned int charging_set_ta_current_pattern(void *data)
{
	kal_bool pumpup;
	unsigned int increase;

	pumpup = *(kal_bool *) (data);
	if (pumpup == KAL_TRUE)
		increase = 1;
	else
		increase = 0;
	/*unsigned int charging_status = KAL_FALSE; */
#if 1
	/* TODO: */
#if 0
	bq25890_pumpx_up(increase);
	battery_log(BAT_LOG_FULL, "Pumping up adaptor...");
#endif

#else
	if (increase == KAL_TRUE) {
		bq25890_set_ichg(0x0);	/* 64mA */
		msleep(85);

		bq25890_set_ichg(0x8);	/* 512mA */
		battery_log(BAT_LOG_FULL, "mtk_ta_increase() on 1");
		msleep(85);

		bq25890_set_ichg(0x0);	/* 64mA */
		battery_log(BAT_LOG_FULL, "mtk_ta_increase() off 1");
		msleep(85);

		bq25890_set_ichg(0x8);	/* 512mA */
		battery_log(BAT_LOG_FULL, "mtk_ta_increase() on 2");
		msleep(85);

		bq25890_set_ichg(0x0);	/* 64mA */
		battery_log(BAT_LOG_FULL, "mtk_ta_increase() off 2");
		msleep(85);

		bq25890_set_ichg(0x8);	/* 512mA */
		battery_log(BAT_LOG_FULL, "mtk_ta_increase() on 3");
		msleep(281);

		bq25890_set_ichg(0x0);	/* 64mA */
		battery_log(BAT_LOG_FULL, "mtk_ta_increase() off 3");
		msleep(85);

		bq25890_set_ichg(0x8);	/* 512mA */
		battery_log(BAT_LOG_FULL, "mtk_ta_increase() on 4");
		msleep(281);

		bq25890_set_ichg(0x0);	/* 64mA */
		battery_log(BAT_LOG_FULL, "mtk_ta_increase() off 4");
		msleep(85);

		bq25890_set_ichg(0x8);	/* 512mA */
		battery_log(BAT_LOG_FULL, "mtk_ta_increase() on 5");
		msleep(281);

		bq25890_set_ichg(0x0);	/* 64mA */
		battery_log(BAT_LOG_FULL, "mtk_ta_increase() off 5");
		msleep(85);

		bq25890_set_ichg(0x8);	/* 512mA */
		battery_log(BAT_LOG_FULL, "mtk_ta_increase() on 6");
		msleep(485);

		bq25890_set_ichg(0x0);	/* 64mA */
		battery_log(BAT_LOG_FULL, "mtk_ta_increase() off 6");
		msleep(50);

		battery_log(BAT_LOG_CRTI, "mtk_ta_increase() end\n");

		bq25890_set_ichg(0x8);	/* 512mA */
		msleep(200);
	} else {
		bq25890_set_ichg(0x0);	/* 64mA */
		msleep(85);

		bq25890_set_ichg(0x8);	/* 512mA */
		battery_log(BAT_LOG_FULL, "mtk_ta_decrease() on 1");
		msleep(281);

		bq25890_set_ichg(0x0);	/* 64mA */
		battery_log(BAT_LOG_FULL, "mtk_ta_decrease() off 1");
		msleep(85);

		bq25890_set_ichg(0x8);	/* 512mA */
		battery_log(BAT_LOG_FULL, "mtk_ta_decrease() on 2");
		msleep(281);

		bq25890_set_ichg(0x0);	/* 64mA */
		battery_log(BAT_LOG_FULL, "mtk_ta_decrease() off 2");
		msleep(85);

		bq25890_set_ichg(0x8);	/* 512mA */
		battery_log(BAT_LOG_FULL, "mtk_ta_decrease() on 3");
		msleep(281);

		bq25890_set_ichg(0x0);	/* 64mA */
		battery_log(BAT_LOG_FULL, "mtk_ta_decrease() off 3");
		msleep(85);

		bq25890_set_ichg(0x8);	/* 512mA */
		battery_log(BAT_LOG_FULL, "mtk_ta_decrease() on 4");
		msleep(85);

		bq25890_set_ichg(0x0);	/* 64mA */
		battery_log(BAT_LOG_FULL, "mtk_ta_decrease() off 4");
		msleep(85);

		bq25890_set_ichg(0x8);	/* 512mA */
		battery_log(BAT_LOG_FULL, "mtk_ta_decrease() on 5");
		msleep(85);

		bq25890_set_ichg(0x0);	/* 64mA */
		battery_log(BAT_LOG_FULL, "mtk_ta_decrease() off 5");
		msleep(85);

		bq25890_set_ichg(0x8);	/* 512mA */
		battery_log(BAT_LOG_FULL, "mtk_ta_decrease() on 6");
		msleep(485);

		bq25890_set_ichg(0x0);	/* 64mA */
		battery_log(BAT_LOG_FULL, "mtk_ta_decrease() off 6");
		msleep(50);

		battery_log(BAT_LOG_CRTI, "mtk_ta_decrease() end\n");

		bq25890_set_ichg(0x8);	/* 512mA */
	}
#endif
	return STATUS_OK;
}

static unsigned int charging_set_ta20_reset(void *data)
{
#if 0 /* TODO */
	bq25890_set_VINDPM(0x13);
	bq25890_set_ichg(8);

	bq25890_set_ico_en_start(0);
	bq25890_set_iinlim(0x0);
	msleep(250);
	bq25890_set_iinlim(0xc);
	bq25890_set_ico_en_start(1);
#endif
	return STATUS_OK;
}

#if 0 /* TODO */
struct timespec ptime[13];

static int cptime[13][2];

static int dtime(int i)
{
	struct timespec time;

	time = timespec_sub(ptime[i], ptime[i-1]);
	return time.tv_nsec/1000000;
}

#define PEOFFTIME 40
#define PEONTIME 90
#endif

static unsigned int charging_set_ta20_current_pattern(void *data)
{
#if 0 /* TODO */
	int value;
	int i, j = 0;
	int flag;
	CHR_VOLTAGE_ENUM chr_vol = *(CHR_VOLTAGE_ENUM *) data;
	int errcnt = 0;

	bq25890_set_VINDPM(0x13);
	bq25890_set_ichg(8);
	bq25890_set_ico_en_start(0);

	usleep_range(1000, 1200);
	value = (chr_vol - CHR_VOLT_05_500000_V) / CHR_VOLT_00_500000_V;

	bq25890_set_iinlim(0x0);
	msleep(70);

	get_monotonic_boottime(&ptime[j++]);
	for (i = 4; i >= 0; i--) {
		flag = value & (1 << i);

		if (flag == 0) {
			bq25890_set_iinlim(0xc);
			msleep(PEOFFTIME);
			get_monotonic_boottime(&ptime[j]);
			cptime[j][0] = PEOFFTIME;
			cptime[j][1] = dtime(j);
			if (cptime[j][1] < 30 || cptime[j][1] > 65) {
				battery_log(BAT_LOG_CRTI,
					"charging_set_ta20_current_pattern fail1: idx:%d target:%d actual:%d\n",
					i, PEOFFTIME, cptime[j][1]);
				errcnt = 1;
				return STATUS_FAIL;
			}
			j++;
			bq25890_set_iinlim(0x0);
			msleep(PEONTIME);
			get_monotonic_boottime(&ptime[j]);
			cptime[j][0] = PEONTIME;
			cptime[j][1] = dtime(j);
			if (cptime[j][1] < 90 || cptime[j][1] > 115) {
				battery_log(BAT_LOG_CRTI,
					"charging_set_ta20_current_pattern fail2: idx:%d target:%d actual:%d\n",
					i, PEOFFTIME, cptime[j][1]);
				errcnt = 1;
				return STATUS_FAIL;
			}
			j++;

		} else {
			bq25890_set_iinlim(0xc);
			msleep(PEONTIME);
			get_monotonic_boottime(&ptime[j]);
			cptime[j][0] = PEONTIME;
			cptime[j][1] = dtime(j);
			if (cptime[j][1] < 90 || cptime[j][1] > 115) {
				battery_log(BAT_LOG_CRTI,
					"charging_set_ta20_current_pattern fail3: idx:%d target:%d actual:%d\n",
					i, PEOFFTIME, cptime[j][1]);
				errcnt = 1;
				return STATUS_FAIL;
			}
			j++;
			bq25890_set_iinlim(0x0);
			msleep(PEOFFTIME);
			get_monotonic_boottime(&ptime[j]);
			cptime[j][0] = PEOFFTIME;
			cptime[j][1] = dtime(j);
			if (cptime[j][1] < 30 || cptime[j][1] > 65) {
				battery_log(BAT_LOG_CRTI,
					"charging_set_ta20_current_pattern fail4: idx:%d target:%d actual:%d\n",
					i, PEOFFTIME, cptime[j][1]);
				errcnt = 1;
				return STATUS_FAIL;
			}
			j++;
		}
	}

	bq25890_set_iinlim(0xc);
	msleep(160);
	get_monotonic_boottime(&ptime[j]);
	cptime[j][0] = 160;
	cptime[j][1] = dtime(j);
	if (cptime[j][1] < 150 || cptime[j][1] > 240) {
		battery_log(BAT_LOG_CRTI,
			"charging_set_ta20_current_pattern fail5: idx:%d target:%d actual:%d\n",
			i, PEOFFTIME, cptime[j][1]);
		errcnt = 1;
		return STATUS_FAIL;
	}
	j++;

	bq25890_set_iinlim(0x0);
	msleep(30);
	bq25890_set_iinlim(0xc);

	battery_log(BAT_LOG_CRTI,
	"[charging_set_ta20_current_pattern]:err:%d chr_vol:%d bit:%d time:%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d!!\n",
	errcnt, chr_vol, value,
	cptime[1][0], cptime[2][0], cptime[3][0], cptime[4][0], cptime[5][0],
	cptime[6][0], cptime[7][0], cptime[8][0], cptime[9][0], cptime[10][0], cptime[11][0]);

	battery_log(BAT_LOG_CRTI,
	"[charging_set_ta20_current_pattern2]:err:%d chr_vol:%d bit:%d time:%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d!!\n",
	errcnt, chr_vol, value,
	cptime[1][1], cptime[2][1], cptime[3][1], cptime[4][1], cptime[5][1],
	cptime[6][1], cptime[7][1], cptime[8][1], cptime[9][1], cptime[10][1], cptime[11][1]);


	bq25890_set_ico_en_start(1);
	bq25890_set_iinlim(0x3f);

	if (errcnt == 0)
		return STATUS_OK;

#endif
	return STATUS_FAIL;
}

static unsigned int charging_set_vindpm(void *data)
{
	unsigned int status = STATUS_OK;
#if 0 /* TODO */
	unsigned int v = *(unsigned int *) data;

	bq25890_set_VINDPM(v);
#endif
	return status;
}

static unsigned int charging_set_vindpm_voltage(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int vindpm_in;
	unsigned int vindpm;
	unsigned int array_size;

	array_size = GETARRAYNUM(VINDPM_REG);
	vindpm_in = bmt_find_closest_level(VINDPM_REG, array_size, *(unsigned int *) data);
	vindpm = charging_parameter_to_value(VINDPM_REG, array_size, vindpm_in);

	charging_set_vindpm(&vindpm);
	battery_log(BAT_LOG_CRTI, "charging_set_vindpm:%d 0x%x\n", vindpm_in, vindpm);
	/*bq25890_set_en_hiz(en);*/

	return status;
}

static unsigned int charging_set_vbus_ovp_en(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int en = *(unsigned int *) data;

	pmic_set_register_value(PMIC_RG_VCDT_HV_EN, en);

	return status;
}

static unsigned int charging_get_bif_vbat(void *data)
{
	unsigned int status = STATUS_OK;
	*(unsigned int *) (data) = 0;

	/* TODO */

	return status;
}

static unsigned int charging_get_bif_tbat(void *data)
{
	unsigned int status = STATUS_OK;
	*(unsigned int *) (data) = 0;

	/* TODO */

	return status;
}

static unsigned int charging_set_hiz_swchr(void *data);
static unsigned int charging_set_error_state(void *data)
{
	unsigned int status = STATUS_OK;

	charging_error = *(unsigned int *) (data);
	charging_set_hiz_swchr(&charging_error);
	return status;
}

static unsigned int charging_set_chrind_ck_pdn(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int pwr_dn;

	pwr_dn = *(unsigned int *) data;
	mt6336_set_flag_register_value(MT6336_CLK_DRV_CHRIND_CK_PDN, pwr_dn);

	return status;
}

static unsigned int charging_sw_init(void *data)
{
	unsigned int status = STATUS_OK;
	/*put here anything needed to be init upon battery_common driver probe*/
#if 0
	bq25890_config_interface(bq25890_CON7, 0x1, 0x1, 3);	/* enable charging timer safety timer */
	bq25890_config_interface(bq25890_CON7, 0x2, 0x3, 1);	/* charging timer 12h */

	/*Vbus current limit */
	bq25890_config_interface(bq25890_CON0, 0x01, 0x01, 6);	/* enable ilimit Pin */
	 /*DPM*/ bq25890_config_interface(bq25890_CON1, 0x6, 0xF, 0);	/* Vindpm offset  600MV */
	bq25890_config_interface(bq25890_COND, 0x1, 0x1, 7);	/* vindpm vth 0:relative 1:absolute */
	/* absolute VINDPM = 2.6 + code x 0.1 =4.5V;K2 24261 4.452V */
	bq25890_config_interface(bq25890_COND, 0x13, 0x7F, 0);

	/*CC mode */
	/*CV mode */
	bq25890_config_interface(bq25890_CON6, 0x20, 0x3F, 2);	/* VREG=CV 4.352V (default 4.208V) */

#if defined(CONFIG_MTK_PUMP_EXPRESS_PLUS_20_SUPPORT)
	/*	upmu_set_rg_vcdt_hv_en(0);*/
#if defined(CONFIG_MTK_PMIC_CHIP_MT6353)
	pmic_set_register_value(PMIC_RG_VCDT_HV_EN, 0);
#else
	pmic_set_register_value(MT6351_PMIC_RG_VCDT_HV_EN, 0);
#endif
#endif
#endif

	/* AUXADC_AVG_NUM_SEL_L[7:0] */
	mt6336_config_interface(0x34D, 0x04, 0xFF, 0);
	/* [7:6]:AUXADC_TRIM_CH3_SEL[1:0];[5:4]:AUXADC_TRIM_CH2_SEL[1:0] */
	mt6336_config_interface(0x355, 0xE0, 0xFF, 0);
	/* [5:4]:AUXADC_TRIM_CH6_SEL[1:0];[1:0]:AUXADC_TRIM_CH4_SEL[1:0] */
	mt6336_config_interface(0x356, 0x11, 0xFF, 0);
	/* [3:2]:AUXADC_TRIM_CH9_SEL[1:0] */
	mt6336_config_interface(0x357, 0x04, 0xFF, 0);
	/* [0:0]:RG_EN_BUCK;[1:1]:RG_EN_CHARGE */
	mt6336_config_interface(0x400, 0x03, 0xFF, 0);
	/* [0:0]:RG_DIS_LOWQ_MODE */
	mt6336_config_interface(0x409, 0x01, 0xFF, 0);
	/* [0:0]:RG_BC12_RST;[2:2]:RG_BC12_TIMER_EN */
	mt6336_config_interface(0x42B, 0x01, 0xFF, 0);
	/* [0:0]:RG_EN_ICL150PIN */
	mt6336_config_interface(0x438, 0x08, 0xFF, 0);
	/* [0:0]:RG_FREQ_SEL */
	mt6336_config_interface(0x43A, 0x02, 0xFF, 0);
	/* [5:5]:RG_A_EN_TRIM_IBATSNS */
	mt6336_config_interface(0x50E, 0x20, 0xFF, 0);
	/* [0:0]:RG_A_LOOP_CLAMP_EN;[6:1]:RG_A_LOOP_GM_EN[5:0] */
	mt6336_config_interface(0x519, 0x3F, 0xFF, 0);
	/* [7:0]:RG_A_LOOP_GM_TUNE_DPM_MSB[7:0] */
	mt6336_config_interface(0x51A, 0x35, 0xFF, 0);
	/* [7:0]:RG_A_LOOP_GM_TUNE_DPM_LSB[7:0] */
	mt6336_config_interface(0x51B, 0x84, 0xFF, 0);
	/* [7:0]:RG_A_LOOP_GM_TUNE_IBAT_MSB[7:0] */
	mt6336_config_interface(0x51C, 0x01, 0xFF, 0);
	/* [7:0]:RG_A_LOOP_GM_TUNE_IBAT_LSB[7:0] */
	mt6336_config_interface(0x51D, 0x84, 0xFF, 0);
	/* [7:0]:RG_A_LOOP_GM_TUNE_ICHIN_MSB[7:0] */
	mt6336_config_interface(0x51E, 0x01, 0xFF, 0);
	/* [7:0]:RG_A_LOOP_GM_TUNE_ICHIN_LSB[7:0] */
	mt6336_config_interface(0x51F, 0x84, 0xFF, 0);
	/* [7:0]:RG_A_LOOP_GM_TUNE_SYS_MSB[7:0] */
	mt6336_config_interface(0x520, 0x34, 0xFF, 0);
	/* [7:0]:RG_A_LOOP_GM_TUNE_SYS_LSB[7:0] */
	mt6336_config_interface(0x521, 0x44, 0xFF, 0);
	/* [7:0]:RG_A_LOOP_GM_TUNE_THR_MSB[7:0] */
	mt6336_config_interface(0x522, 0x35, 0xFF, 0);
	/* [7:0]:RG_A_LOOP_GM_TUNE_THR_LSB[7:0] */
	mt6336_config_interface(0x523, 0x84, 0xFF, 0);
	/* [3:0]:RG_A_LOOP_ICS[3:0] */
	mt6336_config_interface(0x526, 0x04, 0xFF, 0);
	/* [3:0]:RG_A_VRAMP_DCOS[3:0];[6:4]:RG_A_VRAMP_SLP[2:0] */
	mt6336_config_interface(0x53D, 0x22, 0xFF, 0);
	/* [3:0]:RG_A_VRAMP_VCS_RTUNE[3:0] */
	mt6336_config_interface(0x53F, 0x09, 0xFF, 0);
	/* [0:0]:RG_A_LOGIC_GDRI_MINOFF_DIS */
	mt6336_config_interface(0x548, 0x11, 0xFF, 0);
	/* [7:6]:RG_A_PWR_UG_DTC[1:0] */
	mt6336_config_interface(0x552, 0xE8, 0xFF, 0);
	/* [1:1]:rg_da_qi_en_adcin_vbaton */
	mt6336_config_interface(0x5AF, 0x02, 0xFF, 0);
	/* [1:1]:rg_da_qi_en_adcin_vbaton_sel */
	mt6336_config_interface(0x64E, 0x02, 0xFF, 0);

	return status;
}

static unsigned int charging_enable_safetytimer(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int en;

	en = *(unsigned int *) data;
	mt6336_set_flag_register_value(MT6336_RG_EN_CHR_SAFETMR, en);

	return status;
}

static unsigned int charging_set_hiz_swchr(void *data)
{
	unsigned int status = STATUS_OK;
#if 0 /* TODO */
	unsigned int en;
	unsigned int vindpm;

	en = *(unsigned int *) data;
	if (en == 1)
		vindpm = 0x7F;
	else
		vindpm = 0x13;

	charging_set_vindpm(&vindpm);
	/*bq25890_set_en_hiz(en);*/
#endif
	return status;
}

static unsigned int charging_set_boost_current_limit(void *data)
{
	int ret = 0;
	u32 current_limit = 0;

	current_limit = *((u32 *)data);
	/* TODO */

	return ret;
}

static unsigned int charging_enable_otg(void *data)
{
	int ret = 0;
#if 0 /* TODO */
	u8 enable = *((u8 *)data);

#endif
	return ret;
}

static unsigned int (*const charging_func[CHARGING_CMD_NUMBER]) (void *data) = {
	[CHARGING_CMD_INIT] = charging_hw_init,
	[CHARGING_CMD_DUMP_REGISTER] = charging_dump_register,
	[CHARGING_CMD_ENABLE] = charging_enable,
	[CHARGING_CMD_SET_CV_VOLTAGE] = charging_set_cv_voltage,
	[CHARGING_CMD_GET_CURRENT] = charging_get_current,
	[CHARGING_CMD_SET_CURRENT] = charging_set_current,
	[CHARGING_CMD_SET_INPUT_CURRENT] = charging_set_input_current,
	[CHARGING_CMD_GET_CHARGING_STATUS] = charging_get_charging_status,
	[CHARGING_CMD_RESET_WATCH_DOG_TIMER] = charging_reset_watch_dog_timer,
	[CHARGING_CMD_SET_HV_THRESHOLD] =  charging_set_hv_threshold,
	[CHARGING_CMD_GET_HV_STATUS] = charging_get_hv_status,
	[CHARGING_CMD_GET_BATTERY_STATUS] = charging_get_battery_status,
	[CHARGING_CMD_GET_CHARGER_DET_STATUS] =  charging_get_charger_det_status,
	[CHARGING_CMD_GET_CHARGER_TYPE] = charging_get_charger_type,
	[CHARGING_CMD_GET_IS_PCM_TIMER_TRIGGER] = charging_get_is_pcm_timer_trigger,
	[CHARGING_CMD_SET_PLATFORM_RESET] = charging_set_platform_reset,
	[CHARGING_CMD_GET_PLATFORM_BOOT_MODE] =  charging_get_platform_boot_mode,
	[CHARGING_CMD_SET_POWER_OFF] = charging_set_power_off,
	[CHARGING_CMD_SET_TA_CURRENT_PATTERN] = charging_set_ta_current_pattern,
	[CHARGING_CMD_SET_ERROR_STATE] = charging_set_error_state,
	[CHARGING_CMD_SET_VINDPM] = charging_set_vindpm_voltage,
	[CHARGING_CMD_SET_VBUS_OVP_EN] = charging_set_vbus_ovp_en,
	[CHARGING_CMD_GET_BIF_VBAT] = charging_get_bif_vbat,
	[CHARGING_CMD_SET_CHRIND_CK_PDN] = charging_set_chrind_ck_pdn,
	[CHARGING_CMD_SW_INIT] = charging_sw_init,
	[CHARGING_CMD_ENABLE_SAFETY_TIMER] = charging_enable_safetytimer,
	[CHARGING_CMD_SET_HIZ_SWCHR] = charging_set_hiz_swchr,
	[CHARGING_CMD_GET_BIF_TBAT] = charging_get_bif_tbat,
	[CHARGING_CMD_SET_TA20_RESET] = charging_set_ta20_reset,
	[CHARGING_CMD_SET_TA20_CURRENT_PATTERN] =  charging_set_ta20_current_pattern,
	[CHARGING_CMD_SET_BOOST_CURRENT_LIMIT] = charging_set_boost_current_limit,
	[CHARGING_CMD_ENABLE_OTG] = charging_enable_otg,
};

/*
* FUNCTION
*        Internal_chr_control_handler
*
* DESCRIPTION
*         This function is called to set the charger hw
*
* CALLS
*
* PARAMETERS
*        None
*
* RETURNS
*
*
* GLOBALS AFFECTED
*       None
*/
int chr_control_interface(CHARGING_CTRL_CMD cmd, void *data)
{
	int status;

	if (cmd < CHARGING_CMD_NUMBER) {
		if (charging_func[cmd] != NULL)
			status = charging_func[cmd](data);
		else {
			battery_log(BAT_LOG_CRTI, "[chr_control_interface]cmd:%d not supported\n", cmd);
			status = STATUS_UNSUPPORTED;
		}
	} else
		status = STATUS_UNSUPPORTED;

	return status;
}
