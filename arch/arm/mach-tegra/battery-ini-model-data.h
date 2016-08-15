/*
 * battery-ini-model-data.h: Battery INI model data for different platforms.
 *
 * Copyright (c) 2013, NVIDIA Corporation. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _MACH_TEGRA_BATTERY_INI_MODEL_DATA_H
#define _MACH_TEGRA_BATTERY_INI_MODEL_DATA_H

#include <linux/max17048_battery.h>
#include <linux/power/max17042_battery.h>

#define NTC_10K_TGAIN	0xE6A2
#define NTC_10K_TOFF	0x2694
#define NTC_68K_TGAIN	0xE665
#define NTC_68K_TOFF	0x26C1

/*
 * Battery model data for YOKU 4100mA for MAX17048 for Macallan.
 * System shutdown voltage: 3.0V
 * INI Files: 1283683
 */
extern struct max17048_battery_model macallan_yoku_4100mA_max17048_battery;

/*
 * Battery model data for YOKU 3900mA for MAX17048 for Macallan.
 * System shutdown voltage: 3.0V
 * INI Files: 1283683
 */
extern struct max17048_battery_model macallan_yoku_3900mA_max17048_battery;

/*
 * Battery model data for YOKU 2000mA for MAX17042 for Pluto.
 * System shutdown voltage: 2.9V
 * INI Files: 1264825
 */
extern struct max17042_config_data pluto_yoku_2000mA_max17042_battery;

extern struct max17042_config_data pisces_conf_data_samsung;
extern struct max17042_config_data pisces_conf_data_sony;
extern struct max17042_config_data pisces_conf_data_lg;
#endif
