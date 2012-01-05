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

#ifndef __ARCH_ARM_MACH_MSM_BOARD_MSM8930_H
#define __ARCH_ARM_MACH_MSM_BOARD_MSM8930_H

#include <linux/regulator/gpio-regulator.h>
#include <linux/mfd/pm8xxx/pm8038.h>
#include <linux/i2c.h>
#include <linux/i2c/sx150x.h>
#include <mach/irqs.h>
#include <mach/rpm-regulator.h>
#include <mach/msm_memtypes.h>

/*
 * TODO: When physical 8930/PM8038 hardware becomes
 * available, remove this block.
 */
#ifndef MSM8930_PHASE_2
#include <linux/mfd/pm8xxx/pm8921.h>
#define PM8921_GPIO_BASE		NR_GPIO_IRQS
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8921_GPIO_BASE)
#define PM8921_MPP_BASE			(PM8921_GPIO_BASE + PM8921_NR_GPIOS)
#define PM8921_MPP_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8921_MPP_BASE)
#endif

/* Macros assume PMIC GPIOs and MPPs start at 1 */
#define PM8038_GPIO_BASE		NR_GPIO_IRQS
#define PM8038_GPIO_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8038_GPIO_BASE)
#define PM8038_MPP_BASE			(PM8038_GPIO_BASE + PM8038_NR_GPIOS)
#define PM8038_MPP_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8038_MPP_BASE)
#define PM8038_IRQ_BASE			(NR_MSM_IRQS + NR_GPIO_IRQS)

/*
 * TODO: When physical 8930/PM8038 hardware becomes
 * available, replace this block with 8930/pm8038 regulator
 * declarations.
 */
#ifndef MSM8930_PHASE_2
extern struct pm8xxx_regulator_platform_data
	msm_pm8921_regulator_pdata[] __devinitdata;

extern int msm_pm8921_regulator_pdata_len __devinitdata;

extern struct gpio_regulator_platform_data
	msm_gpio_regulator_pdata[] __devinitdata;

extern struct rpm_regulator_platform_data msm_rpm_regulator_pdata __devinitdata;

#define GPIO_VREG_ID_EXT_5V		0
#define GPIO_VREG_ID_EXT_L2		1
#define GPIO_VREG_ID_EXT_3P3V		2
#endif

extern struct regulator_init_data msm8930_saw_regulator_core0_pdata;
extern struct regulator_init_data msm8930_saw_regulator_core1_pdata;

extern struct pm8xxx_regulator_platform_data
	msm8930_pm8038_regulator_pdata[] __devinitdata;

extern int msm8930_pm8038_regulator_pdata_len __devinitdata;

#define MSM8930_GPIO_VREG_ID_EXT_5V		0
#define MSM8930_GPIO_VREG_ID_EXT_OTG_SW		1

extern struct gpio_regulator_platform_data
	msm8930_gpio_regulator_pdata[] __devinitdata;

#if defined(CONFIG_GPIO_SX150X) || defined(CONFIG_GPIO_SX150X_MODULE)
enum {
	GPIO_EXPANDER_IRQ_BASE = (PM8038_IRQ_BASE + PM8038_NR_IRQS),
	GPIO_EXPANDER_GPIO_BASE = (PM8038_MPP_BASE + PM8038_NR_MPPS),
	/* CAM Expander */
	GPIO_CAM_EXPANDER_BASE = GPIO_EXPANDER_GPIO_BASE,
	GPIO_CAM_GP_STROBE_READY = GPIO_CAM_EXPANDER_BASE,
	GPIO_CAM_GP_AFBUSY,
	GPIO_CAM_GP_STROBE_CE,
	GPIO_CAM_GP_CAM1MP_XCLR,
	GPIO_CAM_GP_CAMIF_RESET_N,
	GPIO_CAM_GP_XMT_FLASH_INT,
	GPIO_CAM_GP_LED_EN1,
	GPIO_CAM_GP_LED_EN2,

};
#endif

enum {
	SX150X_CAM,
};

#endif

extern struct sx150x_platform_data msm8930_sx150x_data[];
extern struct msm_camera_board_info msm8930_camera_board_info;
void msm8930_init_cam(void);
void msm8930_init_fb(void);
void msm8930_init_pmic(void);

/*
 * TODO: When physical 8930/PM8038 hardware becomes
 * available, remove this block or add the config
 * option.
 */
#ifndef MSM8930_PHASE_2
void msm8960_init_pmic(void);
void msm8960_pm8921_gpio_mpp_init(void);
#endif

void msm8930_init_mmc(void);
int msm8930_init_gpiomux(void);
void msm8930_allocate_fb_region(void);
void msm8930_pm8038_gpio_mpp_init(void);
void msm8930_mdp_writeback(struct memtype_reserve *reserve_table);

#define PLATFORM_IS_CHARM25() \
	(machine_is_msm8930_cdp() && \
		(socinfo_get_platform_subtype() == 1) \
	)

#define MSM_8930_GSBI4_QUP_I2C_BUS_ID 4
#define MSM_8930_GSBI3_QUP_I2C_BUS_ID 3
#define MSM_8930_GSBI10_QUP_I2C_BUS_ID 10
