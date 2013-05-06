/* Copyright (c) 2009-2013, The Linux Foundation. All rights reserved.
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
#define of_board_is_sim()	of_machine_is_compatible("qcom,sim")
#define of_board_is_rumi()	of_machine_is_compatible("qcom,rumi")
#define of_board_is_fluid()	of_machine_is_compatible("qcom,fluid")
#define of_board_is_liquid()	of_machine_is_compatible("qcom,liquid")
#define of_board_is_dragonboard()	\
	of_machine_is_compatible("qcom,dragonboard")
#define of_board_is_cdp()	of_machine_is_compatible("qcom,cdp")
#define of_board_is_mtp()	of_machine_is_compatible("qcom,mtp")
#define of_board_is_qrd()	of_machine_is_compatible("qcom,qrd")
#define of_board_is_xpm()	of_machine_is_compatible("qcom,xpm")

#define machine_is_msm8974()	of_machine_is_compatible("qcom,msm8974")
#define machine_is_msm9625()	of_machine_is_compatible("qcom,msm9625")
#define machine_is_msm8610()	of_machine_is_compatible("qcom,msm8610")
#define machine_is_msm8226()	of_machine_is_compatible("qcom,msm8226")
#define machine_is_apq8074()	of_machine_is_compatible("qcom,apq8074")

#define early_machine_is_msm8610()	\
	of_flat_dt_is_compatible(of_get_flat_dt_root(), "qcom,msm8610")
#define early_machine_is_mpq8092()	\
	of_flat_dt_is_compatible(of_get_flat_dt_root(), "qcom,mpq8092")
#define early_machine_is_apq8084()	\
	of_flat_dt_is_compatible(of_get_flat_dt_root(), "qcom,apq8084")
#define early_machine_is_msmkrypton()	\
	of_flat_dt_is_compatible(of_get_flat_dt_root(), "qcom,msmkrypton")
#define early_machine_is_fsm9900()	\
	of_flat_dt_is_compatible(of_get_flat_dt_root(), "qcom,fsm9900")
#define early_machine_is_msmsamarium()	\
	of_flat_dt_is_compatible(of_get_flat_dt_root(), "qcom,msmsamarium")
#else
#define of_board_is_sim()		0
#define of_board_is_rumi()		0
#define of_board_is_fluid()		0
#define of_board_is_liquid()		0
#define of_board_is_dragonboard()	0
#define of_board_is_cdp()		0
#define of_board_is_mtp()		0
#define of_board_is_qrd()		0
#define of_board_is_xpm()		0

#define machine_is_msm8974()		0
#define machine_is_msm9625()		0
#define machine_is_msm8610()		0
#define machine_is_msm8226()		0
#define machine_is_apq8074()		0

#define early_machine_is_msm8610()	0
#define early_machine_is_mpq8092()	0
#define early_machine_is_apq8084()	0
#define early_machine_is_msmkrypton()	0
#define early_machine_is_fsm9900()	0
#define early_machine_is_msmsamarium()	0
#endif

#define PLATFORM_SUBTYPE_MDM	1
#define PLATFORM_SUBTYPE_SGLTE	6

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
	MSM_CPU_8960AB,
	MSM_CPU_7X27A,
	FSM_CPU_9XXX,
	MSM_CPU_7X25A,
	MSM_CPU_7X25AA,
	MSM_CPU_7X25AB,
	MSM_CPU_8064,
	MSM_CPU_8064AB,
	MSM_CPU_8064AA,
	MSM_CPU_8930,
	MSM_CPU_8930AA,
	MSM_CPU_8930AB,
	MSM_CPU_7X27AA,
	MSM_CPU_9615,
	MSM_CPU_8974,
	MSM_CPU_8974PRO_AA,
	MSM_CPU_8974PRO_AB,
	MSM_CPU_8974PRO_AC,
	MSM_CPU_8627,
	MSM_CPU_8625,
	MSM_CPU_9625,
	MSM_CPU_8092,
	MSM_CPU_8226,
	MSM_CPU_8610,
	MSM_CPU_8625Q,
	MSM_CPU_8084,
	MSM_CPU_KRYPTON,
	FSM_CPU_9900,
	MSM_CPU_SAMARIUM,
};

enum pmic_model {
	PMIC_MODEL_PM8058	= 13,
	PMIC_MODEL_PM8028	= 14,
	PMIC_MODEL_PM8901	= 15,
	PMIC_MODEL_PM8027	= 16,
	PMIC_MODEL_ISL_9519	= 17,
	PMIC_MODEL_PM8921	= 18,
	PMIC_MODEL_PM8018	= 19,
	PMIC_MODEL_PM8015	= 20,
	PMIC_MODEL_PM8014	= 21,
	PMIC_MODEL_PM8821	= 22,
	PMIC_MODEL_PM8038	= 23,
	PMIC_MODEL_PM8922	= 24,
	PMIC_MODEL_PM8917	= 25,
	PMIC_MODEL_UNKNOWN	= 0xFFFFFFFF
};

enum msm_cpu socinfo_get_msm_cpu(void);
uint32_t socinfo_get_id(void);
uint32_t socinfo_get_version(void);
uint32_t socinfo_get_raw_id(void);
char *socinfo_get_build_id(void);
uint32_t socinfo_get_platform_type(void);
uint32_t socinfo_get_platform_subtype(void);
uint32_t socinfo_get_platform_version(void);
enum pmic_model socinfo_get_pmic_model(void);
uint32_t socinfo_get_pmic_die_revision(void);
int __init socinfo_init(void) __must_check;
const int read_msm_cpu_type(void);
const int get_core_count(void);
const int cpu_is_krait(void);
const int cpu_is_krait_v1(void);
const int cpu_is_krait_v2(void);
const int cpu_is_krait_v3(void);

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
#if defined(CONFIG_ARCH_MSM7X27) && !defined(CONFIG_ARCH_MSM7X27A)
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

static inline int cpu_is_msm7x25ab(void)
{
#ifdef CONFIG_ARCH_MSM7X27A
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_7X25AB;
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

static inline int cpu_is_msm8960ab(void)
{
#ifdef CONFIG_ARCH_MSM8960
	return read_msm_cpu_type() == MSM_CPU_8960AB;
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

static inline int cpu_is_apq8064ab(void)
{
#ifdef CONFIG_ARCH_APQ8064
	return read_msm_cpu_type() == MSM_CPU_8064AB;
#else
	return 0;
#endif
}

static inline int cpu_is_apq8064aa(void)
{
#ifdef CONFIG_ARCH_APQ8064
	return read_msm_cpu_type() == MSM_CPU_8064AA;
#else
	return 0;
#endif
}

static inline int cpu_is_msm8930(void)
{
#ifdef CONFIG_ARCH_MSM8930
	return read_msm_cpu_type() == MSM_CPU_8930;
#else
	return 0;
#endif
}

static inline int cpu_is_msm8930aa(void)
{
#ifdef CONFIG_ARCH_MSM8930
	return read_msm_cpu_type() == MSM_CPU_8930AA;
#else
	return 0;
#endif
}

static inline int cpu_is_msm8930ab(void)
{
#ifdef CONFIG_ARCH_MSM8930
	return read_msm_cpu_type() == MSM_CPU_8930AB;
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

static inline int cpu_is_msm8625(void)
{
#ifdef CONFIG_ARCH_MSM8625
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_8625;
#else
	return 0;
#endif
}

static inline int cpu_is_msm8974(void)
{
#ifdef CONFIG_ARCH_MSM8974
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_8974;
#else
	return 0;
#endif
}

static inline int cpu_is_msm8974pro_aa(void)
{
#ifdef CONFIG_ARCH_MSM8974
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_8974PRO_AA;
#else
	return 0;
#endif
}

static inline int cpu_is_msm8974pro_ab(void)
{
#ifdef CONFIG_ARCH_MSM8974
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_8974PRO_AB;
#else
	return 0;
#endif
}

static inline int cpu_is_msm8974pro_ac(void)
{
#ifdef CONFIG_ARCH_MSM8974
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_8974PRO_AC;
#else
	return 0;
#endif
}

static inline int cpu_is_mpq8092(void)
{
#ifdef CONFIG_ARCH_MPQ8092
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_8092;
#else
	return 0;
#endif

}

static inline int cpu_is_msm8226(void)
{
#ifdef CONFIG_ARCH_MSM8226
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_8226;
#else
	return 0;
#endif
}

static inline int cpu_is_msm8610(void)
{
#ifdef CONFIG_ARCH_MSM8610
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_8610;
#else
	return 0;
#endif
}

static inline int cpu_is_msm8625q(void)
{
#ifdef CONFIG_ARCH_MSM8625
	enum msm_cpu cpu = socinfo_get_msm_cpu();

	BUG_ON(cpu == MSM_CPU_UNKNOWN);
	return cpu == MSM_CPU_8625Q;
#else
	return 0;
#endif
}

static inline int soc_class_is_msm8960(void)
{
	return cpu_is_msm8960() || cpu_is_msm8960ab();
}

static inline int soc_class_is_apq8064(void)
{
	return cpu_is_apq8064() || cpu_is_apq8064ab() || cpu_is_apq8064aa();
}

static inline int soc_class_is_msm8930(void)
{
	return cpu_is_msm8930() || cpu_is_msm8930aa() || cpu_is_msm8930ab() ||
	       cpu_is_msm8627();
}

static inline int soc_class_is_msm8974(void)
{
	return cpu_is_msm8974() || cpu_is_msm8974pro_aa() ||
	       cpu_is_msm8974pro_ab() || cpu_is_msm8974pro_ac();
}

#endif
