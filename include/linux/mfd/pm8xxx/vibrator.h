/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __PMIC8XXX_VIBRATOR_H__
#define __PMIC8XXX_VIBRATOR_H__

#define PM8XXX_VIBRATOR_DEV_NAME "pm8xxx-vib"

struct pm8xxx_vibrator_platform_data {
	int initial_vibrate_ms;
	int max_timeout_ms;
	int level_mV;
};

#endif /* __PMIC8XXX_VIBRATOR_H__ */
