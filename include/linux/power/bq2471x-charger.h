/*
 * bq2471x-charger.h -- BQ2471X Charger driver
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Andy Park <andyp@nvidia.com>
 * Author: Syed Rafiuddin <srafiuddin@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#ifndef __LINUX_POWER_BQ2471X_CHARGER_H
#define __LINUX_POWER_BQ2471X_CHARGER_H

/* Register definitions */
#define BQ2471X_CHARGE_OPTION		0x12
#define BQ2471X_CHARGE_CURRENT		0x14
#define BQ2471X_MAX_CHARGE_VOLTAGE	0x15
#define BQ2471X_MIN_SYS_VOLTAGE		0x3E
#define BQ2471X_INPUT_CURRENT		0x3F
#define BQ2471X_MANUFACTURER_ID_REG	0xFE
#define BQ2471X_DEVICE_ID_REG		0xFF

#define BQ2471X_MANUFACTURER_ID		0x4000

#define BQ24715_DEVICE_ID		0x1000
#define BQ24717_DEVICE_ID		0x1200

#define BQ2471X_CHARGE_OPTION_POR	0x44E1

#define BQ2471X_CHARGE_CURRENT_SHIFT		6
#define BQ2471X_MAX_CHARGE_VOLTAGE_SHIFT	4
#define BQ2471X_MIN_SYS_VOLTAGE_SHIFT		8
#define BQ2471X_INPUT_CURRENT_SHIFT		6

#define BQ2471X_ENABLE_CHARGE_MASK	BIT(0)
#define BQ2471X_WATCHDOG_TIMER		0x6000

#define BQ2471X_MAX_REGS		(BQ2471X_DEVICE_ID_REG + 1)

struct bq2471x_platform_data {
	int     irq;
	int     dac_ctrl;
	int     dac_ichg;
	int     dac_v;
	int     dac_minsv;
	int     dac_iin;
	int     wdt_refresh_timeout;
	int     gpio;
	int	charge_broadcast_mode;
	int	gpio_active_low;
};
#endif /* __LINUX_POWER_BQ2471X_CHARGER_H */
