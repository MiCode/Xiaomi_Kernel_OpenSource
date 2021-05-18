/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_STATIC_POWER_MTK6785_H__
#define __MTK_STATIC_POWER_MTK6785_H__

/* #define SPOWER_NOT_READY 1 */

/* #define WITHOUT_LKG_EFUSE */
#define PRECISE_NODE

/* mv */
#define V_OF_FUSE_CPU	950
#define V_OF_FUSE_GPU	900
#define V_OF_FUSE_VCORE	800
#define V_OF_FUSE_MODEM	875
#define V_OF_FUSE_VSRAM_PROC12	1050
#define V_OF_FUSE_VSRAM_PROC11	1050
#define V_OF_FUSE_VSRAM_GPU	1000
#define V_OF_FUSE_VSRAM_CORE	900 /* SOC + VPU */
//TODO
#define V_OF_FUSE_VSRAM_MODEM    975
#define V_OF_FUSE_VCORE_OFF	550
#define V_OF_FUSE_MODEM_OFF	550

#define T_OF_FUSE		30

/* devinfo offset for each bank */
/* CCI use LL leakage */
#define DEVINFO_IDX_LL 136 /* 07B8 */
#define DEVINFO_IDX_L 136 /* 07B8 */
#define DEVINFO_IDX_CCI 137 /* 07BC */
#define DEVINFO_IDX_GPU 137 /* 07BC */
#define DEVINFO_IDX_VCORE 137 /* 07BC */
#define DEVINFO_IDX_MODEM 137 /* 07BC */
#define DEVINFO_IDX_VCORE_OFF 136 /* 07B8 */
#define DEVINFO_IDX_VSRAM_PROC12 135 /* 07B4 */
#define DEVINFO_IDX_VSRAM_PROC11 135 /* 07B4 */
#define DEVINFO_IDX_VSRAM_GPU 136    /* 07B8 */
#define DEVINFO_IDX_VSRAM_CORE 135 /* 07B4 */
#define DEVINFO_IDX_VSRAM_MODEM 135 /* 07B4 */
#define DEVINFO_IDX_MODEM_OFF 138 /* 07C0 */

#define DEVINFO_OFF_LL 0
#define DEVINFO_OFF_L 8
#define DEVINFO_OFF_CCI 24
#define DEVINFO_OFF_GPU 16
#define DEVINFO_OFF_VCORE 8
#define DEVINFO_OFF_MODEM 0
#define DEVINFO_OFF_VCORE_OFF 16
#define DEVINFO_OFF_VSRAM_PROC12 24
#define DEVINFO_OFF_VSRAM_PROC11 16
#define DEVINFO_OFF_VSRAM_GPU 24
#define DEVINFO_OFF_VSRAM_CORE 8
#define DEVINFO_OFF_VSRAM_MODEM 0
#define DEVINFO_OFF_MODEM_OFF 24

/* default leakage value for each bank */
#define DEF_CPULL_LEAKAGE       59
#define DEF_CPUL_LEAKAGE	100
#define DEF_CCI_LEAKAGE		14
#define DEF_GPU_LEAKAGE		53
#define DEF_VCORE_LEAKAGE	34
#define DEF_MODEM_LEAKAGE	39
#define DEF_VSRAM_PROC12_LEAKAGE         3
#define DEF_VSRAM_PROC11_LEAKAGE         6
#define DEF_VSRAM_GPU_LEAKAGE            1
#define DEF_VSRAM_CORE_LEAKAGE         3

//TODO CCJ
#define DEF_VSRAM_MODEM_LEAKAGE		3
#define DEF_VCORE_OFF_LEAKAGE         6
#define DEF_MODEM_OFF_LEAKAGE		6

enum {
	MTK_SPOWER_CPULL,
	MTK_SPOWER_CPUL,
	MTK_SPOWER_CCI,
	MTK_SPOWER_GPU,
	MTK_SPOWER_VCORE,
	MTK_SPOWER_MODEM,
#if 0 //CCJ
	MTK_SPOWER_VPU,
#endif
	MTK_SPOWER_VSRAM_PROC12,
	MTK_SPOWER_VSRAM_PROC11,
	MTK_SPOWER_VSRAM_GPU,
	MTK_SPOWER_VSRAM_CORE,
	MTK_SPOWER_VSRAM_MODEM,
	MTK_SPOWER_VCORE_OFF,
	MTK_SPOWER_MODEM_OFF,
	MTK_SPOWER_MAX
};

enum {
	MTK_LL_LEAKAGE,
	MTK_L_LEAKAGE,
	MTK_CCI_LEAKAGE,
	MTK_GPU_LEAKAGE,
	MTK_VCORE_LEAKAGE,
	MTK_MODEM_LEAKAGE,
#if 0 //CCJ
	MTK_VPU_LEAKAGE,
#endif
	MTK_VSRAM_PROC12_LEAKAGE,
	MTK_VSRAM_PROC11_LEAKAGE,
	MTK_VSRAM_GPU_LEAKAGE,
	MTK_VSRAM_CORE_LEAKAGE,
	MTK_VSRAM_MODEM_LEAKAGE,
	MTK_VCORE_OFF_LEAKAGE,
	MTK_MODEM_OFF_LEAKAGE,
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
#define VCORE_OFF_DEVINFO_DOMAIN (BIT(MTK_SPOWER_VCORE_OFF))
#define MODEM_OFF_DEVINFO_DOMAIN (BIT(MTK_SPOWER_MODEM_OFF))
#define VSRAM_PROC12_DEVINFO_DOMAIN (BIT(MTK_SPOWER_VSRAM_PROC12))
#define VSRAM_PROC11_DEVINFO_DOMAIN (BIT(MTK_SPOWER_VSRAM_PROC11))
#define VSRAM_GPU_DEVINFO_DOMAIN (BIT(MTK_SPOWER_VSRAM_GPU))
#define VSRAM_CORE_DEVINFO_DOMAIN (BIT(MTK_SPOWER_VSRAM_CORE))
#define VSRAM_MODEM_DEVINFO_DOMAIN (BIT(MTK_SPOWER_VSRAM_MODEM))

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
