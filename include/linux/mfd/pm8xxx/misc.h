/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#ifndef __MFD_PM8XXX_MISC_H__
#define __MFD_PM8XXX_MISC_H__

#include <linux/err.h>

#define PM8XXX_MISC_DEV_NAME	"pm8xxx-misc"

/**
 * struct pm8xxx_misc_platform_data - PM8xxx misc driver platform data
 * @priority:	PMIC prority level in a multi-PMIC system. Lower value means
 *		greater priority. Actions are performed from highest to lowest
 *		priority PMIC.
 */
struct pm8xxx_misc_platform_data {
	int	priority;
};

#if defined(CONFIG_MFD_PM8XXX_MISC) || defined(CONFIG_MFD_PM8XXX_MISC_MODULE)

/**
 * pm8xxx_reset_pwr_off - switch all PM8XXX PMIC chips attached to the system to
 *			  either reset or shutdown when they are turned off
 * @reset: 0 = shudown the PMICs, 1 = shutdown and then restart the PMICs
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_reset_pwr_off(int reset);

#else

static inline int pm8xxx_reset_pwr_off(int reset)
{
	return -ENODEV;
}

#endif

#endif
