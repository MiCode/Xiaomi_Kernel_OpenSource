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

/* Define MTCMOS Bus Protect Mask */
#define ADSP_INFRA_PROT_STEP1_0_MASK     ((0x1 << 18) | (0x1 << 19) | \
					(0x1 << 22) | (0x1 << 26) | (0x1 << 27) | (0x1 << 28) | \
					(0x1 << 29) | (0x1 << 30))
#define ADSP_INFRA_PROT_STEP1_0_ACK_MASK   ((0x1 << 18) | (0x1 << 19) | \
					(0x1 << 22) | (0x1 << 26) | (0x1 << 27) | (0x1 << 28) | \
					(0x1 << 29) | (0x1 << 30))
#define ADSP_INFRA_PROT_STEP1_1_MASK     ((0x1 << 8))
#define ADSP_INFRA_PROT_STEP1_1_ACK_MASK   ((0x1 << 8))
#define ADSP_INFRA_PROT_STEP2_0_MASK     ((0x1 << 20) | (0x1 << 21) | (0x1 << 25) | (0x1 << 31))
#define ADSP_INFRA_PROT_STEP2_0_ACK_MASK   ((0x1 << 20) | (0x1 << 21) | (0x1 << 25) | (0x1 << 31))
#define ADSP_INFRA_PROT_STEP2_1_MASK     ((0x1 << 4) | (0x1 << 5))
#define ADSP_INFRA_PROT_STEP2_1_ACK_MASK   ((0x1 << 4) | (0x1 << 5))
#define ADSP_TOP_PROT_STEP1_0_MASK       ((0x1 << 14) | (0x1 << 15) | (0x1 << 16) | (0x1 << 17))
#define ADSP_TOP_PROT_STEP1_0_ACK_MASK   ((0x1 << 14) | (0x1 << 15) | (0x1 << 16) | (0x1 << 17))
#define ADSP_TOP_PROT_STEP2_0_MASK       ((0x1 << 23) | (0x1 << 24))
#define ADSP_TOP_PROT_STEP2_0_ACK_MASK   ((0x1 << 23) | (0x1 << 24))
#define CAM_MAIN_PROT_STEP1_0_MASK       ((0x1 << 16) | (0x1 << 26))
#define CAM_MAIN_PROT_STEP1_0_ACK_MASK   ((0x1 << 16) | (0x1 << 26))
#define CAM_MAIN_PROT_STEP1_1_MASK       ((0x1 << 18))
#define CAM_MAIN_PROT_STEP1_1_ACK_MASK   ((0x1 << 18))
#define CAM_MAIN_PROT_STEP1_2_MASK       ((0x1 << 10))
#define CAM_MAIN_PROT_STEP1_2_ACK_MASK   ((0x1 << 10))
#define CAM_MAIN_PROT_STEP2_0_MASK       ((0x1 << 5) | (0x1 << 7) | (0x1 << 9))
#define CAM_MAIN_PROT_STEP2_0_ACK_MASK   ((0x1 << 5) | (0x1 << 7) | (0x1 << 9))
#define CAM_MAIN_PROT_STEP2_1_MASK       ((0x1 << 11))
#define CAM_MAIN_PROT_STEP2_1_ACK_MASK   ((0x1 << 11))
#define CAM_VCORE_PROT_STEP1_0_MASK      ((0x1 << 4) | (0x1 << 6) | (0x1 << 8))
#define CAM_VCORE_PROT_STEP1_0_ACK_MASK   ((0x1 << 4) | (0x1 << 6) | (0x1 << 8))
#define CAM_VCORE_PROT_STEP1_1_MASK      ((0x1 << 12))
#define CAM_VCORE_PROT_STEP1_1_ACK_MASK   ((0x1 << 12))
#define CAM_VCORE_PROT_STEP2_0_MASK      ((0x1 << 17) | (0x1 << 27))
#define CAM_VCORE_PROT_STEP2_0_ACK_MASK   ((0x1 << 17) | (0x1 << 27))
#define CAM_VCORE_PROT_STEP2_1_MASK      ((0x1 << 19))
#define CAM_VCORE_PROT_STEP2_1_ACK_MASK   ((0x1 << 19))
#define CAM_VCORE_PROT_STEP2_2_MASK      ((0x1 << 13))
#define CAM_VCORE_PROT_STEP2_2_ACK_MASK   ((0x1 << 13))
#define CONN_PROT_STEP1_0_MASK           ((0x1 << 1))
#define CONN_PROT_STEP1_0_ACK_MASK       ((0x1 << 1))
#define CONN_PROT_STEP1_1_MASK           ((0x1 << 12))
#define CONN_PROT_STEP1_1_ACK_MASK       ((0x1 << 12))
#define CONN_PROT_STEP2_0_MASK           ((0x1 << 0))
#define CONN_PROT_STEP2_0_ACK_MASK       ((0x1 << 0))
#define CONN_PROT_STEP2_1_MASK           ((0x1 << 8))
#define CONN_PROT_STEP2_1_ACK_MASK       ((0x1 << 8))
#define DIS0_PROT_STEP1_0_MASK           ((0x1 << 8) | (0x1 << 30))
#define DIS0_PROT_STEP1_0_ACK_MASK       ((0x1 << 8) | (0x1 << 30))
#define DIS0_PROT_STEP1_1_MASK           ((0x1 << 10))
#define DIS0_PROT_STEP1_1_ACK_MASK       ((0x1 << 10))
#define DIS0_PROT_STEP2_0_MASK           ((0x1 << 9) | (0x1 << 31))
#define DIS0_PROT_STEP2_0_ACK_MASK       ((0x1 << 9) | (0x1 << 31))
#define DIS0_PROT_STEP2_1_MASK           ((0x1 << 11))
#define DIS0_PROT_STEP2_1_ACK_MASK       ((0x1 << 11))
#define DIS1_PROT_STEP1_0_MASK           ((0x1 << 0) | (0x1 << 2) | (0x1 << 12))
#define DIS1_PROT_STEP1_0_ACK_MASK       ((0x1 << 0) | (0x1 << 2) | (0x1 << 12))
#define DIS1_PROT_STEP2_0_MASK           ((0x1 << 1) | (0x1 << 3) | (0x1 << 13))
#define DIS1_PROT_STEP2_0_ACK_MASK       ((0x1 << 1) | (0x1 << 3) | (0x1 << 13))

#define ISP_MAIN_PROT_STEP1_0_MASK       ((0x1 << 0) | (0x1 << 2) | (0x1 << 10))
#define ISP_MAIN_PROT_STEP1_0_ACK_MASK   ((0x1 << 0) | (0x1 << 2) | (0x1 << 10))
#define ISP_MAIN_PROT_STEP2_0_MASK       ((0x1 << 1) | (0x1 << 3) | (0x1 << 11))
#define ISP_MAIN_PROT_STEP2_0_ACK_MASK   ((0x1 << 1) | (0x1 << 3) | (0x1 << 11))

#define ISP_VCORE_PROT_STEP1_0_MASK      ((0x1 << 14) | (0x1 << 24))
#define ISP_VCORE_PROT_STEP1_0_ACK_MASK   ((0x1 << 14) | (0x1 << 24))
#define ISP_VCORE_PROT_STEP1_1_MASK      ((0x1 << 28))
#define ISP_VCORE_PROT_STEP1_1_ACK_MASK   ((0x1 << 28))

#define ISP_VCORE_PROT_STEP2_0_MASK      ((0x1 << 15) | (0x1 << 25))
#define ISP_VCORE_PROT_STEP2_0_ACK_MASK   ((0x1 << 15) | (0x1 << 25))
#define ISP_VCORE_PROT_STEP2_1_MASK      ((0x1 << 29))
#define ISP_VCORE_PROT_STEP2_1_ACK_MASK   ((0x1 << 29))

#define MDP0_PROT_STEP1_0_MASK           ((0x1 << 18))
#define MDP0_PROT_STEP1_0_ACK_MASK       ((0x1 << 18))
#define MDP0_PROT_STEP1_1_MASK           ((0x1 << 20))
#define MDP0_PROT_STEP1_1_ACK_MASK       ((0x1 << 20))
#define MDP0_PROT_STEP2_0_MASK           ((0x1 << 19))
#define MDP0_PROT_STEP2_0_ACK_MASK       ((0x1 << 19))
#define MDP0_PROT_STEP2_1_MASK           ((0x1 << 21))
#define MDP0_PROT_STEP2_1_ACK_MASK       ((0x1 << 21))
#define MDP1_PROT_STEP1_0_MASK           ((0x1 << 28))
#define MDP1_PROT_STEP1_0_ACK_MASK       ((0x1 << 28))
#define MDP1_PROT_STEP1_1_MASK           ((0x1 << 22))
#define MDP1_PROT_STEP1_1_ACK_MASK       ((0x1 << 22))
#define MDP1_PROT_STEP2_0_MASK           ((0x1 << 29))
#define MDP1_PROT_STEP2_0_ACK_MASK       ((0x1 << 29))
#define MDP1_PROT_STEP2_1_MASK           ((0x1 << 23))
#define MDP1_PROT_STEP2_1_ACK_MASK       ((0x1 << 23))
#define MFG0_PROT_STEP1_0_MASK           ((0x1 << 4))
#define MFG0_PROT_STEP1_0_ACK_MASK       ((0x1 << 4))
#define MFG0_PROT_STEP2_0_MASK           ((0x1 << 9))
#define MFG0_PROT_STEP2_0_ACK_MASK       ((0x1 << 9))
#define MFG1_PROT_STEP1_0_MASK           ((0x1 << 0))
#define MFG1_PROT_STEP1_0_ACK_MASK       ((0x1 << 0))
#define MFG1_PROT_STEP2_0_MASK           ((0x1 << 1))
#define MFG1_PROT_STEP2_0_ACK_MASK       ((0x1 << 1))
#define MFG1_PROT_STEP3_0_MASK           ((0x1 << 2))
#define MFG1_PROT_STEP3_0_ACK_MASK       ((0x1 << 2))
#define MFG1_PROT_STEP4_0_MASK           ((0x1 << 3))
#define MFG1_PROT_STEP4_0_ACK_MASK       ((0x1 << 3))

#define MFG1_MFGRPC_PROT_STEP1_0_MASK       ((0x1 << 16) | (0x1 << 17) | (0x1 << 18) | (0x1 << 19))
#define MFG1_MFGRPC_PROT_STEP1_0_ACK_MASK   ((0x1 << 16) | (0x1 << 17) | (0x1 << 18) | (0x1 << 19))
#define MFG1_EMI_PROT_STEP2_0_MASK       ((0x1 << 18) | (0x1 << 19))
#define MFG1_EMI_PROT_STEP2_0_ACK_MASK   ((0x1 << 18) | (0x1 << 19))
#define MFG1_EMI_PROT_STEP2_1_MASK       ((0x1 << 18) | (0x1 << 19))
#define MFG1_EMI_PROT_STEP2_1_ACK_MASK   ((0x1 << 18) | (0x1 << 19))

#define MD1_EMI_PROT_STEP1_0_MASK       ((0x1 << 16) | (0x1 << 17))
#define MD1_EMI_PROT_STEP1_0_ACK_MASK   ((0x1 << 16) | (0x1 << 17))
#define MD1_EMI_PROT_STEP1_1_MASK       ((0x1 << 16) | (0x1 << 17))
#define MD1_EMI_PROT_STEP1_1_ACK_MASK   ((0x1 << 16) | (0x1 << 17))

#define MM_INFRA_PROT_STEP1_0_MASK       ((0x1 << 0) | (0x1 << 2) | (0x1 << 4) | (0x1 << 6))
#define MM_INFRA_PROT_STEP1_0_ACK_MASK   ((0x1 << 0) | (0x1 << 2) | (0x1 << 4) | (0x1 << 6))
#define MM_INFRA_PROT_STEP1_1_MASK       ((0x1 << 4) | (0x1 << 6))
#define MM_INFRA_PROT_STEP1_1_ACK_MASK   ((0x1 << 4) | (0x1 << 6))
#define MM_INFRA_PROT_STEP1_2_MASK       ((0x1 << 12))
#define MM_INFRA_PROT_STEP1_2_ACK_MASK   ((0x1 << 12))
#define MM_INFRA_PROT_STEP1_3_MASK       ((0x1 << 11))
#define MM_INFRA_PROT_STEP1_3_ACK_MASK   ((0x1 << 11))
#define MM_INFRA_PROT_STEP2_0_MASK       ((0x1 << 9) | (0x1 << 11) | \
					(0x1 << 13) | (0x1 << 15) | (0x1 << 17) | (0x1 << 19) | \
					(0x1 << 21) | (0x1 << 23) | (0x1 << 25) | (0x1 << 27) | \
					(0x1 << 29) | (0x1 << 31))
#define MM_INFRA_PROT_STEP2_0_ACK_MASK   ((0x1 << 9) | (0x1 << 11) | \
					(0x1 << 13) | (0x1 << 15) | (0x1 << 17) | (0x1 << 19) | \
					(0x1 << 21) | (0x1 << 23) | (0x1 << 25) | (0x1 << 27) | \
					(0x1 << 29) | (0x1 << 31))
#define MM_INFRA_PROT_STEP2_1_MASK       ((0x1 << 1) | (0x1 << 3) | \
					(0x1 << 9) | (0x1 << 11) | (0x1 << 13) | (0x1 << 15) | \
					(0x1 << 17) | (0x1 << 19) | (0x1 << 21) | (0x1 << 23) | \
					(0x1 << 25) | (0x1 << 27) | (0x1 << 29) | (0x1 << 31))
#define MM_INFRA_PROT_STEP2_1_ACK_MASK   ((0x1 << 1) | (0x1 << 3) | \
					(0x1 << 9) | (0x1 << 11) | (0x1 << 13) | (0x1 << 15) | \
					(0x1 << 17) | (0x1 << 19) | (0x1 << 21) | (0x1 << 23) | \
					(0x1 << 25) | (0x1 << 27) | (0x1 << 29) | (0x1 << 31))
#define MM_INFRA_PROT_STEP2_2_MASK       ((0x1 << 15) | (0x1 << 17))
#define MM_INFRA_PROT_STEP2_2_ACK_MASK   ((0x1 << 15) | (0x1 << 17))
#define MM_INFRA_PROT_STEP2_3_MASK       ((0x1 << 14) | (0x1 << 15))
#define MM_INFRA_PROT_STEP2_3_ACK_MASK   ((0x1 << 14) | (0x1 << 15))
#define MM_INFRA_PROT_STEP3_0_MASK       ((0x1 << 20) | (0x1 << 21))
#define MM_INFRA_PROT_STEP3_0_ACK_MASK   ((0x1 << 20) | (0x1 << 21))
#define MM_INFRA_PROT_STEP3_1_MASK       ((0x1 << 20) | (0x1 << 21))
#define MM_INFRA_PROT_STEP3_1_ACK_MASK   ((0x1 << 20) | (0x1 << 21))

#define MM_PROC_PROT_STEP1_0_MASK        ((0x1 << 10) | (0x1 << 11))
#define MM_PROC_PROT_STEP1_0_ACK_MASK    ((0x1 << 10) | (0x1 << 11))
#define MM_PROC_PROT_STEP2_0_MASK        ((0x1 << 12) | (0x1 << 13))
#define MM_PROC_PROT_STEP2_0_ACK_MASK    ((0x1 << 12) | (0x1 << 13))
#define UFS0_PROT_STEP1_0_MASK           ((0x1 << 7))
#define UFS0_PROT_STEP1_0_ACK_MASK       ((0x1 << 7))
#define UFS0_PROT_STEP2_0_MASK           ((0x1 << 4))
#define UFS0_PROT_STEP2_0_ACK_MASK       ((0x1 << 4))
#define UFS0_PROT_STEP3_0_MASK           ((0x1 << 8))
#define UFS0_PROT_STEP3_0_ACK_MASK       ((0x1 << 8))
#define VDE0_PROT_STEP1_0_MASK           ((0x1 << 10))
#define VDE0_PROT_STEP1_0_ACK_MASK       ((0x1 << 10))
#define VDE0_PROT_STEP1_1_MASK           ((0x1 << 26) | (0x1 << 30))
#define VDE0_PROT_STEP1_1_ACK_MASK       ((0x1 << 26) | (0x1 << 30))
#define VDE0_PROT_STEP2_0_MASK           ((0x1 << 11))
#define VDE0_PROT_STEP2_0_ACK_MASK       ((0x1 << 11))
#define VDE0_PROT_STEP2_1_MASK           ((0x1 << 27) | (0x1 << 31))
#define VDE0_PROT_STEP2_1_ACK_MASK       ((0x1 << 27) | (0x1 << 31))
#define VDE1_PROT_STEP1_0_MASK           ((0x1 << 20))
#define VDE1_PROT_STEP1_0_ACK_MASK       ((0x1 << 20))
#define VDE1_PROT_STEP1_1_MASK           ((0x1 << 24))
#define VDE1_PROT_STEP1_1_ACK_MASK       ((0x1 << 24))
#define VDE1_PROT_STEP2_0_MASK           ((0x1 << 21))
#define VDE1_PROT_STEP2_0_ACK_MASK       ((0x1 << 21))
#define VDE1_PROT_STEP2_1_MASK           ((0x1 << 25))
#define VDE1_PROT_STEP2_1_ACK_MASK       ((0x1 << 25))
#define VEN0_PROT_STEP1_0_MASK           ((0x1 << 12))
#define VEN0_PROT_STEP1_0_ACK_MASK       ((0x1 << 12))
#define VEN0_PROT_STEP1_1_MASK           ((0x1 << 14))
#define VEN0_PROT_STEP1_1_ACK_MASK       ((0x1 << 14))
#define VEN0_PROT_STEP2_0_MASK           ((0x1 << 13))
#define VEN0_PROT_STEP2_0_ACK_MASK       ((0x1 << 13))
#define VEN0_PROT_STEP2_1_MASK           ((0x1 << 15))
#define VEN0_PROT_STEP2_1_ACK_MASK       ((0x1 << 15))
#define VEN1_PROT_STEP1_0_MASK           ((0x1 << 22))
#define VEN1_PROT_STEP1_0_ACK_MASK       ((0x1 << 22))
#define VEN1_PROT_STEP1_1_MASK           ((0x1 << 16))
#define VEN1_PROT_STEP1_1_ACK_MASK       ((0x1 << 16))
#define VEN1_PROT_STEP2_0_MASK           ((0x1 << 23))
#define VEN1_PROT_STEP2_0_ACK_MASK       ((0x1 << 23))
#define VEN1_PROT_STEP2_1_MASK           ((0x1 << 17))
#define VEN1_PROT_STEP2_1_ACK_MASK       ((0x1 << 17))


/*
 * MT6983 power domain support
 */

static const struct scp_domain_data scp_domain_data_mt6983[] = {
	[MT6983_POWER_DOMAIN_MD1] = {
		.name = "md1",
		.sta_mask = BIT(0),
		.ctl_offs = 0xE00,
		.extb_iso_offs = 0xF2C,
		.extb_iso_bits = 0x3,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C64, 0x0C68, 0x0C60, 0x0C6C,
				MD1_EMI_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C74, 0x0C78, 0x0C70, 0x0C7C,
				MD1_EMI_PROT_STEP1_1_MASK),
		},
		.caps = MTK_SCPD_MD_OPS | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_CONN] = {
		.name = "conn",
		.sta_mask = BIT(1),
		.ctl_offs = 0xE04,
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C94, 0x0C98, 0x0C90, 0x0C9C,
				CONN_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C54, 0x0C58, 0x0C50, 0x0C5C,
				CONN_PROT_STEP1_1_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C94, 0x0C98, 0x0C90, 0x0C9C,
				CONN_PROT_STEP2_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				CONN_PROT_STEP2_1_MASK),
		},
		.caps = MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_UFS0] = {
		.name = "ufs0",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE10,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				UFS0_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C84, 0x0C88, 0x0C80, 0x0C8C,
				UFS0_PROT_STEP2_0_MASK),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				UFS0_PROT_STEP3_0_MASK),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_MM_INFRA] = {
		.name = "mm_infra",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE6C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mm_infra_0"},
		.subsys_clk_prefix = "mminfra",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MM_INFRA_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MM_INFRA_PROT_STEP1_1_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C34, 0x0C38, 0x0C30, 0x0C3C,
				MM_INFRA_PROT_STEP1_2_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C54, 0x0C58, 0x0C50, 0x0C5C,
				MM_INFRA_PROT_STEP1_3_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MM_INFRA_PROT_STEP2_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MM_INFRA_PROT_STEP2_1_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C34, 0x0C38, 0x0C30, 0x0C3C,
				MM_INFRA_PROT_STEP2_2_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				MM_INFRA_PROT_STEP2_3_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C64, 0x0C68, 0x0C60, 0x0C6C,
				MM_INFRA_PROT_STEP3_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C74, 0x0C78, 0x0C70, 0x0C7C,
				MM_INFRA_PROT_STEP3_1_MASK),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_MFG0_DORMANT] = {
		.name = "mfg0_dormant",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEB8,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0CA4, 0x0CA8, 0x0CA0, 0x0CAC,
				MFG0_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				MFG0_PROT_STEP2_0_MASK),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON |
							MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_ADSP_INFRA] = {
		.name = "adsp_infra",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE1C,
		.basic_clk_name = {"adsp_ao_0"},
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				ADSP_INFRA_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C54, 0x0C58, 0x0C50, 0x0C5C,
				ADSP_INFRA_PROT_STEP1_1_MASK),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				ADSP_INFRA_PROT_STEP2_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				ADSP_INFRA_PROT_STEP2_1_MASK),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,

	},
	[MT6983_POWER_DOMAIN_MM_PROC_DORMANT] = {
		.name = "mm_proc_dormant",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE70,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.basic_clk_name = {"mm_proc_dormant_0"},
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MM_PROC_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				MM_PROC_PROT_STEP2_0_MASK),
		},
		.sram_table = {
			SRAM_NO_ACK(0xF08, 8), SRAM_NO_ACK(0xF08, 9),
			SRAM_NO_ACK(0xF08, 10), SRAM_NO_ACK(0xF08, 11),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_L2TCM_SRAM | MTK_SCPD_SRAM_SLP |
							MTK_SCPD_BYPASS_INIT_ON |
							MTK_SCPD_IS_PWR_CON_ON,
	},
	[MT6983_POWER_DOMAIN_ISP_VCORE] = {
		.name = "isp_vcore",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE30,
		.basic_clk_name = {"isp_vcore_0"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				ISP_VCORE_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				ISP_VCORE_PROT_STEP1_1_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				ISP_VCORE_PROT_STEP2_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				ISP_VCORE_PROT_STEP2_1_MASK),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_DIS0] = {
		.name = "disp0",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE64,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"dis0_0"},
		.subsys_clk_prefix = "dis0",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				DIS0_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				DIS0_PROT_STEP1_1_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				DIS0_PROT_STEP2_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				DIS0_PROT_STEP2_1_MASK),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,

	},
	[MT6983_POWER_DOMAIN_DIS1] = {
		.name = "disp1",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE68,
		.basic_clk_name = {"dis1_0"},
		.subsys_clk_prefix = "dis1",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				DIS1_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				DIS1_PROT_STEP2_0_MASK),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_CAM_VCORE] = {
		.name = "cam_vcore",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE58,
		.basic_clk_name = {"cam_vcore_0", "cam_vcore_1", "cam_vcore_2"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C34, 0x0C38, 0x0C30, 0x0C3C,
				CAM_VCORE_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0CC4, 0x0CC8, 0x0CC0, 0x0CCC,
				CAM_VCORE_PROT_STEP1_1_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				CAM_VCORE_PROT_STEP2_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				CAM_VCORE_PROT_STEP2_1_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C44, 0x0C48, 0x0C40, 0x0C4C,
				CAM_VCORE_PROT_STEP2_2_MASK),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_MFG1] = {
		.name = "mfg1",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEBC,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0CA4, 0x0CA8, 0x0CA0, 0x0CAC,
				MFG1_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0CA4, 0x0CA8, 0x0CA0, 0x0CAC,
				MFG1_PROT_STEP2_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0CA4, 0x0CA8, 0x0CA0, 0x0CAC,
				MFG1_PROT_STEP3_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0CA4, 0x0CA8, 0x0CA0, 0x0CAC,
				MFG1_PROT_STEP4_0_MASK),
			BUS_PROT_IGN(MFGRPC_TYPE, 0x1040, 0x1044, 0x103C, 0x1048,
				MFG1_MFGRPC_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C64, 0x0C68, 0x0C60, 0x0C6C,
				MFG1_EMI_PROT_STEP2_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C74, 0x0C78, 0x0C70, 0x0C7C,
				MFG1_EMI_PROT_STEP2_1_MASK),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_ADSP_TOP_DORMANT] = {
		.name = "adsp_top_dormant",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE18,
		.sram_slp_bits = GENMASK(9, 9),
		.sram_slp_ack_bits = GENMASK(13, 13),
		.bp_table = {
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				ADSP_TOP_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(VLP_TYPE, 0x0214, 0x0218, 0x0210, 0x0220,
				ADSP_TOP_PROT_STEP2_0_MASK),
		},
		.caps = MTK_SCPD_SRAM_ISO | MTK_SCPD_SRAM_SLP | MTK_SCPD_IS_PWR_CON_ON |
							MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_AUDIO] = {
		.name = "audio",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE14,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"audio_0", "audio_1"},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_ISP_MAIN] = {
		.name = "isp_main",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE24,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "isp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C34, 0x0C38, 0x0C30, 0x0C3C,
				ISP_MAIN_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C34, 0x0C38, 0x0C30, 0x0C3C,
				ISP_MAIN_PROT_STEP2_0_MASK),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_VDE0] = {
		.name = "vdec0",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE34,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"vde0_0"},
		.subsys_clk_prefix = "vde0",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				VDE0_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				VDE0_PROT_STEP1_1_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				VDE0_PROT_STEP2_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				VDE0_PROT_STEP2_1_MASK),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_VEN0] = {
		.name = "venc0",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE3C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ven0_0"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				VEN0_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				VEN0_PROT_STEP1_1_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				VEN0_PROT_STEP2_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				VEN0_PROT_STEP2_1_MASK),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_MDP0] = {
		.name = "mdp0",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE5C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mdp0_0"},
		.subsys_clk_prefix = "mdp0",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MDP0_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MDP0_PROT_STEP1_1_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MDP0_PROT_STEP2_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MDP0_PROT_STEP2_1_MASK),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_VDE1] = {
		.name = "vdec1",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE38,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"vde1_0"},
		.subsys_clk_prefix = "vde1",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				VDE1_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				VDE1_PROT_STEP1_1_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				VDE1_PROT_STEP2_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				VDE1_PROT_STEP2_1_MASK),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_VEN1] = {
		.name = "venc1",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE40,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"ven1_0"},
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				VEN1_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				VEN1_PROT_STEP1_1_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				VEN1_PROT_STEP2_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				VEN1_PROT_STEP2_1_MASK),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_MDP1] = {
		.name = "mdp1",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE60,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"mdp1_0"},
		.subsys_clk_prefix = "mdp1",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MDP1_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MDP1_PROT_STEP1_1_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				MDP1_PROT_STEP2_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				MDP1_PROT_STEP2_1_MASK),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_CAM_MAIN] = {
		.name = "cam_main",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE44,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "cam",
		.subsys_lp_clk_prefix = "cam_lp",
		.bp_table = {
			BUS_PROT_IGN(IFR_TYPE, 0x0C14, 0x0C18, 0x0C10, 0x0C1C,
				CAM_MAIN_PROT_STEP1_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C24, 0x0C28, 0x0C20, 0x0C2C,
				CAM_MAIN_PROT_STEP1_1_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0CC4, 0x0CC8, 0x0CC0, 0x0CCC,
				CAM_MAIN_PROT_STEP1_2_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0C34, 0x0C38, 0x0C30, 0x0C3C,
				CAM_MAIN_PROT_STEP2_0_MASK),
			BUS_PROT_IGN(IFR_TYPE, 0x0CC4, 0x0CC8, 0x0CC0, 0x0CCC,
				CAM_MAIN_PROT_STEP2_1_MASK),
		},
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_MFG2] = {
		.name = "mfg2",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEC0,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_MFG3] = {
		.name = "mfg3",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEC4,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_MFG4] = {
		.name = "mfg4",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEC8,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON,
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_MFG5] = {
		.name = "mfg5",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xECC,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_MFG6] = {
		.name = "mfg6",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xED0,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_MFG7] = {
		.name = "mfg7",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xED4,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_MFG8] = {
		.name = "mfg8",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xED8,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_MFG9] = {
		.name = "mfg9",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEDC,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_MFG10] = {
		.name = "mfg10",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEE0,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_MFG11] = {
		.name = "mfg11",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEE4,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_MFG12] = {
		.name = "mfg12",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEE8,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_MFG13] = {
		.name = "mfg13",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEEC,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_MFG14] = {
		.name = "mfg14",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEF0,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_MFG15] = {
		.name = "mfg15",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEF4,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_MFG16] = {
		.name = "mfg16",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEF8,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_MFG17] = {
		.name = "mfg17",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xEFC,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_MFG18] = {
		.name = "mfg18",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xF00,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_ISP_DIP1] = {
		.name = "isp_dip1",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE28,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "dip1",
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_ISP_IPE] = {
		.name = "isp_ipe",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE2C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.basic_clk_name = {"isp_ipe_0"},
		.subsys_clk_prefix = "ipe",
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_CAM_MRAW] = {
		.name = "cam_mraw",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE48,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "mraw",
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_CAM_SUBA] = {
		.name = "cam_suba",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE4C,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "suba",
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_CAM_SUBB] = {
		.name = "cam_subb",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE50,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "subb",
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_CAM_SUBC] = {
		.name = "cam_subc",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE54,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.subsys_clk_prefix = "subc",
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_APU] = {
		.name = "apu",
		.caps = MTK_SCPD_APU_OPS | MTK_SCPD_BYPASS_INIT_ON,
	},
	[MT6983_POWER_DOMAIN_DP_TX] = {
		.name = "dp_tx",
		.sta_mask = GENMASK(31, 30),
		.ctl_offs = 0xE74,
		.sram_pdn_bits = GENMASK(8, 8),
		.sram_pdn_ack_bits = GENMASK(12, 12),
		.caps = MTK_SCPD_IS_PWR_CON_ON | MTK_SCPD_BYPASS_INIT_ON,
	},
};

static const struct scp_subdomain scp_subdomain_mt6983[] = {
	//{MT6983_POWER_DOMAIN_ADSP_AO, MT6983_POWER_DOMAIN_ADSP_INFRA},
	{MT6983_POWER_DOMAIN_MM_INFRA, MT6983_POWER_DOMAIN_MM_PROC_DORMANT},
	{MT6983_POWER_DOMAIN_MM_INFRA, MT6983_POWER_DOMAIN_ISP_VCORE},
	{MT6983_POWER_DOMAIN_MM_INFRA, MT6983_POWER_DOMAIN_DIS0},
	{MT6983_POWER_DOMAIN_MM_INFRA, MT6983_POWER_DOMAIN_DIS1},
	{MT6983_POWER_DOMAIN_MM_INFRA, MT6983_POWER_DOMAIN_CAM_VCORE},
	{MT6983_POWER_DOMAIN_ADSP_INFRA, MT6983_POWER_DOMAIN_ADSP_TOP_DORMANT},
	{MT6983_POWER_DOMAIN_ADSP_INFRA, MT6983_POWER_DOMAIN_AUDIO},
	{MT6983_POWER_DOMAIN_ISP_VCORE, MT6983_POWER_DOMAIN_ISP_MAIN},
	{MT6983_POWER_DOMAIN_DIS0, MT6983_POWER_DOMAIN_VDE0},
	{MT6983_POWER_DOMAIN_DIS0, MT6983_POWER_DOMAIN_VEN0},
	{MT6983_POWER_DOMAIN_DIS0, MT6983_POWER_DOMAIN_MDP0},
	{MT6983_POWER_DOMAIN_DIS0, MT6983_POWER_DOMAIN_DP_TX},
	{MT6983_POWER_DOMAIN_VDE0, MT6983_POWER_DOMAIN_VDE1},
	{MT6983_POWER_DOMAIN_DIS1, MT6983_POWER_DOMAIN_VEN1},
	{MT6983_POWER_DOMAIN_DIS1, MT6983_POWER_DOMAIN_MDP1},
	{MT6983_POWER_DOMAIN_CAM_VCORE, MT6983_POWER_DOMAIN_CAM_MAIN},
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
