// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015-2020 MediaTek Inc.
 */
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <soc/mediatek/smi.h>
#include <dt-bindings/memory/mt2701-larb-port.h>
/* mt8173 */
#define SMI_LARB_MMU_EN		0xf00
#define SMI_LARB_MMU_EN_MT8167		0xfc0
/* mt2701 */
#define REG_SMI_SECUR_CON_BASE		0x5c0
/* every register control 8 port, register offset 0x4 */
#define REG_SMI_SECUR_CON_OFFSET(id)	(((id) >> 3) << 2)
#define REG_SMI_SECUR_CON_ADDR(id)	\
	(REG_SMI_SECUR_CON_BASE + REG_SMI_SECUR_CON_OFFSET(id))
/*
 * every port have 4 bit to control, bit[port + 3] control virtual or physical,
 * bit[port + 2 : port + 1] control the domain, bit[port] control the security
 * or non-security.
 */
#define SMI_SECUR_CON_VAL_MSK(id)	(~(0xf << (((id) & 0x7) << 2)))
#define SMI_SECUR_CON_VAL_VIRT(id)	BIT((((id) & 0x7) << 2) + 3)
/* mt2701 domain should be set to 3 */
#define SMI_SECUR_CON_VAL_DOMAIN(id)	(0x3 << ((((id) & 0x7) << 2) + 1))
/* mt2712 */
#define SMI_LARB_NONSEC_CON(id)	(0x380 + ((id) * 4))
#define F_MMU_EN		BIT(0)
#define SMI_LARB_SLP_CON		0x00c
#define SLP_PROT_EN			BIT(0)
#define SLP_PROT_RDY			BIT(16)
/* SMI COMMON */
#define SMI_BUS_SEL			0x220
#define SMI_BUS_LARB_SHIFT(larbid)	((larbid) << 1)
/* COUNT PROBE */
int count_number = 1;
int smi_dev_number = 1;
/* All are MMU0 defaultly. Only specialize mmu1 here. */
#define F_MMU1_LARB(larbid)		(0x1 << SMI_BUS_LARB_SHIFT(larbid))
#if !IS_ENABLED(CONFIG_MTK_SMI_EXT)
enum mtk_smi_gen {
	MTK_SMI_GEN1,
	MTK_SMI_GEN2
};
struct mtk_smi_common_plat {
	enum mtk_smi_gen gen;
	/* Adjust some larbs to mmu1 to balance the bandwidth */
	unsigned int bus_sel;
};
struct mtk_smi_larb_gen {
	bool need_larbid;
	int port_in_larb[MTK_LARB_NR_MAX + 1];
	void (*config_port)(struct device *);
	void (*larb_sleep_ctrl)(struct device *dev, bool toslp);
};
struct mtk_smi {
	struct device			*dev;
	struct clk			*clk_apb, *clk_smi;
	struct clk			*clk_gals0, *clk_gals1;
	struct clk			*clk_async; /*only needed by mt2701*/
	void __iomem			*smi_ao_base; /* only for gen1 */
	void __iomem			*base;	      /* only for gen2 */
	const struct mtk_smi_common_plat *plat;
};
struct mtk_smi_larb { /* larb: local arbiter */
	struct mtk_smi			smi;
	void __iomem			*base;
	struct device			*smi_common_dev;
	const struct mtk_smi_larb_gen	*larb_gen;
	int				larbid;
	u32				*mmu;
};
static int mtk_smi_clk_enable(const struct mtk_smi *smi)
{
	int ret;
	ret = clk_prepare_enable(smi->clk_apb);
	if (ret)
		return ret;
	ret = clk_prepare_enable(smi->clk_smi);
	if (ret)
		goto err_disable_apb;
	ret = clk_prepare_enable(smi->clk_gals0);
	if (ret)
		goto err_disable_smi;
	ret = clk_prepare_enable(smi->clk_gals1);
	if (ret)
		goto err_disable_gals0;
	return 0;
err_disable_gals0:
	clk_disable_unprepare(smi->clk_gals0);
err_disable_smi:
	clk_disable_unprepare(smi->clk_smi);
err_disable_apb:
	clk_disable_unprepare(smi->clk_apb);
	return ret;
}
static void mtk_smi_clk_disable(const struct mtk_smi *smi)
{
	clk_disable_unprepare(smi->clk_gals1);
	clk_disable_unprepare(smi->clk_gals0);
	clk_disable_unprepare(smi->clk_smi);
	clk_disable_unprepare(smi->clk_apb);
}
static int
mtk_smi_larb_bind(struct device *dev, struct device *master, void *data)
{
	struct mtk_smi_larb *larb = dev_get_drvdata(dev);
	struct mtk_smi_iommu *smi_iommu = data;
	unsigned int         i;
	if (larb->larb_gen->need_larbid) {
		larb->mmu = &smi_iommu->larb_imu[larb->larbid].mmu;
		return 0;
	}
	/*
	 * If there is no larbid property, Loop to find the corresponding
	 * iommu information.
	 */
	for (i = 0; i < smi_iommu->larb_nr; i++) {
		if (dev == smi_iommu->larb_imu[i].dev) {
			/* The 'mmu' may be updated in iommu-attach/detach. */
			larb->mmu = &smi_iommu->larb_imu[i].mmu;
			return 0;
		}
	}
	return -ENODEV;
}
static void mtk_smi_larb_config_port_gen2_general(struct device *dev)
{
	struct mtk_smi_larb *larb = dev_get_drvdata(dev);
	u32 reg;
	int i;
	for_each_set_bit(i, (unsigned long *)larb->mmu, 32) {
		reg = readl_relaxed(larb->base + SMI_LARB_NONSEC_CON(i));
		reg |= F_MMU_EN;
		writel(reg, larb->base + SMI_LARB_NONSEC_CON(i));
	}
}
static void mtk_smi_larb_config_port_mt2712(struct device *dev)
{
	struct mtk_smi_larb *larb = dev_get_drvdata(dev);
	/*
	 * larb 8/9 is the bdpsys larb, the iommu_en is enabled defaultly.
	 * Don't need to set it again.
	 */
	if (larb->larbid == 8 || larb->larbid == 9)
		return;
	mtk_smi_larb_config_port_gen2_general(dev);
}
static void mtk_smi_larb_config_port_mt8173(struct device *dev)
{
	struct mtk_smi_larb *larb = dev_get_drvdata(dev);
	writel(*larb->mmu, larb->base + SMI_LARB_MMU_EN);
}
static void mtk_smi_larb_config_port_mt8167(struct device *dev)
{
	struct mtk_smi_larb *larb = dev_get_drvdata(dev);

	writel(*larb->mmu, larb->base + SMI_LARB_MMU_EN_MT8167);
}
static void mtk_smi_larb_config_port_gen1(struct device *dev)
{
	struct mtk_smi_larb *larb = dev_get_drvdata(dev);
	const struct mtk_smi_larb_gen *larb_gen = larb->larb_gen;
	struct mtk_smi *common = dev_get_drvdata(larb->smi_common_dev);
	int i, m4u_port_id, larb_port_num;
	u32 sec_con_val, reg_val;
	m4u_port_id = larb_gen->port_in_larb[larb->larbid];
	larb_port_num = larb_gen->port_in_larb[larb->larbid + 1]
			- larb_gen->port_in_larb[larb->larbid];
	for (i = 0; i < larb_port_num; i++, m4u_port_id++) {
		if (*larb->mmu & BIT(i)) {
			/* bit[port + 3] controls the virtual or physical */
			sec_con_val = SMI_SECUR_CON_VAL_VIRT(m4u_port_id);
		} else {
			/* do not need to enable m4u for this port */
			continue;
		}
		reg_val = readl(common->smi_ao_base
			+ REG_SMI_SECUR_CON_ADDR(m4u_port_id));
		reg_val &= SMI_SECUR_CON_VAL_MSK(m4u_port_id);
		reg_val |= sec_con_val;
		reg_val |= SMI_SECUR_CON_VAL_DOMAIN(m4u_port_id);
		writel(reg_val,
			common->smi_ao_base
			+ REG_SMI_SECUR_CON_ADDR(m4u_port_id));
	}
}
static void mtk_smi_larb_sleep_ctrl_mt8168(struct device *dev, bool toslp)
{
	struct mtk_smi_larb *larb = dev_get_drvdata(dev);
	void __iomem *base = larb->base;
	u32 tmp;
	/* larb4 should be use a general way. */
	if (larb->larbid == 4)
		return;
	if (toslp) {
		writel_relaxed(SLP_PROT_EN, base + SMI_LARB_SLP_CON);
		if (readl_poll_timeout_atomic(base + SMI_LARB_SLP_CON,
				tmp, !!(tmp & SLP_PROT_RDY), 10, 10000))
			dev_notice(dev, "larb sleep con not ready(%d)\n", tmp);
	} else
		writel_relaxed(0, base + SMI_LARB_SLP_CON);
}
static void
mtk_smi_larb_unbind(struct device *dev, struct device *master, void *data)
{
	/* Do nothing as the iommu is always enabled. */
}
static const struct component_ops mtk_smi_larb_component_ops = {
	.bind = mtk_smi_larb_bind,
	.unbind = mtk_smi_larb_unbind,
};
static const struct mtk_smi_larb_gen mtk_smi_larb_mt8173 = {
	/* mt8173 do not need the port in larb */
	.config_port = mtk_smi_larb_config_port_mt8173,
};
static const struct mtk_smi_larb_gen mtk_smi_larb_mt8167 = {
	.config_port = mtk_smi_larb_config_port_mt8167,
};
static const struct mtk_smi_larb_gen mtk_smi_larb_mt2701 = {
	.need_larbid = true,
	.port_in_larb = {
		LARB0_PORT_OFFSET, LARB1_PORT_OFFSET,
		LARB2_PORT_OFFSET, LARB3_PORT_OFFSET
	},
	.config_port = mtk_smi_larb_config_port_gen1,
};
static const struct mtk_smi_larb_gen mtk_smi_larb_mt2712 = {
	.need_larbid = true,
	.config_port = mtk_smi_larb_config_port_mt2712,
};
static const struct mtk_smi_larb_gen mtk_smi_larb_mt8183 = {
	.config_port = mtk_smi_larb_config_port_gen2_general,
};
static const struct mtk_smi_larb_gen mtk_smi_larb_mt8168 = {
	.config_port = mtk_smi_larb_config_port_gen2_general,
	.larb_sleep_ctrl = mtk_smi_larb_sleep_ctrl_mt8168,
};
static const struct of_device_id mtk_smi_larb_of_ids[] = {
	{
		.compatible = "mediatek,mt8173-smi-larb",
		.data = &mtk_smi_larb_mt8173
	},
	{
		.compatible = "mediatek,mt8167-smi-larb",
		.data = &mtk_smi_larb_mt8167
	},
	{
		.compatible = "mediatek,mt2701-smi-larb",
		.data = &mtk_smi_larb_mt2701
	},
	{
		.compatible = "mediatek,mt2712-smi-larb",
		.data = &mtk_smi_larb_mt2712
	},
	{
		.compatible = "mediatek,mt8183-smi-larb",
		.data = &mtk_smi_larb_mt8183
	},
	{
		.compatible = "mediatek,mt8168-smi-larb",
		.data = &mtk_smi_larb_mt8168
	},
	{}
};


static int mtk_smi_larb_probe(struct platform_device *pdev)
{
	struct mtk_smi_larb *larb;
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct device_node *smi_node;
	struct platform_device *smi_pdev;
	int err;

	larb = devm_kzalloc(dev, sizeof(*larb), GFP_KERNEL);
	if (!larb)
		return -ENOMEM;

	larb->larb_gen = of_device_get_match_data(dev);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	larb->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(larb->base))
		return PTR_ERR(larb->base);

	larb->smi.clk_apb = devm_clk_get(dev, "apb");
	if (IS_ERR(larb->smi.clk_apb))
		return PTR_ERR(larb->smi.clk_apb);

	larb->smi.clk_smi = devm_clk_get(dev, "smi");
	if (IS_ERR(larb->smi.clk_smi))
		return PTR_ERR(larb->smi.clk_smi);

	larb->smi.clk_gals0 = devm_clk_get(dev, "gals");
	if (PTR_ERR(larb->smi.clk_gals0) == -ENOENT)
		larb->smi.clk_gals0 = NULL;
	else if (IS_ERR(larb->smi.clk_gals0))
		return PTR_ERR(larb->smi.clk_gals0);

	larb->smi.dev = dev;
	if (larb->larb_gen->need_larbid) {
		err = of_property_read_u32(dev->of_node, "mediatek,larb-id",
					   &larb->larbid);
		if (err) {
			dev_notice(dev, "missing larbid property\n");
			return err;
		}
	}
	smi_node = of_parse_phandle(dev->of_node, "mediatek,smi", 0);
	if (!smi_node)
		return -EINVAL;

	smi_pdev = of_find_device_by_node(smi_node);
	of_node_put(smi_node);
	if (smi_pdev) {
		if (!platform_get_drvdata(smi_pdev))
			return -EPROBE_DEFER;
		larb->smi_common_dev = &smi_pdev->dev;
	} else {
		dev_notice(dev, "Failed to get the smi_common device\n");
		return -EINVAL;
	}
	pm_runtime_enable(dev);
	platform_set_drvdata(pdev, larb);

	return component_add(dev, &mtk_smi_larb_component_ops);
}
static int mtk_smi_larb_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	component_del(&pdev->dev, &mtk_smi_larb_component_ops);
	return 0;
}
static int __maybe_unused mtk_smi_larb_resume(struct device *dev)
{
	struct mtk_smi_larb *larb = dev_get_drvdata(dev);
	const struct mtk_smi_larb_gen *larb_gen = larb->larb_gen;
	int ret;
	/* Power on smi-common. */
	ret = pm_runtime_get_sync(larb->smi_common_dev);
	if (ret < 0) {
		dev_notice(dev, "smi-common pm get failed(%d).\n", ret);
		return ret;
	}
	ret = mtk_smi_clk_enable(&larb->smi);
	if (ret < 0) {
		dev_notice(dev, "larb clk enable failed(%d).\n", ret);
		pm_runtime_put_sync(larb->smi_common_dev);
		return ret;
	}
	if (larb_gen->larb_sleep_ctrl)
		larb_gen->larb_sleep_ctrl(dev, false);
	/* Configure the basic setting for this larb */
	larb_gen->config_port(dev);
	return 0;
}
static int __maybe_unused mtk_smi_larb_suspend(struct device *dev)
{
	struct mtk_smi_larb *larb = dev_get_drvdata(dev);
	const struct mtk_smi_larb_gen *larb_gen = larb->larb_gen;

	if (larb_gen->larb_sleep_ctrl)
		larb_gen->larb_sleep_ctrl(dev, true);
	mtk_smi_clk_disable(&larb->smi);
	pm_runtime_put_sync(larb->smi_common_dev);
	return 0;
}
static const struct dev_pm_ops smi_larb_pm_ops = {
	SET_RUNTIME_PM_OPS(mtk_smi_larb_suspend, mtk_smi_larb_resume, NULL)
};
static struct platform_driver mtk_smi_larb_driver = {
	.probe	= mtk_smi_larb_probe,
	.remove	= mtk_smi_larb_remove,
	.driver	= {
		.name = "mtk-smi-larb",
		.of_match_table = mtk_smi_larb_of_ids,
		.pm             = &smi_larb_pm_ops,
	}
};
static const struct mtk_smi_common_plat mtk_smi_common_gen1 = {
	.gen = MTK_SMI_GEN1,
};
static const struct mtk_smi_common_plat mtk_smi_common_gen2 = {
	.gen = MTK_SMI_GEN2,
};
static const struct mtk_smi_common_plat mtk_smi_common_mt8183 = {
	.gen = MTK_SMI_GEN2,
	.bus_sel = F_MMU1_LARB(1) | F_MMU1_LARB(3) | F_MMU1_LARB(4) |
		   F_MMU1_LARB(7),
};
static const struct of_device_id mtk_smi_common_of_ids[] = {
	{
		.compatible = "mediatek,mt8173-smi-common",
		.data = &mtk_smi_common_gen2,
	},
	{
		.compatible = "mediatek,mt8167-smi-common",
		.data = &mtk_smi_common_gen2,
	},
	{
		.compatible = "mediatek,mt2701-smi-common",
		.data = &mtk_smi_common_gen1,
	},
	{
		.compatible = "mediatek,mt2712-smi-common",
		.data = &mtk_smi_common_gen2,
	},
	{
		.compatible = "mediatek,mt8168-smi-common",
		.data = &mtk_smi_common_gen2,
	},
	{
		.compatible = "mediatek,mt8183-smi-common",
		.data = &mtk_smi_common_mt8183,
	},
	{}
};
#ifdef CONFIG_MACH_MT8167
static struct mtk_smi *gmtk_common_dev;
#endif

static int mtk_smi_common_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_smi *common;
	struct resource *res;
	int ret;

	common = devm_kzalloc(dev, sizeof(*common), GFP_KERNEL);
	if (!common)
		return -ENOMEM;

	common->dev = dev;
	common->plat = of_device_get_match_data(dev);
	common->clk_apb = devm_clk_get(dev, "apb");
	if (IS_ERR(common->clk_apb))
		return PTR_ERR(common->clk_apb);

	common->clk_smi = devm_clk_get(dev, "smi");
	if (IS_ERR(common->clk_smi))
		return PTR_ERR(common->clk_smi);

	common->clk_gals0 = devm_clk_get(dev, "gals0");
	if (PTR_ERR(common->clk_gals0) == -ENOENT)
		common->clk_gals0 = NULL;
	else if (IS_ERR(common->clk_gals0))
		return PTR_ERR(common->clk_gals0);

	common->clk_gals1 = devm_clk_get(dev, "gals1");
	if (PTR_ERR(common->clk_gals1) == -ENOENT)
		common->clk_gals1 = NULL;
	else if (IS_ERR(common->clk_gals1))
		return PTR_ERR(common->clk_gals1);

	/*
	 * for mtk smi gen 1, we need to get the ao(always on) base to config
	 * m4u port, and we need to enable the aync clock for transform the smi
	 * clock into emi clock domain, but for mtk smi gen2, there's no smi ao
	 * base.
	 */
	if (common->plat->gen == MTK_SMI_GEN1) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		common->smi_ao_base = devm_ioremap_resource(dev, res);
		if (IS_ERR(common->smi_ao_base))
			return PTR_ERR(common->smi_ao_base);

		common->clk_async = devm_clk_get(dev, "async");
		if (IS_ERR(common->clk_async))
			return PTR_ERR(common->clk_async);

		ret = clk_prepare_enable(common->clk_async);
		if (ret)
			return ret;

	} else {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		common->base = devm_ioremap_resource(dev, res);
		if (IS_ERR(common->base))
			return PTR_ERR(common->base);

	}
	pm_runtime_enable(dev);
	platform_set_drvdata(pdev, common);
#ifdef CONFIG_MACH_MT8167
	/*
	 * Without pm_runtime_get_sync(dev), the disp power domain
	 * would be turn off after pm_runtime_enable, meanwhile disp
	 * hw are still access register, this would cause system
	 * abnormal.
	 *
	 * If we do not call pm_runtime_get_sync, then system would hang
	 * in larb0's power domain attach, power domain SA and DE are
	 * still checking that. We would like to bypass this first and
	 * don't block the software flow.
	 */
	pm_runtime_get_sync(dev);
	gmtk_common_dev = common;
#endif
	return 0;
}
static int mtk_smi_common_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	return 0;
}
static int __maybe_unused mtk_smi_common_resume(struct device *dev)
{
	struct mtk_smi *common = dev_get_drvdata(dev);
	unsigned int bus_sel = common->plat->bus_sel;
	int ret;
	ret = mtk_smi_clk_enable(common);
	if (ret)
		return ret;
	if (common->plat->gen == MTK_SMI_GEN2 && bus_sel)
		writel(bus_sel, common->base + SMI_BUS_SEL);
	return 0;
}
static int __maybe_unused mtk_smi_common_suspend(struct device *dev)
{
	struct mtk_smi *common = dev_get_drvdata(dev);
	mtk_smi_clk_disable(common);
	return 0;
}
static const struct dev_pm_ops smi_common_pm_ops = {
	SET_RUNTIME_PM_OPS(mtk_smi_common_suspend, mtk_smi_common_resume, NULL)
};
static struct platform_driver mtk_smi_common_driver = {
	.probe	= mtk_smi_common_probe,
	.remove = mtk_smi_common_remove,
	.driver	= {
		.name = "mtk-smi-common",
		.of_match_table = mtk_smi_common_of_ids,
		.pm             = &smi_common_pm_ops,
	}
};
#ifdef CONFIG_MACH_MT8167
/* put the disp power domain that we got in smi probe */
static int __init mtk_smi_init_late(void)
{
	pm_runtime_put_sync(gmtk_common_dev->dev);
	return 0;
}
#endif
#else /* IS_ENABLED(CONFIG_MTK_SMI_EXT) */
#include <linux/of_address.h>
static u32 nr_larbs, nr_dev;
static struct mtk_smi_dev **smi_dev;
s32 mtk_smi_clk_enable(struct mtk_smi_dev *smi)
{
	s32 i, j, ret = 0;

	if (!smi) {
		pr_info("No such device or address\n");
		return -ENXIO;
	} else if (!smi->dev || !smi->clks) {
		pr_info("SMI%u no such device or address\n", smi->id);
		return -ENXIO;
	}
	for (i = 1; i < smi->nr_clks; i++) { /* without MTCMOS */
		ret = clk_prepare_enable(smi->clks[i]);
		if (ret) {
			dev_info(smi->dev, "SMI%u CLK%d enable failed:%d\n",
				smi->id, i, ret);
			for (j = i - 1; j > 0; j--)
				clk_disable_unprepare(smi->clks[j]);
			return ret;
		}
	}
	atomic_inc(&(smi->clk_cnts));
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_smi_clk_enable);
void mtk_smi_clk_disable(struct mtk_smi_dev *smi)
{
	s32 i;

	if (!smi)
		pr_info("No such device or address\n");
	else if (!smi->dev || !smi->clks)
		pr_info("SMI%u no such device or address\n", smi->id);
	else {
		atomic_dec(&(smi->clk_cnts));
		for (i = smi->nr_clks - 1; i > 0; i--)
			clk_disable_unprepare(smi->clks[i]);
	}
}
EXPORT_SYMBOL_GPL(mtk_smi_clk_disable);
struct mtk_smi_dev *mtk_smi_dev_get(const u32 id)
{
	if (id > nr_dev)
		pr_info("Invalid id: %u, nr_dev=%u\n", id, nr_dev);
	else if (!smi_dev[id])
		pr_info("SMI%u no such device or address\n", id);
	else
		return smi_dev[id];
	return NULL;
}
EXPORT_SYMBOL_GPL(mtk_smi_dev_get);
s32 mtk_smi_conf_set(const struct mtk_smi_dev *smi, const u32 scen_id)
{
	u32 cnts, i;

	if (!smi) {
		pr_info("No such device or address\n");
		return -ENXIO;
	} else if (!smi->dev) {
		pr_info("SMI%u no such device or address\n", smi->id);
		return -ENXIO;
	}
	cnts = atomic_read(&(smi->clk_cnts));
	if (cnts <= 0) {
		dev_dbg(smi->dev, "SMI%u without MTCMOS: %d\n", smi->id, cnts);
		return cnts;
	}
	for (i = 0; i < smi->nr_conf_pairs; i++) /* conf */
		writel(smi->conf_pairs[i].val,
			smi->base + smi->conf_pairs[i].off);
	for (i = 0; i < smi->nr_scen_pairs; i++) /* scen */
		writel(smi->scen_pairs[scen_id][i].val,
			smi->base + smi->scen_pairs[scen_id][i].off);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_smi_conf_set);
static s32 mtk_smi_clks_get(struct mtk_smi_dev *smi)
{
	struct property *prop;
	const char *name, *clk_names = "clock-names";
	s32 i = 0, ret;

	if (!smi) {
		pr_info("No such device or address\n");
		return -ENXIO;
	} else if (!smi->dev) {
		pr_info("SMI%u no such device or address\n", smi->id);
		return -ENXIO;
	}
	ret = of_property_count_strings(smi->dev->of_node, clk_names);
	if (ret < 0)
		return ret;
	smi->nr_clks = (u32)ret;
	smi->clks = devm_kcalloc(smi->dev, smi->nr_clks, sizeof(*smi->clks),
		GFP_KERNEL);
	if (!smi->clks)
		return -ENOMEM;
	of_property_for_each_string(smi->dev->of_node, clk_names, prop, name) {

		smi->clks[i] = devm_clk_get(smi->dev, name);
		if (IS_ERR(smi->clks[i])) {
			dev_info(smi->dev, "SMI%u CLK%d:%s get failed, err:%x\n",
				smi->id, i, name, IS_ERR(smi->clks[i]));
			break;
		}
		dev_info(smi->dev, "SMI%u CLK%d:%s\n", smi->id, i, name);
		i += 1;
	}
	if (i < smi->nr_clks)
		return PTR_ERR(smi->clks[i]);
	atomic_set(&(smi->clk_cnts), 0);

	if (count_number == smi_dev_number+1) {
		ret = smi_register();
		if (ret)
			pr_info("Failed to register SMI_EXT driver\n");
	} else if (count_number > smi_dev_number) {
		pr_info("SMI probe too much\n");
	}

	return 0;


}
static int mtk_smi_dev_probe(struct platform_device *pdev, const u32 id)
{
	struct resource *res;
	void __iomem *base;


	if (id > nr_dev) {
		dev_dbg(&pdev->dev,
			"Invalid id:%u, nr_dev=%u\n", id, nr_dev);
		return -EINVAL;
	}
	smi_dev[id] =
		devm_kzalloc(&pdev->dev, sizeof(*smi_dev[id]), GFP_KERNEL);
	if (!smi_dev[id])
		return -ENOMEM;
	smi_dev[id]->id = id;
	smi_dev[id]->dev = &pdev->dev;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(smi_dev[id]->dev, res);
	if (IS_ERR(base)) {
		dev_info(&pdev->dev, "SMI%u base:%p read failed\n",
			id, base);
		return PTR_ERR(base);
	}
	smi_dev[id]->base = base;
	if (of_address_to_resource(smi_dev[id]->dev->of_node, 0, res))
		return -EINVAL;

	dev_info(&pdev->dev,
		"SMI%u base: VA=%p, PA=%pa\n", id, base, &res->start);
	platform_set_drvdata(pdev, smi_dev[id]);
	return mtk_smi_clks_get(smi_dev[id]);
}

static int mtk_smi_larb_probe(struct platform_device *pdev)
{
	u32 id = 0;
	s32 ret;

	if (!pdev) {
		pr_notice("platform_device missed\n");
		return -ENODEV;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "mediatek,smi-id", &id);
	if (ret) {
		dev_info(&pdev->dev, "LARB read failed:%d\n", ret);
		return ret;
	}
	count_number = count_number + 1;

	return mtk_smi_dev_probe(pdev, id);


}
static int mtk_smi_common_probe(struct platform_device *pdev)
{
	u32 id = 0, cnt = 0;
	s32 ret;

	if (count_number == 1)
		smi_dev_number = smi_get_dev_num();

	if (!pdev) {
		pr_notice("platform_device missed\n");
		return -ENODEV;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "mediatek,smi-id", &id);
	if (ret) {
		dev_info(&pdev->dev, "COMMON read failed:%d\n", ret);
		return ret;
	}
	nr_larbs = id;
	ret = of_property_read_u32(pdev->dev.of_node, "mediatek,smi-cnt", &cnt);
	if (ret)
		dev_dbg(&pdev->dev, "COMMON%u read all:%d failed:%d\n",
			nr_larbs, cnt, ret);
	if (!smi_dev) {
		nr_dev = cnt ? cnt : (id + 1);
		smi_dev = devm_kcalloc(
			&pdev->dev, nr_dev, sizeof(*smi_dev), GFP_KERNEL);
		dev_info(&pdev->dev, "COMMON%u nr_dev:%u\n", id, nr_dev);
	}
	if (!smi_dev)
		return -ENOMEM;

	count_number = count_number + 1;
	return mtk_smi_dev_probe(pdev, id);
}


static const struct of_device_id mtk_smi_larb_of_ids[] = {
	{
		.compatible = "mediatek,smi_larb",
	},
	{}
};
static const struct of_device_id mtk_smi_common_of_ids[] = {
	{
		.compatible = "mediatek,smi_common",
	},
	{
		.compatible = "mediatek,smi_sub_common",
	},
	{}
};

static struct platform_driver mtk_smi_larb_driver = {
	.probe	= mtk_smi_larb_probe,
	.driver	= {
		.name = "mtk-smi-larb",
		.of_match_table = mtk_smi_larb_of_ids,
	}
};
static struct platform_driver mtk_smi_common_driver = {
	.probe	= mtk_smi_common_probe,
	.driver	= {
		.name = "mtk-smi-common",
		.of_match_table = mtk_smi_common_of_ids,
	}
};


#endif /* IS_ENABLED(CONFIG_MTK_SMI_EXT) */
static int __init mtk_smi_init(void)
{
	int ret;

	ret = platform_driver_register(&mtk_smi_common_driver);
	if (ret != 0) {
		pr_err("Failed to register SMI driver\n");
		return ret;
	}
	ret = platform_driver_register(&mtk_smi_larb_driver);
	if (ret != 0) {
		pr_err("Failed to register SMI-LARB driver\n");
		goto err_unreg_smi;
	}

	return ret;
err_unreg_smi:
	platform_driver_unregister(&mtk_smi_common_driver);
	return ret;
}

#if !IS_ENABLED(CONFIG_MTK_SMI_EXT)
	module_init(mtk_smi_init);
	#ifdef CONFIG_MACH_MT8167
		late_initcall(mtk_smi_init_late);
	#endif
#else
	#if (defined(CONFIG_MACH_MT6833) || defined(CONFIG_MACH_MT6893) || defined(CONFIG_MACH_MT6885))
		arch_initcall_sync(mtk_smi_init);
	#else
		arch_initcall(mtk_smi_init);
	#endif
#endif
MODULE_DESCRIPTION("MediaTek SMI driver");
MODULE_LICENSE("GPL v2");
