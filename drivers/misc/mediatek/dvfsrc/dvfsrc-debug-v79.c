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

#include "dvfsrc-debug.h"
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
	DVFSRC_ISP_HRT,
	DVFSRC_DEBUG_STA_0,
	DVFSRC_VCORE_REQUEST,
	DVFSRC_CURRENT_LEVEL,
	DVFSRC_TARGET_LEVEL,
	DVFSRC_LAST,
	DVFSRC_RECORD_0,
	DVFSRC_DDR_REQUEST,
	DVSFRC_HRT_REQ_MD_URG,
	DVFSRC_HRT_REQ_MD_BW_0,
	DVFSRC_HRT_REQ_MD_BW_8,
};

static const int mt6779_regs[] = {
	[DVFSRC_BASIC_CONTROL] = 0x0,
	[DVFSRC_SW_REQ1] = 0x4,
	[DVFSRC_INT] = 0xC4,
	[DVFSRC_INT_EN] = 0xC8,
	[DVFSRC_SW_BW_0] = 0x260,
	[DVFSRC_ISP_HRT] = 0x290,
	[DVFSRC_DEBUG_STA_0] = 0x700,
	[DVFSRC_VCORE_REQUEST] = 0x6C,
	[DVFSRC_CURRENT_LEVEL] = 0xD44,
	[DVFSRC_TARGET_LEVEL] = 0xD48,
	[DVFSRC_LAST] = 0xB08,
	[DVFSRC_RECORD_0] = 0xB14,
	[DVFSRC_DDR_REQUEST] = 0xA00,
	[DVSFRC_HRT_REQ_MD_URG] = 0xA88,
	[DVFSRC_HRT_REQ_MD_BW_0] = 0xA8C,
	[DVFSRC_HRT_REQ_MD_BW_8] = 0xACC,
};

enum dvfsrc_spm_regs {
	POWERON_CONFIG_EN,
	SPM_PC_STA,
	SPM_SW_FLAG,
	SPM_DVFS_LEVEL,
	SPM_DVFS_STA,
	SPM_DVS_DFS_LEVEL,
	SPM_DVFS_HISTORY_STA0,
	SPM_DVFS_HISTORY_STA1,
	SPM_DVFS_CMD0,
	SPM_DVFS_CMD1,
	SPM_DVFS_CMD2,
	SPM_DVFS_CMD3,
};

static const int mt6779_spm_regs[] = {
	[POWERON_CONFIG_EN] = 0x0,
	[SPM_PC_STA] = 0x1A4,
	[SPM_SW_FLAG] = 0x600,
	[SPM_DVFS_LEVEL] = 0x0708,
	[SPM_DVFS_STA] = 0x070C,
	[SPM_DVS_DFS_LEVEL] = 0x07BC,
	[SPM_DVFS_HISTORY_STA0] = 0x01C0,
	[SPM_DVFS_HISTORY_STA1] = 0x01C4,
	[SPM_DVFS_CMD0] = 0x710,
	[SPM_DVFS_CMD1] = 0x714,
	[SPM_DVFS_CMD2] = 0x718,
	[SPM_DVFS_CMD3] = 0x71C,
};

static u32 dvfsrc_read(struct mtk_dvfsrc *dvfs, u32 reg, u32 offset)
{
	return readl(dvfs->regs + dvfs->dvd->config->regs[reg] + offset);
}

static u32 spm_read(struct mtk_dvfsrc *dvfs, u32 reg)
{
	return readl(dvfs->spm_regs + dvfs->dvd->config->spm_regs[reg]);
}

static u32 dvfsrc_get_total_emi_req(struct mtk_dvfsrc *dvfsrc)
{
	/* DVFSRC_DEBUG_STA_2 */
	return dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0x8) & 0xFFF;
}
static u32 dvfsrc_get_scp_req(struct mtk_dvfsrc *dvfsrc)
{
	/* DVFSRC_DEBUG_STA_2 */
	return (dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0x8) >> 14) & 0x1;
}

static u32 dvfsrc_get_hifi_scenario(struct mtk_dvfsrc *dvfsrc)
{
	/* DVFSRC_DEBUG_STA_2 */
	return (dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0x8) >> 16) & 0xFF;
}

static u32 dvfsrc_get_hifi_vcore_gear(struct mtk_dvfsrc *dvfsrc)
{
	u32 hifi_scen;

	hifi_scen = __builtin_ffs(dvfsrc_get_hifi_scenario(dvfsrc));

	if (hifi_scen)
		return (dvfsrc_read(dvfsrc, DVFSRC_VCORE_REQUEST, 0xC) >>
			((hifi_scen - 1) * 4)) & 0xF;
	else
		return 0;

}

static u32 dvfsrc_get_hifi_ddr_gear(struct mtk_dvfsrc *dvfsrc)
{
	u32 hifi_scen;

	hifi_scen = __builtin_ffs(dvfsrc_get_hifi_scenario(dvfsrc));

	if (hifi_scen)
		return (dvfsrc_read(dvfsrc, DVFSRC_DDR_REQUEST, 0x14) >>
			((hifi_scen - 1) * 4)) & 0xF;
	else
		return 0;
}

static u32 dvfsrc_get_hifi_rising_ddr_gear(struct mtk_dvfsrc *dvfsrc)
{
	u32 offset;

	offset = 0x18 + 0x1C * dvfsrc_read(dvfsrc, DVFSRC_LAST, 0);
	/* DVFSRC_RECORD_0_6 */

	return (dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset) >> 15) & 0x7;
}

static u32 dvfsrc_get_md_bw(struct mtk_dvfsrc *dvfsrc)
{
	u32 is_urgent, md_scen;
	u32 val;
	u32 index, shift;

	val = dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0);
	is_urgent = (val >> 16) & 0x1;
	md_scen = val & 0xFFFF;

	if (is_urgent) {
		val = dvfsrc_read(dvfsrc, DVSFRC_HRT_REQ_MD_URG, 0) & 0x1F;
	} else {
		index = md_scen / 3;
		shift = (md_scen % 3) * 10;

		if (index > 10)
			return 0;

		if (index < 8) {
			val = dvfsrc_read(dvfsrc, DVFSRC_HRT_REQ_MD_BW_0,
				index * 4);
		} else {
			val = dvfsrc_read(dvfsrc, DVFSRC_HRT_REQ_MD_BW_8,
				(index - 8) * 4);
		}
		val = (val >> shift) & 0x3FF;
	}
	return val;
}

u32 dvfsrc_get_md_scenario(void)
{
	/* DVFSRC_DEBUG_STA_0 */
	struct mtk_dvfsrc *dvfsrc = dvfsrc_drv;

	return dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0);
}
EXPORT_SYMBOL(dvfsrc_get_md_scenario);

static u32 dvfsrc_get_md_rising_ddr_gear(struct mtk_dvfsrc *dvfsrc)
{
	u32 val;
	u32 last = dvfsrc_read(dvfsrc, DVFSRC_LAST, 0);

	/* DVFSRC_RECORD_0_6 */
	val = dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, 0x18 + 0x1C * last);

	return (val >> 9) & 0x7;
}

static u32 dvfsrc_get_hrt_bw_ddr_gear(struct mtk_dvfsrc *dvfsrc)
{
	u32 val;
	u32 last = dvfsrc_read(dvfsrc, DVFSRC_LAST, 0);

	/* DVFSRC_RECORD_0_6 */
	val = dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, 0x18 + 0x1C * last);

	return (val >> 2) & 0x7;
}

static char *dvfsrc_dump_info(struct mtk_dvfsrc *dvfsrc,
	char *p, u32 size)
{
	int vcore_uv = 0;
	char *buff_end = p + size;

	if (dvfsrc->vcore_power)
		vcore_uv = regulator_get_voltage(dvfsrc->vcore_power);

	p += snprintf(p, buff_end - p, "%-10s: %-8u uv\n",
			"Vcore", vcore_uv);
#if IS_ENABLED(CONFIG_MTK_DRAMC_LEGACY)
	p += snprintf(p, buff_end - p, "%-10s: %-8u khz\n",
			"DDR", get_dram_data_rate() * 1000);
#endif
	p += snprintf(p, buff_end - p, "\n");

	return p;
}

static char *dvfsrc_dump_record(struct mtk_dvfsrc *dvfsrc,
	char *p, u32 size)
{
	int i, rec_offset, offset;
	char *buff_end = p + size;

	p += sprintf(p, "%-17s: 0x%08x\n",
			"DVFSRC_LAST",
			dvfsrc_read(dvfsrc, DVFSRC_LAST, 0));

	if (dvfsrc->dvd->config->ip_verion > 0)
		rec_offset = 0x20;
	else
		rec_offset = 0x1C;

	for (i = 0; i < 8; i++) {
		offset = i * rec_offset;
		p += snprintf(p, buff_end - p,
			"[%d]%-14s: %08x,%08x,%08x,%08x\n",
			i,
			"DVFSRC_REC 0~3",
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x0),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x4),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x8),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0xC));
		if (dvfsrc->dvd->config->ip_verion > 0) {
			p += snprintf(p, buff_end - p,
			"[%d]%-14s: %08x,%08x,%08x,%08x\n",
			i,
			"DVFSRC_REC 4~7",
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x10),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x14),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x18),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x1C));
		} else {
			p += snprintf(p, buff_end - p,
			"[%d]%-14s: %08x,%08x,%08x\n",
			i,
			"DVFSRC_REC 4~6",
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x10),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x14),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x18));
		}
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
		"%-16s: %08x, %08x, %08x, %08x\n",
		"SW_REQ 1~4",
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x0),
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x4),
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x8),
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0xC));

	p += snprintf(p, buff_end - p,
		"%-16s: %08x, %08x, %08x, %08x\n",
		"SW_REQ 5~8",
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x10),
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x14),
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x18),
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x1C));

	p += snprintf(p, buff_end - p, "%-16s: %d, %d, %d, %d, %d\n",
		"SW_BW_0~4",
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x0),
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x4),
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x8),
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0xC),
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x10));

	if (dvfsrc->dvd->config->ip_verion > 1)
		p += snprintf(p, buff_end - p, "%-16s: %d, %d\n",
		"SW_BW_5~6",
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x14),
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x18));


	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
		"ISP_HRT",
		dvfsrc_read(dvfsrc, DVFSRC_ISP_HRT, 0x0));

	p += snprintf(p, buff_end - p, "%-16s: 0x%08x, 0x%08x, 0x%08x\n",
		"DEBUG_STA",
		dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0x0),
		dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0x4),
		dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0x8));

	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
		"DVFSRC_INT",
		dvfsrc_read(dvfsrc, DVFSRC_INT, 0x0));

	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
		"DVFSRC_INT_EN",
		dvfsrc_read(dvfsrc, DVFSRC_INT_EN, 0x0));

	p += snprintf(p, buff_end - p, "%-16s: 0x%02x\n",
		"TOTAL_EMI_REQ",
		dvfsrc_get_total_emi_req(dvfsrc));

	p += snprintf(p, buff_end - p, "%-16s: %d\n",
		"MD_RISING_REQ",
		dvfsrc_get_md_rising_ddr_gear(dvfsrc));

	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
		"MD_HRT_BW",
		dvfsrc_get_md_bw(dvfsrc));

	p += sprintf(p, "%-16s: %d\n",
		"HRT_BW_REQ",
		dvfsrc_get_hrt_bw_ddr_gear(dvfsrc));

	p += snprintf(p, buff_end - p, "%-16s: %d\n",
		"HIFI_VCORE_REQ",
		dvfsrc_get_hifi_vcore_gear(dvfsrc));

	p += snprintf(p, buff_end - p, "%-16s: %d\n",
		"HIFI_DDR_REQ",
		dvfsrc_get_hifi_ddr_gear(dvfsrc));

	p += snprintf(p, buff_end - p, "%-16s: %d\n",
		"HIFI_RISINGREQ",
		dvfsrc_get_hifi_rising_ddr_gear(dvfsrc));

	p += snprintf(p, buff_end - p, "%-16s: %d , 0x%08x\n",
			"SCP_VCORE_REQ",
			dvfsrc_get_scp_req(dvfsrc),
			dvfsrc_read(dvfsrc, DVFSRC_VCORE_REQUEST, 0x0));
	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
			"CURRENT_LEVEL",
			dvfsrc_read(dvfsrc, DVFSRC_CURRENT_LEVEL, 0x0));
	p += snprintf(p, buff_end - p, "%-16s: 0x%08x\n",
			"TARGET_LEVEL",
			dvfsrc_read(dvfsrc, DVFSRC_TARGET_LEVEL, 0x0));
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
			spm_read(dvfsrc, POWERON_CONFIG_EN));
	p += snprintf(p, buff_end - p, "%-24s: 0x%08x\n",
			"SPM_SW_FLAG_0",
			spm_read(dvfsrc, SPM_SW_FLAG));
	p += snprintf(p, buff_end - p, "%-24s: 0x%08x\n",
			"SPM_PC_STA",
			spm_read(dvfsrc, SPM_PC_STA));
	p += snprintf(p, buff_end - p, "%-24s: 0x%08x\n",
			"SPM_DVFS_LEVEL",
			spm_read(dvfsrc, SPM_DVFS_LEVEL));
	p += snprintf(p, buff_end - p, "%-24s: 0x%08x\n",
			"SPM_DVS_DFS_LEVEL",
			spm_read(dvfsrc, SPM_DVS_DFS_LEVEL));
	p += snprintf(p, buff_end - p, "%-24s: 0x%08x\n",
			"SPM_DVFS_STA",
			spm_read(dvfsrc, SPM_DVFS_STA));
	p += snprintf(p, buff_end - p, "%-24s: 0x%08x, 0x%08x\n",
			"SPM_DVFS_HISTORY_STAx",
			spm_read(dvfsrc, SPM_DVFS_HISTORY_STA0),
			spm_read(dvfsrc, SPM_DVFS_HISTORY_STA1));
	p += snprintf(p, buff_end - p,
			"%-24s: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			"SPM_DVFS_CMD0~3",
			spm_read(dvfsrc, SPM_DVFS_CMD0),
			spm_read(dvfsrc, SPM_DVFS_CMD1),
			spm_read(dvfsrc, SPM_DVFS_CMD2),
			spm_read(dvfsrc, SPM_DVFS_CMD3));
	return p;
}

static void dvfsrc_force_opp(struct mtk_dvfsrc *dvfsrc, u32 opp)
{
	dvfsrc->force_opp_idx = opp;
	mtk_dvfsrc_send_request(dvfsrc->dev->parent,
		MTK_DVFSRC_CMD_FORCE_OPP_REQUEST,
		opp);
}

static int dvfsrc_query_request_status(struct mtk_dvfsrc *dvfsrc, u32 id)
{
	int ret = 0;

	switch (id) {
	case DVFSRC_MD_RISING_DDR_REQ:
		ret = dvfsrc_get_md_rising_ddr_gear(dvfsrc);
		break;
	case DVFSRC_MD_HRT_BW:
		ret = dvfsrc_get_md_bw(dvfsrc);
		break;
	case DVFSRC_HIFI_VCORE_REQ:
		ret = dvfsrc_get_hifi_vcore_gear(dvfsrc);
		break;
	case DVFSRC_HIFI_DDR_REQ:
		ret = dvfsrc_get_hifi_ddr_gear(dvfsrc);
		break;
	case DVFSRC_HIFI_RISING_DDR_REQ:
		ret = dvfsrc_get_hifi_rising_ddr_gear(dvfsrc);
		break;
	case DVFSRC_HRT_BW_DDR_REQ:
		ret = dvfsrc_get_hrt_bw_ddr_gear(dvfsrc);
		break;
	}

	return ret;
}

const struct dvfsrc_config mt6779_dvfsrc_config = {
	.ip_verion = 0,
	.regs = mt6779_regs,
	.spm_regs = mt6779_spm_regs,
	.dump_info = dvfsrc_dump_info,
	.dump_record = dvfsrc_dump_record,
	.dump_reg = dvfsrc_dump_reg,
	.dump_spm_info = dvfsrc_dump_spm_info,
	.force_opp = dvfsrc_force_opp,
	.query_request = dvfsrc_query_request_status,
};

