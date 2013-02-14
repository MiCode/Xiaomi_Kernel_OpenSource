/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

#define MSM8930_PHASE_2

#include <linux/regulator/msm-gpio-regulator.h>
#include <linux/mfd/pm8xxx/pm8038.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/i2c.h>
#include <linux/i2c/sx150x.h>
#include <mach/irqs.h>
#include <mach/rpm-regulator.h>
#include <mach/msm_memtypes.h>
#include <mach/msm_rtb.h>

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
/*
 * PM8917 has more GPIOs and MPPs than PM8038; therefore, use PM8038 sizes at
 * all times so that PM8038 vs PM8917 can be chosen at runtime.  This results in
 * the Linux GPIO address space being contiguous for PM8917 and discontiguous
 * for PM8038.
 */
#define PM8038_GPIO_BASE		NR_GPIO_IRQS
#define PM8038_GPIO_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8038_GPIO_BASE)
#define PM8038_MPP_BASE			(PM8038_GPIO_BASE + PM8917_NR_GPIOS)
#define PM8038_MPP_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8038_MPP_BASE)
#define PM8038_IRQ_BASE			(NR_MSM_IRQS + NR_GPIO_IRQS)

/* These PM8917 alias macros are used to provide context in board files. */
#define PM8917_GPIO_PM_TO_SYS(pm_gpio)	PM8038_GPIO_PM_TO_SYS(pm_gpio)
#define PM8917_MPP_PM_TO_SYS(pm_gpio)	PM8038_MPP_PM_TO_SYS(pm_gpio)
#define PM8917_IRQ_BASE			PM8038_IRQ_BASE

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

extern struct regulator_init_data msm8930_pm8038_saw_regulator_core0_pdata;
extern struct regulator_init_data msm8930_pm8038_saw_regulator_core1_pdata;
extern struct regulator_init_data msm8930_pm8917_saw_regulator_core0_pdata;
extern struct regulator_init_data msm8930_pm8917_saw_regulator_core1_pdata;

extern struct pm8xxx_regulator_platform_data
	msm8930_pm8038_regulator_pdata[] __devinitdata;
extern int msm8930_pm8038_regulator_pdata_len __devinitdata;

extern struct pm8xxx_regulator_platform_data
	msm8930_pm8917_regulator_pdata[] __devinitdata;
extern int msm8930_pm8917_regulator_pdata_len __devinitdata;

#define MSM8930_GPIO_VREG_ID_EXT_5V		0
#define MSM8930_GPIO_VREG_ID_EXT_OTG_SW		1

extern struct gpio_regulator_platform_data
	msm8930_pm8038_gpio_regulator_pdata[] __devinitdata;
extern struct gpio_regulator_platform_data
	msm8930_pm8917_gpio_regulator_pdata[] __devinitdata;

extern struct rpm_regulator_platform_data
	msm8930_pm8038_rpm_regulator_pdata __devinitdata;
extern struct rpm_regulator_platform_data
	msm8930_pm8917_rpm_regulator_pdata __devinitdata;

#if defined(CONFIG_GPIO_SX150X) || defined(CONFIG_GPIO_SX150X_MODULE)
enum {
	GPIO_EXPANDER_IRQ_BASE = (PM8038_IRQ_BASE + PM8038_NR_IRQS),
	GPIO_EXPANDER_GPIO_BASE = (PM8038_MPP_BASE + PM8917_NR_MPPS),
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
extern void msm8930_add_vidc_device(void);
unsigned char msm8930_mhl_display_enabled(void);

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
void msm8930_pm8917_gpio_mpp_init(void);
void msm8930_set_display_params(char *prim_panel, char *ext_panel);
void msm8930_mdp_writeback(struct memtype_reserve *reserve_table);
void __init msm8930_init_gpu(void);

#define PLATFORM_IS_CHARM25() \
	(machine_is_msm8930_cdp() && \
		(socinfo_get_platform_subtype() == 1) \
	)

#define MSM_8930_GSBI3_QUP_I2C_BUS_ID 3
#define MSM_8930_GSBI4_QUP_I2C_BUS_ID 4
#define MSM_8930_GSBI9_QUP_I2C_BUS_ID 0
#define MSM_8930_GSBI10_QUP_I2C_BUS_ID 10
#define MSM_8930_GSBI12_QUP_I2C_BUS_ID 12

#define HDMI_MHL_MUX_GPIO       73

extern struct msm_rtb_platform_data msm8930_rtb_pdata;
extern struct msm_cache_dump_platform_data msm8930_cache_dump_pdata;
