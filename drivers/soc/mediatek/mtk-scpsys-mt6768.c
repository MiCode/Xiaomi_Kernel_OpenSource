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

#include <dt-bindings/power/mt6768-power.h>

/*
 * MT6768 power domain support
 */


static const struct scp_domain_data scp_domain_data_mt6768[] = {
	[MT6768_POWER_DOMAIN_MD] = {
		.name = "md",
		.sta_mask = BIT(0),
		.ctl_offs = 0x320,
		.extb_iso_offs = 0x398,
		.extb_iso_bits = 0x1,    //need to review
		//BUS_PROT_IGN(_type, _set_ofs, _clr_ofs,	_en_ofs, _sta_ofs, _mask)
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6768_TOP_AXI_PROT_EN_INFRA_1_MD),
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6768_TOP_AXI_PROT_EN_INFRA_2_MD),
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0220, 0x0258,
				MT6768_TOP_AXI_PROT_EN_INFRA_3_MD),
		},
		.caps = MTK_SCPD_MD_OPS | MTK_SCPD_BYPASS_INIT_ON,
	},

	[MT6768_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.sta_mask = BIT(1),
		.ctl_offs = 0x032C, //CONN_PWR_CON
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6768_TOP_AXI_PROT_EN_INFRA_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				MT6768_TOP_AXI_PROT_EN_INFRA_CONN_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6768_TOP_AXI_PROT_EN_INFRA_1_CONN),

		},
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},

	[MT6768_POWER_DOMAIN_DPY] = {
		.name = "dpy",
		.sta_mask = BIT(2),
		.ctl_offs = 0x031C,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6768_TOP_AXI_PROT_EN_INFRA_DPY),
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				MT6768_TOP_AXI_PROT_EN_INFRA_DPY_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6768_TOP_AXI_PROT_EN_INFRA_2_DPY),

		},
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},

	[MT6768_POWER_DOMAIN_DISP] = {
		.name = "disp",
		.sta_mask = BIT(5),
		.ctl_offs = 0x030C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"disp"},
		.subsys_clk_prefix = "disp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				MT6768_TOP_AXI_PROT_EN_INFRA_MM_DISP),
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				MT6768_TOP_AXI_PROT_EN_INFRA_MM_2_DISP),
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6768_TOP_AXI_PROT_EN_INFRA_DISP),
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6768_TOP_AXI_PROT_EN_INFRA_DISP_2ND),
		},
	},

	[MT6768_POWER_DOMAIN_MFG] = {
		.name = "mfg",
		.sta_mask = BIT(11),
		.ctl_offs = 0x0338,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mfg"},
		.subsys_clk_prefix = "mfg",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6768_TOP_AXI_PROT_EN_INFRA_MFG),
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6768_TOP_AXI_PROT_EN_INFRA_MFG_2ND),
		},
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6768_POWER_DOMAIN_ISP] = {
		.name = "isp",
		.sta_mask = BIT(6),
		.ctl_offs = 0x0308,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "isp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				MT6768_TOP_AXI_PROT_EN_INFRA_ISP),
			BUS_PROT_IGN(SMI_TYPE, 0x03C4, 0x03C8, 0x03C0, 0x03C0,
				MT6768_TOP_AXI_PROT_EN_SMI_ISP_2ND),
		},
	},
	[MT6768_POWER_DOMAIN_IFR] = {
		.name = "IFR",
		.sta_mask = BIT(3),
		.ctl_offs = 0x0318,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},



	[MT6768_POWER_DOMAIN_MFG_CORE0] = {
		.name = "mfg_core0",
		.sta_mask = BIT(12),
		.ctl_offs = 0x034C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},

	[MT6768_POWER_DOMAIN_MFG_CORE1] = {
		.name = "mfg_core1",
		.sta_mask = BIT(13),
		.ctl_offs = 0x310,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},

	[MT6768_POWER_DOMAIN_MFG_ASYNC] = {
		.name = "mfg_async",
		.sta_mask = BIT(14),
		.ctl_offs = 0x0334,
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},

	[MT6768_POWER_DOMAIN_CAM] = {
		.name = "cam",
		.sta_mask = BIT(7),
		.ctl_offs = 0x0344,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"cam"},
		.subsys_clk_prefix = "cam",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				MT6768_TOP_AXI_PROT_EN_INFRA_0_CAM),
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6768_TOP_AXI_PROT_EN_INFRA_2_CAM),
			BUS_PROT_IGN(SMI_TYPE, 0x03C4, 0x03C8, 0x03C0, 0x03C0,
				MT6768_TOP_AXI_PROT_EN_SMI_CAM),
		},
	},

	[MT6768_POWER_DOMAIN_VENC] = {
		.name = "venc",
		.sta_mask = BIT(9),
		.ctl_offs = 0x0304,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"venc"},
		.subsys_clk_prefix = "venc",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				MT6768_TOP_AXI_PROT_EN_INFRA_MM_VENC),
			BUS_PROT_IGN(SMI_TYPE, 0x03C4, 0x03C8, 0x03C0, 0x03C0,
				MT6768_TOP_AXI_PROT_EN_SMI_VENC),
		},
	},

	[MT6768_POWER_DOMAIN_VDEC] = {
		.name = "vdec",
		.sta_mask = BIT(8),
		.ctl_offs = 0x0370,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "vdec",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				MT6768_TOP_AXI_PROT_EN_INFRA_MM_VDEC),
			BUS_PROT_IGN(SMI_TYPE, 0x03C4, 0x03C8, 0x03C0, 0x03C0,
				MT6768_TOP_AXI_PROT_EN_SMI_VDNC),
		},
	},

};

static const struct scp_subdomain scp_subdomain_mt6768[] = {
	{MT6768_POWER_DOMAIN_MFG_ASYNC, MT6768_POWER_DOMAIN_MFG},
	{MT6768_POWER_DOMAIN_DISP, MT6768_POWER_DOMAIN_ISP},
	{MT6768_POWER_DOMAIN_MFG, MT6768_POWER_DOMAIN_MFG_CORE0},
	{MT6768_POWER_DOMAIN_MFG, MT6768_POWER_DOMAIN_MFG_CORE1},
	{MT6768_POWER_DOMAIN_DISP, MT6768_POWER_DOMAIN_CAM},
	{MT6768_POWER_DOMAIN_DISP, MT6768_POWER_DOMAIN_VDEC},
	{MT6768_POWER_DOMAIN_DISP, MT6768_POWER_DOMAIN_VENC},
};

static const struct scp_soc_data mt6768_data = {
	.domains = scp_domain_data_mt6768,
	.num_domains = MT6768_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6768,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6768),
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
		.compatible = "mediatek,mt6768-scpsys",
		.data = &mt6768_data,
	}, {
		/* sentinel */
	}
};

static int mt6768_scpsys_probe(struct platform_device *pdev)
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

static struct platform_driver mt6768_scpsys_drv = {
	.probe = mt6768_scpsys_probe,
	.driver = {
		.name = "mtk-scpsys-mt6768",
		.suppress_bind_attrs = true,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_scpsys_match_tbl),
	},
};
#if IS_BUILTIN(CONFIG_MTK_SCPSYS)
static int __init mt6768_scpsys_drv_init(void)
{
	return platform_driver_register(&mt6768_scpsys_drv);
}
subsys_initcall(mt6768_scpsys_drv_init);
#else
module_platform_driver(mt6768_scpsys_drv);
#endif
MODULE_LICENSE("GPL");
