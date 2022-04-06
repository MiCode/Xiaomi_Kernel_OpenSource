// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Ren-Ting Wang <ren-ting.wang@mediatek.com>

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

#include <dt-bindings/power/mt6895-power.h>

/*
 * MT6895 power domain support
 */

static const struct scp_domain_data scp_domain_data_mt6895[] = {
	[MT6895_POWER_DOMAIN_MD] = {
		.name = "md",
		.sta_mask = BIT(0),
		.ctl_offs = 0xE00,
		.caps = MTK_SCPD_MD_OPS | MTK_SCPD_BYPASS_INIT_ON,
		.extb_iso_offs = 0xF2C,
		.extb_iso_bits = 0x3,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C54, 0x0C58, 0x0C50, 0x0C5C,
				MT6895_TOP_AXI_PROT_EN_INFRASYS1_MD),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				MT6895_TOP_AXI_PROT_EN_INFRASYS0_MD),
			BUS_PROT_IGN(IFR_TYPE, 0x0C64, 0x0C68, 0x0C60, 0x0C6C,
				MT6895_TOP_AXI_PROT_EN_EMISYS0_MD),
		},
	},
	[MT6895_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE04,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C94, 0x0C98, 0x0C90, 0x0C9C,
				MT6895_TOP_AXI_PROT_EN_MCU0_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C54, 0x0C58, 0x0C50, 0x0C5C,
				MT6895_TOP_AXI_PROT_EN_INFRASYS1_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C94, 0x0C98, 0x0C90, 0x0C9C,
				MT6895_TOP_AXI_PROT_EN_MCU0_CONN_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				MT6895_TOP_AXI_PROT_EN_INFRASYS0_CONN),
		},
		.caps = MTK_SCPD_BYPASS_INIT_ON | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_UFS0_SHUTDOWN] = {
		.name = "ufs0_shutdown",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE10,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6895_VLP_AXI_PROT_EN_UFS0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C84, 0x0C88, 0x0C80, 0x0C8C,
				MT6895_TOP_AXI_PROT_EN_PERISYS0_UFS0),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6895_VLP_AXI_PROT_EN_UFS0_2ND),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_AUDIO] = {
		.name = "audio",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE14,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"audio"},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_ADSP_TOP_DORMANT] = {
		.name = "adsp_top_dormant",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE18,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6895_VLP_AXI_PROT_EN_ADSP_TOP),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6895_VLP_AXI_PROT_EN_ADSP_TOP_2ND),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_ADSP_INFRA] = {
		.name = "adsp_infra",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE1C,
		.basic_clk_name = {"adsp"},
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6895_VLP_AXI_PROT_EN_ADSP_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x0C54, 0x0C58, 0x0C50, 0x0C5C,
				MT6895_TOP_AXI_PROT_EN_INFRASYS1_ADSP_INFRA),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6895_VLP_AXI_PROT_EN_ADSP_INFRA_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_ISP_MAIN] = {
		.name = "isp_main",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE24,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"isp"},
		.subsys_clk_prefix = "isp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C34, 0x0C38, 0x0C30, 0x0C3C,
				MT6895_TOP_AXI_PROT_EN_MMSYS2_ISP_MAIN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C34, 0x0C38, 0x0C30, 0x0C3C,
				MT6895_TOP_AXI_PROT_EN_MMSYS2_ISP_MAIN_2ND),
		},
		.caps = MTK_SCPD_BYPASS_INIT_ON | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_ISP_DIP1] = {
		.name = "isp_dip1",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE28,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "dip1",
		.caps = MTK_SCPD_BYPASS_INIT_ON | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_ISP_IPE] = {
		.name = "isp_ipe",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE2C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ipe"},
		.subsys_clk_prefix = "ipe",
		.caps = MTK_SCPD_BYPASS_INIT_ON | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_ISP_VCORE] = {
		.name = "isp_vcore",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE30,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6895_TOP_AXI_PROT_EN_MMSYS0_ISP_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6895_TOP_AXI_PROT_EN_MMSYS1_ISP_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6895_TOP_AXI_PROT_EN_MMSYS0_ISP_VCORE_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6895_TOP_AXI_PROT_EN_MMSYS1_ISP_VCORE_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_VDE0] = {
		.name = "vde0",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE34,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"vde"},
		.subsys_clk_prefix = "vde0",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6895_TOP_AXI_PROT_EN_MMSYS0_VDE0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6895_TOP_AXI_PROT_EN_MMSYS1_VDE0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6895_TOP_AXI_PROT_EN_MMSYS0_VDE0_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6895_TOP_AXI_PROT_EN_MMSYS1_VDE0_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_VDE1] = {
		.name = "vde1",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE38,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"vde"},
		.subsys_clk_prefix = "vde1",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6895_TOP_AXI_PROT_EN_MMSYS0_VDE1),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6895_TOP_AXI_PROT_EN_MMSYS1_VDE1),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6895_TOP_AXI_PROT_EN_MMSYS0_VDE1_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6895_TOP_AXI_PROT_EN_MMSYS1_VDE1_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_VEN0] = {
		.name = "ven0",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE3C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ven"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6895_TOP_AXI_PROT_EN_MMSYS0_VEN0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6895_TOP_AXI_PROT_EN_MMSYS1_VEN0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6895_TOP_AXI_PROT_EN_MMSYS0_VEN0_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6895_TOP_AXI_PROT_EN_MMSYS1_VEN0_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_VEN1] = {
		.name = "ven1",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE40,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ven"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6895_TOP_AXI_PROT_EN_MMSYS0_VEN1),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6895_TOP_AXI_PROT_EN_MMSYS1_VEN1),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6895_TOP_AXI_PROT_EN_MMSYS0_VEN1_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6895_TOP_AXI_PROT_EN_MMSYS1_VEN1_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_CAM_MAIN] = {
		.name = "cam_main",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE44,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_lp_clk_prefix = "cam_lp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C34, 0x0C38, 0x0C30, 0x0C3C,
				MT6895_TOP_AXI_PROT_EN_MMSYS2_CAM_MAIN),
			BUS_PROT_IGN(IFR_TYPE, 0x0CC4, 0x0CC8, 0x0CC0, 0x0CCC,
				MT6895_TOP_AXI_PROT_EN_DRAMC0_CAM_MAIN),
			BUS_PROT_IGN(IFR_TYPE, 0x0C34, 0x0C38, 0x0C30, 0x0C3C,
				MT6895_TOP_AXI_PROT_EN_MMSYS2_CAM_MAIN_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0CC4, 0x0CC8, 0x0CC0, 0x0CCC,
				MT6895_TOP_AXI_PROT_EN_DRAMC0_CAM_MAIN_2ND),
		},
		.caps = MTK_SCPD_BYPASS_INIT_ON | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_CAM_MRAW] = {
		.name = "cam_mraw",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE48,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_mraw",
		.caps = MTK_SCPD_BYPASS_INIT_ON | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_CAM_SUBA] = {
		.name = "cam_suba",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE4C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_suba",
		.caps = MTK_SCPD_BYPASS_INIT_ON | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_CAM_SUBB] = {
		.name = "cam_subb",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE50,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_subb",
		.caps = MTK_SCPD_BYPASS_INIT_ON | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_CAM_SUBC] = {
		.name = "cam_subc",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE54,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_subc",
		.caps = MTK_SCPD_BYPASS_INIT_ON | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_CAM_VCORE] = {
		.name = "cam_vcore",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE58,
		.basic_clk_name = {"cam", "ccu", "ccu_ahb"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6895_TOP_AXI_PROT_EN_MMSYS0_CAM_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6895_TOP_AXI_PROT_EN_MMSYS1_CAM_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x0CC4, 0x0CC8, 0x0CC0, 0x0CCC,
				MT6895_TOP_AXI_PROT_EN_DRAMC0_CAM_VCORE),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6895_TOP_AXI_PROT_EN_MMSYS0_CAM_VCORE_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6895_TOP_AXI_PROT_EN_MMSYS1_CAM_VCORE_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				MT6895_TOP_AXI_PROT_EN_INFRASYS0_CAM_VCORE),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_MDP0] = {
		.name = "mdp0",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE5C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mdp"},
		.subsys_clk_prefix = "mdp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6895_TOP_AXI_PROT_EN_MMSYS0_MDP0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6895_TOP_AXI_PROT_EN_MMSYS1_MDP0),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6895_TOP_AXI_PROT_EN_MMSYS0_MDP0_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6895_TOP_AXI_PROT_EN_MMSYS1_MDP0_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_MDP1] = {
		.name = "mdp1",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE60,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mdp1"},
		.subsys_clk_prefix = "mdp1",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6895_TOP_AXI_PROT_EN_MMSYS0_MDP1),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6895_TOP_AXI_PROT_EN_MMSYS1_MDP1),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6895_TOP_AXI_PROT_EN_MMSYS0_MDP1_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6895_TOP_AXI_PROT_EN_MMSYS1_MDP1_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_DISP] = {
		.name = "disp",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE64,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"disp"},
		.subsys_clk_prefix = "disp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6895_TOP_AXI_PROT_EN_MMSYS0_DISP),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6895_TOP_AXI_PROT_EN_MMSYS1_DISP),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6895_TOP_AXI_PROT_EN_MMSYS0_DISP_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6895_TOP_AXI_PROT_EN_MMSYS1_DISP_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_DISP1] = {
		.name = "disp1",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE68,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"disp1"},
		.subsys_clk_prefix = "disp1",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6895_TOP_AXI_PROT_EN_MMSYS1_DISP1),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6895_TOP_AXI_PROT_EN_MMSYS1_DISP1_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_MM_INFRA] = {
		.name = "mm_infra",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE6C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mm_infra"},
		.subsys_lp_clk_prefix = "mm_infra_lp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MT6895_TOP_AXI_PROT_EN_MMSYS0_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MT6895_TOP_AXI_PROT_EN_MMSYS1_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x0C54, 0x0C58, 0x0C50, 0x0C5C,
				MT6895_TOP_AXI_PROT_EN_INFRASYS1_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x0C64, 0x0C68, 0x0C60, 0x0C6C,
				MT6895_TOP_AXI_PROT_EN_EMISYS0_MM_INFRA),
			BUS_PROT_IGN(IFR_TYPE, 0x0C74, 0x0C78, 0x0C70, 0x0C7C,
				MT6895_TOP_AXI_PROT_EN_EMISYS1_MM_INFRA),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_CLK,
	},
	[MT6895_POWER_DOMAIN_MM_PROC_DORMANT] = {
		.name = "mm_proc_dormant",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE70,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.basic_clk_name = {"mmup"},
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6895_VLP_AXI_PROT_EN_MM_PROC),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6895_VLP_AXI_PROT_EN_MM_PROC_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x0C34, 0x0C38, 0x0C30, 0x0C3C,
				MT6895_TOP_AXI_PROT_EN_MMSYS2_MM_PROC),
		},
		.sram_table = {
			SRAM_NO_ACK(0xF08, 8), SRAM_NO_ACK(0xF08, 9),
			SRAM_NO_ACK(0xF08, 10), SRAM_NO_ACK(0xF08, 11),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_DP_TX] = {
		.name = "dp_tx",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE74,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6895_POWER_DOMAIN_MFG1] = {
		.name = "mfg1",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEBC,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0CA4, 0x0CA8, 0x0CA0, 0x0CAC,
				MT6895_TOP_AXI_PROT_EN_MD0_MFG1),
			BUS_PROT_IGN(MFGRPC_TYPE, 0x0040, 0x0044, 0x0040, 0x0048,
				MT6895_MFGRPC_AXI_PROT_EN_MFG1),
			BUS_PROT_IGN(IFR_TYPE, 0x0C64, 0x0C68, 0x0C60, 0x0C6C,
				MT6895_TOP_AXI_PROT_EN_EMISYS0_MFG1),
			BUS_PROT_IGN(IFR_TYPE, 0x0C74, 0x0C78, 0x0C70, 0x0C7C,
				MT6895_TOP_AXI_PROT_EN_EMISYS1_MFG1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6895_POWER_DOMAIN_MFG2] = {
		.name = "mfg2",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEC0,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6895_POWER_DOMAIN_MFG3] = {
		.name = "mfg3",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEC4,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6895_POWER_DOMAIN_MFG4] = {
		.name = "mfg4",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEC8,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6895_POWER_DOMAIN_MFG5] = {
		.name = "mfg5",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xECC,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6895_POWER_DOMAIN_MFG6] = {
		.name = "mfg6",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xED0,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6895_POWER_DOMAIN_MFG7] = {
		.name = "mfg7",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xED4,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6895_POWER_DOMAIN_MFG8] = {
		.name = "mfg8",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xED8,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6895_POWER_DOMAIN_MFG9] = {
		.name = "mfg9",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEDC,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6895_POWER_DOMAIN_MFG10] = {
		.name = "mfg10",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEE0,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6895_POWER_DOMAIN_MFG11] = {
		.name = "mfg11",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEE4,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6895_POWER_DOMAIN_MFG12] = {
		.name = "mfg12",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEE8,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6895_POWER_DOMAIN_APU] = {
		.name = "apu",
		.caps = MTK_SCPD_APU_OPS | MTK_SCPD_BYPASS_INIT_ON,
	},
};

static const struct scp_subdomain scp_subdomain_mt6895[] = {
	{MT6895_POWER_DOMAIN_ADSP_INFRA, MT6895_POWER_DOMAIN_AUDIO},
	{MT6895_POWER_DOMAIN_ADSP_INFRA, MT6895_POWER_DOMAIN_ADSP_TOP_DORMANT},
	{MT6895_POWER_DOMAIN_ISP_VCORE, MT6895_POWER_DOMAIN_ISP_MAIN},
	{MT6895_POWER_DOMAIN_ISP_MAIN, MT6895_POWER_DOMAIN_ISP_DIP1},
	{MT6895_POWER_DOMAIN_ISP_MAIN, MT6895_POWER_DOMAIN_ISP_IPE},
	{MT6895_POWER_DOMAIN_MM_INFRA, MT6895_POWER_DOMAIN_ISP_VCORE},
	{MT6895_POWER_DOMAIN_MM_INFRA, MT6895_POWER_DOMAIN_VDE0},
	{MT6895_POWER_DOMAIN_VDE0, MT6895_POWER_DOMAIN_VDE1},
	{MT6895_POWER_DOMAIN_DISP, MT6895_POWER_DOMAIN_VEN0},
	{MT6895_POWER_DOMAIN_DISP, MT6895_POWER_DOMAIN_VEN1},
	{MT6895_POWER_DOMAIN_CAM_VCORE, MT6895_POWER_DOMAIN_CAM_MAIN},
	{MT6895_POWER_DOMAIN_CAM_MAIN, MT6895_POWER_DOMAIN_CAM_MRAW},
	{MT6895_POWER_DOMAIN_CAM_MAIN, MT6895_POWER_DOMAIN_CAM_SUBA},
	{MT6895_POWER_DOMAIN_CAM_MAIN, MT6895_POWER_DOMAIN_CAM_SUBB},
	{MT6895_POWER_DOMAIN_CAM_MAIN, MT6895_POWER_DOMAIN_CAM_SUBC},
	{MT6895_POWER_DOMAIN_MM_INFRA, MT6895_POWER_DOMAIN_CAM_VCORE},
	{MT6895_POWER_DOMAIN_MM_INFRA, MT6895_POWER_DOMAIN_MDP0},
	{MT6895_POWER_DOMAIN_MM_INFRA, MT6895_POWER_DOMAIN_MDP1},
	{MT6895_POWER_DOMAIN_MM_INFRA, MT6895_POWER_DOMAIN_DISP},
	{MT6895_POWER_DOMAIN_MM_INFRA, MT6895_POWER_DOMAIN_DISP1},
	{MT6895_POWER_DOMAIN_MM_INFRA, MT6895_POWER_DOMAIN_MM_PROC_DORMANT},
	{MT6895_POWER_DOMAIN_MM_INFRA, MT6895_POWER_DOMAIN_DP_TX},
	{MT6895_POWER_DOMAIN_MFG1, MT6895_POWER_DOMAIN_MFG2},
	{MT6895_POWER_DOMAIN_MFG2, MT6895_POWER_DOMAIN_MFG3},
	{MT6895_POWER_DOMAIN_MFG2, MT6895_POWER_DOMAIN_MFG4},
	{MT6895_POWER_DOMAIN_MFG2, MT6895_POWER_DOMAIN_MFG5},
	{MT6895_POWER_DOMAIN_MFG2, MT6895_POWER_DOMAIN_MFG6},
	{MT6895_POWER_DOMAIN_MFG3, MT6895_POWER_DOMAIN_MFG7},
	{MT6895_POWER_DOMAIN_MFG4, MT6895_POWER_DOMAIN_MFG8},
	{MT6895_POWER_DOMAIN_MFG7, MT6895_POWER_DOMAIN_MFG9},
	{MT6895_POWER_DOMAIN_MFG5, MT6895_POWER_DOMAIN_MFG10},
	{MT6895_POWER_DOMAIN_MFG6, MT6895_POWER_DOMAIN_MFG11},
	{MT6895_POWER_DOMAIN_MFG10, MT6895_POWER_DOMAIN_MFG12},
};

static const struct scp_soc_data mt6895_data = {
	.domains = scp_domain_data_mt6895,
	.num_domains = MT6895_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6895,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6895),
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
		.compatible = "mediatek,mt6895-scpsys",
		.data = &mt6895_data,
	}, {
		/* sentinel */
	}
};

static int mt6895_scpsys_probe(struct platform_device *pdev)
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

static struct platform_driver mt6895_scpsys_drv = {
	.probe = mt6895_scpsys_probe,
	.driver = {
		.name = "mtk-scpsys-mt6895",
		.suppress_bind_attrs = true,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_scpsys_match_tbl),
	},
};

static int __init mt6895_scpsys_init(void)
{
	return platform_driver_register(&mt6895_scpsys_drv);
}

static void __exit mt6895_scpsys_exit(void)
{
	platform_driver_unregister(&mt6895_scpsys_drv);
}

arch_initcall(mt6895_scpsys_init);
module_exit(mt6895_scpsys_exit);
MODULE_LICENSE("GPL");
