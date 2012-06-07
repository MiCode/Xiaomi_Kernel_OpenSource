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

/*
 * Qualcomm PMIC PM8xxx Thermal Manager driver
 */

#ifndef __PM8XXX_TM_H
#define __PM8XXX_TM_H

#include <linux/errno.h>

#define PM8XXX_TM_DEV_NAME	"pm8xxx-tm"

enum pm8xxx_tm_adc_type {
	PM8XXX_TM_ADC_NONE,	/* Estimates temp based on overload level. */
	PM8XXX_TM_ADC_PM8058_ADC,
	PM8XXX_TM_ADC_PM8XXX_ADC,
};

struct pm8xxx_tm_core_data {
	int				adc_channel;
	unsigned long			default_no_adc_temp;
	enum pm8xxx_tm_adc_type		adc_type;
	u16				reg_addr_temp_alarm_ctrl;
	u16				reg_addr_temp_alarm_pwm;
	char				*tm_name;
	char				*irq_name_temp_stat;
	char				*irq_name_over_temp;
};

#endif
