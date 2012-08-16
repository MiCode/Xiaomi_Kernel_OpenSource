/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
int __init krait_power_init(void);
void secondary_cpu_hs_init(void *base_ptr);

#endif
