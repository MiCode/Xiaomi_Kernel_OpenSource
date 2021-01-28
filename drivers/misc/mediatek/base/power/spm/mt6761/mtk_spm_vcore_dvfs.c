// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017 MediaTek Inc.
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
			flag |= SPM_FLAG_DIS_VCORE_DVS;

		if (dvfsrc_flag & 0x2)
			flag |= SPM_FLAG_DIS_VCORE_DFS;

		return flag;
	} else
		return SPM_FLAG_RUN_COMMON_SCENARIO | SPM_FLAG_DIS_VCORE_DVS |
			SPM_FLAG_DIS_VCORE_DFS;
}


u32 spm_get_dvfs_level(void)
{
	return spm_read(SPM_SW_RSV_9) & 0xFFFF;
}

u32 spm_get_dvfs_final_level(void)
{
	int event_sta = spm_read(SPM_DVFS_EVENT_STA) & 0xFFFF;
	int rsv9 = spm_read(SPM_SW_RSV_9) & 0xFFFF;

	if (event_sta != 0)
		return min(rsv9, event_sta);
	else
		return rsv9;
}

u32 spm_get_pcm_reg9_data(void)
{
	return spm_read(PCM_REG9_DATA);
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
	return spm_read(PCM_REG15_DATA) != 0 ? 1 : 0;
}

void get_spm_reg(char *p)
{
	p += sprintf(p, "%-24s: 0x%08x\n",
			"POWERON_CONFIG_EN",
			spm_read(POWERON_CONFIG_EN));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"SPM_SW_FLAG",
			spm_read(SPM_SW_FLAG));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"SPM_SW_RSV_9",
			spm_read(SPM_SW_RSV_9));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"MD2SPM_DVFS_CON",
			spm_read(MD2SPM_DVFS_CON));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"SPM_DVFS_EVENT_STA",
			spm_read(SPM_DVFS_EVENT_STA));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"SPM_DVFS_LEVEL",
			spm_read(SPM_DVFS_LEVEL));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"SPM_DFS_LEVEL",
			spm_read(SPM_DFS_LEVEL));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"SPM_DVS_LEVEL",
			spm_read(SPM_DVS_LEVEL));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"SPM_DVFS_CON1",
			spm_read(SPM_DVFS_CON1));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"SPM_DVFS_CON1_STA",
			spm_read(SPM_DVFS_CON1_STA));
	p += sprintf(p, "%-24s: 0x%08x\n",
			"SPM_ACK_CHK_TIMER2",
			spm_read(SPM_ACK_CHK_TIMER2));

	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			"PCM_REG_DATA_0~3",
			spm_read(PCM_REG0_DATA), spm_read(PCM_REG1_DATA),
			spm_read(PCM_REG2_DATA), spm_read(PCM_REG3_DATA));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			"PCM_REG_DATA_4~7",
			spm_read(PCM_REG4_DATA), spm_read(PCM_REG5_DATA),
			spm_read(PCM_REG6_DATA), spm_read(PCM_REG7_DATA));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			"PCM_REG_DATA_8~11",
			spm_read(PCM_REG8_DATA), spm_read(PCM_REG9_DATA),
			spm_read(PCM_REG10_DATA), spm_read(PCM_REG11_DATA));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			"PCM_REG_DATA_12~15",
			spm_read(PCM_REG12_DATA), spm_read(PCM_REG13_DATA),
			spm_read(PCM_REG14_DATA), spm_read(PCM_REG15_DATA));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			"SPM_DVFS_CMD0~4",
			spm_read(SPM_DVFS_CMD0), spm_read(SPM_DVFS_CMD1),
			spm_read(SPM_DVFS_CMD2), spm_read(SPM_DVFS_CMD3),
			spm_read(SPM_DVFS_CMD4));
	p += sprintf(p, "%-24s: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			"MD_SPM_DVFS_CMD16~19",
			spm_read(SLEEP_REG_MD_SPM_DVFS_CMD16),
			spm_read(SLEEP_REG_MD_SPM_DVFS_CMD17),
			spm_read(SLEEP_REG_MD_SPM_DVFS_CMD18),
			spm_read(SLEEP_REG_MD_SPM_DVFS_CMD19));
	p += sprintf(p, "%-24s: 0x%08x (%u)\n",
			"PCM_IM_PTR",
			spm_read(PCM_IM_PTR), spm_read(PCM_IM_LEN));
}

void spm_freq_hopping_cmd(int gps_on)
{
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);

	mt_secure_call(MTK_SIP_KERNEL_SPM_VCOREFS_ARGS,
			VCOREFS_SMC_CMD_4, gps_on, 0, 0);

	spin_unlock_irqrestore(&__spm_lock, flags);
}
