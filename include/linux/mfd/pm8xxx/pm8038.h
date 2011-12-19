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
 * Qualcomm PMIC 8038 driver header file
 *
 */

#ifndef __MFD_PM8038_H
#define __MFD_PM8038_H

#include <linux/device.h>
#include <linux/mfd/pm8xxx/irq.h>
#include <linux/mfd/pm8xxx/gpio.h>
#include <linux/mfd/pm8xxx/mpp.h>
#include <linux/mfd/pm8xxx/pwm.h>
#include <linux/mfd/pm8xxx/rtc.h>
#include <linux/input/pmic8xxx-pwrkey.h>
#include <linux/mfd/pm8xxx/misc.h>
#include <linux/regulator/pm8xxx-regulator.h>
#include <linux/mfd/pm8xxx/pm8xxx-adc.h>

#define PM8038_CORE_DEV_NAME "pm8038-core"

#define PM8038_NR_IRQS		256
#define PM8038_NR_GPIOS		12
#define PM8038_NR_MPPS		6

#define PM8038_GPIO_BLOCK_START	24
#define PM8038_MPP_BLOCK_START	16

#define PM8038_IRQ_BLOCK_BIT(block, bit) ((block) * 8 + (bit))

/* GPIO and MPPs [1,N] */
#define PM8038_GPIO_IRQ(base, gpio)	((base) + \
		PM8038_IRQ_BLOCK_BIT(PM8038_GPIO_BLOCK_START, (gpio)-1))
#define PM8038_MPP_IRQ(base, mpp)	((base) + \
		PM8038_IRQ_BLOCK_BIT(PM8038_MPP_BLOCK_START, (mpp)-1))

/* PMIC Interrupts */
#define PM8038_RTC_ALARM_IRQ		PM8038_IRQ_BLOCK_BIT(4, 7)
#define PM8038_PWRKEY_REL_IRQ		PM8038_IRQ_BLOCK_BIT(6, 2)
#define PM8038_PWRKEY_PRESS_IRQ		PM8038_IRQ_BLOCK_BIT(6, 3)
#define PM8038_KEYPAD_IRQ		PM8038_IRQ_BLOCK_BIT(9, 2)
#define PM8038_KEYSTUCK_IRQ		PM8038_IRQ_BLOCK_BIT(9, 3)
#define PM8038_ADC_EOC_USR_IRQ		PM8038_IRQ_BLOCK_BIT(9, 6)
#define PM8038_ADC_BATT_TEMP_WARM_IRQ	PM8038_IRQ_BLOCK_BIT(9, 1)
#define PM8038_ADC_BATT_TEMP_COLD_IRQ	PM8038_IRQ_BLOCK_BIT(9, 0)
#define PM8038_USB_ID_IN_IRQ(base)	(base + PM8921_IRQ_BLOCK_BIT(6, 1))

#define PM8038_RESOUT_IRQ		PM8038_IRQ_BLOCK_BIT(6, 4)

struct pm8038_platform_data {
	int					irq_base;
	struct pm8xxx_gpio_platform_data	*gpio_pdata;
	struct pm8xxx_irq_platform_data		*irq_pdata;
	struct pm8xxx_mpp_platform_data		*mpp_pdata;
	struct pm8xxx_rtc_platform_data         *rtc_pdata;
	struct pm8xxx_pwrkey_platform_data	*pwrkey_pdata;
	struct pm8xxx_misc_platform_data	*misc_pdata;
	struct pm8xxx_regulator_platform_data	*regulator_pdatas;
	int					num_regulators;
	struct pm8xxx_adc_platform_data		*adc_pdata;
};

#endif
