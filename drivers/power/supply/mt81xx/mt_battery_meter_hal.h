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

#ifndef _BATTERY_METER_HAL_H
#define _BATTERY_METER_HAL_H

enum BATTERY_METER_CTRL_CMD {
	BATTERY_METER_CMD_HW_FG_INIT,
	BATTERY_METER_CMD_GET_HW_FG_CURRENT,
	BATTERY_METER_CMD_GET_HW_FG_CURRENT_SIGN,
	BATTERY_METER_CMD_GET_HW_FG_CAR,
	BATTERY_METER_CMD_HW_RESET,
	BATTERY_METER_CMD_GET_ADC_V_BAT_SENSE,
	BATTERY_METER_CMD_GET_ADC_V_I_SENSE,
	BATTERY_METER_CMD_GET_ADC_V_BAT_TEMP,
	BATTERY_METER_CMD_GET_ADC_V_CHARGER,
	BATTERY_METER_CMD_GET_HW_OCV,
	BATTERY_METER_CMD_DUMP_REGISTER,
	BATTERY_METER_CMD_NUMBER
};

typedef s32 (*BATTERY_METER_CONTROL)(int cmd, void *data);

extern s32 bm_ctrl_cmd(int cmd, void *data);

#endif /* #ifndef _BATTERY_METER_HAL_H */
