/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_STATIC_POWER_MTK6768_H__
#define __MTK_STATIC_POWER_MTK6768_H__

/* #define SPOWER_NOT_READY 1 */

/* #define WITHOUT_LKG_EFUSE */

/* mv */
#define V_OF_FUSE_CPU	900
#define V_OF_FUSE_GPU	800
#define V_OF_FUSE_VCORE	800
#define V_OF_FUSE_MODEM	800
#define V_OF_FUSE_VPU	800
#define T_OF_FUSE		30

/* devinfo offset for each bank */
/* CCI use LL leakage */
#define DEVINFO_IDX_LL 136 /* 07B8 */
#define DEVINFO_IDX_L 136 /* 07B8 */
#define DEVINFO_IDX_CCI 136 /* 07B8 */
#define DEVINFO_IDX_GPU 137 /* 07BC */
#define DEVINFO_IDX_VCORE 137 /* 07BC */
#define DEVINFO_IDX_MODEM 137 /* 07BC */
#define DEVINFO_IDX_VPU 136 /* 07B8 */

#define DEVINFO_OFF_LL 16
#define DEVINFO_OFF_L 8
#define DEVINFO_OFF_CCI 0
#define DEVINFO_OFF_GPU 24
#define DEVINFO_OFF_VCORE 16
#define DEVINFO_OFF_MODEM 8
#define DEVINFO_OFF_VPU 16

/* default leakage value for each bank */
#define DEF_CPULL_LEAKAGE	60
#define DEF_CPUL_LEAKAGE	54
#define DEF_CCI_LEAKAGE		10
#define DEF_GPU_LEAKAGE		32
#define DEF_VCORE_LEAKAGE	41
#define DEF_MODEM_LEAKAGE	39
#define DEF_VPU_LEAKAGE         2

enum {
	MTK_SPOWER_CPULL,
	MTK_SPOWER_CPUL,
	MTK_SPOWER_CCI,
	MTK_SPOWER_GPU,
	MTK_SPOWER_VCORE,
	MTK_SPOWER_MODEM,
	MTK_SPOWER_VPU,
#if 0
	MTK_SPOWER_MDLA,
	MTK_SPOWER_SOC,
	MTK_SPOWER_VSRAM_CPU,
	MTK_SPOWER_VSRAM_OTHERS,
	MTK_SPOWER_VSRAM_MODEM,
#endif
	MTK_SPOWER_MAX
};

enum {
	MTK_LL_LEAKAGE,
	MTK_L_LEAKAGE,
	MTK_CCI_LEAKAGE,
	MTK_GPU_LEAKAGE,
	MTK_VCORE_LEAKAGE,
	MTK_MODEM_LEAKAGE,
	MTK_VPU_LEAKAGE,
#if 0
	MTK_MDLA_LEAKAGE,
	MTK_SOC_LEAKAGE,
	MTK_VSRAM_CPU_LEAKAGE,
	MTK_VSRAM_OTHERS_LEAKAGE,
	MTK_VSRAM_MODEM_LEAKAGE,
#endif
	MTK_LEAKAGE_MAX
};

/* record leakage information that read from efuse */
struct spower_leakage_info {
	const char *name;
	unsigned int devinfo_idx;
	unsigned int devinfo_offset;
	unsigned int value;
	unsigned int v_of_fuse;
	int t_of_fuse;
};

extern struct spower_leakage_info spower_lkg_info[MTK_SPOWER_MAX];

/* efuse mapping */
/* 3967 modify */
#define LL_DEVINFO_DOMAIN (BIT(MTK_SPOWER_CPULL))
#define L_DEVINFO_DOMAIN (BIT(MTK_SPOWER_CPUL))
#define CCI_DEVINFO_DOMAIN (BIT(MTK_SPOWER_CCI))
#define GPU_DEVINFO_DOMAIN (BIT(MTK_SPOWER_GPU))
#define VCORE_DEVINFO_DOMAIN (BIT(MTK_SPOWER_VCORE))
#define MODEM_DEVINFO_DOMAIN (BIT(MTK_SPOWER_MODEM))
#define VPU_DEVINFO_DOMAIN (BIT(MTK_SPOWER_VPU))
#if 0
#define MDLA_DEVINFO_DOMAIN (BIT(MTK_SPOWER_MDLA))
#define SOC_DEVINFO_DOMAIN (BIT(MTK_SPOWER_SOC))
#define VSRAM_CPU_DEVINFO_DOMAIN (BIT(MTK_SPOWER_VSRAM_CPU))
#define VSRAM_OTHERS_DEVINFO_DOMAIN (BIT(MTK_SPOWER_VSRAM_OTHERS))
#define VSRAM_MODEM_DEVINFO_DOMAIN (BIT(MTK_SPOWER_VSRAM_MODEM))
#endif
/* used to calculate total leakage that search from raw table */
#define DEFAULT_CORE_INSTANCE 4
#define DEFAULT_LL_CORE_INSTANCE 6
#define DEFAULT_L_CORE_INSTANCE 2
#define DEFAULT_INSTANCE 1

extern char *spower_name[];
extern char *leakage_name[];
extern int default_leakage[];
extern int devinfo_idx[];
extern int devinfo_offset[];
extern int devinfo_table[];

#endif
