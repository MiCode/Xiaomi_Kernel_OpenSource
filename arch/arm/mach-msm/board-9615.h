/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/regulator/msm-gpio-regulator.h>

/*
 * MDM9x15 I2S.
 */
#ifdef CONFIG_I2C
#define I2C_SURF 1
#define I2C_FFA  (1 << 1)
#define I2C_RUMI (1 << 2)
#define I2C_SIM  (1 << 3)
#define I2C_FLUID (1 << 4)
#define I2C_LIQUID (1 << 5)

struct i2c_registry {
	u8                     machs;
	int                    bus;
	struct i2c_board_info *info;
	int                    len;
};
#endif
/* Tabla slave address for I2C */
#define TABLA_I2C_SLAVE_ADDR		0x0d
#define TABLA_ANALOG_I2C_SLAVE_ADDR	0x77
#define TABLA_DIGITAL1_I2C_SLAVE_ADDR	0x66
#define TABLA_DIGITAL2_I2C_SLAVE_ADDR	0x55
#define MSM_9615_GSBI5_QUP_I2C_BUS_ID 0
/*
 * MDM9x15 I2S.
 */

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
uint32_t msm9615_rpm_get_swfi_latency(void);
int msm9615_init_gpiomux(void);
void msm9615_init_mmc(void);
void mdm9615_allocate_fb_region(void);
void mdm9615_init_fb(void);
#endif
