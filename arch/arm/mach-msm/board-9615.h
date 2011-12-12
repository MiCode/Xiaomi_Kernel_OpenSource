/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_BOARD_9615_H
#define __ARCH_ARM_MACH_MSM_BOARD_9615_H

#include <mach/irqs.h>
#include <linux/mfd/pm8xxx/pm8018.h>
#include <linux/regulator/gpio-regulator.h>

/* Macros assume PMIC GPIOs and MPPs start at 1 */
#define PM8018_GPIO_BASE		NR_GPIO_IRQS
#define PM8018_GPIO_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8018_GPIO_BASE)
#define PM8018_MPP_BASE			(PM8018_GPIO_BASE + PM8018_NR_GPIOS)
#define PM8018_MPP_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8018_MPP_BASE)
#define PM8018_IRQ_BASE			(NR_MSM_IRQS + NR_GPIO_IRQS)
#define PM8018_MPP_IRQ_BASE		(PM8018_IRQ_BASE + NR_GPIO_IRQS)

extern struct pm8xxx_regulator_platform_data
	msm_pm8018_regulator_pdata[] __devinitdata;

extern int msm_pm8018_regulator_pdata_len __devinitdata;

extern struct rpm_regulator_platform_data
msm_rpm_regulator_9615_pdata __devinitdata;

#define GPIO_VREG_ID_EXT_2P95V		0

extern struct gpio_regulator_platform_data msm_gpio_regulator_pdata[];

int msm9615_init_gpiomux(void);
void msm9615_init_mmc(void);
#endif
