/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#ifndef __MTK_POWER_GS_V1_H__
#define __MTK_POWER_GS_V1_H__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#define REMAP_SIZE_MASK     0xFFF

enum LPM_PWR_GS_TYPE {
	LPM_PWR_GS_TYPE_SUSPEND = 0,
	LPM_PWR_GS_TYPE_VCORELP_26M,
	LPM_PWR_GS_TYPE_VCORELP,
	LPM_PWR_GS_TYPE_MAX
};

enum GS_FLAGS {
	GS_PMIC = (0x1 << 0),
	GS_CG   = (0x1 << 1),
	GS_DCM  = (0x1 << 2),
	/* GS_ALL will need to be modified, if the gs_dump_flag is changed */
	GS_ALL  = (GS_PMIC | GS_CG | GS_DCM),
};

/* pmic */
struct lpm_gs_pmic_user {
	char *name;
	unsigned int array_sz;
	const unsigned int *array;
};

struct lpm_gs_pmic {
	unsigned int type;
	const char *regulator;
	const char *pwr_domain;
	struct lpm_gs_pmic_user user[LPM_PWR_GS_TYPE_MAX];
};

struct lpm_gs_pmic_info {
	int (*attach)(struct lpm_gs_pmic *p);
	struct lpm_gs_pmic **pmic;
};

/* DCM, CG */
struct lpm_gs_clk_user {
	char *name;
	unsigned int array_sz;
	const unsigned int *array;
};

struct lpm_gs_clk {
	unsigned int type;
	char *name;
	struct lpm_gs_clk_user user[LPM_PWR_GS_TYPE_MAX];
};

struct lpm_gs_clk_info {
	int (*attach)(struct lpm_gs_clk *p);
	struct lpm_gs_clk **dcm;
};

struct phys_to_virt_table {
	void __iomem *va;
	unsigned int pa;
};

struct base_remap {
	unsigned int table_pos;
	unsigned int table_size;
	struct phys_to_virt_table *table;
};

int lpm_pwr_gs_common_init(void);

void lpm_pwr_gs_common_deinit(void);

#endif
