/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_RPM_REGULATOR_H
#define __ARCH_ARM_MACH_MSM_RPM_REGULATOR_H

#include <linux/regulator/machine.h>

#define RPM_REGULATOR_DEV_NAME "rpm-regulator"

#if defined(CONFIG_ARCH_MSM8X60)
#include <mach/rpm-regulator-8660.h>
#elif defined(CONFIG_ARCH_MSM8960)
#include <mach/rpm-regulator-8960.h>
#endif

/**
 * rpm_vreg_set_voltage - vote for a min_uV value of specified regualtor
 * @vreg: ID for regulator
 * @voter: ID for the voter
 * @min_uV: minimum acceptable voltage (in uV) that is voted for
 * @max_uV: maximum acceptable voltage (in uV) that is voted for
 * @sleep_also: 0 for active set only, non-0 for active set and sleep set
 *
 * Returns 0 on success or errno.
 *
 * This function is used to vote for the voltage of a regulator without
 * using the regulator framework.  It is needed by consumers which hold spin
 * locks or have interrupts disabled because the regulator framework can sleep.
 * It is also needed by consumers which wish to only vote for active set
 * regulator voltage.
 *
 * If sleep_also == 0, then a sleep-set value of 0V will be voted for.
 *
 * This function may only be called for regulators which have the sleep flag
 * specified in their private data.
 */
int rpm_vreg_set_voltage(enum rpm_vreg_id vreg_id, enum rpm_vreg_voter voter,
			 int min_uV, int max_uV, int sleep_also);

/**
 * rpm_vreg_set_frequency - sets the frequency of a switching regulator
 * @vreg: ID for regulator
 * @freq: enum corresponding to desired frequency
 *
 * Returns 0 on success or errno.
 */
int rpm_vreg_set_frequency(enum rpm_vreg_id vreg_id, enum rpm_vreg_freq freq);

#endif
