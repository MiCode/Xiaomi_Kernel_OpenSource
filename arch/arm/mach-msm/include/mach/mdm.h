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

#ifndef _ARCH_ARM_MACH_MSM_MDM_H
#define _ARXH_ARM_MACH_MSM_MDM_H


struct charm_platform_data {
	void (*charm_modem_on)(void);
	void (*charm_modem_off)(void);
};

#define AP2MDM_STATUS   136
#define MDM2AP_STATUS   134
#define MDM2AP_WAKEUP   40
#define MDM2AP_ERRFATAL 133
#define AP2MDM_ERRFATAL 93

#define AP2MDM_PMIC_RESET_N     131
#define AP2MDM_KPDPWR_N 132
#define AP2PMIC_TMPNI_CKEN      141
#define AP2MDM_WAKEUP	135

extern void (*charm_intentional_reset)(void);

#endif
