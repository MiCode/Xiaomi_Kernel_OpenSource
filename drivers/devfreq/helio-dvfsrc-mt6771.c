/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/bitops.h>

#include <helio-dvfsrc.h>
#include <mt-plat/mtk_devinfo.h>
#include "mtk_dvfsrc_reg.h"
#include "mtk_spm_vcore_dvfs.h"

#include <ext_wd_drv.h>

__weak int mtk_rgu_cfg_dvfsrc(int enable) { return 0; }

static struct reg_config dvfsrc_init_configs[][128] = {
	/* HELIO_DVFSRC_DRAM_LP4X_2CH */
	{
		{ -1, 0 },
	},
	/* HELIO_DVFSRC_DRAM_LP4X_1CH */
	{
		{ -1, 0 },
	},
	/* HELIO_DVFSRC_DRAM_LP3X_1CH */
	{
		{ -1, 0 },
	},
};

static struct reg_config dvfsrc_suspend_configs[][4] = {
	/* HELIO_DVFSRC_DRAM_LP4X_2CH */
	{
		{ -1, 0 },
	},
	/* HELIO_DVFSRC_DRAM_LP4X_1CH */
	{
		{ -1, 0 },
	},
	/* HELIO_DVFSRC_DRAM_LP3X_1CH */
	{
		{ -1, 0 },
	},
};
static struct reg_config dvfsrc_resume_configs[][4] = {
	/* HELIO_DVFSRC_DRAM_LP4X_2CH */
	{
		{ -1, 0 },
	},
	/* HELIO_DVFSRC_DRAM_LP4X_1CH */
	{
		{ -1, 0 },
	},
	/* HELIO_DVFSRC_DRAM_LP3X_1CH */
	{
		{ -1, 0 },
	},
};
void helio_dvfsrc_platform_init(struct helio_dvfsrc *dvfsrc)
{
	dvfsrc->flag = spm_dvfs_flag_init();
	dvfsrc->dram_type = 0; /* __spm_get_dram_type(); */
	/* dvfsrc->dram_issue = get_devinfo_with_index(138) & BIT(8); */
	dvfsrc->init_config = dvfsrc_init_configs[dvfsrc->dram_type];
	dvfsrc->suspend_config = dvfsrc_suspend_configs[dvfsrc->dram_type];
	dvfsrc->resume_config = dvfsrc_resume_configs[dvfsrc->dram_type];

	dvfsrc->vcore_dvs = SPM_VCORE_DVS_EN;
	dvfsrc->ddr_dfs = SPM_DDR_DFS_EN;
	dvfsrc->mm_clk = SPM_MM_CLK_EN;

	mtk_rgu_cfg_dvfsrc(1);
}

int dvfsrc_transfer_to_dram_level(int data)
{
	if (data >= 0x8000)
		return 2;
	else if (data >= 0x4000)
		return 1;
	return 0;
}

int dvfsrc_transfer_to_vcore_level(int data)
{
	if (data >= 800)
		return 1;
	return 0;
}

