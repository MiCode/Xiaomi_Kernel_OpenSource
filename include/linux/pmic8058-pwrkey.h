/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __PMIC8058_PWRKEY_H__
#define __PMIC8058_PWRKEY_H__

struct pmic8058_pwrkey_pdata {
	bool pull_up;
	/* time after which pwr key event should be generated, if key is
	 * released before that then end key event is reported
	 */
	u16  pwrkey_time_ms;
	/* time delay for pwr-key state change
	 * interrupt triggering.
	 */
	u32  kpd_trigger_delay_us;
	u32  wakeup;
};

#endif /* __PMIC8058_PWRKEY_H__ */
