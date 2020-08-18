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

#include "dvfsrc-helper.h"
#include "dvfsrc-common.h"

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
	DVFSRC_MD_TURBO,
	DVFSRC_95MD_SCEN_BW4,
	DVFSRC_95MD_SCEN_BW0,
	DVFSRC_95MD_SCEN_BW0_T,
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

static const int mt6873_regs[] = {
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
	[DVFSRC_LAST] = 0xAE4,
	[DVFSRC_RECORD_0] = 0xAF0,
	[DVFSRC_DDR_REQUEST] = 0xA00,
	[DVSFRC_HRT_REQ_MD_URG] = 0xA64,
	[DVFSRC_HRT_REQ_MD_BW_0] = 0xA8C,
	[DVFSRC_HRT_REQ_MD_BW_8] = 0xACC,
	[DVFSRC_MD_TURBO] = 0xDC,
	[DVFSRC_95MD_SCEN_BW4] = 0x544,
	[DVFSRC_95MD_SCEN_BW0] = 0x524,
	[DVFSRC_95MD_SCEN_BW0_T] = 0x534,
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
	SPM_DVFS_CMD4,
};
static const int mt6873_spm_regs[] = {
	[POWERON_CONFIG_EN] = 0x0,
	[SPM_PC_STA] = 0x0194,
	[SPM_SW_FLAG] = 0x600,
	[SPM_DVFS_LEVEL] = 0x04A4,
	[SPM_DVFS_STA] = 0x01F8,
	[SPM_DVS_DFS_LEVEL] = 0x04F8,
	[SPM_DVFS_CMD0] = 0x710,
	[SPM_DVFS_CMD1] = 0x714,
	[SPM_DVFS_CMD2] = 0x718,
	[SPM_DVFS_CMD3] = 0x71C,
	[SPM_DVFS_CMD4] = 0x720,
};

static u32 dvfsrc_read(struct mtk_dvfsrc *dvfs, u32 reg, u32 offset)
{
	return readl(dvfs->regs + dvfs->dvd->config->regs[reg] + offset);
}

static u32 spm_read(struct mtk_dvfsrc *dvfs, u32 reg)
{
	return readl(dvfs->spm_regs + dvfs->dvd->config->spm_regs[reg]);
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
	u32 val;
	u32 last;

	if (dvfsrc->dvd->config->ip_verion == 0) {
		/* DVFSRC_RECORD_0_6 */
		last = dvfsrc_read(dvfsrc, DVFSRC_LAST, 0);
		val = dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, 0x18 + 0x1C * last);
		val = (val >> 15) & 0x7;
	} else {
		val = dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0);
		val = (val >> 22) & 0x7;
	}

	return val;
}

static u32 dvfsrc_get_md_bw(struct mtk_dvfsrc *dvfsrc)
{
	u32 is_urgent, md_scen;
	u32 val;
	u32 index, shift;

	if (dvfsrc->dvd->config->ip_verion == 2)
		val = dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0xC) & 0x3FF;
	else {
		val = dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0);
		is_urgent = (val >> 16) & 0x1;
		md_scen = val & 0xFFFF;

		if (is_urgent) {
			val = dvfsrc_read(dvfsrc, DVSFRC_HRT_REQ_MD_URG, 0)
					  & 0x1F;
		} else {
			index = md_scen / 3;
			shift = (md_scen % 3) * 10;

			if (index > 10)
				return 0;

			if (index < 8) {
				val = dvfsrc_read(dvfsrc,
					DVFSRC_HRT_REQ_MD_BW_0,
					index * 4);
			} else {
				val = dvfsrc_read(dvfsrc,
					DVFSRC_HRT_REQ_MD_BW_8,
					(index - 8) * 4);
			}
			val = (val >> shift) & 0x3FF;
		}
	}

	return val;
}

static u32 dvfsrc_get_md_rising_ddr_gear(struct mtk_dvfsrc *dvfsrc)
{
	u32 val;
	u32 last;

	if (dvfsrc->dvd->config->ip_verion == 0) {
		/* DVFSRC_RECORD_0_6 */
		last = dvfsrc_read(dvfsrc, DVFSRC_LAST, 0);
		val = dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, 0x18 + 0x1C * last);
		val = (val >> 9) & 0x7;
	} else {
		val = dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0);
		val = (val >> 29) & 0x7;
	}

	return val;
}

static u32 dvfsrc_get_hrt_bw_ddr_gear(struct mtk_dvfsrc *dvfsrc)
{
	u32 val;
	u32 last = dvfsrc_read(dvfsrc, DVFSRC_LAST, 0);

	if (dvfsrc->dvd->config->ip_verion == 0) {
		/* DVFSRC_RECORD_0_6 */
		val = dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, 0x18 + 0x1C * last);
		val = (val >> 2) & 0x7;
	} else if (dvfsrc->dvd->config->ip_verion == 1) {
		/* DVFSRC_RECORD_0_5 */
		val = dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, 0x14 + 0x20 * last);
		val = (val >> 21) & 0x7;
	} else {
		val = dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0x10);
		val = (val >> 16) & 0x7;
	}

	return val;
}

static u32 dvfsrc_get_md_scen_ddr_gear(struct mtk_dvfsrc *dvfsrc)
{
	u32 is_turbo, is_urgent, md_scen;
	u32 index, shift;
	u32 sta0;
	u32 val = 0;

	if (dvfsrc->dvd->config->ip_verion == 0)
		return 0;

	is_turbo = (dvfsrc_read(dvfsrc, DVFSRC_MD_TURBO, 0x0) == 0) ? 1 : 0;
	sta0 = dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0x0);
	is_urgent = (sta0 >> 16) & 0x1;
	md_scen = sta0 & 0xFFFF;

	if (is_urgent)
		val = dvfsrc_read(dvfsrc, DVFSRC_95MD_SCEN_BW4, 0x0);
	else {
		index = md_scen / 8;
		shift = (md_scen % 8) * 4;
		if (md_scen > 31)
			return 0;

		if (is_turbo)
			val = dvfsrc_read(dvfsrc, DVFSRC_95MD_SCEN_BW0_T
					  , index * 4);
		else
			val = dvfsrc_read(dvfsrc, DVFSRC_95MD_SCEN_BW0
					  , index * 4);

		val = (val >> shift) & 0x7;
	}

	return val;
}

static u32 dvfsrc_get_md_imp_ddr(struct mtk_dvfsrc *dvfsrc)
{
	u32 val;

	if (dvfsrc->dvd->config->ip_verion == 0)
		return 0;

	val = dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0x10);
	val = (val >> 19) & 0x7;

	return val;
}

static char *dvfsrc_dump_record(struct mtk_dvfsrc *dvfsrc,
	char *p, u32 size)
{
	int i, rec_offset, offset;
	char *buff_end = p + size;

	p += sprintf(p, "%-11s: %d\n",
			"DVFSRC_LAST",
			dvfsrc_read(dvfsrc, DVFSRC_LAST, 0));

	if (dvfsrc->dvd->config->ip_verion > 0)
		rec_offset = 0x20;
	else
		rec_offset = 0x1C;

	for (i = 0; i < 8; i++) {
		offset = i * rec_offset;
		p += snprintf(p, buff_end - p,
			"[%d]%-4s:%08x,%08x,%08x,%08x\n",
			i,
			"0~3",
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x0),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x4),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x8),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0xC));
		if (dvfsrc->dvd->config->ip_verion > 0) {
			p += snprintf(p, buff_end - p,
			"[%d]%-4s:%08x,%08x,%08x,%08x\n",
			i,
			"4~7",
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x10),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x14),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x18),
			dvfsrc_read(dvfsrc, DVFSRC_RECORD_0, offset + 0x1C));
		} else {
			p += snprintf(p, buff_end - p,
			"[%d]%-4s:%08x,%08x,%08x\n",
			i,
			"4~6",
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

	p += snprintf(p, buff_end - p, "%-12s: %08x\n",
		"CONTROL",
		dvfsrc_read(dvfsrc, DVFSRC_BASIC_CONTROL, 0x0));

	p += snprintf(p, buff_end - p, "%-12s: %08x\n",
		"CURRENT",
		dvfsrc_read(dvfsrc, DVFSRC_CURRENT_LEVEL, 0x0));

	p += snprintf(p, buff_end - p, "%-12s: %08x\n",
		"TARGET",
		dvfsrc_read(dvfsrc, DVFSRC_TARGET_LEVEL, 0x0));

	p += snprintf(p, buff_end - p,
		"%-12s: %08x, %08x, %08x, %08x\n",
		"SW_REQ 1~4",
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x0),
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x4),
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x8),
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0xC));

	p += snprintf(p, buff_end - p,
		"%-12s: %08x, %08x, %08x, %08x\n",
		"SW_REQ 5~8",
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x10),
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x14),
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x18),
		dvfsrc_read(dvfsrc, DVFSRC_SW_REQ1, 0x1C));

	p += snprintf(p, buff_end - p,
		"%-12s: %d, %d, %d, %d, %d, %d, %d\n",
		"SW_BW_0~4",
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x0),
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x4),
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x8),
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0xC),
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x10),
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x14),
		dvfsrc_read(dvfsrc, DVFSRC_SW_BW_0, 0x18));

	p += snprintf(p, buff_end - p, "%-12s: %x\n",
		"INT",
		dvfsrc_read(dvfsrc, DVFSRC_INT, 0x0));

	p += snprintf(p, buff_end - p, "%-12s: %d\n",
		"ISP_HRT",
		dvfsrc_read(dvfsrc, DVFSRC_ISP_HRT, 0x0));

	p += snprintf(p, buff_end - p,
		"%-12s: %08x, %08x, %08x, %08x\n",
		"DEBUG_STA",
		dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0x0),
		dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0x4),
		dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0x8),
		dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0xC));

	p += snprintf(p, buff_end - p,
		"%-12s: %08x, %08x, %08x\n",
		"DEBUG_STA1",
		dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0x10),
		dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0x14),
		dvfsrc_read(dvfsrc, DVFSRC_DEBUG_STA_0, 0x18));

	p += snprintf(p, buff_end - p, "%-12s: %x\n",
		"INT_EN",
		dvfsrc_read(dvfsrc, DVFSRC_INT_EN, 0x0));

	p += snprintf(p, buff_end - p, "%-12s: %d\n",
		"MD_RISING",
		dvfsrc_get_md_rising_ddr_gear(dvfsrc));

	p += snprintf(p, buff_end - p, "%-12s: %d\n",
		"MD_HRT_BW",
		dvfsrc_get_md_bw(dvfsrc));

	p += snprintf(p, buff_end - p, "%-12s: %d\n",
		"HRT_BW_REQ",
		dvfsrc_get_hrt_bw_ddr_gear(dvfsrc));

	p += snprintf(p, buff_end - p, "%-12s: %d\n",
		"HIFI_VCORE",
		dvfsrc_get_hifi_vcore_gear(dvfsrc));

	p += snprintf(p, buff_end - p, "%-12s: %d\n",
		"HIFI_DDR",
		dvfsrc_get_hifi_ddr_gear(dvfsrc));

	p += snprintf(p, buff_end - p, "%-12s: %d\n",
		"HIFI_RISING",
		dvfsrc_get_hifi_rising_ddr_gear(dvfsrc));

	p += snprintf(p, buff_end - p, "%-12s: %d, %x\n",
		"SCP_VCORE",
		dvfsrc_get_scp_req(dvfsrc),
		dvfsrc_read(dvfsrc, DVFSRC_VCORE_REQUEST, 0x0));

	p += snprintf(p, buff_end - p, "\n");

	return p;
}

static char *dvfsrc_dump_mt6873_spm_info(struct mtk_dvfsrc *dvfsrc,
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
	p += snprintf(p, buff_end - p,
			"%-24s: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			"SPM_DVFS_CMD0~4",
			spm_read(dvfsrc, SPM_DVFS_CMD0),
			spm_read(dvfsrc, SPM_DVFS_CMD1),
			spm_read(dvfsrc, SPM_DVFS_CMD2),
			spm_read(dvfsrc, SPM_DVFS_CMD3));
	return p;
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
	case DVFSRC_MD_SCEN_DDR_REQ:
		ret = dvfsrc_get_md_scen_ddr_gear(dvfsrc);
		break;
	case DVFSRC_MD_IMP_DDR_REQ:
		ret = dvfsrc_get_md_imp_ddr(dvfsrc);
		break;
	}

	return ret;
}

const struct dvfsrc_config mt6779_dvfsrc_config = {
	.ip_verion = 0, /*mt6779 series*/
	.regs = mt6779_regs,
	.dump_record = dvfsrc_dump_record,
	.dump_reg = dvfsrc_dump_reg,
	.query_request = dvfsrc_query_request_status,
};

const struct dvfsrc_config mt6873_dvfsrc_config = {
	.ip_verion = 2, /*mt6873 series*/
	.regs = mt6873_regs,
	.spm_regs = mt6873_spm_regs,
	.dump_record = dvfsrc_dump_record,
	.dump_reg = dvfsrc_dump_reg,
	.query_request = dvfsrc_query_request_status,
	.dump_spm_info = dvfsrc_dump_mt6873_spm_info,
};

