/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2009-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _ARCH_ARM_MACH_MSM_SOCINFO_H_
#define _ARCH_ARM_MACH_MSM_SOCINFO_H_

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/of_fdt.h>
#include <linux/of.h>

#include <asm/cputype.h>
/*
 * SOC version type with major number in the upper 16 bits and minor
 * number in the lower 16 bits.  For example:
 *   1.0 -> 0x00010000
 *   2.3 -> 0x00020003
 */
#define SOCINFO_VERSION_MAJOR(ver) (((ver) & 0xffff0000) >> 16)
#define SOCINFO_VERSION_MINOR(ver) ((ver) & 0x0000ffff)
#define SOCINFO_VERSION(maj, min)  ((((maj) & 0xffff) << 16)|((min) & 0xffff))

#ifdef CONFIG_OF
#define of_board_is_cdp()	of_machine_is_compatible("qcom,cdp")
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
#define of_board_is_skuf()	of_machine_is_compatible("qcom,skuf")
#define of_board_is_sbc()	of_machine_is_compatible("qcom,sbc")

#define machine_is_msm8974()	of_machine_is_compatible("qcom,msm8974")

#define early_machine_is_msm8916()	\
	of_flat_dt_is_compatible(of_get_flat_dt_root(), "qcom,msm8916")
#define early_machine_is_apq8084()	\
	of_flat_dt_is_compatible(of_get_flat_dt_root(), "qcom,apq8084")
#define early_machine_is_msm8996()	\
	of_flat_dt_is_compatible(of_get_flat_dt_root(), "qcom,msm8996")
#define early_machine_is_msm8996_auto()	\
	of_flat_dt_is_compatible(of_get_flat_dt_root(), "qcom,msm8996-cdp")
#define early_machine_is_sm8150()	\
	of_flat_dt_is_compatible(of_get_flat_dt_root(), "qcom,sm8150")
#define early_machine_is_sa8150()	\
	of_flat_dt_is_compatible(of_get_flat_dt_root(), "qcom,sa8150")
#define early_machine_is_kona()	\
	of_flat_dt_is_compatible(of_get_flat_dt_root(), "qcom,kona")
#define early_machine_is_lito()	\
	of_flat_dt_is_compatible(of_get_flat_dt_root(), "qcom,lito")
#define early_machine_is_bengal()	\
	of_flat_dt_is_compatible(of_get_flat_dt_root(), "qcom,bengal")
#define early_machine_is_bengalp()	\
	of_flat_dt_is_compatible(of_get_flat_dt_root(), "qcom,bengalp")
#define early_machine_is_lagoon()	\
	of_flat_dt_is_compatible(of_get_flat_dt_root(), "qcom,lagoon")
#define early_machine_is_scuba()	\
	of_flat_dt_is_compatible(of_get_flat_dt_root(), "qcom,scuba")
#define early_machine_is_sdmshrike()	\
	of_flat_dt_is_compatible(of_get_flat_dt_root(), "qcom,sdmshrike")
#define early_machine_is_sm6150()	\
	of_flat_dt_is_compatible(of_get_flat_dt_root(), "qcom,sm6150")
#define early_machine_is_qcs405()	\
	of_flat_dt_is_compatible(of_get_flat_dt_root(), "qcom,qcs405")
#define early_machine_is_sdxprairie()	\
	of_flat_dt_is_compatible(of_get_flat_dt_root(), "qcom,sdxprairie")
#define early_machine_is_sdmmagpie()	\
	of_flat_dt_is_compatible(of_get_flat_dt_root(), "qcom,sdmmagpie")
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
#define of_board_is_skuf()		0
#define of_board_is_sbc()		0

#define machine_is_msm8974()		0

#define early_machine_is_msm8916()	0
#define early_machine_is_apq8084()	0
#define early_machine_is_msm8996()	0
#define early_machine_is_sm8150()	0
#define early_machine_is_sa8150()	0
#define early_machine_is_kona()		0
#define early_machine_is_lito()		0
#define early_machine_is_bengal()	0
#define early_machine_is_bengalp()	0
#define early_machine_is_lagoon()	0
#define early_machine_is_scuba()	0
#define early_machine_is_sdmshrike()	0
#define early_machine_is_sm6150()	0
#define early_machine_is_qcs405()	0
#define early_machine_is_sdxprairie()	0
#define early_machine_is_sdmmagpie()	0
#endif

#define PLATFORM_SUBTYPE_MDM	1
#define PLATFORM_SUBTYPE_INTERPOSERV3 2
#define PLATFORM_SUBTYPE_SGLTE	6

#define SMEM_IMAGE_VERSION_TABLE	469
#define SMEM_HW_SW_BUILD_ID		137
enum msm_cpu {
	MSM_CPU_UNKNOWN = 0,
	MSM_CPU_8960,
	MSM_CPU_8960AB,
	MSM_CPU_8064,
	MSM_CPU_8974,
	MSM_CPU_8974PRO_AA,
	MSM_CPU_8974PRO_AB,
	MSM_CPU_8974PRO_AC,
	MSM_CPU_8916,
	MSM_CPU_8084,
	MSM_CPU_8996,
	MSM_CPU_SM8150,
	MSM_CPU_SA8150,
	MSM_CPU_KONA,
	MSM_CPU_LITO,
	MSM_CPU_BENGAL,
	MSM_CPU_BENGALP,
	MSM_CPU_LAGOON,
	MSM_CPU_SCUBA,
	MSM_CPU_SDMSHRIKE,
	MSM_CPU_SM6150,
	MSM_CPU_QCS405,
	SDX_CPU_SDXPRAIRIE,
	MSM_CPU_SDMMAGPIE,
};

struct msm_soc_info {
	enum msm_cpu generic_soc_type;
	char *soc_id_string;
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
char *socinfo_get_id_string(void);
uint32_t socinfo_get_platform_type(void);
uint32_t socinfo_get_platform_subtype(void);
uint32_t socinfo_get_platform_version(void);
uint32_t socinfo_get_serial_number(void);
enum pmic_model socinfo_get_pmic_model(void);
uint32_t socinfo_get_pmic_die_revision(void);
int __init socinfo_init(void) __must_check;

#endif
