/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
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

#ifndef __PMIC8XXX_PWRKEY_H__
#define __PMIC8XXX_PWRKEY_H__

#define PM8XXX_PWRKEY_DEV_NAME "pm8xxx-pwrkey"

/**
 * struct pm8xxx_pwrkey_platform_data - platform data for pwrkey driver
 * @pull up:  power on register control for pull up/down configuration
 * @kpd_trigger_delay_us: time delay for power key state change interrupt
 *                  trigger.
 * @wakeup: configure power key as wakeup source
 */
struct pm8xxx_pwrkey_platform_data  {
	bool pull_up;
	/* Time delay for pwr-key state change interrupt triggering in micro-
	 * second. The actual delay can only be one of these eight levels:
	 * 2 sec, 1 sec, 1/2 sec, 1/4 sec, 1/8 sec, 1/16 sec, 1/32 sec, and
	 * 1/64 sec. The valid range of kpd_trigger_delay_us is 1/64 second to
	 * 2 seconds. A value within the valid range will be rounded down to the
	 * closest level. Any value outside the valid range will be rejected.
	 */
	u32  kpd_trigger_delay_us;
	u32  wakeup;
};

#endif /* __PMIC8XXX_PWRKEY_H__ */
