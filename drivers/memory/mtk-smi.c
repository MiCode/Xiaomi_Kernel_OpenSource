/*
 * Copyright (c) 2015-2016 MediaTek Inc.
 * Author: Yong Wu <yong.wu@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <soc/mediatek/smi.h>
#if !IS_ENABLED(CONFIG_MTK_SMI_EXT)
#include <dt-bindings/memory/mt2701-larb-port.h>
#endif

/* mt8173 */
#define SMI_LARB_MMU_EN		0xf00

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

struct mtk_smi_larb_gen {
	bool need_larbid;
	int port_in_larb[MTK_LARB_NR_MAX + 1];
	void (*config_port)(struct device *);
};

struct mtk_smi {
	struct device			*dev;
	struct clk			*clk_apb, *clk_smi;
	struct clk			*clk_async; /* only needed by mt2701 */
	void __iomem			*smi_ao_base;
};

struct mtk_smi_larb { /* larb: local arbiter */
	struct mtk_smi			smi;
	void __iomem			*base;
	struct device			*smi_common_dev;
	const struct mtk_smi_larb_gen	*larb_gen;
	int				larbid;
	u32				*mmu;
};

enum mtk_smi_gen {
	MTK_SMI_GEN1,
	MTK_SMI_GEN2
};

#if IS_ENABLED(CONFIG_MTK_SMI_EXT)
struct mtk_smi_dev *common;
struct mtk_smi_dev **larbs;

int mtk_smi_clk_ref_cnts_read(struct mtk_smi_dev *smi)
{
	/* check parameter */
	if (!smi) {
		pr_info("no such device or address\n");
		return -ENXIO;
	} else if (!smi->dev) {
		pr_info("%s %d no such device or address\n",
			smi->index == common->index ? "common" : "larb",
			smi->index);
		return -ENXIO;
	}
	/* read reference counts */
	return (int)atomic_read(&(smi->clk_ref_cnts));
}
EXPORT_SYMBOL_GPL(mtk_smi_clk_ref_cnts_read);

int mtk_smi_dev_enable(struct mtk_smi_dev *smi)
{
	int	i, j, ret;
	/* check parameter */
	if (!smi) {
		pr_info("no such device or address\n");
		return -ENXIO;
	} else if (!smi->dev || !smi->clks) {
		pr_info("%s %d no such device or address\n",
			smi->index == common->index ? "common" : "larb",
			smi->index);
		return -ENXIO;
	}
	/* enable clocks without mtcmos */
	for (i = 1; i < smi->nr_clks; i++) {
		ret = clk_prepare_enable(smi->clks[i]);
		if (ret) {
			dev_notice(smi->dev, "%s %d clk %d enable failed %d\n",
				smi->index == common->index ? "common" : "larb",
				smi->index, i, ret);
			break;
		}
	}
	/* rollback */
	if (ret)
		for (j = i - 1; j >= 0; j--)
			clk_disable_unprepare(smi->clks[j]);
	atomic_inc(&(smi->clk_ref_cnts));
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_smi_dev_enable);

int mtk_smi_dev_disable(struct mtk_smi_dev *smi)
{
	int	i;
	/* check parameter */
	if (!smi) {
		pr_info("no such device or address\n");
		return -ENXIO;
	} else if (!smi->dev || !smi->clks) {
		pr_info("%s %d no such device or address\n",
			smi->index == common->index ? "common" : "larb",
			smi->index);
		return -ENXIO;
	}
	atomic_dec(&(smi->clk_ref_cnts));
	/* disable clocks without mtcmos */
	for (i = smi->nr_clks - 1; i >= 1; i--)
		clk_disable_unprepare(smi->clks[i]);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_smi_dev_disable);

static int mtk_smi_clks_get(struct mtk_smi_dev *smi)
{
	struct property	*prop;
	const char	*name, *clk_names = "clock-names";
	int		nr_clks = 0, i = 0, ret = 0;
	/* check parameter */
	if (!smi) {
		pr_info("no such device or address\n");
		return -ENXIO;
	} else if (!smi->dev) {
		pr_info("%s %d no such device or address\n",
			smi->index == common->index ? "common" : "larb",
			smi->index);
		return -ENXIO;
	}
	/* count number of clocks */
	nr_clks = of_property_count_strings(smi->dev->of_node, clk_names);
	if (nr_clks <= 0)
		return 0;
	smi->nr_clks = nr_clks;
#if IS_ENABLED(CONFIG_MACH_MT6758) || IS_ENABLED(CONFIG_MACH_MT6765)
	/* workaround for mmdvfs at mt6758/mt6765 */
	if (smi->index == common->index)
		smi->nr_clks = 4;
#endif
	/* allocate and get clks */
	smi->clks = devm_kcalloc(smi->dev, smi->nr_clks, sizeof(*smi->clks),
		GFP_KERNEL);
	if (!smi->clks)
		return -ENOMEM;

	of_property_for_each_string(smi->dev->of_node, clk_names, prop, name) {
		smi->clks[i] = devm_clk_get(smi->dev, name);
		if (IS_ERR(smi->clks[i])) {
			dev_notice(smi->dev, "%s %d clks[%d]=%s get failed\n",
				smi->index == common->index ? "common" : "larb",
				smi->index, i, name);
			ret += 1;
		} else
			dev_dbg(smi->dev, "%s %d clks[%d]=%s\n",
				smi->index == common->index ? "common" : "larb",
				smi->index, i, name);
		i += 1;
		if (i == smi->nr_clks)
			break;
	}
	if (ret)
		return PTR_ERR(smi->clks);
	/* init zero for reference counts */
	atomic_set(&(smi->clk_ref_cnts), 0);
	return ret;
}

int mtk_smi_config_set(struct mtk_smi_dev *smi, const unsigned int scen_indx)
{
	static int		mmu;
	struct mtk_smi_pair	*pairs;
	unsigned int		nr_pairs;
	int			i, ret = 0;
	/* check parameter */
	if (!smi) {
		pr_info("no such device or address\n");
		return -ENXIO;
	} else if (!smi->dev || !smi->config_pairs) {
		pr_info("%s %d no such device or address\n",
			smi->index == common->index ? "common" : "larb",
			smi->index);
		return -ENXIO;
	} else if (scen_indx > smi->nr_scens) {
		dev_info(smi->dev, "%s %d invalid scen_indx %d > nr_scens %d\n",
			smi->index == common->index ? "common" : "larb",
			smi->index, scen_indx, smi->nr_scens);
		return -EINVAL;
	} else if (!mtk_smi_clk_ref_cnts_read(smi)) {
		dev_dbg(smi->dev, "%s %d without mtcmos\n",
			smi->index == common->index ? "common" : "larb",
			smi->index);
		return ret;
	}
	/* nr_pairs and pairs */
	nr_pairs = (scen_indx == smi->nr_scens) ?
		smi->nr_config_pairs : smi->nr_scen_pairs;
	if (scen_indx == smi->nr_scens && smi->config_pairs)
		pairs = smi->config_pairs;
	else if (smi->scen_pairs && smi->scen_pairs[scen_indx])
		pairs = smi->scen_pairs[scen_indx];
	else
		pairs = NULL;
	if (!nr_pairs || !pairs)
		return ret;
	/* write configs */
	for (i = 0; i < nr_pairs; i++) {
		unsigned int prev, curr;

		prev = readl(smi->base + pairs[i].offset);
		if (mmu > common->index ||
			pairs[i].offset < 0x380 || pairs[i].offset >= 0x400)
			writel(pairs[i].value, smi->base + pairs[i].offset);
		else
			continue;
		curr = readl(smi->base + pairs[i].offset);
		dev_dbg(smi->dev, "%s %d pairs[%d] %#x=%#x->%#x->%#x\n",
			smi->index == common->index ? "common" : "larb",
			smi->index, i, pairs[i].offset,
			prev, pairs[i].value, curr);
	}
	if (mmu <= common->index)
		mmu += 1;
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_smi_config_set);

static void mtk_smi_larb_config_port(struct device *dev)
{
	struct mtk_smi_dev *larb = dev_get_drvdata(dev);
	unsigned int reg;
	int i;

	for_each_set_bit(i, (unsigned long *)larb->mmu, 32) {
		reg = readl_relaxed(larb->base + SMI_LARB_NONSEC_CON(i));
		reg |= F_MMU_EN;
		writel(reg, larb->base + SMI_LARB_NONSEC_CON(i));
	}
}

#else /* !CONFIG_MTK_SMI_EXT */
static int mtk_smi_enable(const struct mtk_smi *smi)
{
	int ret;

	ret = pm_runtime_get_sync(smi->dev);
	if (ret < 0)
		return ret;

	ret = clk_prepare_enable(smi->clk_apb);
	if (ret)
		goto err_put_pm;

	ret = clk_prepare_enable(smi->clk_smi);
	if (ret)
		goto err_disable_apb;

	return 0;

err_disable_apb:
	clk_disable_unprepare(smi->clk_apb);
err_put_pm:
	pm_runtime_put_sync(smi->dev);
	return ret;
}

static void mtk_smi_disable(const struct mtk_smi *smi)
{
	clk_disable_unprepare(smi->clk_smi);
	clk_disable_unprepare(smi->clk_apb);
	pm_runtime_put_sync(smi->dev);
}
#endif

int mtk_smi_larb_get(struct device *larbdev)
{
#if IS_ENABLED(CONFIG_MTK_SMI_EXT)
	struct mtk_smi_dev *larb = dev_get_drvdata(larbdev);
	int ret;

	ret = mtk_smi_dev_enable(common);
	if (ret)
		return ret;
	ret = mtk_smi_dev_enable(larb);
	if (ret) {
		mtk_smi_dev_disable(common);
		return ret;
	}
	mtk_smi_larb_config_port(larbdev);
#else
	struct mtk_smi_larb *larb = dev_get_drvdata(larbdev);
	const struct mtk_smi_larb_gen *larb_gen = larb->larb_gen;
	struct mtk_smi *common = dev_get_drvdata(larb->smi_common_dev);
	int ret;

	/* Enable the smi-common's power and clocks */
	ret = mtk_smi_enable(common);
	if (ret)
		return ret;

	/* Enable the larb's power and clocks */
	ret = mtk_smi_enable(&larb->smi);
	if (ret) {
		mtk_smi_disable(common);
		return ret;
	}

	/* Configure the iommu info for this larb */
	larb_gen->config_port(larbdev);
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_smi_larb_get);

void mtk_smi_larb_put(struct device *larbdev)
{
#if IS_ENABLED(CONFIG_MTK_SMI_EXT)
	struct mtk_smi_dev *larb = dev_get_drvdata(larbdev);

	mtk_smi_dev_disable(larb);
	mtk_smi_dev_disable(common);
#else
	struct mtk_smi_larb *larb = dev_get_drvdata(larbdev);
	struct mtk_smi *common = dev_get_drvdata(larb->smi_common_dev);

	/*
	 * Don't de-configure the iommu info for this larb since there may be
	 * several modules in this larb.
	 * The iommu info will be reset after power off.
	 */
	mtk_smi_disable(&larb->smi);
	mtk_smi_disable(common);
#endif
}
EXPORT_SYMBOL_GPL(mtk_smi_larb_put);

static int
mtk_smi_larb_bind(struct device *dev, struct device *master, void *data)
{
#if IS_ENABLED(CONFIG_MTK_SMI_EXT)
	struct mtk_smi_dev *larb = dev_get_drvdata(dev);
	struct mtk_smi_iommu *smi_iommu = data;

	larb->mmu = &smi_iommu->larb_imu[larb->index].mmu;
	return 0;
#else
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
#endif
}

static void mtk_smi_larb_config_port_mt2712(struct device *dev)
{
	struct mtk_smi_larb *larb = dev_get_drvdata(dev);
	u32 reg;
	int i;

	/*
	 * larb 8/9 is the bdpsys larb, the iommu_en is enabled defaultly.
	 * Don't need to set it again.
	 */
	if (larb->larbid == 8 || larb->larbid == 9)
		return;

	for_each_set_bit(i, (unsigned long *)larb->mmu, 32) {
		reg = readl_relaxed(larb->base + SMI_LARB_NONSEC_CON(i));
		reg |= F_MMU_EN;
		writel(reg, larb->base + SMI_LARB_NONSEC_CON(i));
	}
}

static void mtk_smi_larb_config_port_mt8173(struct device *dev)
{
	struct mtk_smi_larb *larb = dev_get_drvdata(dev);

	writel(*larb->mmu, larb->base + SMI_LARB_MMU_EN);
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

static const struct mtk_smi_larb_gen mtk_smi_larb_mt2701 = {
	.need_larbid = true,
#if !IS_ENABLED(CONFIG_MTK_SMI_EXT)
	.port_in_larb = {
		LARB0_PORT_OFFSET, LARB1_PORT_OFFSET,
		LARB2_PORT_OFFSET, LARB3_PORT_OFFSET
	},
#endif
	.config_port = mtk_smi_larb_config_port_gen1,
};

static const struct mtk_smi_larb_gen mtk_smi_larb_mt2712 = {
	.need_larbid = true,
	.config_port = mtk_smi_larb_config_port_mt2712,
};

static const struct of_device_id mtk_smi_larb_of_ids[] = {
	{
		.compatible = "mediatek,smi_larb",
	},
	{
		.compatible = "mediatek,mt8173-smi-larb",
		.data = &mtk_smi_larb_mt8173
	},
	{
		.compatible = "mediatek,mt2701-smi-larb",
		.data = &mtk_smi_larb_mt2701
	},
	{
		.compatible = "mediatek,mt2712-smi-larb",
		.data = &mtk_smi_larb_mt2712
	},
	{}
};

static int mtk_smi_larb_probe(struct platform_device *pdev)
{
#if IS_ENABLED(CONFIG_MTK_SMI_EXT)
	struct resource	*res;
	unsigned int	index;
	int		ret;
	/* check parameter */
	if (!pdev) {
		pr_notice("platform_device larb missed\n");
		return -ENODEV;
	}
	/* index */
	ret = of_property_read_u32(pdev->dev.of_node, "cell-index", &index);
	if (ret) {
		dev_notice(&pdev->dev, "larb index %d read failed %d\n",
			index, ret);
		return ret;
	}
	/* dev */
	larbs[index] = devm_kzalloc(&pdev->dev, sizeof(**larbs), GFP_KERNEL);
	if (!larbs[index])
		return -ENOMEM;
	larbs[index]->dev = &pdev->dev;
	larbs[index]->index = index;
	/* base */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	larbs[index]->base = devm_ioremap_resource(larbs[index]->dev, res);
	if (IS_ERR(larbs[index]->base)) {
		dev_notice(&pdev->dev, "larb %d base 0x%p read failed %d\n",
			larbs[index]->index, larbs[index]->base, ret);
		return PTR_ERR(larbs[index]->base);
	}
	ret = of_address_to_resource(larbs[index]->dev->of_node, 0, res);
	dev_dbg(&pdev->dev, "larb %d base va=0x%p, pa=%pa\n",
		larbs[index]->index, larbs[index]->base, &res->start);
	/* clks */
	ret = mtk_smi_clks_get(larbs[index]);
	if (ret)
		return ret;
	/* device set driver data */
	platform_set_drvdata(pdev, larbs[index]);
	/* add component for iommu */
	ret = component_add(&pdev->dev, &mtk_smi_larb_component_ops);
	if (ret)
		return ret;
	return ret;
#else /* !CONFIG_MTK_SMI_EXT */
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
#endif /* !CONFIG_MTK_SMI_EXT */
}

static int mtk_smi_larb_remove(struct platform_device *pdev)
{
#if !IS_ENABLED(CONFIG_MTK_SMI_EXT)
	pm_runtime_disable(&pdev->dev);
#endif
	component_del(&pdev->dev, &mtk_smi_larb_component_ops);
	return 0;
}

static struct platform_driver mtk_smi_larb_driver = {
	.probe	= mtk_smi_larb_probe,
	.remove	= mtk_smi_larb_remove,
	.driver	= {
		.name = "mtk-smi-larb",
		.of_match_table = mtk_smi_larb_of_ids,
	}
};

static const struct of_device_id mtk_smi_common_of_ids[] = {
	{
		.compatible = "mediatek,smi_common",
	},
	{
		.compatible = "mediatek,mt8173-smi-common",
		.data = (void *)MTK_SMI_GEN2
	},
	{
		.compatible = "mediatek,mt2701-smi-common",
		.data = (void *)MTK_SMI_GEN1
	},
	{
		.compatible = "mediatek,mt2712-smi-common",
		.data = (void *)MTK_SMI_GEN2
	},
	{}
};

static int mtk_smi_common_probe(struct platform_device *pdev)
{
#if IS_ENABLED(CONFIG_MTK_SMI_EXT)
	struct resource	*res;
	unsigned int	nr_larbs;
	int		ret;
	/* check parameter */
	if (!pdev) {
		pr_notice("platform_device common missed\n");
		return -ENODEV;
	}
	/* dev */
	common = devm_kzalloc(&pdev->dev, sizeof(*common), GFP_KERNEL);
	if (!common)
		return -ENOMEM;
	common->dev = &pdev->dev;
	/* index */
	ret = of_property_read_u32(common->dev->of_node, "nr_larbs", &nr_larbs);
	if (ret) {
		dev_notice(&pdev->dev, "common nr_larbs %d read failed %d\n",
			nr_larbs, ret);
		return ret;
	}
	common->index = nr_larbs;
	/* base */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	common->base = devm_ioremap_resource(common->dev, res);
	if (IS_ERR(common->base)) {
		dev_notice(&pdev->dev, "common %d base 0x%p read failed %d\n",
			common->index, common->base, ret);
		return PTR_ERR(common->base);
	}
	ret = of_address_to_resource(common->dev->of_node, 0, res);
	dev_dbg(&pdev->dev, "common %d base va=0x%p, pa=%pa\n",
		common->index, common->base, &res->start);
	/* clks */
	ret = mtk_smi_clks_get(common);
	if (ret)
		return ret;
	/* larbs */
	larbs = devm_kcalloc(common->dev, common->index, sizeof(*larbs),
		GFP_KERNEL);
	if (!larbs)
		return -ENOMEM;
	/* device set driver data */
	platform_set_drvdata(pdev, common);
	return ret;
#else /* !CONFIG_MTK_SMI_EXT  */
	struct device *dev = &pdev->dev;
	struct mtk_smi *common;
	struct resource *res;
	enum mtk_smi_gen smi_gen;
	int ret;

	common = devm_kzalloc(dev, sizeof(*common), GFP_KERNEL);
	if (!common)
		return -ENOMEM;
	common->dev = dev;

	common->clk_apb = devm_clk_get(dev, "apb");
	if (IS_ERR(common->clk_apb))
		return PTR_ERR(common->clk_apb);

	common->clk_smi = devm_clk_get(dev, "smi");
	if (IS_ERR(common->clk_smi))
		return PTR_ERR(common->clk_smi);

	/*
	 * for mtk smi gen 1, we need to get the ao(always on) base to config
	 * m4u port, and we need to enable the aync clock for transform the smi
	 * clock into emi clock domain, but for mtk smi gen2, there's no smi ao
	 * base.
	 */
	smi_gen = (enum mtk_smi_gen)of_device_get_match_data(dev);
	if (smi_gen == MTK_SMI_GEN1) {
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
	}
	pm_runtime_enable(dev);
	platform_set_drvdata(pdev, common);
	return 0;
#endif /* !CONFIG_MTK_SMI_EXT */
}

static int mtk_smi_common_remove(struct platform_device *pdev)
{
#if !IS_ENABLED(CONFIG_MTK_SMI_EXT)
	pm_runtime_disable(&pdev->dev);
#endif
	return 0;
}

static struct platform_driver mtk_smi_common_driver = {
	.probe	= mtk_smi_common_probe,
	.remove = mtk_smi_common_remove,
	.driver	= {
		.name = "mtk-smi-common",
		.of_match_table = mtk_smi_common_of_ids,
	}
};

static int __init mtk_smi_init(void)
{
	int ret;

	ret = platform_driver_register(&mtk_smi_common_driver);
	if (ret != 0) {
		pr_notice("Failed to register SMI driver\n");
		return ret;
	}

	ret = platform_driver_register(&mtk_smi_larb_driver);
	if (ret != 0) {
		pr_notice("Failed to register SMI-LARB driver\n");
		goto err_unreg_smi;
	}

#if IS_ENABLED(CONFIG_MTK_SMI_EXT)
	ret = smi_register(&mtk_smi_common_driver);
	if (ret) {
		pr_notice("Failed to register SMI-EXT driver\n");
		platform_driver_unregister(&mtk_smi_larb_driver);
		platform_driver_unregister(&mtk_smi_common_driver);
		return ret;
	}
#endif
	return ret;

err_unreg_smi:
	platform_driver_unregister(&mtk_smi_common_driver);
	return ret;
}

static void __exit mtk_smi_exit(void)
{
	platform_driver_unregister(&mtk_smi_larb_driver);
	platform_driver_unregister(&mtk_smi_common_driver);
#if IS_ENABLED(CONFIG_MTK_SMI_EXT)
	smi_unregister(&mtk_smi_common_driver);
#endif
}

#if IS_ENABLED(CONFIG_MTK_SMI_EXT) \
	&& !(IS_ENABLED(CONFIG_MACH_MT6765) || IS_ENABLED(CONFIG_MACH_MT6761))
arch_initcall_sync(mtk_smi_init);
#else
module_init(mtk_smi_init);
#endif
module_exit(mtk_smi_exit);
MODULE_DESCRIPTION("SMI");
MODULE_LICENSE("GPL");
