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

/* Private */
#define DVFSRC_OPP_BW_QUERY
#define DVFSRC_FORCE_OPP_SUPPORT
#define DVFSRC_DEBUG_ENHANCE
#define DVFSRC_PROPERTY_ENABLE
/* End */

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
	int (*get_target_level)(struct mtk_dvfsrc *dvfsrc);
	int (*get_current_level)(struct mtk_dvfsrc *dvfsrc);
	u32 (*get_vcore_level)(struct mtk_dvfsrc *dvfsrc);
	u32 (*get_vcp_level)(struct mtk_dvfsrc *dvfsrc);
	u32 (*get_dram_level)(struct mtk_dvfsrc *dvfsrc);
	void (*set_dram_bw)(struct mtk_dvfsrc *dvfsrc, u64 bw);
	void (*set_dram_peak_bw)(struct mtk_dvfsrc *dvfsrc, u64 bw);
	void (*set_dram_hrtbw)(struct mtk_dvfsrc *dvfsrc, u64 bw);
	void (*set_dram_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	void (*set_opp_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	void (*set_vcore_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	void (*set_vscp_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	int (*wait_for_opp_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	int (*wait_for_vcore_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	int (*wait_for_dram_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
#ifdef DVFSRC_FORCE_OPP_SUPPORT
	void (*set_force_opp_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
#endif
};

struct mtk_dvfsrc {
	struct device *dev;
	struct platform_device *dvfsrc_start;
	struct platform_device *devfreq;
	struct platform_device *regulator;
	struct platform_device *icc;
	const struct dvfsrc_soc_data *dvd;
	int dram_type;
	const struct dvfsrc_opp_desc *curr_opps;
	void __iomem *regs;
	spinlock_t req_lock;
	struct mutex pstate_lock;
	struct notifier_block scpsys_notifier;
	bool dvfsrc_enable;
#ifdef DVFSRC_FORCE_OPP_SUPPORT
	bool opp_forced;
	spinlock_t force_lock;
#endif
};

#ifdef DVFSRC_OPP_BW_QUERY
u32 dram_type;
static inline struct device_node *dvfsrc_parse_required_opp(
	struct device_node *np, int index)
{
	struct device_node *required_np;

	required_np = of_parse_phandle(np, "required-opps", index);
	if (unlikely(!required_np)) {
		pr_notice("%s: Unable to parse required-opps: %pOF, index: %d\n",
		       __func__, np, index);
	}
	return required_np;
}
u32 dvfsrc_get_required_opp_peak_bw(struct device_node *np, int index)
{
	struct device_node *required_np;
	u32 peak_bw = 0;

	required_np = dvfsrc_parse_required_opp(np, index);
	if (!required_np)
		return 0;

	if (of_property_read_u32_index(required_np, "opp-peak-KBps", dram_type, &peak_bw))
		pr_info("%s: get fail\n", __func__);

	of_node_put(required_np);
	return peak_bw;
}
EXPORT_SYMBOL(dvfsrc_get_required_opp_peak_bw);
#endif

#ifdef DVFSRC_DEBUG_ENHANCE
#define DVFSRC_DEBUG_DUMP 0
#define DVFSRC_DEBUG_AEE 1
#define DVFSRC_DEBUG_VCORE_CHK 2

#define DVFSRC_AEE_LEVEL_ERROR 0
#define DVFSRC_AEE_FORCE_ERROR 1
#define DVFSRC_AEE_VCORE_CHK_ERROR 2

static BLOCKING_NOTIFIER_HEAD(dvfsrc_debug_notifier);
int register_dvfsrc_debug_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&dvfsrc_debug_notifier, nb);
}
EXPORT_SYMBOL_GPL(register_dvfsrc_debug_notifier);

int unregister_dvfsrc_debug_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&dvfsrc_debug_notifier, nb);
}
EXPORT_SYMBOL_GPL(unregister_dvfsrc_debug_notifier);

static void mtk_dvfsrc_dump_notify(struct mtk_dvfsrc *dvfsrc, u32 flag)
{
	blocking_notifier_call_chain(&dvfsrc_debug_notifier,
			DVFSRC_DEBUG_DUMP, (void *) &flag);

}
static void mtk_dvfsrc_aee_notify(struct mtk_dvfsrc *dvfsrc, u32 aee_type)
{
	blocking_notifier_call_chain(&dvfsrc_debug_notifier,
			DVFSRC_DEBUG_AEE, (void *) &aee_type);

}
static void mtk_dvfsrc_vcore_check(struct mtk_dvfsrc *dvfsrc, u32 vcore_level)
{
	int ret;

	ret = blocking_notifier_call_chain(&dvfsrc_debug_notifier,
		DVFSRC_DEBUG_VCORE_CHK, (void *) &vcore_level);

	if (ret == NOTIFY_BAD) {
		dev_info(dvfsrc->dev,
			"VCORE_ERROR= %d, %d 0x%08x\n",
			vcore_level,
			dvfsrc->dvd->get_current_level(dvfsrc),
			dvfsrc->dvd->get_target_level(dvfsrc));
		mtk_dvfsrc_dump_notify(dvfsrc, 0);
		mtk_dvfsrc_aee_notify(dvfsrc, DVFSRC_AEE_VCORE_CHK_ERROR);
	}
}

#endif

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
	DVFSRC_SW_PEAK_BW,
	DVFSRC_SW_HRT_BW,
	DVFSRC_VCORE_REQUEST,
	DVFSRC_LAST,
	DVFSRC_TARGET_FORCE,
	DVFSRC_FORCE_MASK,
	DVFSRC_TARGET_FORCE_H,
	DVFSRC_SW_FORCE_BW,
	DVFSRC_EXT_SW_REQ,
	DVFSRC_SW_EXT_BW,
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
	[DVFSRC_SW_PEAK_BW] =		0x278,
	[DVFSRC_SW_BW] =		0x26C,
	[DVFSRC_SW_HRT_BW] =		0x290,
	[DVFSRC_TARGET_LEVEL] =		0xD48,
	[DVFSRC_VCORE_REQUEST] =	0x6C,
	[DVFSRC_TARGET_FORCE] =		0xD70,
};

static const int mt6983_regs[] = {
	[DVFSRC_BASIC_CONTROL] =	0x0,
	[DVFSRC_SW_REQ] =		0x18,
	[DVFSRC_LEVEL] =		0x5F0,
	[DVFSRC_SW_PEAK_BW] =		0x1F4,
	[DVFSRC_SW_BW] =		0x1E8,
	[DVFSRC_SW_HRT_BW] =		0x20C,
	[DVFSRC_TARGET_LEVEL] =		0x5F0,
	[DVFSRC_VCORE_REQUEST] =	0x80,
	[DVFSRC_TARGET_FORCE] =		0x5E0,
	[DVFSRC_TARGET_FORCE_H] =	0x5DC,
	[DVFSRC_FORCE_MASK] =		0x5EC,
	[DVFSRC_SW_FORCE_BW] =		0x200,
};

static const int mt6768_regs[] = {
	[DVFSRC_SW_REQ] =			0x4,
	[DVFSRC_LEVEL] =				0xDC,
	[DVFSRC_SW_BW] =			0x16C,
	[DVFSRC_SW_PEAK_BW] =		0x160,
	[DVFSRC_VCORE_REQUEST] =	0x48,
	[DVFSRC_BASIC_CONTROL] =	0x0,
	[DVFSRC_TARGET_FORCE] =		0x300,
};

static const int mt6765_regs[] = {
	[DVFSRC_SW_REQ] =			0x4,
	[DVFSRC_LEVEL] =				0xDC,
	[DVFSRC_SW_BW] =			0x16C,
	[DVFSRC_SW_PEAK_BW] =		0x160,
	[DVFSRC_VCORE_REQUEST] =	0x48,
	[DVFSRC_BASIC_CONTROL] =	0x0,
	[DVFSRC_TARGET_FORCE] =		0x300,
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

static u32 mt6873_get_dram_level(struct mtk_dvfsrc *dvfsrc)
{
	return (dvfsrc_read(dvfsrc, DVFSRC_SW_REQ) >> 12) & 0x7;
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

static void mt6873_set_dram_peak_bw(struct mtk_dvfsrc *dvfsrc, u64 bw)
{
	bw = div_u64(kbps_to_mbps(bw), 100);
	bw = min_t(u64, bw, 0xFF);
	dvfsrc_write(dvfsrc, DVFSRC_SW_PEAK_BW, bw);
}

static void mt6873_set_dram_hrtbw(struct mtk_dvfsrc *dvfsrc, u64 bw)
{
	bw = div_u64((kbps_to_mbps(bw) + 29), 30);
	bw = min_t(u64, bw, 0x3FF);
	dvfsrc_write(dvfsrc, DVFSRC_SW_HRT_BW, bw);
}

static void mt6873_set_dram_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	spin_lock(&dvfsrc->req_lock);
	dvfsrc_rmw(dvfsrc, DVFSRC_SW_REQ, level, 0x7, 12);
	spin_unlock(&dvfsrc->req_lock);
}

static void mt6873_set_vcore_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	spin_lock(&dvfsrc->req_lock);
	dvfsrc_rmw(dvfsrc, DVFSRC_SW_REQ, level, 0x7, 4);
	spin_unlock(&dvfsrc->req_lock);
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

#ifdef DVFSRC_FORCE_OPP_SUPPORT
static void mt6873_set_force_opp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	unsigned long flags;
	int val;
	int ret = 0;

	spin_lock_irqsave(&dvfsrc->force_lock, flags);
	dvfsrc->opp_forced = true;
	if (level > dvfsrc->curr_opps->num_opp - 1) {
		dvfsrc_rmw(dvfsrc, DVFSRC_BASIC_CONTROL, 0, 0x1, 15);
		dvfsrc_write(dvfsrc, DVFSRC_TARGET_FORCE, 0);
		dvfsrc->opp_forced = false;
		goto out;
	}
	dvfsrc_write(dvfsrc, DVFSRC_TARGET_FORCE, 1 << level);
	dvfsrc_rmw(dvfsrc, DVFSRC_BASIC_CONTROL, 1, 0x1, 15);
	ret = readl_poll_timeout_atomic(
			dvfsrc->regs + dvfsrc->dvd->regs[DVFSRC_LEVEL],
			val, val == (1 << level), STARTUP_TIME, POLL_TIMEOUT);
	dvfsrc_write(dvfsrc, DVFSRC_TARGET_FORCE, 0);
out:
	spin_unlock_irqrestore(&dvfsrc->force_lock, flags);
	if (ret < 0) {
		dev_info(dvfsrc->dev,
			"[%s] wait idle, level: %d, last: %d -> %x\n",
			__func__, level,
			dvfsrc->dvd->get_current_level(dvfsrc),
			dvfsrc->dvd->get_target_level(dvfsrc));
#ifdef DVFSRC_DEBUG_ENHANCE
		mtk_dvfsrc_dump_notify(dvfsrc, 0);
		mtk_dvfsrc_aee_notify(dvfsrc, DVFSRC_AEE_FORCE_ERROR);
#endif
	}
}
#endif



static int mt6983_get_target_level(struct mtk_dvfsrc *dvfsrc)
{
	u32 reg;

	reg = dvfsrc_read(dvfsrc, DVFSRC_TARGET_LEVEL);
	if (reg & (1 << 16))
		return ((reg >> 8) & 0x3f) + 1;
	else
		return 0;
}

static int mt6983_get_current_level(struct mtk_dvfsrc *dvfsrc)
{
	u32 curr_level;

	curr_level = (dvfsrc_read(dvfsrc, DVFSRC_LEVEL) & 0x3f) + 1;
	if (curr_level > dvfsrc->curr_opps->num_opp)
		curr_level = 0;
	else
		curr_level = dvfsrc->curr_opps->num_opp - curr_level;

	return curr_level;
}

static u32 mt6983_get_dram_level(struct mtk_dvfsrc *dvfsrc)
{
	return (dvfsrc_read(dvfsrc, DVFSRC_SW_REQ) >> 12) & 0xf;
}

static void mt6983_set_dram_bw(struct mtk_dvfsrc *dvfsrc, u64 bw)
{
	bw = div_u64(kbps_to_mbps(bw), 100);
	bw = min_t(u64, bw, 0x3FF);
	dvfsrc_write(dvfsrc, DVFSRC_SW_BW, bw);
}

static void mt6983_set_dram_peak_bw(struct mtk_dvfsrc *dvfsrc, u64 bw)
{
	bw = div_u64(kbps_to_mbps(bw), 100);
	bw = min_t(u64, bw, 0x3FF);
	dvfsrc_write(dvfsrc, DVFSRC_SW_PEAK_BW, bw);
}

static void mt6983_set_dram_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	spin_lock(&dvfsrc->req_lock);
	dvfsrc_rmw(dvfsrc, DVFSRC_SW_REQ, level, 0xf, 12);
	spin_unlock(&dvfsrc->req_lock);
}

static void mt6983_set_opp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	const struct dvfsrc_opp *opp;

	opp = &dvfsrc->curr_opps->opps[level];
	mt6983_set_dram_level(dvfsrc, opp->dram_opp);
}

#ifdef DVFSRC_FORCE_OPP_SUPPORT
static void mt6983_set_force_opp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	unsigned long flags;
	int val;
	int ret = 0;

	spin_lock_irqsave(&dvfsrc->force_lock, flags);
	if (level > dvfsrc->curr_opps->num_opp - 1) {
		dvfsrc_write(dvfsrc, DVFSRC_SW_FORCE_BW, 0x0);
		dvfsrc_rmw(dvfsrc, DVFSRC_BASIC_CONTROL, 0, 0x1, 14);
		dvfsrc_write(dvfsrc, DVFSRC_TARGET_FORCE, 0);
		dvfsrc_write(dvfsrc, DVFSRC_TARGET_FORCE_H, 0);
		dvfsrc->opp_forced = false;
		goto out;
	}

	if (!dvfsrc->opp_forced) {
		dvfsrc_write(dvfsrc, DVFSRC_SW_FORCE_BW, 0x3FF);
		udelay(STARTUP_TIME);
		dvfsrc_wait_for_idle(dvfsrc);
		udelay(STARTUP_TIME);
		dvfsrc_wait_for_idle(dvfsrc);
	}

	dvfsrc->opp_forced = true;

	if (level >= 32) {
		dvfsrc_write(dvfsrc, DVFSRC_TARGET_FORCE, 0);
		dvfsrc_write(dvfsrc, DVFSRC_TARGET_FORCE_H, 1 << (level - 32));
	} else {
		dvfsrc_write(dvfsrc, DVFSRC_TARGET_FORCE, 1 << level);
		dvfsrc_write(dvfsrc, DVFSRC_TARGET_FORCE_H, 0);
	}
	dvfsrc_rmw(dvfsrc, DVFSRC_FORCE_MASK, 0, 0x1, 1);
	dvfsrc_rmw(dvfsrc, DVFSRC_BASIC_CONTROL, 1, 0x1, 14);
	ret = readl_poll_timeout_atomic(
			dvfsrc->regs + dvfsrc->dvd->regs[DVFSRC_LEVEL],
			val, (val & 0x3f) == level, STARTUP_TIME, POLL_TIMEOUT);
	dvfsrc_rmw(dvfsrc, DVFSRC_FORCE_MASK, 1, 0x1, 1);
out:
	spin_unlock_irqrestore(&dvfsrc->force_lock, flags);
	if (ret < 0) {
		dev_info(dvfsrc->dev,
			"[%s] wait idle, level: %d, last: %d -> %x\n",
			__func__, level,
			dvfsrc->dvd->get_current_level(dvfsrc),
			dvfsrc->dvd->get_target_level(dvfsrc));
#ifdef DVFSRC_DEBUG_ENHANCE
		mtk_dvfsrc_dump_notify(dvfsrc, 0);
		mtk_dvfsrc_aee_notify(dvfsrc, DVFSRC_AEE_FORCE_ERROR);
#endif
	}
}
#endif


static int mt6768_get_target_level(struct mtk_dvfsrc *dvfsrc)
{
	return DVFSRC_GET_TARGET_LEVEL(dvfsrc_read(dvfsrc, DVFSRC_LEVEL));
}

static int mt6768_get_current_level(struct mtk_dvfsrc *dvfsrc)
{
	u32 curr_level;

	curr_level = dvfsrc_read(dvfsrc, DVFSRC_LEVEL);
	curr_level = ffs(DVFSRC_GET_CURRENT_LEVEL(curr_level));

	if ((curr_level > 0) && (curr_level <= dvfsrc->curr_opps->num_opp))
		return curr_level - 1;
	else
		return 0;
}

static u32 mt6768_get_vcore_level(struct mtk_dvfsrc *dvfsrc)
{
	return (dvfsrc_read(dvfsrc, DVFSRC_SW_REQ) >> 2) & 0x3;
}

static u32 mt6768_get_dram_level(struct mtk_dvfsrc *dvfsrc)
{
	return (dvfsrc_read(dvfsrc, DVFSRC_SW_REQ) >> 0) & 0x3;
}

static u32 mt6768_get_vcp_level(struct mtk_dvfsrc *dvfsrc)
{
	return (dvfsrc_read(dvfsrc, DVFSRC_VCORE_REQUEST) >> 30) & 0x3;
}

static void mt6768_set_dram_peak_bw(struct mtk_dvfsrc *dvfsrc, u64 bw)
{
	bw = div_u64(kbps_to_mbps(bw), 100);
	bw = min_t(u64, bw, 0xFF);
	dvfsrc_write(dvfsrc, DVFSRC_SW_PEAK_BW, bw);
}

static void mt6768_set_dram_bw(struct mtk_dvfsrc *dvfsrc, u64 bw)
{
	bw = div_u64(kbps_to_mbps(bw), 100);
	bw = (bw < 0xFF) ? bw : 0xff;

	dvfsrc_write(dvfsrc, DVFSRC_SW_BW, bw);
}

static void mt6768_set_dram_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	spin_lock(&dvfsrc->req_lock);
	dvfsrc_rmw(dvfsrc, DVFSRC_SW_REQ, level, 0x3, 0);
	spin_unlock(&dvfsrc->req_lock);
}

static void mt6768_set_vcore_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	spin_lock(&dvfsrc->req_lock);
	dvfsrc_rmw(dvfsrc, DVFSRC_SW_REQ, level, 0x3, 2);
	spin_unlock(&dvfsrc->req_lock);
}

static void mt6768_set_vscp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	dvfsrc_rmw(dvfsrc, DVFSRC_VCORE_REQUEST, level, 0x3, 30);
}

static void mt6768_set_opp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	const struct dvfsrc_opp *opp;

	opp = &dvfsrc->curr_opps->opps[level];
	mt6768_set_dram_level(dvfsrc, opp->dram_opp);
}

#ifdef DVFSRC_FORCE_OPP_SUPPORT

static void mt6768_set_force_opp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	unsigned long flags;
	int val;
	int ret = 0;

	spin_lock_irqsave(&dvfsrc->force_lock, flags);
	dvfsrc->opp_forced = true;
	if (level > dvfsrc->curr_opps->num_opp - 1) {
		dvfsrc_rmw(dvfsrc, DVFSRC_BASIC_CONTROL, 0, 0x1, 15);
		dvfsrc_write(dvfsrc, DVFSRC_TARGET_FORCE, 0);
		dvfsrc->opp_forced = false;
		goto out;
	}

	level = dvfsrc->curr_opps->num_opp - 1 - level;
	dvfsrc_write(dvfsrc, DVFSRC_TARGET_FORCE, 1 << level);
	dvfsrc_rmw(dvfsrc, DVFSRC_BASIC_CONTROL, 1, 0x1, 15);
	ret = readl_poll_timeout_atomic(
		dvfsrc->regs + dvfsrc->dvd->regs[DVFSRC_LEVEL],
			val, DVFSRC_GET_CURRENT_LEVEL(val) == (1 << level),
			STARTUP_TIME, POLL_TIMEOUT);
	dvfsrc_write(dvfsrc, DVFSRC_TARGET_FORCE, 0);
out:
	spin_unlock_irqrestore(&dvfsrc->force_lock, flags);
	if (ret < 0) {
		dev_info(dvfsrc->dev,
			"[%s] wait idle, level: %d, last: %d -> %x\n",
			__func__, level,
			dvfsrc->dvd->get_current_level(dvfsrc),
			dvfsrc->dvd->get_target_level(dvfsrc));
#ifdef DVFSRC_DEBUG_ENHANCE
		mtk_dvfsrc_dump_notify(dvfsrc, 0);
		mtk_dvfsrc_aee_notify(dvfsrc, DVFSRC_AEE_FORCE_ERROR);
#endif
	}
}
#endif

static int mt6765_get_target_level(struct mtk_dvfsrc *dvfsrc)
{
	return DVFSRC_GET_TARGET_LEVEL(dvfsrc_read(dvfsrc, DVFSRC_LEVEL));
}

static int mt6765_get_current_level(struct mtk_dvfsrc *dvfsrc)
{
	u32 curr_level;

	curr_level = dvfsrc_read(dvfsrc, DVFSRC_LEVEL);
	curr_level = ffs(DVFSRC_GET_CURRENT_LEVEL(curr_level));

	if ((curr_level > 0) && (curr_level <= dvfsrc->curr_opps->num_opp))
		return curr_level - 1;
	else
		return 0;
}

static u32 mt6765_get_vcore_level(struct mtk_dvfsrc *dvfsrc)
{
	return (dvfsrc_read(dvfsrc, DVFSRC_SW_REQ) >> 2) & 0x3;
}

static u32 mt6765_get_dram_level(struct mtk_dvfsrc *dvfsrc)
{
	return (dvfsrc_read(dvfsrc, DVFSRC_SW_REQ) >> 0) & 0x3;
}

static u32 mt6765_get_vcp_level(struct mtk_dvfsrc *dvfsrc)
{
	return (dvfsrc_read(dvfsrc, DVFSRC_VCORE_REQUEST) >> 30) & 0x3;
}

static void mt6765_set_dram_peak_bw(struct mtk_dvfsrc *dvfsrc, u64 bw)
{
	bw = div_u64(kbps_to_mbps(bw), 100);
	bw = min_t(u64, bw, 0xFF);
	dvfsrc_write(dvfsrc, DVFSRC_SW_PEAK_BW, bw);
}

static void mt6765_set_dram_bw(struct mtk_dvfsrc *dvfsrc, u64 bw)
{
	bw = div_u64(kbps_to_mbps(bw), 100);
	bw = (bw < 0xFF) ? bw : 0xff;

	dvfsrc_write(dvfsrc, DVFSRC_SW_BW, bw);
}

static void mt6765_set_dram_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	spin_lock(&dvfsrc->req_lock);
	dvfsrc_rmw(dvfsrc, DVFSRC_SW_REQ, level, 0x3, 0);
	spin_unlock(&dvfsrc->req_lock);
}

static void mt6765_set_vcore_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	spin_lock(&dvfsrc->req_lock);
	dvfsrc_rmw(dvfsrc, DVFSRC_SW_REQ, level, 0x3, 2);
	spin_unlock(&dvfsrc->req_lock);
}

static void mt6765_set_vscp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	dvfsrc_rmw(dvfsrc, DVFSRC_VCORE_REQUEST, level, 0x3, 30);
}

static void mt6765_set_opp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	const struct dvfsrc_opp *opp;

	opp = &dvfsrc->curr_opps->opps[level];
	mt6765_set_dram_level(dvfsrc, opp->dram_opp);
}

#ifdef DVFSRC_FORCE_OPP_SUPPORT

static void mt6765_set_force_opp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	unsigned long flags;
	int val;
	int ret = 0;

	spin_lock_irqsave(&dvfsrc->force_lock, flags);
	dvfsrc->opp_forced = true;
	if (level > dvfsrc->curr_opps->num_opp - 1) {
		dvfsrc_rmw(dvfsrc, DVFSRC_BASIC_CONTROL, 0, 0x1, 15);
		dvfsrc_write(dvfsrc, DVFSRC_TARGET_FORCE, 0);
		dvfsrc->opp_forced = false;
		goto out;
	}

	level = dvfsrc->curr_opps->num_opp - 1 - level;
	dvfsrc_write(dvfsrc, DVFSRC_TARGET_FORCE, 1 << level);
	dvfsrc_rmw(dvfsrc, DVFSRC_BASIC_CONTROL, 1, 0x1, 15);
	ret = readl_poll_timeout_atomic(
		dvfsrc->regs + dvfsrc->dvd->regs[DVFSRC_LEVEL],
			val, DVFSRC_GET_CURRENT_LEVEL(val) == (1 << level),
			STARTUP_TIME, POLL_TIMEOUT);
	dvfsrc_write(dvfsrc, DVFSRC_TARGET_FORCE, 0);
out:
	spin_unlock_irqrestore(&dvfsrc->force_lock, flags);
	if (ret < 0) {
		dev_info(dvfsrc->dev,
			"[%s] wait idle, level: %d, last: %d -> %x\n",
			__func__, level,
			dvfsrc->dvd->get_current_level(dvfsrc),
			dvfsrc->dvd->get_target_level(dvfsrc));
#ifdef DVFSRC_DEBUG_ENHANCE
		mtk_dvfsrc_dump_notify(dvfsrc, 0);
		mtk_dvfsrc_aee_notify(dvfsrc, DVFSRC_AEE_FORCE_ERROR);
#endif
	}
}
#endif


void mtk_dvfsrc_send_request(const struct device *dev, u32 cmd, u64 data)
{
	int ret = 0;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	switch (cmd) {
	case MTK_DVFSRC_CMD_BW_REQUEST:
		dvfsrc->dvd->set_dram_bw(dvfsrc, data);
		goto out;
	case MTK_DVFSRC_CMD_PEAK_BW_REQUEST:
		if (dvfsrc->dvd->set_dram_peak_bw)
			dvfsrc->dvd->set_dram_peak_bw(dvfsrc, data);
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
#ifdef DVFSRC_FORCE_OPP_SUPPORT
	case MTK_DVFSRC_CMD_FORCEOPP_REQUEST:
		if ((dvfsrc->dvd->set_force_opp_level) && dvfsrc->dvfsrc_enable)
			dvfsrc->dvd->set_force_opp_level(dvfsrc, data);
		goto out;
#endif
	default:
		dev_err(dvfsrc->dev, "unknown command: %d\n", cmd);
		goto out;
	}

	if (!dvfsrc->dvfsrc_enable)
		return;

#ifdef DVFSRC_FORCE_OPP_SUPPORT
	if (dvfsrc->opp_forced)
		return;
#endif
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
#ifdef DVFSRC_DEBUG_ENHANCE
		mtk_dvfsrc_vcore_check(dvfsrc, data);
#endif
		break;
	case MTK_DVFSRC_CMD_DRAM_REQUEST:
		ret = dvfsrc->dvd->wait_for_dram_level(dvfsrc, data);
		break;
	}
out:
#ifdef DVFSRC_FORCE_OPP_SUPPORT
	if (dvfsrc->opp_forced)
		return;
#endif

	if (ret < 0) {
		dev_warn(dvfsrc->dev,
			 "%d: idle timeout, data: %llu, last: %d -> %d\n",
			 cmd, data,
			 dvfsrc->dvd->get_current_level(dvfsrc),
			 dvfsrc->dvd->get_target_level(dvfsrc));
#ifdef DVFSRC_DEBUG_ENHANCE
		mtk_dvfsrc_dump_notify(dvfsrc, 0);
		mtk_dvfsrc_aee_notify(dvfsrc, DVFSRC_AEE_LEVEL_ERROR);
#endif
	}
}
EXPORT_SYMBOL(mtk_dvfsrc_send_request);

int mtk_dvfsrc_query_info(const struct device *dev, u32 cmd, int *data)
{
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	switch (cmd) {
	case MTK_DVFSRC_CMD_VCORE_LEVEL_QUERY:
		*data = dvfsrc->dvd->get_vcore_level(dvfsrc);
		break;
	case MTK_DVFSRC_CMD_VSCP_LEVEL_QUERY:
		*data = dvfsrc->dvd->get_vcp_level(dvfsrc);
		break;
	case MTK_DVFSRC_CMD_DRAM_LEVEL_QUERY:
		*data = dvfsrc->dvd->get_dram_level(dvfsrc);
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
#ifdef DVFSRC_PROPERTY_ENABLE
	u32 is_bringup = 0;
	u32 dvfsrc_flag = 0;
	u32 dvfsrc_vmode = 0;
	struct device_node *np = pdev->dev.of_node;
#endif

	dvfsrc = devm_kzalloc(&pdev->dev, sizeof(*dvfsrc), GFP_KERNEL);
	if (!dvfsrc)
		return -ENOMEM;

	dvfsrc->dvd = of_device_get_match_data(&pdev->dev);
	dvfsrc->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	dvfsrc->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(dvfsrc->regs))
		return PTR_ERR(dvfsrc->regs);

	spin_lock_init(&dvfsrc->req_lock);
	mutex_init(&dvfsrc->pstate_lock);
#ifdef DVFSRC_FORCE_OPP_SUPPORT
	spin_lock_init(&dvfsrc->force_lock);
#endif

#ifdef DVFSRC_PROPERTY_ENABLE
	of_property_read_u32(np, "dvfsrc,bringup", &is_bringup);
	of_property_read_u32(np, "dvfsrc_flag", &dvfsrc_flag);
	of_property_read_u32(np, "dvfsrc_vmode", &dvfsrc_vmode);

	if (!is_bringup) {
		arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, MTK_SIP_DVFSRC_INIT,
			      dvfsrc_flag, dvfsrc_vmode, 0, 0, 0, 0, &ares);
		if (!ares.a0) {
			dvfsrc->dram_type = ares.a1;
			dvfsrc->dvfsrc_enable = true;
		} else
			dev_info(dvfsrc->dev, "dvfs mode is disabled\n");
	} else
		dev_info(dvfsrc->dev, "dvfs mode is bringup mode\n");
#else
	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, MTK_SIP_DVFSRC_INIT, 0, 0, 0,
		0, 0, 0, &ares);

	if (!ares.a0) {
		dvfsrc->dram_type = ares.a1;
		dvfsrc->dvfsrc_enable = true;
	} else
		dev_info(dvfsrc->dev, "dvfs mode is disabled\n");
#endif

#ifdef DVFSRC_OPP_BW_QUERY
	dram_type = dvfsrc->dram_type;
#endif
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

	dvfsrc->devfreq = platform_device_register_data(dvfsrc->dev,
			"mtk-dvfsrc-devfreq", -1, NULL, 0);
	if (IS_ERR(dvfsrc->devfreq)) {
		dev_err(dvfsrc->dev, "Failed create devfreq device\n");
		ret = PTR_ERR(dvfsrc->devfreq);
		goto unregister_icc;
	}

	dvfsrc->dvfsrc_start = platform_device_register_data(dvfsrc->dev,
			"mtk-dvfsrc-start", -1, NULL, 0);
	if (IS_ERR(dvfsrc->dvfsrc_start)) {
		dev_err(dvfsrc->dev, "Failed create dvfsrc-start device\n");
		ret = PTR_ERR(dvfsrc->dvfsrc_start);
		goto unregister_devfreq;
	}

	ret = devm_of_platform_populate(dvfsrc->dev);
	if (ret < 0)
		goto unregister_start;

	return 0;

unregister_start:
	platform_device_unregister(dvfsrc->dvfsrc_start);
unregister_devfreq:
	platform_device_unregister(dvfsrc->devfreq);
unregister_icc:
	platform_device_unregister(dvfsrc->icc);
unregister_regulator:
	platform_device_unregister(dvfsrc->regulator);
err:
	return ret;
}

#define DVFSRC_MT6873_SERIES_OPS			\
	.get_target_level = mt6873_get_target_level,	\
	.get_current_level = mt6873_get_current_level,	\
	.get_vcore_level = mt6873_get_vcore_level,	\
	.get_vcp_level = mt6873_get_vcp_level,		\
	.get_dram_level = mt6873_get_dram_level,	\
	.set_dram_bw = mt6873_set_dram_bw,		\
	.set_dram_peak_bw = mt6873_set_dram_peak_bw,	\
	.set_opp_level = mt6873_set_opp_level,		\
	.set_dram_level = mt6873_set_dram_level,	\
	.set_dram_hrtbw = mt6873_set_dram_hrtbw,	\
	.set_vcore_level = mt6873_set_vcore_level,	\
	.set_vscp_level = mt6873_set_vscp_level,	\
	.wait_for_opp_level = mt6873_wait_for_opp_level,	\
	.wait_for_vcore_level = dvfsrc_wait_for_vcore_level,	\
	.wait_for_dram_level = dvfsrc_wait_for_dram_level

#define DVFSRC_MT6983_SERIES_OPS			\
	.get_target_level = mt6983_get_target_level,	\
	.get_current_level = mt6983_get_current_level,	\
	.get_vcore_level = mt6873_get_vcore_level,	\
	.get_vcp_level = mt6873_get_vcp_level,		\
	.get_dram_level = mt6983_get_dram_level,	\
	.set_dram_bw = mt6983_set_dram_bw,		\
	.set_dram_peak_bw = mt6983_set_dram_peak_bw,	\
	.set_opp_level = mt6983_set_opp_level,		\
	.set_dram_level = mt6983_set_dram_level,	\
	.set_dram_hrtbw = mt6873_set_dram_hrtbw,	\
	.set_vcore_level = mt6873_set_vcore_level,	\
	.set_vscp_level = mt6873_set_vscp_level,	\
	.wait_for_opp_level = mt6873_wait_for_opp_level,	\
	.wait_for_vcore_level = dvfsrc_wait_for_vcore_level,	\
	.wait_for_dram_level = dvfsrc_wait_for_dram_level

#define DVFSRC_MT6768_SERIES_OPS			\
	.get_target_level = mt6768_get_target_level,	\
	.get_current_level = mt6768_get_current_level,	\
	.get_vcore_level = mt6768_get_vcore_level,	\
	.get_vcp_level = mt6768_get_vcp_level,		\
	.get_dram_level = mt6768_get_dram_level,	\
	.set_dram_bw = mt6768_set_dram_bw,		\
	.set_dram_peak_bw = mt6768_set_dram_peak_bw,	\
	.set_opp_level = mt6768_set_opp_level,		\
	.set_dram_level = mt6768_set_dram_level,	\
	.set_vcore_level = mt6768_set_vcore_level,	\
	.set_vscp_level = mt6768_set_vscp_level,	\
	.wait_for_opp_level = mt6873_wait_for_opp_level,	\
	.wait_for_vcore_level = dvfsrc_wait_for_vcore_level,    \
	.wait_for_dram_level = dvfsrc_wait_for_dram_level


#define DVFSRC_MT6765_SERIES_OPS			\
	.get_target_level = mt6765_get_target_level,	\
	.get_current_level = mt6765_get_current_level,	\
	.get_vcore_level = mt6765_get_vcore_level,	\
	.get_vcp_level = mt6765_get_vcp_level,		\
	.get_dram_level = mt6765_get_dram_level,	\
	.set_dram_bw = mt6765_set_dram_bw,		\
	.set_dram_peak_bw = mt6765_set_dram_peak_bw,	\
	.set_opp_level = mt6765_set_opp_level,		\
	.set_dram_level = mt6765_set_dram_level,	\
	.set_vcore_level = mt6765_set_vcore_level,	\
	.set_vscp_level = mt6765_set_vscp_level,	\
	.wait_for_opp_level = mt6873_wait_for_opp_level,	\
	.wait_for_vcore_level = dvfsrc_wait_for_vcore_level,    \
	.wait_for_dram_level = dvfsrc_wait_for_dram_level


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
	DVFSRC_MT6873_SERIES_OPS,
	.opps_desc = dvfsrc_opp_mt6873_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6873_desc),
	.regs = mt6873_regs,
#ifdef DVFSRC_FORCE_OPP_SUPPORT
	.set_force_opp_level = mt6873_set_force_opp_level,
#endif
};

static const struct dvfsrc_opp dvfsrc_opp_mt6885_lp4[] = {
	{0, 0}, {1, 0}, {2, 0}, {3, 0},
	{0, 1}, {1, 1}, {2, 1}, {3, 1},
	{0, 2}, {1, 2}, {2, 2}, {3, 2},
	{0, 3}, {1, 3}, {2, 3}, {3, 3},
	{1, 4}, {2, 4}, {3, 4}, {2, 5},
	{3, 5}, {3, 6},
};

static const struct dvfsrc_opp_desc dvfsrc_opp_mt6885_desc[] = {
	DVFSRC_OPP_DESC(dvfsrc_opp_mt6885_lp4),
};

static const struct dvfsrc_soc_data mt6885_data = {
	DVFSRC_MT6873_SERIES_OPS,
	.opps_desc = dvfsrc_opp_mt6885_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6885_desc),
	.regs = mt6873_regs,
#ifdef DVFSRC_FORCE_OPP_SUPPORT
	.set_force_opp_level = mt6873_set_force_opp_level,
#endif
};

static const struct dvfsrc_opp dvfsrc_opp_mt6893_lp4[] = {
	{0, 0}, {1, 0}, {2, 0}, {3, 0},
	{0, 1}, {1, 1}, {2, 1}, {3, 1},
	{0, 2}, {1, 2}, {2, 2}, {3, 2},
	{0, 3}, {1, 3}, {2, 3}, {3, 3},
	{1, 4}, {2, 4}, {3, 4}, {2, 5},
	{3, 5}, {3, 6}, {4, 6}, {4, 7},
};

static const struct dvfsrc_opp_desc dvfsrc_opp_mt6893_desc[] = {
	DVFSRC_OPP_DESC(dvfsrc_opp_mt6893_lp4),
};

static const struct dvfsrc_soc_data mt6893_data = {
	DVFSRC_MT6873_SERIES_OPS,
	.opps_desc = dvfsrc_opp_mt6893_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6893_desc),
	.regs = mt6873_regs,
#ifdef DVFSRC_FORCE_OPP_SUPPORT
	.set_force_opp_level = mt6873_set_force_opp_level,
#endif
};

static const struct dvfsrc_opp dvfsrc_opp_mt6833_lp4[] = {
	{0, 0}, {1, 0}, {2, 0}, {3, 0},
	{0, 1}, {1, 1}, {2, 1}, {3, 1},
	{0, 2}, {1, 2}, {2, 2}, {3, 2},
	{0, 3}, {1, 3}, {2, 3}, {3, 3},
	{1, 4}, {2, 4}, {3, 4}, {1, 5},
	{2, 5}, {3, 5}, {2, 6}, {3, 6},
	{3, 7},
};

static const struct dvfsrc_opp_desc dvfsrc_opp_mt6833_desc[] = {
	DVFSRC_OPP_DESC(dvfsrc_opp_mt6833_lp4),
};

static const struct dvfsrc_soc_data mt6833_data = {
	DVFSRC_MT6873_SERIES_OPS,
	.opps_desc = dvfsrc_opp_mt6833_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6833_desc),
	.regs = mt6873_regs,
#ifdef DVFSRC_FORCE_OPP_SUPPORT
	.set_force_opp_level = mt6873_set_force_opp_level,
#endif
};

static const struct dvfsrc_opp dvfsrc_opp_mt6877_lp4[] = {
	{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0},
	{0, 1}, {1, 1}, {2, 1}, {3, 1},	{4, 1},
	{0, 2}, {1, 2}, {2, 2}, {3, 2}, {4, 2},
	{0, 3}, {1, 3}, {2, 3}, {3, 3}, {4, 3},
	{1, 4}, {2, 4}, {3, 4}, {4, 4}, {2, 5},
	{3, 5}, {4, 5}, {3, 6}, {4, 6}, {4, 7},
};

static const struct dvfsrc_opp_desc dvfsrc_opp_mt6877_desc[] = {
	DVFSRC_OPP_DESC(dvfsrc_opp_mt6877_lp4),
};

static const struct dvfsrc_soc_data mt6877_data = {
	DVFSRC_MT6873_SERIES_OPS,
	.opps_desc = dvfsrc_opp_mt6877_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6877_desc),
	.regs = mt6873_regs,
#ifdef DVFSRC_FORCE_OPP_SUPPORT
	.set_force_opp_level = mt6873_set_force_opp_level,
#endif
};

static const struct dvfsrc_opp dvfsrc_opp_mt6983[] = {
	{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0},
	{0, 1}, {1, 1}, {2, 1}, {3, 1},	{4, 1},
	{1, 2}, {2, 2}, {3, 2}, {4, 2}, {1, 3},
	{2, 3}, {3, 3}, {4, 3},
	{2, 4}, {3, 4}, {4, 4},
	{3, 5}, {4, 5},
	{3, 6}, {4, 6},
	{4, 7},
	{4, 8},
};

static const struct dvfsrc_opp_desc dvfsrc_opp_mt6983_desc[] = {
	DVFSRC_OPP_DESC(dvfsrc_opp_mt6983),
};

static const struct dvfsrc_soc_data mt6983_data = {
	DVFSRC_MT6983_SERIES_OPS,
	.opps_desc = dvfsrc_opp_mt6983_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6983_desc),
	.regs = mt6983_regs,
#ifdef DVFSRC_FORCE_OPP_SUPPORT
	.set_force_opp_level = mt6983_set_force_opp_level,
#endif
};

static const struct dvfsrc_opp dvfsrc_opp_mt6879[] = {
	{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0},
	{0, 1}, {1, 1}, {2, 1}, {3, 1},	{4, 1},
	{0, 2}, {1, 2}, {2, 2}, {3, 2}, {4, 2},
	{0, 3}, {1, 3}, {2, 3}, {3, 3}, {4, 3},
	{0, 4}, {1, 4}, {2, 4}, {3, 4}, {4, 4},
	{1, 5}, {2, 5}, {3, 5}, {4, 5},
	{2, 6}, {3, 6}, {4, 6},
	{4, 7},
};

static const struct dvfsrc_opp_desc dvfsrc_opp_mt6879_desc[] = {
	DVFSRC_OPP_DESC(dvfsrc_opp_mt6879),
};

static const struct dvfsrc_soc_data mt6879_data = {
	DVFSRC_MT6983_SERIES_OPS,
	.opps_desc = dvfsrc_opp_mt6879_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6879_desc),
	.regs = mt6983_regs,
#ifdef DVFSRC_FORCE_OPP_SUPPORT
	.set_force_opp_level = mt6983_set_force_opp_level,
#endif
};

static const struct dvfsrc_opp dvfsrc_opp_mt6855[] = {
	{0, 0}, {1, 0}, {2, 0}, {3, 0},
	{0, 1}, {1, 1}, {2, 1}, {3, 1},
	{0, 2}, {1, 2}, {2, 2}, {3, 2},
	{0, 3}, {1, 3}, {2, 3}, {3, 3},
	{0, 4}, {1, 4}, {2, 4}, {3, 4},
	{1, 5}, {2, 5}, {3, 5},
	{2, 6}, {3, 6},
	{3, 7},
};

static const struct dvfsrc_opp_desc dvfsrc_opp_mt6855_desc[] = {
	DVFSRC_OPP_DESC(dvfsrc_opp_mt6855),
};

static const struct dvfsrc_soc_data mt6855_data = {
	DVFSRC_MT6983_SERIES_OPS,
	.opps_desc = dvfsrc_opp_mt6855_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6855_desc),
	.regs = mt6983_regs,
#ifdef DVFSRC_FORCE_OPP_SUPPORT
	.set_force_opp_level = mt6983_set_force_opp_level,
#endif
};

static const struct dvfsrc_opp dvfsrc_opp_mt6768[] = {
	{0, 0}, {1, 0}, {1, 0}, {2, 0},
	{2, 1}, {2, 0}, {2, 1}, {2, 1},
	{3, 1}, {3, 2}, {3, 1}, {3, 2},
	{3, 1}, {3, 2}, {3, 2}, {3, 2},
};

static const struct dvfsrc_opp dvfsrc_opp_mt6765[] = {
	{0, 0}, {1, 0}, {1, 0}, {2, 0},
	{2, 1}, {2, 0}, {2, 1}, {2, 1},
	{3, 1}, {3, 2}, {3, 1}, {3, 2},
	{3, 1}, {3, 2}, {3, 2}, {3, 2},
};

static const struct dvfsrc_opp_desc dvfsrc_opp_mt6768_desc[] = {
	{0},
	DVFSRC_OPP_DESC(dvfsrc_opp_mt6768),
	{0},
	DVFSRC_OPP_DESC(dvfsrc_opp_mt6768),
};

static const struct dvfsrc_opp_desc dvfsrc_opp_mt6765_desc[] = {
	DVFSRC_OPP_DESC(dvfsrc_opp_mt6765),
	DVFSRC_OPP_DESC(dvfsrc_opp_mt6765),
	DVFSRC_OPP_DESC(dvfsrc_opp_mt6765),
	DVFSRC_OPP_DESC(dvfsrc_opp_mt6765),
	{0},
};

static const struct dvfsrc_soc_data mt6768_data = {
	DVFSRC_MT6768_SERIES_OPS,
	.opps_desc = dvfsrc_opp_mt6768_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6768_desc),
	.regs = mt6768_regs,
#ifdef DVFSRC_FORCE_OPP_SUPPORT
	.set_force_opp_level = mt6768_set_force_opp_level,
#endif
};

static const struct dvfsrc_soc_data mt6765_data = {
	DVFSRC_MT6765_SERIES_OPS,
	.opps_desc = dvfsrc_opp_mt6765_desc,
	.num_opp_desc = ARRAY_SIZE(dvfsrc_opp_mt6765_desc),
	.regs = mt6765_regs,
#ifdef DVFSRC_FORCE_OPP_SUPPORT
	.set_force_opp_level = mt6765_set_force_opp_level,
#endif
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
		.compatible = "mediatek,mt6853-dvfsrc",
		.data = &mt6873_data,
	}, {
		.compatible = "mediatek,mt6789-dvfsrc",
		.data = &mt6873_data,
	}, {
		.compatible = "mediatek,mt6885-dvfsrc",
		.data = &mt6885_data,
	}, {
		.compatible = "mediatek,mt6893-dvfsrc",
		.data = &mt6893_data,
	}, {
		.compatible = "mediatek,mt6833-dvfsrc",
		.data = &mt6833_data,
	}, {
		.compatible = "mediatek,mt6877-dvfsrc",
		.data = &mt6877_data,
	}, {
		.compatible = "mediatek,mt6983-dvfsrc",
		.data = &mt6983_data,
	}, {
		.compatible = "mediatek,mt6895-dvfsrc",
		.data = &mt6983_data,
	}, {
		.compatible = "mediatek,mt6879-dvfsrc",
		.data = &mt6879_data,
	}, {
		.compatible = "mediatek,mt6855-dvfsrc",
		.data = &mt6855_data,
	}, {
		.compatible = "mediatek,mt6768-dvfsrc",
		.data = &mt6768_data,
	}, {
		.compatible = "mediatek,mt6765-dvfsrc",
		.data = &mt6765_data,
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
#if IS_BUILTIN(CONFIG_MTK_DVFSRC)
module_init(mtk_dvfsrc_init);
#else
subsys_initcall(mtk_dvfsrc_init);
#endif
static void __exit mtk_dvfsrc_exit(void)
{
	platform_driver_unregister(&mtk_dvfsrc_driver);
}
module_exit(mtk_dvfsrc_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MTK DVFSRC driver");

