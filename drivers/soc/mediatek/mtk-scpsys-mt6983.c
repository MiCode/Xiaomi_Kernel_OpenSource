// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Wendell Lin <wendell.lin@mediatek.com>
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

#include <dt-bindings/power/mt6983-power.h>

/*
 * MT6983 power domain support
 */

static const struct scp_domain_data scp_domain_data_mt6983[] = {
	[MT6983_POWER_DOMAIN_MD1] = {
		.name = "md1",
		.sta_mask = BIT(0),
		.ctl_offs = 0xE00,
		.caps = MTK_SCPD_MD_OPS,
		.extb_iso_offs = 0xF2C,
		.extb_iso_bits = 0x3,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C54, 0x0C58, 0x0C50, 0x0C5C,
				MT6983_TOP_AXI_PROT_EN_1_MD1),
		},
	},
	[MT6983_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.sta_mask = BIT(1),
		.ctl_offs = 0xE04,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C94, 0x0C98, 0x0C90, 0x0C9C,
				MT6983_TOP_AXI_PROT_EN_MCU_CONNSYS_0_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C54, 0x0C58, 0x0C50, 0x0C5C,
				MT6983_TOP_AXI_PROT_EN_1_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C94, 0x0C98, 0x0C90, 0x0C9C,
				MT6983_TOP_AXI_PROT_EN_MCU_CONNSYS_0_CONN_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				MT6983_TOP_AXI_PROT_EN_0_CONN_2ND),
		},
	},
	[MT6983_POWER_DOMAIN_UFS0] = {//dormant
		.name = "ufs0",
		.sta_mask = BIT(4),
		.ctl_offs = 0xE10,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.basic_clk_name = {"ufs0_0"},
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6983_TOP_AXI_PROT_EN_VLP_UFS0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C84, 0x0C88, 0x0C80, 0x0C8C,
				MT6983_TOP_AXI_PROT_EN_PERISYS_0_UFS0_2ND),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6983_TOP_AXI_PROT_EN_VLP_UFS0_2ND),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP,
	},
	[MT6983_POWER_DOMAIN_ADSP_AO] = {
		.name = "adsp_ao",
		.sta_mask = BIT(8),
		.ctl_offs = 0xE20,
		//.sram_pdn_bits = GENMASK(8, 8),
		//.sram_pdn_ack_bits = GENMASK(12, 12),
		//.basic_clk_name = {"adsp_ao_0"},
	},
	[MT6983_POWER_DOMAIN_MM_INFRA] = {
		.name = "mm_infra",
		.sta_mask = BIT(27),
		.ctl_offs = 0xE6C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mm_infra_0"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6983_TOP_AXI_PROT_EN_MMSYS1_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x0C54, 0x0C58, 0x0C50, 0x0C5C,
				MT6983_TOP_AXI_PROT_EN_INFRASYS1_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6983_TOP_AXI_PROT_EN_MMSYS1_MM_INFRA_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				MT6983_TOP_AXI_PROT_EN_INFRASYS0_MM_INFRA),
		},
	},
	[MT6983_POWER_DOMAIN_MFG0_DORMANT] = {
		.name = "mfg0_dormant",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEB8,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0CA4, 0x0CA8, 0x0CA0, 0x0CAC,
				MT6983_TOP_AXI_PROT_EN_MD_MFGSYS_0_MFG0_DORMANT),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				MT6983_TOP_AXI_PROT_EN_0_MFG0_DORMANT_2ND),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6983_POWER_DOMAIN_ADSP_INFRA] = {
		.name = "adsp_infra",
		.sta_mask = BIT(7),
		.ctl_offs = 0xE1C,
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6983_TOP_AXI_PROT_EN_VLP_ADSP_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x0C54, 0x0C58, 0x0C50, 0x0C5C,
				MT6983_TOP_AXI_PROT_EN_1_ADSP_INFRA),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6983_TOP_AXI_PROT_EN_VLP_ADSP_INFRA_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				MT6983_TOP_AXI_PROT_EN_0_ADSP_INFRA_2ND),
		},
	},
	[MT6983_POWER_DOMAIN_MM_PROC_DORMANT] = {
		.name = "mm_proc_dormant",
		.sta_mask = BIT(28),
		.ctl_offs = 0xE70,
		.sram_slp_bits = GENMASK(12, 8),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.basic_clk_name = {"mm_proc_dormant_0"},
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6983_VLP_AXI_PROT_EN_MM_PROC),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6983_VLP_AXI_PROT_EN_MM_PROC_2ND),
		},
		.sram_table = {{0xF08, false}},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_L2TCM_SRAM,
	},
	[MT6983_POWER_DOMAIN_ISP_VCORE] = {
		.name = "isp_vcore",
		.sta_mask = BIT(12),
		.ctl_offs = 0xE30,
		.basic_clk_name = {"isp_vcore_0"},
	},
	[MT6983_POWER_DOMAIN_DIS0] = {
		.name = "disp0",
		.sta_mask = BIT(25),
		.ctl_offs = 0xE64,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"dis0_0"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6983_TOP_AXI_PROT_EN_MMSYS0_DISP),
		},
	},
	[MT6983_POWER_DOMAIN_DIS1] = {
		.name = "disp1",
		.sta_mask = BIT(26),
		.ctl_offs = 0xE68,
		.basic_clk_name = {"dis1_0"},
	},
	[MT6983_POWER_DOMAIN_CAM_VCORE] = {
		.name = "cam_vcore",
		.sta_mask = BIT(22),
		.ctl_offs = 0xE58,
		.basic_clk_name = {"cam_vcore_0"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C34, 0x0C38, 0x0C30, 0x0C3C,
				MT6983_TOP_AXI_PROT_EN_MMSYS2_CAM_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x0CC4, 0x0CC8, 0x0CC0, 0x0CCC,
				MT6983_TOP_AXI_PROT_EN_DRAMC0_CAM_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				MT6983_TOP_AXI_PROT_EN_INFRASYS0_CAM_VCORE),
		},
	},
	[MT6983_POWER_DOMAIN_MFG1] = {
		.name = "mfg1",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEBC,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0CA4, 0x0CA8, 0x0CA0, 0x0CAC,
				MT6983_TOP_AXI_PROT_EN_MD_MFGSYS_0_MFG1),
			BUS_PROT_IGN(IFR_TYPE, 0x0CA4, 0x0CA8, 0x0CA0, 0x0CAC,
				MT6983_TOP_AXI_PROT_EN_MD_MFGSYS_0_MFG1_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0CA4, 0x0CA8, 0x0CA0, 0x0CAC,
				MT6983_TOP_AXI_PROT_EN_MD_MFGSYS_0_MFG1_3RD),
			BUS_PROT_IGN(IFR_TYPE, 0x0CA4, 0x0CA8, 0x0CA0, 0x0CAC,
				MT6983_TOP_AXI_PROT_EN_MD_MFGSYS_0_MFG1_4TH),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6983_POWER_DOMAIN_ADSP_TOP_DORMANT] = {
		.name = "adsp_top_dormant",
		.sta_mask = BIT(6),
		.ctl_offs = 0xE18,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6983_TOP_AXI_PROT_EN_VLP_ADSP_TOP_DORMANT),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6983_TOP_AXI_PROT_EN_VLP_ADSP_TOP_DORMANT_2ND),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP,
	},
	[MT6983_POWER_DOMAIN_AUDIO] = {
		.name = "audio",
		.sta_mask = BIT(5),
		.ctl_offs = 0xE14,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"audio_0", "audio_1", "audio_2"},
	},
	[MT6983_POWER_DOMAIN_ISP_MAIN] = {
		.name = "isp_main",
		.sta_mask = BIT(9),
		.ctl_offs = 0xE24,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C34, 0x0C38, 0x0C30, 0x0C3C,
				MT6983_TOP_AXI_PROT_EN_MMSYS_2_ISP_MAIN),
		},
	},
	[MT6983_POWER_DOMAIN_VDE0] = {
		.name = "vdec0",
		.sta_mask = BIT(13),
		.ctl_offs = 0xE34,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"vde0_0"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6983_TOP_AXI_PROT_EN_MMSYS0_VDE0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6983_TOP_AXI_PROT_EN_MMSYS1_VDE0),
		},
	},
	[MT6983_POWER_DOMAIN_VEN0] = {
		.name = "venc0",
		.sta_mask = BIT(15),
		.ctl_offs = 0xE3C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ven0_0"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6983_TOP_AXI_PROT_EN_MMSYS0_VEN0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6983_TOP_AXI_PROT_EN_MMSYS1_VEN0),
		},
	},
	[MT6983_POWER_DOMAIN_MDP0] = {
		.name = "mdp0",
		.sta_mask = BIT(23),
		.ctl_offs = 0xE5C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mdp0_0"},
	},
	[MT6983_POWER_DOMAIN_VDE1] = {
		.name = "vdec1",
		.sta_mask = BIT(14),
		.ctl_offs = 0xE38,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"vde1_0"},
	},
	[MT6983_POWER_DOMAIN_VEN1] = {
		.name = "venc1",
		.sta_mask = BIT(16),
		.ctl_offs = 0xE40,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ven1_0"},
	},
	[MT6983_POWER_DOMAIN_MDP1] = {
		.name = "mdp1",
		.sta_mask = BIT(24),
		.ctl_offs = 0xE60,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mdp1_0"},
	},
	[MT6983_POWER_DOMAIN_CAM_MAIN] = {
		.name = "cam_main",
		.sta_mask = BIT(17),
		.ctl_offs = 0xE44,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6983_TOP_AXI_PROT_EN_MMSYS0_CAM_MAIN),
			BUS_PROT_IGN(IFR_TYPE, 0x0CC4, 0x0CC8, 0x0CC0, 0x0CCC,
				MT6983_TOP_AXI_PROT_EN_DRAMC0_CAM_MAIN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C34, 0x0C38, 0x0C30, 0x0C3C,
				MT6983_TOP_AXI_PROT_EN_MMSYS2_CAM_MAIN),
			BUS_PROT_IGN(IFR_TYPE, 0x0CC4, 0x0CC8, 0x0CC0, 0x0CCC,
				MT6983_TOP_AXI_PROT_EN_DRAMC0_CAM_MAIN_2ND),
		},
	},
	[MT6983_POWER_DOMAIN_MFG2] = {
		.name = "mfg2",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEC0,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6983_POWER_DOMAIN_MFG3] = {
		.name = "mfg3",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEC4,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6983_POWER_DOMAIN_MFG4] = {
		.name = "mfg4",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEC8,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6983_POWER_DOMAIN_MFG5] = {
		.name = "mfg5",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xECC,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6983_POWER_DOMAIN_MFG6] = {
		.name = "mfg6",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xED0,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6983_POWER_DOMAIN_MFG7] = {
		.name = "mfg7",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xED4,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6983_POWER_DOMAIN_MFG8] = {
		.name = "mfg8",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xED8,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6983_POWER_DOMAIN_MFG9] = {
		.name = "mfg9",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEDC,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6983_POWER_DOMAIN_MFG10] = {
		.name = "mfg10",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEE0,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6983_POWER_DOMAIN_MFG11] = {
		.name = "mfg11",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEE4,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6983_POWER_DOMAIN_MFG12] = {
		.name = "mfg12",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEE8,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6983_POWER_DOMAIN_MFG13] = {
		.name = "mfg13",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEEC,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6983_POWER_DOMAIN_MFG14] = {
		.name = "mfg14",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEF0,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6983_POWER_DOMAIN_MFG15] = {
		.name = "mfg15",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEF4,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6983_POWER_DOMAIN_MFG16] = {
		.name = "mfg16",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEF8,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6983_POWER_DOMAIN_MFG17] = {
		.name = "mfg17",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEFC,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6983_POWER_DOMAIN_MFG18] = {
		.name = "mfg18",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xF00,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6983_POWER_DOMAIN_ISP_DIP1] = {
		.name = "isp_dip1",
		.sta_mask = BIT(10),
		.ctl_offs = 0xE28,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT6983_POWER_DOMAIN_ISP_IPE] = {
		.name = "isp_ipe",
		.sta_mask = BIT(11),
		.ctl_offs = 0xE2C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"isp_ipe_0"},
	},
	[MT6983_POWER_DOMAIN_CAM_MRAW] = {
		.name = "cam_mraw",
		.sta_mask = BIT(18),
		.ctl_offs = 0xE48,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT6983_POWER_DOMAIN_CAM_SUBA] = {
		.name = "cam_suba",
		.sta_mask = BIT(19),
		.ctl_offs = 0xE4C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT6983_POWER_DOMAIN_CAM_SUBB] = {
		.name = "cam_subb",
		.sta_mask = BIT(20),
		.ctl_offs = 0xE50,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
	[MT6983_POWER_DOMAIN_CAM_SUBC] = {
		.name = "cam_subc",
		.sta_mask = BIT(21),
		.ctl_offs = 0xE54,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
	},
};

static const struct scp_subdomain scp_subdomain_mt6983[] = {
	//{MT6983_POWER_DOMAIN_ADSP_AO, MT6983_POWER_DOMAIN_ADSP_INFRA},
	{MT6983_POWER_DOMAIN_MM_INFRA, MT6983_POWER_DOMAIN_MM_PROC_DORMANT},
	{MT6983_POWER_DOMAIN_MM_INFRA, MT6983_POWER_DOMAIN_ISP_VCORE},
	{MT6983_POWER_DOMAIN_MM_INFRA, MT6983_POWER_DOMAIN_DIS0},
	{MT6983_POWER_DOMAIN_MM_INFRA, MT6983_POWER_DOMAIN_DIS1},
	{MT6983_POWER_DOMAIN_MM_INFRA, MT6983_POWER_DOMAIN_CAM_VCORE},
	{MT6983_POWER_DOMAIN_MFG0_DORMANT, MT6983_POWER_DOMAIN_MFG1},
	{MT6983_POWER_DOMAIN_ADSP_INFRA, MT6983_POWER_DOMAIN_ADSP_TOP_DORMANT},
	{MT6983_POWER_DOMAIN_ADSP_INFRA, MT6983_POWER_DOMAIN_AUDIO},
	{MT6983_POWER_DOMAIN_ISP_VCORE, MT6983_POWER_DOMAIN_ISP_MAIN},
	{MT6983_POWER_DOMAIN_DIS0, MT6983_POWER_DOMAIN_VDE0},
	{MT6983_POWER_DOMAIN_DIS0, MT6983_POWER_DOMAIN_VEN0},
	{MT6983_POWER_DOMAIN_DIS0, MT6983_POWER_DOMAIN_MDP0},
	{MT6983_POWER_DOMAIN_DIS1, MT6983_POWER_DOMAIN_VDE1},
	{MT6983_POWER_DOMAIN_DIS1, MT6983_POWER_DOMAIN_VEN1},
	{MT6983_POWER_DOMAIN_DIS1, MT6983_POWER_DOMAIN_MDP1},
	{MT6983_POWER_DOMAIN_CAM_VCORE, MT6983_POWER_DOMAIN_CAM_MAIN},
	{MT6983_POWER_DOMAIN_MFG1, MT6983_POWER_DOMAIN_MFG2},
	{MT6983_POWER_DOMAIN_MFG2, MT6983_POWER_DOMAIN_MFG3},
	{MT6983_POWER_DOMAIN_MFG3, MT6983_POWER_DOMAIN_MFG4},
	{MT6983_POWER_DOMAIN_MFG4, MT6983_POWER_DOMAIN_MFG5},
	{MT6983_POWER_DOMAIN_MFG5, MT6983_POWER_DOMAIN_MFG6},
	{MT6983_POWER_DOMAIN_MFG6, MT6983_POWER_DOMAIN_MFG7},
	{MT6983_POWER_DOMAIN_MFG7, MT6983_POWER_DOMAIN_MFG8},
	{MT6983_POWER_DOMAIN_MFG8, MT6983_POWER_DOMAIN_MFG9},
	{MT6983_POWER_DOMAIN_MFG9, MT6983_POWER_DOMAIN_MFG10},
	{MT6983_POWER_DOMAIN_MFG10, MT6983_POWER_DOMAIN_MFG11},
	{MT6983_POWER_DOMAIN_MFG11, MT6983_POWER_DOMAIN_MFG12},
	{MT6983_POWER_DOMAIN_MFG12, MT6983_POWER_DOMAIN_MFG13},
	{MT6983_POWER_DOMAIN_MFG13, MT6983_POWER_DOMAIN_MFG14},
	{MT6983_POWER_DOMAIN_MFG14, MT6983_POWER_DOMAIN_MFG15},
	{MT6983_POWER_DOMAIN_MFG15, MT6983_POWER_DOMAIN_MFG16},
	{MT6983_POWER_DOMAIN_MFG16, MT6983_POWER_DOMAIN_MFG17},
	{MT6983_POWER_DOMAIN_MFG17, MT6983_POWER_DOMAIN_MFG18},
	{MT6983_POWER_DOMAIN_ISP_MAIN, MT6983_POWER_DOMAIN_ISP_DIP1},
	{MT6983_POWER_DOMAIN_ISP_MAIN, MT6983_POWER_DOMAIN_ISP_IPE},
	{MT6983_POWER_DOMAIN_CAM_MAIN, MT6983_POWER_DOMAIN_CAM_MRAW},
	{MT6983_POWER_DOMAIN_CAM_MAIN, MT6983_POWER_DOMAIN_CAM_SUBA},
	{MT6983_POWER_DOMAIN_CAM_MAIN, MT6983_POWER_DOMAIN_CAM_SUBB},
	{MT6983_POWER_DOMAIN_CAM_MAIN, MT6983_POWER_DOMAIN_CAM_SUBC},
};

static const struct scp_soc_data mt6983_data = {
	.domains = scp_domain_data_mt6983,
	.num_domains = MT6983_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6983,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6983),
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
		.compatible = "mediatek,mt6983-scpsys",
		.data = &mt6983_data,
	}, {
		/* sentinel */
	}
};

static int mt6983_scpsys_probe(struct platform_device *pdev)
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

static struct platform_driver mt6983_scpsys_drv = {
	.probe = mt6983_scpsys_probe,
	.driver = {
		.name = "mtk-scpsys-mt6983",
		.suppress_bind_attrs = true,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_scpsys_match_tbl),
	},
};

static int __init mt6983_scpsys_init(void)
{
	return platform_driver_register(&mt6983_scpsys_drv);
}

static void __exit mt6983_scpsys_exit(void)
{
	platform_driver_unregister(&mt6983_scpsys_drv);
}

arch_initcall(mt6983_scpsys_init);
module_exit(mt6983_scpsys_exit);
MODULE_LICENSE("GPL");
