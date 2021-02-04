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

#include <linux/types.h>
#include <linux/kernel.h>
#include <mt-plat/battery_meter.h>
#include <mt-plat/battery_common.h>
#include <mt-plat/battery_meter_hal.h>
#include <mt-plat/charging.h>
#include <mach/mt_charging.h>
#include <mt-plat/mtk_boot.h>

#include "mtk_pep_intf.h"
#include "mtk_pep20_intf.h"

#ifdef CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT
#include <mt-plat/diso.h>
#include <mach/mt_diso.h>
#endif


/* ============================================================ // */
/* define */
/* ============================================================ // */
/* cut off to full */
#define POST_CHARGING_TIME (30*60)	/* 30mins */
#define FULL_CHECK_TIMES 6

#define STATUS_OK    0
#define STATUS_UNSUPPORTED    -1
#define STATUS_FAIL -2


/* ============================================================ // */
/* global variable */
/* ============================================================ // */
unsigned int g_bcct_flag;
unsigned int g_bcct_value;
/*input-output curent distinction*/
unsigned int g_bcct_input_flag;
unsigned int g_bcct_input_value;
unsigned int g_full_check_count;
enum CHR_CURRENT_ENUM g_temp_CC_value = CHARGE_CURRENT_0_00_MA;
enum CHR_CURRENT_ENUM g_temp_input_CC_value = CHARGE_CURRENT_0_00_MA;
unsigned int g_usb_state = USB_UNCONFIGURED;
static bool usb_unlimited;
#if (CONFIG_MTK_GAUGE_VERSION == 20)
#ifdef HIGH_BATTERY_VOLTAGE_SUPPORT
enum BATTERY_VOLTAGE_ENUM g_cv_voltage = BATTERY_VOLT_04_340000_V;
#else
enum BATTERY_VOLTAGE_ENUM g_cv_voltage = BATTERY_VOLT_04_200000_V;
#endif
unsigned int get_cv_voltage(void)
{
	return g_cv_voltage;
}
#endif
DEFINE_MUTEX(g_ichg_aicr_access_mutex);
DEFINE_MUTEX(g_aicr_access_mutex);
DEFINE_MUTEX(g_ichg_access_mutex);
DEFINE_MUTEX(g_hv_charging_mutex);
unsigned int g_aicr_upper_bound;
static bool g_pd_enable_power_path = true;
static bool g_enable_dynamic_cv = true;
static bool g_enable_hv_charging = true;
static atomic_t g_en_kpoc_shdn = ATOMIC_INIT(1);

 /* /////////////////////////////////////////////////////////////////////// */
 /* // JEITA */
 /* /////////////////////////////////////////////////////////////////////// */
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
int g_temp_status = TEMP_POS_10_TO_POS_45;
bool temp_error_recovery_chr_flag = true;
#endif

/* ============================================================ // */
/* function prototype */
/* ============================================================ // */
void BATTERY_SetUSBState(int usb_state_value)
{
#if defined(CONFIG_POWER_EXT)
	battery_log(BAT_LOG_CRTI, "[BATTERY_SetUSBState] in FPGA/EVB, no service\r\n");
#else
	if ((usb_state_value < USB_SUSPEND) || ((usb_state_value > USB_CONFIGURED))) {
		battery_log(BAT_LOG_CRTI,
			    "[BATTERY] BAT_SetUSBState Fail! Restore to default value\r\n");
		usb_state_value = USB_UNCONFIGURED;
	} else {
		battery_log(BAT_LOG_CRTI, "[BATTERY] BAT_SetUSBState Success! Set %d\r\n",
			    usb_state_value);
		g_usb_state = usb_state_value;
	}
#endif
}


unsigned int get_charging_setting_current(void)
{
	return g_temp_CC_value;
}

int mtk_get_dynamic_cv(unsigned int *cv)
{
	int ret = 0;
#ifdef CONFIG_MTK_BIF_SUPPORT
	u32 _cv;
	u32 vbat_bif = 0, vbat_auxadc = 0, vbat = 0;
	u32 retry_cnt = 0;
	u32 ircmp_volt_clamp = 0, ircmp_resistor = 0;

	if (!g_enable_dynamic_cv) {
		if (batt_cust_data.high_battery_voltage_support)
			_cv = BATTERY_VOLT_04_340000_V / 1000;
		else
			_cv = BATTERY_VOLT_04_200000_V / 1000;
		goto _out;
	}

	do {
		ret = battery_charging_control(
				CHARGING_CMD_GET_BIF_VBAT, &vbat_bif);
		vbat_auxadc = battery_meter_get_battery_voltage(true);

		if (ret >= 0 && vbat_bif != 0 && vbat_bif < vbat_auxadc) {
			vbat = vbat_bif;
			battery_log(BAT_LOG_CRTI,
				    "%s: use BIF vbat = %dmV, dV to auxadc = %dmV\n",
				    __func__, vbat, vbat_auxadc - vbat_bif);
			break;
		}

		retry_cnt++;
	} while (retry_cnt < 5);

	if (retry_cnt == 5) {
		ret = 0;
		vbat = vbat_auxadc;
		battery_log(BAT_LOG_CRTI,
			    "%s: use AUXADC vbat = %dmV, since BIF vbat = %d\n",
			    __func__, vbat_auxadc, vbat_bif);
	}

	/* Adjust CV according to the obtained vbat */
	if (vbat >= 3400 && vbat < 4300) {
		_cv = 4550;
		battery_charging_control(
			CHARGING_CMD_SET_IRCMP_VOLT_CLAMP, &ircmp_volt_clamp);
		battery_charging_control(
			CHARGING_CMD_SET_IRCMP_RESISTOR, &ircmp_resistor);
	} else {
		if (batt_cust_data.high_battery_voltage_support)
			_cv = BATTERY_VOLT_04_340000_V / 1000;
		else
			_cv = BATTERY_VOLT_04_200000_V / 1000;

		/* Turn on IR compensation */
		ircmp_volt_clamp = 200;
		ircmp_resistor = 80;
		battery_charging_control(
			CHARGING_CMD_SET_IRCMP_VOLT_CLAMP, &ircmp_volt_clamp);
		battery_charging_control(
			CHARGING_CMD_SET_IRCMP_RESISTOR, &ircmp_resistor);

		/* Disable dynamic CV */
		g_enable_dynamic_cv = false;
	}

_out:
	*cv = _cv;
	battery_log(BAT_LOG_CRTI, "%s: CV = %dmV, enable dynamic cv = %d\n",
		    __func__, _cv, g_enable_dynamic_cv);
#else
	ret = -ENOTSUPP;
#endif
	return ret;
}

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)

static enum BATTERY_VOLTAGE_ENUM select_jeita_cv(void)
{
	enum BATTERY_VOLTAGE_ENUM cv_voltage;

	if (g_temp_status == TEMP_ABOVE_POS_60) {
		cv_voltage = JEITA_TEMP_ABOVE_POS_60_CV_VOLTAGE;
	} else if (g_temp_status == TEMP_POS_45_TO_POS_60) {
		cv_voltage = JEITA_TEMP_POS_45_TO_POS_60_CV_VOLTAGE;
	} else if (g_temp_status == TEMP_POS_10_TO_POS_45) {
		if (batt_cust_data.high_battery_voltage_support)
			cv_voltage = BATTERY_VOLT_04_340000_V;
		else
			cv_voltage = JEITA_TEMP_POS_10_TO_POS_45_CV_VOLTAGE;
	} else if (g_temp_status == TEMP_POS_0_TO_POS_10) {
		cv_voltage = JEITA_TEMP_POS_0_TO_POS_10_CV_VOLTAGE;
	} else if (g_temp_status == TEMP_NEG_10_TO_POS_0) {
		cv_voltage = JEITA_TEMP_NEG_10_TO_POS_0_CV_VOLTAGE;
	} else if (g_temp_status == TEMP_BELOW_NEG_10) {
		cv_voltage = JEITA_TEMP_BELOW_NEG_10_CV_VOLTAGE;
	} else {
		cv_voltage = BATTERY_VOLT_04_200000_V;
	}

	return cv_voltage;
}

unsigned int do_jeita_state_machine(void)
{
	enum BATTERY_VOLTAGE_ENUM cv_voltage;
	unsigned int jeita_status = PMU_STATUS_OK;

	/* JEITA battery temp Standard */

	if (BMT_status.temperature >= TEMP_POS_60_THRESHOLD) {
		battery_log(BAT_LOG_CRTI,
			    "[BATTERY] Battery Over high Temperature(%d) !!\n\r",
			    TEMP_POS_60_THRESHOLD);

		g_temp_status = TEMP_ABOVE_POS_60;

		return PMU_STATUS_FAIL;
	} else if (BMT_status.temperature > TEMP_POS_45_THRESHOLD) {	/* control 45c to normal behavior */
		if ((g_temp_status == TEMP_ABOVE_POS_60)
		    && (BMT_status.temperature >= TEMP_POS_60_THRES_MINUS_X_DEGREE)) {
			battery_log(BAT_LOG_CRTI,
				    "[BATTERY] Battery Temperature between %d and %d,not allow charging yet!!\n\r",
				    TEMP_POS_60_THRES_MINUS_X_DEGREE, TEMP_POS_60_THRESHOLD);

			jeita_status = PMU_STATUS_FAIL;
		} else {
			battery_log(BAT_LOG_CRTI,
				    "[BATTERY] Battery Temperature between %d and %d !!\n\r",
				    TEMP_POS_45_THRESHOLD, TEMP_POS_60_THRESHOLD);

			g_temp_status = TEMP_POS_45_TO_POS_60;
		}
	} else if (BMT_status.temperature >= TEMP_POS_10_THRESHOLD) {
		if (((g_temp_status == TEMP_POS_45_TO_POS_60)
		     && (BMT_status.temperature >= TEMP_POS_45_THRES_MINUS_X_DEGREE))
		    || ((g_temp_status == TEMP_POS_0_TO_POS_10)
			&& (BMT_status.temperature <= TEMP_POS_10_THRES_PLUS_X_DEGREE))) {
			battery_log(BAT_LOG_CRTI,
				    "[BATTERY] Battery Temperature not recovery to normal temperature charging mode yet!!\n\r");
		} else {
			battery_log(BAT_LOG_CRTI,
				    "[BATTERY] Battery Normal Temperature between %d and %d !!\n\r",
				    TEMP_POS_10_THRESHOLD, TEMP_POS_45_THRESHOLD);
			g_temp_status = TEMP_POS_10_TO_POS_45;
		}
	} else if (BMT_status.temperature >= TEMP_POS_0_THRESHOLD) {
		if ((g_temp_status == TEMP_NEG_10_TO_POS_0 || g_temp_status == TEMP_BELOW_NEG_10)
		    && (BMT_status.temperature <= TEMP_POS_0_THRES_PLUS_X_DEGREE)) {
			if (g_temp_status == TEMP_NEG_10_TO_POS_0) {
				battery_log(BAT_LOG_CRTI,
					    "[BATTERY] Battery Temperature between %d and %d !!\n\r",
					    TEMP_POS_0_THRES_PLUS_X_DEGREE, TEMP_POS_10_THRESHOLD);
			}
			if (g_temp_status == TEMP_BELOW_NEG_10) {
				battery_log(BAT_LOG_CRTI,
					    "[BATTERY] Battery Temperature between %d and %d,not allow charging yet!!\n\r",
					    TEMP_POS_0_THRESHOLD, TEMP_POS_0_THRES_PLUS_X_DEGREE);
				return PMU_STATUS_FAIL;
			}
		} else {
			battery_log(BAT_LOG_CRTI,
				    "[BATTERY] Battery Temperature between %d and %d !!\n\r",
				    TEMP_POS_0_THRESHOLD, TEMP_POS_10_THRESHOLD);

			g_temp_status = TEMP_POS_0_TO_POS_10;
		}
	} else if (BMT_status.temperature >= TEMP_NEG_10_THRESHOLD) {
		if ((g_temp_status == TEMP_BELOW_NEG_10)
		    && (BMT_status.temperature <= TEMP_NEG_10_THRES_PLUS_X_DEGREE)) {
			battery_log(BAT_LOG_CRTI,
				    "[BATTERY] Battery Temperature between %d and %d,not allow charging yet!!\n\r",
				    TEMP_NEG_10_THRESHOLD, TEMP_NEG_10_THRES_PLUS_X_DEGREE);

			jeita_status = PMU_STATUS_FAIL;
		} else {
			battery_log(BAT_LOG_CRTI,
				    "[BATTERY] Battery Temperature between %d and %d !!\n\r",
				    TEMP_NEG_10_THRESHOLD, TEMP_POS_0_THRESHOLD);

			g_temp_status = TEMP_NEG_10_TO_POS_0;
		}
	} else {
		battery_log(BAT_LOG_CRTI,
			    "[BATTERY] Battery below low Temperature(%d) !!\n\r",
			    TEMP_NEG_10_THRESHOLD);
		g_temp_status = TEMP_BELOW_NEG_10;

		jeita_status = PMU_STATUS_FAIL;
	}

	/* set CV after temperature changed */
	/* In normal range, we adjust CV dynamically */
	if (g_temp_status != TEMP_POS_10_TO_POS_45) {
		cv_voltage = select_jeita_cv();
		battery_charging_control(
				CHARGING_CMD_SET_CV_VOLTAGE, &cv_voltage);
#if (CONFIG_MTK_GAUGE_VERSION == 20)
		g_cv_voltage = cv_voltage;
#endif
	}

	return jeita_status;
}


static void set_jeita_charging_current(void)
{
#ifdef CONFIG_USB_IF
	if (BMT_status.charger_type == STANDARD_HOST)
		return;
#endif

	if (g_temp_status == TEMP_NEG_10_TO_POS_0) {
		g_temp_CC_value = CHARGE_CURRENT_350_00_MA;
		g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
		battery_log(BAT_LOG_CRTI,
				"[BATTERY] JEITA set charging current : %d\r\n",
				g_temp_CC_value);
	}
}

#endif				/* CONFIG_MTK_JEITA_STANDARD_SUPPORT */

bool get_usb_current_unlimited(void)
{
	if (BMT_status.charger_type == STANDARD_HOST ||
		BMT_status.charger_type == CHARGING_HOST)
		return usb_unlimited;

	return false;
}

void set_usb_current_unlimited(bool enable)
{
	usb_unlimited = enable;
}

/*BQ25896 is the first switch chrager separating input and charge current */
unsigned int set_chr_input_current_limit(int current_limit)
{
#ifdef CONFIG_MTK_SWITCH_INPUT_OUTPUT_CURRENT_SUPPORT
	u32 power_path_enable = 1;
	CHR_CURRENT_ENUM chr_type_aicr = 0;	/* 10uA */
	CHR_CURRENT_ENUM chr_type_ichg = 0;

	mutex_lock(&g_aicr_access_mutex);
	if (current_limit != -1) {
		g_bcct_input_flag = 1;
		if (current_limit < 100) {
			/* limit < 100, turn off power path */
			current_limit = 0;
			power_path_enable = 0;
			battery_charging_control(CHARGING_CMD_ENABLE_POWER_PATH,
						 &power_path_enable);
		} else {
			/* Enable power path if it is disabled previously */
			if (g_bcct_input_value == 0) {
				power_path_enable = 1;
				battery_charging_control(
						CHARGING_CMD_ENABLE_POWER_PATH,
						&power_path_enable);
			}

			switch (BMT_status.charger_type) {
			case STANDARD_HOST:
				chr_type_aicr =
					batt_cust_data.usb_charger_current;
				break;
			case NONSTANDARD_CHARGER:
				chr_type_aicr =
					batt_cust_data.non_std_ac_charger_current;
				break;
			case STANDARD_CHARGER:
				chr_type_aicr =
					batt_cust_data.ac_charger_input_current;
				mtk_pep20_set_charging_current(
						&chr_type_ichg, &chr_type_aicr);
				mtk_pep_set_charging_current(
						&chr_type_ichg, &chr_type_aicr);
				break;
			case CHARGING_HOST:
				chr_type_aicr =
					batt_cust_data.charging_host_charger_current;
				break;
			case APPLE_2_1A_CHARGER:
				chr_type_aicr =
					batt_cust_data.apple_2_1a_charger_current;
				break;
			case APPLE_1_0A_CHARGER:
				chr_type_aicr =
					batt_cust_data.apple_1_0a_charger_current;
				break;
			case APPLE_0_5A_CHARGER:
				chr_type_aicr =
					batt_cust_data.apple_0_5a_charger_current;
				break;
			default:
				chr_type_aicr = CHARGE_CURRENT_500_00_MA;
				break;
			}
			chr_type_aicr /= 100;
			if (current_limit > chr_type_aicr)
				current_limit = chr_type_aicr;
		}

		g_bcct_input_value = current_limit;
	} else {
		/* Enable power path if it is disabled previously */
		if (g_bcct_input_value == 0) {
			power_path_enable = 1;
			battery_charging_control(CHARGING_CMD_ENABLE_POWER_PATH,
						 &power_path_enable);
		}

		/* Change to default current setting */
		g_bcct_input_flag = 0;
	}

	battery_log(BAT_LOG_CRTI,
			"[BATTERY] set_chr_input_current_limit (%d)\n",
			current_limit);

	mutex_unlock(&g_aicr_access_mutex);
	return g_bcct_input_flag;
#else
	battery_log(BAT_LOG_CRTI,
		"[BATTERY] set_chr_input_current_limit _NOT_ supported\n");
	return 0;
#endif
}

static void mtk_select_ichg_aicr(void);
unsigned int set_bat_charging_current_limit(int current_limit)
{
	enum CHR_CURRENT_ENUM chr_type_ichg = 0;
	enum CHR_CURRENT_ENUM chr_type_aicr = 0;

	mutex_lock(&g_ichg_access_mutex);
	if (current_limit != -1) {
		g_bcct_flag = 1;
		switch (BMT_status.charger_type) {
		case STANDARD_HOST:
			chr_type_ichg = batt_cust_data.usb_charger_current;
			break;
		case NONSTANDARD_CHARGER:
			chr_type_ichg =
				batt_cust_data.non_std_ac_charger_current;
			break;
		case STANDARD_CHARGER:
			chr_type_ichg = batt_cust_data.ac_charger_current;
			mtk_pep20_set_charging_current(
					&chr_type_ichg, &chr_type_aicr);
			mtk_pep_set_charging_current(
					&chr_type_ichg, &chr_type_aicr);
			break;
		case CHARGING_HOST:
			chr_type_ichg =
				batt_cust_data.charging_host_charger_current;
			break;
		case APPLE_2_1A_CHARGER:
			chr_type_ichg =
				batt_cust_data.apple_2_1a_charger_current;
			break;
		case APPLE_1_0A_CHARGER:
			chr_type_ichg =
				batt_cust_data.apple_1_0a_charger_current;
			break;
		case APPLE_0_5A_CHARGER:
			chr_type_ichg =
				batt_cust_data.apple_0_5a_charger_current;
			break;
		default:
			chr_type_ichg = CHARGE_CURRENT_500_00_MA;
			break;
		}
		chr_type_ichg /= 100;
		if (current_limit > chr_type_ichg)
			current_limit = chr_type_ichg;
		g_bcct_value = current_limit;
	} else			/* change to default current setting */
		g_bcct_flag = 0;

	mtk_select_ichg_aicr();

	battery_log(BAT_LOG_CRTI,
		    "[BATTERY] set_bat_charging_current_limit (%d)\r\n",
		    current_limit);

	mutex_unlock(&g_ichg_access_mutex);
	return g_bcct_flag;
}

int mtk_chr_reset_aicr_upper_bound(void)
{
	g_aicr_upper_bound = 0;
	return 0;
}

int mtk_chr_pd_enable_power_path(unsigned char enable)
{
	int ret = 0;

	g_pd_enable_power_path = enable;
	if (enable && g_bcct_input_flag && (g_bcct_input_value == 0)) {
		battery_log(BAT_LOG_CRTI,
			"%s: thermal set power path off, so keep it off\n",
			__func__);
		return -EINVAL;
	}

	ret = battery_charging_control(CHARGING_CMD_ENABLE_POWER_PATH,
		&enable);

	return ret;
}

int mtk_chr_enable_chr_type_det(unsigned char en)
{
	battery_log(BAT_LOG_CRTI, "%s: enable = %d\n", __func__, en);
	battery_charging_control(CHARGING_CMD_ENABLE_CHR_TYPE_DET, &en);

	return 0;
}

int mtk_chr_enable_discharge(bool enable)
{
	return battery_charging_control(CHARGING_CMD_ENABLE_DISCHARGE, &enable);
}

int mtk_chr_enable_hv_charging(bool en)
{
	battery_log(BAT_LOG_CRTI, "%s: en = %d\n", __func__, en);

	mutex_lock(&g_hv_charging_mutex);
	g_enable_hv_charging = en;
	mutex_unlock(&g_hv_charging_mutex);

	return 0;
}

bool mtk_chr_is_hv_charging_enable(void)
{
	return g_enable_hv_charging;
}

int mtk_chr_enable_kpoc_shutdown(bool en)
{
	if (en)
		atomic_set(&g_en_kpoc_shdn, 1);
	else
		atomic_set(&g_en_kpoc_shdn, 0);
	return 0;
}

bool mtk_chr_is_kpoc_shutdown_enable(void)
{
	int en = 0;

	en = atomic_read(&g_en_kpoc_shdn);

	return en > 0 ? true : false;
}

int set_chr_boost_current_limit(unsigned int current_limit)
{
	int ret = 0;

	ret = battery_charging_control(
			CHARGING_CMD_SET_BOOST_CURRENT_LIMIT, &current_limit);

	return ret;
}

int set_chr_enable_otg(unsigned int enable)
{
	int ret = 0;

	ret = battery_charging_control(CHARGING_CMD_ENABLE_OTG, &enable);

	return ret;
}

int mtk_chr_get_tchr(int *min_temp, int *max_temp)
{
	int ret = 0;
	int temp[2] = { 0, 0 };

	ret = battery_charging_control(
			CHARGING_CMD_GET_CHARGER_TEMPERATURE, temp);
	if (ret < 0)
		return ret;

	*min_temp = temp[0];
	*max_temp = temp[1];

	return ret;
}

int mtk_chr_get_soc(unsigned int *soc)
{
	if (BMT_status.SOC < 0) {
		*soc = 0;
		return -ENOTSUPP;
	}

	*soc = BMT_status.SOC;

	return 0;
}

int mtk_chr_get_ui_soc(unsigned int *ui_soc)
{
	/* UI_SOC2 is the one that shows on UI */
	if (BMT_status.UI_SOC2 < 0) {
		*ui_soc = 0;
		return -ENOTSUPP;
	}

	*ui_soc = BMT_status.UI_SOC2;

	return 0;
}

int mtk_chr_get_vbat(unsigned int *vbat)
{
	if (BMT_status.bat_vol < 0) {
		*vbat = 0;
		return -ENOTSUPP;
	}

	*vbat = BMT_status.bat_vol;

	return 0;
}

int mtk_chr_get_ibat(unsigned int *ibat)
{
	*ibat = BMT_status.IBattery / 10;
	return 0;
}

int mtk_chr_get_vbus(unsigned int *vbus)
{
	if (BMT_status.charger_vol < 0) {
		*vbus = 0;
		return -ENOTSUPP;
	}
	*vbus = BMT_status.charger_vol;

	return 0;
}

int mtk_chr_get_aicr(unsigned int *aicr)
{
	int ret = 0;
	u32 _aicr = 0;		/* 10uA */

	ret = battery_charging_control(CHARGING_CMD_GET_INPUT_CURRENT, &_aicr);
	*aicr = _aicr / 100;

	return ret;
}

int mtk_chr_is_charger_exist(unsigned char *exist)
{
	*exist = (BMT_status.charger_exist ? 1 : 0);
	return 0;
}

static unsigned int charging_full_check(void)
{
	unsigned int status;

	battery_charging_control(CHARGING_CMD_GET_CHARGING_STATUS, &status);
	if (status == true) {
		g_full_check_count++;
		if (g_full_check_count >= FULL_CHECK_TIMES)
			return true;
		else
			return false;
	}

	g_full_check_count = 0;
	return status;
}

static bool mtk_is_pep_series_connect(void)
{
	if (mtk_pep20_get_is_connect() || mtk_pep_get_is_connect())
		return true;

	return false;
}

static int mtk_check_aicr_upper_bound(void)
{
	u32 aicr_upper_bound = 0;	/* 10uA */

	if (mtk_is_pep_series_connect())
		return -EPERM;

	/* Check AICR upper bound gererated by AICL */
	aicr_upper_bound = g_aicr_upper_bound * 100;
	if (g_temp_input_CC_value > aicr_upper_bound && aicr_upper_bound > 0)
		g_temp_input_CC_value = aicr_upper_bound;

	return 0;
}

void select_charging_current(void)
{
	if (g_ftm_battery_flag) {
		battery_log(BAT_LOG_CRTI, "[BATTERY] FTM charging : %d\r\n",
			    charging_level_data[0]);
		g_temp_CC_value = charging_level_data[0];

		if (g_temp_CC_value == CHARGE_CURRENT_450_00_MA) {
			g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
		} else {
			g_temp_input_CC_value = CHARGE_CURRENT_MAX;
			g_temp_CC_value = batt_cust_data.ac_charger_current;

			battery_log(BAT_LOG_CRTI,
					"[BATTERY] set_ac_current \r\n");
		}
	} else {
		if (BMT_status.charger_type == STANDARD_HOST) {
#ifdef CONFIG_USB_IF
			{
				g_temp_input_CC_value = CHARGE_CURRENT_MAX;
				if (g_usb_state == USB_SUSPEND)
					g_temp_CC_value = USB_CHARGER_CURRENT_SUSPEND;
				else if (g_usb_state == USB_UNCONFIGURED)
					g_temp_CC_value =
					    batt_cust_data.usb_charger_current_unconfigured;
				else if (g_usb_state == USB_CONFIGURED)
					g_temp_CC_value =
					    batt_cust_data.usb_charger_current_configured;
				else
					g_temp_CC_value =
					    batt_cust_data.usb_charger_current_unconfigured;

				g_temp_input_CC_value = g_temp_CC_value;

				battery_log(BAT_LOG_CRTI,
					    "[BATTERY] STANDARD_HOST CC mode charging : %d on %d state\r\n",
					    g_temp_CC_value, g_usb_state);
			}
#else
			{
				g_temp_input_CC_value =
					batt_cust_data.usb_charger_current;
				g_temp_CC_value =
					batt_cust_data.usb_charger_current;
			}
#endif
		} else if (BMT_status.charger_type == NONSTANDARD_CHARGER) {
			g_temp_input_CC_value =
				batt_cust_data.non_std_ac_charger_current;
			g_temp_CC_value =
				batt_cust_data.non_std_ac_charger_current;

		} else if (BMT_status.charger_type == STANDARD_CHARGER) {
			if (batt_cust_data.ac_charger_input_current != 0)
				g_temp_input_CC_value =
					batt_cust_data.ac_charger_input_current;
			else
				g_temp_input_CC_value =
					batt_cust_data.ac_charger_current;

			g_temp_CC_value = batt_cust_data.ac_charger_current;
			mtk_pep_set_charging_current(
					&g_temp_CC_value, &g_temp_input_CC_value);
			mtk_pep20_set_charging_current(
					&g_temp_CC_value, &g_temp_input_CC_value);

		} else if (BMT_status.charger_type == CHARGING_HOST) {
			g_temp_input_CC_value =
				batt_cust_data.charging_host_charger_current;
			g_temp_CC_value =
				batt_cust_data.charging_host_charger_current;
		} else if (BMT_status.charger_type == APPLE_2_1A_CHARGER) {
			g_temp_input_CC_value =
				batt_cust_data.apple_2_1a_charger_current;
			g_temp_CC_value =
				batt_cust_data.apple_2_1a_charger_current;
		} else if (BMT_status.charger_type == APPLE_1_0A_CHARGER) {
			g_temp_input_CC_value =
				batt_cust_data.apple_1_0a_charger_current;
			g_temp_CC_value =
				batt_cust_data.apple_1_0a_charger_current;
		} else if (BMT_status.charger_type == APPLE_0_5A_CHARGER) {
			g_temp_input_CC_value =
				batt_cust_data.apple_0_5a_charger_current;
			g_temp_CC_value =
				batt_cust_data.apple_0_5a_charger_current;
		} else {
			g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
			g_temp_CC_value = CHARGE_CURRENT_500_00_MA;
		}

#if defined(CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT)
		if (DISO_data.diso_state.cur_vdc_state == DISO_ONLINE) {
			g_temp_input_CC_value =
				batt_cust_data.ac_charger_current;
			g_temp_CC_value =
				batt_cust_data.ac_charger_current;
		}
#endif

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
		set_jeita_charging_current();
#endif
	}

	mtk_check_aicr_upper_bound();
}

void select_charging_current_bcct(void)
{
/*BQ25896 is the first switch chrager separating input and charge current
 * any switch charger can use this compile option which may be generalized
 * to be CONFIG_SWITCH_INPUT_OUTPUT_CURRENT_SUPPORT
 */
#ifndef CONFIG_MTK_SWITCH_INPUT_OUTPUT_CURRENT_SUPPORT
	if ((BMT_status.charger_type == STANDARD_HOST) ||
	    (BMT_status.charger_type == NONSTANDARD_CHARGER)) {
		if (g_bcct_value < 100)
			g_temp_input_CC_value = CHARGE_CURRENT_0_00_MA;
		else if (g_bcct_value < 500)
			g_temp_input_CC_value = CHARGE_CURRENT_100_00_MA;
		else if (g_bcct_value < 800)
			g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
		else if (g_bcct_value == 800)
			g_temp_input_CC_value = CHARGE_CURRENT_800_00_MA;
		else
			g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
	} else if ((BMT_status.charger_type == STANDARD_CHARGER) ||
		   (BMT_status.charger_type == CHARGING_HOST)) {
		g_temp_input_CC_value = CHARGE_CURRENT_MAX;

		/* --------------------------------------------------- */
		/* set IOCHARGE */
		if (g_bcct_value < 550)
			g_temp_CC_value = CHARGE_CURRENT_0_00_MA;
		else if (g_bcct_value < 650)
			g_temp_CC_value = CHARGE_CURRENT_550_00_MA;
		else if (g_bcct_value < 750)
			g_temp_CC_value = CHARGE_CURRENT_650_00_MA;
		else if (g_bcct_value < 850)
			g_temp_CC_value = CHARGE_CURRENT_750_00_MA;
		else if (g_bcct_value < 950)
			g_temp_CC_value = CHARGE_CURRENT_850_00_MA;
		else if (g_bcct_value < 1050)
			g_temp_CC_value = CHARGE_CURRENT_950_00_MA;
		else if (g_bcct_value < 1150)
			g_temp_CC_value = CHARGE_CURRENT_1050_00_MA;
		else if (g_bcct_value < 1250)
			g_temp_CC_value = CHARGE_CURRENT_1150_00_MA;
		else if (g_bcct_value == 1250)
			g_temp_CC_value = CHARGE_CURRENT_1250_00_MA;
		else
			g_temp_CC_value = CHARGE_CURRENT_650_00_MA;
		/* --------------------------------------------------- */

	} else {
		g_temp_input_CC_value = CHARGE_CURRENT_500_00_MA;
	}
#else
	if (g_bcct_flag == 1)
		g_temp_CC_value = g_bcct_value * 100;
	if (g_bcct_input_flag == 1)
		g_temp_input_CC_value = g_bcct_input_value * 100;
#endif

	mtk_check_aicr_upper_bound();
}

static void mtk_select_ichg_aicr(void)
{
	bool enable_charger = true;

	mutex_lock(&g_ichg_aicr_access_mutex);

	/* Set Ichg, AICR */
	if (get_usb_current_unlimited()) {
		if (batt_cust_data.ac_charger_input_current != 0)
			g_temp_input_CC_value =
				batt_cust_data.ac_charger_input_current;
		else
			g_temp_input_CC_value =
				batt_cust_data.ac_charger_current;

		g_temp_CC_value = batt_cust_data.ac_charger_current;
		battery_log(BAT_LOG_FULL,
			    "USB_CURRENT_UNLIMITED, use batt_cust_data.ac_charger_current\n");
	}
#ifndef CONFIG_MTK_SWITCH_INPUT_OUTPUT_CURRENT_SUPPORT
	else if (g_bcct_flag == 1) {
		select_charging_current_bcct();
		battery_log(BAT_LOG_FULL,
				"[BATTERY] select_charging_current_bcct !\n");
	} else {
		select_charging_current();
		battery_log(BAT_LOG_FULL,
				"[BATTERY] select_charging_current !\n");
	}
#else
	else if (g_bcct_flag == 1 || g_bcct_input_flag == 1) {
		select_charging_current();
		select_charging_current_bcct();
		battery_log(BAT_LOG_FULL,
				"[BATTERY] select_charging_curret_bcct !\n");
	} else {
		select_charging_current();
		battery_log(BAT_LOG_FULL,
				"[BATTERY] select_charging_curret !\n");
	}
#endif
	battery_log(BAT_LOG_CRTI,
		    "[BATTERY] Default CC mode charging : %d, input current = %d\n",
		    g_temp_CC_value, g_temp_input_CC_value);

	battery_charging_control(
			CHARGING_CMD_SET_INPUT_CURRENT, &g_temp_input_CC_value);
	battery_charging_control(
			CHARGING_CMD_SET_CURRENT, &g_temp_CC_value);

	/* For thermal, they need to enable charger immediately */
	if (g_temp_CC_value > 0 && g_temp_input_CC_value > 0)
		battery_charging_control(CHARGING_CMD_ENABLE, &enable_charger);

	/* If AICR < 300mA, stop PE+/PE+20 */
	if (g_temp_input_CC_value < CHARGE_CURRENT_300_00_MA) {
		if (mtk_pep20_get_is_enable()) {
			mtk_pep20_set_is_enable(false);
			if (mtk_pep20_get_is_connect())
				mtk_pep20_reset_ta_vchr();
		}
		if (mtk_pep_get_is_enable()) {
			mtk_pep_set_is_enable(false);
			if (mtk_pep_get_is_connect())
				mtk_pep_reset_ta_vchr();
		}
	} else if (g_bcct_input_flag == 0 && g_bcct_flag == 0) {
		if (!mtk_pep20_get_is_enable()) {
			mtk_pep20_set_is_enable(true);
			mtk_pep20_set_to_check_chr_type(true);
		}
		if (!mtk_pep_get_is_enable()) {
			mtk_pep_set_is_enable(true);
			mtk_pep_set_to_check_chr_type(true);
		}
	}

	mutex_unlock(&g_ichg_aicr_access_mutex);
}

static void mtk_select_cv(void)
{
	int ret = 0;
	u32 dynamic_cv = 0;
	enum BATTERY_VOLTAGE_ENUM cv_voltage;

#ifdef CONFIG_MTK_JEITA_STANDARD_SUPPORT
	/* If temperautre is abnormal, return not permitted */
	if (g_temp_status != TEMP_POS_10_TO_POS_45)
		return;
#endif

	if (batt_cust_data.high_battery_voltage_support)
		cv_voltage = BATTERY_VOLT_04_340000_V;
	else
		cv_voltage = BATTERY_VOLT_04_200000_V;

	ret = mtk_get_dynamic_cv(&dynamic_cv);
	if (ret == 0) {
		cv_voltage = dynamic_cv * 1000;
		battery_log(BAT_LOG_FULL, "%s: set dynamic cv = %dmV\n",
				__func__, dynamic_cv);
	}

	battery_charging_control(CHARGING_CMD_SET_CV_VOLTAGE, &cv_voltage);

#if (CONFIG_MTK_GAUGE_VERSION == 20)
	g_cv_voltage = cv_voltage;
#endif
}

static void pchr_turn_on_charging(void)
{
	u32 charging_enable = true;

#ifdef CONFIG_MTK_DUAL_INPUT_CHARGER_SUPPORT
	if (BMT_status.charger_exist)
		charging_enable = true;
	else
		charging_enable = false;
#endif

	if (BMT_status.bat_charging_state == CHR_ERROR) {
		battery_log(BAT_LOG_CRTI,
			"[BATTERY] Charger Error, turn OFF charging !\n");
		charging_enable = false;
	} else if ((g_platform_boot_mode == META_BOOT) || (g_platform_boot_mode == ADVMETA_BOOT)) {
		battery_log(BAT_LOG_CRTI,
			"[BATTERY] In meta or advanced meta mode, disable charging.\n");
		charging_enable = false;
	} else {
		/* HW initialization */
		battery_charging_control(CHARGING_CMD_INIT, NULL);
		battery_log(BAT_LOG_FULL, "charging_hw_init\n");

		/* PE+/PE+20 algorithm */
		mtk_pep20_start_algorithm();
		mtk_pep_start_algorithm();

		/* Select ICHG/AICR */
		mtk_select_ichg_aicr();

		if (g_temp_CC_value == CHARGE_CURRENT_0_00_MA ||
		    g_temp_input_CC_value == CHARGE_CURRENT_0_00_MA) {
			charging_enable = false;
			battery_log(BAT_LOG_CRTI,
				    "[BATTERY] charging current is set 0mA, turn off charging !\r\n");
		} else		/* Set CV Voltage */
			mtk_select_cv();

	}

	/* enable/disable charging */
	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);

	battery_log(BAT_LOG_FULL,
		    "[BATTERY] pchr_turn_on_charging(), enable =%d !\r\n",
		    charging_enable);
}


unsigned int BAT_PreChargeModeAction(void)
{
#ifdef CONFIG_MTK_BIF_SUPPORT
	int ret = 0;
	bool bif_exist = false;
#endif
	unsigned int led_en = true;

	battery_log(BAT_LOG_CRTI,
			"[BATTERY] Pre-CC mode charge, timer=%d on %d !!\n\r",
			BMT_status.PRE_charging_time,
			BMT_status.total_charging_time);

	BMT_status.PRE_charging_time += BAT_TASK_PERIOD;
	BMT_status.CC_charging_time = 0;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.total_charging_time += BAT_TASK_PERIOD;

#ifdef CONFIG_MTK_BIF_SUPPORT
	/* If defined BIF but not BIF's battery, stop charging */
	ret = battery_charging_control(
			CHARGING_CMD_GET_BIF_IS_EXIST, &bif_exist);
	if (!bif_exist) {
		battery_log(BAT_LOG_CRTI,
			"%s: define BIF but no BIF battery, disable charging\n",
			__func__);
		BMT_status.bat_charging_state = CHR_ERROR;
		return PMU_STATUS_OK;
	}
#endif

	battery_charging_control(CHARGING_CMD_SET_PWRSTAT_LED_EN, &led_en);

	/*  Enable charger */
	pchr_turn_on_charging();
#if (CONFIG_MTK_GAUGE_VERSION == 20)
	if (BMT_status.UI_SOC2 == 100 && charging_full_check()) {
#else
	if (BMT_status.UI_SOC == 100) {
#endif
		BMT_status.bat_charging_state = CHR_BATFULL;
		BMT_status.bat_full = true;
		g_charging_full_reset_bat_meter = true;
	} else if (BMT_status.bat_vol > V_PRE2CC_THRES) {
		BMT_status.bat_charging_state = CHR_CC;
	}

	/* If it is not disabled by throttling,
	 * enable PE+/PE+20, if it is disabled
	 */
	if (g_bcct_input_flag && g_bcct_input_value < 300)
		return PMU_STATUS_OK;

	if (!mtk_pep20_get_is_enable()) {
		mtk_pep20_set_is_enable(true);
		mtk_pep20_set_to_check_chr_type(true);
	}
	if (!mtk_pep_get_is_enable()) {
		mtk_pep_set_is_enable(true);
		mtk_pep_set_to_check_chr_type(true);
	}

	return PMU_STATUS_OK;
}


unsigned int BAT_ConstantCurrentModeAction(void)
{
	unsigned int led_en = true;

	battery_log(BAT_LOG_CRTI,
			"[BATTERY] CC mode charge, timer=%d on %d!!\n",
			BMT_status.CC_charging_time,
			BMT_status.total_charging_time);

	BMT_status.PRE_charging_time = 0;
	BMT_status.CC_charging_time += BAT_TASK_PERIOD;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.total_charging_time += BAT_TASK_PERIOD;

	battery_charging_control(CHARGING_CMD_SET_PWRSTAT_LED_EN, &led_en);

	/*  Enable charger */
	pchr_turn_on_charging();

	if (charging_full_check() == true) {
		BMT_status.bat_charging_state = CHR_BATFULL;
		BMT_status.bat_full = true;
		g_charging_full_reset_bat_meter = true;
	}

	return PMU_STATUS_OK;
}


unsigned int BAT_BatteryFullAction(void)
{
	unsigned int led_en = false;

	battery_log(BAT_LOG_CRTI, "[BATTERY] Battery full !!\n\r");

	BMT_status.bat_full = true;
	BMT_status.total_charging_time = 0;
	BMT_status.PRE_charging_time = 0;
	BMT_status.CC_charging_time = 0;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.POSTFULL_charging_time = 0;
	BMT_status.bat_in_recharging_state = false;

	battery_log(BAT_LOG_FULL, "Turn off PWRSTAT LED\n");
	battery_charging_control(CHARGING_CMD_SET_PWRSTAT_LED_EN, &led_en);

	/*
	 * If CV is set to lower value by JEITA,
	 * Reset CV to normal value if temperture is in normal zone
	 */
	mtk_select_cv();

	if (charging_full_check() == false) {
		battery_log(BAT_LOG_CRTI, "[BATTERY] Battery Re-charging!\n\r");

		BMT_status.bat_in_recharging_state = true;
		BMT_status.bat_charging_state = CHR_CC;
#if (CONFIG_MTK_GAUGE_VERSION == 20)
		battery_meter_reset();
#endif
		mtk_pep20_set_to_check_chr_type(true);
		mtk_pep_set_to_check_chr_type(true);
		g_enable_dynamic_cv = true;
	}


	return PMU_STATUS_OK;
}


unsigned int BAT_BatteryHoldAction(void)
{
	unsigned int charging_enable;

	battery_log(BAT_LOG_CRTI, "[BATTERY] Hold mode !!\n\r");

	if (BMT_status.bat_vol < TALKING_RECHARGE_VOLTAGE || g_call_state == CALL_IDLE) {
			BMT_status.bat_charging_state = CHR_CC;
			battery_log(BAT_LOG_CRTI,
				"[BATTERY] Exit Hold mode and Enter CC mode !!\n\r");
	}

	/*  Disable charger */
	charging_enable = false;
	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);

	return PMU_STATUS_OK;
}


unsigned int BAT_BatteryStatusFailAction(void)
{
	unsigned int charging_enable;

	battery_log(BAT_LOG_CRTI,
			"[BATTERY] BAD Battery status... Charging Stop !!\n\r");

#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	if ((g_temp_status == TEMP_ABOVE_POS_60) ||
		(g_temp_status == TEMP_BELOW_NEG_10))
			temp_error_recovery_chr_flag = false;

	if ((temp_error_recovery_chr_flag == false) &&
		(g_temp_status != TEMP_ABOVE_POS_60) &&
		(g_temp_status != TEMP_BELOW_NEG_10)) {
			temp_error_recovery_chr_flag = true;
			BMT_status.bat_charging_state = CHR_PRE;
	}
#endif

	BMT_status.total_charging_time = 0;
	BMT_status.PRE_charging_time = 0;
	BMT_status.CC_charging_time = 0;
	BMT_status.TOPOFF_charging_time = 0;
	BMT_status.POSTFULL_charging_time = 0;

	/*  Disable charger */
	charging_enable = false;
	battery_charging_control(CHARGING_CMD_ENABLE, &charging_enable);

	/* Disable PE+/PE+20 */
	if (mtk_pep20_get_is_enable()) {
		mtk_pep20_set_is_enable(false);
		if (mtk_pep20_get_is_connect())
			mtk_pep20_reset_ta_vchr();
	}
	if (mtk_pep_get_is_enable()) {
		mtk_pep_set_is_enable(false);
		if (mtk_pep_get_is_connect())
			mtk_pep_reset_ta_vchr();
	}

	return PMU_STATUS_OK;
}


void mt_battery_charging_algorithm(void)
{
	battery_charging_control(CHARGING_CMD_RESET_WATCH_DOG_TIMER, NULL);

	/* Generate AICR upper bound by AICL */
	if (!mtk_is_pep_series_connect()) {
		battery_charging_control(
				CHARGING_CMD_RUN_AICL, &g_aicr_upper_bound);
	}

	mtk_pep20_check_charger();
	mtk_pep_check_charger();
	switch (BMT_status.bat_charging_state) {
	case CHR_PRE:
		BAT_PreChargeModeAction();
		break;

	case CHR_CC:
		BAT_ConstantCurrentModeAction();
		break;

	case CHR_BATFULL:
		BAT_BatteryFullAction();
		break;

	case CHR_HOLD:
		BAT_BatteryHoldAction();
		break;

	case CHR_ERROR:
		BAT_BatteryStatusFailAction();
		break;
	}

	battery_charging_control(CHARGING_CMD_DUMP_REGISTER, NULL);
}
