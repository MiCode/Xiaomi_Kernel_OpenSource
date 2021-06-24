// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/bug.h>
#include "devapc-mtk-common.h"
#include "devapc-mt6781.h"

/* slave type, config_idx, device name, enable_vio_irq */
static struct mtk_device_info mt6781_infra_devices[] = {

	/* 0 */
	{E_DAPC_INFRA_PERI_SLAVE, 0, "INFRA_AO_TOPCKGEN", true},
	{E_DAPC_INFRA_PERI_SLAVE, 1, "INFRA_AO_INFRASYS_CONFIG_REGS", true},
	{E_DAPC_INFRA_PERI_SLAVE, 2, "IO_CFG_REG", true},
	{E_DAPC_INFRA_PERI_SLAVE, 3, "INFRA_AO_ PERICFG", true},
	{E_DAPC_INFRA_PERI_SLAVE, 4, "INFRA_AO_EFUSE_AO_DEBUG", true},
	{E_DAPC_INFRA_PERI_SLAVE, 5, "INFRA_AO_GPIO", true},
	{E_DAPC_INFRA_PERI_SLAVE, 6, "INFRA_AO_SLEEP_CONTROLLER", true},
	{E_DAPC_INFRA_PERI_SLAVE, 7, "INFRA_AO_TOPRGU", true},
	{E_DAPC_INFRA_PERI_SLAVE, 8, "INFRA_AO_APXGPT", true},
	{E_DAPC_INFRA_PERI_SLAVE, 9, "INFRA_AO_RESERVE", true},

	/* 10 */
	{E_DAPC_INFRA_PERI_SLAVE, 10, "INFRA_AO_SEJ", true},
	{E_DAPC_INFRA_PERI_SLAVE, 11, "INFRA_AO_AP_CIRQ_EINT", true},
	{E_DAPC_INFRA_PERI_SLAVE, 12, "INFRA_AO_APMIXEDSYS", false},
	{E_DAPC_INFRA_PERI_SLAVE, 13, "INFRA_AO_PMIC_WRAP", true},
	{E_DAPC_INFRA_PERI_SLAVE, 14, "INFRA_AO_DEVICE_APC_AO_INFRA_PERI", true},
	{E_DAPC_INFRA_PERI_SLAVE, 15, "INFRA_AO_DEVICE_APC_AO_MM", true},
	{E_DAPC_INFRA_PERI_SLAVE, 16, "INFRA_AO_KEYPAD", true},
	{E_DAPC_INFRA_PERI_SLAVE, 17, "INFRA_AO_TOP_MISC", true},
	{E_DAPC_INFRA_PERI_SLAVE, 18, "INFRA_AO_ DVFS_CTRL_PROC", true},
	{E_DAPC_INFRA_PERI_SLAVE, 19, "INFRA_AO_IFNRA_TOP_MBIST_CTRL", true},

	/* 20 */
	{E_DAPC_INFRA_PERI_SLAVE, 20, "INFRA_AO_DPMAIF_AO_TOP", true},
	{E_DAPC_INFRA_PERI_SLAVE, 21, "INFRA_AO_PMIF", true},
	{E_DAPC_INFRA_PERI_SLAVE, 22, "INFRA_AO_AES_TOP_0", true},
	{E_DAPC_INFRA_PERI_SLAVE, 23, "INFRA_AO_SYS_TIMER", false},
	{E_DAPC_INFRA_PERI_SLAVE, 24, "INFRA_AO_MDEM_TEMP_SHARE", true},
	{E_DAPC_INFRA_PERI_SLAVE, 25, "INFRA_AO_DEVICE_APC_AO_MD", true},
	{E_DAPC_INFRA_PERI_SLAVE, 26, "INFRA_AO_SECURITY_AO", true},
	{E_DAPC_INFRA_PERI_SLAVE, 27, "INFRA_AO_SPMI_MST_WRAP", true},
	{E_DAPC_INFRA_PERI_SLAVE, 28, "INFRA_AO_SPM", true},
	{E_DAPC_INFRA_PERI_SLAVE, 29, "INFRA_AO_SPM", true},

	/* 30 */
	{E_DAPC_INFRA_PERI_SLAVE, 30, "INFRA_AO_SPM", true},
	{E_DAPC_INFRA_PERI_SLAVE, 31, "INFRA_AO_SPM", true},
	{E_DAPC_INFRA_PERI_SLAVE, 32, "INFRASYS_AP_DMA", true},
	{E_DAPC_INFRA_PERI_SLAVE, 33, "INFRASYS_RESERVE", true},
	{E_DAPC_INFRA_PERI_SLAVE, 34, "INFRASYS_RESERVE", true},
	{E_DAPC_INFRA_PERI_SLAVE, 35, "INFRASYS_SYS_CIRQ", true},
	{E_DAPC_INFRA_PERI_SLAVE, 36, "INFRASYS_RESERVE", true},
	{E_DAPC_INFRA_PERI_SLAVE, 37, "INFRASYS_RESERVE", true},
	{E_DAPC_INFRA_PERI_SLAVE, 38, "INFRASYS_DEVICE_APC", true},
	{E_DAPC_INFRA_PERI_SLAVE, 39, "INFRASYS_DBG_TRACKER", true},

	/* 40 */
	{E_DAPC_INFRA_PERI_SLAVE, 40, "INFRASYS_CCIF0_AP", true},
	{E_DAPC_INFRA_PERI_SLAVE, 41, "INFRASYS_CCIF0_MD", true},
	{E_DAPC_INFRA_PERI_SLAVE, 42, "INFRASYS_CCIF1_AP", true},
	{E_DAPC_INFRA_PERI_SLAVE, 43, "INFRASYS_CCIF1_MD", true},
	{E_DAPC_INFRA_PERI_SLAVE, 44, "INFRASYS_RESERVE", true},
	{E_DAPC_INFRA_PERI_SLAVE, 45, "INFRASYS_INFRA_PDN_REGISTER", true},
	{E_DAPC_INFRA_PERI_SLAVE, 46, "INFRASYS_TRNG", true},
	{E_DAPC_INFRA_PERI_SLAVE, 47, "INFRASYS_DX_CC", true},
	{E_DAPC_INFRA_PERI_SLAVE, 48, "INFRASYS_CCIF4_AP", true},
	{E_DAPC_INFRA_PERI_SLAVE, 49, "INFRASYS_CQ_DMA", true},

	/* 50 */
	{E_DAPC_INFRA_PERI_SLAVE, 50, "INFRASYS_CCIF4_MD", true},
	{E_DAPC_INFRA_PERI_SLAVE, 51, "INFRASYS_SRAMROM", true},
	/* specification problem, add here */
	{E_DAPC_INFRA_PERI_SLAVE, 52, "INFRASYS_RESERVE", true},
	{E_DAPC_INFRA_PERI_SLAVE, 53, "INFRASYS_RESERVE", true},
	{E_DAPC_INFRA_PERI_SLAVE, 54, "INFRASYS_RESERVE", true},
	{E_DAPC_INFRA_PERI_SLAVE, 55, "INFRASYS_RESERVE", true},
	{E_DAPC_INFRA_PERI_SLAVE, 56, "INFRASYS_EMI", true},
	{E_DAPC_INFRA_PERI_SLAVE, 57, "INFRASYS_DEVICE_MPU_LOW", true},
	{E_DAPC_INFRA_PERI_SLAVE, 58, "INFRASYS_EMI_MPU_REG", true},
	{E_DAPC_INFRA_PERI_SLAVE, 59, "INFRASYS_DPMAIF_TOP", false},

	/* 60 */
	{E_DAPC_INFRA_PERI_SLAVE, 60, "INFRASYS_DPMAIF_TOP", true},
	{E_DAPC_INFRA_PERI_SLAVE, 61, "INFRASYS_DPMAIF_TOP", true},
	{E_DAPC_INFRA_PERI_SLAVE, 62, "INFRASYS_DPMAIF_TOP", true},
	{E_DAPC_INFRA_PERI_SLAVE, 63, "INFRASYS_DRAMC_CH0_TOP0", true},
	{E_DAPC_INFRA_PERI_SLAVE, 64, "INFRASYS_DRAMC_CH0_TOP1", true},
	{E_DAPC_INFRA_PERI_SLAVE, 65, "INFRASYS_DRAMC_CH0_TOP2", true},
	{E_DAPC_INFRA_PERI_SLAVE, 66, "INFRASYS_DRAMC_CH0_TOP3", true},
	{E_DAPC_INFRA_PERI_SLAVE, 67, "INFRASYS_DRAMC_CH0_TOP4", true},
	{E_DAPC_INFRA_PERI_SLAVE, 68, "INFRASYS_DRAMC_CH0_TOP5", true},
	/* specification problem, add here */
	{E_DAPC_INFRA_PERI_SLAVE, 69, "INFRASYS_DRAMC_CH0_TOP6", true},

	/* 70 */
	{E_DAPC_INFRA_PERI_SLAVE, 70, "INFRASYS_GCE", false},
	{E_DAPC_INFRA_PERI_SLAVE, 71, "INFRASYS_DRAMC_CH1_TOP0", true},
	{E_DAPC_INFRA_PERI_SLAVE, 72, "INFRASYS_DRAMC_CH1_TOP1", true},
	{E_DAPC_INFRA_PERI_SLAVE, 73, "INFRASYS_DRAMC_CH1_TOP2", true},
	{E_DAPC_INFRA_PERI_SLAVE, 74, "INFRASYS_DRAMC_CH1_TOP3", true},
	{E_DAPC_INFRA_PERI_SLAVE, 75, "INFRASYS_DRAMC_CH1_TOP4", true},
	{E_DAPC_INFRA_PERI_SLAVE, 76, "INFRASYS_DRAMC_CH1_TOP5", true},
	{E_DAPC_INFRA_PERI_SLAVE, 77, "INFRASYS_DRAMC_CH1_TOP6", true},
	{E_DAPC_INFRA_PERI_SLAVE, 78, "INFRASYS_CCIF2_AP", true},
	{E_DAPC_INFRA_PERI_SLAVE, 79, "INFRASYS_CCIF2_MD", true},

	/* 80 */
	{E_DAPC_INFRA_PERI_SLAVE, 80, "INFRASYS_CCIF3_AP", true},
	{E_DAPC_INFRA_PERI_SLAVE, 81, "INFRASYS_CCIF3_MD", true},
	{E_DAPC_INFRA_PERI_SLAVE, 82, "INFRA_AO_SSPM_1_1", true},
	{E_DAPC_INFRA_PERI_SLAVE, 83, "INFRA_AO_SSPM_1_2", true},
	{E_DAPC_INFRA_PERI_SLAVE, 84, "INFRA_AO_SSPM_1_3", true},
	{E_DAPC_INFRA_PERI_SLAVE, 85, "INFRA_AO_SSPM_2", true},
	{E_DAPC_INFRA_PERI_SLAVE, 86, "INFRA_AO_SSPM_3", true},
	{E_DAPC_INFRA_PERI_SLAVE, 87, "INFRA_AO_SSPM_4", true},
	{E_DAPC_INFRA_PERI_SLAVE, 88, "INFRA_AO_SSPM_5", true},
	{E_DAPC_INFRA_PERI_SLAVE, 89, "INFRA_AO_SSPM_6", true},

	/* 90 */
	{E_DAPC_INFRA_PERI_SLAVE, 90, "INFRA_AO_SSPM_7", true},
	{E_DAPC_INFRA_PERI_SLAVE, 91, "INFRA_AO_SSPM_8", true},
	{E_DAPC_INFRA_PERI_SLAVE, 92, "INFRA_AO_SCP", true},
	{E_DAPC_INFRA_PERI_SLAVE, 93, "INFRA_AO_MCUCFG(*)", true},
	{E_DAPC_INFRA_PERI_SLAVE, 94, "INFRASYS_DBUGSYS", true},
	{E_DAPC_INFRA_PERI_SLAVE, 95, "PERISYS_RESERVE", true},
	{E_DAPC_INFRA_PERI_SLAVE, 96, "PERISYS_AUXADC", true},
	{E_DAPC_INFRA_PERI_SLAVE, 97, "PERISYS_UART0", true},
	{E_DAPC_INFRA_PERI_SLAVE, 98, "PERISYS_UART1", true},
	{E_DAPC_INFRA_PERI_SLAVE, 99, "PERISYS_I2C7", true},

	/* 100 */
	{E_DAPC_INFRA_PERI_SLAVE, 100, "PERISYS_I2C8", true},
	{E_DAPC_INFRA_PERI_SLAVE, 101, "PERISYS_PWM", true},
	{E_DAPC_INFRA_PERI_SLAVE, 102, "PERISYS_I2C0", true},
	{E_DAPC_INFRA_PERI_SLAVE, 103, "PERISYS_I2C1", true},
	{E_DAPC_INFRA_PERI_SLAVE, 104, "PERISYS_I2C2", true},
	{E_DAPC_INFRA_PERI_SLAVE, 105, "PERISYS_SPI0", true},
	{E_DAPC_INFRA_PERI_SLAVE, 106, "PERISYS_PTP", true},
	{E_DAPC_INFRA_PERI_SLAVE, 107, "PERISYS_BTIF", true},
	{E_DAPC_INFRA_PERI_SLAVE, 108, "PERISYS_I2C6", true},
	{E_DAPC_INFRA_PERI_SLAVE, 109, "PERISYS_DISP_PWM", true},

	/* 110 */
	{E_DAPC_INFRA_PERI_SLAVE, 110, "PERISYS_I2C3", true},
	{E_DAPC_INFRA_PERI_SLAVE, 111, "PERISYS_SPI1", true},
	{E_DAPC_INFRA_PERI_SLAVE, 112, "PERISYS_I2C4", true},
	{E_DAPC_INFRA_PERI_SLAVE, 113, "PERISYS_SPI2", true},
	{E_DAPC_INFRA_PERI_SLAVE, 114, "PERISYS_SPI3", true},
	{E_DAPC_INFRA_PERI_SLAVE, 115, "PERISYS_SPI4", true},
	{E_DAPC_INFRA_PERI_SLAVE, 116, "PERISYS_SPI5", true},
	{E_DAPC_INFRA_PERI_SLAVE, 117, "PERISYS_I2C5", true},
	{E_DAPC_INFRA_PERI_SLAVE, 118, "PERISYS_IMP_IIC_WRAP", true},
	{E_DAPC_INFRA_PERI_SLAVE, 119, "PERISYS_UART2", true},

	/* 120 */
	{E_DAPC_INFRA_PERI_SLAVE, 120, "PERISYS_I2C9", true},
	{E_DAPC_INFRA_PERI_SLAVE, 121, "PERISYS_USB", true},
	{E_DAPC_INFRA_PERI_SLAVE, 122, "PERISYS_USB_2.0_SUB", true},
	{E_DAPC_INFRA_PERI_SLAVE, 123, "PERISYS_MSDC0", true},
	{E_DAPC_INFRA_PERI_SLAVE, 124, "PERISYS_MSDC1", true},
	{E_DAPC_INFRA_PERI_SLAVE, 125, "PERISYS_MSDC2", true},
	{E_DAPC_INFRA_PERI_SLAVE, 126, "PERISYS_MSDC3", true},
	{E_DAPC_INFRA_PERI_SLAVE, 127, "PERISYS_UFS", true},
	{E_DAPC_INFRA_PERI_SLAVE, 128, "PERISUS_USB3.0_SIF", true},
	{E_DAPC_INFRA_PERI_SLAVE, 129, "PERISUS_USB3.0_SIF2", true},

	/* 130 */
	{E_DAPC_INFRA_PERI_SLAVE, 130, "PERISYS_USB_2.0_SIF(**)", true},
	{E_DAPC_INFRA_PERI_SLAVE, 131, "PERISYS_AUDIO", true},
	{E_DAPC_INFRA_PERI_SLAVE, 132, "EAST_RESERVE", true},
	{E_DAPC_INFRA_PERI_SLAVE, 133, "EAST_ CSI_TOP_AO", true},
	{E_DAPC_INFRA_PERI_SLAVE, 134, "EAST_ RESERVE", true},
	{E_DAPC_INFRA_PERI_SLAVE, 135, "EAST_ RESERVE", true},
	{E_DAPC_INFRA_PERI_SLAVE, 136, "SOUTH_RESERVE", true},
	{E_DAPC_INFRA_PERI_SLAVE, 137, "SOUTH_RESERVE", true},
	{E_DAPC_INFRA_PERI_SLAVE, 138, "SOUTH_RESERVE", true},
	{E_DAPC_INFRA_PERI_SLAVE, 139, "SOUTH_RESERVE", true},

	/* 140 */
	{E_DAPC_INFRA_PERI_SLAVE, 140, "WEST_MIPI_TX_CONFIG", true},
	{E_DAPC_INFRA_PERI_SLAVE, 141, "WEST_MSDC1", true},
	{E_DAPC_INFRA_PERI_SLAVE, 142, "WEST_USB20_PHY", true},
	{E_DAPC_INFRA_PERI_SLAVE, 143, "WEST_EFUSE", true},
	{E_DAPC_INFRA_PERI_SLAVE, 144, "NORTH_UFS_MPHY", true},
	{E_DAPC_INFRA_PERI_SLAVE, 145, "NORTH_MSDC0", true},
	{E_DAPC_INFRA_PERI_SLAVE, 146, "NORTH_RESERVE_0", true},
	{E_DAPC_INFRA_PERI_SLAVE, 147, "NORTH_RESERVE_1", true},
	{E_DAPC_INFRA_PERI_SLAVE, 148, "PERISYS_CONN", true},
	{E_DAPC_INFRA_PERI_SLAVE, 149, "PERISYS_MD1", true},

	/* 150 */
	{E_DAPC_INFRA_PERI_SLAVE, 150, "PERISYS_AUDIODSP", true},
	{E_DAPC_MM_SLAVE, 0, "IP", true},
	{E_DAPC_MM_SLAVE, 1, "Reserved", false},
	{E_DAPC_MM_SLAVE, 2, "DFD", true},
	{E_DAPC_MM_SLAVE, 3, "Reserved", false},
	{E_DAPC_MM_SLAVE, 4, "G3D Secure Reg", true},
	{E_DAPC_MM_SLAVE, 5, "G3D TestBench", true},
	{E_DAPC_MM_SLAVE, 6, "G3D_CONFIG", true},
	{E_DAPC_MM_SLAVE, 7, "Reserved", false},
	{E_DAPC_MM_SLAVE, 8, "MMSYS_CONFIG", true},

	/* 160 */
	{E_DAPC_MM_SLAVE, 9, "DISP_MUTEX0", true},
	{E_DAPC_MM_SLAVE, 10, "SMI_COMMON", true},
	{E_DAPC_MM_SLAVE, 11, "SMI_LARB0", true},
	{E_DAPC_MM_SLAVE, 12, "SMI_LARB1", true},
	{E_DAPC_MM_SLAVE, 13, "DISP_OVL0", true},
	{E_DAPC_MM_SLAVE, 14, "DISP_OVL0_2L", true},
	{E_DAPC_MM_SLAVE, 15, "DISP_RDMA0", true},
	{E_DAPC_MM_SLAVE, 16, "DISP_RSZ0", true},
	{E_DAPC_MM_SLAVE, 17, "DISP_COLOR0", true},
	{E_DAPC_MM_SLAVE, 18, "Reserved", false},

	/* 170 */
	{E_DAPC_MM_SLAVE, 19, "DISP_CCORR0", true},
	{E_DAPC_MM_SLAVE, 20, "DISP_AAL0", true},
	{E_DAPC_MM_SLAVE, 21, "DISP_GAMMA0", true},
	{E_DAPC_MM_SLAVE, 22, "DISP_POSTMASK0", true},
	{E_DAPC_MM_SLAVE, 23, "DISP_DITHER0", true},
	{E_DAPC_MM_SLAVE, 24, "Reserved", false},
	{E_DAPC_MM_SLAVE, 25, "Reserved", false},
	{E_DAPC_MM_SLAVE, 26, "DISP_DSC_WRAP0", true},
	{E_DAPC_MM_SLAVE, 27, "DSI0", true},
	{E_DAPC_MM_SLAVE, 28, "DISP_WDMA0", true},

	/* 180 */
	{E_DAPC_MM_SLAVE, 29, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 30, "MM_IOMMU_0", true},
	{E_DAPC_MM_SLAVE, 31, "MM_IOMMU_1", true},
	{E_DAPC_MM_SLAVE, 32, "MM_IOMMU_2", true},
	{E_DAPC_MM_SLAVE, 33, "MM_IOMMU_3", true},
	{E_DAPC_MM_SLAVE, 34, "MM_IOMMU_4", true},
	{E_DAPC_MM_SLAVE, 35, "DISP_SMI_2X1_SUB_COMMON_U0", true},
	{E_DAPC_MM_SLAVE, 36, "DISP_SMI_2X1_SUB_COMMON_U1", true},
	{E_DAPC_MM_SLAVE, 37, "Reserved", false},
	{E_DAPC_MM_SLAVE, 38, "IMG1_SMI_2X1_SUB_COMMON", true},

	/* 190 */
	{E_DAPC_MM_SLAVE, 39, "Reserved", false},
	{E_DAPC_MM_SLAVE, 40, "Reserved", false},
	{E_DAPC_MM_SLAVE, 41, "reserved  (mfb_a)", false},
	{E_DAPC_MM_SLAVE, 42, "reserved  (wpe_a)", false},
	{E_DAPC_MM_SLAVE, 43, "reserved  (mss_a)", false},
	{E_DAPC_MM_SLAVE, 44, "reserved", false},
	{E_DAPC_MM_SLAVE, 45, "reserved", false},
	{E_DAPC_MM_SLAVE, 46, "reserved", false},
	{E_DAPC_MM_SLAVE, 47, "reserved", false},
	{E_DAPC_MM_SLAVE, 48, "reserved", false},

	/* 200 */
	{E_DAPC_MM_SLAVE, 49, "reserved", false},
	{E_DAPC_MM_SLAVE, 50, "reserved", false},
	{E_DAPC_MM_SLAVE, 51, "reserved", false},
	{E_DAPC_MM_SLAVE, 52, "reserved", false},
	{E_DAPC_MM_SLAVE, 53, "reserved", false},
	{E_DAPC_MM_SLAVE, 54, "reserved", false},
	{E_DAPC_MM_SLAVE, 55, "reserved", false},
	{E_DAPC_MM_SLAVE, 56, "reserved", false},
	{E_DAPC_MM_SLAVE, 57, "imgsys1_top", true},
	{E_DAPC_MM_SLAVE, 58, "dip_a0", true},

	/* 210 */
	{E_DAPC_MM_SLAVE, 59, "dip_a1", true},
	{E_DAPC_MM_SLAVE, 60, "dip_a2", true},
	{E_DAPC_MM_SLAVE, 61, "dip_a3", true},
	{E_DAPC_MM_SLAVE, 62, "dip_a4", true},
	{E_DAPC_MM_SLAVE, 63, "dip_a5", true},
	{E_DAPC_MM_SLAVE, 64, "dip_a6", true},
	{E_DAPC_MM_SLAVE, 65, "dip_a7", true},
	{E_DAPC_MM_SLAVE, 66, "reserved  (dip_a8)", false},
	{E_DAPC_MM_SLAVE, 67, "reserved  (dip_a9)", false},
	{E_DAPC_MM_SLAVE, 68, "dip_a10", true},

	/* 220 */
	{E_DAPC_MM_SLAVE, 69, "dip_a11", true},
	{E_DAPC_MM_SLAVE, 70, "reserved", false},
	{E_DAPC_MM_SLAVE, 71, "smi_larb9", true},
	{E_DAPC_MM_SLAVE, 72, "2x1_sub_common", true},
	{E_DAPC_MM_SLAVE, 73, "reserved", false},
	{E_DAPC_MM_SLAVE, 74, "mfb_b", true},
	{E_DAPC_MM_SLAVE, 75, "wpe_b", true},
	{E_DAPC_MM_SLAVE, 76, "mss_b", true},
	{E_DAPC_MM_SLAVE, 77, "reserved", false},
	{E_DAPC_MM_SLAVE, 78, "reserved", false},

	/* 230 */
	{E_DAPC_MM_SLAVE, 79, "reserved", false},
	{E_DAPC_MM_SLAVE, 80, "reserved", false},
	{E_DAPC_MM_SLAVE, 81, "reserved", false},
	{E_DAPC_MM_SLAVE, 82, "reserved", false},
	{E_DAPC_MM_SLAVE, 83, "reserved", false},
	{E_DAPC_MM_SLAVE, 84, "reserved", false},
	{E_DAPC_MM_SLAVE, 85, "reserved", false},
	{E_DAPC_MM_SLAVE, 86, "reserved", false},
	{E_DAPC_MM_SLAVE, 87, "reserved", false},
	{E_DAPC_MM_SLAVE, 88, "reserved", false},

	/* 240 */
	{E_DAPC_MM_SLAVE, 89, "imgsys2_top", true},
	{E_DAPC_MM_SLAVE, 90, "reserved  (dip_b0)", false},
	{E_DAPC_MM_SLAVE, 91, "reserved  (dip_a8)", false},
	{E_DAPC_MM_SLAVE, 92, "reserved  (dip_b1)", false},
	{E_DAPC_MM_SLAVE, 93, "reserved  (dip_b2)", false},
	{E_DAPC_MM_SLAVE, 94, "reserved  (dip_b3)", false},
	{E_DAPC_MM_SLAVE, 95, "reserved  (dip_b4)", false},
	{E_DAPC_MM_SLAVE, 96, "reserved  (dip_b5)", false},
	{E_DAPC_MM_SLAVE, 97, "reserved  (dip_b6)", false},
	{E_DAPC_MM_SLAVE, 98, "reserved  (dip_b7)", false},

	/* 250 */
	{E_DAPC_MM_SLAVE, 99, "reserved  (dip_b8)", false},
	{E_DAPC_MM_SLAVE, 100, "reserved  (dip_b9)", false},
	{E_DAPC_MM_SLAVE, 101, "reserved  (dip_b10)", false},
	{E_DAPC_MM_SLAVE, 102, "reserved  (dip_b11)", false},
	{E_DAPC_MM_SLAVE, 103, "reserved", true},
	{E_DAPC_MM_SLAVE, 104, "smi_larb11", true},
	{E_DAPC_MM_SLAVE, 105, "reserved  (smi_larb12)", false},
	{E_DAPC_MM_SLAVE, 106, "rserved", true},
	{E_DAPC_MM_SLAVE, 107, "vdec_core0", true},
	{E_DAPC_MM_SLAVE, 108, "vdec_core0", true},

	/* 260 */
	{E_DAPC_MM_SLAVE, 109, "vdec_core0", true},
	{E_DAPC_MM_SLAVE, 110, "vdec_core0", true},
	{E_DAPC_MM_SLAVE, 111, "vdec_core0", true},
	{E_DAPC_MM_SLAVE, 112, "vdec_core0", true},
	{E_DAPC_MM_SLAVE, 113, "vdec_core0", true},
	{E_DAPC_MM_SLAVE, 114, "vdec_core0", true},
	{E_DAPC_MM_SLAVE, 115, "vdec_core0", true},
	{E_DAPC_MM_SLAVE, 116, "vdec_core0", true},
	{E_DAPC_MM_SLAVE, 117, "vdec_core0", true},
	{E_DAPC_MM_SLAVE, 118, "vdec_core0", true},

	/* 270 */
	{E_DAPC_MM_SLAVE, 119, "vdec_core0", true},
	{E_DAPC_MM_SLAVE, 120, "vdec_core0", true},
	{E_DAPC_MM_SLAVE, 121, "vdec_core0_larb", true},
	{E_DAPC_MM_SLAVE, 122, "vdec_core0_gcon", true},
	{E_DAPC_MM_SLAVE, 123, "vdec_mini_mdp_top", true},
	{E_DAPC_MM_SLAVE, 124, "reserved", false},
	{E_DAPC_MM_SLAVE, 125, "venc_global_con", true},
	{E_DAPC_MM_SLAVE, 126, "smi_larb7", true},
	{E_DAPC_MM_SLAVE, 127, "venc", true},
	{E_DAPC_MM_SLAVE, 128, "jpgenc", true},

	/* 280 */
	{E_DAPC_MM_SLAVE, 129, "reserved", false},
	{E_DAPC_MM_SLAVE, 130, "reserved", false},
	{E_DAPC_MM_SLAVE, 131, "venc_mbist_ctrl", true},
	{E_DAPC_MM_SLAVE, 132, "reserved", false},
	{E_DAPC_MM_SLAVE, 133, "camsys top", true},
	{E_DAPC_MM_SLAVE, 134, "smi_larb13", true},
	{E_DAPC_MM_SLAVE, 135, "smi_larb14", true},
	{E_DAPC_MM_SLAVE, 136, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 137, "seninf_a", true},
	{E_DAPC_MM_SLAVE, 138, "seninf_b", true},

	/* 290 */
	{E_DAPC_MM_SLAVE, 139, "seninf_c", true},
	{E_DAPC_MM_SLAVE, 140, "seninf_d", true},
	{E_DAPC_MM_SLAVE, 141, "seninf_e", true},
	{E_DAPC_MM_SLAVE, 142, "seninf_f", true},
	{E_DAPC_MM_SLAVE, 143, "seninf_g", true},
	{E_DAPC_MM_SLAVE, 144, "seninf_h", true},
	{E_DAPC_MM_SLAVE, 145, "cam_smi_3x1_sub_common_u0", true},
	{E_DAPC_MM_SLAVE, 146, "cam_smi_4x1_sub_common_u0", true},
	{E_DAPC_MM_SLAVE, 147, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 148, "smi_larb_16", true},

	/* 300 */
	{E_DAPC_MM_SLAVE, 149, "smi_larb_17", true},
	{E_DAPC_MM_SLAVE, 150, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 151, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 152, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 153, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 154, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 155, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 156, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 157, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 158, "RESERVED", false},

	/* 310 */
	{E_DAPC_MM_SLAVE, 159, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 160, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 161, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 162, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 163, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 164, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 165, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 166, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 167, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 168, "RESERVED", false},

	/* 320 */
	{E_DAPC_MM_SLAVE, 169, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 170, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 171, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 172, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 173, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 174, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 175, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 176, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 177, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 178, "RESERVED", false},

	/* 330 */
	{E_DAPC_MM_SLAVE, 179, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 180, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 181, "cam_raw_a_ip_group_0", true},
	{E_DAPC_MM_SLAVE, 182, "cam_raw_a_ip_group_1", true},
	{E_DAPC_MM_SLAVE, 183, "cam_raw_a_ip_group_2", true},
	{E_DAPC_MM_SLAVE, 184, "cam_raw_a_ip_group_3", true},
	{E_DAPC_MM_SLAVE, 185, "cam_raw_a_dma_0", true},
	{E_DAPC_MM_SLAVE, 186, "cam_raw_a_dma_1", true},
	{E_DAPC_MM_SLAVE, 187, "ltm_curve_a_0", true},
	{E_DAPC_MM_SLAVE, 188, "ltm_curve_a_1", true},

	/* 340 */
	{E_DAPC_MM_SLAVE, 189, "cam_raw_a_ip_group_0_inner", true},
	{E_DAPC_MM_SLAVE, 190, "cam_raw_a_ip_group_1_inner", true},
	{E_DAPC_MM_SLAVE, 191, "cam_raw_a_ip_group_2_inner", true},
	{E_DAPC_MM_SLAVE, 192, "cam_raw_a_ip_group_3_inner", true},
	{E_DAPC_MM_SLAVE, 193, "cam_raw_a_dma_0_inner", true},
	{E_DAPC_MM_SLAVE, 194, "cam_raw_a_dma_1_inner", true},
	{E_DAPC_MM_SLAVE, 195, "ltm_curve_a_0_inner", true},
	{E_DAPC_MM_SLAVE, 196, "ltm_curve_a_1_inner", true},
	{E_DAPC_MM_SLAVE, 197, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 198, "RESERVED", false},

	/* 350 */
	{E_DAPC_MM_SLAVE, 199, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 200, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 201, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 202, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 203, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 204, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 205, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 206, "cam_raw_a_set", true},
	{E_DAPC_MM_SLAVE, 207, "cam_raw_a_clr", true},
	{E_DAPC_MM_SLAVE, 208, "cam_raw_a_set_inner", true},

	/* 360 */
	{E_DAPC_MM_SLAVE, 209, "cam_raw_a_clr_inner", true},
	{E_DAPC_MM_SLAVE, 210, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 211, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 212, "camsys_a_config", true},
	{E_DAPC_MM_SLAVE, 213, "cam_raw_b_ip_group_0", true},
	{E_DAPC_MM_SLAVE, 214, "cam_raw_b_ip_group_1", true},
	{E_DAPC_MM_SLAVE, 215, "cam_raw_b_ip_group_2", true},
	{E_DAPC_MM_SLAVE, 216, "cam_raw_b_ip_group_3", true},
	{E_DAPC_MM_SLAVE, 217, "cam_raw_b_dma_0", true},
	{E_DAPC_MM_SLAVE, 218, "cam_raw_b_dma_1", true},

	/* 370 */
	{E_DAPC_MM_SLAVE, 219, "ltm_curve_b_0", true},
	{E_DAPC_MM_SLAVE, 220, "ltm_curve_b_1", true},
	{E_DAPC_MM_SLAVE, 221, "cam_raw_b_ip_group_0_inner", true},
	{E_DAPC_MM_SLAVE, 222, "cam_raw_b_ip_group_1_inner", true},
	{E_DAPC_MM_SLAVE, 223, "cam_raw_b_ip_group_2_inner", true},
	{E_DAPC_MM_SLAVE, 224, "cam_raw_b_ip_group_3_inner", true},
	{E_DAPC_MM_SLAVE, 225, "cam_raw_b_dma_0_inner", true},
	{E_DAPC_MM_SLAVE, 226, "cam_raw_b_dma_1_inner", true},
	{E_DAPC_MM_SLAVE, 227, "ltm_curve_b_0_inner", true},
	{E_DAPC_MM_SLAVE, 228, "ltm_curve_b_1_inner", true},

	/* 380 */
	{E_DAPC_MM_SLAVE, 229, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 230, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 231, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 232, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 233, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 234, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 235, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 236, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 237, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 238, "cam_raw_b_set", true},

	/* 390 */
	{E_DAPC_MM_SLAVE, 239, "cam_raw_b_clr", true},
	{E_DAPC_MM_SLAVE, 240, "cam_raw_b_set_inner", true},
	{E_DAPC_MM_SLAVE, 241, "cam_raw_b_clr_inner", true},
	{E_DAPC_MM_SLAVE, 242, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 243, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 244, "camsys_b_config", true},
	{E_DAPC_MM_SLAVE, 245, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 246, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 247, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 248, "RESERVED", false},

	/* 400 */
	{E_DAPC_MM_SLAVE, 249, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 250, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 251, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 252, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 253, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 254, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 255, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 256, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 257, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 258, "RESERVED", false},

	/* 410 */
	{E_DAPC_MM_SLAVE, 259, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 260, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 261, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 262, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 263, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 264, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 265, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 266, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 267, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 268, "RESERVED", false},

	/* 420 */
	{E_DAPC_MM_SLAVE, 269, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 270, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 271, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 272, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 273, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 274, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 275, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 276, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 277, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 278, "RESERVED", false},

	/* 430 */
	{E_DAPC_MM_SLAVE, 279, "camsv_2", true},
	{E_DAPC_MM_SLAVE, 280, "camsv_3", true},
	{E_DAPC_MM_SLAVE, 281, "camsv_4", true},
	{E_DAPC_MM_SLAVE, 282, "camsv_5", true},
	{E_DAPC_MM_SLAVE, 283, "camsv_6", true},
	{E_DAPC_MM_SLAVE, 284, "camsv_7", true},
	{E_DAPC_MM_SLAVE, 285, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 286, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 287, "camsv_2_inner", true},
	{E_DAPC_MM_SLAVE, 288, "camsv_3_inner", true},

	/* 440 */
	{E_DAPC_MM_SLAVE, 289, "camsv_4_inner", true},
	{E_DAPC_MM_SLAVE, 290, "camsv_5_inner", true},
	{E_DAPC_MM_SLAVE, 291, "camsv_6_inner", true},
	{E_DAPC_MM_SLAVE, 292, "camsv_7_inner", true},
	{E_DAPC_MM_SLAVE, 293, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 294, "asg", true},
	{E_DAPC_MM_SLAVE, 295, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 296, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 297, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 298, "RESERVED", false},

	/* 450 */
	{E_DAPC_MM_SLAVE, 299, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 300, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 301, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 302, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 303, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 304, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 305, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 306, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 307, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 308, "RESERVED", false},

	/* 460 */
	{E_DAPC_MM_SLAVE, 309, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 310, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 311, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 312, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 313, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 314, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 315, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 316, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 317, "RESERVED", false},
	{E_DAPC_MM_SLAVE, 318, "RESERVED", false},

	/* 470 */
	{E_DAPC_MM_SLAVE, 319, "mdpsys_config", true},
	{E_DAPC_MM_SLAVE, 320, "mdp_mutex0", true},
	{E_DAPC_MM_SLAVE, 321, "smi_larb0", true},
	{E_DAPC_MM_SLAVE, 322, "mdp_rdma0", true},
	{E_DAPC_MM_SLAVE, 323, "Reserved", false},
	{E_DAPC_MM_SLAVE, 324, "mdp_aal0", true},
	{E_DAPC_MM_SLAVE, 325, "Reserved", false},
	{E_DAPC_MM_SLAVE, 326, "mdp_hdr0", true},
	{E_DAPC_MM_SLAVE, 327, "mdp_rsz0", true},
	{E_DAPC_MM_SLAVE, 328, "mdp_rsz1", true},

	/* 480 */
	{E_DAPC_MM_SLAVE, 329, "mdp_wrot0", true},
	{E_DAPC_MM_SLAVE, 330, "mdp_wrot1", true},
	{E_DAPC_MM_SLAVE, 331, "mdp_tdshp0", true},
	{E_DAPC_MM_SLAVE, 332, "Reserved", false},
	{E_DAPC_MM_SLAVE, 333, "Reserved", false},
	{E_DAPC_MM_SLAVE, 334, "Reserved", false},
	{E_DAPC_MM_SLAVE, 335, "ipesys_top", true},
	{E_DAPC_MM_SLAVE, 336, "fdvt", true},
	{E_DAPC_MM_SLAVE, 337, "Reserved (fe)", false},
	{E_DAPC_MM_SLAVE, 338, "rsc", true},

	/* 490 */
	{E_DAPC_MM_SLAVE, 339, "reserved", false},
	{E_DAPC_MM_SLAVE, 340, "reserved", false},
	{E_DAPC_MM_SLAVE, 341, "reserved", false},
	{E_DAPC_MM_SLAVE, 342, "reserved", false},
	{E_DAPC_MM_SLAVE, 343, "reserved", false},
	{E_DAPC_MM_SLAVE, 344, "reserved", false},
	{E_DAPC_MM_SLAVE, 345, "reserved", false},
	{E_DAPC_MM_SLAVE, 346, "reserved", false},
	{E_DAPC_MM_SLAVE, 347, "reserved", false},
	{E_DAPC_MM_SLAVE, 348, "reserved", false},

	/* 500 */
	{E_DAPC_MM_SLAVE, 349, "ipe_smi_2x1_sub_common", true},
	{E_DAPC_MM_SLAVE, 350, "smi_larb20", true},
	{E_DAPC_MM_SLAVE, 351, "depth", true},
	{E_DAPC_MM_SLAVE, 352, "reserved", false},
	{E_DAPC_MM_SLAVE, 353, "reserved", false},
	{E_DAPC_MM_SLAVE, 354, "reserved", false},
	{E_DAPC_MM_SLAVE, 355, "reserved", false},
	{E_DAPC_MM_SLAVE, 356, "reserved", false},
	{E_DAPC_MM_SLAVE, 357, "reserved", false},
	{E_DAPC_MM_SLAVE, 358, "reserved", false},

	/* 510 */
	{E_DAPC_MM_SLAVE, 359, "reserved", false},
	{E_DAPC_MM_SLAVE, 360, "reserved", false},
	{E_DAPC_MM_SLAVE, 361, "reserved", false},
	{E_DAPC_MM_SLAVE, 362, "reserved", false},
	{E_DAPC_MM_SLAVE, 363, "reserved", false},
	{E_DAPC_MM_SLAVE, 364, "reserved", false},
	{E_DAPC_MM_SLAVE, 365, "reserved", false},
	{E_DAPC_MM_SLAVE, 366, "smi_larb19", true},
	{E_DAPC_MM_SLAVE, 367, "reserved", false},
	{E_DAPC_OTHERS_SLAVE, -1, "SSPM_SBUS2APB_BRIDGE_UNALIGN", true},

	/* 520 */
	{E_DAPC_OTHERS_SLAVE, -1, "SSPM_SBUS2APB_BRIDGE_OUT_OF_BOUND", true},
	{E_DAPC_OTHERS_SLAVE, -1, "SSPM_SBUS2APB_BRIDGE_WAY_EN", true},
	{E_DAPC_OTHERS_SLAVE, -1, "PERIAPB_BRIDGE_UNALIGN", true},
	{E_DAPC_OTHERS_SLAVE, -1, "PERIAPB_BRIDGE_OUT_OF_BOUND", true},
	{E_DAPC_OTHERS_SLAVE, -1, "PERIAPB_BRIDGE_WAY_EN", true},
	{E_DAPC_OTHERS_SLAVE, -1, "paxi_secure_md_vio_decerr", true},
	{E_DAPC_OTHERS_SLAVE, -1, "paxi_secure_hifi3_vio_decerr", true},
	{E_DAPC_OTHERS_SLAVE, -1, "EAST_PERIAPB_ERR_WAY_EN", false},
	{E_DAPC_OTHERS_SLAVE, -1, "SOUTH_PERIAPB_UNALIGN", false},
	{E_DAPC_OTHERS_SLAVE, -1, "SOUTH_PERIAPB_OUT_OF_BOUND", false},

	/* 530 */
	{E_DAPC_OTHERS_SLAVE, -1, "SOUTH_PERIAPB_ERR_WAY_EN", false},
	{E_DAPC_OTHERS_SLAVE, -1, "WEST_PERIAPB_UNALIGN", false},
	{E_DAPC_OTHERS_SLAVE, -1, "WEST_PERIAPB_OUT_OF_BOUND", false},
	{E_DAPC_OTHERS_SLAVE, -1, "WEST_PERIAPB_ERR_WAY_EN", false},
	{E_DAPC_OTHERS_SLAVE, -1, "NORTH_SBUS2APB_BRIDGE_UNALIGN", true},
	{E_DAPC_OTHERS_SLAVE, -1, "NORTH_SBUS2APB_BRIDGE_OUT_OF_BOUND", true},
	{E_DAPC_OTHERS_SLAVE, -1, "NORTH_SBUS2APB_BRIDGE_ERR_WAY_EN", true},
	{E_DAPC_OTHERS_SLAVE, -1, "INFRA_PDN_SBUS2APB_BRIDGE_DECODE_ERROR", true},
	{E_DAPC_OTHERS_SLAVE, -1, "TOPAXI_SI2_DECERR", true},
	{E_DAPC_OTHERS_SLAVE, -1, "TOPAXI_SI1_DECERR", true},

	/* 540 */
	{E_DAPC_OTHERS_SLAVE, -1, "TOPAXI_SI0_DECERR", true},
	{E_DAPC_OTHERS_SLAVE, -1, "PERIAXI_SI0_DECERR", true},
	{E_DAPC_OTHERS_SLAVE, -1, "PERIAXI_SI1_DECERR", false},
	{E_DAPC_OTHERS_SLAVE, -1, "TOPAXI_SI3_DECERR", true},
	{E_DAPC_OTHERS_SLAVE, -1, "TOPAXI_SI4_DECERR", true},
	{E_DAPC_OTHERS_SLAVE, -1, "SRAMROM_SI0_DECERR", true},
	{E_DAPC_OTHERS_SLAVE, -1, "SRAMROM*", true},

};

static struct PERIAXI_ID_INFO paxi_int_mi_id_to_master[] = {
	{"N/A",                    { 0, 0, 0, 0 } },
	{"SPM",                    { 0, 0, 1, 0 } },
	{"N/A",                    { 0, 0, 0, 1 } },
	{"PTP_THERM_CTRL",         { 0, 0, 1, 1 } },
	{"APDMA_INT",              { 1, 0, 2, 0 } },
	{"MD2AP",                  { 1, 1, 2, 2 } },
};

static struct TOPAXI_ID_INFO topaxi_mi0_id_to_master[] = {
	{"APMCU_Write",       { 0, 0, 2, 2,	2, 2, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"APMCU_Write",       { 0, 0, 2, 2,	2, 2, 0, 0,	1, 0, 0, 0,	0, 0 } },
	{"APMCU_Write",       { 0, 0, 2, 2,	2, 2, 2, 2,	2, 2, 1, 0,	0, 0 } },
	{"APMCU_Read",        { 0, 0, 2, 2,	2, 2, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"APMCU_Read",        { 0, 0, 2, 2,	2, 2, 0, 0,	1, 0, 0, 0,	0, 0 } },
	{"APMCU_Read",        { 0, 0, 2, 0,	0, 0, 0, 0,	0, 1, 0, 0,	0, 0 } },
	{"APMCU_Read",        { 0, 0, 2, 1,	0, 0, 0, 0,	0, 1, 0, 0,	0, 0 } },
	{"APMCU_Read",        { 0, 0, 2, 2,	2, 2, 2, 2,	2, 2, 1, 0,	0, 0 } },
	{"APMCU_Read",        { 0, 0, 2, 2,	2, 2, 2, 2,	2, 2, 2, 1,	0, 0 } },
	{"DEBUGSYS",          { 1, 0, 0, 0,	0, 2, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"MSDC0",             { 1, 0, 1, 0,	0, 0, 0, 0,	2, 2, 0, 0,	0, 0 } },
	{"PWM",               { 1, 0, 1, 0,	0, 1, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"MSDC1",             { 1, 0, 1, 0,	0, 1, 0, 0,	1, 0, 0, 0,	0, 0 } },
	{"AUDIO",             { 1, 0, 1, 0,	0, 1, 0, 0,	0, 1, 0, 0,	0, 0 } },
	{"SPI0",              { 1, 0, 1, 0,	0, 1, 0, 0,	1, 1, 0, 0,	0, 0 } },
	{"N/A",               { 1, 0, 1, 0,	0, 0, 1, 0,	1, 0, 0, 0,	0, 0 } },
	{"SPI2",              { 1, 0, 1, 0,	0, 0, 1, 0,	0, 1, 0, 0,	0, 0 } },
	{"SPI1",              { 1, 0, 1, 0,	0, 0, 1, 0,	1, 1, 0, 0,	0, 0 } },
	{"USB",               { 1, 0, 1, 0,	0, 0, 1, 0,	0, 0, 0, 0,	0, 0 } },
	{"N/A",               { 1, 0, 1, 0,	0, 1, 1, 0,	0, 0, 0, 0,	0, 0 } },
	{"SPM",               { 1, 0, 1, 0,	0, 1, 1, 0,	1, 0, 0, 0,	0, 0 } },
	{"N/A",               { 1, 0, 1, 0,	0, 1, 1, 0,	0, 1, 0, 0,	0, 0 } },
	{"PTP_THERM_CTRL",    { 1, 0, 1, 0,	0, 1, 1, 0,	1, 1, 0, 0,	0, 0 } },
	{"APDMA_EXT",         { 1, 0, 1, 0,	0, 0, 0, 1,	2, 0, 0, 0,	0, 0 } },
	{"N/A",               { 1, 0, 1, 0,	0, 1, 0, 1,	0, 0, 0, 0,	0, 0 } },
	{"SPI3",              { 1, 0, 1, 0,	0, 1, 0, 1,	1, 0, 0, 0,	0, 0 } },
	{"SPI4",              { 1, 0, 1, 0,	0, 1, 0, 1,	0, 1, 0, 0,	0, 0 } },
	{"SPI5",              { 1, 0, 1, 0,	0, 1, 0, 1,	1, 1, 0, 0,	0, 0 } },
	{"UFS",               { 1, 0, 1, 0,	0, 0, 1, 1,	2, 2, 0, 0,	0, 0 } },
	{"AudioDSP",          { 1, 0, 0, 1,	0, 0, 2, 2,	2, 2, 2, 0,	0, 0 } },
	{"CONNSYS",           { 1, 0, 0, 1,	0, 1, 2, 2,	2, 0, 0, 0,	0, 0 } },
	{"DXCC",              { 1, 0, 1, 1,	0, 2, 2, 2,	2, 0, 0, 0,	0, 0 } },
	{"CQDMA",             { 1, 0, 0, 0,	1, 2, 2, 2,	0, 0, 0, 0,	0, 0 } },
	{"DPMAIF",            { 1, 0, 1, 0,	1, 2, 2, 2,	2, 0, 0, 0,	0, 0 } },
	{"GCE",               { 1, 0, 0, 1,	1, 2, 2, 0,	0, 0, 0, 0,	0, 0 } },
	{"SCP/SSPM",          { 1, 0, 1, 1,	1, 2, 2, 2,	0, 0, 0, 0,	0, 0 } },
	{"SCP/SSPM",          { 0, 1, 2, 2,	2, 0, 0, 0,	0, 0, 0, 0,	0, 0 } },
};

static const char *topaxi_mi0_trans(int bus_id)
{
	const char *master = "UNKNOWN_MASTER_FROM_TOPAXI";
	int master_count = ARRAY_SIZE(topaxi_mi0_id_to_master);
	int i, j;

	DEVAPC_DBG_MSG("%s %s:0x%x\n",
		__func__, "bus id", bus_id);

	for (i = 0; i < master_count; i++) {
		for (j = 0; j < TOPAXI_MI0_BIT_LENGTH ; j++) {
			if (topaxi_mi0_id_to_master[i].bit[j] == 2)
				continue;

			if (((bus_id >> j) & 0x1) ==
				topaxi_mi0_id_to_master[i].bit[j]) {
				continue;
			} else {
				break;
			}
		}
		if (j == TOPAXI_MI0_BIT_LENGTH) {
			DEVAPC_MSG("%s %s %s\n",
				"catch it from TOPAXI_MI0.",
				"Master is:",
				topaxi_mi0_id_to_master[i].master);
			master = topaxi_mi0_id_to_master[i].master;
		}
	}

	return master;
}

static const char *paxi_int_mi_trans(int bus_id)
{
	const char *master = "UNKNOWN_MASTER_FROM_PAXI";
	int master_count = ARRAY_SIZE(paxi_int_mi_id_to_master);
	int i, j;

	DEVAPC_DBG_MSG("%s %s:0x%x\n",
		__func__, "bus id", bus_id);

	if ((bus_id & 0x3) == 0x2) {
		master = topaxi_mi0_trans(bus_id >> 2);
		return master;
	}

	for (i = 0; i < master_count; i++) {
		for (j = 0; j < PERIAXI_INT_MI_BIT_LENGTH ; j++) {
			if (paxi_int_mi_id_to_master[i].bit[j] == 2)
				continue;

			if (((bus_id >> j) & 0x1) ==
				paxi_int_mi_id_to_master[i].bit[j]) {
				continue;
			} else {
				break;
			}
		}
		if (j == PERIAXI_INT_MI_BIT_LENGTH) {
			DEVAPC_MSG("%s %s %s\n",
				"catch it from PERIAXI_INT_MI.",
				"Master is:",
				paxi_int_mi_id_to_master[i].master);
			master = paxi_int_mi_id_to_master[i].master;
		}
	}

	return master;
}

const char *bus_id_to_master(int bus_id, uint32_t vio_addr, int vio_idx)
{
	uint32_t h_byte;
	const char *master = "UNKNOWN_MASTER";

	DEVAPC_DBG_MSG("bus id = 0x%x, vio_addr = 0x%x\n",
		bus_id, vio_addr);

	/* SPM MTCMOS disable will set way_en[7:4] reg to block transaction,
	 * and it will triggered TOPAXI_SI0_DECERR instead of slave vio.
	 */
	if (vio_idx == TOPAXI_SI0_DECERR) {
		DEVAPC_DBG_MSG("vio is from TOPAXI_SI0_DECERR\n");
		master = topaxi_mi0_trans(bus_id);
		return master;

	} else if (vio_idx == PERIAXI_SI1_DECERR) {
		DEVAPC_DBG_MSG("vio is from PERIAXI_SI1_DECERR\n");
		master = paxi_int_mi_trans(bus_id);
		return master;

	} else if (vio_idx == SRAMROM_VIO_INDEX) {
		DEVAPC_DBG_MSG("vio is from SRAMROM\n");
		if ((bus_id & 0x1) == 0x0)
			master = topaxi_mi0_trans(bus_id >> 1);
		else
			DEVAPC_MSG("[FAILED] Cannot decode bus_id: 0x%x\n",
				bus_id);

		return master;

	}

	h_byte = (vio_addr >> 24) & 0xFF;

	/* to Infra/Peri/Audio/MD/CONN
	 * or MMSYS
	 * or MFG
	 */
	if (((h_byte >> 4) == 0x0) && h_byte != 0x0C && h_byte != 0x0D &&
		h_byte != 0x0E) {
		DEVAPC_DBG_MSG("vio addr is from on-chip SRAMROM\n");
		if ((bus_id & 0x1) == 0x0)
			master = topaxi_mi0_trans(bus_id >> 1);
		else
			DEVAPC_MSG("decode failed for sram_s\n");

	} else if (h_byte == 0x10 || h_byte == 0x11 || h_byte == 0x18 ||
		h_byte == 0x0C || h_byte == 0x0D || h_byte == 0x0E ||
		(h_byte >> 4) == 0x2 || (h_byte >> 4) == 0x3) {
		DEVAPC_DBG_MSG("vio addr is from Infra/Peri\n");

		master = paxi_int_mi_trans(bus_id);

	} else if (h_byte == 0x14 || h_byte == 0x15 || h_byte == 0x16 ||
		h_byte == 0x17 || h_byte == 0x1A || h_byte == 0x1B ||
		h_byte == 0x1C) {
		DEVAPC_DBG_MSG("vio addr is from MM\n");
		if ((bus_id & 0x1) == 1)
			return "GCE";
		master = topaxi_mi0_trans(bus_id >> 1);

	} else if (h_byte == 0x13) {
		DEVAPC_DBG_MSG("vio addr is from MFG\n");
		master = topaxi_mi0_trans(bus_id);

	} else {
		DEVAPC_MSG("[FAILED] Cannot decode vio addr\n");
		master = "UNKNOWN_MASTER";
	}

	return master;
}

/* violation index corresponds to subsys */
const char *index_to_subsys(uint32_t index)
{
	if (index == SMI_COMMON || index == SMI_LARB0 ||
		index == SMI_LARB1 ||
		index == DISP_SMI_2X1_SUB_COMMON_U0 ||
		index == DISP_SMI_2X1_SUB_COMMON_U1 ||
		index == IMG1_SMI_2X1_SUB_COMMON ||
		index == SMI_LARB9 || index == SMI_LARB11 ||
		index == SMI_LARB12 || index == SMI_LARB7 ||
		index == SMI_LARB13 || index == SMI_LARB14 ||
		index == CAM_SIM_3X1_SUB_COMMON_U0 ||
		index == CAM_SIM_4X1_SUB_COMMON_U0 ||
		index == SMI_LARB_16 || index == SMI_LARB_17 ||
		index == SMI_LARB0_S ||
		index == IPE_SMI_2X1_SUB_COMMON ||
		index == SMI_LARB20 ||
		index == SMI_LARB19)
		return "SMI";
	else if (index >= MMSYS_MDP_START && index <= MMSYS_MDP_END)
		return "MMSYS_MDP";
	else if (index >= MMSYS_DISP_START && index <= MMSYS_DISP_END)
		return "MMSYS_DISP";
	else if (index == IMGSYS1_TOP || index == IMGSYS2_TOP)
		return "IMGSYS";
	else if (index == VENC_GLOBAL_CON || index == VENC ||
		index == VENC_MBIST_CTR)
		return "VENCSYS";
	else if (index >= VDECSYS_START && index <= VDECSYS_END)
		return "VDECSYS";
	else if ((index >= CAMSYS_START && index <= CAMSYS_END) ||
		(index >= CAMSYS_P1_START && index <= CAMSYS_P1_END))
		return "CAMSYS";
	else if (index >= CAMSYS_SENINF_START && index <= CAMSYS_SENINF_END)
		return "CAMSYS_SENINF";
	else if (index < ARRAY_SIZE(mt6781_infra_devices))
		return mt6781_infra_devices[index].device;
	else
		return "OUT_OF_BOUND";
}

static uint32_t mt6781_shift_group_get(uint32_t vio_idx)
{
	if (vio_idx >= 0 && vio_idx <= 31)
		return 0;
	else if (vio_idx >= 32 && vio_idx <= 81)
		return 1;
	else if ((vio_idx >= 82 && vio_idx <= 92) ||
		(vio_idx >= 519 && vio_idx <= 521))
		return 2;
	else if (vio_idx == 93)
		return 3;
	else if (vio_idx == 94)
		return 4;
	else if ((vio_idx >= 95 && vio_idx <= 120) ||
		(vio_idx >= 522 && vio_idx <= 524))
		return 5;
	else if (vio_idx >= 121 && vio_idx <= 130)
		return 6;
	else if (vio_idx == 131 || vio_idx == 148)
		return 7;
	else if (vio_idx == 149 || vio_idx == 525)
		return 8;
	else if (vio_idx == 150 || vio_idx == 526)
		return 9;
	else if (vio_idx >= 531 && vio_idx <= 533)
		return 10;
	else if ((vio_idx >= 132 && vio_idx <= 147) ||
		(vio_idx >= 534 && vio_idx <= 536))
		return 11;
	else if (vio_idx >= 151 && vio_idx <= 158)
		return 12;
	else if (vio_idx >= 159 && vio_idx <= 518)
		return 13;
	else if (vio_idx >= 541 && vio_idx <= 542)
		return 14;
	else if (vio_idx == 540)
		return 15;
	else if (vio_idx == 545)
		return 16;
	else if (vio_idx == 537)
		return 17;
	else if (vio_idx == 543)
		return 18;
	else if (vio_idx == 544)
		return 19;
	else if (vio_idx == 539)
		return 20;
	else if (vio_idx == 538)
		return 21;

	DEVAPC_MSG("%s:%d Wrong vio_idx:%d\n",
		__func__, __LINE__, vio_idx);

	return 31;
}

static ssize_t mt6781_devapc_dbg_read(struct file *file, char __user *buffer,
	size_t count, loff_t *ppos)
{
	return mtk_devapc_dbg_read(file, buffer, count, ppos);
}

static ssize_t mt6781_devapc_dbg_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *data)
{
	return mtk_devapc_dbg_write(file, buffer, count, data);
}

static const struct file_operations devapc_dbg_fops = {
	.owner = THIS_MODULE,
	.write = mt6781_devapc_dbg_write,
	.read = mt6781_devapc_dbg_read,
};

static struct mtk_devapc_dbg_status mt6781_devapc_dbg_stat = {
	.enable_ut = PLAT_DBG_UT_DEFAULT,
	.enable_KE = PLAT_DBG_KE_DEFAULT,
	.enable_AEE = PLAT_DBG_AEE_DEFAULT,
	.enable_dapc = PLAT_DBG_DAPC_DEFAULT,
};

static struct mtk_devapc_vio_info mt6781_devapc_vio_info = {
	.vio_cfg_max_idx = PLAT_VIO_CFG_MAX_IDX,
	.vio_max_idx = PLAT_VIO_MAX_IDX,
	.vio_mask_sta_num = PLAT_VIO_MASK_STA_NUM,
	.vio_shift_max_bit = PLAT_VIO_SHIFT_MAX_BIT,
	.sramrom_vio_idx = SRAMROM_VIO_INDEX,
};

static const struct mtk_infra_vio_dbg_desc mt6781_vio_dbgs = {
	.infra_vio_dbg_mstid = INFRA_VIO_DBG_MSTID,
	.infra_vio_dbg_mstid_start_bit = INFRA_VIO_DBG_MSTID_START_BIT,
	.infra_vio_dbg_dmnid = INFRA_VIO_DBG_DMNID,
	.infra_vio_dbg_dmnid_start_bit = INFRA_VIO_DBG_DMNID_START_BIT,
	.infra_vio_dbg_w_vio = INFRA_VIO_DBG_W_VIO,
	.infra_vio_dbg_w_vio_start_bit = INFRA_VIO_DBG_W_VIO_START_BIT,
	.infra_vio_dbg_r_vio = INFRA_VIO_DBG_R_VIO,
	.infra_vio_dbg_r_vio_start_bit = INFRA_VIO_DBG_R_VIO_START_BIT,
	.infra_vio_addr_high = INFRA_VIO_ADDR_HIGH,
	.infra_vio_addr_high_start_bit = INFRA_VIO_ADDR_HIGH_START_BIT,
};

static const struct mtk_sramrom_sec_vio_desc mt6781_sramrom_sec_vios = {
	.sramrom_sec_vio_id_mask = SRAMROM_SEC_VIO_ID_MASK,
	.sramrom_sec_vio_id_shift = SRAMROM_SEC_VIO_ID_SHIFT,
	.sramrom_sec_vio_domain_mask = SRAMROM_SEC_VIO_DOMAIN_MASK,
	.sramrom_sec_vio_domain_shift = SRAMROM_SEC_VIO_DOMAIN_SHIFT,
	.sramrom_sec_vio_rw_mask = SRAMROM_SEC_VIO_RW_MASK,
	.sramrom_sec_vio_rw_shift = SRAMROM_SEC_VIO_RW_SHIFT,
};

static const struct mtk_devapc_pd_desc mt6781_devapc_pds = {
	.pd_vio_mask_offset = PD_VIO_MASK_OFFSET,
	.pd_vio_sta_offset = PD_VIO_STA_OFFSET,
	.pd_vio_dbg0_offset = PD_VIO_DBG0_OFFSET,
	.pd_vio_dbg1_offset = PD_VIO_DBG1_OFFSET,
	.pd_apc_con_offset = PD_APC_CON_OFFSET,
	.pd_shift_sta_offset = PD_SHIFT_STA_OFFSET,
	.pd_shift_sel_offset = PD_SHIFT_SEL_OFFSET,
	.pd_shift_con_offset = PD_SHIFT_CON_OFFSET,
};

static struct mtk_devapc_soc mt6781_data = {
	.dbg_stat = &mt6781_devapc_dbg_stat,
	.device_info = mt6781_infra_devices,
	.ndevices = ARRAY_SIZE(mt6781_infra_devices),
	.vio_info = &mt6781_devapc_vio_info,
	.vio_dbgs = &mt6781_vio_dbgs,
	.sramrom_sec_vios = &mt6781_sramrom_sec_vios,
	.devapc_pds = &mt6781_devapc_pds,
	.master_get = &bus_id_to_master,
	.subsys_get = &index_to_subsys,
	.shift_group_get = &mt6781_shift_group_get,
};

static const struct of_device_id mt6781_devapc_dt_match[] = {
	{ .compatible = "mediatek,mt6781-devapc" },
	{},
};

static int mt6781_devapc_probe(struct platform_device *pdev)
{
	return mtk_devapc_probe(pdev, &mt6781_data);
}

static int mt6781_devapc_remove(struct platform_device *dev)
{
	return mtk_devapc_remove(dev);
}

static struct platform_driver mt6781_devapc_driver = {
	.probe = mt6781_devapc_probe,
	.remove = mt6781_devapc_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.owner = THIS_MODULE,
		.of_match_table = mt6781_devapc_dt_match,
	},
};

/*
 * devapc_init: module init function.
 */
static int __init mt6781_devapc_init(void)
{
	int ret;

	DEVAPC_MSG("module initialized\n");

	ret = platform_driver_register(&mt6781_devapc_driver);
	if (ret) {
		DEVAPC_MSG("Unable to register driver, ret(%d)\n", ret);
		return ret;
	}

	proc_create("devapc_dbg", 0664, NULL, &devapc_dbg_fops);

	return 0;
}

arch_initcall(mt6781_devapc_init);

MODULE_DESCRIPTION("Mediatek MT6781 Device APC Driver");
MODULE_AUTHOR("Jackson Chang <jackson-kt.chang@mediatek.com>");
MODULE_LICENSE("GPL");
