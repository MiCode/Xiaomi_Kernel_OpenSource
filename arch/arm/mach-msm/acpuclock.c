/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/smp.h>
#include "acpuclock.h"
#include <trace/events/power.h>

static struct acpuclk_data *acpuclk_data;

unsigned long acpuclk_get_rate(int cpu)
{
	if (!acpuclk_data->get_rate)
		return 0;

	return acpuclk_data->get_rate(cpu);
}

int acpuclk_set_rate(int cpu, unsigned long rate, enum setrate_reason reason)
{
	int ret;

	if (!acpuclk_data->set_rate)
		return 0;

	trace_cpu_frequency_switch_start(acpuclk_get_rate(cpu), rate, cpu);
	ret = acpuclk_data->set_rate(cpu, rate, reason);
	if (!ret) {
		trace_cpu_frequency_switch_end(cpu);
		trace_cpu_frequency(rate, cpu);
	}

	return ret;
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

void __devinit acpuclk_register(struct acpuclk_data *data)
{
	acpuclk_data = data;
}
