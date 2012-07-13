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
#ifndef __ARCH_ARM_MACH_MSM_DEVICES_MSM7X2XA_H
#define __ARCH_ARM_MACH_MSM_DEVICES_MSM7X2XA_H

#define MSM_GSBI0_QUP_I2C_BUS_ID	0
#define MSM_GSBI1_QUP_I2C_BUS_ID	1

void __init msm_common_io_init(void);
void __init msm_init_pmic_vibrator(void);
void __init msm7x25a_kgsl_3d0_init(void);
int __init msm7x2x_misc_init(void);
extern struct platform_device msm7x27a_device_vfe;
extern struct platform_device msm7x27a_device_csic0;
extern struct platform_device msm7x27a_device_csic1;
extern struct platform_device msm7x27a_device_clkctl;

extern struct platform_device msm8625_device_csic0;
extern struct platform_device msm8625_device_csic1;

void __init msm8625_init_irq(void);
void __init msm8625_map_io(void);
int  ar600x_wlan_power(bool on);
void __init msm8x25_spm_device_init(void);
void __init msm_pm_register_cpr_ops(void);
void __init msm8x25_kgsl_3d0_init(void);
void __iomem *core1_reset_base(void);
extern void setup_mm_for_reboot(void);
#endif
