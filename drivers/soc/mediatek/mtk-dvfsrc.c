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
#define kbps_to_mbps(x) ((x) / 1000)

#define MT8183_DVFSRC_OPP_LP4   0
#define MT8183_DVFSRC_OPP_LP4X  1
#define MT8183_DVFSRC_OPP_LP3   2

#define POLL_TIMEOUT        1000
#define STARTUP_TIME        1

#define MTK_SIP_DVFSRC_INIT		0x00

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
	u32 num_domains;
	const struct dvfsrc_opp **opps;
	struct dvfsrc_domain *domains;
	int (*get_target_level)(struct mtk_dvfsrc *dvfsrc);
	int (*get_current_level)(struct mtk_dvfsrc *dvfsrc);
	u32 (*get_vcore_level)(struct mtk_dvfsrc *dvfsrc);
	void (*set_dram_bw)(struct mtk_dvfsrc *dvfsrc, u64 bw);
	void (*set_opp_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	void (*set_vcore_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	int (*wait_for_opp_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
	int (*wait_for_vcore_level)(struct mtk_dvfsrc *dvfsrc, u32 level);
};

struct mtk_dvfsrc {
	struct device *dev;
	struct clk *clk_dvfsrc;
	const struct dvfsrc_soc_data *dvd;
	int dram_type;
	void __iomem *regs;
	struct mutex req_lock;
	struct mutex pstate_lock;
	struct notifier_block scpsys_notifier;
};

static u32 dvfsrc_read(struct mtk_dvfsrc *dvfs, u32 offset)
{
	return readl(dvfs->regs + dvfs->dvd->regs[offset]);
}

static void dvfsrc_write(struct mtk_dvfsrc *dvfs, u32 offset, u32 val)
{
	writel(val, dvfs->regs + dvfs->dvd->regs[offset]);
}

enum dvfsrc_regs {
	DVFSRC_SW_REQ,
	DVFSRC_SW_REQ2,
	DVFSRC_LEVEL,
	DVFSRC_SW_BW,
	DVFSRC_LAST,
};

static const int mt8183_regs[] = {
	[DVFSRC_SW_REQ] =	0x4,
	[DVFSRC_SW_REQ2] =	0x8,
	[DVFSRC_LEVEL] =	0xDC,
	[DVFSRC_SW_BW] =	0x160,
	[DVFSRC_LAST] =		0x308,
};

static const struct dvfsrc_opp *get_current_opp(struct mtk_dvfsrc *dvfsrc)
{
	int level;

	level = dvfsrc->dvd->get_current_level(dvfsrc);
	return &dvfsrc->dvd->opps[dvfsrc->dram_type][level];
}

static int dvfsrc_is_idle(struct mtk_dvfsrc *dvfsrc)
{
	if (!dvfsrc->dvd->get_target_level)
		return true;

	return dvfsrc->dvd->get_target_level(dvfsrc);
}

static int dvfsrc_wait_for_vcore_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	const struct dvfsrc_opp *curr;

	return readx_poll_timeout_atomic(get_current_opp, dvfsrc, curr,
					 curr->vcore_opp >= level, STARTUP_TIME,
					 POLL_TIMEOUT);
}

static int mt8183_wait_for_opp_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	const struct dvfsrc_opp *target, *curr;
	int ret;

	target = &dvfsrc->dvd->opps[dvfsrc->dram_type][level];
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
	dvfsrc_write(dvfsrc, DVFSRC_SW_BW, kbps_to_mbps(bw) / 100);
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

static void mt8183_set_vcore_level(struct mtk_dvfsrc *dvfsrc, u32 level)
{
	dvfsrc_write(dvfsrc, DVFSRC_SW_REQ2, level << 2);
}

void mtk_dvfsrc_send_request(const struct device *dev, u32 cmd, u64 data)
{
	int ret, state;
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	dev_dbg(dvfsrc->dev, "cmd: %d, data: %llu\n", cmd, data);

	mutex_lock(&dvfsrc->req_lock);

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
	default:
		dev_err(dvfsrc->dev, "unknown command: %d\n", cmd);
		goto out;
	}

	/* DVFSRC need to wait at least 2T(~196ns) to handle request
	 * after recieving command
	 */
	udelay(STARTUP_TIME);

	ret = readx_poll_timeout(dvfsrc_is_idle, dvfsrc,
				 state, state == DVFSRC_IDLE,
				 STARTUP_TIME, POLL_TIMEOUT);

	if (ret < 0) {
		dev_warn(dvfsrc->dev,
			 "%d: idle timeout, data: %llu, last: %d -> %d\n",
			 cmd, data,
			 dvfsrc->dvd->get_current_level(dvfsrc),
			 dvfsrc->dvd->get_target_level(dvfsrc));
		goto out;
	}

	if (cmd == MTK_DVFSRC_CMD_OPP_REQUEST)
		dvfsrc->dvd->wait_for_opp_level(dvfsrc, data);
	else
		dvfsrc->dvd->wait_for_vcore_level(dvfsrc, data);

out:
	mutex_unlock(&dvfsrc->req_lock);
}
EXPORT_SYMBOL(mtk_dvfsrc_send_request);

int mtk_dvfsrc_query_info(const struct device *dev, u32 cmd, int *data)
{
	struct mtk_dvfsrc *dvfsrc = dev_get_drvdata(dev);

	switch (cmd) {
	case MTK_DVFSRC_CMD_VCORE_QUERY:
		*data = dvfsrc->dvd->get_vcore_level(dvfsrc);
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

	d = dvfsrc->dvd->domains;

	if (pstate > dvfsrc->dvd->num_opp) {
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

	dvfsrc->clk_dvfsrc = devm_clk_get(dvfsrc->dev, "dvfsrc");
	if (IS_ERR(dvfsrc->clk_dvfsrc)) {
		dev_err(dvfsrc->dev, "failed to get clock: %ld\n",
			PTR_ERR(dvfsrc->clk_dvfsrc));
		return PTR_ERR(dvfsrc->clk_dvfsrc);
	}

	ret = clk_prepare_enable(dvfsrc->clk_dvfsrc);
	if (ret)
		return ret;

	mutex_init(&dvfsrc->req_lock);
	mutex_init(&dvfsrc->pstate_lock);

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, MTK_SIP_DVFSRC_INIT, 0, 0, 0,
		0, 0, 0, &ares);

	if (!ares.a0) {
		dvfsrc->dram_type = ares.a1;
		dev_info(dvfsrc->dev, "dram_type: %d\n", dvfsrc->dram_type);
	} else {
		dev_err(dvfsrc->dev, "init fails: %lu\n", ares.a0);
		clk_disable_unprepare(dvfsrc->clk_dvfsrc);
		return ares.a0;
	}

	platform_set_drvdata(pdev, dvfsrc);
	pstate_notifier_register(dvfsrc);

	return devm_of_platform_populate(&pdev->dev);
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
	.num_domains = 0,
	.get_target_level = mt8183_get_target_level,
	.get_current_level = mt8183_get_current_level,
	.get_vcore_level = mt8183_get_vcore_level,
	.set_dram_bw = mt8183_set_dram_bw,
	.set_opp_level = mt8183_set_opp_level,
	.set_vcore_level = mt8183_set_vcore_level,
	.wait_for_opp_level = mt8183_wait_for_opp_level,
	.wait_for_vcore_level = dvfsrc_wait_for_vcore_level,
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
