// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 */
#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk_dvfsrc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <dt-bindings/soc/mtk,dvfsrc.h>
#include "mtk-scpsys.h"

#define DVFSRC_IDLE     0x00
#define DVFSRC_GET_TARGET_LEVEL(x)  (((x) >> 0) & 0x0000ffff)
#define DVFSRC_GET_CURRENT_LEVEL(x) (((x) >> 16) & 0x0000ffff)
#define kbps_to_mbps(x) (div_u64(x, 1000))

#define MT8183_DVFSRC_OPP_LP4   0
#define MT8183_DVFSRC_OPP_LP4X  1
#define MT8183_DVFSRC_OPP_LP3   2

#define POLL_TIMEOUT        1000
#define STARTUP_TIME        1

#define MTK_SIP_DVFSRC_INIT		0x00

#define DVFSRC_OPP_DESC(_opp_table)	\
{	\
	.opps = _opp_table,	\
	.num_opp = ARRAY_SIZE(_opp_table),	\
}

struct dvfsrc_opp {
	u32 vcore_opp;
	u32 dram_opp;
};

struct dvfsrc_domain {
	u32 id;
	u32 state;
};

struct dvfsrc_opp_desc {
	const struct dvfsrc_opp *opps;
	u32 num_opp;
};

struct mtk_dvfsrc;
struct dvfsrc_soc_data {
	const int *regs;
	u32 num_domains;
	struct dvfsrc_domain *domains;
	u32 num_opp_desc;
	const struct dvfsrc_opp_desc *opps_desc;
	void (*dvfsrc_hw_init)(struct mtk_dvfsrc *dvfsrc);
	int (*get_target_level)(struct mtk_dvfsrc *dvfsrc);
	int (*get_current_level)(struct mtk_dvfsrc *dvfsrc);
	u32 (*get_vcore_level)(struct mtk_dvfsrc *dvfsrc);
	u32 (*get_vcp_level)(struct mtk_dvfsrc *dvfsrc);
	void (*set_dram_bw)(struct mtk_dvfsrc *dvfsrc, u64 bw);
	void (*set_dram_hrtbw)(struct mtk_dvfsrc *dvfsrc, u64 bw);
	void (*set_dram_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	void (*set_opp_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	void (*set_vcore_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	void (*set_vscp_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	int (*wait_for_opp_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	int (*wait_for_vcore_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	int (*wait_for_dram_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
};

struct mtk_dvfsrc {
	struct device *dev;
	struct platform_device *regulator;
	struct platform_device *icc;
	const struct dvfsrc_soc_data *dvd;
	int dram_type;
	const struct dvfsrc_opp_desc *curr_opps;
	void __iomem *regs;
	struct mutex req_lock;
	struct mutex pstate_lock;
	struct notifier_block scpsys_notifier;
	bool dvfsrc_enable;
};

static u32 dvfsrc_read(struct mtk_dvfsrc *dvfs, u32 offset)
{
	return readl(dvfs->regs + dvfs->dvd->regs[offset]);
}

static void dvfsrc_write(struct mtk_dvfsrc *dvfs, u32 offset, u32 val)
{
	writel(val, dvfs->regs + dvfs->dvd->regs[offset]);
}

#define dvfsrc_rmw(dvfs, offset, val, mask, shift) \
	dvfsrc_write(dvfs, offset, \
		(dvfsrc_read(dvfs, offset) & ~(mask << shift)) | (val << shift))

enum dvfsrc_regs {
	DVFSRC_BASIC_CONTROL,
	DVFSRC_SW_REQ,
	DVFSRC_SW_REQ2,
	DVFSRC_LEVEL,
	DVFSRC_TARGET_LEVEL,
	DVFSRC_SW_BW,
	DVFSRC_SW_HRT_BW,
	DVFSRC_VCORE_REQUEST,
	DVFSRC_LAST,

	/* MT6873 ip series regs */
	DVFSRC_INT_EN,
	DVFSRC_VCORE_USER_REQ,
	DVFSRC_BW_USER_REQ,
	DVFSRC_TIMEOUT_NEXTREQ,
	DVFSRC_LEVEL_LABEL_0_1,
	DVFSRC_LEVEL_LABEL_2_3,
	DVFSRC_LEVEL_LABEL_4_5,
	DVFSRC_LEVEL_LABEL_6_7,
	DVFSRC_LEVEL_LABEL_8_9,
	DVFSRC_LEVEL_LABEL_10_11,
	DVFSRC_LEVEL_LABEL_12_13,
	DVFSRC_LEVEL_LABEL_14_15,
	DVFSRC_QOS_EN,
	DVFSRC_HRT_BW_BASE,
	DVFSRC_EMI_MON_DEBOUNCE_TIME,
	DVFSRC_MD_LATENCY_IMPROVE,
	DVFSRC_BASIC_CONTROL_3,
	DVFSRC_DEBOUNCE_TIME,
	DVFSRC_LEVEL_MASK,
	DVFSRC_95MD_SCEN_BW0,
	DVFSRC_95MD_SCEN_BW1,
	DVFSRC_95MD_SCEN_BW2,
	DVFSRC_95MD_SCEN_BW3,
	DVFSRC_95MD_SCEN_BW0_T,
	DVFSRC_95MD_SCEN_BW1_T,
	DVFSRC_95MD_SCEN_BW2_T,
	DVFSRC_95MD_SCEN_BW3_T,
	DVFSRC_95MD_SCEN_BW4,
	DVFSRC_RSRV_5,
	DVFSRC_DDR_REQUEST,
	DVFSRC_DDR_REQUEST3,
	DVFSRC_DDR_REQUEST5,
	DVFSRC_DDR_REQUEST6,
	DVFSRC_DDR_REQUEST7,
	DVFSRC_DDR_QOS0,
	DVFSRC_DDR_QOS1,
	DVFSRC_DDR_QOS2,
	DVFSRC_DDR_QOS3,
	DVFSRC_DDR_QOS4,
	DVFSRC_HRT_REQ_UNIT,
	DVSFRC_HRT_REQ_MD_URG,
	DVFSRC_HRT_REQ_MD_BW_0,
	DVFSRC_HRT_REQ_MD_BW_1,
	DVFSRC_HRT_REQ_MD_BW_2,
	DVFSRC_HRT_REQ_MD_BW_3,
	DVFSRC_HRT_REQ_MD_BW_4,
	DVFSRC_HRT_REQ_MD_BW_5,
	DVFSRC_HRT_REQ_MD_BW_6,
	DVFSRC_HRT_REQ_MD_BW_7,
	DVFSRC_HRT1_REQ_MD_BW_0,
	DVFSRC_HRT1_REQ_MD_BW_1,
	DVFSRC_HRT1_REQ_MD_BW_2,
	DVFSRC_HRT1_REQ_MD_BW_3,
	DVFSRC_HRT1_REQ_MD_BW_4,
	DVFSRC_HRT1_REQ_MD_BW_5,
	DVFSRC_HRT1_REQ_MD_BW_6,
	DVFSRC_HRT1_REQ_MD_BW_7,
	DVFSRC_HRT_REQ_MD_BW_8,
	DVFSRC_HRT_REQ_MD_BW_9,
	DVFSRC_HRT_REQ_MD_BW_10,
	DVFSRC_HRT1_REQ_MD_BW_8,
	DVFSRC_HRT1_REQ_MD_BW_9,
	DVFSRC_HRT1_REQ_MD_BW_10,
	DVFSRC_HRT_REQUEST,
	DVFSRC_HRT_HIGH_2,
	DVFSRC_HRT_HIGH_1,
	DVFSRC_HRT_HIGH,
	DVFSRC_HRT_LOW_2,
	DVFSRC_HRT_LOW_1,
	DVFSRC_HRT_LOW,
	DVFSRC_DDR_ADD_REQUEST,
	DVFSRC_DDR_QOS5,
	DVFSRC_DDR_QOS6,
	DVFSRC_HRT_HIGH_3,
	DVFSRC_HRT_LOW_3,
	DVFSRC_LEVEL_LABEL_16_17,
	DVFSRC_LEVEL_LABEL_18_19,
	DVFSRC_LEVEL_LABEL_20_21,
	DVFSRC_LEVEL_LABEL_22_23,
	DVFSRC_LEVEL_LABEL_24_25,
	DVFSRC_LEVEL_LABEL_26_27,
	DVFSRC_LEVEL_LABEL_28_29,
	DVFSRC_LEVEL_LABEL_30_31,
	DVFSRC_CURRENT_FORCE,
	DVFSRC_TARGET_FORCE,
};

static const int mt8183_regs[] = {
	[DVFSRC_SW_REQ] =	0x4,
	[DVFSRC_SW_REQ2] =	0x8,
	[DVFSRC_LEVEL] =	0xDC,
	[DVFSRC_SW_BW] =	0x160,
	[DVFSRC_LAST] =		0x308,
};

static const int mt6873_regs[] = {
	[DVFSRC_BASIC_CONTROL] =	0x0,
	[DVFSRC_SW_REQ] =		0xC,
	[DVFSRC_LEVEL] =		0xD44,
	[DVFSRC_SW_BW] =		0x260,
	[DVFSRC_SW_HRT_BW] =		0x290,
	[DVFSRC_TARGET_LEVEL] =		0xD48,
	[DVFSRC_VCORE_REQUEST] =	0x6C,

	[DVFSRC_INT_EN] =		0xC8,
	[DVFSRC_VCORE_USER_REQ] =	0xE4,
	[DVFSRC_BW_USER_REQ] =		0xE8,
	[DVFSRC_TIMEOUT_NEXTREQ] =	0xF8,
	[DVFSRC_LEVEL_LABEL_0_1] =	0x100,
	[DVFSRC_LEVEL_LABEL_2_3] =	0x104,
	[DVFSRC_LEVEL_LABEL_4_5] =	0x108,
	[DVFSRC_LEVEL_LABEL_6_7] =	0x10C,
	[DVFSRC_LEVEL_LABEL_8_9] =	0x110,
	[DVFSRC_LEVEL_LABEL_10_11] =	0x114,
	[DVFSRC_LEVEL_LABEL_12_13] =	0x118,
	[DVFSRC_LEVEL_LABEL_14_15] =	0x11C,
	[DVFSRC_QOS_EN] =		0x280,
	[DVFSRC_HRT_BW_BASE] =		0x294,
	[DVFSRC_EMI_MON_DEBOUNCE_TIME] =	0x308,
	[DVFSRC_MD_LATENCY_IMPROVE] =	0x30C,
	[DVFSRC_BASIC_CONTROL_3] =	0x310,
	[DVFSRC_DEBOUNCE_TIME] =	0x314,
	[DVFSRC_LEVEL_MASK] =		0x318,
	[DVFSRC_95MD_SCEN_BW0] =	0x524,
	[DVFSRC_95MD_SCEN_BW1] =	0x528,
	[DVFSRC_95MD_SCEN_BW2] =	0x52C,
	[DVFSRC_95MD_SCEN_BW3] =	0x530,
	[DVFSRC_95MD_SCEN_BW0_T] =	0x534,
	[DVFSRC_95MD_SCEN_BW1_T] =	0x538,
	[DVFSRC_95MD_SCEN_BW2_T] =	0x53C,
	[DVFSRC_95MD_SCEN_BW3_T] =	0x540,
	[DVFSRC_95MD_SCEN_BW4] =	0x544,
	[DVFSRC_RSRV_5] =		0x614,
	[DVFSRC_DDR_REQUEST] =		0xA00,
	[DVFSRC_DDR_REQUEST3] =		0xA08,
	[DVFSRC_DDR_REQUEST5] =		0xA10,
	[DVFSRC_DDR_REQUEST6] =		0xA14,
	[DVFSRC_DDR_REQUEST7] =		0xA18,
	[DVFSRC_DDR_QOS0] =		0xA34,
	[DVFSRC_DDR_QOS1] =		0xA38,
	[DVFSRC_DDR_QOS2] =		0xA3C,
	[DVFSRC_DDR_QOS3] =		0xA40,
	[DVFSRC_DDR_QOS4] =		0xA44,
	[DVFSRC_HRT_REQ_UNIT] =		0xA60,
	[DVSFRC_HRT_REQ_MD_URG] =	0xA64,
	[DVFSRC_HRT_REQ_MD_BW_0] =	0xA68,
	[DVFSRC_HRT_REQ_MD_BW_1] =	0xA6C,
	[DVFSRC_HRT_REQ_MD_BW_2] =	0xA70,
	[DVFSRC_HRT_REQ_MD_BW_3] =	0xA74,
	[DVFSRC_HRT_REQ_MD_BW_4] =	0xA78,
	[DVFSRC_HRT_REQ_MD_BW_5] =	0xA7C,
	[DVFSRC_HRT_REQ_MD_BW_6] =	0xA80,
	[DVFSRC_HRT_REQ_MD_BW_7] =	0xA84,
	[DVFSRC_HRT1_REQ_MD_BW_0] =	0xA88,
	[DVFSRC_HRT1_REQ_MD_BW_1] =	0xA8C,
	[DVFSRC_HRT1_REQ_MD_BW_2] =	0xA90,
	[DVFSRC_HRT1_REQ_MD_BW_3] =	0xA94,
	[DVFSRC_HRT1_REQ_MD_BW_4] =	0xA98,
	[DVFSRC_HRT1_REQ_MD_BW_5] =	0xA9C,
	[DVFSRC_HRT1_REQ_MD_BW_6] =	0xAA0,
	[DVFSRC_HRT1_REQ_MD_BW_7] =	0xAA4,
	[DVFSRC_HRT_REQ_MD_BW_8] =	0xAA8,
	[DVFSRC_HRT_REQ_MD_BW_9] =	0xAAC,
	[DVFSRC_HRT_REQ_MD_BW_10] =	0xAB0,
	[DVFSRC_HRT1_REQ_MD_BW_8] =	0xAB4,
	[DVFSRC_HRT1_REQ_MD_BW_9] =	0xAB8,
	[DVFSRC_HRT1_REQ_MD_BW_10] =	0xABC,
	[DVFSRC_HRT_REQUEST] =		0xAC4,
	[DVFSRC_HRT_HIGH_2] =		0xAC8,
	[DVFSRC_HRT_HIGH_1] =		0xACC,
	[DVFSRC_HRT_HIGH] =		0xAD0,
	[DVFSRC_HRT_LOW_2] =		0xAD4,
	[DVFSRC_HRT_LOW_1] =		0xAD8,
	[DVFSRC_HRT_LOW] =		0xADC,
	[DVFSRC_DDR_ADD_REQUEST] =	0xAE0,
	[DVFSRC_DDR_QOS5] =		0xD18,
	[DVFSRC_DDR_QOS6] =		0xD1C,
	[DVFSRC_HRT_HIGH_3] =		0xD38,
	[DVFSRC_HRT_LOW_3] =		0xD3C,
	[DVFSRC_LEVEL_LABEL_16_17] =	0xD4C,
	[DVFSRC_LEVEL_LABEL_18_19] =	0xD50,
	[DVFSRC_LEVEL_LABEL_20_21] =	0xD54,
	[DVFSRC_LEVEL_LABEL_22_23] =	0xD58,
	[DVFSRC_LEVEL_LABEL_24_25] =	0xD5C,
	[DVFSRC_LEVEL_LABEL_26_27] =	0xD60,
	[DVFSRC_LEVEL_LABEL_28_29] =	0xD64,
	[DVFSRC_LEVEL_LABEL_30_31] =	0xD68,
	[DVFSRC_CURRENT_FORCE] =	0xD6C,
	[DVFSRC_TARGET_FORCE] =		0xD70,
};

static const struct dvfsrc_opp *get_current_opp(struct mtk_dvfsrc *dvfsrc)
{
	int level;

	level = dvfsrc->dvd->get_current_level(dvfsrc);
	return &dvfsrc->curr_opps->opps[level];
}

static int dvfsrc_is_idle(struct mtk_dvfsrc *dvfsrc)
{
	if (!dvfsrc->dvd->get_target_level)
		return true;

	return dvfsrc->dvd->get_target_level(dvfsrc);
}

static int dvfsrc_wait_for_idle(struct mtk_dvfsrc *dvfsrc)
{
	int state;

	return readx_poll_timeout_atomic(dvfsrc_is_idle, dvfsrc,
		state, state == DVFSRC_IDLE,
		STARTUP_TIME, POLL_TIMEOUT);
}

static int dvfsrc_wait_for_vcore_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	const struct dvfsrc_opp *curr;

	return readx_poll_timeout_atomic(get_current_opp, dvfsrc, curr,
		curr->vcore_opp >= level, STARTUP_TIME,
		POLL_TIMEOUT);
}

static int dvfsrc_wait_for_dram_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	const struct dvfsrc_opp *curr;

	return readx_poll_timeout_atomic(get_current_opp, dvfsrc, curr,
		curr->dram_opp >= level, STARTUP_TIME,
		POLL_TIMEOUT);
}

static int mt8183_wait_for_opp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	const struct dvfsrc_opp *target, *curr;
	int ret;

	target = &dvfsrc->curr_opps->opps[level];
	ret = readx_poll_timeout(get_current_opp, dvfsrc, curr,
				 curr->dram_opp >= target->dram_opp &&
				 curr->vcore_opp >= target->vcore_opp,
				 STARTUP_TIME, POLL_TIMEOUT);
	if (ret < 0) {
		dev_warn(dvfsrc->dev,
			 "timeout, target: %u, dram: %d, vcore: %d\n", level,
			 curr->dram_opp, curr->vcore_opp);
		return ret;
	}

	return 0;
}

static int mt8183_get_target_level(struct mtk_dvfsrc *dvfsrc)
{
	return DVFSRC_GET_TARGET_LEVEL(dvfsrc_read(dvfsrc, DVFSRC_LEVEL));
}

static int mt8183_get_current_level(struct mtk_dvfsrc *dvfsrc)
{
	int level;

	/* HW level 0 is begin from 0x10000 */
	level = DVFSRC_GET_CURRENT_LEVEL(dvfsrc_read(dvfsrc, DVFSRC_LEVEL));
	/* Array index start from 0 */
	return ffs(level) - 1;
}

static u32 mt8183_get_vcore_level(struct mtk_dvfsrc *dvfsrc)
{
	return (dvfsrc_read(dvfsrc, DVFSRC_SW_REQ2) >> 2) & 0x3;
}

static void mt8183_set_dram_bw(struct mtk_dvfsrc *dvfsrc, u64 bw)
{
	dvfsrc_write(dvfsrc, DVFSRC_SW_BW, div_u64(kbps_to_mbps(bw), 100));
}

static void mt8183_set_opp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	int vcore_opp, dram_opp;
	const struct dvfsrc_opp *opp;

	/* translate pstate to dvfsrc level, and set it to DVFSRC HW */
	opp = &dvfsrc->curr_opps->opps[level];
	vcore_opp = opp->vcore_opp;
	dram_opp = opp->dram_opp;

	dev_dbg(dvfsrc->dev, "vcore_opp: %d, dram_opp: %d\n",
		vcore_opp, dram_opp);
	dvfsrc_write(dvfsrc, DVFSRC_SW_REQ, dram_opp | vcore_opp << 2);
}

static void mt8183_set_vcore_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	dvfsrc_write(dvfsrc, DVFSRC_SW_REQ2, level << 2);
}

static int mt6873_get_target_level(struct mtk_dvfsrc *dvfsrc)
{
	return dvfsrc_read(dvfsrc, DVFSRC_TARGET_LEVEL);
}

static int mt6873_get_current_level(struct mtk_dvfsrc *dvfsrc)
{
	u32 curr_level;

	/* HW level 0 is begin from 0x1, and max opp is 0x1*/
	curr_level = ffs(dvfsrc_read(dvfsrc, DVFSRC_LEVEL));
	if (curr_level > dvfsrc->curr_opps->num_opp)
		curr_level = 0;
	else
		curr_level = dvfsrc->curr_opps->num_opp - curr_level;

	return curr_level;
}

static int mt6873_wait_for_opp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	const struct dvfsrc_opp *target, *curr;

	target = &dvfsrc->curr_opps->opps[level];
	return readx_poll_timeout_atomic(get_current_opp, dvfsrc, curr,
		curr->dram_opp >= target->dram_opp,
		STARTUP_TIME, POLL_TIMEOUT);
}

static u32 mt6873_get_vcore_level(struct mtk_dvfsrc *dvfsrc)
{
	return (dvfsrc_read(dvfsrc, DVFSRC_SW_REQ) >> 4) & 0x7;
}

static u32 mt6873_get_vcp_level(struct mtk_dvfsrc *dvfsrc)
{
	return (dvfsrc_read(dvfsrc, DVFSRC_VCORE_REQUEST) >> 12) & 0x7;
}

static void mt6873_set_dram_bw(struct mtk_dvfsrc *dvfsrc, u64 bw)
{
	bw = div_u64(kbps_to_mbps(bw), 100);
	bw = min_t(u64, bw, 0xFF);
	dvfsrc_write(dvfsrc, DVFSRC_SW_BW, bw);
}

static void mt6873_set_dram_hrtbw(struct mtk_dvfsrc *dvfsrc, u64 bw)
{
	bw = div_u64((kbps_to_mbps(bw) + 29), 30);
	bw = min_t(u64, bw, 0x3FF);
	dvfsrc_write(dvfsrc, DVFSRC_SW_HRT_BW, bw);
}

static void mt6873_set_dram_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	mutex_lock(&dvfsrc->req_lock);
	dvfsrc_rmw(dvfsrc, DVFSRC_SW_REQ, level, 0x7, 12);
	mutex_unlock(&dvfsrc->req_lock);
}

static void mt6873_set_vcore_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	mutex_lock(&dvfsrc->req_lock);
	dvfsrc_rmw(dvfsrc, DVFSRC_SW_REQ, level, 0x7, 4);
	mutex_unlock(&dvfsrc->req_lock);
}

static void mt6873_set_vscp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	dvfsrc_rmw(dvfsrc, DVFSRC_VCORE_REQUEST, level, 0x7, 12);
}

static void mt6873_set_opp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	const struct dvfsrc_opp *opp;

	opp = &dvfsrc->curr_opps->opps[level];
	mt6873_set_dram_level(dvfsrc, opp->dram_opp);
}

void mtk_dvfsrc_send_request(const struct device *dev, u32 cmd, u64 data)
{
	int ret = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	dev_dbg(dvfsrc->dev, "cmd: %d, data: %llu\n", cmd, data);

	switch (cmd) {
	case MTK_DVFSRC_CMD_BW_REQUEST:
		dvfsrc->dvd->set_dram_bw(dvfsrc, data);
		goto out;
	case MTK_DVFSRC_CMD_OPP_REQUEST:
		dvfsrc->dvd->set_opp_level(dvfsrc, data);
		break;
	case MTK_DVFSRC_CMD_VCORE_REQUEST:
		dvfsrc->dvd->set_vcore_level(dvfsrc, data);
		break;
	case MTK_DVFSRC_CMD_HRTBW_REQUEST:
		if (dvfsrc->dvd->set_dram_hrtbw)
			dvfsrc->dvd->set_dram_hrtbw(dvfsrc, data);
		else
			goto out;
		break;
	case MTK_DVFSRC_CMD_DRAM_REQUEST:
		dvfsrc->dvd->set_dram_level(dvfsrc, data);
		break;
	case MTK_DVFSRC_CMD_VSCP_REQUEST:
		dvfsrc->dvd->set_vscp_level(dvfsrc, data);
		break;
	default:
		dev_err(dvfsrc->dev, "unknown command: %d\n", cmd);
		goto out;
	}

	if (!dvfsrc->dvfsrc_enable)
		return;

	/* DVFSRC need to wait at least 2T(~196ns) to handle request
	 * after recieving command
	 */
	udelay(STARTUP_TIME);
	dvfsrc_wait_for_idle(dvfsrc);
	/* The previous change may be requested by previous request.
	 * So we delay 1us , then start checking opp is reached enough.
	 */
	udelay(STARTUP_TIME);

	switch (cmd) {
	case MTK_DVFSRC_CMD_OPP_REQUEST:
		if (dvfsrc->dvd->wait_for_opp_level)
			ret = dvfsrc->dvd->wait_for_opp_level(dvfsrc, data);
		break;
	case MTK_DVFSRC_CMD_VCORE_REQUEST:
	case MTK_DVFSRC_CMD_VSCP_REQUEST:
		ret = dvfsrc->dvd->wait_for_vcore_level(dvfsrc, data);
		break;
	case MTK_DVFSRC_CMD_HRTBW_REQUEST:
		ret = dvfsrc_wait_for_idle(dvfsrc);
		break;
	case MTK_DVFSRC_CMD_DRAM_REQUEST:
		ret = dvfsrc->dvd->wait_for_dram_level(dvfsrc, data);
		break;
	}
out:
	if (ret < 0) {
		dev_warn(dvfsrc->dev,
			 "%d: idle timeout, data: %llu, last: %d -> %d\n",
			 cmd, data,
			 dvfsrc->dvd->get_current_level(dvfsrc),
			 dvfsrc->dvd->get_target_level(dvfsrc));
	}
}
EXPORT_SYMBOL(mtk_dvfsrc_send_request);

int mtk_dvfsrc_query_info(const struct device *dev, u32 cmd, int *data)
{
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	switch (cmd) {
	case MTK_DVFSRC_CMD_VCORE_QUERY:
		*data = dvfsrc->dvd->get_vcore_level(dvfsrc);
		break;
	case MTK_DVFSRC_CMD_VCP_QUERY:
		*data = dvfsrc->dvd->get_vcp_level(dvfsrc);
		break;
	case MTK_DVFSRC_CMD_CURR_LEVEL_QUERY:
		*data = dvfsrc->dvd->get_current_level(dvfsrc);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(mtk_dvfsrc_query_info);

static int dvfsrc_set_performance(struct notifier_block *b,
				  unsigned long pstate, void *v)
{
	bool match = false;
	int i;
	struct mtk_dvfsrc *dvfsrc;
	struct scp_event_data *sc = v;
	struct dvfsrc_domain *d;
	u32 highest;

	if (sc->event_type != MTK_SCPSYS_PSTATE)
		return 0;

	dvfsrc = container_of(b, struct mtk_dvfsrc, scpsys_notifier);

	/* feature not support */
	if (!dvfsrc->dvd->num_domains)
		return 0;

	d = dvfsrc->dvd->domains;

	if (pstate > dvfsrc->curr_opps->num_opp) {
		dev_err(dvfsrc->dev, "pstate out of range = %ld\n", pstate);
		return 0;
	}

	mutex_lock(&dvfsrc->pstate_lock);

	for (i = 0, highest = 0; i < dvfsrc->dvd->num_domains; i++, d++) {
		if (sc->domain_id == d->id) {
			d->state = pstate;
			match = true;
		}
		highest = max(highest, d->state);
	}

	if (!match)
		goto out;

	/* pstat start from level 1, array index start from 0 */
	mtk_dvfsrc_send_request(dvfsrc->dev, MTK_DVFSRC_CMD_OPP_REQUEST,
				highest - 1);

out:
	mutex_unlock(&dvfsrc->pstate_lock);
	return 0;
}

static void pstate_notifier_register(struct mtk_dvfsrc *dvfsrc)
{
	dvfsrc->scpsys_notifier.notifier_call = dvfsrc_set_performance;
	register_scpsys_notifier(&dvfsrc->scpsys_notifier);
}

static int mtk_dvfsrc_probe(struct platform_device *pdev)
{
	struct arm_smccc_res ares;
	struct resource *res;
	struct mtk_dvfsrc *dvfsrc;
	int ret;

	dvfsrc = devm_kzalloc(&pdev->dev, sizeof(*dvfsrc), GFP_KERNEL);
	if (!dvfsrc)
		return -ENOMEM;

	dvfsrc->dvd = of_device_get_match_data(&pdev->dev);
	dvfsrc->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dvfsrc->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(dvfsrc->regs))
		return PTR_ERR(dvfsrc->regs);

	mutex_init(&dvfsrc->req_lock);
	mutex_init(&dvfsrc->pstate_lock);

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, MTK_SIP_DVFSRC_INIT, 0, 0, 0,
		0, 0, 0, &ares);

	if (!ares.a0) {
		dvfsrc->dram_type = ares.a1;
		dvfsrc->dvd->dvfsrc_hw_init(dvfsrc);
		dvfsrc->dvfsrc_enable = true;
	} else
		dev_info(dvfsrc->dev, "dvfs mode is disabled\n");

	dvfsrc->curr_opps = &dvfsrc->dvd->opps_desc[dvfsrc->dram_type];
	platform_set_drvdata(pdev, dvfsrc);
	if (dvfsrc->dvd->num_domains)
		pstate_notifier_register(dvfsrc);

	dvfsrc->regulator = platform_device_register_data(dvfsrc->dev,
			"mtk-dvfsrc-regulator", -1, NULL, 0);
	if (IS_ERR(dvfsrc->regulator)) {
		dev_err(dvfsrc->dev, "Failed create regulator device\n");
		ret = PTR_ERR(dvfsrc->regulator);
		goto err;
	}

	dvfsrc->icc = platform_device_register_data(dvfsrc->dev,
			"mediatek-emi-icc", -1, NULL, 0);
	if (IS_ERR(dvfsrc->icc)) {
		dev_err(dvfsrc->dev, "Failed create icc device\n");
		ret = PTR_ERR(dvfsrc->icc);
		goto unregister_regulator;
	}

	ret = devm_of_platform_populate(dvfsrc->dev);
	if (ret < 0)
		goto unregister_icc;

	return 0;

unregister_icc:
	platform_device_unregister(dvfsrc->icc);
unregister_regulator:
	platform_device_unregister(dvfsrc->regulator);
err:
	return ret;
}

static const struct dvfsrc_opp dvfsrc_opp_mt8183_lp4[] = {
	{0, 0}, {0, 1}, {0, 2}, {1, 2},
};

static const struct dvfsrc_opp dvfsrc_opp_mt8183_lp3[] = {
	{0, 0}, {0, 1}, {1, 1}, {1, 2},
};

static const struct dvfsrc_opp_desc dvfsrc_opp_mt8183_desc[] = {
	DVFSRC_OPP_DESC(dvfsrc_opp_mt8183_lp4),
	DVFSRC_OPP_DESC(dvfsrc_opp_mt8183_lp3),
	DVFSRC_OPP_DESC(dvfsrc_opp_mt8183_lp3),
};


static const struct dvfsrc_soc_data mt8183_data = {
	.opps_desc = dvfsrc_opp_mt8183_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt8183_desc),
	.regs = mt8183_regs,
	.get_target_level = mt8183_get_target_level,
	.get_current_level = mt8183_get_current_level,
	.get_vcore_level = mt8183_get_vcore_level,
	.set_dram_bw = mt8183_set_dram_bw,
	.set_opp_level = mt8183_set_opp_level,
	.set_vcore_level = mt8183_set_vcore_level,
	.wait_for_opp_level = mt8183_wait_for_opp_level,
	.wait_for_vcore_level = dvfsrc_wait_for_vcore_level,
};

static void dvfsrc_init_mt6873(struct mtk_dvfsrc *dvfsrc)
{
	/* Setup opp table */
	dvfsrc_write(dvfsrc, DVFSRC_LEVEL_LABEL_0_1, 0x50436053);
	dvfsrc_write(dvfsrc, DVFSRC_LEVEL_LABEL_2_3, 0x40335042);
	dvfsrc_write(dvfsrc, DVFSRC_LEVEL_LABEL_4_5, 0x40314032);
	dvfsrc_write(dvfsrc, DVFSRC_LEVEL_LABEL_6_7, 0x30223023);
	dvfsrc_write(dvfsrc, DVFSRC_LEVEL_LABEL_8_9, 0x20133021);
	dvfsrc_write(dvfsrc, DVFSRC_LEVEL_LABEL_10_11, 0x20112012);
	dvfsrc_write(dvfsrc, DVFSRC_LEVEL_LABEL_12_13, 0x10032010);
	dvfsrc_write(dvfsrc, DVFSRC_LEVEL_LABEL_14_15, 0x10011002);
	dvfsrc_write(dvfsrc, DVFSRC_LEVEL_LABEL_16_17, 0x00131000);
	dvfsrc_write(dvfsrc, DVFSRC_LEVEL_LABEL_18_19, 0x00110012);
	dvfsrc_write(dvfsrc, DVFSRC_LEVEL_LABEL_20_21, 0x00000010);
	/* Setup hw emi qos policy */
	dvfsrc_write(dvfsrc, DVFSRC_DDR_REQUEST, 0x00004321);
	dvfsrc_write(dvfsrc, DVFSRC_DDR_REQUEST3, 0x00000065);
	/* Setup up HRT QOS policy */
	dvfsrc_write(dvfsrc, DVFSRC_HRT_BW_BASE, 0x00000004);
	dvfsrc_write(dvfsrc, DVFSRC_HRT_REQ_UNIT, 0x0000001E);
	dvfsrc_write(dvfsrc, DVFSRC_HRT_HIGH_3, 0x18A618A6);
	dvfsrc_write(dvfsrc, DVFSRC_HRT_HIGH_2, 0x18A61183);
	dvfsrc_write(dvfsrc, DVFSRC_HRT_HIGH_1, 0x0D690B80);
	dvfsrc_write(dvfsrc, DVFSRC_HRT_HIGH, 0x070804B0);
	dvfsrc_write(dvfsrc, DVFSRC_HRT_LOW_3, 0x18A518A5);
	dvfsrc_write(dvfsrc, DVFSRC_HRT_LOW_2, 0x18A51182);
	dvfsrc_write(dvfsrc, DVFSRC_HRT_LOW_1, 0x0D680B7F);
	dvfsrc_write(dvfsrc, DVFSRC_HRT_LOW, 0x070704AF);
	dvfsrc_write(dvfsrc, DVFSRC_HRT_REQUEST, 0x66654321);
	/* Setup up SRT QOS policy */
	dvfsrc_write(dvfsrc, DVFSRC_QOS_EN, 0x0000407C);
	dvfsrc_write(dvfsrc, DVFSRC_DDR_QOS0, 0x00000019);
	dvfsrc_write(dvfsrc, DVFSRC_DDR_QOS1, 0x00000026);
	dvfsrc_write(dvfsrc, DVFSRC_DDR_QOS2, 0x00000033);
	dvfsrc_write(dvfsrc, DVFSRC_DDR_QOS3, 0x0000003B);
	dvfsrc_write(dvfsrc, DVFSRC_DDR_QOS4, 0x0000004C);
	dvfsrc_write(dvfsrc, DVFSRC_DDR_QOS5, 0x00000066);
	dvfsrc_write(dvfsrc, DVFSRC_DDR_QOS6, 0x00000066);
	dvfsrc_write(dvfsrc, DVFSRC_DDR_REQUEST5, 0x54321000);
	dvfsrc_write(dvfsrc, DVFSRC_DDR_REQUEST7, 0x66000000);
	/* Setup up MD request policy */
	dvfsrc_write(dvfsrc, DVFSRC_BASIC_CONTROL_3, 0x00000006);
	dvfsrc_write(dvfsrc, DVFSRC_DEBOUNCE_TIME, 0x00001965);
	dvfsrc_write(dvfsrc, DVFSRC_MD_LATENCY_IMPROVE, 0x00000040);
	dvfsrc_write(dvfsrc, DVFSRC_DDR_ADD_REQUEST, 0x66543210);
	dvfsrc_write(dvfsrc, DVFSRC_EMI_MON_DEBOUNCE_TIME, 0x4C2D0000);
	dvfsrc_write(dvfsrc, DVFSRC_LEVEL_MASK, 0x000EE000);
	dvfsrc_write(dvfsrc, DVSFRC_HRT_REQ_MD_URG, 0x000D20D2);
	dvfsrc_write(dvfsrc, DVFSRC_HRT_REQ_MD_BW_0, 0x00200802);
	dvfsrc_write(dvfsrc, DVFSRC_HRT_REQ_MD_BW_1, 0x00200802);
	dvfsrc_write(dvfsrc, DVFSRC_HRT_REQ_MD_BW_2, 0x00200800);
	dvfsrc_write(dvfsrc, DVFSRC_HRT_REQ_MD_BW_3, 0x00400802);
	dvfsrc_write(dvfsrc, DVFSRC_HRT_REQ_MD_BW_4, 0x00601404);
	dvfsrc_write(dvfsrc, DVFSRC_HRT_REQ_MD_BW_5, 0x00D02C09);
	dvfsrc_write(dvfsrc, DVFSRC_HRT_REQ_MD_BW_6, 0x00000012);
	dvfsrc_write(dvfsrc, DVFSRC_HRT_REQ_MD_BW_7, 0x00000024);
	dvfsrc_write(dvfsrc, DVFSRC_HRT_REQ_MD_BW_8, 0x00000000);
	dvfsrc_write(dvfsrc, DVFSRC_HRT_REQ_MD_BW_9, 0x00000000);
	dvfsrc_write(dvfsrc, DVFSRC_HRT_REQ_MD_BW_10, 0x00034800);
	dvfsrc_write(dvfsrc, DVFSRC_HRT1_REQ_MD_BW_0, 0x04B12C4B);
	dvfsrc_write(dvfsrc, DVFSRC_HRT1_REQ_MD_BW_1, 0x04B12C4B);
	dvfsrc_write(dvfsrc, DVFSRC_HRT1_REQ_MD_BW_2, 0x04B12C00);
	dvfsrc_write(dvfsrc, DVFSRC_HRT1_REQ_MD_BW_3, 0x04B12C4B);
	dvfsrc_write(dvfsrc, DVFSRC_HRT1_REQ_MD_BW_4, 0x04B12C4B);
	dvfsrc_write(dvfsrc, DVFSRC_HRT1_REQ_MD_BW_5, 0x04B12C4B);
	dvfsrc_write(dvfsrc, DVFSRC_HRT1_REQ_MD_BW_6, 0x0000004B);
	dvfsrc_write(dvfsrc, DVFSRC_HRT1_REQ_MD_BW_7, 0x0000005C);
	dvfsrc_write(dvfsrc, DVFSRC_HRT1_REQ_MD_BW_8, 0x00000000);
	dvfsrc_write(dvfsrc, DVFSRC_HRT1_REQ_MD_BW_9, 0x00000000);
	dvfsrc_write(dvfsrc, DVFSRC_HRT1_REQ_MD_BW_10, 0x00034800);
	dvfsrc_write(dvfsrc, DVFSRC_95MD_SCEN_BW0_T, 0x40444440);
	dvfsrc_write(dvfsrc, DVFSRC_95MD_SCEN_BW1_T, 0x44444444);
	dvfsrc_write(dvfsrc, DVFSRC_95MD_SCEN_BW2_T, 0x00400444);
	dvfsrc_write(dvfsrc, DVFSRC_95MD_SCEN_BW3_T, 0x60000000);
	dvfsrc_write(dvfsrc, DVFSRC_95MD_SCEN_BW0, 0x20222220);
	dvfsrc_write(dvfsrc, DVFSRC_95MD_SCEN_BW1, 0x22222222);
	dvfsrc_write(dvfsrc, DVFSRC_95MD_SCEN_BW2, 0x00200222);
	dvfsrc_write(dvfsrc, DVFSRC_95MD_SCEN_BW3, 0x60000000);
	dvfsrc_write(dvfsrc, DVFSRC_95MD_SCEN_BW4, 0x00000006);
	/* Setup up hifi request policy */
	dvfsrc_write(dvfsrc, DVFSRC_DDR_REQUEST6, 0x66543210);
	/* Setup up hw request vcore policy */
	dvfsrc_write(dvfsrc, DVFSRC_VCORE_USER_REQ, 0x00010A29);
	/* Setup misc*/
	dvfsrc_write(dvfsrc, DVFSRC_TIMEOUT_NEXTREQ, 0x00000015);
	dvfsrc_write(dvfsrc, DVFSRC_RSRV_5, 0x00000001);
	dvfsrc_write(dvfsrc, DVFSRC_INT_EN, 0x00000002);
	/* Init opp and trigger dvfsrc to run*/
	dvfsrc_write(dvfsrc, DVFSRC_CURRENT_FORCE, 0x00000001);
	dvfsrc_write(dvfsrc, DVFSRC_BASIC_CONTROL, 0x6698444B);
	dvfsrc_write(dvfsrc, DVFSRC_BASIC_CONTROL, 0x6698054B);
	dvfsrc_write(dvfsrc, DVFSRC_CURRENT_FORCE, 0x00000000);
}

static const struct dvfsrc_opp dvfsrc_opp_mt6873_lp4[] = {
	{0, 0}, {1, 0}, {2, 0}, {3, 0},
	{0, 1}, {1, 1}, {2, 1}, {3, 1},
	{0, 2}, {1, 2}, {2, 2}, {3, 2},
	{1, 3}, {2, 3}, {3, 3}, {1, 4},
	{2, 4}, {3, 4}, {2, 5}, {3, 5},
	{3, 6},
};

static const struct dvfsrc_opp_desc dvfsrc_opp_mt6873_desc[] = {
	DVFSRC_OPP_DESC(dvfsrc_opp_mt6873_lp4),
};

static const struct dvfsrc_soc_data mt6873_data = {
	.opps_desc = dvfsrc_opp_mt6873_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6873_desc),
	.regs = mt6873_regs,
	.dvfsrc_hw_init = dvfsrc_init_mt6873,
	.get_target_level = mt6873_get_target_level,
	.get_current_level = mt6873_get_current_level,
	.get_vcore_level = mt6873_get_vcore_level,
	.get_vcp_level = mt6873_get_vcp_level,
	.set_dram_bw = mt6873_set_dram_bw,
	.set_opp_level = mt6873_set_opp_level,
	.set_dram_hrtbw = mt6873_set_dram_hrtbw,
	.set_vcore_level = mt6873_set_vcore_level,
	.set_vscp_level = mt6873_set_vscp_level,
	.wait_for_opp_level = mt6873_wait_for_opp_level,
	.wait_for_vcore_level = dvfsrc_wait_for_vcore_level,
	.wait_for_dram_level = dvfsrc_wait_for_dram_level,
};

static int mtk_dvfsrc_remove(struct platform_device *pdev)
{
	struct mtk_dvfsrc *dvfsrc = platform_get_drvdata(pdev);

	platform_device_unregister(dvfsrc->regulator);
	platform_device_unregister(dvfsrc->icc);

	return 0;
}

static const struct of_device_id mtk_dvfsrc_of_match[] = {
	{
		.compatible = "mediatek,mt8183-dvfsrc",
		.data = &mt8183_data,
	}, {
		.compatible = "mediatek,mt6873-dvfsrc",
		.data = &mt6873_data,
	}, {
		/* sentinel */
	},
};

static struct platform_driver mtk_dvfsrc_driver = {
	.probe	= mtk_dvfsrc_probe,
	.remove	= mtk_dvfsrc_remove,
	.driver = {
		.name = "mtk-dvfsrc",
		.of_match_table = of_match_ptr(mtk_dvfsrc_of_match),
	},
};

static int __init mtk_dvfsrc_init(void)
{
	return platform_driver_register(&mtk_dvfsrc_driver);
}
subsys_initcall(mtk_dvfsrc_init);

static void __exit mtk_dvfsrc_exit(void)
{
	platform_driver_unregister(&mtk_dvfsrc_driver);
}
module_exit(mtk_dvfsrc_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MTK DVFSRC driver");
