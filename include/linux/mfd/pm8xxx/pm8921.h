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
 * Qualcomm PMIC 8921 driver header file
 *
 */

#ifndef __MFD_PM8921_H
#define __MFD_PM8921_H

#include <linux/device.h>
#include <linux/mfd/pm8xxx/irq.h>
#include <linux/mfd/pm8xxx/gpio.h>
#include <linux/mfd/pm8xxx/mpp.h>
#include <linux/mfd/pm8xxx/rtc.h>
#include <linux/mfd/pm8xxx/pwm.h>
#include <linux/mfd/pm8xxx/misc.h>
#include <linux/mfd/pm8xxx/tm.h>
#include <linux/mfd/pm8xxx/batt-alarm.h>
#include <linux/input/pmic8xxx-pwrkey.h>
#include <linux/input/pmic8xxx-keypad.h>
#include <linux/regulator/pm8921-regulator.h>
#include <linux/mfd/pm8xxx/pm8921-charger.h>
#include <linux/mfd/pm8921-adc.h>
#include <linux/mfd/pm8xxx/pm8921-bms.h>
#include <linux/leds.h>

#define PM8921_NR_IRQS		256

#define PM8921_NR_GPIOS		44

#define PM8921_NR_MPPS		12

#define PM8921_GPIO_BLOCK_START	24
#define PM8921_MPP_BLOCK_START	16
#define PM8921_IRQ_BLOCK_BIT(block, bit) ((block) * 8 + (bit))

/* GPIOs and MPPs [1,N] */
#define PM8921_GPIO_IRQ(base, gpio)	((base) + \
		PM8921_IRQ_BLOCK_BIT(PM8921_GPIO_BLOCK_START, (gpio)-1))
#define PM8921_MPP_IRQ(base, mpp)	((base) + \
		PM8921_IRQ_BLOCK_BIT(PM8921_MPP_BLOCK_START, (mpp)-1))

/* PMIC Interrupts */
#define PM8921_RTC_ALARM_IRQ		PM8921_IRQ_BLOCK_BIT(4, 7)
#define PM8921_BATT_ALARM_IRQ		PM8921_IRQ_BLOCK_BIT(5, 6)
#define PM8921_PWRKEY_REL_IRQ		PM8921_IRQ_BLOCK_BIT(6, 2)
#define PM8921_PWRKEY_PRESS_IRQ		PM8921_IRQ_BLOCK_BIT(6, 3)
#define PM8921_KEYPAD_IRQ		PM8921_IRQ_BLOCK_BIT(9, 2)
#define PM8921_KEYSTUCK_IRQ		PM8921_IRQ_BLOCK_BIT(9, 3)
#define PM8921_ADC_EOC_USR_IRQ		PM8921_IRQ_BLOCK_BIT(9, 6)
#define PM8921_ADC_BATT_TEMP_WARM_IRQ	PM8921_IRQ_BLOCK_BIT(9, 1)
#define PM8921_ADC_BATT_TEMP_COLD_IRQ	PM8921_IRQ_BLOCK_BIT(9, 0)
#define PM8921_USB_ID_IN_IRQ(base)	(base + PM8921_IRQ_BLOCK_BIT(6, 1))

#define PM8921_USBIN_VALID_IRQ		PM8921_IRQ_BLOCK_BIT(1, 7)
#define PM8921_USBIN_OV_IRQ		PM8921_IRQ_BLOCK_BIT(1, 6)
#define PM8921_BATT_INSERTED_IRQ	PM8921_IRQ_BLOCK_BIT(1, 5)
#define PM8921_VBATDET_LOW_IRQ		PM8921_IRQ_BLOCK_BIT(1, 4)
#define PM8921_USBIN_UV_IRQ		PM8921_IRQ_BLOCK_BIT(1, 3)
#define PM8921_VBAT_OV_IRQ		PM8921_IRQ_BLOCK_BIT(1, 2)
#define PM8921_CHGWDOG_IRQ		PM8921_IRQ_BLOCK_BIT(1, 1)
#define PM8921_VCP_IRQ			PM8921_IRQ_BLOCK_BIT(1, 0)
#define PM8921_ATCDONE_IRQ		PM8921_IRQ_BLOCK_BIT(2, 7)
#define PM8921_ATCFAIL_IRQ		PM8921_IRQ_BLOCK_BIT(2, 6)
#define PM8921_CHGDONE_IRQ		PM8921_IRQ_BLOCK_BIT(2, 5)
#define PM8921_CHGFAIL_IRQ		PM8921_IRQ_BLOCK_BIT(2, 4)
#define PM8921_CHGSTATE_IRQ		PM8921_IRQ_BLOCK_BIT(2, 3)
#define PM8921_LOOP_CHANGE_IRQ		PM8921_IRQ_BLOCK_BIT(2, 2)
#define PM8921_FASTCHG_IRQ		PM8921_IRQ_BLOCK_BIT(2, 1)
#define PM8921_TRKLCHG_IRQ		PM8921_IRQ_BLOCK_BIT(2, 0)
#define PM8921_BATT_REMOVED_IRQ		PM8921_IRQ_BLOCK_BIT(3, 7)
#define PM8921_BATTTEMP_HOT_IRQ		PM8921_IRQ_BLOCK_BIT(3, 6)
#define PM8921_CHGHOT_IRQ		PM8921_IRQ_BLOCK_BIT(3, 5)
#define PM8921_BATTTEMP_COLD_IRQ	PM8921_IRQ_BLOCK_BIT(3, 4)
#define PM8921_CHG_GONE_IRQ		PM8921_IRQ_BLOCK_BIT(3, 3)
#define PM8921_BAT_TEMP_OK_IRQ		PM8921_IRQ_BLOCK_BIT(3, 2)
#define PM8921_COARSE_DET_LOW_IRQ	PM8921_IRQ_BLOCK_BIT(3, 1)
#define PM8921_VDD_LOOP_IRQ		PM8921_IRQ_BLOCK_BIT(3, 0)
#define PM8921_VREG_OV_IRQ		PM8921_IRQ_BLOCK_BIT(5, 7)
#define PM8921_VBATDET_IRQ		PM8921_IRQ_BLOCK_BIT(5, 5)
#define PM8921_BATFET_IRQ		PM8921_IRQ_BLOCK_BIT(5, 4)
#define PM8921_PSI_IRQ			PM8921_IRQ_BLOCK_BIT(5, 3)
#define PM8921_DCIN_VALID_IRQ		PM8921_IRQ_BLOCK_BIT(5, 2)
#define PM8921_DCIN_OV_IRQ		PM8921_IRQ_BLOCK_BIT(5, 1)
#define PM8921_DCIN_UV_IRQ		PM8921_IRQ_BLOCK_BIT(5, 0)

#define PM8921_BMS_SBI_WRITE_OK		PM8921_IRQ_BLOCK_BIT(15, 7)
#define PM8921_BMS_CC_THR		PM8921_IRQ_BLOCK_BIT(15, 6)
#define PM8921_BMS_VSENSE_THR		PM8921_IRQ_BLOCK_BIT(15, 5)
#define PM8921_BMS_VSENSE_FOR_R		PM8921_IRQ_BLOCK_BIT(15, 4)
#define PM8921_BMS_OCV_FOR_R		PM8921_IRQ_BLOCK_BIT(15, 3)
#define PM8921_BMS_GOOD_OCV		PM8921_IRQ_BLOCK_BIT(15, 2)
#define PM8921_BMS_VSENSE_AVG		PM8921_IRQ_BLOCK_BIT(15, 1)
#define PM8921_BMS_CCADC_EOC		PM8921_IRQ_BLOCK_BIT(15, 0)

#define PM8921_OVERTEMP_IRQ		PM8921_IRQ_BLOCK_BIT(4, 2)
#define PM8921_TEMPSTAT_IRQ		PM8921_IRQ_BLOCK_BIT(6, 7)

/* PMIC I/O Resources */
#define PM8921_RTC_BASE 0x11D

struct pm8921_platform_data {
	int					irq_base;
	struct pm8xxx_irq_platform_data		*irq_pdata;
	struct pm8xxx_gpio_platform_data	*gpio_pdata;
	struct pm8xxx_mpp_platform_data		*mpp_pdata;
	struct pm8xxx_rtc_platform_data         *rtc_pdata;
	struct pm8xxx_pwrkey_platform_data	*pwrkey_pdata;
	struct pm8xxx_keypad_platform_data	*keypad_pdata;
	struct pm8921_charger_platform_data	*charger_pdata;
	struct pm8921_bms_platform_data		*bms_pdata;
	struct pm8xxx_misc_platform_data	*misc_pdata;
	struct pm8921_regulator_platform_data	*regulator_pdatas;
	int					num_regulators;
	struct pm8921_adc_platform_data		*adc_pdata;
	struct led_platform_data		*leds_pdata;
};

#endif
