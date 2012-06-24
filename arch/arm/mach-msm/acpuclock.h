/*
 * MSM architecture CPU clock driver header
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2012, Code Aurora Forum. All rights reserved.
 * Author: San Mehat <san@android.com>
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

#ifndef __ARCH_ARM_MACH_MSM_ACPUCLOCK_H
#define __ARCH_ARM_MACH_MSM_ACPUCLOCK_H

/**
 * enum setrate_reason - Reasons for use with acpuclk_set_rate()
 */
enum setrate_reason {
	SETRATE_CPUFREQ = 0,
	SETRATE_SWFI,
	SETRATE_PC,
	SETRATE_HOTPLUG,
	SETRATE_INIT,
};

/**
 * struct acpuclk_pdata - Platform data for acpuclk
 */
struct acpuclk_pdata {
	unsigned long max_speed_delta_khz;
	unsigned int max_axi_khz;
};

/**
 * struct acpuclk_data - Function pointers and data for function implementations
 */
struct acpuclk_data {
	unsigned long (*get_rate)(int cpu);
	int (*set_rate)(int cpu, unsigned long rate, enum setrate_reason);
	uint32_t switch_time_us;
	unsigned long power_collapse_khz;
	unsigned long wait_for_irq_khz;
};

/**
 * acpulock_get_rate() - Get a CPU's clock rate in KHz
 * @cpu: CPU to query the rate of
 */
unsigned long acpuclk_get_rate(int cpu);

/**
 * acpuclk_set_rate() - Set a CPU's clock rate
 * @cpu: CPU to set rate of
 * @rate: Desired rate in KHz
 * @setrate_reason: Reason for the rate switch
 *
 * Returns 0 for success.
 */
int acpuclk_set_rate(int cpu, unsigned long rate, enum setrate_reason);

/**
 * acpuclk_get_switch_time() - Query estimated time in us for a CPU rate switch
 */
uint32_t acpuclk_get_switch_time(void);

/**
 * acpuclk_power_collapse() - Prepare current CPU clocks for power-collapse
 *
 * Returns the previous rate of the CPU in KHz.
 */
unsigned long acpuclk_power_collapse(void);

/**
 * acpuclk_wait_for_irq() - Prepare current CPU clocks for SWFI
 *
 * Returns the previous rate of the CPU in KHz.
 */
unsigned long acpuclk_wait_for_irq(void);

/**
 * acpuclk_register() - Register acpuclk_data function implementations
 * @data: acpuclock API implementations and data
 */
void acpuclk_register(struct acpuclk_data *data);

#endif
