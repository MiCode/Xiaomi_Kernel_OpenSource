/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_BOARD_APQ8064_H
#define __ARCH_ARM_MACH_MSM_BOARD_APQ8064_H

#include <linux/regulator/msm-gpio-regulator.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/mfd/pm8xxx/pm8821.h>
#include <mach/msm_memtypes.h>
#include <mach/irqs.h>
#include <mach/rpm-regulator.h>
#include <mach/msm_rtb.h>
#include <mach/msm_cache_dump.h>

/* Macros assume PMIC GPIOs and MPPs start at 1 */
#define PM8921_GPIO_BASE		NR_GPIO_IRQS
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8921_GPIO_BASE)
#define PM8921_MPP_BASE			(PM8921_GPIO_BASE + PM8921_NR_GPIOS)
#define PM8921_MPP_PM_TO_SYS(pm_mpp)	(pm_mpp - 1 + PM8921_MPP_BASE)
#define PM8921_IRQ_BASE			(NR_MSM_IRQS + NR_GPIO_IRQS)

#define PM8821_MPP_BASE			(PM8921_MPP_BASE + PM8921_NR_MPPS)
#define PM8821_MPP_PM_TO_SYS(pm_mpp)	(pm_mpp - 1 + PM8821_MPP_BASE)
#define PM8821_IRQ_BASE			(PM8921_IRQ_BASE + PM8921_NR_IRQS)

#define TABLA_INTERRUPT_BASE		(PM8821_IRQ_BASE + PM8821_NR_IRQS)

extern struct pm8xxx_regulator_platform_data
	msm8064_pm8921_regulator_pdata[] __devinitdata;

extern int msm8064_pm8921_regulator_pdata_len __devinitdata;

extern struct pm8xxx_regulator_platform_data
	msm8064_pm8917_regulator_pdata[] __devinitdata;

extern int msm8064_pm8917_regulator_pdata_len __devinitdata;

#define GPIO_VREG_ID_EXT_5V		0
#define GPIO_VREG_ID_EXT_3P3V		1
#define GPIO_VREG_ID_EXT_TS_SW		2
#define GPIO_VREG_ID_EXT_MPP8		3
#define GPIO_VREG_ID_EXT_SATA_PWR	4

#define GPIO_VREG_ID_AVC_1P2V		0
#define GPIO_VREG_ID_AVC_1P8V		1
#define GPIO_VREG_ID_AVC_2P2V		2
#define GPIO_VREG_ID_AVC_5V		3
#define GPIO_VREG_ID_AVC_3P3V		4

#define APQ8064_EXT_3P3V_REG_EN_GPIO	77

extern struct gpio_regulator_platform_data
	apq8064_gpio_regulator_pdata[] __devinitdata;

extern struct gpio_regulator_platform_data
	mpq8064_gpio_regulator_pdata[] __devinitdata;

extern struct fixed_voltage_config mpq8064_3p3_regulator_pdata;

extern struct rpm_regulator_platform_data
	apq8064_rpm_regulator_pdata __devinitdata;

extern struct rpm_regulator_platform_data
	apq8064_rpm_regulator_pm8921_pdata __devinitdata;

extern struct regulator_init_data msm8064_saw_regulator_pdata_8921_s5;
extern struct regulator_init_data msm8064_saw_regulator_pdata_8921_s6;
extern struct regulator_init_data msm8064_saw_regulator_pdata_8821_s0;
extern struct regulator_init_data msm8064_saw_regulator_pdata_8821_s1;

struct mmc_platform_data;
int __init apq8064_add_sdcc(unsigned int controller,
		struct mmc_platform_data *plat);

void apq8064_init_mmc(void);
void apq8064_init_gpiomux(void);
void apq8064_init_pmic(void);

extern struct msm_camera_board_info apq8064_camera_board_info;
void apq8064_init_cam(void);


/* Tabla slave address for I2C */
#define APQ_8064_TABLA_I2C_SLAVE_ADDR		0x0d
#define APQ_8064_TABLA_ANALOG_I2C_SLAVE_ADDR	0x77
#define APQ_8064_TABLA_DIGITAL1_I2C_SLAVE_ADDR	0x66
#define APQ_8064_TABLA_DIGITAL2_I2C_SLAVE_ADDR	0x55

#define APQ_8064_GSBI1_QUP_I2C_BUS_ID 0
#define APQ_8064_GSBI3_QUP_I2C_BUS_ID 3
#define APQ_8064_GSBI4_QUP_I2C_BUS_ID 4
#define APQ_8064_GSBI5_QUP_I2C_BUS_ID 5

unsigned char apq8064_hdmi_as_primary_selected(void);
unsigned char apq8064_mhl_display_enabled(void);
void apq8064_init_fb(void);
void apq8064_allocate_fb_region(void);
void apq8064_mdp_writeback(struct memtype_reserve *reserve_table);
void __init apq8064_set_display_params(char *prim_panel, char *ext_panel,
		unsigned char resolution);

void apq8064_init_gpu(void);
void apq8064_pm8xxx_gpio_mpp_init(void);
void __init configure_apq8064_pm8917_power_grid(void);

#if defined(CONFIG_BT) && defined(CONFIG_MARIMBA_CORE)
void __init apq8064_bt_power_init(void);
#endif

#define PLATFORM_IS_MPQ8064() \
	(machine_is_mpq8064_hrd() || \
	 machine_is_mpq8064_dtv() || \
	 machine_is_mpq8064_cdp() \
	)


#define GPIO_EXPANDER_IRQ_BASE	(TABLA_INTERRUPT_BASE + \
					NR_TABLA_IRQS)
#define GPIO_EXPANDER_GPIO_BASE	(PM8821_MPP_BASE + PM8821_NR_MPPS)

#define GPIO_EPM_EXPANDER_BASE	GPIO_EXPANDER_GPIO_BASE
#define SX150X_EPM_NR_GPIOS	16
#define SX150X_EPM_NR_IRQS	8

#define SX150X_EXP1_GPIO_BASE	(GPIO_EPM_EXPANDER_BASE + \
					SX150X_EPM_NR_GPIOS)
#define SX150X_EXP1_IRQ_BASE	(GPIO_EXPANDER_IRQ_BASE + \
				SX150X_EPM_NR_IRQS)
#define SX150X_EXP1_NR_IRQS	16
#define SX150X_EXP1_NR_GPIOS	16

#define SX150X_EXP2_GPIO_BASE	(SX150X_EXP1_GPIO_BASE + \
					SX150X_EXP1_NR_GPIOS)
#define SX150X_EXP2_IRQ_BASE	(SX150X_EXP1_IRQ_BASE + SX150X_EXP1_NR_IRQS)
#define SX150X_EXP2_NR_IRQS	8
#define SX150X_EXP2_NR_GPIOS	8

#define SX150X_EXP3_GPIO_BASE	(SX150X_EXP2_GPIO_BASE + \
					SX150X_EXP2_NR_GPIOS)
#define SX150X_EXP3_IRQ_BASE	(SX150X_EXP2_IRQ_BASE + SX150X_EXP2_NR_IRQS)
#define SX150X_EXP3_NR_IRQS	8
#define SX150X_EXP3_NR_GPIOS	8

#define SX150X_EXP4_GPIO_BASE	(SX150X_EXP3_GPIO_BASE + \
					SX150X_EXP3_NR_GPIOS)
#define SX150X_EXP4_IRQ_BASE	(SX150X_EXP3_IRQ_BASE + SX150X_EXP3_NR_IRQS)
#define SX150X_EXP4_NR_IRQS	16
#define SX150X_EXP4_NR_GPIOS	16

#define SX150X_GPIO(_expander, _pin) (SX150X_EXP##_expander##_GPIO_BASE + _pin)

enum {
	SX150X_EPM,
	SX150X_EXP1,
	SX150X_EXP2,
	SX150X_EXP3,
	SX150X_EXP4,
};

extern struct msm_rtb_platform_data apq8064_rtb_pdata;
extern struct msm_cache_dump_platform_data apq8064_cache_dump_pdata;
#endif
