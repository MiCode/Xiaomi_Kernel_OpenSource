/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _ARCH_ARM_MACH_MSM_SOCINFO_H_
#define _ARCH_ARM_MACH_MSM_SOCINFO_H_

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/of_fdt.h>
#include <linux/of.h>

#include <asm/cputype.h>
#include <asm/mach-types.h>
/*
 * SOC version type with major number in the upper 16 bits and minor
 * number in the lower 16 bits.  For example:
 *   1.0 -> 0x00010000
 *   2.3 -> 0x00020003
 */
#define SOCINFO_VERSION_MAJOR(ver) ((ver & 0xffff0000) >> 16)
#define SOCINFO_VERSION_MINOR(ver) (ver & 0x0000ffff)

#ifdef CONFIG_OF
#define early_machine_is_copper()	\
	of_flat_dt_is_compatible(of_get_flat_dt_root(), "qcom,msmcopper")
#define machine_is_copper()		\
	of_machine_is_compatible("qcom,msmcopper")
#else
#define early_machine_is_copper()	0
#define machine_is_copper()		0
#endif

enum msm_cpu {
	MSM_CPU_UNKNOWN = 0,
	MSM_CPU_7X01,
	MSM_CPU_7X25,
	MSM_CPU_7X27,
	MSM_CPU_8X50,
	MSM_CPU_8X50A,
	MSM_CPU_7X30,
	MSM_CPU_8X55,
	MSM_CPU_8X60,
	MSM_CPU_8960,
	MSM_CPU_7X27A,
	FSM_CPU_9XXX,
	MSM_CPU_7X25A,
	MSM_CPU_7X25AA,
	MSM_CPU_8064,
	MSM_CPU_8930,
	MSM_CPU_7X27AA,
	MSM_CPU_9615,
	MSM_CPU_COPPER,
	MSM_CPU_8627,
};

enum msm_cpu socinfo_get_msm_cpu(void);
uint32_t socinfo_get_id(void);
uint32_t socinfo_get_version(void);
char *socinfo_get_build_id(void);
uint32_t socinfo_get_platform_type(void);
uint32_t socinfo_get_platform_subtype(void);
uint32_t socinfo_get_platform_version(void);
int __init socinfo_init(void) __must_check;
const int read_msm_cpu_type(void);
const int get_core_count(void);
const int cpu_is_krait_v1(void);

static inline int cpu_is_msm7x01(void)
{
#ifdef CONFIG_ARCH_MSM7X01A
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_7X01;
#else
	return 0;
#endif
}

static inline int cpu_is_msm7x25(void)
{
#ifdef CONFIG_ARCH_MSM7X25
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_7X25;
#else
	return 0;
#endif
}

static inline int cpu_is_msm7x27(void)
{
#ifdef CONFIG_ARCH_MSM7X27
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_7X27;
#else
	return 0;
#endif
}

static inline int cpu_is_msm7x27a(void)
{
#ifdef CONFIG_ARCH_MSM7X27A
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_7X27A;
#else
	return 0;
#endif
}

static inline int cpu_is_msm7x27aa(void)
{
#ifdef CONFIG_ARCH_MSM7X27A
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_7X27AA;
#else
	return 0;
#endif
}

static inline int cpu_is_msm7x25a(void)
{
#ifdef CONFIG_ARCH_MSM7X27A
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_7X25A;
#else
	return 0;
#endif
}

static inline int cpu_is_msm7x25aa(void)
{
#ifdef CONFIG_ARCH_MSM7X27A
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_7X25AA;
#else
	return 0;
#endif
}

static inline int cpu_is_msm7x30(void)
{
#ifdef CONFIG_ARCH_MSM7X30
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_7X30;
#else
	return 0;
#endif
}

static inline int cpu_is_qsd8x50(void)
{
#ifdef CONFIG_ARCH_QSD8X50
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_8X50;
#else
	return 0;
#endif
}

static inline int cpu_is_msm8x55(void)
{
#ifdef CONFIG_ARCH_MSM7X30
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_8X55;
#else
	return 0;
#endif
}

static inline int cpu_is_msm8x60(void)
{
#ifdef CONFIG_ARCH_MSM8X60
	return read_msm_cpu_type() == MSM_CPU_8X60;
#else
	return 0;
#endif
}

static inline int cpu_is_msm8960(void)
{
#ifdef CONFIG_ARCH_MSM8960
	return read_msm_cpu_type() == MSM_CPU_8960;
#else
	return 0;
#endif
}

static inline int cpu_is_apq8064(void)
{
#ifdef CONFIG_ARCH_APQ8064
	return read_msm_cpu_type() == MSM_CPU_8064;
#else
	return 0;
#endif
}

static inline int cpu_is_msm8930(void)
{
#ifdef CONFIG_ARCH_MSM8930
	return (read_msm_cpu_type() == MSM_CPU_8930) ||
	       (read_msm_cpu_type() == MSM_CPU_8627);
#else
	return 0;
#endif
}

static inline int cpu_is_msm8627(void)
{
/* 8930 and 8627 will share the same CONFIG_ARCH type unless otherwise needed */
#ifdef CONFIG_ARCH_MSM8930
	return read_msm_cpu_type() == MSM_CPU_8627;
#else
	return 0;
#endif
}

static inline int cpu_is_fsm9xxx(void)
{
#ifdef CONFIG_ARCH_FSM9XXX
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == FSM_CPU_9XXX;
#else
	return 0;
#endif
}

static inline int cpu_is_msm9615(void)
{
#ifdef CONFIG_ARCH_MSM9615
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_9615;
#else
	return 0;
#endif
}
#endif
