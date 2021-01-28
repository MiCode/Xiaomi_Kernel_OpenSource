// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/slab.h>
#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk_dvfsrc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include "mtk-scpsys.h"
#include <linux/regulator/consumer.h>

#define DVFSRC_IDLE		0x00
#define DVFSRC_GET_TARGET_LEVEL(x)	(((x) >> 0) & 0x0000ffff)
#define DVFSRC_GET_CURRENT_LEVEL(x)	(((x) >> 16) & 0x0000ffff)
#define kBps_to_MBps(x)	(x / 1000)

#define MT8183_DVFSRC_OPP_LP4	0
#define MT8183_DVFSRC_OPP_LP4X	1
#define MT8183_DVFSRC_OPP_LP3	2

#define POLL_TIMEOUT		1000
#define STARTUP_TIME		1

/**  VCORE UV table */
static u32 num_vopp;
static u32 *vopp_uv_tlb;
#define MTK_SIP_VCOREFS_VCORE_NUM  0x6
#define MTK_SIP_VCOREFS_VCORE_UV  0x4

struct dvfsrc_opp {
	u32 vcore_opp;
	u32 dram_opp;
};

struct dvfsrc_domain {
	u32 id;
	u32 state;
};

struct mtk_dvfsrc;
struct dvfsrc_soc_data {
	const int *regs;
	u32 num_opp;
	const struct dvfsrc_opp **opps;
	bool vcore_check;
	bool vopp_table_init;
	int (*get_target_level)(struct mtk_dvfsrc *dvfsrc);
	int (*get_current_level)(struct mtk_dvfsrc *dvfsrc);
	u32 (*get_vcore_level)(struct mtk_dvfsrc *dvfsrc);
	u32 (*get_vcp_level)(struct mtk_dvfsrc *dvfsrc);
	void (*set_dram_bw)(struct mtk_dvfsrc *dvfsrc, u64 bw);
	void (*set_opp_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	void (*set_dram_hrtbw)(struct mtk_dvfsrc *dvfsrc, u64 bw);
	void (*set_vcore_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	void (*set_vscp_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	int (*set_force_opp_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	int (*wait_for_vcore_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	int (*wait_for_opp_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
};

struct mtk_dvfsrc {
	struct device *dev;
	struct clk *clk_dvfsrc;
	const struct dvfsrc_soc_data *dvd;
	int dram_type;
	u32 mode;
	u32 flag;
	int num_domains;
	struct dvfsrc_domain *domains;
	void __iomem *regs;
	struct mutex lock;
	struct mutex sw_req_lock;
	struct notifier_block scpsys_notifier;
	struct regulator *vcore_power;
	bool opp_forced;
	bool is_dvfsrc_enable;
};

static DEFINE_MUTEX(pstate_lock);
static DEFINE_SPINLOCK(force_req_lock);

static bool is_dvfsrc_init_complete;

int mtk_dvfsrc_vcore_uv_table(u32 opp)
{
	if ((!vopp_uv_tlb) || (opp >= num_vopp))
		return 0;

	return vopp_uv_tlb[num_vopp - opp - 1];
}
EXPORT_SYMBOL(mtk_dvfsrc_vcore_uv_table);

int mtk_dvfsrc_vcore_opp_count(void)
{
	return num_vopp;
}
EXPORT_SYMBOL(mtk_dvfsrc_vcore_opp_count);

static void mtk_dvfsrc_setup_vopp_table(struct mtk_dvfsrc *dvfsrc)
{
	int i;
	struct arm_smccc_res ares;
	u32 num_opp = dvfsrc->dvd->num_opp;

	num_vopp =
		dvfsrc->dvd->opps[dvfsrc->dram_type][num_opp - 1].vcore_opp + 1;
	vopp_uv_tlb = kcalloc(num_vopp, sizeof(u32), GFP_KERNEL);

	if (!vopp_uv_tlb)
		return;

	for (i = 0; i < num_vopp; i++) {
		arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL,
			MTK_SIP_VCOREFS_VCORE_UV,
			i, 0, 0, 0, 0, 0,
			&ares);

		if (!ares.a0)
			vopp_uv_tlb[i] = ares.a1;
		else {
			kfree(vopp_uv_tlb);
			vopp_uv_tlb = NULL;
			break;
		}
	}
	for (i = 0; i < num_vopp; i++)
		dev_info(dvfsrc->dev, "dvfsrc vopp[%d] = %d\n",
			i, mtk_dvfsrc_vcore_uv_table(i));

}

static void mtk_dvfsrc_vcore_check(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	int opp_uv;
	int vcore_uv = 0;

	if (!vopp_uv_tlb)
		return;

	if (!dvfsrc->vcore_power) {
		dvfsrc->vcore_power =
			regulator_get_optional(dvfsrc->dev, "vcore");
		if (IS_ERR(dvfsrc->vcore_power))
			dvfsrc->vcore_power = NULL;
	} else {
		opp_uv = vopp_uv_tlb[level];
		vcore_uv = regulator_get_voltage(dvfsrc->vcore_power);
		if (vcore_uv < opp_uv) {
			pr_info("DVFS FAIL= %d, %d %d 0x%08x 0x%08x\n",
			level, opp_uv, vcore_uv,
			dvfsrc->dvd->get_current_level(dvfsrc),
			dvfsrc->dvd->get_target_level(dvfsrc));
		}
	}
}

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
	DVFSRC_SW_REQ,
	DVFSRC_LEVEL,
	DVFSRC_SW_BW,
	DVFSRC_SW_HRT_BW,
	DVFSRC_TARGET_LEVEL,
	DVFSRC_VCORE_REQUEST,
	DVFSRC_BASIC_CONTROL,
	DVFSRC_TARGET_FORCE,
};

static const int mt8183_regs[] = {
	[DVFSRC_SW_REQ] =	0x4,
	[DVFSRC_LEVEL] =	0xDC,
	[DVFSRC_SW_BW] =	0x160,
};

static const int mt6779_regs[] = {
	[DVFSRC_SW_REQ] =	0xC,
	[DVFSRC_LEVEL] =	0xD44,
	[DVFSRC_SW_BW] =	0x260,
	[DVFSRC_SW_HRT_BW] =	0x290,
	[DVFSRC_TARGET_LEVEL] =	0xD48,
	[DVFSRC_VCORE_REQUEST] = 0x6C,
	[DVFSRC_BASIC_CONTROL] = 0x0,
	[DVFSRC_TARGET_FORCE] = 0xD70,
};

static const int mt6761_regs[] = {
	[DVFSRC_SW_REQ] =	0x4,
	[DVFSRC_LEVEL] =	0xDC,
	[DVFSRC_SW_BW] =	0x160,
	[DVFSRC_VCORE_REQUEST] = 0x48,
	[DVFSRC_BASIC_CONTROL] = 0x0,
	[DVFSRC_TARGET_FORCE] = 0x300,
};

static const struct dvfsrc_opp *get_current_opp(struct mtk_dvfsrc *dvfsrc)
{
	int level;

	level = dvfsrc->dvd->get_current_level(dvfsrc);
	return &dvfsrc->dvd->opps[dvfsrc->dram_type][level];
}

static bool dvfsrc_is_idle(struct mtk_dvfsrc *dvfsrc)
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

static int dvfsrc_wait_for_opp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	const struct dvfsrc_opp *target, *curr;

	target = &dvfsrc->dvd->opps[dvfsrc->dram_type][level];

	return readx_poll_timeout_atomic(get_current_opp, dvfsrc, curr,
		curr->dram_opp >= target->dram_opp,
		STARTUP_TIME, POLL_TIMEOUT);
}

static int dvfsrc_wait_for_vcore_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	const struct dvfsrc_opp *curr;

	return readx_poll_timeout_atomic(get_current_opp, dvfsrc, curr,
		curr->vcore_opp >= level,
		STARTUP_TIME, POLL_TIMEOUT);
}

static int mt6779_get_target_level(struct mtk_dvfsrc *dvfsrc)
{
	return dvfsrc_read(dvfsrc, DVFSRC_TARGET_LEVEL);
}

static int mt6779_get_current_level(struct mtk_dvfsrc *dvfsrc)
{
	u32 curr_level;

	curr_level = ffs(dvfsrc_read(dvfsrc, DVFSRC_LEVEL));
	if (curr_level > dvfsrc->dvd->num_opp)
		curr_level = 0;
	else
		curr_level = dvfsrc->dvd->num_opp - curr_level;

	return curr_level;
}

static u32 mt6779_get_vcore_level(struct mtk_dvfsrc *dvfsrc)
{
	return (dvfsrc_read(dvfsrc, DVFSRC_SW_REQ) >> 4) & 0x7;
}

static u32 mt6779_get_vcp_level(struct mtk_dvfsrc *dvfsrc)
{
	return (dvfsrc_read(dvfsrc, DVFSRC_VCORE_REQUEST) >> 12) & 0x7;
}

static void mt6779_set_dram_bw(struct mtk_dvfsrc *dvfsrc, u64 bw)
{
	bw = kBps_to_MBps(bw) / 100;
	bw = (bw < 0xFF) ? bw : 0xff;

	dvfsrc_write(dvfsrc, DVFSRC_SW_BW, bw);
}

/* bw unit 30MBps */
static void mt6779_set_dram_hrtbw(struct mtk_dvfsrc *dvfsrc, u64 bw)
{
	bw = (kBps_to_MBps(bw) + 29) / 30;
	if (bw > 0x3FF)
		bw = 0x3FF;

	dvfsrc_write(dvfsrc, DVFSRC_SW_HRT_BW, bw);
}

static void mt6779_set_vcore_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	dev_dbg(dvfsrc->dev, "vcore_level: %d\n", level);
	mutex_lock(&dvfsrc->sw_req_lock);
	dvfsrc_rmw(dvfsrc, DVFSRC_SW_REQ, level, 0x7, 4);
	mutex_unlock(&dvfsrc->sw_req_lock);
}

static void mt6779_set_vscp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	dev_dbg(dvfsrc->dev, "vscp_level: %d\n", level);
	dvfsrc_rmw(dvfsrc, DVFSRC_VCORE_REQUEST, level, 0x7, 12);
}

static void mt6779_set_dram_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	dev_dbg(dvfsrc->dev, "dram_opp: %d\n", level);
	mutex_lock(&dvfsrc->sw_req_lock);
	dvfsrc_rmw(dvfsrc, DVFSRC_SW_REQ, level, 0x7, 12);
	mutex_unlock(&dvfsrc->sw_req_lock);
}

static void mt6779_set_opp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	const struct dvfsrc_opp *opp;

	opp = &dvfsrc->dvd->opps[dvfsrc->dram_type][level];

	mt6779_set_dram_level(dvfsrc, opp->dram_opp);
}

static int mt6779_set_force_opp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	unsigned long flags;
	int val;
	int ret = 0;

	if (level > dvfsrc->dvd->num_opp - 1) {
		dvfsrc_rmw(dvfsrc, DVFSRC_BASIC_CONTROL, 0, 0x1, 15);
		dvfsrc_write(dvfsrc, DVFSRC_TARGET_FORCE, 0);
		dvfsrc->opp_forced = false;
		return 0;
	}

	dvfsrc->opp_forced = true;
	spin_lock_irqsave(&force_req_lock, flags);
	dvfsrc_write(dvfsrc, DVFSRC_TARGET_FORCE, 1 << level);
	dvfsrc_rmw(dvfsrc, DVFSRC_BASIC_CONTROL, 1, 0x1, 15);
	ret = readl_poll_timeout_atomic(
			dvfsrc->regs + dvfsrc->dvd->regs[DVFSRC_LEVEL],
			val, val == (1 << level), STARTUP_TIME, POLL_TIMEOUT);

	if (ret < 0) {
		dev_info(dvfsrc->dev,
			"[%s] wait idle, level: %d, last: %d -> %x\n",
			__func__, level,
			dvfsrc->dvd->get_current_level(dvfsrc),
			dvfsrc->dvd->get_target_level(dvfsrc));

	}
	dvfsrc_write(dvfsrc, DVFSRC_TARGET_FORCE, 0);
	spin_unlock_irqrestore(&force_req_lock, flags);

	return ret;
}

static int mt6761_get_target_level(struct mtk_dvfsrc *dvfsrc)
{
	return DVFSRC_GET_TARGET_LEVEL(dvfsrc_read(dvfsrc, DVFSRC_LEVEL));
}

static int mt6761_get_current_level(struct mtk_dvfsrc *dvfsrc)
{
	u32 curr_level;

	curr_level = dvfsrc_read(dvfsrc, DVFSRC_LEVEL);
	curr_level = ffs(DVFSRC_GET_CURRENT_LEVEL(curr_level));

	if ((curr_level > 0) && (curr_level <= dvfsrc->dvd->num_opp))
		return curr_level - 1;
	else
		return 0;
}

static u32 mt6761_get_vcore_level(struct mtk_dvfsrc *dvfsrc)
{
	return (dvfsrc_read(dvfsrc, DVFSRC_SW_REQ) >> 2) & 0x3;
}

static u32 mt6761_get_vcp_level(struct mtk_dvfsrc *dvfsrc)
{
	return (dvfsrc_read(dvfsrc, DVFSRC_VCORE_REQUEST) >> 30) & 0x3;
}

static void mt6761_set_dram_bw(struct mtk_dvfsrc *dvfsrc, u64 bw)
{
	bw = kBps_to_MBps(bw) / 100;
	bw = (bw < 0xFF) ? bw : 0xff;

	dvfsrc_write(dvfsrc, DVFSRC_SW_BW, bw);
}

static void mt6761_set_vcore_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	dev_dbg(dvfsrc->dev, "vcore_level: %d\n", level);
	mutex_lock(&dvfsrc->sw_req_lock);
	dvfsrc_rmw(dvfsrc, DVFSRC_SW_REQ, level, 0x3, 2);
	mutex_unlock(&dvfsrc->sw_req_lock);
}

static void mt6761_set_vscp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	dev_dbg(dvfsrc->dev, "vscp_level: %d\n", level);
	dvfsrc_rmw(dvfsrc, DVFSRC_VCORE_REQUEST, level, 0x3, 30);
}

static void mt6761_set_dram_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	dev_dbg(dvfsrc->dev, "dram_opp: %d\n", level);
	mutex_lock(&dvfsrc->sw_req_lock);
	dvfsrc_rmw(dvfsrc, DVFSRC_SW_REQ, level, 0x3, 0);
	mutex_unlock(&dvfsrc->sw_req_lock);
}

static void mt6761_set_opp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	const struct dvfsrc_opp *opp;

	opp = &dvfsrc->dvd->opps[dvfsrc->dram_type][level];

	mt6761_set_dram_level(dvfsrc, opp->dram_opp);
}

static int mt6761_set_force_opp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	unsigned long flags;
	int val;
	int ret = 0;

	if (level > dvfsrc->dvd->num_opp - 1) {
		dvfsrc_rmw(dvfsrc, DVFSRC_BASIC_CONTROL, 0, 0x1, 15);
		dvfsrc_write(dvfsrc, DVFSRC_TARGET_FORCE, 0);
		dvfsrc->opp_forced = false;
		return 0;
	}

	dvfsrc->opp_forced = true;
	spin_lock_irqsave(&force_req_lock, flags);
	level = dvfsrc->dvd->num_opp - 1 - level;
	dvfsrc_write(dvfsrc, DVFSRC_TARGET_FORCE, 1 << level);
	dvfsrc_rmw(dvfsrc, DVFSRC_BASIC_CONTROL, 1, 0x1, 15);
	ret = readl_poll_timeout_atomic(
			dvfsrc->regs + dvfsrc->dvd->regs[DVFSRC_LEVEL],
			val, val == (1 << level), STARTUP_TIME, POLL_TIMEOUT);

	if (ret < 0) {
		dev_info(dvfsrc->dev,
			"[%s] wait idle, level: %d, last: %d -> %x\n",
			__func__, level,
			dvfsrc->dvd->get_current_level(dvfsrc),
			dvfsrc->dvd->get_target_level(dvfsrc));

	}
	dvfsrc_write(dvfsrc, DVFSRC_TARGET_FORCE, 0);
	spin_unlock_irqrestore(&force_req_lock, flags);

	return ret;
}


static int mt8183_get_target_level(struct mtk_dvfsrc *dvfsrc)
{
	return DVFSRC_GET_TARGET_LEVEL(dvfsrc_read(dvfsrc, DVFSRC_LEVEL));
}

static int mt8183_get_current_level(struct mtk_dvfsrc *dvfsrc)
{
	return ffs(DVFSRC_GET_CURRENT_LEVEL(dvfsrc_read(dvfsrc, DVFSRC_LEVEL)));
}

static void mt8183_set_dram_bw(struct mtk_dvfsrc *dvfsrc, u64 bw)
{
	dvfsrc_write(dvfsrc, DVFSRC_SW_BW, kBps_to_MBps(bw) / 100);
}

static void mt8183_set_opp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	int vcore_opp, dram_opp;
	const struct dvfsrc_opp *opp;

	/* translate pstate to dvfsrc level, and set it to DVFSRC HW */
	opp = &dvfsrc->dvd->opps[dvfsrc->dram_type][level];
	vcore_opp = opp->vcore_opp;
	dram_opp = opp->dram_opp;

	dev_dbg(dvfsrc->dev, "vcore_opp: %d, dram_opp: %d\n",
		vcore_opp, dram_opp);
	dvfsrc_write(dvfsrc, DVFSRC_SW_REQ, dram_opp | vcore_opp << 2);
}

void mtk_dvfsrc_send_request(const struct device *dev, u32 cmd, u64 data)
{
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);
	int ret = 0;

	dev_dbg(dvfsrc->dev, "cmd: %d, data: %llu\n", cmd, data);

	if (cmd == MTK_DVFSRC_CMD_FORCE_OPP_REQUEST) {
		if (dvfsrc->dvd->set_force_opp_level)
			dvfsrc->dvd->set_force_opp_level(dvfsrc, data);
		return;
	}

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
	case MTK_DVFSRC_CMD_VSCP_REQUEST:
		dvfsrc->dvd->set_vscp_level(dvfsrc, data);
		break;
	default:
		dev_err(dvfsrc->dev, "unknown command: %d\n", cmd);
		goto out;
		break;
	}

	if (dvfsrc->opp_forced || !dvfsrc->is_dvfsrc_enable)
		goto out;

	udelay(STARTUP_TIME);
	dvfsrc_wait_for_idle(dvfsrc);
	udelay(STARTUP_TIME);

	switch (cmd) {
	case MTK_DVFSRC_CMD_OPP_REQUEST:
		if (dvfsrc->dvd->wait_for_opp_level)
			ret = dvfsrc->dvd->wait_for_opp_level(dvfsrc, data);
		break;
	case MTK_DVFSRC_CMD_VCORE_REQUEST:
	case MTK_DVFSRC_CMD_VSCP_REQUEST:
		if (dvfsrc->dvd->wait_for_vcore_level) {
			ret = dvfsrc->dvd->wait_for_vcore_level(dvfsrc, data);
			if (dvfsrc->dvd->vcore_check)
				mtk_dvfsrc_vcore_check(dvfsrc, data);
		}
		break;
	case MTK_DVFSRC_CMD_HRTBW_REQUEST:
		ret = dvfsrc_wait_for_idle(dvfsrc);
		break;
	}
out:
	if (ret) {
		dev_warn(dvfsrc->dev,
			"[%s][%d] wait idle, level: %llu, last: %d -> %x\n",
			__func__, cmd, data,
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
	default:
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(mtk_dvfsrc_query_info);

static int dvfsrc_set_performance(struct notifier_block *b,
				 unsigned long l, void *v)
{
	int i;
	bool match = false;
	u32 highest;
	struct mtk_dvfsrc *dvfsrc;
	struct scp_event_data *sc = v;
	struct dvfsrc_domain *d;

	if (sc->event_type != MTK_SCPSYS_PSTATE)
		return 0;

	dvfsrc = container_of(b, struct mtk_dvfsrc, scpsys_notifier);

	d = dvfsrc->domains;

	if (l > dvfsrc->dvd->num_opp) {
		dev_err(dvfsrc->dev, "pstate out of range = %ld\n", l);
		goto out;
	}

	mutex_lock(&pstate_lock);
	for (i = 0, highest = 0; i < dvfsrc->num_domains; i++, d++) {
		if (sc->domain_id == d->id) {
			d->state = l;
			match = true;
		}
		highest = max(highest, d->state);
	}

	if (!match)
		goto out;

	if (highest != 0)
		highest = highest - 1;

	mtk_dvfsrc_send_request(dvfsrc->dev, MTK_DVFSRC_CMD_OPP_REQUEST,
		highest);

out:
	mutex_unlock(&pstate_lock);
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
	struct device_node *node = pdev->dev.of_node;
	int i;
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

	dvfsrc->num_domains = of_count_phandle_with_args(node,
		"perf-domains", NULL);

	if (dvfsrc->num_domains > 0) {
		dvfsrc->domains = devm_kzalloc(&pdev->dev,
			sizeof(struct dvfsrc_domain) * dvfsrc->num_domains,
			GFP_KERNEL);

		if (!dvfsrc->domains)
			return -ENOMEM;

		for (i = 0; i < dvfsrc->num_domains; i++)
			of_property_read_u32_index(node, "perf-domains",
				i, &dvfsrc->domains[i].id);
	} else
		dvfsrc->num_domains = 0;

	dvfsrc->clk_dvfsrc = devm_clk_get(dvfsrc->dev, "dvfsrc");
	if (IS_ERR(dvfsrc->clk_dvfsrc)) {
		dev_err(dvfsrc->dev, "failed to get clock: %ld\n",
			PTR_ERR(dvfsrc->clk_dvfsrc));
		return PTR_ERR(dvfsrc->clk_dvfsrc);
	}

	ret = clk_prepare_enable(dvfsrc->clk_dvfsrc);
	if (ret)
		return ret;

	of_property_read_u32(node, "dvfsrc,mode", &dvfsrc->mode);
	of_property_read_u32(node, "dvfsrc,flag", &dvfsrc->flag);

	mutex_init(&dvfsrc->lock);
	mutex_init(&dvfsrc->sw_req_lock);
	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, MTK_SIP_SPM_DVFSRC_INIT,
		dvfsrc->mode, dvfsrc->flag, 0, 0, 0, 0,
		&ares);

	if (!ares.a0) {
		dvfsrc->dram_type = ares.a1;
		dvfsrc->is_dvfsrc_enable = true;
	} else
		dev_info(dvfsrc->dev, "spm init fails: %lx\n", ares.a0);

	platform_set_drvdata(pdev, dvfsrc);
	pstate_notifier_register(dvfsrc);

	if ((dvfsrc->dvd->vopp_table_init) || (dvfsrc->dvd->vcore_check))
		mtk_dvfsrc_setup_vopp_table(dvfsrc);

	ret = devm_of_platform_populate(&pdev->dev);
	if (ret)
		dev_err(&pdev->dev, "Failed to populate dvfsrc context\n");

	if (dvfsrc->is_dvfsrc_enable)
		is_dvfsrc_init_complete = true;

	return 0;
}

static const struct dvfsrc_opp dvfsrc_opp_mt8183_lp4[] = {
	{0, 0}, {0, 1}, {0, 2}, {1, 2},
};

static const struct dvfsrc_opp dvfsrc_opp_mt8183_lp3[] = {
	{0, 0}, {0, 1}, {1, 1}, {1, 2},
};

static const struct dvfsrc_opp *dvfsrc_opp_mt8183[] = {
	[MT8183_DVFSRC_OPP_LP4] = dvfsrc_opp_mt8183_lp4,
	[MT8183_DVFSRC_OPP_LP4X] = dvfsrc_opp_mt8183_lp3,
	[MT8183_DVFSRC_OPP_LP3] = dvfsrc_opp_mt8183_lp3,
};


static const struct dvfsrc_soc_data mt8183_data = {
	.opps = dvfsrc_opp_mt8183,
	.num_opp = ARRAY_SIZE(dvfsrc_opp_mt8183_lp4),
	.regs = mt8183_regs,
	.get_target_level = mt8183_get_target_level,
	.get_current_level = mt8183_get_current_level,
	.set_dram_bw = mt8183_set_dram_bw,
	.set_opp_level = mt8183_set_opp_level,
};

static const struct dvfsrc_opp dvfsrc_opp_mt6779_lp4[] = {
	{0, 0}, {0, 1}, {0, 2}, {0, 3},
	{1, 1}, {1, 2}, {1, 3}, {1, 4},
	{2, 1}, {2, 2}, {2, 3}, {2, 4}, {2, 5},
};

static const struct dvfsrc_opp *dvfsrc_opp_mt6779[] = {
	[0] = dvfsrc_opp_mt6779_lp4,
};

static const struct dvfsrc_soc_data mt6779_data = {
	.opps = dvfsrc_opp_mt6779,
	.num_opp = ARRAY_SIZE(dvfsrc_opp_mt6779_lp4),
	.regs = mt6779_regs,
	.vcore_check = true,
	.vopp_table_init = true,
	.get_target_level = mt6779_get_target_level,
	.get_current_level = mt6779_get_current_level,
	.get_vcore_level = mt6779_get_vcore_level,
	.get_vcp_level = mt6779_get_vcp_level,
	.wait_for_vcore_level = dvfsrc_wait_for_vcore_level,
	.wait_for_opp_level = dvfsrc_wait_for_opp_level,
	.set_dram_bw = mt6779_set_dram_bw,
	.set_opp_level = mt6779_set_opp_level,
	.set_dram_hrtbw = mt6779_set_dram_hrtbw,
	.set_vcore_level = mt6779_set_vcore_level,
	.set_vscp_level = mt6779_set_vscp_level,
	.set_force_opp_level = mt6779_set_force_opp_level,
};

static const struct dvfsrc_opp dvfsrc_opp_mt6761_lp3[] = {
	{0, 0},
	{1, 0},
	{1, 0},
	{2, 0},
	{2, 1},
	{2, 0},
	{2, 1},
	{2, 1},
	{3, 1},
	{3, 2},
	{3, 1},
	{3, 2},
	{3, 1},
	{3, 2},
	{3, 2},
	{3, 2},
};

static const struct dvfsrc_opp *dvfsrc_opp_mt6761[] = {
	[0] = dvfsrc_opp_mt6761_lp3,
};

static const struct dvfsrc_soc_data mt6761_data = {
	.opps = dvfsrc_opp_mt6761,
	.num_opp = ARRAY_SIZE(dvfsrc_opp_mt6761_lp3),
	.regs = mt6761_regs,
	.vcore_check = false,
	.vopp_table_init = false,
	.get_target_level = mt6761_get_target_level,
	.get_current_level = mt6761_get_current_level,
	.get_vcore_level = mt6761_get_vcore_level,
	.get_vcp_level = mt6761_get_vcp_level,
	.wait_for_vcore_level = dvfsrc_wait_for_vcore_level,
	.wait_for_opp_level = dvfsrc_wait_for_opp_level,
	.set_dram_bw = mt6761_set_dram_bw,
	.set_opp_level = mt6761_set_opp_level,
	.set_vcore_level = mt6761_set_vcore_level,
	.set_vscp_level = mt6761_set_vscp_level,
	.set_force_opp_level = mt6761_set_force_opp_level,
};

static int mtk_dvfsrc_remove(struct platform_device *pdev)
{
	struct mtk_dvfsrc *dvfsrc = platform_get_drvdata(pdev);

	clk_disable_unprepare(dvfsrc->clk_dvfsrc);

	return 0;
}

static const struct of_device_id mtk_dvfsrc_of_match[] = {
	{
		.compatible = "mediatek,mt8183-dvfsrc",
		.data = &mt8183_data,
	}, {
		.compatible = "mediatek,mt6779-dvfsrc",
		.data = &mt6779_data,
	}, {
		.compatible = "mediatek,mt6761-dvfsrc",
		.data = &mt6761_data,
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

builtin_platform_driver(mtk_dvfsrc_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MTK DVFSRC driver");

