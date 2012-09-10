/*
 * Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

/**
 * enum pm8xxx_tm_adc_type - support ADC API types for PMIC thermal manager
 * %PM8XXX_TM_ADC_NONE:		Do not call any ADC API and instead estimate
 *				PMIC temerature based on over temperature stage.
 * %PM8XXX_TM_ADC_PM8058_ADC:	Use the pmic8058-xoadc ADC API
 * %PM8XXX_TM_ADC_PM8XXX_ADC:	Use the pm8xxx-adc ADC API
 */
enum pm8xxx_tm_adc_type {
	PM8XXX_TM_ADC_NONE,
	PM8XXX_TM_ADC_PM8058_ADC,
	PM8XXX_TM_ADC_PM8XXX_ADC,
};

/**
 * struct pm8xxx_tm_core_data - PM8XXX thermal manager core data
 * @tm_name:			Thermal zone name for the device
 * @irq_name_temp_stat:		String name used to identify TEMP_STAT IRQ
 * @irq_name_over_temp:		String name used to identify OVER_TEMP IRQ
 * @reg_addr_temp_alarm_ctrl:	PMIC SSBI address for temp alarm control
 *				register
 * @reg_addr_temp_alarm_pwm:	PMIC SSBI address for temp alarm pwm register
 * @adc_type:			Determines which ADC API to use in order to read
 *				the PMIC die temperature.
 * @adc_channel:		ADC channel identifier
 *				If adc_type == PM8XXX_TM_ADC_PM8XXX_ADC, then
 *				use a value from enum pm8xxx_adc_channels.
 *				If adc_type == PM8XXX_TM_ADC_PM8058_ADC, then
 *				use a channel value specified in
 *				<linux/pmic8058-xoadc.h>
 * @default_no_adc_temp:	Default temperature in millicelcius to report
 *				while stage == 0 and stage has never been
 *				greater than 0 if adc_type == PM8XXX_TM_ADC_NONE
 * @allow_software_override:	true --> writing "enabled" to thermalfs mode
 *				file results in software override of PMIC
 *				automatic over temperature shutdown
 *				false --> PMIC automatic over temperature
 *				shutdown always enabled.  mode file cannot be
 *				set to "enabled".
 */
struct pm8xxx_tm_core_data {
	char			*tm_name;
	char			*irq_name_temp_stat;
	char			*irq_name_over_temp;
	u16			reg_addr_temp_alarm_ctrl;
	u16			reg_addr_temp_alarm_pwm;
	enum pm8xxx_tm_adc_type	adc_type;
	int			adc_channel;
	unsigned long		default_no_adc_temp;
	bool			allow_software_override;
};

#endif
