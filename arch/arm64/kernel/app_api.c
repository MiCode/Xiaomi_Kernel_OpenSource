/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#include <linux/bitops.h>
#include <linux/spinlock.h>
#include <linux/cpu.h>
#include <linux/export.h>

#include <asm/app_api.h>

static spinlock_t spinlock;
static spinlock_t spinlock_32bit_app;
static DEFINE_PER_CPU(int, app_config_applied);
static unsigned long app_config_set[NR_CPUS];
static unsigned long app_config_clear[NR_CPUS];

void set_app_setting_bit(uint32_t bit)
{
	unsigned long flags;
	uint64_t reg;
	int cpu;

	spin_lock_irqsave(&spinlock, flags);
	asm volatile("mrs %0, S3_1_C15_C15_0" : "=r" (reg));
	reg = reg | BIT(bit);
	isb();
	asm volatile("msr S3_1_C15_C15_0, %0" : : "r" (reg));
	isb();
	if (bit == APP_SETTING_BIT) {
		cpu = raw_smp_processor_id();
		app_config_set[cpu]++;

		this_cpu_write(app_config_applied, 1);
	}
	spin_unlock_irqrestore(&spinlock, flags);

}
EXPORT_SYMBOL(set_app_setting_bit);

void clear_app_setting_bit(uint32_t bit)
{
	unsigned long flags;
	uint64_t reg;
	int cpu;

	spin_lock_irqsave(&spinlock, flags);
	asm volatile("mrs %0, S3_1_C15_C15_0" : "=r" (reg));
	reg = reg & ~BIT(bit);
	isb();
	asm volatile("msr S3_1_C15_C15_0, %0" : : "r" (reg));
	isb();
	if (bit == APP_SETTING_BIT) {
		cpu = raw_smp_processor_id();
		app_config_clear[cpu]++;

		this_cpu_write(app_config_applied, 0);
	}
	spin_unlock_irqrestore(&spinlock, flags);
}
EXPORT_SYMBOL(clear_app_setting_bit);

void set_app_setting_bit_for_32bit_apps(void)
{
	unsigned long flags;
	uint64_t reg;

	spin_lock_irqsave(&spinlock_32bit_app, flags);
	asm volatile("mrs %0, S3_0_c15_c15_1 " : "=r" (reg));
	reg = reg | BIT(18);
	reg = reg & ~BIT(2);
	reg = reg | 0x3;
	isb();
	asm volatile("msr S3_0_c15_c15_1, %0" : : "r" (reg));
	isb();
	spin_unlock_irqrestore(&spinlock_32bit_app, flags);
}
EXPORT_SYMBOL(set_app_setting_bit_for_32bit_apps);

void clear_app_setting_bit_for_32bit_apps(void)
{
	unsigned long flags;
	uint64_t reg;

	spin_lock_irqsave(&spinlock_32bit_app, flags);
	asm volatile("mrs %0, S3_0_c15_c15_1 " : "=r" (reg));
	reg = reg & ~BIT(18);
	reg = reg & ~0x3;
	isb();
	asm volatile("msr S3_0_c15_c15_1, %0" : : "r" (reg));
	isb();
	spin_unlock_irqrestore(&spinlock_32bit_app, flags);
}
EXPORT_SYMBOL(clear_app_setting_bit_for_32bit_apps);

static int __init init_app_api(void)
{
	spin_lock_init(&spinlock);
	spin_lock_init(&spinlock_32bit_app);
	return 0;
}
early_initcall(init_app_api);
