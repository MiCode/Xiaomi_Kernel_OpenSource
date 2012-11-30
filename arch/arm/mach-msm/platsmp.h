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

/*
 * control for which core is the next to come out of the secondary
 * boot "holding pen".
 */
extern volatile int pen_release;

void __cpuinit msm_secondary_startup(void);
void __cpuinit write_pen_release(int val);

/* HOTPLUG Interface */
int platform_cpu_kill(unsigned int cpu);
void platform_cpu_die(unsigned int cpu);
int platform_cpu_disable(unsigned int cpu);

extern struct smp_operations arm_smp_ops __initdata;
extern struct smp_operations msm8960_smp_ops __initdata;
extern struct smp_operations msm8974_smp_ops __initdata;
extern struct smp_operations msm8625_smp_ops __initdata;
extern struct smp_operations scorpion_smp_ops __initdata;
