
/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
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

#ifndef MIPI_TOSHIBA_H
#define MIPI_TOSHIBA_H

#include <linux/pwm.h>
#include <linux/mfd/pm8xxx/pm8921.h>

int mipi_toshiba_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel);

#define MIPI_TOSHIBA_PWM_FREQ_HZ 3921
#define MIPI_TOSHIBA_PWM_PERIOD_USEC (USEC_PER_SEC / MIPI_TOSHIBA_PWM_FREQ_HZ)
#define MIPI_TOSHIBA_PWM_LEVEL 255
#define MIPI_TOSHIBA_PWM_DUTY_LEVEL \
	(MIPI_TOSHIBA_PWM_PERIOD_USEC / MIPI_TOSHIBA_PWM_LEVEL)

#endif  /* MIPI_TOSHIBA_H */
