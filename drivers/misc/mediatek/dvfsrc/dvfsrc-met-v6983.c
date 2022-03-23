// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk_dvfsrc.h>

#include "dvfsrc-met.h"
#include "dvfsrc-common.h"

static inline u32 dvfsrc_met_read(struct mtk_dvfsrc_met *dvfs, u32 offset)
{
	return readl(dvfs->regs + offset);
}

enum met_src_index {
	MD2SPM = 0,
	DDR__SW_REQ2,
	DDR__SW_REQ3,
	DDR__QOS_BW,
	DDR__EMI_TOTAL,
	DDR__HRT_BW,
	DDR__HIFI,
	DDR__HIFI_LATENCY,
	DDR__MD_LATENCY,
	DDR__MD_DDR,
	VCORE__SCP,
	VCORE__HIFI,
	VCORE__SRAMRC,
	VCORE__SW_REQ1,
	VCORE__SW_REQ2,
	VCORE__SW_REQ3,
	VCORE__SW_REQ4,
	VCORE__SW_REQ5,
	VCORE__SW_REQ6,
	VCORE__SW_REQ7,
	VCORE__SW_REQ8,
	SCP_REQ,
	PMQOS_TOTAL,
	PMQOS_BW0,
	PMQOS_BW1,
	PMQOS_BW2,
	PMQOS_BW3,
	PMQOS_BW4,
	PMQOS_BW5,
	PMQOS_BW6,
	PMQOS_BW7,
	PMQOS_BW8,
	PMQOS_BW9,
	HRT_MD_BW,
	HRT_DISP_BW,
	HRT_ISP_BW,
	MD_SCENARIO,
	HIFI_SCENARIO_IDX,
	MD_EMI_LATENCY,
	SRC_MAX,
};

/* met profile table */
static unsigned int met_vcorefs_src[SRC_MAX];
static char *met_src_name[SRC_MAX] = {
	"MD2SPM",
	"DDR__SW_REQ2",
	"DDR__SW_REQ3",
	"DDR__QOS_BW",
	"DDR__EMI_TOTAL",
	"DDR__HRT_BW",
	"DDR__HIFI",
	"DDR__HIFI_LATENCY",
	"DDR__MD_LATENCY",
	"DDR__MD_DDR",
	"VCORE__SCP",
	"VCORE__HIFI",
	"VCORE__SRAMRC",
	"VCORE__SW_REQ1",
	"VCORE__SW_REQ2",
	"VCORE__SW_REQ3",
	"VCORE__SW_REQ4",
	"VCORE__SW_REQ5",
	"VCORE__SW_REQ6",
	"VCORE__SW_REQ7",
	"VCORE__SW_REQ8",
	"SCP_REQ",
	"PMQOS_TOTAL",
	"PMQOS_BW0",
	"PMQOS_BW1",
	"PMQOS_BW2",
	"PMQOS_BW3",
	"PMQOS_BW4",
	"PMQOS_BW5",
	"PMQOS_BW6",
	"PMQOS_BW7",
	"PMQOS_BW8",
	"PMQOS_BW9",
	"HRT_MD_BW",
	"HRT_DISP_BW",
	"HRT_ISP_BW",
	"MD_SCENARIO",
	"HIFI_SCENARIO_IDX",
	"MD_EMI_LATENCY",
};

#define DVFSRC_SW_REQ1	(0x10)
#define DVFSRC_SW_REQ2	(0x14)
#define DVFSRC_SW_REQ3	(0x18)
#define DVFSRC_SW_REQ4	(0x1C)
#define DVFSRC_SW_REQ5	(0x20)
#define DVFSRC_SW_REQ6	(0x24)
#define DVFSRC_SW_REQ7	(0x28)
#define DVFSRC_SW_REQ8	(0x2C)

#define DVFSRC_ISP_HRT  (0x20C)

#define DVFSRC_VCORE_REQUEST (0x80)
#define DVFSRC_DEBUG_STA_0 (0x29C)
#define DVFSRC_DEBUG_STA_2 (0x2A4)
#define DVFSRC_DEBUG_STA_8 (0x2BC)

#define DVFSRC_PCIE_VCORE_REQ (0xE4)

#define DVFSRC_SW_BW_0	(0x1DC)
#define DVFSRC_SW_BW_1	(0x1E0)
#define DVFSRC_SW_BW_2	(0x1E4)
#define DVFSRC_SW_BW_3	(0x1E8)
#define DVFSRC_SW_BW_4	(0x1EC)
#define DVFSRC_SW_BW_5	(0x1F0)
#define DVFSRC_SW_BW_6	(0x1F4)
#define DVFSRC_SW_BW_7	(0x1F8)
#define DVFSRC_SW_BW_8	(0x1FC)
#define DVFSRC_SW_BW_9	(0x200)

#define DVFSRC_MD_DDR_FLOOR_REQUEST (0x5E4)
#define DVFSRC_QOS_DDR_REQUEST (0x5E8)

#define DVFSRC_LEVEL_HEX  (0x5F0)

/* DVFSRC_SW_REQX */
#define DDR_SW_AP_SHIFT		12
#define DDR_SW_AP_MASK		0xF
#define VCORE_SW_AP_SHIFT	4
#define VCORE_SW_AP_MASK	0x7

/* DVFSRC_VCORE_REQUEST 0x70 */
#define VCORE_SCP_GEAR_SHIFT	12
#define VCORE_SCP_GEAR_MASK	0x7

/* DVFSRC_SRARMC */
#define VCORE_SRAMRC_GEAR_SHIFT	18
#define VCORE_SRAMRC_GEAR_MASK	0x7

/* met profile function */
static int dvfsrc_get_src_req_num(void)
{
	return SRC_MAX;
}

static char **dvfsrc_get_src_req_name(void)
{
	return met_src_name;
}

static u32 dvfsrc_get_current_level(struct mtk_dvfsrc_met *dvfsrc)
{
	return dvfsrc_met_read(dvfsrc, DVFSRC_LEVEL_HEX) & 0x3F;
}

static u32 dvfsrc_ddr_qos(struct mtk_dvfsrc_met *dvfsrc)
{
	return dvfsrc_met_read(dvfsrc, DVFSRC_QOS_DDR_REQUEST);
}

static int dvfsrc_emi_mon_gear(struct mtk_dvfsrc_met *dvfs)
{
	unsigned int total_bw_status;
	int i;
	u32 max_idx, level_mask;

	switch (dvfs->dvd->version) {
	case 0x6983:
	case 0x6895:
		max_idx = 7;
		level_mask = 0xFF;
	break;
	case 0x6879:
	case 0x6855:
		max_idx = 6;
		level_mask = 0x7F;
	break;
	default:
		max_idx = 5;
		level_mask = 0x3F;
	break;
	}

	total_bw_status = dvfsrc_met_read(dvfs, DVFSRC_DEBUG_STA_2) & level_mask;
	for (i = max_idx; i >= 0 ; i--) {
		if ((total_bw_status >> i) > 0)
			return i + 1;
	}

	return 0;
}

static void vcorefs_get_src_ddr_req(struct mtk_dvfsrc_met *dvfs)
{
	unsigned int sw_req;

	sw_req = dvfsrc_met_read(dvfs, DVFSRC_SW_REQ2);
	met_vcorefs_src[DDR__SW_REQ2] = (sw_req >> DDR_SW_AP_SHIFT) & DDR_SW_AP_MASK;

	sw_req = dvfsrc_met_read(dvfs, DVFSRC_SW_REQ3);
	met_vcorefs_src[DDR__SW_REQ3] = (sw_req >> DDR_SW_AP_SHIFT) & DDR_SW_AP_MASK;

	met_vcorefs_src[DDR__QOS_BW] = dvfsrc_ddr_qos(dvfs);

	met_vcorefs_src[DDR__EMI_TOTAL] = dvfsrc_emi_mon_gear(dvfs);

	met_vcorefs_src[DDR__HRT_BW] = mtk_dvfsrc_query_debug_info(DVFSRC_HRT_BW_DDR_REQ);

	met_vcorefs_src[DDR__HIFI] = mtk_dvfsrc_query_debug_info(DVFSRC_HIFI_DDR_REQ);

	met_vcorefs_src[DDR__HIFI_LATENCY] = mtk_dvfsrc_query_debug_info(DVFSRC_HIFI_RISING_DDR_REQ);

	met_vcorefs_src[DDR__MD_LATENCY] = mtk_dvfsrc_query_debug_info(DVFSRC_MD_IMP_DDR_REQ);

	met_vcorefs_src[DDR__MD_DDR] = mtk_dvfsrc_query_debug_info(DVFSRC_MD_SCEN_DDR_REQ);
}

static void vcorefs_get_src_vcore_req(struct mtk_dvfsrc_met *dvfs)
{
	u32 val;
	u32 sta;

	val = dvfsrc_met_read(dvfs, DVFSRC_SW_REQ1);
	met_vcorefs_src[VCORE__SW_REQ1] = (val >> VCORE_SW_AP_SHIFT) & VCORE_SW_AP_MASK;

	val = dvfsrc_met_read(dvfs, DVFSRC_SW_REQ2);
	met_vcorefs_src[VCORE__SW_REQ2] = (val >> VCORE_SW_AP_SHIFT) & VCORE_SW_AP_MASK;

	val = dvfsrc_met_read(dvfs, DVFSRC_SW_REQ3);
	met_vcorefs_src[VCORE__SW_REQ3] = (val >> VCORE_SW_AP_SHIFT) & VCORE_SW_AP_MASK;

	val = dvfsrc_met_read(dvfs, DVFSRC_SW_REQ4);
	met_vcorefs_src[VCORE__SW_REQ4] = (val >> VCORE_SW_AP_SHIFT) & VCORE_SW_AP_MASK;

	val = dvfsrc_met_read(dvfs, DVFSRC_SW_REQ5);
	met_vcorefs_src[VCORE__SW_REQ5] = (val >> VCORE_SW_AP_SHIFT) & VCORE_SW_AP_MASK;

	val = dvfsrc_met_read(dvfs, DVFSRC_SW_REQ6);
	met_vcorefs_src[VCORE__SW_REQ6] = (val >> VCORE_SW_AP_SHIFT) & VCORE_SW_AP_MASK;

	val = dvfsrc_met_read(dvfs, DVFSRC_SW_REQ7);
	met_vcorefs_src[VCORE__SW_REQ7] = (val >> VCORE_SW_AP_SHIFT) & VCORE_SW_AP_MASK;

	val = dvfsrc_met_read(dvfs, DVFSRC_SW_REQ8);
	met_vcorefs_src[VCORE__SW_REQ8] = (val >> VCORE_SW_AP_SHIFT) & VCORE_SW_AP_MASK;

	met_vcorefs_src[VCORE__HIFI] = mtk_dvfsrc_query_debug_info(DVFSRC_HIFI_VCORE_REQ);

	sta = dvfsrc_met_read(dvfs, DVFSRC_DEBUG_STA_2);
	if ((sta >> 14) & 0x1) {
		val = dvfsrc_met_read(dvfs, DVFSRC_VCORE_REQUEST);
		met_vcorefs_src[VCORE__SCP] = (val >> VCORE_SCP_GEAR_SHIFT) & VCORE_SCP_GEAR_MASK;
	} else
		met_vcorefs_src[VCORE__SCP] = 0;

	sta = dvfsrc_met_read(dvfs, DVFSRC_DEBUG_STA_8);
	met_vcorefs_src[VCORE__SRAMRC] = (sta >> VCORE_SRAMRC_GEAR_SHIFT) & VCORE_SRAMRC_GEAR_MASK;
}

static void vcorefs_get_src_misc_info(struct mtk_dvfsrc_met *dvfs)
{
	u32 i;
	u32 sta0, sta2;

	met_vcorefs_src[PMQOS_BW0] = dvfsrc_met_read(dvfs, DVFSRC_SW_BW_0);
	met_vcorefs_src[PMQOS_BW1] = dvfsrc_met_read(dvfs, DVFSRC_SW_BW_1);
	met_vcorefs_src[PMQOS_BW2] = dvfsrc_met_read(dvfs, DVFSRC_SW_BW_2);
	met_vcorefs_src[PMQOS_BW3] = dvfsrc_met_read(dvfs, DVFSRC_SW_BW_3);
	met_vcorefs_src[PMQOS_BW4] = dvfsrc_met_read(dvfs, DVFSRC_SW_BW_4);
	met_vcorefs_src[PMQOS_BW5] = dvfsrc_met_read(dvfs, DVFSRC_SW_BW_5);
	met_vcorefs_src[PMQOS_BW6] = dvfsrc_met_read(dvfs, DVFSRC_SW_BW_6);
	met_vcorefs_src[PMQOS_BW7] = dvfsrc_met_read(dvfs, DVFSRC_SW_BW_7);
	met_vcorefs_src[PMQOS_BW8] = dvfsrc_met_read(dvfs, DVFSRC_SW_BW_8);
	met_vcorefs_src[PMQOS_BW9] = dvfsrc_met_read(dvfs, DVFSRC_SW_BW_9);
	met_vcorefs_src[PMQOS_TOTAL] = 0;

	for (i = 0; i < 9; i++) {
		if (i == 6)
			continue;
		met_vcorefs_src[PMQOS_TOTAL] += met_vcorefs_src[PMQOS_BW0 + i];
	}
	met_vcorefs_src[HRT_ISP_BW] = dvfsrc_met_read(dvfs, DVFSRC_ISP_HRT);
	met_vcorefs_src[HRT_MD_BW] = mtk_dvfsrc_query_debug_info(DVFSRC_MD_HRT_BW);

	sta0 = dvfsrc_met_read(dvfs, DVFSRC_DEBUG_STA_0);
	met_vcorefs_src[MD2SPM] = sta0 & 0xFFFF;
	met_vcorefs_src[MD_SCENARIO] = sta0 & 0xFFFF;

	sta2 = dvfsrc_met_read(dvfs, DVFSRC_DEBUG_STA_2);
	met_vcorefs_src[SCP_REQ] = (sta2 >> 14) & 0x1;
	met_vcorefs_src[MD_EMI_LATENCY] = (sta2 >> 12) & 0x3;
	met_vcorefs_src[HIFI_SCENARIO_IDX] = (sta2 >> 16) & 0xFF;
}


static unsigned int *dvfsrc_get_src_req(struct mtk_dvfsrc_met *dvfs)
{
	vcorefs_get_src_ddr_req(dvfs);
	vcorefs_get_src_vcore_req(dvfs);
	vcorefs_get_src_misc_info(dvfs);

	return met_vcorefs_src;
}

static int dvfsrc_get_ddr_ratio(struct mtk_dvfsrc_met *dvfs)
{
	int dram_opp;

	dram_opp = mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_DRAM_OPP);

	return 0;
}

const struct dvfsrc_met_config mt6983_met_config = {
	.dvfsrc_get_src_req_num = dvfsrc_get_src_req_num,
	.dvfsrc_get_src_req_name = dvfsrc_get_src_req_name,
	.dvfsrc_get_src_req = dvfsrc_get_src_req,
	.dvfsrc_get_ddr_ratio = dvfsrc_get_ddr_ratio,
	.get_current_level = dvfsrc_get_current_level,
};
