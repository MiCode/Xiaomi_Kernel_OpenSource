/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/string.h>

#include <mtk_spm_internal.h>
#include <mtk_spm_reg.h>
#include <mtk_sleep_reg_md_reg.h>
#include <sleep_def.h>

int spm_dvfs_flag_init(int dvfsrc_en)
{
	int flag = SPM_FLAG_RUN_COMMON_SCENARIO;
	int dvfsrc_flag = dvfsrc_en >> 1;

	if (dvfsrc_en & 1) {
		if (dvfsrc_flag & 0x1)
			flag |= SPM_FLAG_DISABLE_VCORE_DVS;

		if (dvfsrc_flag & 0x2)
			flag |= SPM_FLAG_DISABLE_VCORE_DFS;

		return flag;
	} else {
		return SPM_FLAG_RUN_COMMON_SCENARIO |
			SPM_FLAG_DISABLE_VCORE_DVS |
			SPM_FLAG_DISABLE_VCORE_DFS;
	}
}

u32 spm_get_dvfs_level(void)
{
	return spm_read(SPM_DVFS_LEVEL) & 0xFFFF;
}

u32 spm_get_pcm_reg9_data(void)
{
	return spm_read(SPM_DVFS_STA);
}

u32 spm_vcorefs_get_MD_status(void)
{
	return spm_read(MD2SPM_DVFS_CON);
}

u32 spm_vcorefs_get_md_srcclkena(void)
{
	return spm_read(PCM_REG13_DATA) & (1U << 8);
}

int is_spm_enabled(void)
{
	return spm_read(SPM_PC_STA) != 0 ? 1 : 0;
}

void get_spm_reg(char *p)
{
	p += sprintf(p, "%-24s: 0x%08x\n",
			"POWERON_CONFIG_EN",
			spm_read(POWERON_CONFIG_EN));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"SPM_SW_FLAG_0",
			spm_read(SPM_SW_FLAG_0));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"SPM_PC_STA",
			spm_read(SPM_PC_STA));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"SPM_DVFS_LEVEL",
			spm_read(SPM_DVFS_LEVEL));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"SPM_DVS_DFS_LEVEL",
			spm_read(SPM_DVS_DFS_LEVEL));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"SPM_DVFS_STA",
			spm_read(SPM_DVFS_STA));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"SPM_DVFS_MISC",
			spm_read(SPM_DVFS_MISC));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"SPM_VCORE_DVFS_SHORTCUT00",
			spm_read(SPM_VCORE_DVFS_SHORTCUT00));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x\n",
			"SPM_VCORE_DVFS_SHORTCUT_STAx",
			spm_read(SPM_VCORE_DVFS_SHORTCUT_STA0),
			spm_read(SPM_VCORE_DVFS_SHORTCUT_STA1));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x\n",
			"SPM_DVFS_HISTORY_STAx",
			spm_read(SPM_DVFS_HISTORY_STA0),
			spm_read(SPM_DVFS_HISTORY_STA1));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			"SPM_DVFS_CMD0~3",
			spm_read(SPM_DVFS_CMD0), spm_read(SPM_DVFS_CMD1),
			spm_read(SPM_DVFS_CMD2), spm_read(SPM_DVFS_CMD3));
}

