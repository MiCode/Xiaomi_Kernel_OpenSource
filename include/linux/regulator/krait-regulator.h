/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#ifndef __KRAIT_REGULATOR_H__
#define __KRAIT_REGULATOR_H__

#define KRAIT_REGULATOR_DRIVER_NAME "krait-power-regulator"

/**
 * krait_power_init - driver initialization function
 *
 * This function registers the krait-power-regulator platform driver. This
 * should be called from appropriate initialization code. Returns 0 on
 * success and error on failure.
 */

#ifdef CONFIG_ARCH_MSM8974
int __init krait_power_init(void);
void secondary_cpu_hs_init(void *base_ptr);

/**
 * krait_power_mdd_enable - function to turn on/off MDD. Turning off MDD
 *				turns off badngap reference for LDO. If
 *				a core is running on a LDO, requests to
 *				turn off MDD will not be honoured
 * @on:	boolean to indicate whether to turn MDD on/off
 *
 * CONTEXT: Can be called in interrupt context, only when the core
 *		is about to go to idle, this guarantees that there are no
 *		frequency changes on that cpu happening. Note if going from off
 *		to on mode there will be settling delays
 *
 * RETURNS: -EINVAL if MDD cannot be turned off
 */
int krait_power_mdd_enable(int cpu_num, bool on);
#else
static inline int __init krait_power_init(void)
{
	return -ENOSYS;
}

static inline void secondary_cpu_hs_init(void *base_ptr) {}
static inline int krait_power_mdd_enable(int cpu_num, bool on)
{
	return -EINVAL;
}
#endif

#endif
