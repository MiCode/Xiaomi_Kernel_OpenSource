// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
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

#include <dt-bindings/power/mt6985-power.h>

#define MT6985_TOP_AXI_PROT_EN_MFGSYS0_MFG1	(BIT(0) | BIT(1) |  \
			BIT(2) | BIT(3))
#define MT6985_GPU_EB_PROT_EN_MFGSYS_RPC0_MFG1	(BIT(16) | BIT(17) |  \
			BIT(18) | BIT(19))
#define MT6985_TOP_AXI_PROT_EN_EMISYS0_MFG1	(BIT(19) | BIT(20))
#define MT6985_TOP_AXI_PROT_EN_EMISYS1_MFG1	(BIT(19) | BIT(20))
#define MT6985_TOP_AXI_PROT_EN_INFRASYS1_MD	(BIT(12))
#define MT6985_TOP_AXI_PROT_EN_INFRASYS0_MD	(BIT(11))
#define MT6985_TOP_AXI_PROT_EN_EMISYS0_MD	(BIT(17) | BIT(18))
#define MT6985_TOP_AXI_PROT_EN_EMISYS1_MD	(BIT(17) | BIT(18))
#define MT6985_TOP_AXI_PROT_EN_INFRASYS0_CONN	(BIT(25))
#define MT6985_TOP_AXI_PROT_EN_CONNSYS0_CONN	(BIT(1))
#define MT6985_TOP_AXI_PROT_EN_INFRASYS0_CONN_2ND	(BIT(26))
#define MT6985_TOP_AXI_PROT_EN_CONNSYS0_CONN_2ND	(BIT(0))
#define MT6985_VLP_AXI_PROT_EN_UFS0	(BIT(7))
#define MT6985_TOP_AXI_PROT_EN_PERISYS0_UFS0	(BIT(4))
#define MT6985_VLP_AXI_PROT_EN_UFS0_2ND	(BIT(8))
#define MT6985_UFSCFG_AO_PROT_EN_UFS_UFS0_PHY	(BIT(0))
#define MT6985_VLP_AXI_PROT_EN1_PEXTP_MAC0	(BIT(11))
#define MT6985_VLP_AXI_PROT_EN1_PEXTP_MAC1	(BIT(10))
#define MT6985_VLP_AXI_PROT_EN1_PEXTP_PHY0	(BIT(13))
#define MT6985_VLP_AXI_PROT_EN1_PEXTP_PHY1	(BIT(12))
#define MT6985_TOP_AXI_PROT_EN_PERISYS0_AUDIO	(BIT(8))
#define MT6985_TOP_AXI_PROT_EN_PERISYS0_AUDIO_2ND	(BIT(5))
#define MT6985_TOP_AXI_PROT_EN_INFRASYS1_ADSP_TOP	(BIT(14))
#define MT6985_TOP_AXI_PROT_EN_PERISYS0_ADSP_TOP	(BIT(8))
#define MT6985_VLP_AXI_PROT_EN_ADSP_TOP	(BIT(4) | BIT(5))
#define MT6985_VLP_AXI_PROT_EN_ADSP_TOP_2ND	(BIT(10))
#define MT6985_TOP_AXI_PROT_EN_INFRASYS0_ADSP_TOP	(BIT(29) | BIT(30))
#define MT6985_TOP_AXI_PROT_EN_MMSYS2_ISP_MAIN	(BIT(0) | BIT(2))
#define MT6985_TOP_AXI_PROT_EN_MMSYS2_ISP_MAIN_2ND	(BIT(1) | BIT(3))
#define MT6985_IMG_SUB0_PROT_EN_SMI_ISP_DIP1	(BIT(0) | BIT(1) |  \
			BIT(2) | BIT(3))
#define MT6985_IMG_SUB1_PROT_EN_SMI_ISP_DIP1	(BIT(1) | BIT(2))
#define MT6985_TOP_AXI_PROT_EN_MMSYS0_ISP_VCORE	(BIT(14) | BIT(24))
#define MT6985_TOP_AXI_PROT_EN_MMSYS0_ISP_VCORE_2ND	(BIT(15) | BIT(25))
#define MT6985_TOP_AXI_PROT_EN_MMSYS0_VDE0	(BIT(10))
#define MT6985_TOP_AXI_PROT_EN_MMSYS1_VDE0	(BIT(30))
#define MT6985_TOP_AXI_PROT_EN_MMSYS1_VDE0_2ND	(BIT(27) | BIT(29))
#define MT6985_TOP_AXI_PROT_EN_MMSYS0_VDE1	(BIT(20))
#define MT6985_TOP_AXI_PROT_EN_MMSYS1_VDE1	(BIT(25))
#define MT6985_TOP_AXI_PROT_EN_MMSYS0_VEN0	(BIT(12))
#define MT6985_TOP_AXI_PROT_EN_MMSYS1_VEN0	(BIT(14))
#define MT6985_TOP_AXI_PROT_EN_MMSYS0_VEN0_2ND	(BIT(13))
#define MT6985_TOP_AXI_PROT_EN_MMSYS1_VEN0_2ND	(BIT(15))
#define MT6985_TOP_AXI_PROT_EN_MMSYS0_VEN1	(BIT(22))
#define MT6985_TOP_AXI_PROT_EN_MMSYS1_VEN1	(BIT(16))
#define MT6985_TOP_AXI_PROT_EN_MMSYS0_VEN1_2ND	(BIT(23))
#define MT6985_TOP_AXI_PROT_EN_MMSYS1_VEN1_2ND	(BIT(17))
#define MT6985_TOP_AXI_PROT_EN_MMSYS2_VEN2	(BIT(18) | BIT(19))
#define MT6985_TOP_AXI_PROT_EN_MMSYS0_CAM_MAIN	(BIT(16) | BIT(26) |  \
			BIT(30))
#define MT6985_TOP_AXI_PROT_EN_MMSYS1_CAM_MAIN	(BIT(18))
#define MT6985_TOP_AXI_PROT_EN_MMSYS2_CAM_MAIN	(BIT(4) | BIT(6) |  \
			BIT(8) | BIT(10))
#define MT6985_CAM_SUB0_PROT_EN_SMI_CAM_MRAW	(BIT(3) | BIT(4))
#define MT6985_CAM_SUB2_PROT_EN_SMI_CAM_MRAW	(BIT(4) | BIT(5))
#define MT6985_CAM_SUB1_PROT_EN_SMI_CAM_MRAW	(BIT(1))
#define MT6985_TOP_AXI_PROT_EN_CCUSYS0_CAM_MRAW	(BIT(10))
#define MT6985_TOP_AXI_PROT_EN_CCUSYS0_CAM_MRAW_2ND	(BIT(11))
#define MT6985_CAM_SUB0_PROT_EN_SMI_CAM_SUBA	(BIT(0) | BIT(5))
#define MT6985_CAM_SUB2_PROT_EN_SMI_CAM_SUBA	(BIT(2))
#define MT6985_CAM_SUB0_PROT_EN_SMI_CAM_SUBB	(BIT(2))
#define MT6985_CAM_SUB2_PROT_EN_SMI_CAM_SUBB	(BIT(1))
#define MT6985_CAM_SUB0_PROT_EN_SMI_CAM_SUBC	(BIT(1) | BIT(6))
#define MT6985_CAM_SUB2_PROT_EN_SMI_CAM_SUBC	(BIT(3))
#define MT6985_TOP_AXI_PROT_EN_MMSYS2_CAM_VCORE	(BIT(5) | BIT(7) |  \
			BIT(9) | BIT(11))
#define MT6985_TOP_AXI_PROT_EN_CCUSYS0_CAM_VCORE	(BIT(12))
#define MT6985_TOP_AXI_PROT_EN_INFRASYS0_CAM_VCORE	(BIT(27))
#define MT6985_TOP_AXI_PROT_EN_MMSYS0_CAM_VCORE	(BIT(17) | BIT(27) |  \
			BIT(31))
#define MT6985_TOP_AXI_PROT_EN_MMSYS1_CAM_VCORE	(BIT(19))
#define MT6985_TOP_AXI_PROT_EN_MMSYS0_MDP0	(BIT(18))
#define MT6985_TOP_AXI_PROT_EN_MMSYS1_MDP0	(BIT(20))
#define MT6985_TOP_AXI_PROT_EN_MMSYS0_MDP0_2ND	(BIT(19))
#define MT6985_TOP_AXI_PROT_EN_MMSYS1_MDP0_2ND	(BIT(21))
#define MT6985_TOP_AXI_PROT_EN_EMISYS0_MDP0	(BIT(11))
#define MT6985_TOP_AXI_PROT_EN_EMISYS1_MDP0	(BIT(11))
#define MT6985_TOP_AXI_PROT_EN_MMSYS0_MDP1	(BIT(28))
#define MT6985_TOP_AXI_PROT_EN_MMSYS1_MDP1	(BIT(22))
#define MT6985_TOP_AXI_PROT_EN_MMSYS0_MDP1_2ND	(BIT(29))
#define MT6985_TOP_AXI_PROT_EN_MMSYS1_MDP1_2ND	(BIT(23))
#define MT6985_TOP_AXI_PROT_EN_MMSYS1_DIS0	(BIT(10) | BIT(12))
#define MT6985_TOP_AXI_PROT_EN_MMSYS2_DIS0	(BIT(22) | BIT(24))
#define MT6985_TOP_AXI_PROT_EN_MMSYS2_DIS0_2ND	(BIT(23) | BIT(25))
#define MT6985_TOP_AXI_PROT_EN_EMISYS0_DIS0	(BIT(12))
#define MT6985_TOP_AXI_PROT_EN_EMISYS1_DIS0	(BIT(12))
#define MT6985_TOP_AXI_PROT_EN_MMSYS1_DIS1	(BIT(0) | BIT(2))
#define MT6985_TOP_AXI_PROT_EN_MMSYS2_DIS1	(BIT(28) | BIT(30))
#define MT6985_TOP_AXI_PROT_EN_MMSYS2_DIS1_2ND	(BIT(29) | BIT(31))
#define MT6985_TOP_AXI_PROT_EN_MMSYS2_OVLSYS	(BIT(20) | BIT(22) |  \
			BIT(24))
#define MT6985_TOP_AXI_PROT_EN_MMSYS2_OVLSYS_2ND	(BIT(23) | BIT(25))
#define MT6985_TOP_AXI_PROT_EN_MMSYS2_OVLSYS1	(BIT(26) | BIT(28) |  \
			BIT(30))
#define MT6985_TOP_AXI_PROT_EN_MMSYS2_OVLSYS1_2ND	(BIT(29) | BIT(31))
#define MT6985_VLP_AXI_PROT_EN_MM_INFRA	(BIT(11))
#define MT6985_TOP_AXI_PROT_EN_INFRASYS1_MM_INFRA	(BIT(10))
#define MT6985_TOP_AXI_PROT_EN_MMSYS0_MM_INFRA	(BIT(0) | BIT(2) |  \
			BIT(4) | BIT(6) |  \
			BIT(10) | BIT(12) |  \
			BIT(14) | BIT(18) |  \
			BIT(20) | BIT(22) |  \
			BIT(24) | BIT(28))
#define MT6985_TOP_AXI_PROT_EN_MMSYS1_MM_INFRA	(BIT(0) | BIT(2) |  \
			BIT(4) | BIT(6) |  \
			BIT(10) | BIT(12) |  \
			BIT(14) | BIT(16) |  \
			BIT(20) | BIT(22) |  \
			BIT(24) | BIT(26) |  \
			BIT(28) | BIT(30))
#define MT6985_TOP_AXI_PROT_EN_MMSYS2_MM_INFRA	(BIT(5) | BIT(7) |  \
			BIT(9) | BIT(11) |  \
			BIT(12) | BIT(20) |  \
			BIT(26))
#define MT6985_VLP_AXI_PROT_EN_MM_INFRA_2ND	(BIT(12))
#define MT6985_TOP_AXI_PROT_EN_INFRASYS0_MM_INFRA	(BIT(8) | BIT(9))
#define MT6985_TOP_AXI_PROT_EN_MMSYS0_MM_INFRA_2ND	(BIT(11) | BIT(13) |  \
			BIT(15) | BIT(17) |  \
			BIT(19) | BIT(21) |  \
			BIT(23) | BIT(25) |  \
			BIT(27) | BIT(29) |  \
			BIT(31))
#define MT6985_TOP_AXI_PROT_EN_MMSYS1_MM_INFRA_2ND	(BIT(1) | BIT(3) |  \
			BIT(9) | BIT(11) |  \
			BIT(13) | BIT(15) |  \
			BIT(17) | BIT(19) |  \
			BIT(21) | BIT(23) |  \
			BIT(31))
#define MT6985_TOP_AXI_PROT_EN_MMSYS2_MM_INFRA_2ND	(BIT(15) | BIT(21) |  \
			BIT(27))
#define MT6985_TOP_AXI_PROT_EN_EMISYS0_MM_INFRA	(BIT(21) | BIT(22))
#define MT6985_TOP_AXI_PROT_EN_EMISYS1_MM_INFRA	(BIT(21) | BIT(22))
#define MT6985_IFR_MEM_PROT_EN_EMISYS_MM_INFRA	(BIT(4) | BIT(5))
#define MT6985_SEMI_PROT_EN_EMISYS_MM_INFRA	(BIT(4) | BIT(5))
#define MT6985_VLP_AXI_PROT_EN_MM_PROC	(BIT(11))
#define MT6985_TOP_AXI_PROT_EN_MMSYS2_MM_PROC	(BIT(12))
#define MT6985_VLP_AXI_PROT_EN_MM_PROC_2ND	(BIT(12))
#define MT6985_TOP_AXI_PROT_EN_MMSYS2_MM_PROC_2ND	(BIT(15))
#define MT6985_TOP_AXI_PROT_EN_MMSYS1_VDE_VCORE0	(BIT(26) | BIT(28))
#define MT6985_TOP_AXI_PROT_EN_MMSYS0_VDE_VCORE0	(BIT(11))
#define MT6985_TOP_AXI_PROT_EN_MMSYS1_VDE_VCORE0_2ND	(BIT(31))
#define MT6985_TOP_AXI_PROT_EN_MMSYS1_VDE_VCORE1	(BIT(24))
#define MT6985_TOP_AXI_PROT_EN_MMSYS0_VDE_VCORE1	(BIT(21))

enum regmap_type {
	INVALID_TYPE = 0,
	IFR_TYPE = 1,
	IFR_MEM_TYPE = 2,
	GPU_EB_RPC_TYPE = 3,
	VLP_TYPE = 4,
	UFSCFG_AO_TYPE = 5,
	IMG_SUB0_TYPE = 6,
	IMG_SUB1_TYPE = 7,
	CAM_SUB0_TYPE = 8,
	CAM_SUB2_TYPE = 9,
	CAM_SUB1_TYPE = 10,
	SEMI_TYPE = 11,
	BUS_TYPE_NUM,
};

static const char *bus_list[BUS_TYPE_NUM] = {
	[IFR_TYPE] = "ifr_bus",
	[IFR_MEM_TYPE] = "ifr_mem_bus",
	[GPU_EB_RPC_TYPE] = "gpu_eb_rpc",
	[VLP_TYPE] = "vlpcfg",
	[UFSCFG_AO_TYPE] = "ufscfg_ao_bus",
	[IMG_SUB0_TYPE] = "img_sub0_bus",
	[IMG_SUB1_TYPE] = "img_sub1_bus",
	[CAM_SUB0_TYPE] = "cam_sub0_bus",
	[CAM_SUB2_TYPE] = "cam_sub2_bus",
	[CAM_SUB1_TYPE] = "cam_sub1_bus",
	[SEMI_TYPE] = "semi_bus",
};

/*
 * MT6985 power domain support
 */

static const struct scp_domain_data scp_domain_mt6985_gpu_eb_rpc_data[] = {
	[MT6985_POWER_DOMAIN_MFG1] = {
		.name = "mfg1",
		.ctl_offs = 0x070,
		.sram_pdn_bits = GENMASK(8, 8),
		.basic_clk_name = {"mfg_ref", "mfgsc_ref"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x1a4, 0x1a8, 0x1a0, 0x1ac,
				MT6985_TOP_AXI_PROT_EN_MFGSYS0_MFG1),
			BUS_PROT_IGN(GPU_EB_RPC_TYPE, 0x40, 0x44, 0x40, 0x48,
				MT6985_GPU_EB_PROT_EN_MFGSYS_RPC0_MFG1),
			BUS_PROT_IGN(IFR_TYPE, 0x124, 0x128, 0x120, 0x12c,
				MT6985_TOP_AXI_PROT_EN_EMISYS0_MFG1),
			BUS_PROT_IGN(IFR_TYPE, 0x104, 0x108, 0x100, 0x10c,
				MT6985_TOP_AXI_PROT_EN_EMISYS1_MFG1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6985_POWER_DOMAIN_MFG2] = {
		.name = "mfg2",
		.ctl_offs = 0x0A0,
		.sram_pdn_bits = GENMASK(8, 8),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6985_POWER_DOMAIN_MFG3] = {
		.name = "mfg3",
		.ctl_offs = 0x0A4,
		.sram_pdn_bits = GENMASK(8, 8),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6985_POWER_DOMAIN_MFG4] = {
		.name = "mfg4",
		.ctl_offs = 0x0A8,
		.sram_pdn_bits = GENMASK(8, 8),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6985_POWER_DOMAIN_MFG5] = {
		.name = "mfg5",
		.ctl_offs = 0x0AC,
		.sram_pdn_bits = GENMASK(8, 8),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6985_POWER_DOMAIN_MFG6] = {
		.name = "mfg6",
		.ctl_offs = 0x0B0,
		.sram_pdn_bits = GENMASK(8, 8),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6985_POWER_DOMAIN_MFG7] = {
		.name = "mfg7",
		.ctl_offs = 0x0B4,
		.sram_pdn_bits = GENMASK(8, 8),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6985_POWER_DOMAIN_MFG8] = {
		.name = "mfg8",
		.ctl_offs = 0x0B8,
		.sram_pdn_bits = GENMASK(8, 8),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6985_POWER_DOMAIN_MFG9] = {
		.name = "mfg9",
		.ctl_offs = 0x0BC,
		.sram_pdn_bits = GENMASK(8, 8),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6985_POWER_DOMAIN_MFG10] = {
		.name = "mfg10",
		.ctl_offs = 0x0C0,
		.sram_pdn_bits = GENMASK(8, 8),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6985_POWER_DOMAIN_MFG11] = {
		.name = "mfg11",
		.ctl_offs = 0x0C4,
		.sram_pdn_bits = GENMASK(8, 8),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6985_POWER_DOMAIN_MFG12] = {
		.name = "mfg12",
		.ctl_offs = 0x0C8,
		.sram_pdn_bits = GENMASK(8, 8),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6985_POWER_DOMAIN_MFG13] = {
		.name = "mfg13",
		.ctl_offs = 0x0CC,
		.sram_pdn_bits = GENMASK(8, 8),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6985_POWER_DOMAIN_MFG14] = {
		.name = "mfg14",
		.ctl_offs = 0x0D0,
		.sram_pdn_bits = GENMASK(8, 8),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6985_POWER_DOMAIN_MFG15] = {
		.name = "mfg15",
		.ctl_offs = 0x0D4,
		.sram_pdn_bits = GENMASK(8, 8),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6985_POWER_DOMAIN_MFG16] = {
		.name = "mfg16",
		.ctl_offs = 0x0D8,
		.sram_pdn_bits = GENMASK(8, 8),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6985_POWER_DOMAIN_MFG17] = {
		.name = "mfg17",
		.ctl_offs = 0x0DC,
		.sram_pdn_bits = GENMASK(8, 8),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6985_POWER_DOMAIN_MFG18] = {
		.name = "mfg18",
		.ctl_offs = 0x0E0,
		.sram_pdn_bits = GENMASK(8, 8),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6985_POWER_DOMAIN_MFG19] = {
		.name = "mfg19",
		.ctl_offs = 0x0E4,
		.sram_pdn_bits = GENMASK(8, 8),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
};

static const struct scp_subdomain scp_subdomain_mt6985_gpu_eb_rpc[] = {
	{MT6985_POWER_DOMAIN_MFG1, MT6985_POWER_DOMAIN_MFG2},
	{MT6985_POWER_DOMAIN_MFG2, MT6985_POWER_DOMAIN_MFG3},
	{MT6985_POWER_DOMAIN_MFG2, MT6985_POWER_DOMAIN_MFG4},
	{MT6985_POWER_DOMAIN_MFG2, MT6985_POWER_DOMAIN_MFG5},
	{MT6985_POWER_DOMAIN_MFG2, MT6985_POWER_DOMAIN_MFG6},
	{MT6985_POWER_DOMAIN_MFG2, MT6985_POWER_DOMAIN_MFG7},
	{MT6985_POWER_DOMAIN_MFG2, MT6985_POWER_DOMAIN_MFG8},
	{MT6985_POWER_DOMAIN_MFG3, MT6985_POWER_DOMAIN_MFG9},
	{MT6985_POWER_DOMAIN_MFG4, MT6985_POWER_DOMAIN_MFG10},
	{MT6985_POWER_DOMAIN_MFG5, MT6985_POWER_DOMAIN_MFG11},
	{MT6985_POWER_DOMAIN_MFG3, MT6985_POWER_DOMAIN_MFG12},
	{MT6985_POWER_DOMAIN_MFG4, MT6985_POWER_DOMAIN_MFG13},
	{MT6985_POWER_DOMAIN_MFG5, MT6985_POWER_DOMAIN_MFG14},
	{MT6985_POWER_DOMAIN_MFG6, MT6985_POWER_DOMAIN_MFG15},
	{MT6985_POWER_DOMAIN_MFG7, MT6985_POWER_DOMAIN_MFG16},
	{MT6985_POWER_DOMAIN_MFG8, MT6985_POWER_DOMAIN_MFG17},
	{MT6985_POWER_DOMAIN_MFG7, MT6985_POWER_DOMAIN_MFG18},
	{MT6985_POWER_DOMAIN_MFG8, MT6985_POWER_DOMAIN_MFG19},
};

static const struct scp_soc_data mt6985_gpu_eb_rpc_data = {
	.domains = scp_domain_mt6985_gpu_eb_rpc_data,
	.num_domains = MT6985_GPU_EB_RPC_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6985_gpu_eb_rpc,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6985_gpu_eb_rpc),
	.regs = {
		.pwr_sta_offs = 0xF94,
		.pwr_sta2nd_offs = 0xF98,
	}
};

static const struct scp_domain_data scp_domain_mt6985_spm_data[] = {
	[MT6985_POWER_DOMAIN_MD] = {
		.name = "md",
		.ctl_offs = 0xE00,
		.extb_iso_offs = 0xF74,
		.extb_iso_bits = 0x3,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x024, 0x028, 0x020, 0x02c,
				MT6985_TOP_AXI_PROT_EN_INFRASYS1_MD),
			BUS_PROT_IGN(IFR_TYPE, 0x004, 0x008, 0x000, 0x00c,
				MT6985_TOP_AXI_PROT_EN_INFRASYS0_MD),
			BUS_PROT_IGN(IFR_TYPE, 0x124, 0x128, 0x120, 0x12c,
				MT6985_TOP_AXI_PROT_EN_EMISYS0_MD),
			BUS_PROT_IGN(IFR_TYPE, 0x104, 0x108, 0x100, 0x10c,
				MT6985_TOP_AXI_PROT_EN_EMISYS1_MD),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_MD_OPS | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6985_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.ctl_offs = 0xE04,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x004, 0x008, 0x000, 0x00c,
				MT6985_TOP_AXI_PROT_EN_INFRASYS0_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x1c4, 0x1c8, 0x1c0, 0x1cc,
				MT6985_TOP_AXI_PROT_EN_CONNSYS0_CONN),
			BUS_PROT_IGN(IFR_TYPE, 0x004, 0x008, 0x000, 0x00c,
				MT6985_TOP_AXI_PROT_EN_INFRASYS0_CONN_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x1c4, 0x1c8, 0x1c0, 0x1cc,
				MT6985_TOP_AXI_PROT_EN_CONNSYS0_CONN_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6985_POWER_DOMAIN_UFS0_SHUTDOWN] = {
		.name = "ufs0_shutdown",
		.ctl_offs = 0xE10,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6985_VLP_AXI_PROT_EN_UFS0),
			BUS_PROT_IGN(IFR_TYPE, 0x0e4, 0x0e8, 0x0e0, 0x0ec,
				MT6985_TOP_AXI_PROT_EN_PERISYS0_UFS0),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6985_VLP_AXI_PROT_EN_UFS0_2ND),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6985_POWER_DOMAIN_UFS0_PHY_SHUTDOWN] = {
		.name = "ufs0_phy_shutdown",
		.ctl_offs = 0xE14,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(UFSCFG_AO_TYPE, 0x54, 0x58, 0x50, 0x5c,
				MT6985_UFSCFG_AO_PROT_EN_UFS_UFS0_PHY),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6985_POWER_DOMAIN_PEXTP_MAC0_SHUTDOWN] = {
		.name = "pextp_mac0_shutdown",
		.ctl_offs = 0xE18,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "pextp0",
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6985_VLP_AXI_PROT_EN1_PEXTP_MAC0),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6985_POWER_DOMAIN_PEXTP_MAC1_SHUTDOWN] = {
		.name = "pextp_mac1_shutdown",
		.ctl_offs = 0xE1C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "pextp1",
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6985_VLP_AXI_PROT_EN1_PEXTP_MAC1),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6985_POWER_DOMAIN_PEXTP_PHY0] = {
		.name = "pextp_phy0",
		.ctl_offs = 0xE20,
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6985_VLP_AXI_PROT_EN1_PEXTP_PHY0),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6985_POWER_DOMAIN_PEXTP_PHY1] = {
		.name = "pextp_phy1",
		.ctl_offs = 0xE24,
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0234, 0x0238, 0x0230, 0x0240,
				MT6985_VLP_AXI_PROT_EN1_PEXTP_PHY1),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6985_POWER_DOMAIN_AUDIO] = {
		.name = "audio",
		.ctl_offs = 0xE2C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"audio"},
		.subsys_clk_prefix = "audio",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0e4, 0x0e8, 0x0e0, 0x0ec,
				MT6985_TOP_AXI_PROT_EN_PERISYS0_AUDIO),
			BUS_PROT_IGN(IFR_TYPE, 0x0e4, 0x0e8, 0x0e0, 0x0ec,
				MT6985_TOP_AXI_PROT_EN_PERISYS0_AUDIO_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6985_POWER_DOMAIN_ADSP_TOP_DORMANT] = {
		.name = "adsp_top_dormant",
		.ctl_offs = 0xE30,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.basic_clk_name = {"adsp", "aud_bus"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x024, 0x028, 0x020, 0x02c,
				MT6985_TOP_AXI_PROT_EN_INFRASYS1_ADSP_TOP),
			BUS_PROT_IGN(IFR_TYPE, 0x0e4, 0x0e8, 0x0e0, 0x0ec,
				MT6985_TOP_AXI_PROT_EN_PERISYS0_ADSP_TOP),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6985_VLP_AXI_PROT_EN_ADSP_TOP),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6985_VLP_AXI_PROT_EN_ADSP_TOP_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x004, 0x008, 0x000, 0x00c,
				MT6985_TOP_AXI_PROT_EN_INFRASYS0_ADSP_TOP),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6985_POWER_DOMAIN_ISP_MAIN] = {
		.name = "isp_main",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 8,
		.basic_clk_name = {"isp"},
		.caps = MTK_SCPD_HWV_OPS,
	},
	[MT6985_POWER_DOMAIN_ISP_DIP1] = {
		.name = "isp_dip1",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 13,
		.caps = MTK_SCPD_HWV_OPS,
	},
	[MT6985_POWER_DOMAIN_ISP_VCORE] = {
		.name = "isp_vcore",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 3,
		.caps = MTK_SCPD_HWV_OPS,
	},
	[MT6985_POWER_DOMAIN_VDE0] = {
		.name = "vde0",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 7,
		.basic_clk_name = {"vde"},
		.caps = MTK_SCPD_HWV_OPS,
	},
	[MT6985_POWER_DOMAIN_VDE1] = {
		.name = "vde1",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 14,
		.basic_clk_name = {"vde"},
		.caps = MTK_SCPD_HWV_OPS,
	},
	[MT6985_POWER_DOMAIN_VEN0] = {
		.name = "ven0",
		.ctl_offs = 0xE54,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ven"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6985_TOP_AXI_PROT_EN_MMSYS0_VEN0),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6985_TOP_AXI_PROT_EN_MMSYS1_VEN0),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6985_TOP_AXI_PROT_EN_MMSYS0_VEN0_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6985_TOP_AXI_PROT_EN_MMSYS1_VEN0_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6985_POWER_DOMAIN_VEN1] = {
		.name = "ven1",
		.ctl_offs = 0xE58,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ven"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6985_TOP_AXI_PROT_EN_MMSYS0_VEN1),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6985_TOP_AXI_PROT_EN_MMSYS1_VEN1),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6985_TOP_AXI_PROT_EN_MMSYS0_VEN1_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6985_TOP_AXI_PROT_EN_MMSYS1_VEN1_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6985_POWER_DOMAIN_VEN2] = {
		.name = "ven2",
		.ctl_offs = 0xE5C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ven"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6985_TOP_AXI_PROT_EN_MMSYS2_VEN2),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6985_POWER_DOMAIN_CAM_MAIN] = {
		.name = "cam_main",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 9,
		.caps = MTK_SCPD_HWV_OPS,
	},
	[MT6985_POWER_DOMAIN_CAM_MRAW] = {
		.name = "cam_mraw",
		.ctl_offs = 0xE64,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_mraw",
		.bp_table = {
			BUS_PROT_IGN(CAM_SUB0_TYPE, 0x3c4, 0x3c8, 0x3c0, 0x3c0,
				MT6985_CAM_SUB0_PROT_EN_SMI_CAM_MRAW),
			BUS_PROT_IGN(CAM_SUB2_TYPE, 0x3c4, 0x3c8, 0x3c0, 0x3c0,
				MT6985_CAM_SUB2_PROT_EN_SMI_CAM_MRAW),
			BUS_PROT_IGN(CAM_SUB1_TYPE, 0x3c4, 0x3c8, 0x3c0, 0x3c0,
				MT6985_CAM_SUB1_PROT_EN_SMI_CAM_MRAW),
			BUS_PROT_IGN(IFR_TYPE, 0x264, 0x268, 0x260, 0x26c,
				MT6985_TOP_AXI_PROT_EN_CCUSYS0_CAM_MRAW),
			BUS_PROT_IGN(IFR_TYPE, 0x264, 0x268, 0x260, 0x26c,
				MT6985_TOP_AXI_PROT_EN_CCUSYS0_CAM_MRAW_2ND),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6985_POWER_DOMAIN_CAM_SUBA] = {
		.name = "cam_suba",
		.ctl_offs = 0xE68,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_suba",
		.bp_table = {
			BUS_PROT_IGN(CAM_SUB0_TYPE, 0x3c4, 0x3c8, 0x3c0, 0x3c0,
				MT6985_CAM_SUB0_PROT_EN_SMI_CAM_SUBA),
			BUS_PROT_IGN(CAM_SUB2_TYPE, 0x3c4, 0x3c8, 0x3c0, 0x3c0,
				MT6985_CAM_SUB2_PROT_EN_SMI_CAM_SUBA),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6985_POWER_DOMAIN_CAM_SUBB] = {
		.name = "cam_subb",
		.ctl_offs = 0xE6C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_subb",
		.bp_table = {
			BUS_PROT_IGN(CAM_SUB0_TYPE, 0x3c4, 0x3c8, 0x3c0, 0x3c0,
				MT6985_CAM_SUB0_PROT_EN_SMI_CAM_SUBB),
			BUS_PROT_IGN(CAM_SUB2_TYPE, 0x3c4, 0x3c8, 0x3c0, 0x3c0,
				MT6985_CAM_SUB2_PROT_EN_SMI_CAM_SUBB),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6985_POWER_DOMAIN_CAM_SUBC] = {
		.name = "cam_subc",
		.ctl_offs = 0xE70,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam_subc",
		.bp_table = {
			BUS_PROT_IGN(CAM_SUB0_TYPE, 0x3c4, 0x3c8, 0x3c0, 0x3c0,
				MT6985_CAM_SUB0_PROT_EN_SMI_CAM_SUBC),
			BUS_PROT_IGN(CAM_SUB2_TYPE, 0x3c4, 0x3c8, 0x3c0, 0x3c0,
				MT6985_CAM_SUB2_PROT_EN_SMI_CAM_SUBC),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6985_POWER_DOMAIN_CAM_VCORE] = {
		.name = "cam_vcore",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 4,
		.basic_clk_name = {"cam", "ccu", "ccu_ahb"},
		.caps = MTK_SCPD_HWV_OPS,
	},
	[MT6985_POWER_DOMAIN_MDP0_SHUTDOWN] = {
		.name = "mdp0_shutdown",
		.ctl_offs = 0xE78,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mdp"},
		.subsys_clk_prefix = "mdp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6985_TOP_AXI_PROT_EN_MMSYS0_MDP0),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6985_TOP_AXI_PROT_EN_MMSYS1_MDP0),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6985_TOP_AXI_PROT_EN_MMSYS0_MDP0_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6985_TOP_AXI_PROT_EN_MMSYS1_MDP0_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x124, 0x128, 0x120, 0x12c,
				MT6985_TOP_AXI_PROT_EN_EMISYS0_MDP0),
			BUS_PROT_IGN(IFR_TYPE, 0x104, 0x108, 0x100, 0x10c,
				MT6985_TOP_AXI_PROT_EN_EMISYS1_MDP0),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6985_POWER_DOMAIN_MDP1_SHUTDOWN] = {
		.name = "mdp1_shutdown",
		.ctl_offs = 0xE7C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mdp1"},
		.subsys_clk_prefix = "mdp1",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6985_TOP_AXI_PROT_EN_MMSYS0_MDP1),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6985_TOP_AXI_PROT_EN_MMSYS1_MDP1),
			BUS_PROT_IGN(IFR_TYPE, 0x1e4, 0x1e8, 0x1e0, 0x1ec,
				MT6985_TOP_AXI_PROT_EN_MMSYS0_MDP1_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6985_TOP_AXI_PROT_EN_MMSYS1_MDP1_2ND),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6985_POWER_DOMAIN_DIS0_SHUTDOWN] = {
		.name = "dis0_shutdown",
		.ctl_offs = 0xE80,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"disp"},
		.subsys_clk_prefix = "disp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6985_TOP_AXI_PROT_EN_MMSYS1_DIS0),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6985_TOP_AXI_PROT_EN_MMSYS2_DIS0),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6985_TOP_AXI_PROT_EN_MMSYS2_DIS0_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x124, 0x128, 0x120, 0x12c,
				MT6985_TOP_AXI_PROT_EN_EMISYS0_DIS0),
			BUS_PROT_IGN(IFR_TYPE, 0x104, 0x108, 0x100, 0x10c,
				MT6985_TOP_AXI_PROT_EN_EMISYS1_DIS0),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6985_POWER_DOMAIN_DIS1_SHUTDOWN] = {
		.name = "dis1_shutdown",
		.ctl_offs = 0xE84,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"disp1"},
		.subsys_clk_prefix = "disp1",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x204, 0x208, 0x200, 0x20c,
				MT6985_TOP_AXI_PROT_EN_MMSYS1_DIS1),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6985_TOP_AXI_PROT_EN_MMSYS2_DIS1),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6985_TOP_AXI_PROT_EN_MMSYS2_DIS1_2ND),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6985_POWER_DOMAIN_OVLSYS_SHUTDOWN] = {
		.name = "ovlsys_shutdown",
		.ctl_offs = 0xE88,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ovl"},
		.subsys_clk_prefix = "ovl",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6985_TOP_AXI_PROT_EN_MMSYS2_OVLSYS),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6985_TOP_AXI_PROT_EN_MMSYS2_OVLSYS_2ND),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6985_POWER_DOMAIN_OVLSYS1_SHUTDOWN] = {
		.name = "ovlsys1_shutdown",
		.ctl_offs = 0xE8C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ovl1"},
		.subsys_clk_prefix = "ovl1",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6985_TOP_AXI_PROT_EN_MMSYS2_OVLSYS1),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6985_TOP_AXI_PROT_EN_MMSYS2_OVLSYS1_2ND),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6985_POWER_DOMAIN_MM_INFRA] = {
		.name = "mm_infra",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 0,
		.basic_clk_name = {"mm_infra"},
		.caps = MTK_SCPD_HWV_OPS,
	},
	[MT6985_POWER_DOMAIN_MM_PROC_DORMANT] = {
		.name = "mm_proc_dormant",
		.ctl_offs = 0xE94,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.basic_clk_name = {"mmup"},
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6985_VLP_AXI_PROT_EN_MM_PROC),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6985_TOP_AXI_PROT_EN_MMSYS2_MM_PROC),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MT6985_VLP_AXI_PROT_EN_MM_PROC_2ND),
			BUS_PROT_IGN(IFR_TYPE, 0x224, 0x228, 0x220, 0x22c,
				MT6985_TOP_AXI_PROT_EN_MMSYS2_MM_PROC_2ND),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6985_POWER_DOMAIN_DP_TX] = {
		.name = "dp_tx",
		.ctl_offs = 0xE98,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6985_POWER_DOMAIN_CSI_RX] = {
		.name = "csi_rx",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 1,
		.caps = MTK_SCPD_HWV_OPS,
	},
	[MT6985_POWER_DOMAIN_VDE_VCORE0] = {
		.name = "vde_vcore0",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 2,
		.caps = MTK_SCPD_HWV_OPS,
	},
	[MT6985_POWER_DOMAIN_VDE_VCORE1] = {
		.name = "vde_vcore1",
		.hwv_set_ofs = 0x0198,
		.hwv_clr_ofs = 0x019C,
		.hwv_done_ofs = 0x141C,
		.hwv_en_ofs = 0x1410,
		.hwv_set_sta_ofs = 0x146C,
		.hwv_clr_sta_ofs = 0x1470,
		.hwv_shift = 12,
		.caps = MTK_SCPD_HWV_OPS,
	},
	[MT6985_POWER_DOMAIN_APU] = {
		.name = "apu",
		.caps = MTK_SCPD_APU_OPS | MTK_SCPD_BYPASS_INIT_ON,
	},
};

static const struct scp_subdomain scp_subdomain_mt6985_spm[] = {
	{MT6985_POWER_DOMAIN_ISP_VCORE, MT6985_POWER_DOMAIN_ISP_MAIN},
	{MT6985_POWER_DOMAIN_ISP_MAIN, MT6985_POWER_DOMAIN_ISP_DIP1},
	{MT6985_POWER_DOMAIN_MM_INFRA, MT6985_POWER_DOMAIN_ISP_VCORE},
	{MT6985_POWER_DOMAIN_VDE_VCORE0, MT6985_POWER_DOMAIN_VDE0},
	{MT6985_POWER_DOMAIN_VDE_VCORE1, MT6985_POWER_DOMAIN_VDE1},
	{MT6985_POWER_DOMAIN_MM_INFRA, MT6985_POWER_DOMAIN_VEN0},
	{MT6985_POWER_DOMAIN_MM_INFRA, MT6985_POWER_DOMAIN_VEN1},
	{MT6985_POWER_DOMAIN_VEN1, MT6985_POWER_DOMAIN_VEN2},
	{MT6985_POWER_DOMAIN_CAM_VCORE, MT6985_POWER_DOMAIN_CAM_MAIN},
	{MT6985_POWER_DOMAIN_CAM_MAIN, MT6985_POWER_DOMAIN_CAM_MRAW},
	{MT6985_POWER_DOMAIN_CAM_MAIN, MT6985_POWER_DOMAIN_CAM_SUBA},
	{MT6985_POWER_DOMAIN_CAM_MAIN, MT6985_POWER_DOMAIN_CAM_SUBB},
	{MT6985_POWER_DOMAIN_CAM_MAIN, MT6985_POWER_DOMAIN_CAM_SUBC},
	{MT6985_POWER_DOMAIN_MM_INFRA, MT6985_POWER_DOMAIN_CAM_VCORE},
	{MT6985_POWER_DOMAIN_MM_INFRA, MT6985_POWER_DOMAIN_MDP0_SHUTDOWN},
	{MT6985_POWER_DOMAIN_MM_INFRA, MT6985_POWER_DOMAIN_MDP1_SHUTDOWN},
	{MT6985_POWER_DOMAIN_MM_INFRA, MT6985_POWER_DOMAIN_DIS0_SHUTDOWN},
	{MT6985_POWER_DOMAIN_MM_INFRA, MT6985_POWER_DOMAIN_DIS1_SHUTDOWN},
	{MT6985_POWER_DOMAIN_DIS0_SHUTDOWN, MT6985_POWER_DOMAIN_OVLSYS_SHUTDOWN},
	{MT6985_POWER_DOMAIN_DIS1_SHUTDOWN, MT6985_POWER_DOMAIN_OVLSYS1_SHUTDOWN},
	{MT6985_POWER_DOMAIN_MM_INFRA, MT6985_POWER_DOMAIN_MM_PROC_DORMANT},
	{MT6985_POWER_DOMAIN_MM_INFRA, MT6985_POWER_DOMAIN_DP_TX},
	{MT6985_POWER_DOMAIN_MM_INFRA, MT6985_POWER_DOMAIN_CSI_RX},
	{MT6985_POWER_DOMAIN_MM_INFRA, MT6985_POWER_DOMAIN_VDE_VCORE0},
	{MT6985_POWER_DOMAIN_VDE0, MT6985_POWER_DOMAIN_VDE_VCORE1},
};

static const struct scp_soc_data mt6985_spm_data = {
	.domains = scp_domain_mt6985_spm_data,
	.num_domains = MT6985_SPM_POWER_DOMAIN_NR,
	.subdomains = scp_subdomain_mt6985_spm,
	.num_subdomains = ARRAY_SIZE(scp_subdomain_mt6985_spm),
	.regs = {
		.pwr_sta_offs = 0xF84,
		.pwr_sta2nd_offs = 0xF88,
	}
};

/*
 * scpsys driver init
 */

static const struct of_device_id of_scpsys_match_tbl[] = {
	{
		.compatible = "mediatek,mt6985-gpusys",
		.data = &mt6985_gpu_eb_rpc_data,
	}, {
		.compatible = "mediatek,mt6985-scpsys",
		.data = &mt6985_spm_data,
	}, {
		/* sentinel */
	}
};

static int mt6985_scpsys_probe(struct platform_device *pdev)
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

static struct platform_driver mt6985_scpsys_drv = {
	.probe = mt6985_scpsys_probe,
	.driver = {
		.name = "mtk-scpsys-mt6985",
		.suppress_bind_attrs = true,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_scpsys_match_tbl),
	},
};

module_platform_driver(mt6985_scpsys_drv);
MODULE_LICENSE("GPL");
