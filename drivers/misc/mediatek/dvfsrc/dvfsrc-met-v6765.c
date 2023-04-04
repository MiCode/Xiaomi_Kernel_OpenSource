// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk_dvfsrc.h>
#include <linux/regulator/consumer.h>

#include "dvfsrc-met.h"
#include "dvfsrc-common.h"

static inline u32 dvfsrc_met_read(struct mtk_dvfsrc_met *dvfs, u32 offset)
{
	return readl(dvfs->regs + offset);
}

enum met_src_index {
	MD2SPM = 0,
	DDR_SW_REQ1_PMQOS,
	DDR_SW_REQ2_CM,
	DDR_QOS_BW,
	VCORE_SW_REQ1_PMQOS,
	VCORE_SW_REQ2_CM,
	VCORE_SCP,
	PMQOS_TOTAL,
	PMQOS_PEAK,
	PMQOS_BW1,
	PMQOS_BW2,
	PMQOS_BW3,
	PMQOS_BW4,
	MD_REQ_OPP,
	SRC_MAX
};

/* met profile table */
static unsigned int met_vcorefs_src[SRC_MAX];

static char *met_src_name[SRC_MAX] = {
	"MD2SPM",
	"DDR__SW_REQ1_PMQOS",
	"DDR__SW_REQ2_CM",
	"DDR__QOS_BW",
	"VCORE__SW_REQ1_PMQOS",
	"VCORE__SW_REQ2_CM",
	"VCORE__SCP",
	"PMQOS_TOTAL",
	"PMQOS_PEAK",
	"PMQOS_BW1",
	"PMQOS_BW2",
	"PMQOS_BW3",
	"PMQOS_BW4",
	"MD_REQ_OPP",
};

#define DVFSRC_BASIC_CONTROL	(0x0)
#define DVFSRC_SW_REQ	(0x4)
#define DVFSRC_SW_REQ2	(0x8)
#define DVFSRC_EMI_QOS0            (0x24)
#define DVFSRC_EMI_QOS1            (0x28)
#define DVFSRC_VCORE_REQUEST (0x48)
#define DVFSRC_SW_BW_0	(0x160)
#define DVFSRC_SW_BW_1	(0x164)
#define DVFSRC_SW_BW_2	(0x168)
#define DVFSRC_SW_BW_3	(0x16C)
#define DVFSRC_SW_BW_4	(0x170)
#define DVFSRC_MD_SCENARIO         (0X310)
#define DVFSRC_RSRV_0              (0x600)
#define DVFSRC_LEVEL               (0xDC)
#define DVFSRC_EMI_MD2SPM0_T       (0x3C)
#define DVFSRC_EMI_MD2SPM1_T       (0x40)

#define DVFSRC_CURRENT_LEVEL(x)	(((x) >> 16) & 0x0000ffff)

/* DVFSRC_SW_REQ 0x4 */
#define EMI_SW_AP_SHIFT		0
#define EMI_SW_AP_MASK		0x3
#define VCORE_SW_AP_SHIFT	2
#define VCORE_SW_AP_MASK	0x3

/* DVFSRC_SW_REQ2 0x8 */
#define EMI_SW_AP2_SHIFT	0
#define EMI_SW_AP2_MASK		0x3
#define VCORE_SW_AP2_SHIFT	2
#define VCORE_SW_AP2_MASK	0x3

/* DVFSRC_VCORE_REQUEST 0x48 */
#define VCORE_SCP_GEAR_SHIFT	30
#define VCORE_SCP_GEAR_MASK	0x3

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
	return DVFSRC_CURRENT_LEVEL(dvfsrc_met_read(dvfsrc, DVFSRC_LEVEL));
}

static u32 dvfsrc_ddr_qos(struct mtk_dvfsrc_met *dvfs)
{
	u32 qos_peak_bw, qos_bw1, qos_bw2, qos_bw3, qos_bw4;
	u32 qos_total_bw;

	qos_peak_bw = dvfsrc_met_read(dvfs, DVFSRC_SW_BW_0);
	qos_bw1 = dvfsrc_met_read(dvfs, DVFSRC_SW_BW_1);
	qos_bw2 = dvfsrc_met_read(dvfs, DVFSRC_SW_BW_2);
	qos_bw3 = dvfsrc_met_read(dvfs, DVFSRC_SW_BW_3);
	qos_bw4 = dvfsrc_met_read(dvfs, DVFSRC_SW_BW_4);
	qos_total_bw = qos_bw1 + qos_bw2 + qos_bw3 + qos_bw4;

	met_vcorefs_src[PMQOS_PEAK] = qos_peak_bw;
	met_vcorefs_src[PMQOS_BW1] = qos_bw1;
	met_vcorefs_src[PMQOS_BW2] = qos_bw2;
	met_vcorefs_src[PMQOS_BW3] = qos_bw3;
	met_vcorefs_src[PMQOS_BW4] = qos_bw4;
	met_vcorefs_src[PMQOS_TOTAL] = qos_total_bw;

	qos_total_bw = max_t(u32, qos_total_bw, qos_peak_bw);

	return qos_total_bw;
}

static void vcorefs_get_src_ddr_req(struct mtk_dvfsrc_met *dvfs)
{
	unsigned int sw_req, sw_req2;
	unsigned int qos0_thres, qos1_thres;
	u32 total_bw;

	qos0_thres = dvfsrc_met_read(dvfs, DVFSRC_EMI_QOS0);
	qos1_thres = dvfsrc_met_read(dvfs, DVFSRC_EMI_QOS1);

	sw_req = dvfsrc_met_read(dvfs, DVFSRC_SW_REQ);
	met_vcorefs_src[DDR_SW_REQ1_PMQOS] = (sw_req >> EMI_SW_AP_SHIFT) & EMI_SW_AP_MASK;

	sw_req2 = dvfsrc_met_read(dvfs, DVFSRC_SW_REQ2);
	met_vcorefs_src[DDR_SW_REQ2_CM] = (sw_req2 >> EMI_SW_AP2_SHIFT) & EMI_SW_AP2_MASK;

	total_bw = dvfsrc_ddr_qos(dvfs);

	if (total_bw > qos1_thres)
		met_vcorefs_src[DDR_QOS_BW] = 2;
	else if (total_bw > qos0_thres)
		met_vcorefs_src[DDR_QOS_BW] = 1;
	else
		met_vcorefs_src[DDR_QOS_BW] = 0;
}

static void vcorefs_get_src_vcore_req(struct mtk_dvfsrc_met *dvfs)
{
	unsigned int sw_req, sw_req2, sw_req3;

	sw_req = dvfsrc_met_read(dvfs, DVFSRC_SW_REQ);
	met_vcorefs_src[VCORE_SW_REQ1_PMQOS] = (sw_req >> VCORE_SW_AP_SHIFT) & VCORE_SW_AP_MASK;

	sw_req2 = dvfsrc_met_read(dvfs, DVFSRC_SW_REQ2);
	met_vcorefs_src[VCORE_SW_REQ2_CM] = (sw_req2 >> VCORE_SW_AP2_SHIFT) & VCORE_SW_AP2_MASK;

	sw_req3 = dvfsrc_met_read(dvfs, DVFSRC_VCORE_REQUEST);
	met_vcorefs_src[VCORE_SCP] = (sw_req3 >> VCORE_SCP_GEAR_SHIFT) & VCORE_SCP_GEAR_MASK;
}

static void vcorefs_get_src_misc_info(struct mtk_dvfsrc_met *dvfs)
{
	met_vcorefs_src[MD2SPM] = dvfsrc_met_read(dvfs, DVFSRC_MD_SCENARIO);
	met_vcorefs_src[MD_REQ_OPP] = dvfsrc_met_read(dvfs, DVFSRC_RSRV_0);
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
	return 0;
}

const struct dvfsrc_met_config mt6765_met_config = {
	.dvfsrc_get_src_req_num = dvfsrc_get_src_req_num,
	.dvfsrc_get_src_req_name = dvfsrc_get_src_req_name,
	.dvfsrc_get_src_req = dvfsrc_get_src_req,
	.dvfsrc_get_ddr_ratio = dvfsrc_get_ddr_ratio,
	.get_current_level = dvfsrc_get_current_level,
};

