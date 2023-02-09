// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Chong-ming Wei <chong-ming.wei@mediatek.com>
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

#include <dt-bindings/power/mt6886-power.h>

#define MT6886_TOP_AXI_PROT_EN_MD_STA_0_MFG1	(BIT(0) | BIT(1))
#define MT6886_GPU_EB_PROT_EN_MFGSYS_SET_0_MFG1	(BIT(16) | BIT(18))
#define MT6886_TOP_AXI_PROT_EN_EMISYS_STA_0_MFG1	(BIT(19) | BIT(18))
#define MT6886_TOP_AXI_PROT_EN_INFRASYS_STA_1_MD	(BIT(9))
#define MT6886_TOP_AXI_PROT_EN_EMISYS_STA_0_MD	(BIT(17) | BIT(16))
#define MT6886_TOP_AXI_PROT_EN_MCU_STA_0_CONN	(BIT(1))
#define MT6886_TOP_AXI_PROT_EN_INFRASYS_STA_1_CONN	(BIT(12))
#define MT6886_TOP_AXI_PROT_EN_MCU_STA_0_CONN_2ND	(BIT(0))
#define MT6886_TOP_AXI_PROT_EN_INFRASYS_STA_0_CONN	(BIT(8))
#define MT6886_VLP_AXI_PROT_EN_UFS0	(BIT(7))
#define MT6886_TOP_AXI_PROT_EN_PERISYS_STA_0_UFS0	(BIT(4))
#define MT6886_VLP_AXI_PROT_EN_UFS0_2ND	(BIT(8))
#define MT6886_AXI_PROT_EN_UFS0_PHY	(BIT(0))
#define MT6886_TOP_AXI_PROT_EN_PERISYS_STA_0_AUDIO	(BIT(6))
#define MT6886_VLP_AXI_PROT_EN_ADSP_TOP	(BIT(14) | BIT(16) |  \
			BIT(17))
#define MT6886_VLP_AXI_PROT_EN_ADSP_TOP_2ND	(BIT(23) | BIT(24))
#define MT6886_VLP_AXI_PROT_EN_ADSP_INFRA	(BIT(18) | BIT(19) |  \
			BIT(22) | BIT(26) |  \
			BIT(27) | BIT(28) |  \
			BIT(29) | BIT(30))
#define MT6886_TOP_AXI_PROT_EN_INFRASYS_STA_1_ADSP_INFRA	(BIT(8))
#define MT6886_VLP_AXI_PROT_EN_ADSP_INFRA_2ND	(BIT(20) | BIT(21) |  \
			BIT(25) | BIT(31))
#define MT6886_TOP_AXI_PROT_EN_INFRASYS_STA_0_ADSP_INFRA	(BIT(4) | BIT(5))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_2_ISP_MAIN	(BIT(0) | BIT(2))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_2_ISP_MAIN_2ND	(BIT(1) | BIT(3))
#define MT6886_IMG_SUB0_PROT_EN_SMI_ISP_DIP1	(BIT(0) | BIT(1) |  \
			BIT(2) | BIT(3))
#define MT6886_IMG_SUB1_PROT_EN_SMI_ISP_DIP1	(BIT(1) | BIT(2))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_VCORE	(BIT(14) | BIT(24))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_VCORE_2ND	(BIT(15) | BIT(25))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_VDE0	(BIT(20))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_VDE0_2ND	(BIT(21))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_VEN0	(BIT(12))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_1_VEN0	(BIT(14))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_VEN0_2ND	(BIT(13))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_1_VEN0_2ND	(BIT(15))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_MAIN	(BIT(16) | BIT(26))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_1_CAM_MAIN	(BIT(18))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_2_CAM_MAIN	(BIT(5) | BIT(7) |  \
			BIT(9))
#define MT6886_TOP_AXI_PROT_EN_DRAMC_STA_0_CAM_MRAW	(BIT(10))
#define MT6886_CAM_SUB0_PROT_EN_SMI_CAM_MRAW	(BIT(5) | BIT(6))
#define MT6886_CAM_SUB2_PROT_EN_SMI_CAM_MRAW	(BIT(2) | BIT(6))
#define MT6886_CAM_SUB1_PROT_EN_SMI_CAM_MRAW	(BIT(0))
#define MT6886_TOP_AXI_PROT_EN_DRAMC_STA_0_CAM_MRAW_2ND	(BIT(11))
#define MT6886_CAM_SUB0_PROT_EN_SMI_CAM_SUBA	(BIT(2))
#define MT6886_CAM_SUB2_PROT_EN_SMI_CAM_SUBA	(BIT(1))
#define MT6886_CAM_SUB0_PROT_EN_SMI_CAM_SUBB	(BIT(3))
#define MT6886_CAM_SUB2_PROT_EN_SMI_CAM_SUBB	(BIT(3))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_VCORE	(BIT(16) | BIT(26))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_1_CAM_VCORE	(BIT(18))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_2_CAM_VCORE	(BIT(4) | BIT(6) |  \
			BIT(8))
#define MT6886_TOP_AXI_PROT_EN_DRAMC_STA_0_CAM_VCORE	(BIT(12))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_VCORE_2ND	(BIT(17) | BIT(27))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_1_CAM_VCORE_2ND	(BIT(19))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_2_CAM_VCORE_2ND	(BIT(5) | BIT(7) |  \
			BIT(9))
#define MT6886_TOP_AXI_PROT_EN_INFRASYS_STA_0_CAM_VCORE	(BIT(13))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_MDP0	(BIT(18))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_1_MDP0	(BIT(20))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_MDP0_2ND	(BIT(19))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_1_MDP0_2ND	(BIT(21))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_DISP	(BIT(8) | BIT(30))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_1_DISP	(BIT(10))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_DISP_2ND	(BIT(9) | BIT(31))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_1_DISP_2ND	(BIT(11))
#define MT6886_VLP_AXI_PROT_EN_MM_INFRA	(BIT(11))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_MM_INFRA	(BIT(2) | BIT(6) |  \
			BIT(8) | BIT(12) |  \
			BIT(14) | BIT(18) |  \
			BIT(24) | BIT(30))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_1_MM_INFRA	(BIT(4) | BIT(10) |  \
			BIT(14) | BIT(20))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_2_MM_INFRA	(BIT(4) | BIT(6) |  \
			BIT(8) | BIT(12))
#define MT6886_TOP_AXI_PROT_EN_INFRASYS_STA_1_MM_INFRA	(BIT(11))
#define MT6886_VLP_AXI_PROT_EN_MM_INFRA_2ND	(BIT(12))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_MM_INFRA_2ND	(BIT(9) | BIT(13) |  \
			BIT(15) | BIT(17) |  \
			BIT(19) | BIT(25) |  \
			BIT(27) | BIT(31))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_1_MM_INFRA_2ND	(BIT(9) | BIT(11) |  \
			BIT(15) | BIT(19) |  \
			BIT(21))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_2_MM_INFRA_2ND	(BIT(17))
#define MT6886_TOP_AXI_PROT_EN_INFRASYS_STA_0_MM_INFRA	(BIT(14))
#define MT6886_TOP_AXI_PROT_EN_EMISYS_STA_0_MM_INFRA	(BIT(21) | BIT(20))
#define MT6886_AXI_PROT_EN_MM_INFRA	(BIT(4) | BIT(5))
#define MT6886_VLP_AXI_PROT_EN_MM_PROC	(BIT(11) | BIT(13))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_2_MM_PROC	(BIT(12))
#define MT6886_VLP_AXI_PROT_EN_MM_PROC_2ND	(BIT(12))
#define MT6886_TOP_AXI_PROT_EN_MMSYS_STA_2_MM_PROC_2ND	(BIT(17))

enum regmap_type {
	INVALID_TYPE = 0,
	IFR_TYPE = 1,
	GPU_EB_RPC_TYPE = 2,
	VLP_TYPE = 3,
	UFSCFG_AO_TYPE = 4,
	IMG_SUB0_TYPE = 5,
	IMG_SUB1_TYPE = 6,
	CAM_SUB0_TYPE = 7,
	CAM_SUB2_TYPE = 8,
	CAM_SUB1_TYPE = 9,
	EMI_TYPE = 10,
	BUS_TYPE_NUM,
};

static const char *bus_list[BUS_TYPE_NUM] = {
	[IFR_TYPE] = "infracfg",
	[GPU_EB_RPC_TYPE] = "gpu_eb_rpc",
	[VLP_TYPE] = "vlpcfg",
	[UFSCFG_AO_TYPE] = "ufscfg-ao-bus",
	[IMG_SUB0_TYPE] = "img_sub0_bus",
	[IMG_SUB1_TYPE] = "img_sub1_bus",
	[CAM_SUB0_TYPE] = "cam_sub0_bus",
	[CAM_SUB2_TYPE] = "cam_sub2_bus",
	[CAM_SUB1_TYPE] = "cam_sub1_bus",
	[EMI_TYPE] = "emi-bus",
};

/*
 * MT6886 power domain support
 */

static const struct scp_domain_data scp_domain_mt6886_gpu_eb_rpc_data[] = {
	[MT6886_POWER_DOMAIN_MFG1] = {
		.name = "mfg1",
		.ctl_offs = 0x70,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0CA4, 0x0CA8, 0x0CA0, 0x0CAC,
				MT6886_TOP_AXI_PROT_EN_MD_STA_0_MFG1),
			BUS_PROT_IGN(GPU_EB_RPC_TYPE, 0x40, 0x44, 0x40, 0x48,
				MT6886_GPU_EB_PROT_EN_MFGSYS_SET_0_MFG1),
			BUS_PROT_IGN(IFR_TYPE, 0x0C64, 0x0C68, 0x0C60, 0x0C6C,
				MT6886_TOP_AXI_PROT_EN_EMISYS_STA_0_MFG1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6886_POWER_DOMAIN_MFG2] = {
		.name = "mfg2",
		.ctl_offs = 0xA0,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6886_POWER_DOMAIN_MFG9] = {
		.name = "mfg9",
		.ctl_offs = 0xBC,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6886_POWER_DOMAIN_MFG10] = {
		.name = "mfg10",
		.ctl_offs = 0xC0,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6886_POWER_DOMAIN_MFG11] = {
		.name = "mfg11",
		.ctl_offs = 0xC4,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6886_POWER_DOMAIN_MFG12] = {
		.name = "mfg12",
		.ctl_offs = 0xC8,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
};

static const struct scp_subdomain scp_subdomain_mt6886_gpu_eb_rpc[] = {
	{MT6886_POWER_DOMAIN_MFG1, MT6886_POWER_DOMAIN_MFG2},
	{MT6886_POWER_DOMAIN_MFG2, MT6886_POWER_DOMAIN_MFG9},
	{MT6886_POWER_DOMAIN_MFG2, MT6886_POWER_DOMAIN_MFG10},
	{MT6886_POWER_DOMAIN_MFG2, MT6886_POWER_DOMAIN_MFG11},
	{MT6886_POWER_DOMAIN_MFG2, MT6886_POWER_DOMAIN_MFG12},
};

static const struct scp_soc_data mt6886_gpu_eb_rpc_data = {
	.domains = scp_domain_mt6886_gpu_eb_rpc_data,
	.num_domains = MT6886_GPU_EB_RPC_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6886_gpu_eb_rpc,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6886_gpu_eb_rpc),
	.regs = {
		.pwr_sta_offs = 0xF88,
		.pwr_sta2nd_offs = 0xF8C,
	}
};

static const struct scp_domain_data scp_domain_mt6886_spm_data[] = {
	[MT6886_POWER_DOMAIN_MD] = {
		.name = "md",
		.ctl_offs = 0xE00,
		.extb_iso_offs = 0xF70,
		.extb_iso_bits = 0x3,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C54, 0x0C58, 0x0C50, 0x0C5C,
				MT6886_TOP_AXI_PROT_EN_INFRASYS_STA_1_MD),
			BUS_PROT_IGN(IFR_TYPE, 0x0C64, 0x0C68, 0x0C60, 0x0C6C,
				MT6886_TOP_AXI_PROT_EN_EMISYS_STA_0_MD),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_MD_OPS | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6886_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.ctl_offs = 0xE04,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C94, 0x0C98, 0x0C90, 0x0C9C,
				MT6886_TOP_AXI_PROT_EN_MCU_STA_0_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C54, 0x0C58, 0x0C50, 0x0C5C,
				MT6886_TOP_AXI_PROT_EN_INFRASYS_STA_1_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C94, 0x0C98, 0x0C90, 0x0C9C,
				MT6886_TOP_AXI_PROT_EN_MCU_STA_0_CONN_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				MT6886_TOP_AXI_PROT_EN_INFRASYS_STA_0_CONN),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6886_POWER_DOMAIN_UFS0_SHUTDOWN] = {
		.name = "ufs0_shutdown",
		.ctl_offs = 0xE10,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6886_VLP_AXI_PROT_EN_UFS0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C84, 0x0C88, 0x0C80, 0x0C8C,
				MT6886_TOP_AXI_PROT_EN_PERISYS_STA_0_UFS0),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6886_VLP_AXI_PROT_EN_UFS0_2ND),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6886_POWER_DOMAIN_UFS0_PHY] = {
		.name = "ufs0_phy",
		.ctl_offs = 0xE14,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(UFSCFG_AO_TYPE, 0x54, 0x58, 0x50, 0x5c,
				MT6886_AXI_PROT_EN_UFS0_PHY),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6886_POWER_DOMAIN_AUDIO] = {
		.name = "audio",
		.ctl_offs = 0xE2C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"audio"},
		.subsys_clk_prefix = "audio",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C84, 0x0C88, 0x0C80, 0x0C8C,
				MT6886_TOP_AXI_PROT_EN_PERISYS_STA_0_AUDIO),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6886_POWER_DOMAIN_ADSP_TOP_DORMANT] = {
		.name = "adsp_top_dormant",
		.ctl_offs = 0xE30,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6886_VLP_AXI_PROT_EN_ADSP_TOP),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6886_VLP_AXI_PROT_EN_ADSP_TOP_2ND),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6886_POWER_DOMAIN_ADSP_INFRA] = {
		.name = "adsp_infra",
		.ctl_offs = 0xE34,
		.basic_clk_name = {"adsp"},
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6886_VLP_AXI_PROT_EN_ADSP_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x0C54, 0x0C58, 0x0C50, 0x0C5C,
				MT6886_TOP_AXI_PROT_EN_INFRASYS_STA_1_ADSP_INFRA),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6886_VLP_AXI_PROT_EN_ADSP_INFRA_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				MT6886_TOP_AXI_PROT_EN_INFRASYS_STA_0_ADSP_INFRA),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6886_POWER_DOMAIN_ISP_MAIN] = {
		.name = "isp_main",
		.ctl_offs = 0xE3C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"isp"},
		.subsys_clk_prefix = "isp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C34, 0x0C38, 0x0C30, 0x0C3C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_2_ISP_MAIN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C34, 0x0C38, 0x0C30, 0x0C3C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_2_ISP_MAIN_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6886_POWER_DOMAIN_ISP_DIP1] = {
		.name = "isp_dip1",
		.ctl_offs = 0xE40,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "dip1",
		.bp_table = {
			BUS_PROT_IGN(IMG_SUB0_TYPE, 0x3c4, 0x3c8, 0x3c0, 0x3c0,
				MT6886_IMG_SUB0_PROT_EN_SMI_ISP_DIP1),
			BUS_PROT_IGN(IMG_SUB1_TYPE, 0x3c4, 0x3c8, 0x3c0, 0x3c0,
				MT6886_IMG_SUB1_PROT_EN_SMI_ISP_DIP1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6886_POWER_DOMAIN_ISP_VCORE] = {
		.name = "isp_vcore",
		.ctl_offs = 0xE48,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_ISP_VCORE_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6886_POWER_DOMAIN_VDE0] = {
		.name = "vde0",
		.ctl_offs = 0xE4C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"vde"},
		.subsys_clk_prefix = "vde0",
		.bp_table = {
			BUS_PROT(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_VDE0),
			BUS_PROT(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_VDE0_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6886_POWER_DOMAIN_VEN0] = {
		.name = "ven0",
		.ctl_offs = 0xE54,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ven"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_VEN0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_1_VEN0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_VEN0_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_1_VEN0_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6886_POWER_DOMAIN_CAM_MAIN] = {
		.name = "cam_main",
		.ctl_offs = 0xE60,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam",
		.subsys_lp_clk_prefix = "cam_lp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_MAIN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_1_CAM_MAIN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C34, 0x0C38, 0x0C30, 0x0C3C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_2_CAM_MAIN),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6886_POWER_DOMAIN_CAM_MRAW] = {
		.name = "cam_mraw",
		.ctl_offs = 0xE64,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_mraw",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0CC4, 0x0CC8, 0x0CC0, 0x0CCC,
				MT6886_TOP_AXI_PROT_EN_DRAMC_STA_0_CAM_MRAW),
			BUS_PROT_IGN(CAM_SUB0_TYPE, 0x3c4, 0x3c8, 0x3c0, 0x3c0,
				MT6886_CAM_SUB0_PROT_EN_SMI_CAM_MRAW),
			BUS_PROT_IGN(CAM_SUB2_TYPE, 0x3c4, 0x3c8, 0x3c0, 0x3c0,
				MT6886_CAM_SUB2_PROT_EN_SMI_CAM_MRAW),
			BUS_PROT_IGN(CAM_SUB1_TYPE, 0x3c4, 0x3c8, 0x3c0, 0x3c0,
				MT6886_CAM_SUB1_PROT_EN_SMI_CAM_MRAW),
			BUS_PROT_IGN(IFR_TYPE, 0x0CC4, 0x0CC8, 0x0CC0, 0x0CCC,
				MT6886_TOP_AXI_PROT_EN_DRAMC_STA_0_CAM_MRAW_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6886_POWER_DOMAIN_CAM_SUBA] = {
		.name = "cam_suba",
		.ctl_offs = 0xE68,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_suba",
		.bp_table = {
			BUS_PROT_IGN(CAM_SUB0_TYPE, 0x3c4, 0x3c8, 0x3c0, 0x3c0,
				MT6886_CAM_SUB0_PROT_EN_SMI_CAM_SUBA),
			BUS_PROT_IGN(CAM_SUB2_TYPE, 0x3c4, 0x3c8, 0x3c0, 0x3c0,
				MT6886_CAM_SUB2_PROT_EN_SMI_CAM_SUBA),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6886_POWER_DOMAIN_CAM_SUBB] = {
		.name = "cam_subb",
		.ctl_offs = 0xE6C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_subb",
		.bp_table = {
			BUS_PROT_IGN(CAM_SUB0_TYPE, 0x3c4, 0x3c8, 0x3c0, 0x3c0,
				MT6886_CAM_SUB0_PROT_EN_SMI_CAM_SUBB),
			BUS_PROT_IGN(CAM_SUB2_TYPE, 0x3c4, 0x3c8, 0x3c0, 0x3c0,
				MT6886_CAM_SUB2_PROT_EN_SMI_CAM_SUBB),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6886_POWER_DOMAIN_CAM_VCORE] = {
		.name = "cam_vcore",
		.ctl_offs = 0xE74,
		.basic_clk_name = {"cam", "ccu", "ccu_ahb"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_1_CAM_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x0C34, 0x0C38, 0x0C30, 0x0C3C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_2_CAM_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x0CC4, 0x0CC8, 0x0CC0, 0x0CCC,
				MT6886_TOP_AXI_PROT_EN_DRAMC_STA_0_CAM_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_CAM_VCORE_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_1_CAM_VCORE_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C34, 0x0C38, 0x0C30, 0x0C3C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_2_CAM_VCORE_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				MT6886_TOP_AXI_PROT_EN_INFRASYS_STA_0_CAM_VCORE),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6886_POWER_DOMAIN_MDP0] = {
		.name = "mdp0",
		.ctl_offs = 0xE78,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mdp"},
		.subsys_clk_prefix = "mdp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_MDP0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_1_MDP0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_MDP0_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_1_MDP0_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6886_POWER_DOMAIN_DISP] = {
		.name = "disp",
		.ctl_offs = 0xE80,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"disp"},
		.subsys_clk_prefix = "disp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_DISP),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_1_DISP),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_DISP_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_1_DISP_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6886_POWER_DOMAIN_MM_INFRA] = {
		.name = "mm_infra",
		.ctl_offs = 0xE90,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mm_infra"},
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6886_VLP_AXI_PROT_EN_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_1_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x0C34, 0x0C38, 0x0C30, 0x0C3C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_2_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x0C54, 0x0C58, 0x0C50, 0x0C5C,
				MT6886_TOP_AXI_PROT_EN_INFRASYS_STA_1_MM_INFRA),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6886_VLP_AXI_PROT_EN_MM_INFRA_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_0_MM_INFRA_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_1_MM_INFRA_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C34, 0x0C38, 0x0C30, 0x0C3C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_2_MM_INFRA_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				MT6886_TOP_AXI_PROT_EN_INFRASYS_STA_0_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x0C64, 0x0C68, 0x0C60, 0x0C6C,
				MT6886_TOP_AXI_PROT_EN_EMISYS_STA_0_MM_INFRA),
			BUS_PROT_IGN(EMI_TYPE, 0x84, 0x88, 0x80, 0x8c,
				MT6886_AXI_PROT_EN_MM_INFRA),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_CLK,
	},
	[MT6886_POWER_DOMAIN_MM_PROC_DORMANT] = {
		.name = "mm_proc_dormant",
		.ctl_offs = 0xE94,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.basic_clk_name = {"mmup"},
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6886_VLP_AXI_PROT_EN_MM_PROC),
			BUS_PROT_IGN(IFR_TYPE, 0x0C34, 0x0C38, 0x0C30, 0x0C3C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_2_MM_PROC),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6886_VLP_AXI_PROT_EN_MM_PROC_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C34, 0x0C38, 0x0C30, 0x0C3C,
				MT6886_TOP_AXI_PROT_EN_MMSYS_STA_2_MM_PROC_2ND),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6886_POWER_DOMAIN_APU] = {
		.name = "apu",
		.caps = MTK_SCPD_APU_OPS | MTK_SCPD_BYPASS_INIT_ON,
	},
};

static const struct scp_subdomain scp_subdomain_mt6886_spm[] = {
	{MT6886_POWER_DOMAIN_ADSP_INFRA, MT6886_POWER_DOMAIN_ADSP_TOP_DORMANT},
	{MT6886_POWER_DOMAIN_ISP_VCORE, MT6886_POWER_DOMAIN_ISP_MAIN},
	{MT6886_POWER_DOMAIN_ISP_MAIN, MT6886_POWER_DOMAIN_ISP_DIP1},
	{MT6886_POWER_DOMAIN_MM_PROC_DORMANT, MT6886_POWER_DOMAIN_ISP_VCORE},
	{MT6886_POWER_DOMAIN_MM_PROC_DORMANT, MT6886_POWER_DOMAIN_VDE0},
	{MT6886_POWER_DOMAIN_MM_INFRA, MT6886_POWER_DOMAIN_VEN0},
	{MT6886_POWER_DOMAIN_CAM_VCORE, MT6886_POWER_DOMAIN_CAM_MAIN},
	{MT6886_POWER_DOMAIN_CAM_MAIN, MT6886_POWER_DOMAIN_CAM_MRAW},
	{MT6886_POWER_DOMAIN_CAM_MAIN, MT6886_POWER_DOMAIN_CAM_SUBA},
	{MT6886_POWER_DOMAIN_CAM_MAIN, MT6886_POWER_DOMAIN_CAM_SUBB},
	{MT6886_POWER_DOMAIN_MM_PROC_DORMANT, MT6886_POWER_DOMAIN_CAM_VCORE},
	{MT6886_POWER_DOMAIN_MM_INFRA, MT6886_POWER_DOMAIN_MDP0},
	{MT6886_POWER_DOMAIN_MM_INFRA, MT6886_POWER_DOMAIN_DISP},
	{MT6886_POWER_DOMAIN_MM_INFRA, MT6886_POWER_DOMAIN_MM_PROC_DORMANT},
};

static const struct scp_soc_data mt6886_spm_data = {
	.domains = scp_domain_mt6886_spm_data,
	.num_domains = MT6886_SPM_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6886_spm,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6886_spm),
	.regs = {
		.pwr_sta_offs = 0xF78,
		.pwr_sta2nd_offs = 0xF7C,
	}
};

/*
 * scpsys driver init
 */

static const struct of_device_id of_scpsys_match_tbl[] = {
	{
		.compatible = "mediatek,mt6886-gpusys",
		.data = &mt6886_gpu_eb_rpc_data,
	}, {
		.compatible = "mediatek,mt6886-scpsys",
		.data = &mt6886_spm_data,
	}, {
		/* sentinel */
	}
};

static int mt6886_scpsys_probe(struct platform_device *pdev)
{
	const struct scp_subdomain *sd;
	const struct scp_soc_data *soc;
	struct scp *scp;
	struct genpd_onecell_data *pd_data;
	int i, ret;

	soc = of_device_get_match_data(&pdev->dev);

	scp = init_scp(pdev, soc->domains, soc->num_domains, &soc->regs, bus_list, BUS_TYPE_NUM);
	if (IS_ERR(scp))
		return PTR_ERR(scp);

	ret = mtk_register_power_domains(pdev, scp, soc->num_domains);
	if (ret)
		return ret;

	pd_data = &scp->pd_data;

	for (i = 0, sd = soc->subdomains; i < soc->num_subdomains; i++, sd++) {
		ret = pm_genpd_add_subdomain(pd_data->domains[sd->origin],
					     pd_data->domains[sd->subdomain]);
		if (ret && IS_ENABLED(CONFIG_PM)) {
			dev_err(&pdev->dev, "Failed to add subdomain: %d\n",
				ret);
			return ret;
		}
	}

	return 0;
}

static struct platform_driver mt6886_scpsys_drv = {
	.probe = mt6886_scpsys_probe,
	.driver = {
		.name = "mtk-scpsys-mt6886",
		.suppress_bind_attrs = true,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_scpsys_match_tbl),
	},
};

module_platform_driver(mt6886_scpsys_drv);
MODULE_LICENSE("GPL");
