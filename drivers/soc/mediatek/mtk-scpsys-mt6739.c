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

//for MT6739
#define MT6739_TOP_AXI_PROT_EN_INFRA_1_MD	(BIT(3) | BIT(4) | BIT(7))
#define MT6739_TOP_AXI_PROT_EN_INFRA_2_MD	(BIT(6))
#define MT6739_TOP_AXI_PROT_EN_INFRA_CONN	(BIT(13) | BIT(14))
#define MT6739_TOP_AXI_PROT_EN_INFRA_MM_DISP	(BIT(1))
#define MT6739_TOP_AXI_PROT_EN_INFRA_MFG 	(BIT(21) | BIT(22))

/*
 * MT6768 power domain support
 */
static const struct scp_domain_data scp_domain_data_mt6739[] = {
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

static int mt6739_scpsys_probe(struct platform_device *pdev)
{
	const struct scp_subdomain *sd;
	const struct scp_soc_data *soc;
	struct scp *scp;
	struct genpd_onecell_data *pd_data;
	int i, ret;

	soc = of_device_get_match_data(&pdev->dev);

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
