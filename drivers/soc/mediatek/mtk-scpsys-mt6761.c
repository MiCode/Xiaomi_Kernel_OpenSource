// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_opp.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>

#include "scpsys.h"
#include "mtk-scpsys.h"

#include <dt-bindings/power/mt6761-power.h>


/*
 * MT6761 power domain support
 */


static const struct scp_domain_data scp_domain_data_mt6761[] = {
	[MT6761_POWER_DOMAIN_MD] = {
		.name = "md",
		.sta_mask = BIT(0),
		.ctl_offs = 0x320,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		//BUS_PROT_IGN(_type, _set_ofs, _clr_ofs,	_en_ofs, _sta_ofs, _mask)
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6761_TOP_AXI_PROT_EN_INFRA_1_MD1),
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6761_TOP_AXI_PROT_EN_INFRA_2_MD1),
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				MT6761_TOP_AXI_PROT_EN_INFRA_3_MD1),
		},
		.caps = MTK_SCPD_LEGACY_MD_OPS | MTK_SCPD_BYPASS_INIT_ON,
	},

	[MT6761_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.sta_mask = BIT(1),
		.ctl_offs = 0x032C, //CONN_PWR_CON
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6761_TOP_AXI_PROT_EN_INFRA_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				MT6761_TOP_AXI_PROT_EN_INFRA_CONN_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6761_TOP_AXI_PROT_EN_INFRA_1_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				MT6761_TOP_AXI_PROT_EN_INFRA_1_CONN_2ND),

		},
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},

	[MT6761_POWER_DOMAIN_DPY] = {
		.name = "dpy",
		.sta_mask = BIT(2),
		.ctl_offs = 0x031C,
		.sram_pdn_bits = GENMASK(11, 8),
		.sram_pdn_ack_bits = GENMASK(15, 12),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6761_TOP_AXI_PROT_EN_INFRA_DPY),
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				MT6761_TOP_AXI_PROT_EN_INFRA_DPY_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6761_TOP_AXI_PROT_EN_INFRA_2_DPY),

		},
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},

	[MT6761_POWER_DOMAIN_DIS] = {
		.name = "disp",
		.sta_mask = BIT(3),
		.ctl_offs = 0x030C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mm"},
		.subsys_clk_prefix = "mm",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				MT6761_TOP_AXI_PROT_EN_INFRA_MM_DIS),
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6761_TOP_AXI_PROT_EN_INFRA_MM_2_DIS),
		},
	},

	[MT6761_POWER_DOMAIN_MFG] = {
		.name = "mfg",
		.sta_mask = BIT(4),
		.ctl_offs = 0x0338,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6761_TOP_AXI_PROT_EN_INFRA_MFG),
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6761_TOP_AXI_PROT_EN_INFRA_MFG_2ND),
		},
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},

	[MT6761_POWER_DOMAIN_IFR] = {
		.name = "ifr",
		.sta_mask = BIT(6),
		.ctl_offs = 0x0318,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},

	[MT6761_POWER_DOMAIN_MFG_CORE0] = {
		.name = "mfg_core0",
		.sta_mask = BIT(7),
		.ctl_offs = 0x034C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},

	[MT6761_POWER_DOMAIN_MFG_ASYNC] = {
		.name = "mfg_async",
		.sta_mask = BIT(23),
		.ctl_offs = 0x0334,
		.basic_clk_name = {"mfg"},
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},

	[MT6761_POWER_DOMAIN_CAM] = {
		.name = "cam",
		.sta_mask = BIT(25),
		.ctl_offs = 0x0344,
		.sram_pdn_bits = GENMASK(9, 8),
		.sram_pdn_ack_bits = GENMASK(13, 12),
		.subsys_clk_prefix = "cam",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				MT6761_TOP_AXI_PROT_EN_INFRA_1_CAM),
			BUS_PROT_IGN(SMI_TYPE, 0x03C4, 0x03C8, 0x03C0, 0x03C0,
				MT6761_TOP_AXI_PROT_EN_INFRA_2_CAM),
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6761_TOP_AXI_PROT_EN_INFRA_3_CAM),
		},
	},

	[MT6761_POWER_DOMAIN_VCODEC] = {
		.name = "vcodec",
		.sta_mask = BIT(26),
		.ctl_offs = 0x0300,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},

};

static const struct scp_subdomain scp_subdomain_mt6761[] = {
	{MT6761_POWER_DOMAIN_MFG_ASYNC, MT6761_POWER_DOMAIN_MFG},
	{MT6761_POWER_DOMAIN_MFG, MT6761_POWER_DOMAIN_MFG_CORE0},
	{MT6761_POWER_DOMAIN_DIS, MT6761_POWER_DOMAIN_CAM},
	{MT6761_POWER_DOMAIN_DIS, MT6761_POWER_DOMAIN_VCODEC},
};

static const struct scp_soc_data mt6761_data = {
	.domains = scp_domain_data_mt6761,
	.num_domains = MT6761_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6761,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6761),
	.regs = {
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184
	}
};

static int scpsys_domain_is_on(struct scp_domain *scpd)
{
	struct scp *scp = scpd->scp;

	u32 status = readl(scp->base + scp->ctrl_reg.pwr_sta_offs) &
						scpd->data->sta_mask;
	u32 status2 = readl(scp->base + scp->ctrl_reg.pwr_sta2nd_offs) &
						scpd->data->sta_mask;

	if (status && status2)
		return true;
	if (!status && !status2)
		return false;

	return -EINVAL;
}

static int set_bus_protection(struct regmap *map, u32 set_ofs, u32 sta_ofs, u32 mask)
{
	u32 val;
	int ret = 0;

	regmap_write(map, set_ofs, mask);

	ret = regmap_read_poll_timeout_atomic(map, sta_ofs,
			val, (val & mask) == mask,
			MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);

	if (ret < 0) {
		pr_err("%s val=0x%x, mask=0x%x, (val & mask)=0x%x\n",
			__func__, val, mask, (val & mask));
	}
	return ret;
}

#define MD1_PROT_STEP1_0_MASK			((0x1 << 7))
#define MD1_PROT_STEP1_0_ACK_MASK		((0x1 << 7))
#define MD1_PROT_STEP2_0_MASK			((0x1 << 3)	\
							|(0x1 << 4))
#define MD1_PROT_STEP2_0_ACK_MASK		((0x1 << 3)	\
							|(0x1 << 4))
#define MD1_PROT_STEP2_1_MASK			((0x1 << 6))
#define MD1_PROT_STEP2_1_ACK_MASK			((0x1 << 6))

static int mt6761_md_power_on(struct generic_pm_domain *genpd)
{
	struct scp_domain *scpd = container_of(genpd, struct scp_domain, genpd);
	struct scp *scp = scpd->scp;
	struct regmap *infracfg = scp->infracfg;
	void __iomem *ctl_addr = scp->base + scpd->data->ctl_offs;
	void __iomem *sram_iso_addr = scp->base + 0x0394;
	void __iomem *extra_ctl_addr = scp->base + 0x0398;
	u32 val = 0;
	int ret = 0, tmp = 0;

	/* for md subsys, reset_b is prior to power_on bit */
	val = readl(ctl_addr);
	val &= ~(0x1 << 0);
	writel(val, ctl_addr);

	/* subsys power on */
	val |= (0x1 << 2);
	writel(val, ctl_addr);
	val |= (0x1 << 3);
	writel(val, ctl_addr);

	/* wait until PWR_ACK = 1 */
	ret = readx_poll_timeout_atomic(scpsys_domain_is_on, scpd, tmp, tmp > 0,
				 MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);

	if (ret < 0)
		goto out;

	/* TINFO="MD_SRAM_ISO_CON[0]=1"*/
	writel(readl(sram_iso_addr) | (0x1 << 0), sram_iso_addr);
	/* TINFO="Set PWR_ISO = 0" */
	val &= ~(0x1 << 1);
	writel(val, ctl_addr);
	/* TINFO="Set PWR_CLK_DIS = 0" */
	val &= ~(0x1 << 4);
	writel(val, ctl_addr);
	/* TINFO="Set PWR_RST_B = 1" */
	val |= (0x1 << 0);
	writel(val, ctl_addr);
	/* TINFO="MD_EXTRA_PWR_CON[0]=0"*/
	writel(readl(extra_ctl_addr) & ~(0x1 << 0), extra_ctl_addr);
	/* TINFO="Finish to turn on MD1" */

	/* TINFO="Set SRAM_PDN = 0" */
	val &= ~(0x1 << 8);
	writel(val, ctl_addr);
	/* TINFO="Release bus protect - step2 : 0" */
	regmap_write(infracfg, 0x02A4, MD1_PROT_STEP2_0_MASK);
	/* TINFO="Release bus protect - step2 : 1" */
	regmap_write(infracfg, 0x02AC, MD1_PROT_STEP2_1_MASK);
	/* TINFO="Release bus protect - step1 : 0" */
	regmap_write(infracfg, 0x02A4, MD1_PROT_STEP1_0_MASK);

	return 0;

out:
	dev_err(scp->dev, "Failed to power on mtcmos %s[%d, 0x%x]\n", genpd->name, ret, val);

	return ret;
}



static int mt6761_md_power_off(struct generic_pm_domain *genpd)
{
	struct scp_domain *scpd = container_of(genpd, struct scp_domain, genpd);
	struct scp *scp = scpd->scp;
	struct regmap *infracfg = scp->infracfg;
	void __iomem *ctl_addr = scp->base + scpd->data->ctl_offs;
	void __iomem *sram_iso_addr = scp->base + 0x0394;
	void __iomem *extra_ctl_addr = scp->base + 0x0398;
	u32 val = 0;
	int ret = 0, tmp = 0;

	/* TINFO="Set bus protect - step1 : 0" */
	ret = set_bus_protection(infracfg, 0x02A0, 0x0228, MD1_PROT_STEP1_0_MASK);
	if (ret < 0)
		goto out;
	/* TINFO="Set bus protect - step2 : 0" */
	ret = set_bus_protection(infracfg, 0x02A0, 0x0228, MD1_PROT_STEP2_0_MASK);
	if (ret < 0)
		goto out;
	/* TINFO="Set bus protect - step2 : 1" */
	ret = set_bus_protection(infracfg, 0x02A8, 0x0258, MD1_PROT_STEP2_1_MASK);
	if (ret < 0)
		goto out;
	/* TINFO="Set SRAM_PDN = 1" */
	val = readl(ctl_addr);
	val |= (0x1 << 8);
	writel(val, ctl_addr);

	/* TINFO="Start to turn off MD1" */
	/* TINFO="MD_EXTRA_PWR_CON[0]=1"*/
	writel(readl(extra_ctl_addr) | (0x1 << 0), extra_ctl_addr);
	/* TINFO="Set PWR_CLK_DIS = 1" */
	val |= (0x1 << 4);
	writel(val, ctl_addr);
	/* TINFO="Set PWR_ISO = 1" */
	val |= (0x1 << 1);
	writel(val, ctl_addr);
	/* TINFO="MD_SRAM_ISO_CON[0]=0"*/
	writel(readl(sram_iso_addr) & ~(0x1 << 0), sram_iso_addr);
	/* TINFO="Set PWR_ON = 0" */
	val &= ~(0x1 << 2);
	writel(val, ctl_addr);
	/* TINFO="Set PWR_ON_2ND = 0" */
	val &= ~(0x1 << 3);
	writel(val, ctl_addr);
	/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
	ret = readx_poll_timeout_atomic(scpsys_domain_is_on, scpd, tmp, tmp == 0,
			MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
	if (ret < 0)
		goto out;

	return 0;

out:
	dev_err(scp->dev, "Failed to power off mtcmos %s(%d, 0x%x)\n", genpd->name, ret, val);

	return ret;
}

static struct md_callbacks mt6761_md_callbacks = {
	.md_power_on = mt6761_md_power_on,
	.md_power_off = mt6761_md_power_off,
};

/*
 * scpsys driver init
 */

static const struct of_device_id of_scpsys_match_tbl[] = {
	{
		.compatible = "mediatek,mt6761-scpsys",
		.data = &mt6761_data,
	}, {
		/* sentinel */
	}
};

static int mt6761_scpsys_probe(struct platform_device *pdev)
{
	const struct scp_subdomain *sd;
	const struct scp_soc_data *soc;
	struct scp *scp;
	struct genpd_onecell_data *pd_data;
	int i, ret;

	soc = of_device_get_match_data(&pdev->dev);

	register_md_callback(&mt6761_md_callbacks);

	scp = init_scp(pdev, soc->domains, soc->num_domains, &soc->regs);
	if (IS_ERR(scp))
		return PTR_ERR(scp);

	mtk_register_power_domains(pdev, scp, soc->num_domains);

	pd_data = &scp->pd_data;

	for (i = 0, sd = soc->subdomains; i < soc->num_subdomains; i++, sd++) {
		ret = pm_genpd_add_subdomain(pd_data->domains[sd->origin],
					     pd_data->domains[sd->subdomain]);
		if (ret && IS_ENABLED(CONFIG_PM))
			dev_err(&pdev->dev, "Failed to add subdomain: %d\n",
				ret);
	}

	return 0;
}

static struct platform_driver mt6761_scpsys_drv = {
	.probe = mt6761_scpsys_probe,
	.driver = {
		.name = "mtk-scpsys-mt6761",
		.suppress_bind_attrs = true,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_scpsys_match_tbl),
	},
};

module_platform_driver(mt6761_scpsys_drv);
MODULE_LICENSE("GPL");
