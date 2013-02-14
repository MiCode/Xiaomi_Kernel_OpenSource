/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_INCLUDE_MACH_RPM_REGULATOR_SMD_H
#define __ARCH_ARM_MACH_MSM_INCLUDE_MACH_RPM_REGULATOR_SMD_H

#include <linux/device.h>

struct rpm_regulator;

/**
 * enum rpm_regulator_voltage_corner - possible voltage corner values
 *
 * These should be used in regulator_set_voltage() and
 * rpm_regulator_set_voltage() calls for corner type regulators as if they had
 * units of uV.
 *
 * Note, the meaning of corner values is set by the RPM.  It is possible that
 * future platforms will utilize different corner values.  The values specified
 * in this enum correspond to MSM8974 for PMIC PM8841 SMPS 2 (VDD_Dig).
 */
enum rpm_regulator_voltage_corner {
	RPM_REGULATOR_CORNER_NONE = 1,
	RPM_REGULATOR_CORNER_RETENTION,
	RPM_REGULATOR_CORNER_SVS_KRAIT,
	RPM_REGULATOR_CORNER_SVS_SOC,
	RPM_REGULATOR_CORNER_NORMAL,
	RPM_REGULATOR_CORNER_TURBO,
	RPM_REGULATOR_CORNER_SUPER_TURBO,
};

#if defined(CONFIG_MSM_RPM_REGULATOR_SMD) || defined(CONFIG_MSM_RPM_REGULATOR)

struct rpm_regulator *rpm_regulator_get(struct device *dev, const char *supply);

void rpm_regulator_put(struct rpm_regulator *regulator);

int rpm_regulator_enable(struct rpm_regulator *regulator);

int rpm_regulator_disable(struct rpm_regulator *regulator);

int rpm_regulator_set_voltage(struct rpm_regulator *regulator, int min_uV,
			      int max_uV);

int __init rpm_regulator_smd_driver_init(void);

#else

static inline struct rpm_regulator *rpm_regulator_get(struct device *dev,
					const char *supply) { return NULL; }

static inline void rpm_regulator_put(struct rpm_regulator *regulator) { }

static inline int rpm_regulator_enable(struct rpm_regulator *regulator)
			{ return 0; }

static inline int rpm_regulator_disable(struct rpm_regulator *regulator)
			{ return 0; }

static inline int rpm_regulator_set_voltage(struct rpm_regulator *regulator,
					int min_uV, int max_uV) { return 0; }

static inline int __init rpm_regulator_smd_driver_init(void) { return 0; }

#endif /* CONFIG_MSM_RPM_REGULATOR_SMD */

#endif
