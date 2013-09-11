/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <linux/msm_ion.h>
#include <mach/msm_iomap.h>
#include <mach/irqs-8930.h>
#include <mach/rpm.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include <mach/board.h>
#include <mach/socinfo.h>
#include <mach/iommu_domains.h>
#include <mach/msm_rtb.h>
#include <mach/msm_cache_dump.h>

#include "devices.h"
#include "rpm_log.h"
#include "rpm_stats.h"
#include "rpm_rbcpr_stats.h"
#include "footswitch.h"
#include "pm.h"

#ifdef CONFIG_MSM_MPM
#include <mach/mpm.h>
#endif
#define MSM8930_PC_CNTR_PHYS	(MSM8930_IMEM_PHYS + 0x664)
#define MSM8930_PC_CNTR_SIZE		0x40
#define MSM8930_RPM_MASTER_STATS_BASE	0x10B100

static struct resource msm8930_resources_pccntr[] = {
	{
		.start	= MSM8930_PC_CNTR_PHYS,
		.end	= MSM8930_PC_CNTR_PHYS + MSM8930_PC_CNTR_SIZE,
		.flags	= IORESOURCE_MEM,
	},
};

static struct msm_pm_init_data_type msm_pm_data = {
	.retention_calls_tz = true,
};

static struct msm_pm_sleep_status_data msm_pm_slp_sts_data = {
	.base_addr = MSM_ACC0_BASE + 0x08,
	.cpu_offset = MSM_ACC1_BASE - MSM_ACC0_BASE,
	.mask = 1UL << 13,
};

struct platform_device msm8930_cpu_slp_status = {
	.name		= "cpu_slp_status",
	.id		= -1,
	.dev = {
		.platform_data = &msm_pm_slp_sts_data,
	},
};

struct platform_device msm8930_pm_8x60 = {
	.name		= "pm-8x60",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(msm8930_resources_pccntr),
	.resource	= msm8930_resources_pccntr,
	.dev = {
		.platform_data = &msm_pm_data,
	},
};

struct msm_rpm_platform_data msm8930_rpm_data __initdata = {
	.reg_base_addrs = {
		[MSM_RPM_PAGE_STATUS] = MSM_RPM_BASE,
		[MSM_RPM_PAGE_CTRL] = MSM_RPM_BASE + 0x400,
		[MSM_RPM_PAGE_REQ] = MSM_RPM_BASE + 0x600,
		[MSM_RPM_PAGE_ACK] = MSM_RPM_BASE + 0xa00,
	},
	.irq_ack = RPM_APCC_CPU0_GP_HIGH_IRQ,
	.irq_err = RPM_APCC_CPU0_GP_LOW_IRQ,
	.irq_wakeup = RPM_APCC_CPU0_WAKE_UP_IRQ,
	.ipc_rpm_reg = MSM_APCS_GCC_BASE + 0x008,
	.ipc_rpm_val = 4,
	.target_id = {
		MSM_RPM_MAP(8930, NOTIFICATION_CONFIGURED_0, NOTIFICATION, 4),
		MSM_RPM_MAP(8930, NOTIFICATION_REGISTERED_0, NOTIFICATION, 4),
		MSM_RPM_MAP(8930, INVALIDATE_0, INVALIDATE, 8),
		MSM_RPM_MAP(8960, TRIGGER_TIMED_TO, TRIGGER_TIMED, 1),
		MSM_RPM_MAP(8960, TRIGGER_TIMED_SCLK_COUNT, TRIGGER_TIMED, 1),
		MSM_RPM_MAP(8930, RPM_CTL, RPM_CTL, 1),
		MSM_RPM_MAP(8930, CXO_CLK, CXO_CLK, 1),
		MSM_RPM_MAP(8930, PXO_CLK, PXO_CLK, 1),
		MSM_RPM_MAP(8930, APPS_FABRIC_CLK, APPS_FABRIC_CLK, 1),
		MSM_RPM_MAP(8930, SYSTEM_FABRIC_CLK, SYSTEM_FABRIC_CLK, 1),
		MSM_RPM_MAP(8930, MM_FABRIC_CLK, MM_FABRIC_CLK, 1),
		MSM_RPM_MAP(8930, DAYTONA_FABRIC_CLK, DAYTONA_FABRIC_CLK, 1),
		MSM_RPM_MAP(8930, SFPB_CLK, SFPB_CLK, 1),
		MSM_RPM_MAP(8930, CFPB_CLK, CFPB_CLK, 1),
		MSM_RPM_MAP(8930, MMFPB_CLK, MMFPB_CLK, 1),
		MSM_RPM_MAP(8930, EBI1_CLK, EBI1_CLK, 1),
		MSM_RPM_MAP(8930, APPS_FABRIC_CFG_HALT_0,
				APPS_FABRIC_CFG_HALT, 2),
		MSM_RPM_MAP(8930, APPS_FABRIC_CFG_CLKMOD_0,
				APPS_FABRIC_CFG_CLKMOD, 3),
		MSM_RPM_MAP(8930, APPS_FABRIC_CFG_IOCTL,
				APPS_FABRIC_CFG_IOCTL, 1),
		MSM_RPM_MAP(8930, APPS_FABRIC_ARB_0, APPS_FABRIC_ARB, 6),
		MSM_RPM_MAP(8930, SYS_FABRIC_CFG_HALT_0,
				SYS_FABRIC_CFG_HALT, 2),
		MSM_RPM_MAP(8930, SYS_FABRIC_CFG_CLKMOD_0,
				SYS_FABRIC_CFG_CLKMOD, 3),
		MSM_RPM_MAP(8930, SYS_FABRIC_CFG_IOCTL,
				SYS_FABRIC_CFG_IOCTL, 1),
		MSM_RPM_MAP(8930, SYSTEM_FABRIC_ARB_0,
				SYSTEM_FABRIC_ARB, 20),
		MSM_RPM_MAP(8930, MMSS_FABRIC_CFG_HALT_0,
				MMSS_FABRIC_CFG_HALT, 2),
		MSM_RPM_MAP(8930, MMSS_FABRIC_CFG_CLKMOD_0,
				MMSS_FABRIC_CFG_CLKMOD, 3),
		MSM_RPM_MAP(8930, MMSS_FABRIC_CFG_IOCTL,
				MMSS_FABRIC_CFG_IOCTL, 1),
		MSM_RPM_MAP(8930, MM_FABRIC_ARB_0, MM_FABRIC_ARB, 11),
		MSM_RPM_MAP(8930, PM8038_S1_0, PM8038_S1, 2),
		MSM_RPM_MAP(8930, PM8038_S2_0, PM8038_S2, 2),
		MSM_RPM_MAP(8930, PM8038_S3_0, PM8038_S3, 2),
		MSM_RPM_MAP(8930, PM8038_S4_0, PM8038_S4, 2),
		MSM_RPM_MAP(8930, PM8038_S5_0, PM8038_S5, 2),
		MSM_RPM_MAP(8930, PM8038_S6_0, PM8038_S6, 2),
		MSM_RPM_MAP(8930, PM8038_L1_0, PM8038_L1, 2),
		MSM_RPM_MAP(8930, PM8038_L2_0, PM8038_L2, 2),
		MSM_RPM_MAP(8930, PM8038_L3_0, PM8038_L3, 2),
		MSM_RPM_MAP(8930, PM8038_L4_0, PM8038_L4, 2),
		MSM_RPM_MAP(8930, PM8038_L5_0, PM8038_L5, 2),
		MSM_RPM_MAP(8930, PM8038_L6_0, PM8038_L6, 2),
		MSM_RPM_MAP(8930, PM8038_L7_0, PM8038_L7, 2),
		MSM_RPM_MAP(8930, PM8038_L8_0, PM8038_L8, 2),
		MSM_RPM_MAP(8930, PM8038_L9_0, PM8038_L9, 2),
		MSM_RPM_MAP(8930, PM8038_L10_0, PM8038_L10, 2),
		MSM_RPM_MAP(8930, PM8038_L11_0, PM8038_L11, 2),
		MSM_RPM_MAP(8930, PM8038_L12_0, PM8038_L12, 2),
		MSM_RPM_MAP(8930, PM8038_L13_0, PM8038_L13, 2),
		MSM_RPM_MAP(8930, PM8038_L14_0, PM8038_L14, 2),
		MSM_RPM_MAP(8930, PM8038_L15_0, PM8038_L15, 2),
		MSM_RPM_MAP(8930, PM8038_L16_0, PM8038_L16, 2),
		MSM_RPM_MAP(8930, PM8038_L17_0, PM8038_L17, 2),
		MSM_RPM_MAP(8930, PM8038_L18_0, PM8038_L18, 2),
		MSM_RPM_MAP(8930, PM8038_L19_0, PM8038_L19, 2),
		MSM_RPM_MAP(8930, PM8038_L20_0, PM8038_L20, 2),
		MSM_RPM_MAP(8930, PM8038_L21_0, PM8038_L21, 2),
		MSM_RPM_MAP(8930, PM8038_L22_0, PM8038_L22, 2),
		MSM_RPM_MAP(8930, PM8038_L23_0, PM8038_L23, 2),
		MSM_RPM_MAP(8930, PM8038_L24_0, PM8038_L24, 2),
		MSM_RPM_MAP(8930, PM8038_L25_0, PM8038_L25, 2),
		MSM_RPM_MAP(8930, PM8038_L26_0, PM8038_L26, 2),
		MSM_RPM_MAP(8930, PM8038_L27_0, PM8038_L27, 2),
		MSM_RPM_MAP(8930, PM8038_CLK1_0, PM8038_CLK1, 2),
		MSM_RPM_MAP(8930, PM8038_CLK2_0, PM8038_CLK2, 2),
		MSM_RPM_MAP(8930, PM8038_LVS1, PM8038_LVS1, 1),
		MSM_RPM_MAP(8930, PM8038_LVS2, PM8038_LVS2, 1),
		MSM_RPM_MAP_PMIC(8930, 8038, NCP_0, NCP, 2),
		MSM_RPM_MAP_PMIC(8930, 8038, CXO_BUFFERS, CXO_BUFFERS, 1),
		MSM_RPM_MAP_PMIC(8930, 8038, USB_OTG_SWITCH, USB_OTG_SWITCH, 1),
		MSM_RPM_MAP_PMIC(8930, 8038, HDMI_SWITCH, HDMI_SWITCH, 1),
		MSM_RPM_MAP_PMIC(8930, 8038, QDSS_CLK, QDSS_CLK, 1),
		MSM_RPM_MAP_PMIC(8930, 8038, VOLTAGE_CORNER, VOLTAGE_CORNER, 1),
	},
	.target_status = {
		MSM_RPM_STATUS_ID_MAP(8930, VERSION_MAJOR),
		MSM_RPM_STATUS_ID_MAP(8930, VERSION_MINOR),
		MSM_RPM_STATUS_ID_MAP(8930, VERSION_BUILD),
		MSM_RPM_STATUS_ID_MAP(8930, SUPPORTED_RESOURCES_0),
		MSM_RPM_STATUS_ID_MAP(8930, SUPPORTED_RESOURCES_1),
		MSM_RPM_STATUS_ID_MAP(8930, SUPPORTED_RESOURCES_2),
		MSM_RPM_STATUS_ID_MAP(8930, RESERVED_SUPPORTED_RESOURCES_0),
		MSM_RPM_STATUS_ID_MAP(8930, SEQUENCE),
		MSM_RPM_STATUS_ID_MAP(8930, RPM_CTL),
		MSM_RPM_STATUS_ID_MAP(8930, CXO_CLK),
		MSM_RPM_STATUS_ID_MAP(8930, PXO_CLK),
		MSM_RPM_STATUS_ID_MAP(8930, APPS_FABRIC_CLK),
		MSM_RPM_STATUS_ID_MAP(8930, SYSTEM_FABRIC_CLK),
		MSM_RPM_STATUS_ID_MAP(8930, MM_FABRIC_CLK),
		MSM_RPM_STATUS_ID_MAP(8930, DAYTONA_FABRIC_CLK),
		MSM_RPM_STATUS_ID_MAP(8930, SFPB_CLK),
		MSM_RPM_STATUS_ID_MAP(8930, CFPB_CLK),
		MSM_RPM_STATUS_ID_MAP(8930, MMFPB_CLK),
		MSM_RPM_STATUS_ID_MAP(8930, EBI1_CLK),
		MSM_RPM_STATUS_ID_MAP(8930, APPS_FABRIC_CFG_HALT),
		MSM_RPM_STATUS_ID_MAP(8930, APPS_FABRIC_CFG_CLKMOD),
		MSM_RPM_STATUS_ID_MAP(8930, APPS_FABRIC_CFG_IOCTL),
		MSM_RPM_STATUS_ID_MAP(8930, APPS_FABRIC_ARB),
		MSM_RPM_STATUS_ID_MAP(8930, SYS_FABRIC_CFG_HALT),
		MSM_RPM_STATUS_ID_MAP(8930, SYS_FABRIC_CFG_CLKMOD),
		MSM_RPM_STATUS_ID_MAP(8930, SYS_FABRIC_CFG_IOCTL),
		MSM_RPM_STATUS_ID_MAP(8930, SYSTEM_FABRIC_ARB),
		MSM_RPM_STATUS_ID_MAP(8930, MMSS_FABRIC_CFG_HALT),
		MSM_RPM_STATUS_ID_MAP(8930, MMSS_FABRIC_CFG_CLKMOD),
		MSM_RPM_STATUS_ID_MAP(8930, MMSS_FABRIC_CFG_IOCTL),
		MSM_RPM_STATUS_ID_MAP(8930, MM_FABRIC_ARB),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_S1_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_S1_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_S2_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_S2_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_S3_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_S3_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_S4_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_S4_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L1_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L1_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L2_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L2_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L3_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L3_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L4_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L4_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L5_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L5_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L6_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L6_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L7_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L7_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L8_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L8_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L9_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L9_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L10_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L10_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L11_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L11_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L12_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L12_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L13_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L13_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L14_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L14_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L15_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L15_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L16_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L16_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L17_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L17_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L18_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L18_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L19_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L19_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L20_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L20_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L21_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L21_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L22_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L22_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L23_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L23_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L24_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L24_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L25_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_L25_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_CLK1_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_CLK1_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_CLK2_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_CLK2_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_LVS1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_LVS2),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_NCP_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_NCP_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_CXO_BUFFERS),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_USB_OTG_SWITCH),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_HDMI_SWITCH),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_QDSS_CLK),
		MSM_RPM_STATUS_ID_MAP(8930, PM8038_VOLTAGE_CORNER),
	},
	.target_ctrl_id = {
		MSM_RPM_CTRL_MAP(8930, VERSION_MAJOR),
		MSM_RPM_CTRL_MAP(8930, VERSION_MINOR),
		MSM_RPM_CTRL_MAP(8930, VERSION_BUILD),
		MSM_RPM_CTRL_MAP(8930, REQ_CTX_0),
		MSM_RPM_CTRL_MAP(8930, REQ_SEL_0),
		MSM_RPM_CTRL_MAP(8930, ACK_CTX_0),
		MSM_RPM_CTRL_MAP(8930, ACK_SEL_0),
	},
	.sel_invalidate = MSM_RPM_8930_SEL_INVALIDATE,
	.sel_notification = MSM_RPM_8930_SEL_NOTIFICATION,
	.sel_last = MSM_RPM_8930_SEL_LAST,
	.ver = {3, 0, 0},
};

struct msm_rpm_platform_data msm8930_rpm_data_pm8917 __initdata = {
	.reg_base_addrs = {
		[MSM_RPM_PAGE_STATUS] = MSM_RPM_BASE,
		[MSM_RPM_PAGE_CTRL] = MSM_RPM_BASE + 0x400,
		[MSM_RPM_PAGE_REQ] = MSM_RPM_BASE + 0x600,
		[MSM_RPM_PAGE_ACK] = MSM_RPM_BASE + 0xa00,
	},
	.irq_ack = RPM_APCC_CPU0_GP_HIGH_IRQ,
	.irq_err = RPM_APCC_CPU0_GP_LOW_IRQ,
	.irq_wakeup = RPM_APCC_CPU0_WAKE_UP_IRQ,
	.ipc_rpm_reg = MSM_APCS_GCC_BASE + 0x008,
	.ipc_rpm_val = 4,
	.target_id = {
		MSM_RPM_MAP(8930, NOTIFICATION_CONFIGURED_0, NOTIFICATION, 4),
		MSM_RPM_MAP(8930, NOTIFICATION_REGISTERED_0, NOTIFICATION, 4),
		MSM_RPM_MAP(8930, INVALIDATE_0, INVALIDATE, 8),
		MSM_RPM_MAP(8960, TRIGGER_TIMED_TO, TRIGGER_TIMED, 1),
		MSM_RPM_MAP(8960, TRIGGER_TIMED_SCLK_COUNT, TRIGGER_TIMED, 1),
		MSM_RPM_MAP(8930, RPM_CTL, RPM_CTL, 1),
		MSM_RPM_MAP(8930, CXO_CLK, CXO_CLK, 1),
		MSM_RPM_MAP(8930, PXO_CLK, PXO_CLK, 1),
		MSM_RPM_MAP(8930, APPS_FABRIC_CLK, APPS_FABRIC_CLK, 1),
		MSM_RPM_MAP(8930, SYSTEM_FABRIC_CLK, SYSTEM_FABRIC_CLK, 1),
		MSM_RPM_MAP(8930, MM_FABRIC_CLK, MM_FABRIC_CLK, 1),
		MSM_RPM_MAP(8930, DAYTONA_FABRIC_CLK, DAYTONA_FABRIC_CLK, 1),
		MSM_RPM_MAP(8930, SFPB_CLK, SFPB_CLK, 1),
		MSM_RPM_MAP(8930, CFPB_CLK, CFPB_CLK, 1),
		MSM_RPM_MAP(8930, MMFPB_CLK, MMFPB_CLK, 1),
		MSM_RPM_MAP(8930, EBI1_CLK, EBI1_CLK, 1),
		MSM_RPM_MAP(8930, APPS_FABRIC_CFG_HALT_0,
				APPS_FABRIC_CFG_HALT, 2),
		MSM_RPM_MAP(8930, APPS_FABRIC_CFG_CLKMOD_0,
				APPS_FABRIC_CFG_CLKMOD, 3),
		MSM_RPM_MAP(8930, APPS_FABRIC_CFG_IOCTL,
				APPS_FABRIC_CFG_IOCTL, 1),
		MSM_RPM_MAP(8930, APPS_FABRIC_ARB_0, APPS_FABRIC_ARB, 6),
		MSM_RPM_MAP(8930, SYS_FABRIC_CFG_HALT_0,
				SYS_FABRIC_CFG_HALT, 2),
		MSM_RPM_MAP(8930, SYS_FABRIC_CFG_CLKMOD_0,
				SYS_FABRIC_CFG_CLKMOD, 3),
		MSM_RPM_MAP(8930, SYS_FABRIC_CFG_IOCTL,
				SYS_FABRIC_CFG_IOCTL, 1),
		MSM_RPM_MAP(8930, SYSTEM_FABRIC_ARB_0,
				SYSTEM_FABRIC_ARB, 20),
		MSM_RPM_MAP(8930, MMSS_FABRIC_CFG_HALT_0,
				MMSS_FABRIC_CFG_HALT, 2),
		MSM_RPM_MAP(8930, MMSS_FABRIC_CFG_CLKMOD_0,
				MMSS_FABRIC_CFG_CLKMOD, 3),
		MSM_RPM_MAP(8930, MMSS_FABRIC_CFG_IOCTL,
				MMSS_FABRIC_CFG_IOCTL, 1),
		MSM_RPM_MAP(8930, MM_FABRIC_ARB_0, MM_FABRIC_ARB, 11),
		MSM_RPM_MAP(8930, PM8917_S1_0, PM8917_S1, 2),
		MSM_RPM_MAP(8930, PM8917_S2_0, PM8917_S2, 2),
		MSM_RPM_MAP(8930, PM8917_S3_0, PM8917_S3, 2),
		MSM_RPM_MAP(8930, PM8917_S4_0, PM8917_S4, 2),
		MSM_RPM_MAP(8930, PM8917_S5_0, PM8917_S5, 2),
		MSM_RPM_MAP(8930, PM8917_S6_0, PM8917_S6, 2),
		MSM_RPM_MAP(8930, PM8917_S7_0, PM8917_S7, 2),
		MSM_RPM_MAP(8930, PM8917_S8_0, PM8917_S8, 2),
		MSM_RPM_MAP(8930, PM8917_L1_0, PM8917_L1, 2),
		MSM_RPM_MAP(8930, PM8917_L2_0, PM8917_L2, 2),
		MSM_RPM_MAP(8930, PM8917_L3_0, PM8917_L3, 2),
		MSM_RPM_MAP(8930, PM8917_L4_0, PM8917_L4, 2),
		MSM_RPM_MAP(8930, PM8917_L5_0, PM8917_L5, 2),
		MSM_RPM_MAP(8930, PM8917_L6_0, PM8917_L6, 2),
		MSM_RPM_MAP(8930, PM8917_L7_0, PM8917_L7, 2),
		MSM_RPM_MAP(8930, PM8917_L8_0, PM8917_L8, 2),
		MSM_RPM_MAP(8930, PM8917_L9_0, PM8917_L9, 2),
		MSM_RPM_MAP(8930, PM8917_L10_0, PM8917_L10, 2),
		MSM_RPM_MAP(8930, PM8917_L11_0, PM8917_L11, 2),
		MSM_RPM_MAP(8930, PM8917_L12_0, PM8917_L12, 2),
		MSM_RPM_MAP(8930, PM8917_L14_0, PM8917_L14, 2),
		MSM_RPM_MAP(8930, PM8917_L15_0, PM8917_L15, 2),
		MSM_RPM_MAP(8930, PM8917_L16_0, PM8917_L16, 2),
		MSM_RPM_MAP(8930, PM8917_L17_0, PM8917_L17, 2),
		MSM_RPM_MAP(8930, PM8917_L18_0, PM8917_L18, 2),
		MSM_RPM_MAP(8930, PM8917_L21_0, PM8917_L21, 2),
		MSM_RPM_MAP(8930, PM8917_L22_0, PM8917_L22, 2),
		MSM_RPM_MAP(8930, PM8917_L23_0, PM8917_L23, 2),
		MSM_RPM_MAP(8930, PM8917_L24_0, PM8917_L24, 2),
		MSM_RPM_MAP(8930, PM8917_L25_0, PM8917_L25, 2),
		MSM_RPM_MAP(8930, PM8917_L26_0, PM8917_L26, 2),
		MSM_RPM_MAP(8930, PM8917_L27_0, PM8917_L27, 2),
		MSM_RPM_MAP(8930, PM8917_L28_0, PM8917_L28, 2),
		MSM_RPM_MAP(8930, PM8917_L29_0, PM8917_L29, 2),
		MSM_RPM_MAP(8930, PM8917_L30_0, PM8917_L30, 2),
		MSM_RPM_MAP(8930, PM8917_L31_0, PM8917_L31, 2),
		MSM_RPM_MAP(8930, PM8917_L32_0, PM8917_L32, 2),
		MSM_RPM_MAP(8930, PM8917_L33_0, PM8917_L33, 2),
		MSM_RPM_MAP(8930, PM8917_L34_0, PM8917_L34, 2),
		MSM_RPM_MAP(8930, PM8917_L35_0, PM8917_L35, 2),
		MSM_RPM_MAP(8930, PM8917_L36_0, PM8917_L36, 2),
		MSM_RPM_MAP(8930, PM8917_CLK1_0, PM8917_CLK1, 2),
		MSM_RPM_MAP(8930, PM8917_CLK2_0, PM8917_CLK2, 2),
		MSM_RPM_MAP(8930, PM8917_LVS1, PM8917_LVS1, 1),
		MSM_RPM_MAP(8930, PM8917_LVS3, PM8917_LVS3, 1),
		MSM_RPM_MAP(8930, PM8917_LVS4, PM8917_LVS4, 1),
		MSM_RPM_MAP(8930, PM8917_LVS5, PM8917_LVS5, 1),
		MSM_RPM_MAP(8930, PM8917_LVS6, PM8917_LVS6, 1),
		MSM_RPM_MAP(8930, PM8917_LVS7, PM8917_LVS7, 1),
		MSM_RPM_MAP_PMIC(8930, 8917, NCP_0, NCP, 2),
		MSM_RPM_MAP_PMIC(8930, 8917, CXO_BUFFERS, CXO_BUFFERS, 1),
		MSM_RPM_MAP_PMIC(8930, 8917, USB_OTG_SWITCH, USB_OTG_SWITCH, 1),
		MSM_RPM_MAP_PMIC(8930, 8917, HDMI_SWITCH, HDMI_SWITCH, 1),
		MSM_RPM_MAP_PMIC(8930, 8917, QDSS_CLK, QDSS_CLK, 1),
		MSM_RPM_MAP_PMIC(8930, 8917, VOLTAGE_CORNER, VOLTAGE_CORNER, 1),
	},
	.target_status = {
		MSM_RPM_STATUS_ID_MAP(8930, VERSION_MAJOR),
		MSM_RPM_STATUS_ID_MAP(8930, VERSION_MINOR),
		MSM_RPM_STATUS_ID_MAP(8930, VERSION_BUILD),
		MSM_RPM_STATUS_ID_MAP(8930, SUPPORTED_RESOURCES_0),
		MSM_RPM_STATUS_ID_MAP(8930, SUPPORTED_RESOURCES_1),
		MSM_RPM_STATUS_ID_MAP(8930, SUPPORTED_RESOURCES_2),
		MSM_RPM_STATUS_ID_MAP(8930, RESERVED_SUPPORTED_RESOURCES_0),
		MSM_RPM_STATUS_ID_MAP(8930, SEQUENCE),
		MSM_RPM_STATUS_ID_MAP(8930, RPM_CTL),
		MSM_RPM_STATUS_ID_MAP(8930, CXO_CLK),
		MSM_RPM_STATUS_ID_MAP(8930, PXO_CLK),
		MSM_RPM_STATUS_ID_MAP(8930, APPS_FABRIC_CLK),
		MSM_RPM_STATUS_ID_MAP(8930, SYSTEM_FABRIC_CLK),
		MSM_RPM_STATUS_ID_MAP(8930, MM_FABRIC_CLK),
		MSM_RPM_STATUS_ID_MAP(8930, DAYTONA_FABRIC_CLK),
		MSM_RPM_STATUS_ID_MAP(8930, SFPB_CLK),
		MSM_RPM_STATUS_ID_MAP(8930, CFPB_CLK),
		MSM_RPM_STATUS_ID_MAP(8930, MMFPB_CLK),
		MSM_RPM_STATUS_ID_MAP(8930, EBI1_CLK),
		MSM_RPM_STATUS_ID_MAP(8930, APPS_FABRIC_CFG_HALT),
		MSM_RPM_STATUS_ID_MAP(8930, APPS_FABRIC_CFG_CLKMOD),
		MSM_RPM_STATUS_ID_MAP(8930, APPS_FABRIC_CFG_IOCTL),
		MSM_RPM_STATUS_ID_MAP(8930, APPS_FABRIC_ARB),
		MSM_RPM_STATUS_ID_MAP(8930, SYS_FABRIC_CFG_HALT),
		MSM_RPM_STATUS_ID_MAP(8930, SYS_FABRIC_CFG_CLKMOD),
		MSM_RPM_STATUS_ID_MAP(8930, SYS_FABRIC_CFG_IOCTL),
		MSM_RPM_STATUS_ID_MAP(8930, SYSTEM_FABRIC_ARB),
		MSM_RPM_STATUS_ID_MAP(8930, MMSS_FABRIC_CFG_HALT),
		MSM_RPM_STATUS_ID_MAP(8930, MMSS_FABRIC_CFG_CLKMOD),
		MSM_RPM_STATUS_ID_MAP(8930, MMSS_FABRIC_CFG_IOCTL),
		MSM_RPM_STATUS_ID_MAP(8930, MM_FABRIC_ARB),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_S1_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_S1_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_S2_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_S2_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_S3_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_S3_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_S4_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_S4_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_S5_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_S5_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_S6_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_S6_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_S7_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_S7_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_S8_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_S8_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L1_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L1_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L2_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L2_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L3_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L3_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L4_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L4_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L5_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L5_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L6_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L6_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L7_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L7_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L8_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L8_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L9_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L9_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L10_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L10_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L11_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L11_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L12_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L12_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L14_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L14_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L15_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L15_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L16_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L16_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L17_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L17_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L18_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L18_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L21_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L21_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L22_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L22_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L23_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L23_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L24_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L24_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L25_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L25_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L26_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L26_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L27_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L27_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L28_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L28_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L29_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L29_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L30_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L30_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L31_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L31_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L32_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L32_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L33_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L33_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L34_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L34_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L35_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L35_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L36_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_L36_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_CLK1_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_CLK1_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_CLK2_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_CLK2_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_LVS1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_LVS3),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_LVS4),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_LVS5),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_LVS6),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_LVS7),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_NCP_0),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_NCP_1),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_CXO_BUFFERS),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_USB_OTG_SWITCH),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_HDMI_SWITCH),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_QDSS_CLK),
		MSM_RPM_STATUS_ID_MAP(8930, PM8917_VOLTAGE_CORNER),
	},
	.target_ctrl_id = {
		MSM_RPM_CTRL_MAP(8930, VERSION_MAJOR),
		MSM_RPM_CTRL_MAP(8930, VERSION_MINOR),
		MSM_RPM_CTRL_MAP(8930, VERSION_BUILD),
		MSM_RPM_CTRL_MAP(8930, REQ_CTX_0),
		MSM_RPM_CTRL_MAP(8930, REQ_SEL_0),
		MSM_RPM_CTRL_MAP(8930, ACK_CTX_0),
		MSM_RPM_CTRL_MAP(8930, ACK_SEL_0),
	},
	.sel_invalidate = MSM_RPM_8930_SEL_INVALIDATE,
	.sel_notification = MSM_RPM_8930_SEL_NOTIFICATION,
	.sel_last = MSM_RPM_8930_SEL_LAST,
	.ver = {3, 0, 0},
};
struct platform_device msm8930_rpm_device = {
	.name   = "msm_rpm",
	.id     = -1,
};

static struct msm_rpm_log_platform_data msm_rpm_log_pdata = {
	.phys_addr_base = 0x10B6A0,
	.reg_offsets = {
		[MSM_RPM_LOG_PAGE_INDICES] = 0x00000080,
		[MSM_RPM_LOG_PAGE_BUFFER]  = 0x000000A0,
	},
	.phys_size = SZ_8K,
	.log_len = 8192,		  /* log's buffer length in bytes */
	.log_len_mask = (8192 >> 2) - 1,  /* length mask in units of u32 */
};

struct platform_device msm8930_rpm_log_device = {
	.name	= "msm_rpm_log",
	.id	= -1,
	.dev	= {
		.platform_data = &msm_rpm_log_pdata,
	},
};

static struct msm_rpmstats_platform_data msm_rpm_stat_pdata = {
	.version = 1,
};

static struct resource msm_rpm_stat_resource[] = {
	{
		.start	= 0x0010D204,
		.end	= 0x0010D204 + SZ_8K,
		.flags	= IORESOURCE_MEM,
		.name	= "phys_addr_base"

	},
};


struct platform_device msm8930_rpm_stat_device = {
	.name = "msm_rpm_stat",
	.id = -1,
	.resource = msm_rpm_stat_resource,
	.num_resources	= ARRAY_SIZE(msm_rpm_stat_resource),
	.dev	= {
		.platform_data = &msm_rpm_stat_pdata,
	}
};

static struct resource resources_rpm_master_stats[] = {
	{
		.start	= MSM8930_RPM_MASTER_STATS_BASE,
		.end	= MSM8930_RPM_MASTER_STATS_BASE + SZ_256,
		.flags	= IORESOURCE_MEM,
	},
};

static char *master_names[] = {
	"KPSS",
	"MPSS",
	"LPASS",
	"RIVA",
};

static struct msm_rpm_master_stats_platform_data msm_rpm_master_stat_pdata = {
	.masters = master_names,
	.num_masters = ARRAY_SIZE(master_names),
	.master_offset = 32,
};

struct platform_device msm8930_rpm_master_stat_device = {
	.name = "msm_rpm_master_stats",
	.id = -1,
	.num_resources	= ARRAY_SIZE(resources_rpm_master_stats),
	.resource	= resources_rpm_master_stats,
	.dev = {
		.platform_data = &msm_rpm_master_stat_pdata,
	},
};

static struct resource msm_rpm_rbcpr_resource = {
	.start = 0x0010DB00,
	.end = 0x0010DB00 + SZ_8K - 1,
	.flags = IORESOURCE_MEM,
};

static struct msm_rpmrbcpr_platform_data msm_rpm_rbcpr_pdata = {
	.rbcpr_data = {
		.upside_steps = 1,
		.downside_steps = 2,
		.svs_voltage = 1050000,
		.nominal_voltage = 1162500,
		.turbo_voltage = 1287500,
	},
};

struct platform_device msm8930_rpm_rbcpr_device = {
	.name = "msm_rpm_rbcpr",
	.id = -1,
	.dev = {
		.platform_data = &msm_rpm_rbcpr_pdata,
	},
	.resource = &msm_rpm_rbcpr_resource,
};

struct platform_device msm_bus_8930_sys_fabric = {
	.name  = "msm_bus_fabric",
	.id    =  MSM_BUS_FAB_SYSTEM,
};
struct platform_device msm_bus_8930_apps_fabric = {
	.name  = "msm_bus_fabric",
	.id    = MSM_BUS_FAB_APPSS,
};
struct platform_device msm_bus_8930_mm_fabric = {
	.name  = "msm_bus_fabric",
	.id    = MSM_BUS_FAB_MMSS,
};
struct platform_device msm_bus_8930_sys_fpb = {
	.name  = "msm_bus_fabric",
	.id    = MSM_BUS_FAB_SYSTEM_FPB,
};
struct platform_device msm_bus_8930_cpss_fpb = {
	.name  = "msm_bus_fabric",
	.id    = MSM_BUS_FAB_CPSS_FPB,
};

struct platform_device msm8627_device_acpuclk = {
	.name		= "acpuclk-8627",
	.id		= -1,
};

struct platform_device msm8930_device_acpuclk = {
	.name		= "acpuclk-8930",
	.id		= -1,
};

struct platform_device msm8930aa_device_acpuclk = {
	.name		= "acpuclk-8930aa",
	.id		= -1,
};

struct platform_device msm8930ab_device_acpuclk = {
	.name		= "acpuclk-8930ab",
	.id		= -1,
};

static struct fs_driver_data gfx3d_fs_data = {
	.clks = (struct fs_clk_data[]){
		{ .name = "core_clk", .reset_rate = 27000000 },
		{ .name = "iface_clk" },
		{ .name = "bus_clk" },
		{ 0 }
	},
	.bus_port0 = MSM_BUS_MASTER_GRAPHICS_3D,
};

static struct fs_driver_data ijpeg_fs_data = {
	.clks = (struct fs_clk_data[]){
		{ .name = "core_clk" },
		{ .name = "iface_clk" },
		{ .name = "bus_clk" },
		{ 0 }
	},
	.bus_port0 = MSM_BUS_MASTER_JPEG_ENC,
};

static struct fs_driver_data mdp_fs_data_8930 = {
	.clks = (struct fs_clk_data[]){
		{ .name = "core_clk" },
		{ .name = "iface_clk" },
		{ .name = "bus_clk" },
		{ .name = "vsync_clk" },
		{ .name = "lut_clk" },
		{ .name = "tv_src_clk" },
		{ .name = "tv_clk" },
		{ .name = "reset1_clk" },
		{ 0 }
	},
	.bus_port0 = MSM_BUS_MASTER_MDP_PORT0,
	.bus_port1 = MSM_BUS_MASTER_MDP_PORT1,
};

static struct fs_driver_data mdp_fs_data_8930_pm8917 = {
	.clks = (struct fs_clk_data[]){
		{ .name = "core_clk" },
		{ .name = "iface_clk" },
		{ .name = "bus_clk" },
		{ .name = "vsync_clk" },
		{ .name = "lut_clk" },
		{ .name = "reset1_clk" },
		{ 0 }
	},
	.bus_port0 = MSM_BUS_MASTER_MDP_PORT0,
	.bus_port1 = MSM_BUS_MASTER_MDP_PORT1,
};

static struct fs_driver_data mdp_fs_data_8627 = {
	.clks = (struct fs_clk_data[]){
		{ .name = "core_clk" },
		{ .name = "iface_clk" },
		{ .name = "bus_clk" },
		{ .name = "vsync_clk" },
		{ .name = "lut_clk" },
		{ .name = "reset1_clk" },
		{ 0 }
	},
	.bus_port0 = MSM_BUS_MASTER_MDP_PORT0,
	.bus_port1 = MSM_BUS_MASTER_MDP_PORT1,
};

static struct fs_driver_data rot_fs_data = {
	.clks = (struct fs_clk_data[]){
		{ .name = "core_clk" },
		{ .name = "iface_clk" },
		{ .name = "bus_clk" },
		{ 0 }
	},
	.bus_port0 = MSM_BUS_MASTER_ROTATOR,
};

static struct fs_driver_data ved_fs_data = {
	.clks = (struct fs_clk_data[]){
		{ .name = "core_clk" },
		{ .name = "iface_clk" },
		{ .name = "bus_clk" },
		{ 0 }
	},
	.bus_port0 = MSM_BUS_MASTER_HD_CODEC_PORT0,
	.bus_port1 = MSM_BUS_MASTER_HD_CODEC_PORT1,
};

static struct fs_driver_data vfe_fs_data = {
	.clks = (struct fs_clk_data[]){
		{ .name = "core_clk" },
		{ .name = "iface_clk" },
		{ .name = "bus_clk" },
		{ 0 }
	},
	.bus_port0 = MSM_BUS_MASTER_VFE,
};

static struct fs_driver_data vpe_fs_data = {
	.clks = (struct fs_clk_data[]){
		{ .name = "core_clk" },
		{ .name = "iface_clk" },
		{ .name = "bus_clk" },
		{ 0 }
	},
	.bus_port0 = MSM_BUS_MASTER_VPE,
};

struct platform_device *msm8930_footswitch[] __initdata = {
	FS_8X60(FS_MDP,    "vdd",	"mdp.0",	&mdp_fs_data_8930),
	FS_8X60(FS_ROT,    "vdd",	"msm_rotator.0", &rot_fs_data),
	FS_8X60(FS_IJPEG,  "vdd",	"msm_gemini.0", &ijpeg_fs_data),
	FS_8X60(FS_VFE,    "vdd",	"msm_vfe.0",	&vfe_fs_data),
	FS_8X60(FS_VPE,    "vdd",	"msm_vpe.0",	&vpe_fs_data),
	FS_8X60(FS_GFX3D,  "vdd",	"kgsl-3d0.0",	&gfx3d_fs_data),
	FS_8X60(FS_VED,    "vdd",	"msm_vidc.0",	&ved_fs_data),
};
unsigned msm8930_num_footswitch __initdata = ARRAY_SIZE(msm8930_footswitch);

struct platform_device *msm8930_pm8917_footswitch[] __initdata = {
	FS_8X60(FS_MDP,    "vdd",	"mdp.0",      &mdp_fs_data_8930_pm8917),
	FS_8X60(FS_ROT,    "vdd",	"msm_rotator.0", &rot_fs_data),
	FS_8X60(FS_IJPEG,  "vdd",	"msm_gemini.0", &ijpeg_fs_data),
	FS_8X60(FS_VFE,    "vdd",	"msm_vfe.0",	&vfe_fs_data),
	FS_8X60(FS_VPE,    "vdd",	"msm_vpe.0",	&vpe_fs_data),
	FS_8X60(FS_GFX3D,  "vdd",	"kgsl-3d0.0",	&gfx3d_fs_data),
	FS_8X60(FS_VED,    "vdd",	"msm_vidc.0",	&ved_fs_data),
};
unsigned msm8930_pm8917_num_footswitch __initdata =
		ARRAY_SIZE(msm8930_pm8917_footswitch);

struct platform_device *msm8627_footswitch[] __initdata = {
	FS_8X60(FS_MDP,    "vdd",	"mdp.0",	&mdp_fs_data_8627),
	FS_8X60(FS_ROT,    "vdd",	"msm_rotator.0", &rot_fs_data),
	FS_8X60(FS_IJPEG,  "vdd",	"msm_gemini.0", &ijpeg_fs_data),
	FS_8X60(FS_VFE,    "vdd",	"msm_vfe.0",	&vfe_fs_data),
	FS_8X60(FS_VPE,    "vdd",	"msm_vpe.0",	&vpe_fs_data),
	FS_8X60(FS_GFX3D,  "vdd",	"kgsl-3d0.0",	&gfx3d_fs_data),
	FS_8X60(FS_VED,    "vdd",	"msm_vidc.0",	&ved_fs_data),
};
unsigned msm8627_num_footswitch __initdata = ARRAY_SIZE(msm8627_footswitch);

/* MSM Video core device */
#ifdef CONFIG_MSM_BUS_SCALING
static struct msm_bus_vectors vidc_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};
static struct msm_bus_vectors vidc_venc_vga_vectors[] = {
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 54525952,
		.ib  = 436207616,
	},
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 72351744,
		.ib  = 289406976,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 500000,
		.ib  = 1000000,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 500000,
		.ib  = 1000000,
	},
};
static struct msm_bus_vectors vidc_vdec_vga_vectors[] = {
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 40894464,
		.ib  = 327155712,
	},
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 48234496,
		.ib  = 192937984,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 500000,
		.ib  = 2000000,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 500000,
		.ib  = 2000000,
	},
};
static struct msm_bus_vectors vidc_venc_720p_vectors[] = {
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 163577856,
		.ib  = 1308622848,
	},
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 219152384,
		.ib  = 876609536,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 1750000,
		.ib  = 3500000,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 1750000,
		.ib  = 3500000,
	},
};
static struct msm_bus_vectors vidc_vdec_720p_vectors[] = {
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 121634816,
		.ib  = 973078528,
	},
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 155189248,
		.ib  = 620756992,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 1750000,
		.ib  = 7000000,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 1750000,
		.ib  = 7000000,
	},
};
static struct msm_bus_vectors vidc_venc_1080p_vectors[] = {
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 372244480,
		.ib  = 2560000000U,
	},
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 501219328,
		.ib  = 2560000000U,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 2500000,
		.ib  = 5000000,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 2500000,
		.ib  = 5000000,
	},
};
static struct msm_bus_vectors vidc_vdec_1080p_vectors[] = {
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 222298112,
		.ib  = 2560000000U,
	},
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 330301440,
		.ib  = 2560000000U,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 2500000,
		.ib  = 700000000,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 2500000,
		.ib  = 10000000,
	},
};
static struct msm_bus_vectors vidc_venc_1080p_turbo_vectors[] = {
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 222298112,
		.ib  = 3522000000U,
	},
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 330301440,
		.ib  = 3522000000U,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 2500000,
		.ib  = 700000000,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 2500000,
		.ib  = 10000000,
	},
};
static struct msm_bus_vectors vidc_vdec_1080p_turbo_vectors[] = {
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 222298112,
		.ib  = 3522000000U,
	},
	{
		.src = MSM_BUS_MASTER_HD_CODEC_PORT1,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 330301440,
		.ib  = 3522000000U,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 2500000,
		.ib  = 700000000,
	},
	{
		.src = MSM_BUS_MASTER_AMPSS_M0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 2500000,
		.ib  = 10000000,
	},
};

static struct msm_bus_paths vidc_bus_client_config[] = {
	{
		ARRAY_SIZE(vidc_init_vectors),
		vidc_init_vectors,
	},
	{
		ARRAY_SIZE(vidc_venc_vga_vectors),
		vidc_venc_vga_vectors,
	},
	{
		ARRAY_SIZE(vidc_vdec_vga_vectors),
		vidc_vdec_vga_vectors,
	},
	{
		ARRAY_SIZE(vidc_venc_720p_vectors),
		vidc_venc_720p_vectors,
	},
	{
		ARRAY_SIZE(vidc_vdec_720p_vectors),
		vidc_vdec_720p_vectors,
	},
	{
		ARRAY_SIZE(vidc_venc_1080p_vectors),
		vidc_venc_1080p_vectors,
	},
	{
		ARRAY_SIZE(vidc_vdec_1080p_vectors),
		vidc_vdec_1080p_vectors,
	},
	{
		ARRAY_SIZE(vidc_venc_1080p_turbo_vectors),
		vidc_vdec_1080p_turbo_vectors,
	},
	{
		ARRAY_SIZE(vidc_vdec_1080p_turbo_vectors),
		vidc_vdec_1080p_turbo_vectors,
	},
};

static struct msm_bus_scale_pdata vidc_bus_client_data = {
	vidc_bus_client_config,
	ARRAY_SIZE(vidc_bus_client_config),
	.name = "vidc",
};
#endif

#define MSM_VIDC_BASE_PHYS 0x04400000
#define MSM_VIDC_BASE_SIZE 0x00100000

static struct resource apq8930_device_vidc_resources[] = {
	{
		.start	= MSM_VIDC_BASE_PHYS,
		.end	= MSM_VIDC_BASE_PHYS + MSM_VIDC_BASE_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= VCODEC_IRQ,
		.end	= VCODEC_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

struct msm_vidc_platform_data apq8930_vidc_platform_data = {
#ifdef CONFIG_MSM_BUS_SCALING
	.vidc_bus_client_pdata = &vidc_bus_client_data,
#endif
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	.memtype = ION_CP_MM_HEAP_ID,
	.enable_ion = 1,
	.cp_enabled = 1,
#else
	.memtype = MEMTYPE_EBI1,
	.enable_ion = 0,
#endif
	.disable_dmx = 1,
	.disable_fullhd = 0,
	.cont_mode_dpb_count = 18,
	.fw_addr = 0x9fe00000,
};

struct platform_device apq8930_msm_device_vidc = {
	.name = "msm_vidc",
	.id = 0,
	.num_resources = ARRAY_SIZE(apq8930_device_vidc_resources),
	.resource = apq8930_device_vidc_resources,
	.dev = {
		.platform_data = &apq8930_vidc_platform_data,
	},
};

struct platform_device *vidc_device[] __initdata = {
	&apq8930_msm_device_vidc
};

void __init msm8930_add_vidc_device(void)
{
	if (cpu_is_msm8627()) {
		struct msm_vidc_platform_data *pdata;
		pdata = (struct msm_vidc_platform_data *)
			apq8930_msm_device_vidc.dev.platform_data;
		pdata->disable_fullhd = 1;
	}
	platform_add_devices(vidc_device, ARRAY_SIZE(vidc_device));
}

struct msm_iommu_domain_name msm8930_iommu_ctx_names[] = {
	/* Camera */
	{
		.name = "ijpeg_src",
		.domain = CAMERA_DOMAIN,
	},
	/* Camera */
	{
		.name = "ijpeg_dst",
		.domain = CAMERA_DOMAIN,
	},
	/* Camera */
	{
		.name = "jpegd_src",
		.domain = CAMERA_DOMAIN,
	},
	/* Camera */
	{
		.name = "jpegd_dst",
		.domain = CAMERA_DOMAIN,
	},
	/* Rotator */
	{
		.name = "rot_src",
		.domain = ROTATOR_SRC_DOMAIN,
	},
	/* Rotator */
	{
		.name = "rot_dst",
		.domain = ROTATOR_SRC_DOMAIN,
	},
	/* Video */
	{
		.name = "vcodec_a_mm1",
		.domain = VIDEO_DOMAIN,
	},
	/* Video */
	{
		.name = "vcodec_b_mm2",
		.domain = VIDEO_DOMAIN,
	},
	/* Video */
	{
		.name = "vcodec_a_stream",
		.domain = VIDEO_DOMAIN,
	},
};

static struct mem_pool msm8930_video_pools[] =  {
	/*
	 * Video hardware has the following requirements:
	 * 1. All video addresses used by the video hardware must be at a higher
	 *    address than video firmware address.
	 * 2. Video hardware can only access a range of 256MB from the base of
	 *    the video firmware.
	*/
	[VIDEO_FIRMWARE_POOL] =
	/* Low addresses, intended for video firmware */
		{
			.paddr	= SZ_128K,
			.size	= SZ_16M - SZ_128K,
		},
	[VIDEO_MAIN_POOL] =
	/* Main video pool */
		{
			.paddr	= SZ_16M,
			.size	= SZ_256M - SZ_16M,
		},
	[GEN_POOL] =
	/* Remaining address space up to 2G */
		{
			.paddr	= SZ_256M,
			.size	= SZ_2G - SZ_256M,
		},
};

static struct mem_pool msm8930_camera_pools[] =  {
	[GEN_POOL] =
	/* One address space for camera */
		{
			.paddr	= SZ_128K,
			.size	= SZ_2G - SZ_128K,
		},
};

static struct mem_pool msm8930_display_read_pools[] =  {
	[GEN_POOL] =
	/* One address space for display reads */
		{
			.paddr	= SZ_128K,
			.size	= SZ_2G - SZ_128K,
		},
};

static struct mem_pool msm8930_rotator_src_pools[] =  {
	[GEN_POOL] =
	/* One address space for rotator src */
		{
			.paddr	= SZ_128K,
			.size	= SZ_2G - SZ_128K,
		},
};

static struct msm_iommu_domain msm8930_iommu_domains[] = {
		[VIDEO_DOMAIN] = {
			.iova_pools = msm8930_video_pools,
			.npools = ARRAY_SIZE(msm8930_video_pools),
		},
		[CAMERA_DOMAIN] = {
			.iova_pools = msm8930_camera_pools,
			.npools = ARRAY_SIZE(msm8930_camera_pools),
		},
		[DISPLAY_READ_DOMAIN] = {
			.iova_pools = msm8930_display_read_pools,
			.npools = ARRAY_SIZE(msm8930_display_read_pools),
		},
		[ROTATOR_SRC_DOMAIN] = {
			.iova_pools = msm8930_rotator_src_pools,
			.npools = ARRAY_SIZE(msm8930_rotator_src_pools),
		},
};

struct iommu_domains_pdata msm8930_iommu_domain_pdata = {
	.domains = msm8930_iommu_domains,
	.ndomains = ARRAY_SIZE(msm8930_iommu_domains),
	.domain_names = msm8930_iommu_ctx_names,
	.nnames = ARRAY_SIZE(msm8930_iommu_ctx_names),
	.domain_alloc_flags = 0,
};

struct platform_device msm8930_iommu_domain_device = {
	.name = "iommu_domains",
	.id = -1,
	.dev = {
		.platform_data = &msm8930_iommu_domain_pdata,
	}
};

struct msm_rtb_platform_data msm8930_rtb_pdata = {
	.size = SZ_1M,
};

static int __init msm_rtb_set_buffer_size(char *p)
{
	int s;

	s = memparse(p, NULL);
	msm8930_rtb_pdata.size = ALIGN(s, SZ_4K);
	return 0;
}
early_param("msm_rtb_size", msm_rtb_set_buffer_size);


struct platform_device msm8930_rtb_device = {
	.name           = "msm_rtb",
	.id             = -1,
	.dev            = {
		.platform_data = &msm8930_rtb_pdata,
	},
};

#define MSM8930_L1_SIZE  SZ_1M
/*
 * The actual L2 size is smaller but we need a larger buffer
 * size to store other dump information
 */
#define MSM8930_L2_SIZE  SZ_4M

struct msm_cache_dump_platform_data msm8930_cache_dump_pdata = {
	.l2_size = MSM8930_L2_SIZE,
	.l1_size = MSM8930_L1_SIZE,
};

struct platform_device msm8930_cache_dump_device = {
	.name           = "msm_cache_dump",
	.id             = -1,
	.dev            = {
		.platform_data = &msm8930_cache_dump_pdata,
	},
};
