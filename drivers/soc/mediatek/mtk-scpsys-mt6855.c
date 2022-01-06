// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

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

#include <dt-bindings/power/mt6855-power.h>

/*
 * MT6855 power domain support
 */

static const struct scp_domain_data scp_domain_data_mt6855[] = {
	[MT6855_POWER_DOMAIN_MD] = {
		.name = "md",
		.sta_mask = BIT(0),
		.ctl_offs = 0xE00,
		.caps = MTK_SCPD_MD_OPS | MTK_SCPD_BYPASS_INIT_ON,
		.extb_iso_offs = 0xF2C,
		.extb_iso_bits = 0x3,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C54, 0x0C58, 0x0C50, 0x0C5C,
				MT6855_TOP_AXI_PROT_EN_INFRASYS1_MD),
			BUS_PROT_IGN(IFR_TYPE, 0x0C64, 0x0C68, 0x0C60, 0x0C6C,
				MT6855_TOP_AXI_PROT_EN_EMISYS0_MD),
		},
	},
	[MT6855_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.sta_mask = BIT(1),
		.ctl_offs = 0xE04,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C94, 0x0C98, 0x0C90, 0x0C9C,
				MT6855_TOP_AXI_PROT_EN_MCU0_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C54, 0x0C58, 0x0C50, 0x0C5C,
				MT6855_TOP_AXI_PROT_EN_INFRASYS1_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C94, 0x0C98, 0x0C90, 0x0C9C,
				MT6855_TOP_AXI_PROT_EN_MCU0_CONN_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				MT6855_TOP_AXI_PROT_EN_INFRASYS0_CONN),
		},
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6855_POWER_DOMAIN_UFS0_SHUTDOWN] = {
		.name = "ufs0_shutdown",
		.sta_mask = BIT(4),
		.ctl_offs = 0xE10,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6855_VLP_AXI_PROT_EN_UFS0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C84, 0x0C88, 0x0C80, 0x0C8C,
				MT6855_TOP_AXI_PROT_EN_PERISYS0_UFS0),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6855_VLP_AXI_PROT_EN_UFS0_2ND),
		},
		.caps = MTK_SCPD_SRAM_ISO,
	},
	[MT6855_POWER_DOMAIN_AUDIO] = {
		.name = "audio",
		.sta_mask = BIT(5),
		.ctl_offs = 0xE14,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"audio"},
	},
	[MT6855_POWER_DOMAIN_ISP_MAIN] = {
		.name = "isp_main",
		.sta_mask = BIT(9),
		.ctl_offs = 0xE24,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"isp"},
		.subsys_clk_prefix = "isp",
	},
	[MT6855_POWER_DOMAIN_ISP_DIP1] = {
		.name = "isp_dip1",
		.sta_mask = BIT(10),
		.ctl_offs = 0xE28,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "dip1",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6855_TOP_AXI_PROT_EN_MMSYS0_ISP_DIP1),
		},
	},
	[MT6855_POWER_DOMAIN_ISP_IPE] = {
		.name = "isp_ipe",
		.sta_mask = BIT(11),
		.ctl_offs = 0xE2C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ipe"},
		.subsys_clk_prefix = "ipe",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C34, 0x0C38, 0x0C30, 0x0C3C,
				MT6855_TOP_AXI_PROT_EN_MMSYS2_ISP_IPE),
		},
	},
	[MT6855_POWER_DOMAIN_VDE0] = {
		.name = "vde0",
		.sta_mask = BIT(13),
		.ctl_offs = 0xE34,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"vde"},
		.subsys_clk_prefix = "vde",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6855_TOP_AXI_PROT_EN_MMSYS0_VDE0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6855_TOP_AXI_PROT_EN_MMSYS1_VDE0),
		},
	},
	[MT6855_POWER_DOMAIN_VEN0] = {
		.name = "ven0",
		.sta_mask = BIT(15),
		.ctl_offs = 0xE3C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ven"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6855_TOP_AXI_PROT_EN_MMSYS0_VEN0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6855_TOP_AXI_PROT_EN_MMSYS1_VEN0),
		},
	},
	[MT6855_POWER_DOMAIN_CAM_MAIN] = {
		.name = "cam_main",
		.sta_mask = BIT(17),
		.ctl_offs = 0xE44,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"cam"},
		.subsys_clk_prefix = "cam",
		.subsys_lp_clk_prefix = "cam_lp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6855_TOP_AXI_PROT_EN_MMSYS0_CAM_MAIN),
		},
	},
	[MT6855_POWER_DOMAIN_CAM_SUBA] = {
		.name = "cam_suba",
		.sta_mask = BIT(19),
		.ctl_offs = 0xE4C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_suba",
	},
	[MT6855_POWER_DOMAIN_CAM_SUBB] = {
		.name = "cam_subb",
		.sta_mask = BIT(20),
		.ctl_offs = 0xE50,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_subb",
	},
	[MT6855_POWER_DOMAIN_DISP] = {
		.name = "disp",
		.sta_mask = BIT(25),
		.ctl_offs = 0xE64,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"disp"},
		.basic_lp_clk_name = {"mdp"},
		.subsys_clk_prefix = "disp",
		.subsys_lp_clk_prefix = "mdp_lp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6855_TOP_AXI_PROT_EN_MMSYS0_DISP),
		},
	},
	[MT6855_POWER_DOMAIN_MM_INFRA] = {
		.name = "mm_infra",
		.sta_mask = BIT(27),
		.ctl_offs = 0xE6C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mm_infra"},
		.subsys_lp_clk_prefix = "mm_infra_lp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6855_TOP_AXI_PROT_EN_MMSYS1_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x0C54, 0x0C58, 0x0C50, 0x0C5C,
				MT6855_TOP_AXI_PROT_EN_INFRASYS1_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6855_TOP_AXI_PROT_EN_MMSYS1_MM_INFRA_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				MT6855_TOP_AXI_PROT_EN_INFRASYS0_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x0C64, 0x0C68, 0x0C60, 0x0C6C,
				MT6855_TOP_AXI_PROT_EN_EMISYS0_MM_INFRA),
		},
	},
	[MT6855_POWER_DOMAIN_MM_PROC_DORMANT] = {
		.name = "mm_proc_dormant",
		.sta_mask = BIT(28),
		.ctl_offs = 0xE70,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.basic_clk_name = {"mmup"},
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6855_VLP_AXI_PROT_EN_MM_PROC),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6855_VLP_AXI_PROT_EN_MM_PROC_2ND),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP,
	},
	[MT6855_POWER_DOMAIN_CSI_RX] = {
		.name = "csi_rx",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xF48,
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6855_POWER_DOMAIN_MFG1] = {
		.name = "mfg1",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEBC,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0CA4, 0x0CA8, 0x0CA0, 0x0CAC,
				MT6855_TOP_AXI_PROT_EN_MD0_MFG1),
			BUS_PROT_IGN(IFR_TYPE, 0x0CA4, 0x0CA8, 0x0CA0, 0x0CAC,
				MT6855_TOP_AXI_PROT_EN_MD0_MFG1_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C64, 0x0C68, 0x0C60, 0x0C6C,
				MT6855_TOP_AXI_PROT_EN_EMISYS0_MFG1),
			BUS_PROT_IGN(IFR_TYPE, 0x0CA4, 0x0CA8, 0x0CA0, 0x0CAC,
				MT6855_TOP_AXI_PROT_EN_MD0_MFG1_3RD),
			BUS_PROT_IGN(IFR_TYPE, 0x0CA4, 0x0CA8, 0x0CA0, 0x0CAC,
				MT6855_TOP_AXI_PROT_EN_MD0_MFG1_4RD),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6855_POWER_DOMAIN_MFG2] = {
		.name = "mfg2",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEC0,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
};

static const struct scp_subdomain scp_subdomain_mt6855[] = {
	{MT6855_POWER_DOMAIN_DISP, MT6855_POWER_DOMAIN_ISP_DIP1},
	{MT6855_POWER_DOMAIN_DISP, MT6855_POWER_DOMAIN_ISP_MAIN},
	{MT6855_POWER_DOMAIN_DISP, MT6855_POWER_DOMAIN_ISP_IPE},
	{MT6855_POWER_DOMAIN_DISP, MT6855_POWER_DOMAIN_VDE0},
	{MT6855_POWER_DOMAIN_DISP, MT6855_POWER_DOMAIN_VEN0},
	{MT6855_POWER_DOMAIN_CAM_MAIN, MT6855_POWER_DOMAIN_CAM_SUBA},
	{MT6855_POWER_DOMAIN_CAM_MAIN, MT6855_POWER_DOMAIN_CAM_SUBB},
	{MT6855_POWER_DOMAIN_MM_INFRA, MT6855_POWER_DOMAIN_DISP},
	{MT6855_POWER_DOMAIN_MM_INFRA, MT6855_POWER_DOMAIN_MM_PROC_DORMANT},
};

static const struct scp_soc_data mt6855_data = {
	.domains = scp_domain_data_mt6855,
	.num_domains = MT6855_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6855,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6855),
	.regs = {
		.pwr_sta_offs = 0xF34,
		.pwr_sta2nd_offs = 0xF38
	}
};

/*
 * scpsys driver init
 */

static const struct of_device_id of_scpsys_match_tbl[] = {
	{
		.compatible = "mediatek,mt6855-scpsys",
		.data = &mt6855_data,
	}, {
		/* sentinel */
	}
};

static int mt6855_scpsys_probe(struct platform_device *pdev)
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

static struct platform_driver mt6855_scpsys_drv = {
	.probe = mt6855_scpsys_probe,
	.driver = {
		.name = "mtk-scpsys-mt6855",
		.suppress_bind_attrs = true,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_scpsys_match_tbl),
	},
};

static int __init mt6855_scpsys_init(void)
{
	return platform_driver_register(&mt6855_scpsys_drv);
}

static void __exit mt6855_scpsys_exit(void)
{
	platform_driver_unregister(&mt6855_scpsys_drv);
}

arch_initcall(mt6855_scpsys_init);
module_exit(mt6855_scpsys_exit);
MODULE_LICENSE("GPL");
