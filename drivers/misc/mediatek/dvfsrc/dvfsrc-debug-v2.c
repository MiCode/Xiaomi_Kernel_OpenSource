// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/soc/mediatek/mtk_dvfsrc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

#include "dvfsrc-helper.h"
#include "dvfsrc-common.h"
#if IS_ENABLED(CONFIG_MTK_DRAMC_LEGACY)
#include <mtk_dramc.h>
#endif

enum dvfsrc_regs {
	DVFSRC_BASIC_CONTROL,
	DVFSRC_SW_REQ1,
	DVFSRC_INT,
	DVFSRC_INT_EN,
	DVFSRC_SW_BW_0,
	DVFSRC_VCORE_REQUEST,
	DVFSRC_LEVEL,
	DVFSRC_LAST,
	DVFSRC_MD_SCENARIO,
	DVFSRC_RECORD_0,
	DVFSRC_RSRV_0,
};

static const int mt6768_regs[] = {
	[DVFSRC_BASIC_CONTROL] = 0x0,
	[DVFSRC_SW_REQ1] = 0x4,
	[DVFSRC_INT] = 0x98,
	[DVFSRC_INT_EN] = 0x9C,
	[DVFSRC_SW_BW_0] = 0x160,
	[DVFSRC_VCORE_REQUEST] = 0x48,
	[DVFSRC_LEVEL] = 0xDC,
	[DVFSRC_LAST] = 0x308,
	[DVFSRC_MD_SCENARIO] = 0x310,
	[DVFSRC_RECORD_0] = 0x400,
	[DVFSRC_RSRV_0] = 0x600,
};

enum dvfsrc_spm_regs {
	SPM_POWERON_CONFIG_EN,
	SPM_PCM_IM_PTR,
	SPM_MD2SPM_DVFS_CON,
	SPM_SW_FLAG,
	SPM_SW_RSV_9,
	SPM_DVFS_EVENT_STA,
	SPM_DVFS_LEVEL,
	SPM_DFS_LEVEL,
	SPM_DVS_LEVEL,
	SPM_DVFS_CMD0,
	SPM_DVFS_CMD1,
	SPM_DVFS_CMD2,
	SPM_DVFS_CMD3,
	SPM_DVFS_CMD4,
};

static const int mt6768_spm_regs[] = {
	[SPM_POWERON_CONFIG_EN] = 0x0,
	[SPM_PCM_IM_PTR] = 0x020,
	[SPM_MD2SPM_DVFS_CON] = 0x43C,
	[SPM_SW_FLAG] = 0x600,
	[SPM_SW_RSV_9] = 0x658,
	[SPM_DVFS_EVENT_STA] = 0x69C,
	[SPM_DVFS_LEVEL] = 0x6A4,
	[SPM_DFS_LEVEL] = 0x6B0,
	[SPM_DVS_LEVEL] = 0x6B4,
	[SPM_DVFS_CMD0] = 0x710,
	[SPM_DVFS_CMD1] = 0x714,
	[SPM_DVFS_CMD2] = 0x718,
	[SPM_DVFS_CMD3] = 0x71C,
	[SPM_DVFS_CMD4] = 0x720,
};

#define DVFSRC_TARGET_LEVEL(x)	(((x) >> 0) & 0x0000ffff)
#define DVFSRC_CURRENT_LEVEL(x)	(((x) >> 16) & 0x0000ffff)

static u32 dvfsrc_read(struct mtk_dvfsrc *dvfs, u32 reg, u32 offset)
{
	return readl(dvfs->regs + dvfs->dvd->config->regs[reg] + offset);
}

static u32 spm_read(struct mtk_dvfsrc *dvfs, u32 reg)
{
	return readl(dvfs->spm_regs + dvfs->dvd->config->spm_regs[reg]);
}

static char *dvfsrc_dump_record(struct mtk_dvfsrc *dvfsrc,
	char *p, u32 size)
{
	int i, rec_offset, offset;
	char *buff_end = p + size;

	p += sprintf(p, "%-17s: 0x%08x\n",
			"DVFSRC_LAST",
			dvfsrc_read(dvfsrc, DVFSRC_LAST, 0));

	rec_offset = 0xC;

	for (i = 0; i < 8; i++) {
		offset = i * rec_offset;
		p += snprintf(p, buff_end - p,
			"[%d]%-14s: %08x,%08x,%08x\n",
			i,
			"DVFSRC_REC 0~2",
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x0),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x4),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x8));
	}
	p += snprintf(p, buff_end - p, "\n");

	return p;
}

static char *dvfsrc_dump_reg(struct mtk_dvfsrc *dvfsrc, char *p, u32 size)
{
	char *buff_end = p + size;

	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
		"BASIC_CONTROL",
		dvfsrc_read(dvfsrc, DVFSRC_BASIC_CONTROL, 0x0));
	p += snprintf(p, buff_end - p,
		"%-16s: %08x, %08x\n",
		"SW_REQ 1~2",
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x0),
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x4));

	p += snprintf(p, buff_end - p, "%-16s: %d, %d, %d, %d, %d\n",
		"SW_BW_0~4",
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x0),
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x4),
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x8),
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0xC),
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x10));

	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
		"INT",
		dvfsrc_read(dvfsrc, DVFSRC_INT, 0x0));

	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
		"INT_EN",
		dvfsrc_read(dvfsrc, DVFSRC_INT_EN, 0x0));

	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
		"MD_SCENARIO",
		dvfsrc_read(dvfsrc, DVFSRC_MD_SCENARIO, 0x0));

	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
		"MD_RSV0",
		dvfsrc_read(dvfsrc, DVFSRC_RSRV_0, 0x0));

	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
		"SCP_VCORE_REQ",
		dvfsrc_read(dvfsrc, DVFSRC_VCORE_REQUEST, 0x0));

	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
		"CURRENT_LEVEL",
		dvfsrc_read(dvfsrc, DVFSRC_LEVEL, 0x0));

	p += snprintf(p, buff_end - p, "%-16s: %d\n",
		"FORCE_OPP_IDX",
		dvfsrc->force_opp_idx);

	p += snprintf(p, buff_end - p, "%-16s: %d\n",
		"CURR_DVFS_OPP",
		mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_DVFS_OPP));

	p += snprintf(p, buff_end - p, "%-16s: %d\n",
		"CURR_VCORE_OPP",
		mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_VCORE_OPP));

	p += snprintf(p, buff_end - p, "%-16s: %d\n",
		"CURR_DRAM_OPP",
		mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_DRAM_OPP));

	p += snprintf(p, buff_end - p, "\n");

	return p;
}

static char *dvfsrc_dump_spm_info(struct mtk_dvfsrc *dvfsrc,
	char *p, u32 size)
{
	char *buff_end = p + size;

	if (!dvfsrc->spm_regs)
		return p;

	p += snprintf(p, buff_end - p, "%-24s: 0x%08x\n",
			"POWERON_CONFIG_EN",
			spm_read(dvfsrc, SPM_POWERON_CONFIG_EN));
	p += snprintf(p, buff_end - p, "%-24s: 0x%08x\n",
			"SPM_SW_FLAG",
			spm_read(dvfsrc, SPM_SW_FLAG));
	p += snprintf(p, buff_end - p, "%-24s: 0x%08x\n",
			"SPM_SW_RSV_9",
			spm_read(dvfsrc, SPM_SW_RSV_9));
	p += snprintf(p, buff_end - p, "%-24s: 0x%08x\n",
			"MD2SPM_DVFS_CON",
			spm_read(dvfsrc, SPM_MD2SPM_DVFS_CON));
	p += snprintf(p, buff_end - p, "%-24s: 0x%08x\n",
			"SPM_DVFS_EVENT_STA",
			spm_read(dvfsrc, SPM_DVFS_EVENT_STA));
	p += snprintf(p, buff_end - p, "%-24s: 0x%08x\n",
			"SPM_DVFS_LEVEL",
			spm_read(dvfsrc, SPM_DVFS_LEVEL));
	p += snprintf(p, buff_end - p, "%-24s: 0x%08x\n",
			"SPM_DFS_LEVEL",
			spm_read(dvfsrc, SPM_DFS_LEVEL));
	p += snprintf(p, buff_end - p, "%-24s: 0x%08x\n",
			"SPM_DVS_LEVEL",
			spm_read(dvfsrc, SPM_DVS_LEVEL));
	p += snprintf(p, buff_end - p,
		"%-24s: 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
		"SPM_DVFS_CMD0~4",
		spm_read(dvfsrc, SPM_DVFS_CMD0),
		spm_read(dvfsrc, SPM_DVFS_CMD1),
		spm_read(dvfsrc, SPM_DVFS_CMD2),
		spm_read(dvfsrc, SPM_DVFS_CMD3),
		spm_read(dvfsrc, SPM_DVFS_CMD4));

	p += snprintf(p, buff_end - p, "%-24s: 0x%08x\n",
			"PCM_IM_PTR",
			spm_read(dvfsrc, SPM_PCM_IM_PTR));
	p += snprintf(p, buff_end - p, "\n");

	return p;
}

static int dvfsrc_query_request_status(struct mtk_dvfsrc *dvfsrc, u32 id)
{
	return 0;
}


const struct dvfsrc_config mt6768_dvfsrc_config = {
	.regs = mt6768_regs,
	.spm_regs = mt6768_spm_regs,
	.dump_record = dvfsrc_dump_record,
	.dump_reg = dvfsrc_dump_reg,
	.dump_spm_info = dvfsrc_dump_spm_info,
	.query_request = dvfsrc_query_request_status,
};

