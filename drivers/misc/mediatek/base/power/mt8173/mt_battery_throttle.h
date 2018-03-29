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


#ifndef _MT_BATTERY_THROTTLE_
#define _MT_BATTERY_THROTTLE_

extern bool mtk_get_gpu_power_loading(unsigned int *pLoading);
extern int mt_cpufreq_thermal_protect(unsigned int limited_power, unsigned int limitor_index);
extern bool upmu_is_chr_det(void);
extern s32 battery_meter_get_battery_zcv(void);
extern s32 battery_meter_get_batteryR(void);
extern s32 battery_meter_get_battery_voltage_cached(void);
extern s32 battery_meter_get_average_battery_voltage(void);

#define BATTERY_MAX_BUDGET  (-1)
#define BATTERY_MIN_BUDGET  430
#define BATTERY_MAX_BUDGET_FACTOR  10
#define BATTERY_MIN_BUDGET_FACTOR  0
#define BATTERY_BUDGET_MIN_VOLTAGE 3450
#define BATTERY_BUDGET_TOLERANCE_VOLTAGE 50
#define BATTERY_LIMITOR 1
#define VBAT_LOWER_BOUND 3100
#define WORK_INTERVAL 10000

enum throttle_mode {
	TH_NORMAL = 0,
	TH_BUDGET = 1,
	TH_DISABLE = 2,
};

#endif
