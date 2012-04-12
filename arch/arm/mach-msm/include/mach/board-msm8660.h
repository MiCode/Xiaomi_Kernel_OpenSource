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

#ifndef __ARCH_ARM_MACH_MSM_BOARD_MSM8660_H
#define __ARCH_ARM_MACH_MSM_BOARD_MSM8660_H

#include <linux/mfd/pmic8058.h>
#include <linux/mfd/pmic8901.h>
#include <mach/irqs.h>

/* Macros assume PMIC GPIOs start at 0 */
#define PM8058_GPIO_BASE			NR_MSM_GPIOS
#define PM8058_GPIO_PM_TO_SYS(pm_gpio)		(pm_gpio + PM8058_GPIO_BASE)
#define PM8058_GPIO_SYS_TO_PM(sys_gpio)		(sys_gpio - PM8058_GPIO_BASE)
#define PM8058_MPP_BASE			(PM8058_GPIO_BASE + PM8058_GPIOS)
#define PM8058_MPP_PM_TO_SYS(pm_gpio)		(pm_gpio + PM8058_MPP_BASE)
#define PM8058_MPP_SYS_TO_PM(sys_gpio)		(sys_gpio - PM8058_MPP_BASE)
#define PM8058_IRQ_BASE				(NR_MSM_IRQS + NR_GPIO_IRQS)

#define PM8901_MPP_BASE				(PM8058_GPIO_BASE + \
						PM8058_GPIOS + PM8058_MPPS)
#define PM8901_MPP_PM_TO_SYS(pm_gpio)		(pm_gpio + PM8901_MPP_BASE)
#define PM8901_MPP_SYS_TO_PM(sys_gpio)		(sys_gpio - PM901_MPP_BASE)
#define PM8901_IRQ_BASE				(PM8058_IRQ_BASE + \
						NR_PMIC8058_IRQS)

#ifdef CONFIG_MSM_CAMERA_V4L2
extern struct msm_camera_board_info msm8x60_camera_board_info;
void msm8x60_init_cam(void);
#endif

#endif
