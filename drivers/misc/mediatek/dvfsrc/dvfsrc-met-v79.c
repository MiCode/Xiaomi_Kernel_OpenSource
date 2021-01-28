// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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
	SRC_MD2SPM_IDX = 0,
	DDR_SW_REQ1_SPM_IDX,
	DDR_SW_REQ2_CM_IDX,
	DDR_SW_REQ3_PMQOS_IDX,
	DDR_SW_REQ4_MD_IDX,
	DDR_SW_REQ8_MCUSYS_IDX,
	DDR_QOS_BW_IDX,
	DDR_EMI_TOTAL_IDX,
	DDR_HRT_BW_IDX,
	DDR_HIFI_IDX,
	DDR_HIFI_LATENCY_IDX,
	DDR_MD_LATENCY_IDX,
	VCORE_SW_REQ3_PMQOS_IDX,
	VCORE_SCP_IDX,
	VCORE_HIFI_IDX,
	SRC_SCP_REQ_IDX,
	SRC_PMQOS_TATOL_IDX,
	SRC_PMQOS_BW0_IDX,
	SRC_PMQOS_BW1_IDX,
	SRC_PMQOS_BW2_IDX,
	SRC_PMQOS_BW3_IDX,
	SRC_PMQOS_BW4_IDX,
	SRC_HRT_MD_BW_IDX,
	SRC_HRT_DISP_BW_IDX,
	SRC_HRT_ISP_BW_IDX,
	SRC_MD_SCENARIO_IDX,
	SRC_HIFI_SCENARIO_IDX,
	SRC_MD_EMI_LATENCY_IDX,
	SRC_MAX
};

/* met profile table */
static unsigned int met_vcorefs_src[SRC_MAX];

static char *met_src_name[SRC_MAX] = {
	"MD2SPM",
	"DDR__SW_REQ1_SPM",
	"DDR__SW_REQ2_CM",
	"DDR__SW_REQ3_PMQOS",
	"DDR__SW_REQ4_MD",
	"DDR__SW_REQ8_MCUSYS",
	"DDR__QOS_BW",
	"DDR__EMI_TOTAL",
	"DDR__HRT_BW",
	"DDR__HIFI",
	"DDR__HIFI_LATENCY",
	"DDR__MD_LATENCY",
	"VCORE__SW_REQ3_PMQOS",
	"VCORE__SCP",
	"VCORE__HIFI",
	"SCP_REQ",
	"PMQOS_TATOL",
	"PMQOS_BW0",
	"PMQOS_BW1",
	"PMQOS_BW2",
	"PMQOS_BW3",
	"PMQOS_BW4",
	"HRT_MD_BW",
	"HRT_DISP_BW",
	"HRT_ISP_BW",
	"MD_SCENARIO",
	"HIFI_SCENARIO_IDX",
	"MD_EMI_LATENCY",
};

#define DVFSRC_SW_REQ1	(0x4)
#define DVFSRC_SW_REQ2	(0x8)
#define DVFSRC_SW_REQ3	(0xC)
#define DVFSRC_SW_REQ4	(0x10)
#define DVFSRC_SW_REQ5	(0x14)
#define DVFSRC_SW_REQ6	(0x18)
#define DVFSRC_SW_REQ7	(0x1C)
#define DVFSRC_SW_REQ8	(0x20)
#define DVFSRC_ISP_HRT  (0x290)

#define DVFSRC_VCORE_REQUEST	(0x6C)
#define DVFSRC_DEBUG_STA_0 (0x700)
#define DVFSRC_DEBUG_STA_2 (0x708)

#define DVFSRC_SW_BW_0	(0x260)
#define DVFSRC_SW_BW_1	(0x264)
#define DVFSRC_SW_BW_2	(0x268)
#define DVFSRC_SW_BW_3	(0x26C)
#define DVFSRC_SW_BW_4	(0x270)

#define DVFSRC_CURRENT_LEVEL	(0xD44)

/* DVFSRC_SW_REQX */
#define DDR_SW_AP_SHIFT		12
#define DDR_SW_AP_MASK		0x7
#define VCORE_SW_AP_SHIFT	4
#define VCORE_SW_AP_MASK	0x7

/* DVFSRC_VCORE_REQUEST 0x70 */
#define VCORE_SCP_GEAR_SHIFT	12
#define VCORE_SCP_GEAR_MASK		0x7

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
	return dvfsrc_met_read(dvfsrc, DVFSRC_CURRENT_LEVEL);
}

static u32 dvfsrc_mt6779_ddr_qos(struct mtk_dvfsrc_met *dvfs)
{
	unsigned int qos_total_bw = dvfsrc_met_read(dvfs, DVFSRC_SW_BW_0) +
			   dvfsrc_met_read(dvfs, DVFSRC_SW_BW_1) +
			   dvfsrc_met_read(dvfs, DVFSRC_SW_BW_2) +
			   dvfsrc_met_read(dvfs, DVFSRC_SW_BW_3) +
			   dvfsrc_met_read(dvfs, DVFSRC_SW_BW_4);

	if (qos_total_bw < 0x19)
		return 0;
	else if (qos_total_bw < 0x26)
		return 2;
	else if (qos_total_bw < 0x33)
		return 2;
	else if (qos_total_bw < 0x4c)
		return 3;
	else if (qos_total_bw < 0x66)
		return 4;
	else
		return 5;
}

static int dvfsrc_mt6779_emi_mon_gear(struct mtk_dvfsrc_met *dvfs)
{
	unsigned int total_bw_status;
	int i;

	total_bw_status = dvfsrc_met_read(dvfs, DVFSRC_DEBUG_STA_2) & 0x1F;

	for (i = 4; i >= 0 ; i--) {
		if ((total_bw_status >> i) > 0) {
			if (i == 0)
				return i + 2;
			else
				return i + 1;
		}
	}

	return 0;
}

static void vcorefs_get_src_ddr_req(struct mtk_dvfsrc_met *dvfs)
{
	unsigned int sw_req;

	sw_req = dvfsrc_met_read(dvfs, DVFSRC_SW_REQ1);
	met_vcorefs_src[DDR_SW_REQ1_SPM_IDX] =
		(sw_req >> DDR_SW_AP_SHIFT) & DDR_SW_AP_MASK;

	sw_req = dvfsrc_met_read(dvfs, DVFSRC_SW_REQ2);
	met_vcorefs_src[DDR_SW_REQ2_CM_IDX] =
		(sw_req >> DDR_SW_AP_SHIFT) & DDR_SW_AP_MASK;

	sw_req = dvfsrc_met_read(dvfs, DVFSRC_SW_REQ3);
	met_vcorefs_src[DDR_SW_REQ3_PMQOS_IDX] =
		(sw_req >> DDR_SW_AP_SHIFT) & DDR_SW_AP_MASK;

	sw_req = dvfsrc_met_read(dvfs, DVFSRC_SW_REQ4);
	met_vcorefs_src[DDR_SW_REQ4_MD_IDX] =
		(sw_req >> DDR_SW_AP_SHIFT) & DDR_SW_AP_MASK;

	sw_req = dvfsrc_met_read(dvfs, DVFSRC_SW_REQ8);
	met_vcorefs_src[DDR_SW_REQ8_MCUSYS_IDX] =
		(sw_req >> DDR_SW_AP_SHIFT) & DDR_SW_AP_MASK;

	met_vcorefs_src[DDR_QOS_BW_IDX] =
		dvfsrc_mt6779_ddr_qos(dvfs);

	met_vcorefs_src[DDR_EMI_TOTAL_IDX] =
		dvfsrc_mt6779_emi_mon_gear(dvfs);

	met_vcorefs_src[DDR_HRT_BW_IDX] =
		mtk_dvfsrc_query_debug_info(DVFSRC_HRT_BW_DDR_REQ);

	met_vcorefs_src[DDR_HIFI_IDX] =
		mtk_dvfsrc_query_debug_info(DVFSRC_HIFI_DDR_REQ);

	met_vcorefs_src[DDR_HIFI_LATENCY_IDX] =
		mtk_dvfsrc_query_debug_info(DVFSRC_HIFI_RISING_DDR_REQ);

	met_vcorefs_src[DDR_MD_LATENCY_IDX] =
		mtk_dvfsrc_query_debug_info(DVFSRC_MD_RISING_DDR_REQ);
}

static void vcorefs_get_src_vcore_req(struct mtk_dvfsrc_met *dvfs)
{
	u32 sw_req;
	u32 scp_en;

	scp_en = (dvfsrc_met_read(dvfs, DVFSRC_DEBUG_STA_2) >> 14) & 0x1;

	sw_req = dvfsrc_met_read(dvfs, DVFSRC_SW_REQ3);
	met_vcorefs_src[VCORE_SW_REQ3_PMQOS_IDX] =
		(sw_req >> VCORE_SW_AP_SHIFT) & VCORE_SW_AP_MASK;

	if (scp_en) {
		sw_req = dvfsrc_met_read(dvfs, DVFSRC_VCORE_REQUEST);
		met_vcorefs_src[VCORE_SCP_IDX] =
			(sw_req >> VCORE_SCP_GEAR_SHIFT) & VCORE_SCP_GEAR_MASK;
	} else
		met_vcorefs_src[VCORE_SCP_IDX] = 0;

	met_vcorefs_src[VCORE_HIFI_IDX] =
		mtk_dvfsrc_query_debug_info(DVFSRC_HIFI_VCORE_REQ);
}

static void vcorefs_get_src_misc_info(struct mtk_dvfsrc_met *dvfs)
{
	u32 qos_bw0, qos_bw1, qos_bw2, qos_bw3, qos_bw4;
	u32 sta0, sta2;

	qos_bw0 = dvfsrc_met_read(dvfs, DVFSRC_SW_BW_0);
	qos_bw1 = dvfsrc_met_read(dvfs, DVFSRC_SW_BW_1);
	qos_bw2 = dvfsrc_met_read(dvfs, DVFSRC_SW_BW_2);
	qos_bw3 = dvfsrc_met_read(dvfs, DVFSRC_SW_BW_3);
	qos_bw4 = dvfsrc_met_read(dvfs, DVFSRC_SW_BW_4);
	sta0 = dvfsrc_met_read(dvfs, DVFSRC_DEBUG_STA_0);
	sta2 = dvfsrc_met_read(dvfs, DVFSRC_DEBUG_STA_2);

	met_vcorefs_src[SRC_MD2SPM_IDX] =
		sta0 & 0xFFFF;

	met_vcorefs_src[SRC_SCP_REQ_IDX] =
		(sta2 >> 14) & 0x1;

	met_vcorefs_src[SRC_PMQOS_TATOL_IDX] =
		qos_bw0 + qos_bw1 + qos_bw2 + qos_bw3 + qos_bw4;

	met_vcorefs_src[SRC_PMQOS_BW0_IDX] =
		qos_bw0;

	met_vcorefs_src[SRC_PMQOS_BW1_IDX] =
		qos_bw1;

	met_vcorefs_src[SRC_PMQOS_BW2_IDX] =
		qos_bw2;

	met_vcorefs_src[SRC_PMQOS_BW3_IDX] =
		qos_bw3;

	met_vcorefs_src[SRC_PMQOS_BW4_IDX] =
		qos_bw4;

	met_vcorefs_src[SRC_HRT_ISP_BW_IDX] =
		dvfsrc_met_read(dvfs, DVFSRC_ISP_HRT);

	met_vcorefs_src[SRC_MD_SCENARIO_IDX] =
		sta0 & 0xFFFF;

	met_vcorefs_src[SRC_MD_EMI_LATENCY_IDX] =
		(sta2 >> 12) & 0x3;

	met_vcorefs_src[SRC_HRT_MD_BW_IDX] =
		mtk_dvfsrc_query_debug_info(DVFSRC_MD_HRT_BW);

	met_vcorefs_src[SRC_HIFI_SCENARIO_IDX] =
		(sta2 >> 16) & 0xFF;
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
	int level, dram_opp;

	level = mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_DVFS_OPP);
	dram_opp = mtk_dvfsrc_query_opp_info(MTK_DVFSRC_CURR_DRAM_OPP);

	if ((dram_opp < 3) || (level == 10))
		return 8;
	else
		return 4;
}

const struct dvfsrc_met_config mt6779_met_config = {
	.dvfsrc_get_src_req_num = dvfsrc_get_src_req_num,
	.dvfsrc_get_src_req_name = dvfsrc_get_src_req_name,
	.dvfsrc_get_src_req = dvfsrc_get_src_req,
	.dvfsrc_get_ddr_ratio = dvfsrc_get_ddr_ratio,
	.get_current_level = dvfsrc_get_current_level,
};

