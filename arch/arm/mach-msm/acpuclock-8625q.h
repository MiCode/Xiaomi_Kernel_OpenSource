/*
 * MSM architecture CPU clock driver header
 *
 * Copyright (c) 2012, Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __ARCH_ARM_MACH_MSM_ACPUCLOCK_8625Q_H
#define __ARCH_ARM_MACH_MSM_ACPUCLOCK_8625Q_H

# include "acpuclock.h"
/**
 * struct acpuclk_pdata_8625q - Platform data for acpuclk
 */
struct acpuclk_pdata_8625q {
	struct acpuclk_pdata *acpu_clk_data;
	unsigned int pvs_voltage_uv;
	bool flag;
};

#endif /* __ARCH_ARM_MACH_MSM_ACPUCLOCK_8625Q_H */
