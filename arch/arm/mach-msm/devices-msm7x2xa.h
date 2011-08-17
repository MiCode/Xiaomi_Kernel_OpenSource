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
#ifndef __ARCH_ARM_MACH_MSM_DEVICES_MSM7X2XA_H
#define __ARCH_ARM_MACH_MSM_DEVICES_MSM7X2XA_H

#define MSM_GSBI0_QUP_I2C_BUS_ID	0
#define MSM_GSBI1_QUP_I2C_BUS_ID	1

void __init msm_common_io_init(void);
void __init msm_init_pmic_vibrator(void);
void __init msm7x25a_kgsl_3d0_init(void);
int __init msm7x2x_misc_init(void);
#endif
