/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_KRAIT_REGULATOR_PMIC_H
#define __ARCH_ARM_MACH_MSM_KRAIT_REGULATOR_PMIC_H

#ifdef CONFIG_KRAIT_REGULATOR
bool krait_pmic_is_ready(void);
int krait_pmic_post_pfm_entry(void);
int krait_pmic_post_pwm_entry(void);
int krait_pmic_pre_disable(void);
int krait_pmic_pre_multiphase_enable(void);
#else
bool krait_pmic_is_ready(void)
{
	return false;
}
int krait_pmic_post_pfm_entry(void)
{
	return -ENXIO;
}
int krait_pmic_post_pwm_entry(void)
{
	return -ENXIO;
}
int krait_pmic_pre_disable(void)
{
	return 0;
}
int krait_pmic_pre_multiphase_enable(void)
{
	return 0;
}
#endif
#endif
