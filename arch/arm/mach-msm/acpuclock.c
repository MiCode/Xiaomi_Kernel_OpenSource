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

#include <linux/cpu.h>
#include "acpuclock.h"

static struct acpuclk_data *acpuclk_data;

unsigned long acpuclk_get_rate(int cpu)
{
	if (!acpuclk_data->get_rate)
		return 0;

	return acpuclk_data->get_rate(cpu);
}

int acpuclk_set_rate(int cpu, unsigned long rate, enum setrate_reason reason)
{
	if (!acpuclk_data->set_rate)
		return 0;

	return acpuclk_data->set_rate(cpu, rate, reason);
}

uint32_t acpuclk_get_switch_time(void)
{
	return acpuclk_data->switch_time_us;
}

unsigned long acpuclk_power_collapse(void)
{
	unsigned long rate = acpuclk_get_rate(smp_processor_id());
	acpuclk_set_rate(smp_processor_id(), acpuclk_data->power_collapse_khz,
			 SETRATE_PC);
	return rate;
}

unsigned long acpuclk_wait_for_irq(void)
{
	unsigned long rate = acpuclk_get_rate(smp_processor_id());
	acpuclk_set_rate(smp_processor_id(), acpuclk_data->wait_for_irq_khz,
			 SETRATE_SWFI);
	return rate;
}

void __init acpuclk_register(struct acpuclk_data *data)
{
	acpuclk_data = data;
}

int __init acpuclk_init(struct acpuclk_soc_data *soc_data)
{
	int rc;

	if (!soc_data->init)
		return -EINVAL;

	rc = soc_data->init(soc_data);
	if (rc)
		return rc;

	if (!acpuclk_data)
		return -ENODEV;

	return 0;
}
