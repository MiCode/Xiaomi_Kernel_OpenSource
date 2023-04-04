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

#include "mtk-scpsys.h"

#include <dt-bindings/power/mt6739-power.h>

#ifdef CONFIG_FPGA_EARLY_PORTING
#define IGNORE_MTCMOS_CHECK
#endif

#define MD_SRAM_ISO_CON	0x0394

//for MT6739
#define MT6739_TOP_AXI_PROT_EN_INFRA_1_MD	(BIT(3) | BIT(4) | BIT(7))
#define MT6739_TOP_AXI_PROT_EN_INFRA_2_MD	(BIT(6))
#define MT6739_TOP_AXI_PROT_EN_INFRA_CONN	(BIT(13) | BIT(14))
#define MT6739_TOP_AXI_PROT_EN_INFRA_MM_DISP	(BIT(1))
#define MT6739_TOP_AXI_PROT_EN_INFRA_MFG 	(BIT(21) | BIT(22))

/* Define MTCMOS power control */
#define PWR_RST_B                        (0x1 << 0)
#define PWR_ISO                          (0x1 << 1)
#define PWR_ON                           (0x1 << 2)
#define PWR_ON_2ND                       (0x1 << 3)
#define PWR_CLK_DIS                      (0x1 << 4)
#define SRAM_CKISO                       (0x1 << 5)
#define MD_PWR_CLK_DIS                   (0x1 << 5)
#define SRAM_ISOINT_B                    (0x1 << 6)
#define SLPB_CLAMP                       (0x1 << 7)

/* Define Non-CPU SRAM Mask */
#define MD1_SRAM_PDN                     (0x1 << 6)

/*
 * MT6768 power domain support
 */
static const struct scp_domain_data scp_domain_data_mt6739[] = {
	[MT6739_POWER_DOMAIN_MD] = {
		.name = "md",
		.sta_mask = BIT(0),
		.ctl_offs = 0x0320,
		.extb_iso_offs = 0x398,
		.extb_iso_bits = 0x1,    //need to review
		//BUS_PROT_IGN(_type, _set_ofs, _clr_ofs,	_en_ofs, _sta_ofs, _mask)
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0224,
				MT6739_TOP_AXI_PROT_EN_INFRA_1_MD),
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0254,
				MT6739_TOP_AXI_PROT_EN_INFRA_2_MD),
		},
		.caps = MTK_SCPD_LEGACY_MD_OPS | MTK_SCPD_BYPASS_INIT_ON,
	},

	[MT6739_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.sta_mask = BIT(1),
		.ctl_offs = 0x032c, //CONN_PWR_CON
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6739_TOP_AXI_PROT_EN_INFRA_CONN),

		},
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},

	[MT6739_POWER_DOMAIN_DIS] = {
		.name = "dis",
		.sta_mask = BIT(3),
		.ctl_offs = 0x030C,
		.sram_pdn_bits = GENMASK(5, 5),
		.sram_pdn_ack_bits = GENMASK(6, 6),
		.basic_clk_name = {"mm"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6739_TOP_AXI_PROT_EN_INFRA_MM_DISP),
		},
	},

	[MT6739_POWER_DOMAIN_MFG] = {
		.name = "mfg",
		.sta_mask = BIT(4),
		.ctl_offs = 0x338,
		.sram_pdn_bits = GENMASK(5, 5),
		.sram_pdn_ack_bits = GENMASK(6, 6),
		.basic_clk_name = {"mfg"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6739_TOP_AXI_PROT_EN_INFRA_MFG),
		},
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},

	[MT6739_POWER_DOMAIN_MFG_CORE0] = {
		.name = "mfg_core0",
		.sta_mask = BIT(31),
		.ctl_offs = 0x33C,
		.sram_pdn_bits = GENMASK(5, 5),
		.sram_pdn_ack_bits = GENMASK(6, 6),
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},

	[MT6739_POWER_DOMAIN_ISP] = {
		.name = "isp",
		.sta_mask = BIT(5),
		.ctl_offs = 0x0308,
		.sram_pdn_bits = GENMASK(5, 5),
		.sram_pdn_ack_bits = GENMASK(6, 6),
		.basic_clk_name = {"mm"},
	},

	[MT6739_POWER_DOMAIN_VCODEC] = {
		.name = "vcodec",
		.sta_mask = BIT(7),
		.ctl_offs = 0x0300,
		.sram_pdn_bits = GENMASK(5, 5),
		.sram_pdn_ack_bits = GENMASK(6, 6),
		.basic_clk_name = {"mm"},
	},
};

static const struct scp_subdomain scp_subdomain_mt6739[] = {
		{MT6739_POWER_DOMAIN_DIS, MT6739_POWER_DOMAIN_ISP},
		{MT6739_POWER_DOMAIN_DIS, MT6739_POWER_DOMAIN_VCODEC},
};

static const struct scp_soc_data mt6739_data = {
	.domains = scp_domain_data_mt6739,
	.num_domains = MT6739_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6739,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6739),
	.regs = {
		.pwr_sta_offs = 0x0180,
		.pwr_sta2nd_offs = 0x0184
	}
};

static int clear_bus_protection(struct regmap *map, struct bus_prot *bp)
{
	u32 val;
	u32 clr_ofs = bp->clr_ofs;
	u32 en_ofs = bp->en_ofs;
	u32 sta_ofs = bp->sta_ofs;
	u32 mask = bp->mask;
	bool ignore_ack = bp->ignore_clr_ack;
	int ret = 0;

	if (clr_ofs)
		regmap_write(map, clr_ofs, mask);
	else
		regmap_update_bits(map, en_ofs, mask, 0);

	if (ignore_ack)
		return 0;

	ret = regmap_read_poll_timeout_atomic(map, sta_ofs,
			val, !(val & mask),
			MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);

	if (ret < 0) {
		pr_err("%s val=0x%x, mask=0x%x, (val & mask)=0x%x\n",
			__func__, val, mask, (val & mask));
	}
	return ret;
}

static int spm_topaxi_protect_is_ready(struct regmap *map, unsigned int reg, unsigned int mask_value)
{
	/*unsigned long flags;*/
	signed int val;

#ifdef TOPAXI_PROTECT_LOCK
	int count = 0;
	struct timeval tm_s, tm_e;
	unsigned int tm_val = 0;

	do_gettimeofday(&tm_s);
	/*spm_mtcmos_noncpu_lock(flags);*/
	regmap_read(mpa, reg, &val);
	while (val & mask_value != mask_value) {
		count++;
		if (count > _TOPAXI_TIMEOUT_CNT_)
			break;
		regmap_read(mpa, reg, &val)
	}

	/*spm_mtcmos_noncpu_unlock(flags);*/

	if (count > _TOPAXI_TIMEOUT_CNT_) {
		do_gettimeofday(&tm_e);
		tm_val = (tm_e.tv_sec - tm_s.tv_sec) * 1000000 + (tm_e.tv_usec - tm_s.tv_usec);
		pr_debug("TOPAXI Bus Protect Timeout Error (%d us)(%d) !!\n", tm_val, count);
		pr_debug("INFRA_TOPAXI_PROTECTEN_STA0 = 0x%x != 0x%x\n",
				clk_readl(reg), mask_value);
		/*more relative log can add here*/
		WARN_ON(1);
	}
#else
	regmap_read(map, reg, &val);
	while ((val & mask_value) != mask_value) {
	}
#endif

	return 0;
}

static int set_bus_protection(struct regmap *map, struct bus_prot *bp)
{
	u32 set_ofs = bp->set_ofs;
	u32 en_ofs = bp->en_ofs;
	u32 sta_ofs = bp->sta_ofs;
	u32 mask = bp->mask;
	int ret = 0;

	if (set_ofs)
		regmap_write(map, set_ofs, mask);
	else
		regmap_update_bits(map, en_ofs, mask, mask);

#ifndef IGNORE_MTCMOS_CHECK
	spm_topaxi_protect_is_ready(map, sta_ofs, mask);
#endif
	return ret;
}

static int scpsys_bus_protect_disable(struct scp_domain *scpd)
{
	struct scp *scp = scpd->scp;
	const struct bus_prot *bp_table = scpd->data->bp_table;
	struct regmap *map = scp->infracfg;
	int i = 0;

	for (i = MAX_STEPS - 1; i >= 0; i--) {
		int ret;
		struct bus_prot bp = bp_table[i];
		if (bp.type != IFR_TYPE) {
			continue;
		}
		/* restore bus protect setting */
		ret = clear_bus_protection(map, &bp);
		if (ret)
			return ret;
	}

	return 0;
}

static int scpsys_bus_protect_enable(struct scp_domain *scpd)
{
	struct scp *scp = scpd->scp;
	const struct bus_prot *bp_table = scpd->data->bp_table;
	struct regmap *map = scp->infracfg;
	int i = 0;

	for (i = 0; i < MAX_STEPS; i++) {
		int ret;
		struct bus_prot bp = bp_table[i];
		if (bp.type != IFR_TYPE) {
			break;
		}

		ret = set_bus_protection(map, &bp);
		if (ret) {
			scpsys_bus_protect_disable(scpd);
			return ret;
		}
	}
	return 0;
}

static void scpsys_sram_iso_up(struct scp_domain *scpd)
{
	u32 val;
	struct scp *scp;
	void __iomem *ctl_addr;

	if (!scpd->data->extb_iso_offs)
		return;

	scp = scpd->scp;
	ctl_addr = scp->base + MD_SRAM_ISO_CON;
	val = readl(ctl_addr) | (0x1 << 0);
	writel(val, ctl_addr);
}

static void scpsys_sram_iso_down(struct scp_domain *scpd)
{
	u32 val;
	struct scp *scp;
	void __iomem *ctl_addr;

	scp = scpd->scp;
	ctl_addr = scp->base + MD_SRAM_ISO_CON;
	val = readl(ctl_addr) & ~(0x1 << 0);
	writel(val, ctl_addr);
}

static void scpsys_extb_iso_up(struct scp_domain *scpd)
{
	u32 val;
	struct scp *scp;
	void __iomem *ctl_addr;

	if (!scpd->data->extb_iso_offs)
		return;

	scp = scpd->scp;
	ctl_addr = scp->base + scpd->data->extb_iso_offs;
	val = readl(ctl_addr) | scpd->data->extb_iso_bits;
	writel(val, ctl_addr);
}

static void scpsys_extb_iso_down(struct scp_domain *scpd)
{
	u32 val;
	struct scp *scp;
	void __iomem *ctl_addr;

	if (!scpd->data->extb_iso_offs)
		return;

	scp = scpd->scp;
	ctl_addr = scp->base + scpd->data->extb_iso_offs;
	val = readl(ctl_addr) & ~scpd->data->extb_iso_bits;
	writel(val, ctl_addr);
}

static int scpsys_md_domain_is_on(struct scp_domain *scpd)
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

static int scpsys_legacy_md1_power_off(struct generic_pm_domain *genpd)
{
	struct scp_domain *scpd = container_of(genpd, struct scp_domain, genpd);
	struct scp *scp = scpd->scp;
	void __iomem *ctl_addr = scp->base + scpd->data->ctl_offs;
	u32 val;
	int ret, tmp = 0;

	/* TINFO="Start to turn off MD1" */
	/* TINFO="Set bus protect" */
	scpsys_bus_protect_enable(scpd);
	/* TINFO="MD_EXTRA_PWR_CON[0]=1"*/
	scpsys_extb_iso_up(scpd);
	/* TINFO="Set MD_PWR_CLK_DIS = 1" */
	val = readl(ctl_addr);
	val |= MD_PWR_CLK_DIS;
	writel(val, ctl_addr);
	/* TINFO="Set PWR_ISO = 1" */
	val |= PWR_ISO;
	writel(val, ctl_addr);
	/* TINFO="MD_SRAM_ISO_CON[0]=0"*/
	scpsys_sram_iso_down(scpd);
	/* TINFO="Set SRAM_PDN = 1" */
	val |= MD1_SRAM_PDN;
	writel(val, ctl_addr);
#ifndef IGNORE_MTCMOS_CHECK
#endif
	/* TINFO="Set PWR_ON = 0" */
	val &= ~PWR_ON;
	writel(val, ctl_addr);
	/* TINFO="Set PWR_ON_2ND = 0" */
	val &= ~PWR_ON_2ND;
	writel(val, ctl_addr);
#ifndef IGNORE_MTCMOS_CHECK
	/* TINFO="Wait until PWR_STATUS = 0 and PWR_STATUS_2ND = 0" */
	ret = readx_poll_timeout_atomic(scpsys_md_domain_is_on, scpd, tmp, tmp == 0,
				MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
	if (ret < 0)
		goto out;
#endif
    /* TINFO="Set PWR_RST_B = 0" */
	val &= ~PWR_RST_B;
	writel(val, ctl_addr);
	/* TINFO="Finish to turn off MD1" */
	return 0;

out:
	dev_notice(scp->dev, "Failed to power off domain %s\n", genpd->name);

	return ret;
}

static int scpsys_legacy_md1_power_on(struct generic_pm_domain *genpd)
{
	struct scp_domain *scpd = container_of(genpd, struct scp_domain, genpd);
	struct scp *scp = scpd->scp;
	void __iomem *ctl_addr = scp->base + scpd->data->ctl_offs;
	u32 val;
	int ret, tmp = 0;

	/* TINFO="Start to turn on MD1" */
	/* TINFO="Set PWR_RST_B = 0" */
	val = readl(ctl_addr);
	val &= ~PWR_RST_B;
	writel(val, ctl_addr);

	/* TINFO="Set PWR_ON = 1" */
	val |= PWR_ON;
	writel(val, ctl_addr);
	/* TINFO="Set PWR_ON_2ND = 1" */
	val |= PWR_ON_2ND;
	writel(val, ctl_addr);
#ifndef IGNORE_MTCMOS_CHECK
	/* TINFO="Wait until PWR_STATUS = 1 and PWR_STATUS_2ND = 1" */
	/*while(!scpsys_md_domain_is_on(scpd)) {
	}*/
	ret = readx_poll_timeout_atomic(scpsys_md_domain_is_on, scpd, tmp, tmp > 0,
				 MTK_POLL_DELAY_US, MTK_POLL_TIMEOUT);
	if (ret < 0)
		goto out;
#endif
	/* TINFO="Set SRAM_PDN = 0" */
	val &= ~(0x1 << 6);
	writel(val, ctl_addr);
	/* TINFO="MD_SRAM_ISO_CON[0]=1"*/
	scpsys_sram_iso_up(scpd);
	/* TINFO="Set PWR_ISO = 0" */
	val &= ~PWR_ISO;
	writel(val, ctl_addr);
	/* TINFO="Set MD_PWR_CLK_DIS = 0" */
	val &= ~MD_PWR_CLK_DIS;
	writel(val, ctl_addr);
	/* TINFO="Set PWR_RST_B = 1" */
	val |= PWR_RST_B;
	writel(val, ctl_addr);
	/* TINFO="MD_EXTRA_PWR_CON[0]=0"*/
	scpsys_extb_iso_down(scpd);
	/* TINFO="Release bus protect" */
	scpsys_bus_protect_disable(scpd);
#ifndef IGNORE_MTCMOS_CHECK
	udelay(250);
	/* Note that this protect ack check after releasing protect has been ignored */
#endif
	/* TINFO="Finish to turn on MD1" */
	return 0;
out:
	dev_err(scp->dev, "Failed to power on mtcmos %s[%d, 0x%x]\n", genpd->name, ret, val);
	return ret;
}

/*
 * scpsys driver init
 */

static const struct of_device_id of_scpsys_match_tbl[] = {
	{
		.compatible = "mediatek,mt6739-scpsys",
		.data = &mt6739_data,
	}, {
		/* sentinel */
	}
};

static struct md_callbacks mt6739_md_callbacks = {
	.md_power_on = scpsys_legacy_md1_power_on,
	.md_power_off = scpsys_legacy_md1_power_off,
};

static int mt6739_scpsys_probe(struct platform_device *pdev)
{
	const struct scp_subdomain *sd;
	const struct scp_soc_data *soc;
	struct scp *scp;
	struct genpd_onecell_data *pd_data;
	int i, ret;

	soc = of_device_get_match_data(&pdev->dev);

	register_md_callback(&mt6739_md_callbacks);
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

static struct platform_driver mt6739_scpsys_drv = {
	.probe = mt6739_scpsys_probe,
	.driver = {
		.name = "mtk-scpsys-mt6739",
		.suppress_bind_attrs = true,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_scpsys_match_tbl),
	},
};
#if IS_BUILTIN(CONFIG_MTK_SCPSYS)
static int __init mt6739_scpsys_drv_init(void)
{
	return platform_driver_register(&mt6739_scpsys_drv);
}
subsys_initcall(mt6739_scpsys_drv_init);
#else
module_platform_driver(mt6739_scpsys_drv);
#endif
MODULE_LICENSE("GPL");
