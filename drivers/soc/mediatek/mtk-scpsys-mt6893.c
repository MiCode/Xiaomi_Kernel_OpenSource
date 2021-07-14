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

#include <dt-bindings/power/mt6893-power.h>

/*
 * MT6893 power domain support
 */

static const struct scp_domain_data scp_domain_data_mt6893[] = {
	[MT6893_POWER_DOMAIN_MFG0] = {
		.name = "mfg0",
		.sta_mask = BIT(2),
		.ctl_offs = 0x308,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mfg"},
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6893_POWER_DOMAIN_MFG1] = {
		.name = "mfg1",
		.sta_mask = BIT(3),
		.ctl_offs = 0x30C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				MT6893_TOP_AXI_PROT_EN_1_MFG1),
			BUS_PROT_IGN(IFR_TYPE, 0x0714, 0x0718, 0x0710, 0x0724,
				MT6893_TOP_AXI_PROT_EN_2_MFG1),
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6893_TOP_AXI_PROT_EN_MFG1),
			BUS_PROT_IGN(IFR_TYPE, 0x0714, 0x0718, 0x0710, 0x0724,
				MT6893_TOP_AXI_PROT_EN_2_MFG1_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0xBB8, 0xBBC, 0xBB4, 0xBC4,
				MT6893_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_MFG1),
		},
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6893_POWER_DOMAIN_MFG2] = {
		.name = "mfg2",
		.sta_mask = BIT(4),
		.ctl_offs = 0x310,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6893_POWER_DOMAIN_MFG3] = {
		.name = "mfg3",
		.sta_mask = BIT(5),
		.ctl_offs = 0x314,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6893_POWER_DOMAIN_MFG4] = {
		.name = "mfg4",
		.sta_mask = BIT(6),
		.ctl_offs = 0x318,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6893_POWER_DOMAIN_MFG5] = {
		.name = "mfg5",
		.sta_mask = BIT(7),
		.ctl_offs = 0x31C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6893_POWER_DOMAIN_MFG6] = {
		.name = "mfg6",
		.sta_mask = BIT(8),
		.ctl_offs = 0x320,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6893_POWER_DOMAIN_ISP] = {
		.name = "isp",
		.sta_mask = BIT(12),
		.ctl_offs = 0x330,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"isp"},
		.subsys_clk_prefix = "isp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0DCC, 0x0DD0, 0x0DC8, 0x0DD8,
				MT6893_TOP_AXI_PROT_EN_MM_2_ISP),
			BUS_PROT_IGN(IFR_TYPE, 0x0DCC, 0x0DD0, 0x0DC8, 0x0DD8,
				MT6893_TOP_AXI_PROT_EN_MM_2_ISP_2ND),
		},
	},
	[MT6893_POWER_DOMAIN_ISP2] = {
		.name = "isp2",
		.sta_mask = BIT(13),
		.ctl_offs = 0x334,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"isp2"},
		.subsys_clk_prefix = "isp2",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6893_TOP_AXI_PROT_EN_MM_ISP2),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6893_TOP_AXI_PROT_EN_MM_ISP2_2ND),
		},
	},
	[MT6893_POWER_DOMAIN_IPE] = {
		.name = "ipe",
		.sta_mask = BIT(14),
		.ctl_offs = 0x338,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ipe"},
		.subsys_clk_prefix = "ipe",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6893_TOP_AXI_PROT_EN_MM_IPE),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6893_TOP_AXI_PROT_EN_MM_IPE_2ND),
		},
	},
	[MT6893_POWER_DOMAIN_VDEC] = {
		.name = "vdec",
		.sta_mask = BIT(15),
		.ctl_offs = 0x33C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"vdec"},
		.subsys_clk_prefix = "vdec",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6893_TOP_AXI_PROT_EN_MM_VDEC),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6893_TOP_AXI_PROT_EN_MM_VDEC_2ND),
		},
	},
	[MT6893_POWER_DOMAIN_VDEC2] = {
		.name = "vdec2",
		.sta_mask = BIT(16),
		.ctl_offs = 0x340,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "vdec2",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0DCC, 0x0DD0, 0x0DC8, 0x0DD8,
				MT6893_TOP_AXI_PROT_EN_MM_2_VDEC2),
			BUS_PROT_IGN(IFR_TYPE, 0x0DCC, 0x0DD0, 0x0DC8, 0x0DD8,
				MT6893_TOP_AXI_PROT_EN_MM_2_VDEC2_2ND),
		},
	},
	[MT6893_POWER_DOMAIN_VENC] = {
		.name = "venc",
		.sta_mask = BIT(17),
		.ctl_offs = 0x344,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"venc"},
		.subsys_clk_prefix = "venc",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6893_TOP_AXI_PROT_EN_MM_VENC),
			BUS_PROT_IGN(IFR_TYPE, 0x0DCC, 0x0DD0, 0x0DC8, 0x0DD8,
				MT6893_TOP_AXI_PROT_EN_MM_2_VENC),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6893_TOP_AXI_PROT_EN_MM_VENC_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0DCC, 0x0DD0, 0x0DC8, 0x0DD8,
				MT6893_TOP_AXI_PROT_EN_MM_2_VENC_2ND),
		},
	},
	[MT6893_POWER_DOMAIN_VENC_CORE1] = {
		.name = "venc_core1",
		.sta_mask = BIT(18),
		.ctl_offs = 0x348,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "venc2",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6893_TOP_AXI_PROT_EN_MM_VENC_CORE1),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6893_TOP_AXI_PROT_EN_MM_VENC_CORE1_2ND),
		},
	},
	[MT6893_POWER_DOMAIN_MDP] = {
		.name = "mdp",
		.sta_mask = BIT(19),
		.ctl_offs = 0x34C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mdp"},
		.subsys_clk_prefix = "mdp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6893_TOP_AXI_PROT_EN_MDP),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6893_TOP_AXI_PROT_EN_MM_MDP),
			BUS_PROT_IGN(IFR_TYPE, 0x0DCC, 0x0DD0, 0x0DC8, 0x0DD8,
				MT6893_TOP_AXI_PROT_EN_MM_2_MDP),
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6893_TOP_AXI_PROT_EN_MDP_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6893_TOP_AXI_PROT_EN_MM_MDP_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0DCC, 0x0DD0, 0x0DC8, 0x0DD8,
				MT6893_TOP_AXI_PROT_EN_MM_2_MDP_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0xBB8, 0xBBC, 0xBB4, 0xBC4,
				MT6893_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_MDP),
		},
	},
	[MT6893_POWER_DOMAIN_DISP] = {
		.name = "disp",
		.sta_mask = BIT(20),
		.ctl_offs = 0x350,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"disp"},
		.subsys_clk_prefix = "disp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6893_TOP_AXI_PROT_EN_MM_DISP),
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6893_TOP_AXI_PROT_EN_DISP),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6893_TOP_AXI_PROT_EN_MM_DISP_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0xBB8, 0xBBC, 0xBB4, 0xBC4,
				MT6893_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_DISP),
		},
	},
	[MT6893_POWER_DOMAIN_AUDIO] = {
		.name = "audio",
		.sta_mask = BIT(21),
		.ctl_offs = 0x354,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"audio"},
		.subsys_clk_prefix = "audio",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0714, 0x0718, 0x0710, 0x0724,
				MT6893_TOP_AXI_PROT_EN_2_AUDIO),
		},
	},
	[MT6893_POWER_DOMAIN_ADSP_DORMANT] = {
		.name = "adsp_dormant",
		.sta_mask = BIT(22),
		.ctl_offs = 0x358,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.basic_clk_name = {"adsp"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0714, 0x0718, 0x0710, 0x0724,
				MT6893_TOP_AXI_PROT_EN_2_ADSP_DORMANT),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP,
	},
	[MT6893_POWER_DOMAIN_CAM] = {
		.name = "cam",
		.sta_mask = BIT(23),
		.ctl_offs = 0x35C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"cam"},
		.subsys_clk_prefix = "cam",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0714, 0x0718, 0x0710, 0x0724,
				MT6893_TOP_AXI_PROT_EN_2_CAM),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6893_TOP_AXI_PROT_EN_MM_CAM),
			BUS_PROT_IGN(IFR_TYPE, 0x02A8, 0x02AC, 0x0250, 0x0258,
				MT6893_TOP_AXI_PROT_EN_1_CAM),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6893_TOP_AXI_PROT_EN_MM_CAM_2ND),
		},
	},
	[MT6893_POWER_DOMAIN_CAM_RAWA] = {
		.name = "cam_rawa",
		.sta_mask = BIT(24),
		.ctl_offs = 0x360,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_rawa",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6893_TOP_AXI_PROT_EN_MM_CAM_RAWA),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6893_TOP_AXI_PROT_EN_MM_CAM_RAWA_2ND),
		},
	},
	[MT6893_POWER_DOMAIN_CAM_RAWB] = {
		.name = "cam_rawb",
		.sta_mask = BIT(25),
		.ctl_offs = 0x364,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_rawb",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6893_TOP_AXI_PROT_EN_MM_CAM_RAWB),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6893_TOP_AXI_PROT_EN_MM_CAM_RAWB_2ND),
		},
	},
	[MT6893_POWER_DOMAIN_CAM_RAWC] = {
		.name = "cam_rawc",
		.sta_mask = BIT(26),
		.ctl_offs = 0x368,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_rawc",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6893_TOP_AXI_PROT_EN_MM_CAM_RAWC),
			BUS_PROT_IGN(IFR_TYPE, 0x02D4, 0x02D8, 0x02D0, 0x02EC,
				MT6893_TOP_AXI_PROT_EN_MM_CAM_RAWC_2ND),
		},
	},
	[MT6893_POWER_DOMAIN_DP_TX] = {
		.name = "dp_tx",
		.sta_mask = BIT(27),
		.ctl_offs = 0x3AC,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6893_POWER_DOMAIN_MD] = {
		.name = "md",
		.sta_mask = BIT(0),
		.ctl_offs = 0x300,
		.caps = MTK_SCPD_MD_OPS,
		.extb_iso_offs = 0x398,
		.extb_iso_bits = 0x3,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6893_TOP_AXI_PROT_EN_MD),
			BUS_PROT_IGN(IFR_TYPE, 0xB84, 0xB88, 0xB80, 0xB90,
				MT6893_TOP_AXI_PROT_EN_INFRA_VDNR_MD),
			BUS_PROT_IGN(IFR_TYPE, 0xBB8, 0xBBC, 0xBB4, 0xBC4,
				MT6893_TOP_AXI_PROT_EN_SUB_INFRA_VDNR_MD),
		},
	},
	[MT6893_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.sta_mask = BIT(1),
		.ctl_offs = 0x304,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6893_TOP_AXI_PROT_EN_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x02A0, 0x02A4, 0x0220, 0x0228,
				MT6893_TOP_AXI_PROT_EN_CONN_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0714, 0x0718, 0x0710, 0x0724,
				MT6893_TOP_AXI_PROT_EN_2_CONN),
		},
	},
	[MT6893_POWER_DOMAIN_APU] = {
		.name = "apu",
		.caps = MTK_SCPD_APU_OPS,
	},
};

static const struct scp_subdomain scp_subdomain_mt6893[] = {
	{MT6893_POWER_DOMAIN_MFG0, MT6893_POWER_DOMAIN_MFG1},
	{MT6893_POWER_DOMAIN_MFG1, MT6893_POWER_DOMAIN_MFG2},
	{MT6893_POWER_DOMAIN_MFG1, MT6893_POWER_DOMAIN_MFG3},
	{MT6893_POWER_DOMAIN_MFG1, MT6893_POWER_DOMAIN_MFG4},
	{MT6893_POWER_DOMAIN_MFG1, MT6893_POWER_DOMAIN_MFG5},
	{MT6893_POWER_DOMAIN_MFG1, MT6893_POWER_DOMAIN_MFG6},
	{MT6893_POWER_DOMAIN_MDP, MT6893_POWER_DOMAIN_ISP},
	{MT6893_POWER_DOMAIN_DISP, MT6893_POWER_DOMAIN_ISP2},
	{MT6893_POWER_DOMAIN_MDP, MT6893_POWER_DOMAIN_ISP2},
	{MT6893_POWER_DOMAIN_DISP, MT6893_POWER_DOMAIN_IPE},
	{MT6893_POWER_DOMAIN_MDP, MT6893_POWER_DOMAIN_IPE},
	{MT6893_POWER_DOMAIN_DISP, MT6893_POWER_DOMAIN_VDEC},
	{MT6893_POWER_DOMAIN_MDP, MT6893_POWER_DOMAIN_VDEC2},
	{MT6893_POWER_DOMAIN_VDEC, MT6893_POWER_DOMAIN_VDEC2},
	{MT6893_POWER_DOMAIN_DISP, MT6893_POWER_DOMAIN_VENC},
	{MT6893_POWER_DOMAIN_DISP, MT6893_POWER_DOMAIN_VENC_CORE1},
	{MT6893_POWER_DOMAIN_MDP, MT6893_POWER_DOMAIN_VENC_CORE1},
	{MT6893_POWER_DOMAIN_MDP, MT6893_POWER_DOMAIN_VENC},
	{MT6893_POWER_DOMAIN_DISP, MT6893_POWER_DOMAIN_CAM},
	{MT6893_POWER_DOMAIN_MDP, MT6893_POWER_DOMAIN_CAM},
	{MT6893_POWER_DOMAIN_CAM, MT6893_POWER_DOMAIN_CAM_RAWA},
	{MT6893_POWER_DOMAIN_CAM, MT6893_POWER_DOMAIN_CAM_RAWB},
	{MT6893_POWER_DOMAIN_CAM, MT6893_POWER_DOMAIN_CAM_RAWC},
	{MT6893_POWER_DOMAIN_DISP, MT6893_POWER_DOMAIN_DP_TX},
};

static const struct scp_soc_data mt6893_data = {
	.domains = scp_domain_data_mt6893,
	.num_domains = MT6893_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6893,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6893),
	.regs = {
		.pwr_sta_offs = 0x16C,
		.pwr_sta2nd_offs = 0x170
	}
};


/*
 * scpsys driver init
 */

static const struct of_device_id of_scpsys_match_tbl[] = {
	{
		.compatible = "mediatek,mt6893-scpsys",
		.data = &mt6893_data,
	}, {
		/* sentinel */
	}
};

static int mt6893_scpsys_probe(struct platform_device *pdev)
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

static struct platform_driver mt6893_scpsys_drv = {
	.probe = mt6893_scpsys_probe,
	.driver = {
		.name = "mtk-scpsys-mt6893",
		.suppress_bind_attrs = true,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_scpsys_match_tbl),
	},
};

static int __init mt6893_scpsys_init(void)
{
	return platform_driver_register(&mt6893_scpsys_drv);
}

static void __exit mt6893_scpsys_exit(void)
{
	platform_driver_unregister(&mt6893_scpsys_drv);
}

arch_initcall(mt6893_scpsys_init);
module_exit(mt6893_scpsys_exit);
MODULE_LICENSE("GPL");
