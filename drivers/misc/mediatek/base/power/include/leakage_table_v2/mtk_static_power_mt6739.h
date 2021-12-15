/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_STATIC_POWER_MTK6739_H__
#define __MTK_STATIC_POWER_MTK6739_H__

/* mv */
#define V_OF_FUSE_CPU		1000
#define V_OF_FUSE_GPU		900
#define V_OF_FUSE_VCORE		800
#define V_OF_FUSE_VMD		800
#define V_OF_FUSE_MODEM		750
#define T_OF_FUSE		30

/* No efuse for leakage power in MT6739 */
#define WITHOUT_LKG_EFUSE

/* devinfo offset for each bank */
/* CCI use LL leakage */
#define DEVINFO_IDX_LL 136 /* 07B8 */
#define DEVINFO_IDX_L 136 /* 07b8 */
#define DEVINFO_IDX_CCI 136 /* 07B8 */
#define DEVINFO_IDX_GPU 137 /* 07BC */
#define DEVINFO_IDX_VCORE 137 /* 07BC */
#define DEVINFO_IDX_VMD 137 /* 07BC */
#define DEVINFO_IDX_MODEM 137 /* 07BC */

#define DEVINFO_OFF_LL 16
#define DEVINFO_OFF_L 8
#define DEVINFO_OFF_CCI 16
#define DEVINFO_OFF_GPU 24
#define DEVINFO_OFF_VCORE 16
#define DEVINFO_OFF_VMD 0
#define DEVINFO_OFF_MODEM 8

/* fake efuse */
/* default leakage value for each bank */
#define DEF_CPULL_LEAKAGE	23	/* (5277*4 + 3357) / 1024 = 23.8 */
#if 0					/* no CPUL/CCI in mt6739 */
#define DEF_CPUL_LEAKAGE	0
#define DEF_CCI_LEAKAGE		0
#endif
#define DEF_GPU_LEAKAGE		7	/* 7790  / 1024 = 7.6  */
#define DEF_VCORE_LEAKAGE	43	/* 44146 / 1024 = 43.1 */
#define DEF_VMD_LEAKAGE		2	/* 2317  / 1024 = 2.2  */
#define DEF_MODEM_LEAKAGE	3	/* 3645  / 1024 = 3.5  */

enum {
	MTK_SPOWER_CPULL,
	MTK_SPOWER_CPULL_CLUSTER,

	MTK_SPOWER_GPU,
	MTK_SPOWER_VCORE,
	MTK_SPOWER_VMD,
	MTK_SPOWER_MODEM,

	MTK_SPOWER_MAX
};

enum {
	MTK_LL_LEAKAGE,
	MTK_GPU_LEAKAGE,
	MTK_VCORE_LEAKAGE,
	MTK_VMD_LEAKAGE,
	MTK_MODEM_LEAKAGE,

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
#define LL_DEVINFO_DOMAIN (BIT(MTK_SPOWER_CPULL) | BIT(MTK_SPOWER_CPULL_CLUSTER))
#define L_DEVINFO_DOMAIN (BIT(MTK_SPOWER_CPUL) | BIT(MTK_SPOWER_CPUL_CLUSTER))
#define CCI_DEVINFO_DOMAIN (BIT(MTK_SPOWER_CPULL) | BIT(MTK_SPOWER_CPULL_CLUSTER))
#define GPU_DEVINFO_DOMAIN (BIT(MTK_SPOWER_GPU))
#define VCORE_DEVINFO_DOMAIN (BIT(MTK_SPOWER_VCORE))
#define VMD_DEVINFO_DOMAIN (BIT(MTK_SPOWER_VMD))
#define MODEM_DEVINFO_DOMAIN (BIT(MTK_SPOWER_MODEM))

/* used to calculate total leakage that search from raw table */
#define BIG_CORE_INSTANCE 2
#define DEFAULT_CORE_INSTANCE 4
#define DEFAULT_INSTANCE 1

extern char *spower_name[];
extern char *leakage_name[];
extern int default_leakage[];
extern int devinfo_idx[];
extern int devinfo_offset[];
extern int devinfo_table[];

#endif
