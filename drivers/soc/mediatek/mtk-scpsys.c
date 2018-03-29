/*
 * Copyright (c) 2015 Pengutronix, Sascha Hauer <kernel@pengutronix.de>
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
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/pm_domain.h>
#include <linux/delay.h>
#include <linux/soc/mediatek/infracfg.h>
#include <dt-bindings/power/mt8173-power.h>
#include <linux/mfd/syscon.h>
#include "mt_gpufreq.h"

#define SPM_VDE_PWR_CON			0x0210
#define SPM_MFG_PWR_CON			0x0214
#define SPM_VEN_PWR_CON			0x0230
#define SPM_ISP_PWR_CON			0x0238
#define SPM_DIS_PWR_CON			0x023c
#define SPM_VEN2_PWR_CON		0x0298
#define SPM_AUDIO_PWR_CON		0x029c
#define SPM_MFG_2D_PWR_CON		0x02c0
#define SPM_MFG_ASYNC_PWR_CON		0x02c4
#define SPM_USB_PWR_CON			0x02cc
#define SPM_PWR_STATUS			0x060c
#define SPM_PWR_STATUS_2ND		0x0610

#define PWR_RST_B_BIT			BIT(0)
#define PWR_ISO_BIT			BIT(1)
#define PWR_ON_BIT			BIT(2)
#define PWR_ON_2ND_BIT			BIT(3)
#define PWR_CLK_DIS_BIT			BIT(4)

#define DIS_PWR_STA_MASK		BIT(3)
#define MFG_PWR_STA_MASK		BIT(4)
#define ISP_PWR_STA_MASK		BIT(5)
#define VDE_PWR_STA_MASK		BIT(7)
#define VEN2_PWR_STA_MASK		BIT(20)
#define VEN_PWR_STA_MASK		BIT(21)
#define MFG_2D_PWR_STA_MASK		BIT(22)
#define MFG_ASYNC_PWR_STA_MASK		BIT(23)
#define AUDIO_PWR_STA_MASK		BIT(24)
#define USB_PWR_STA_MASK		BIT(25)

struct scp_domain_data {
	const char *name;
	u32 sta_mask;
	int ctl_offs;
	u32 sram_pdn_bits;
	u32 sram_pdn_ack_bits;
	u32 bus_prot_mask;
	int id;
	const char *clk_name;
};

static const struct scp_domain_data scp_domain_data[] = {
	{
		.id = MT8173_POWER_DOMAIN_VDEC,
		.name = "vde",
		.sta_mask = VDE_PWR_STA_MASK,
		.ctl_offs = SPM_VDE_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.clk_name = "disp",
	}, {
		.id = MT8173_POWER_DOMAIN_VENC,
		.name = "ven",
		.sta_mask = VEN_PWR_STA_MASK,
		.ctl_offs = SPM_VEN_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.clk_name = "disp",
	}, {
		.id = MT8173_POWER_DOMAIN_ISP,
		.name = "isp",
		.sta_mask = ISP_PWR_STA_MASK,
		.ctl_offs = SPM_ISP_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12),
		.clk_name = "disp",
	}, {
		.id = MT8173_POWER_DOMAIN_DISP,
		.name = "disp",
		.sta_mask = DIS_PWR_STA_MASK,
		.ctl_offs = SPM_DIS_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.clk_name = "disp",
		.bus_prot_mask = MT8173_TOP_AXI_PROT_EN_MM_M0 |
			MT8173_TOP_AXI_PROT_EN_MM_M1,
	}, {
		.id = MT8173_POWER_DOMAIN_VENC_LT,
		.name = "ven2",
		.sta_mask = VEN2_PWR_STA_MASK,
		.ctl_offs = SPM_VEN2_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.clk_name = "disp",
	}, {
		.id = MT8173_POWER_DOMAIN_AUDIO,
		.name = "audio",
		.sta_mask = AUDIO_PWR_STA_MASK,
		.ctl_offs = SPM_AUDIO_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
	},  {
		.id = MT8173_POWER_DOMAIN_MFG_ASYNC,
		.name = "mfg_async",
		.sta_mask = MFG_ASYNC_PWR_STA_MASK,
		.ctl_offs = SPM_MFG_ASYNC_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = 0,
	}, {
		.id = MT8173_POWER_DOMAIN_MFG_2D,
		.name = "mfg_2d",
		.sta_mask = MFG_2D_PWR_STA_MASK,
		.ctl_offs = SPM_MFG_2D_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12),
	}, {
		.id = MT8173_POWER_DOMAIN_MFG,
		.name = "mfg",
		.sta_mask = MFG_PWR_STA_MASK,
		.ctl_offs = SPM_MFG_PWR_CON,
		.sram_pdn_bits = GENMASK(13, 8),
		.sram_pdn_ack_bits = GENMASK(21, 16),
		.bus_prot_mask = MT8173_TOP_AXI_PROT_EN_MFG_S |
			MT8173_TOP_AXI_PROT_EN_MFG_M0 |
			MT8173_TOP_AXI_PROT_EN_MFG_M1 |
			MT8173_TOP_AXI_PROT_EN_MFG_SNOOP_OUT,
	}, {
		.id = MT8173_POWER_DOMAIN_USB,
		.name = "usb",
		.sta_mask = USB_PWR_STA_MASK,
		.ctl_offs = SPM_USB_PWR_CON,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
	},
};

#define NUM_DOMAINS	ARRAY_SIZE(scp_domain_data)

struct scp;

struct scp_domain {
	struct generic_pm_domain pmd;
	const struct scp_domain_data *data;
	struct scp *scp;
	struct clk *clk;
};

struct scp {
	struct scp_domain domains[NUM_DOMAINS];
	struct genpd_onecell_data pd_data;
	struct device *dev;
	void __iomem *base;
	struct regmap *infracfg;
};

static int scpsys_power_on(struct generic_pm_domain *genpd)
{
	struct scp_domain *scpd = container_of(genpd, struct scp_domain, pmd);
	struct scp *scp = scpd->scp;
	const struct scp_domain_data *data = scpd->data;
	unsigned long expired;
	void __iomem *ctl_addr = scpd->scp->base + data->ctl_offs;
	u32 sram_pdn_ack = data->sram_pdn_ack_bits;
	u32 val;
	int ret;

	if ((strcmp(genpd->name, "mfg_async") == 0)) {
		ret = mt_gpufreq_voltage_enable_set(1);
		if (ret != 0)
			dev_err(scp->dev, "scpsys_power_on %s regulator_fail ret=%d\n", scpd->data->name, ret);
	}

	if (scpd->clk) {
		ret = clk_prepare_enable(scpd->clk);
		if (ret)
			return ret;
	}

	val = readl(ctl_addr);
	val |= PWR_ON_BIT;
	writel(val, ctl_addr);
	val |= PWR_ON_2ND_BIT;
	writel(val, ctl_addr);

	/* wait until PWR_ACK = 1 */
	expired = jiffies + HZ;
	while (!(readl(scp->base + SPM_PWR_STATUS) & data->sta_mask) ||
			!(readl(scp->base + SPM_PWR_STATUS_2ND) &
			data->sta_mask)) {
		cpu_relax();
		if (time_after(jiffies, expired)) {
			ret = -EIO;
			goto out;
		}
	}

	val &= ~PWR_CLK_DIS_BIT;
	writel(val, ctl_addr);

	val &= ~PWR_ISO_BIT;
	writel(val, ctl_addr);

	val |= PWR_RST_B_BIT;
	writel(val, ctl_addr);

	val &= ~data->sram_pdn_bits;
	writel(val, ctl_addr);

	/* wait until SRAM_PDN_ACK all 0 */
	expired = jiffies + HZ;
	while (sram_pdn_ack && (readl(ctl_addr) & sram_pdn_ack)) {
		cpu_relax();
		if (time_after(jiffies, expired)) {
			ret = -EIO;
			goto out;
		}
	}

	if (data->bus_prot_mask) {
		ret = mtk_infracfg_clear_bus_protection(scp->infracfg,
				data->bus_prot_mask);
		if (ret)
			return ret;
	}

	return 0;
out:
	dev_err(scp->dev, "Failed to power on domain %s\n", scpd->data->name);

	return ret;
}

static int scpsys_power_off(struct generic_pm_domain *genpd)
{
	struct scp_domain *scpd = container_of(genpd, struct scp_domain, pmd);
	struct scp *scp = scpd->scp;
	const struct scp_domain_data *data = scpd->data;
	unsigned long expired;
	void __iomem *ctl_addr = scpd->scp->base + data->ctl_offs;
	u32 sram_pdn_ack = data->sram_pdn_ack_bits;
	u32 val;
	int ret;

	if (data->bus_prot_mask) {
		ret = mtk_infracfg_set_bus_protection(scp->infracfg,
				data->bus_prot_mask);
		if (ret)
			return ret;
	}

	val = readl(ctl_addr);
	val |= data->sram_pdn_bits;
	writel(val, ctl_addr);

	/* wait until SRAM_PDN_ACK all 1 */
	expired = jiffies + HZ;
	while ((readl(ctl_addr) & sram_pdn_ack) != sram_pdn_ack) {
		cpu_relax();
		if (time_after(jiffies, expired)) {
			ret = -EIO;
			goto out;
		}
	}

	val |= PWR_ISO_BIT;
	writel(val, ctl_addr);

	val &= ~PWR_RST_B_BIT;
	writel(val, ctl_addr);

	val |= PWR_CLK_DIS_BIT;
	writel(val, ctl_addr);

	val &= ~PWR_ON_BIT;
	writel(val, ctl_addr);

	val &= ~PWR_ON_2ND_BIT;
	writel(val, ctl_addr);

	/* wait until PWR_ACK = 0 */
	expired = jiffies + HZ;
	while ((readl(scp->base + SPM_PWR_STATUS) & data->sta_mask) ||
			(readl(scp->base + SPM_PWR_STATUS_2ND) &
			 data->sta_mask)) {
		cpu_relax();
		if (time_after(jiffies, expired)) {
			ret = -EIO;
			goto out;
		}
	}

	if (scpd->clk)
		clk_disable_unprepare(scpd->clk);

	if ((strcmp(genpd->name, "mfg_async") == 0)) {
		ret = mt_gpufreq_voltage_enable_set(0);
		if (ret != 0) {
			dev_err(scp->dev, "PVR scpsys_power_off %s regulator_fail, ret=%d\n", scpd->data->name, ret);
			mt_gpufreq_voltage_enable_set(1);
		}
	}

	return 0;

out:
	dev_err(scp->dev, "Failed to power off domain %s\n", scpd->data->name);

	return ret;
}

static int scpsys_probe(struct platform_device *pdev)
{
	struct genpd_onecell_data *pd_data;
	struct resource *res;
	int i;
	struct scp *scp;

	scp = devm_kzalloc(&pdev->dev, sizeof(*scp), GFP_KERNEL);
	if (!scp)
		return -ENOMEM;

	scp->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	scp->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(scp->base))
		return PTR_ERR(scp->base);

	pd_data = &scp->pd_data;

	pd_data->domains = devm_kzalloc(&pdev->dev,
			sizeof(*pd_data->domains) * NUM_DOMAINS, GFP_KERNEL);
	if (!pd_data->domains)
		return -ENOMEM;

	scp->infracfg = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
			"infracfg");
	if (IS_ERR(scp->infracfg)) {
		dev_err(&pdev->dev, "Cannot find infracfg controller: %ld\n",
				PTR_ERR(scp->infracfg));
		return PTR_ERR(scp->infracfg);
	}

	pd_data->num_domains = NUM_DOMAINS;

	for (i = 0; i < NUM_DOMAINS; i++) {
		struct scp_domain *scpd = &scp->domains[i];
		struct generic_pm_domain *pmd = &scpd->pmd;

		if (scp_domain_data[i].clk_name) {
			const char *name = scp_domain_data[i].clk_name;

			scpd->clk = devm_clk_get(&pdev->dev, name);
			if (IS_ERR(scpd->clk)) {
				dev_err(&pdev->dev, "Failed to get %s clk: %ld\n",
						name, PTR_ERR(scpd->clk));
				return PTR_ERR(scpd->clk);
			}
		}

		pd_data->domains[scp_domain_data[i].id] = pmd;
		scpd->data = &scp_domain_data[i];
		scpd->scp = scp;

		pmd->name = scp_domain_data[i].name;
		pmd->power_off = scpsys_power_off;
		pmd->power_on = scpsys_power_on;
		pmd->power_off_latency_ns = 20000;
		pmd->power_on_latency_ns = 20000;

		pm_genpd_init(pmd, NULL, true);

		/*
		 * If PM is disabled turn on all domains by default so that
		 * consumers can work.
		 */
		if (!IS_ENABLED(CONFIG_PM))
			pmd->power_on(pmd);
	}

	pm_genpd_add_subdomain(pd_data->domains[MT8173_POWER_DOMAIN_MFG_ASYNC],
		pd_data->domains[MT8173_POWER_DOMAIN_MFG_2D]);
	pm_genpd_add_subdomain(pd_data->domains[MT8173_POWER_DOMAIN_MFG_2D],
		pd_data->domains[MT8173_POWER_DOMAIN_MFG]);

	return of_genpd_add_provider_onecell(pdev->dev.of_node, pd_data);
}

static const struct of_device_id of_scpsys_match_tbl[] = {
	{
		.compatible = "mediatek,mt8173-scpsys",
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, of_scpsys_match_tbl);

static struct platform_driver scpsys_drv = {
	.driver = {
		.name = "mtk-scpsys",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_scpsys_match_tbl),
	},
	.probe = scpsys_probe,
};

module_platform_driver(scpsys_drv);

MODULE_AUTHOR("Sascha Hauer, Pengutronix");
MODULE_DESCRIPTION("MediaTek MT8173 scpsys driver");
MODULE_LICENSE("GPL v2");
