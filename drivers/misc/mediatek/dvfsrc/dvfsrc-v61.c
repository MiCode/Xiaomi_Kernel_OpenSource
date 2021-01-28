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

#include "dvfsrc.h"
#include "dvfsrc-opp.h"
#include <memory/mediatek/dramc.h>

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

static const int mt6761_regs[] = {
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

#define DVFSRC_TARGET_LEVEL(x)	(((x) >> 0) & 0x0000ffff)
#define DVFSRC_CURRENT_LEVEL(x)	(((x) >> 16) & 0x0000ffff)

static u32 dvfsrc_read(struct mtk_dvfsrc *dvfs, u32 reg, u32 offset)
{
	return readl(dvfs->regs + dvfs->dvd->config->regs[reg] + offset);
}

static int dvfsrc_get_current_level(struct mtk_dvfsrc *dvfsrc)
{
	u32 curr_level;

	curr_level = dvfsrc_read(dvfsrc, DVFSRC_LEVEL, 0x0);
	curr_level = ffs(DVFSRC_CURRENT_LEVEL(curr_level));
	if ((curr_level > 0) && (curr_level <= dvfsrc->opp_desc->num_opp))
		return curr_level - 1;
	else
		return 0;
}

static u32 dvfsrc_get_current_rglevel(struct mtk_dvfsrc *dvfsrc)
{
	u32 curr_level;

	curr_level = dvfsrc_read(dvfsrc, DVFSRC_LEVEL, 0x0);

	return DVFSRC_CURRENT_LEVEL(curr_level);
}

static char *dvfsrc_dump_info(struct mtk_dvfsrc *dvfsrc, char *p, u32 size)
{
	int vcore_uv = 0;
	char *buff_end = p + size;

	if (dvfsrc->vcore_power)
		vcore_uv = regulator_get_voltage(dvfsrc->vcore_power);

	p += snprintf(p, buff_end - p, "%-10s: %-8u uv\n",
			"Vcore", vcore_uv);
#if IS_ENABLED(CONFIG_MTK_DRAMC)
	p += snprintf(p, buff_end - p, "%-10s: %-8u khz\n",
			"DDR", mtk_dramc_get_data_rate() * 1000);
#endif
	p += snprintf(p, buff_end - p, "\n");

	return p;
}

static char *dvfsrc_dump_record(struct mtk_dvfsrc *dvfsrc, char *p, u32 size)
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

static int dvfsrc_query_request_status(struct mtk_dvfsrc *dvfsrc, u32 id)
{
	return 0;
}

static void dvfsrc_force_opp(struct mtk_dvfsrc *dvfsrc, u32 opp)
{
	dvfsrc->force_opp_idx = opp;
	mtk_dvfsrc_send_request(dvfsrc->dev->parent,
		MTK_DVFSRC_CMD_FORCE_OPP_REQUEST,
		opp);
}

const struct dvfsrc_config mt6761_dvfsrc_config = {
	.regs = mt6761_regs,
	.dump_info = dvfsrc_dump_info,
	.dump_record = dvfsrc_dump_record,
	.dump_reg = dvfsrc_dump_reg,
	.get_current_level = dvfsrc_get_current_level,
	.get_current_rglevel = dvfsrc_get_current_rglevel,
	.force_opp = dvfsrc_force_opp,
	.query_request = dvfsrc_query_request_status,
};

