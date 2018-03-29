/*
 *  Richtek Charger Interface for Mediatek
 *
 *  Copyright (C) 2015 Richtek Technology Corp.
 *  ShuFanLee <shufan_lee@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/platform_device.h>
#include "mt_charging.h"
#include <mt-plat/upmu_common.h>
#include <mt-plat/mt_boot.h>
#include <mt-plat/mt_reboot.h>
#include <mt-plat/mt_gpio.h>

#include <linux/delay.h>
#include <linux/reboot.h>

#include "mtk_charger_intf.h"

#define STATUS_OK	0

/* Necessary functions for integrating with MTK */
/* All of them are copied from original source code of MTK */

#define GETARRAYNUM(array) (sizeof(array)/sizeof(array[0]))

/* for MT6391 */
static const u32 VCDT_HV_VTH[] = {
	BATTERY_VOLT_04_000000_V, BATTERY_VOLT_04_100000_V, BATTERY_VOLT_04_150000_V,
	    BATTERY_VOLT_04_200000_V,
	BATTERY_VOLT_04_250000_V, BATTERY_VOLT_04_300000_V, BATTERY_VOLT_04_350000_V,
	    BATTERY_VOLT_04_400000_V,
	BATTERY_VOLT_04_450000_V, BATTERY_VOLT_04_500000_V, BATTERY_VOLT_04_550000_V,
	    BATTERY_VOLT_04_600000_V,
	BATTERY_VOLT_07_000000_V, BATTERY_VOLT_07_500000_V, BATTERY_VOLT_08_500000_V,
	    BATTERY_VOLT_10_500000_V
};

static u32 charging_parameter_to_value(const u32 *parameter, const u32 array_size, const u32 val)
{
	u32 i;

	for (i = 0; i < array_size; i++) {
		if (val == *(parameter + i))
			return i;
	}

	battery_log(BAT_LOG_CRTI, "NO register value match. val=%d\r\n", val);
	/* TODO: ASSERT(0);      // not find the value */
	return 0;
}

static u32 bmt_find_closest_level(const u32 *pList, u32 number, u32 level)
{
	u32 i;
	u32 max_value_in_last_element;

	if (pList[0] < pList[1])
		max_value_in_last_element = true;
	else
		max_value_in_last_element = false;

	if (max_value_in_last_element == true) {
		for (i = (number - 1); i != 0; i--) {	/* max value in the last element */
			if (pList[i] <= level)
				return pList[i];
		}

		battery_log(BAT_LOG_CRTI, "Can't find closest level, small value first \r\n");
		return pList[0];
	}

	for (i = 0; i < number; i++) {	/* max value in the first element */
		if (pList[i] <= level)
			return pList[i];
	}

	battery_log(BAT_LOG_CRTI, "Can't find closest level, large value first \r\n");
	return pList[number - 1];

}

/* The following functions are for chr_control_interface */


int mtk_charger_set_hv_threshold(void *data)
{
	u32 status = STATUS_OK;

	u32 set_hv_voltage;
	u32 array_size;
	u16 register_value;
	u32 voltage = *(u32 *) (data);

	array_size = GETARRAYNUM(VCDT_HV_VTH);
	set_hv_voltage = bmt_find_closest_level(VCDT_HV_VTH, array_size, voltage);
	register_value = charging_parameter_to_value(VCDT_HV_VTH, array_size, set_hv_voltage);
	upmu_set_rg_vcdt_hv_vth(register_value);

	return status;
}

int mtk_charger_get_hv_status(void *data)
{
	u32 status = STATUS_OK;

	*(bool *) (data) = upmu_get_rgs_vcdt_hv_det();
	return status;
}

int mtk_charger_get_battery_status(void *data)
{
	u32 status = STATUS_OK;

	/* upmu_set_baton_tdet_en(1); */
	upmu_set_rg_baton_en(1);
	*(bool *) (data) = upmu_get_rgs_baton_undet();

	return status;
}

int mtk_charger_get_charger_det_status(void *data)
{
	u32 status = STATUS_OK;

	*(bool *) (data) = upmu_get_rgs_chrdet();

	return status;
}

int mtk_charger_get_charger_type(void *data)
{
	u32 status = STATUS_OK;

	*(int *)(data) = hw_charger_type_detection();

	return status;
}

int mtk_charger_get_is_pcm_timer_trigger(void *data)
{
	u32 status = STATUS_OK;

	if (slp_get_wake_reason() == 3)
		*(bool *) (data) = true;
	else
		*(bool *) (data) = false;

	battery_log(BAT_LOG_CRTI, "slp_get_wake_reason=%d\n", slp_get_wake_reason());

	*(bool *) (data) = false;
	return status;

}

int mtk_charger_set_platform_reset(void *data)
{
	u32 status = STATUS_OK;

	battery_log(BAT_LOG_CRTI, "charging_set_platform_reset\n");

	if (system_state == SYSTEM_BOOTING)
		arch_reset(0, NULL);
	else
		orderly_reboot(true);

	return status;

}

int mtk_charger_get_platform_boot_mode(void *data)
{
	u32 status = STATUS_OK;

	*(u32 *) (data) = get_boot_mode();

	battery_log(BAT_LOG_CRTI, "get_boot_mode=%d\n", get_boot_mode());

	return status;

}

int(*mtk_charger_intf[CHARGING_CMD_NUMBER])(void *data);
int chr_control_interface(int cmd, void *data)
{
	int ret = 0;

	if (cmd < CHARGING_CMD_NUMBER) {
		if (mtk_charger_intf[cmd] != NULL)
			ret = mtk_charger_intf[cmd](data);
		else
			ret = -ENOTSUPP;
	} else
		ret = -ENOTSUPP;

	if (ret == -ENOTSUPP)
		battery_log(BAT_LOG_CRTI, "%s: function %d is not support\n",
			__func__, cmd);
	else if (ret < 0)
		battery_log(BAT_LOG_CRTI, "%s: function %d failed, ret = %d\n",
			__func__, cmd, ret);

	return ret;
}
