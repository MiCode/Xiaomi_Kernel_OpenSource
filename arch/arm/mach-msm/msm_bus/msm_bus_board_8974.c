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
#include <linux/init.h>
#include <linux/device.h>
#include <linux/module.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include <mach/board.h>
#include <mach/rpm.h>
#include "msm_bus_core.h"
#include "msm_bus_noc.h"
#include "msm_bus_bimc.h"

#define NMASTERS 120
#define NSLAVES 150
#define NFAB_8974 7

enum msm_bus_8974_master_ports_type {
	/* System NOC Masters */
	MASTER_PORT_LPASS_AHB = 0,
	MASTER_PORT_QDSS_BAM,
	MASTER_PORT_SNOC_CFG,
	MASTER_PORT_GW_BIMC_SNOC,
	MASTER_PORT_GW_CNOC_SNOC,
	MASTER_PORT_CRYPTO_CORE0,
	MASTER_PORT_CRYPTO_CORE1,
	MASTER_PORT_LPASS_PROC,
	MASTER_PORT_MSS,
	MASTER_PORT_MSS_NAV,
	MASTER_PORT_OCMEM_DMA,
	MASTER_PORT_GW_PNOC_SNOC,
	MASTER_PORT_WCSS,
	MASTER_PORT_QDSS_ETR,
	MASTER_PORT_USB3,

	/* MMSS NOC Masters */
	MASTER_PORT_GW_CNOC_MNOC_MMSS_CFG = 0,
	MASTER_PORT_GW_CNOC_MNOC_CFG,
	MASTER_PORT_GFX3D_PORT0,
	MASTER_PORT_GFX3D_PORT1,
	MASTER_PORT_JPEG,
	MASTER_PORT_MDP,
	/* Venus video core */
	MASTER_PORT_VIDEO_PORT0,
	MASTER_PORT_VIDEO_PORT1,
	MASTER_PORT_VFE = 16,

	/* BIMC Masters */
	MASTER_PORT_KMPSS_M0 = 0,
	MASTER_PORT_KMPSS_M1,
	MASTER_PORT_MSS_PROC,
	MASTER_PORT_GW_MNOC_BIMC_0,
	MASTER_PORT_GW_MNOC_BIMC_1,
	MASTER_PORT_GW_SNOC_BIMC_0,
	MASTER_PORT_GW_SNOC_BIMC_1,

	/* OCMEM NOC Masters */
	MASTER_PORT_CNOC_ONOC_CFG = 0,
	MASTER_PORT_JPEG_OCMEM,
	MASTER_PORT_MDP_OCMEM,
	MASTER_PORT_VIDEO_P0_OCMEM,
	MASTER_PORT_VIDEO_P1_OCMEM,
	MASTER_PORT_VFE_OCMEM,

	/* Peripheral NOC Masters */
	MASTER_PORT_SDCC_1 = 0,
	MASTER_PORT_SDCC_3,
	MASTER_PORT_SDCC_2,
	MASTER_PORT_SDCC_4,
	MASTER_PORT_TSIF,
	MASTER_PORT_BAM_DMA,
	MASTER_PORT_BLSP_2,
	MASTER_PORT_USB_HSIC,
	MASTER_PORT_BLSP_1,
	MASTER_PORT_USB_HS,
	MASTER_PORT_PNOC_CFG,
	MASTER_PORT_GW_SNOC_PNOC,

	/* Config NOC Masters */
	MASTER_PORT_RPM_INST = 0,
	MASTER_PORT_RPM_DATA,
	MASTER_PORT_RPM_SYS,
	MASTER_PORT_DEHR,
	MASTER_PORT_QDSS_DAP,
	MASTER_PORT_SPDM,
	MASTER_PORT_TIC,
	MASTER_PORT_GW_SNOC_CNOC,
};

enum msm_bus_8974_slave_ports_type {
	/* System NOC Slaves */
	SLAVE_PORT_KMPSS = 1,
	SLAVE_PORT_LPASS,
	SLAVE_PORT_USB3 = 4,
	SLAVE_PORT_WCSS = 6,
	SLAVE_PORT_GW_SNOC_BIMC_P0,
	SLAVE_PORT_GW_SNOC_BIMC_P1,
	SLAVE_PORT_GW_SNOC_CNOC,
	SLAVE_PORT_OCIMEM,
	SLAVE_PORT_SNOC_OCMEM,
	SLAVE_PORT_GW_SNOC_PNOC,
	SLAVE_PORT_SERVICE_SNOC,
	SLAVE_PORT_QDSS_STM,

	/* MMSS NOC Slaves */
	SLAVE_PORT_CAMERA_CFG = 0,
	SLAVE_PORT_DISPLAY_CFG,
	SLAVE_PORT_OCMEM_CFG,
	SLAVE_PORT_CPR_CFG,
	SLAVE_PORT_CPR_XPU_CFG,
	SLAVE_PORT_MISC_CFG = 6,
	SLAVE_PORT_MISC_XPU_CFG,
	SLAVE_PORT_VENUS_CFG,
	SLAVE_PORT_GFX3D_CFG,
	SLAVE_PORT_MMSS_CLK_CFG = 11,
	SLAVE_PORT_MMSS_CLK_XPU_CFG,
	SLAVE_PORT_MNOC_MPU_CFG,
	SLAVE_PORT_ONOC_MPU_CFG,
	SLAVE_PORT_GW_MMSS_BIMC_P0 = 16,
	SLAVE_PORT_GW_MMSS_BIMC_P1,
	SLAVE_PORT_SERVICE_MNOC,

	/* BIMC Slaves */
	SLAVE_PORT_EBI1_CH0 = 0,
	SLAVE_PORT_EBI1_CH1,
	SLAVE_PORT_KMPSS_L2,
	SLAVE_PORT_GW_BIMC_SNOC,

	/* OCMEM NOC Slaves */
	SLAVE_PORT_OCMEM_P0 = 0,
	SLAVE_PORT_OCMEM_P1,
	SLAVE_PORT_SERVICE_ONOC,

	/*Peripheral NOC Slaves */
	SLAVE_PORT_SDCC_1 = 0,
	SLAVE_PORT_SDCC_3,
	SLAVE_PORT_SDCC_2,
	SLAVE_PORT_SDCC_4,
	SLAVE_PORT_TSIF,
	SLAVE_PORT_BAM_DMA,
	SLAVE_PORT_BLSP_2,
	SLAVE_PORT_USB_HSIC,
	SLAVE_PORT_BLSP_1,
	SLAVE_PORT_USB_HS,
	SLAVE_PORT_PDM,
	SLAVE_PORT_PERIPH_APU_CFG,
	SLAVE_PORT_PNOC_MPU_CFG,
	SLAVE_PORT_PRNG,
	SLAVE_PORT_GW_PNOC_SNOC,
	SLAVE_PORT_SERVICE_PNOC,

	/* Config NOC slaves */
	SLAVE_PORT_CLK_CTL = 1,
	SLAVE_PORT_CNOC_MSS,
	SLAVE_PORT_SECURITY,
	SLAVE_PORT_TCSR,
	SLAVE_PORT_TLMM,
	SLAVE_PORT_CRYPTO_0_CFG,
	SLAVE_PORT_CRYPTO_1_CFG,
	SLAVE_PORT_IMEM_CFG,
	SLAVE_PORT_MESSAGE_RAM,
	SLAVE_PORT_BIMC_CFG,
	SLAVE_PORT_BOOT_ROM,
	SLAVE_PORT_CNOC_MNOC_MMSS_CFG,
	SLAVE_PORT_PMIC_ARB,
	SLAVE_PORT_SPDM_WRAPPER,
	SLAVE_PORT_DEHR_CFG,
	SLAVE_PORT_MPM,
	SLAVE_PORT_QDSS_CFG,
	SLAVE_PORT_RBCPR_CFG,
	SLAVE_PORT_RBCPR_QDSS_APU_CFG,
	SLAVE_PORT_CNOC_MNOC_CFG,
	SLAVE_PORT_SNOC_MPU_CFG,
	SLAVE_PORT_CNOC_ONOC_CFG,
	SLAVE_PORT_PNOC_CFG,
	SLAVE_PORT_SNOC_CFG,
	SLAVE_PORT_EBI1_DLL_CFG,
	SLAVE_PORT_PHY_APU_CFG,
	SLAVE_PORT_EBI1_PHY_CFG,
	SLAVE_PORT_RPM,
	SLAVE_PORT_GW_CNOC_SNOC,
	SLAVE_PORT_SERVICE_CNOC,
};

/* Hardware IDs for RPM */
enum msm_bus_8974_mas_hw_id {
	MAS_APPSS_PROC = 0,
	MAS_AMSS_PROC,
	MAS_MNOC_BIMC,
	MAS_SNOC_BIMC,
	MAS_CNOC_MNOC_MMSS_CFG,
	MAS_CNOC_MNOC_CFG,
	MAS_GFX3D,
	MAS_JPEG,
	MAS_MDP,
	MAS_VIDEO_P0,
	MAS_VIDEO_P1,
	MAS_VFE,
	MAS_CNOC_ONOC_CFG,
	MAS_JPEG_OCMEM,
	MAS_MDP_OCMEM,
	MAS_VIDEO_P0_OCMEM,
	MAS_VIDEO_P1_OCMEM,
	MAS_VFE_OCMEM,
	MAS_LPASS_AHB,
	MAS_QDSS_BAM,
	MAS_SNOC_CFG,
	MAS_BIMC_SNOC,
	MAS_CNOC_SNOC,
	MAS_CRYPTO_CORE0,
	MAS_CRYPTO_CORE1,
	MAS_LPASS_PROC,
	MAS_MSS,
	MAS_MSS_NAV,
	MAS_OCMEM_DMA,
	MAS_PNOC_SNOC,
	MAS_WCSS,
	MAS_QDSS_ETR,
	MAS_USB3,
	MAS_SDCC_1,
	MAS_SDCC_3,
	MAS_SDCC_2,
	MAS_SDCC_4,
	MAS_TSIF,
	MAS_BAM_DMA,
	MAS_BLSP_2,
	MAS_USB_HSIC,
	MAS_BLSP_1,
	MAS_USB_HS,
	MAS_PNOC_CFG,
	MAS_SNOC_PNOC,
	MAS_RPM_INST,
	MAS_RPM_DATA,
	MAS_RPM_SYS,
	MAS_DEHR,
	MAS_QDSS_DAP,
	MAS_SPDM,
	MAS_TIC,
	MAS_SNOC_CNOC,
	MAS_OVNOC_SNOC,
	MAS_OVNOC_ONOC,
	MAS_V_OCMEM_GFX3D,
	MAS_ONOC_OVNOC,
	MAS_SNOC_OVNOC,
};

enum msm_bus_8974_slv_hw_id {
	SLV_EBI = 0,
	SLV_APSS_L2,
	SLV_BIMC_SNOC,
	SLV_CAMERA_CFG,
	SLV_DISPLAY_CFG,
	SLV_OCMEM_CFG,
	SLV_CPR_CFG,
	SLV_CPR_XPU_CFG,
	SLV_MISC_CFG,
	SLV_MISC_XPU_CFG,
	SLV_VENUS_CFG,
	SLV_GFX3D_CFG,
	SLV_MMSS_CLK_CFG,
	SLV_MMSS_CLK_XPU_CFG,
	SLV_MNOC_MPU_CFG,
	SLV_ONOC_MPU_CFG,
	SLV_MMSS_BIMC,
	SLV_SERVICE_MNOC,
	SLV_OCMEM,
	SLV_SERVICE_ONOC,
	SLV_APPSS,
	SLV_LPASS,
	SLV_USB3,
	SLV_WCSS,
	SLV_SNOC_BIMC,
	SLV_SNOC_CNOC,
	SLV_OCIMEM,
	SLV_SNOC_OCMEM,
	SLV_SNOC_PNOC,
	SLV_SERVICE_SNOC,
	SLV_QDSS_STM,
	SLV_SDCC_1,
	SLV_SDCC_3,
	SLV_SDCC_2,
	SLV_SDCC_4,
	SLV_TSIF,
	SLV_BAM_DMA,
	SLV_BLSP_2,
	SLV_USB_HSIC,
	SLV_BLSP_1,
	SLV_USB_HS,
	SLV_PDM,
	SLV_PERIPH_APU_CFG,
	SLV_MPU_CFG,
	SLV_PRNG,
	SLV_PNOC_SNOC,
	SLV_SERVICE_PNOC,
	SLV_CLK_CTL,
	SLV_CNOC_MSS,
	SLV_SECURITY,
	SLV_TCSR,
	SLV_TLMM,
	SLV_CRYPTO_0_CFG,
	SLV_CRYPTO_1_CFG,
	SLV_IMEM_CFG,
	SLV_MESSAGE_RAM,
	SLV_BIMC_CFG,
	SLV_BOOT_ROM,
	SLV_CNOC_MNOC_MMSS_CFG,
	SLV_PMIC_ARB,
	SLV_SPDM_WRAPPER,
	SLV_DEHR_CFG,
	SLV_MPM,
	SLV_QDSS_CFG,
	SLV_RBCPR_CFG,
	SLV_RBCPR_QDSS_APU_CFG,
	SLV_CNOC_MNOC_CFG,
	SLV_SNOC_MPU_CFG,
	SLV_CNOC_ONOC_CFG,
	SLV_PNOC_CFG,
	SLV_SNOC_CFG,
	SLV_EBI1_DLL_CFG,
	SLV_PHY_APU_CFG,
	SLV_EBI1_PHY_CFG,
	SLV_RPM,
	SLV_CNOC_SNOC,
	SLV_SERVICE_CNOC,
	SLV_SNOC_OVNOC,
	SLV_ONOC_OVNOC,
	SLV_OVNOC_ONOC,
	SLV_OVNOC_SNOC,
};

static uint32_t master_iids[NMASTERS];
static uint32_t slave_iids[NSLAVES];

/* System NOC nodes */
static int mport_lpass_ahb[] = {MASTER_PORT_LPASS_AHB,};
static int mport_qdss_bam[] = {MASTER_PORT_QDSS_BAM,};
static int mport_snoc_cfg[] = {MASTER_PORT_SNOC_CFG,};
static int mport_gw_bimc_snoc[] = {MASTER_PORT_GW_BIMC_SNOC,};
static int mport_gw_cnoc_snoc[] = {MASTER_PORT_GW_CNOC_SNOC,};
static int mport_crypto_core0[] = {MASTER_PORT_CRYPTO_CORE0,};
static int mport_crypto_core1[] = {MASTER_PORT_CRYPTO_CORE1};
static int mport_lpass_proc[] = {MASTER_PORT_LPASS_PROC};
static int mport_mss[] = {MASTER_PORT_MSS};
static int mport_mss_nav[] = {MASTER_PORT_MSS_NAV};
static int mport_ocmem_dma[] = {MASTER_PORT_OCMEM_DMA};
static int mport_gw_pnoc_snoc[] = {MASTER_PORT_GW_PNOC_SNOC};
static int mport_wcss[] = {MASTER_PORT_WCSS};
static int mport_qdss_etr[] = {MASTER_PORT_QDSS_ETR};
static int mport_usb3[] = {MASTER_PORT_USB3};

static int sport_kmpss[] = {SLAVE_PORT_KMPSS};
static int sport_lpass[] = {SLAVE_PORT_LPASS};
static int sport_usb3[] = {SLAVE_PORT_USB3};
static int sport_wcss[] = {SLAVE_PORT_WCSS};
static int sport_gw_snoc_bimc[] = {
	SLAVE_PORT_GW_SNOC_BIMC_P0,
	SLAVE_PORT_GW_SNOC_BIMC_P1,
	};
static int sport_gw_snoc_cnoc[] = {SLAVE_PORT_GW_SNOC_CNOC};
static int sport_ocimem[] = {SLAVE_PORT_OCIMEM};
static int sport_snoc_ocmem[] = {SLAVE_PORT_SNOC_OCMEM};
static int sport_gw_snoc_pnoc[] = {SLAVE_PORT_GW_SNOC_PNOC};
static int sport_service_snoc[] = {SLAVE_PORT_SERVICE_SNOC};
static int sport_qdss_stm[] = {SLAVE_PORT_QDSS_STM};


/* MMSS NOC nodes */
static int mport_gw_cnoc_mnoc_cfg[] = {
	MASTER_PORT_GW_CNOC_MNOC_MMSS_CFG,
	MASTER_PORT_GW_CNOC_MNOC_CFG,
};
static int mport_gfx3d[] = {
	MASTER_PORT_GFX3D_PORT0,
	MASTER_PORT_GFX3D_PORT1,
};
static int mport_jpeg[] = {MASTER_PORT_JPEG};
static int mport_mdp[] = {MASTER_PORT_MDP};
static int mport_video_port0[] = {MASTER_PORT_VIDEO_PORT0};
static int mport_video_port1[] = {MASTER_PORT_VIDEO_PORT1};
static int mport_vfe[] = {MASTER_PORT_VFE};

static int sport_camera_cfg[] = {SLAVE_PORT_CAMERA_CFG};
static int sport_display_cfg[] = {SLAVE_PORT_DISPLAY_CFG};
static int sport_ocmem_cfg[] = {SLAVE_PORT_OCMEM_CFG};
static int sport_cpr_cfg[] = {SLAVE_PORT_CPR_CFG};
static int sport_cpr_xpu_cfg[] = {SLAVE_PORT_CPR_XPU_CFG,};
static int sport_misc_cfg[] = {SLAVE_PORT_MISC_CFG};
static int sport_misc_xpu_cfg[] = {SLAVE_PORT_MISC_XPU_CFG};
static int sport_venus_cfg[] = {SLAVE_PORT_VENUS_CFG};
static int sport_gfx3d_cfg[] = {SLAVE_PORT_GFX3D_CFG};
static int sport_mmss_clk_cfg[] = {SLAVE_PORT_MMSS_CLK_CFG};
static int sport_mmss_clk_xpu_cfg[] = {
	SLAVE_PORT_MMSS_CLK_XPU_CFG
};
static int sport_mnoc_mpu_cfg[] = {SLAVE_PORT_MNOC_MPU_CFG};
static int sport_onoc_mpu_cfg[] = {SLAVE_PORT_ONOC_MPU_CFG};
static int sport_gw_mmss_bimc[] = {
	SLAVE_PORT_GW_MMSS_BIMC_P0,
	SLAVE_PORT_GW_MMSS_BIMC_P1,
};
static int sport_service_mnoc[] = {SLAVE_PORT_SERVICE_MNOC};

/* BIMC Nodes */

static int mport_kmpss_m0[] = {MASTER_PORT_KMPSS_M0,};
static int mport_kmpss_m1[] = {MASTER_PORT_KMPSS_M1};
static int mport_mss_proc[] = {MASTER_PORT_MSS_PROC};
static int mport_gw_mnoc_bimc[] = {
	MASTER_PORT_GW_MNOC_BIMC_0,
	MASTER_PORT_GW_MNOC_BIMC_1,
};
static int mport_gw_snoc_bimc[] = {
	MASTER_PORT_GW_SNOC_BIMC_0,
	MASTER_PORT_GW_SNOC_BIMC_1,
};

static int sport_ebi1[] = {
	SLAVE_PORT_EBI1_CH0,
	SLAVE_PORT_EBI1_CH1,
};
static int sport_kmpss_l2[] = {SLAVE_PORT_KMPSS_L2,};
static int sport_gw_bimc_snoc[] = {SLAVE_PORT_GW_BIMC_SNOC,};

/* OCMEM NOC Nodes */
static int mport_cnoc_onoc_cfg[] = {
	MASTER_PORT_CNOC_ONOC_CFG,
};
static int mport_jpeg_ocmem[] = {MASTER_PORT_JPEG_OCMEM,};
static int mport_mdp_ocmem[] = {MASTER_PORT_MDP_OCMEM,};
static int mport_video_p0_ocmem[] = {
	MASTER_PORT_VIDEO_P0_OCMEM,
};
static int mport_video_p1_ocmem[] = {
	MASTER_PORT_VIDEO_P1_OCMEM,
};
static int mport_vfe_ocmem[] = {MASTER_PORT_VFE_OCMEM,};
static int sport_ocmem[] = {
	SLAVE_PORT_OCMEM_P0,
	SLAVE_PORT_OCMEM_P1,
};

static int sport_service_onoc[] = {SLAVE_PORT_SERVICE_ONOC,};

/* Peripheral NOC Nodes */
static int mport_sdcc_1[] = {MASTER_PORT_SDCC_1,};
static int mport_sdcc_3[] = {MASTER_PORT_SDCC_3,};
static int mport_sdcc_2[] = {MASTER_PORT_SDCC_2,};
static int mport_sdcc_4[] = {MASTER_PORT_SDCC_4,};
static int mport_tsif[] = {MASTER_PORT_TSIF,};
static int mport_bam_dma[] = {MASTER_PORT_BAM_DMA,};
static int mport_blsp_2[] = {MASTER_PORT_BLSP_2,};
static int mport_usb_hsic[] = {MASTER_PORT_USB_HSIC,};
static int mport_blsp_1[] = {MASTER_PORT_BLSP_1,};
static int mport_usb_hs[] = {MASTER_PORT_USB_HS,};
static int mport_pnoc_cfg[] = {MASTER_PORT_PNOC_CFG,};
static int mport_gw_snoc_pnoc[] = {MASTER_PORT_GW_SNOC_PNOC,};

static int sport_sdcc_1[] = {SLAVE_PORT_SDCC_1,};
static int sport_sdcc_3[] = {SLAVE_PORT_SDCC_3,};
static int sport_sdcc_2[] = {SLAVE_PORT_SDCC_2,};
static int sport_sdcc_4[] = {SLAVE_PORT_SDCC_4,};
static int sport_tsif[] = {SLAVE_PORT_TSIF,};
static int sport_bam_dma[] = {SLAVE_PORT_BAM_DMA,};
static int sport_blsp_2[] = {SLAVE_PORT_BLSP_2,};
static int sport_usb_hsic[] = {SLAVE_PORT_USB_HSIC,};
static int sport_blsp_1[] = {SLAVE_PORT_BLSP_1,};
static int sport_usb_hs[] = {SLAVE_PORT_USB_HS,};
static int sport_pdm[] = {SLAVE_PORT_PDM,};
static int sport_periph_apu_cfg[] = {
	SLAVE_PORT_PERIPH_APU_CFG,
};
static int sport_pnoc_mpu_cfg[] = {SLAVE_PORT_PNOC_MPU_CFG,};
static int sport_prng[] = {SLAVE_PORT_PRNG,};
static int sport_gw_pnoc_snoc[] = {SLAVE_PORT_GW_PNOC_SNOC,};
static int sport_service_pnoc[] = {SLAVE_PORT_SERVICE_PNOC,};

/* Config NOC Nodes */
static int mport_rpm_inst[] = {MASTER_PORT_RPM_INST,};
static int mport_rpm_data[] = {MASTER_PORT_RPM_DATA,};
static int mport_rpm_sys[] = {MASTER_PORT_RPM_SYS,};
static int mport_dehr[] = {MASTER_PORT_DEHR,};
static int mport_qdss_dap[] = {MASTER_PORT_QDSS_DAP,};
static int mport_spdm[] = {MASTER_PORT_SPDM,};
static int mport_tic[] = {MASTER_PORT_TIC,};
static int mport_gw_snoc_cnoc[] = {MASTER_PORT_GW_SNOC_CNOC,};

static int sport_clk_ctl[] = {SLAVE_PORT_CLK_CTL,};
static int sport_cnoc_mss[] = {SLAVE_PORT_CNOC_MSS,};
static int sport_security[] = {SLAVE_PORT_SECURITY,};
static int sport_tcsr[] = {SLAVE_PORT_TCSR,};
static int sport_tlmm[] = {SLAVE_PORT_TLMM,};
static int sport_crypto_0_cfg[] = {SLAVE_PORT_CRYPTO_0_CFG,};
static int sport_crypto_1_cfg[] = {SLAVE_PORT_CRYPTO_1_CFG,};
static int sport_imem_cfg[] = {SLAVE_PORT_IMEM_CFG,};
static int sport_message_ram[] = {SLAVE_PORT_MESSAGE_RAM,};
static int sport_bimc_cfg[] = {SLAVE_PORT_BIMC_CFG,};
static int sport_boot_rom[] = {SLAVE_PORT_BOOT_ROM,};
static int sport_cnoc_mnoc_mmss_cfg[] = {SLAVE_PORT_CNOC_MNOC_MMSS_CFG,};
static int sport_cnoc_mnoc_cfg[] = {SLAVE_PORT_CNOC_MNOC_CFG,};
static int sport_pmic_arb[] = {SLAVE_PORT_PMIC_ARB,};
static int sport_spdm_wrapper[] = {SLAVE_PORT_SPDM_WRAPPER,};
static int sport_dehr_cfg[] = {SLAVE_PORT_DEHR_CFG,};
static int sport_mpm[] = {SLAVE_PORT_MPM,};
static int sport_qdss_cfg[] = {SLAVE_PORT_QDSS_CFG,};
static int sport_rbcpr_cfg[] = {SLAVE_PORT_RBCPR_CFG,};
static int sport_rbcpr_qdss_apu_cfg[] = {SLAVE_PORT_RBCPR_QDSS_APU_CFG,};
static int sport_snoc_mpu_cfg[] = {SLAVE_PORT_SNOC_MPU_CFG,};
static int sport_cnoc_onoc_cfg[] = {SLAVE_PORT_CNOC_ONOC_CFG,};
static int sport_pnoc_cfg[] = {SLAVE_PORT_PNOC_CFG,};
static int sport_snoc_cfg[] = {SLAVE_PORT_SNOC_CFG,};
static int sport_ebi1_dll_cfg[] = {SLAVE_PORT_EBI1_DLL_CFG,};
static int sport_phy_apu_cfg[] = {SLAVE_PORT_PHY_APU_CFG,};
static int sport_ebi1_phy_cfg[] = {SLAVE_PORT_EBI1_PHY_CFG,};
static int sport_rpm[] = {SLAVE_PORT_RPM,};
static int sport_gw_cnoc_snoc[] = {SLAVE_PORT_GW_CNOC_SNOC,};
static int sport_service_cnoc[] = {SLAVE_PORT_SERVICE_CNOC,};

static int tier2[] = {MSM_BUS_BW_TIER2,};

/*
 * QOS Ports defined only when qos ports are different than
 * master ports
 **/
static int qports_gemini[] = {0};
static int qports_mdp[] = {1};
static int qports_venus_p0[] = {4};
static int qports_venus_p1[] = {5};
static int qports_vfe[] = {6};
static int qports_gemini_ocmem[] = {0};
static int qports_venus_p0_ocmem[] = {2};
static int qports_venus_p1_ocmem[] = {3};
static int qports_vfe_ocmem[] = {4};
static int qports_crypto_c0[] = {2};
static int qports_crypto_c1[] = {3};
static int qports_lpass_proc[] = {4};
static int qports_ocmem_dma[] = {7};
static int qports_gw_snoc_bimc[] = {5, 6};
static int qports_kmpss[] = {0, 1};
static int qports_lpass_ahb[] = {0};
static int qports_qdss_bam[] = {1};
static int qports_gw_pnoc_snoc[] = {8};
static int qports_qdss_etr[] = {10};
static int qports_usb3[] = {11};
static int qports_oxili[] = {2, 3};
static int qports_gw_mnoc_bimc[] = {3, 4};

static struct msm_bus_node_info sys_noc_info[] = {
	{
		.id = MSM_BUS_MASTER_LPASS_AHB,
		.masterp = mport_lpass_ahb,
		.num_mports = ARRAY_SIZE(mport_lpass_ahb),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.qport = qports_lpass_ahb,
		.mas_hw_id = MAS_LPASS_AHB,
		.mode = NOC_QOS_MODE_FIXED,
		.prio_rd = 2,
		.prio_wr = 2,
	},
	{
		.id = MSM_BUS_MASTER_QDSS_BAM,
		.masterp = mport_qdss_bam,
		.num_mports = ARRAY_SIZE(mport_qdss_bam),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.mode = NOC_QOS_MODE_FIXED,
		.qport = qports_qdss_bam,
		.mas_hw_id = MAS_QDSS_BAM,
	},
	{
		.id = MSM_BUS_MASTER_SNOC_CFG,
		.masterp = mport_snoc_cfg,
		.num_mports = ARRAY_SIZE(mport_snoc_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.mas_hw_id = MAS_SNOC_CFG,
	},
	{
		.id = MSM_BUS_FAB_BIMC,
		.gateway = 1,
		.slavep = sport_gw_snoc_bimc,
		.num_sports = ARRAY_SIZE(sport_gw_snoc_bimc),
		.masterp = mport_gw_bimc_snoc,
		.num_mports = ARRAY_SIZE(mport_gw_bimc_snoc),
		.buswidth = 8,
		.mas_hw_id = MAS_BIMC_SNOC,
		.slv_hw_id = SLV_SNOC_BIMC,
	},
	{
		.id = MSM_BUS_FAB_CONFIG_NOC,
		.gateway = 1,
		.slavep = sport_gw_snoc_cnoc,
		.num_sports = ARRAY_SIZE(sport_gw_snoc_cnoc),
		.masterp = mport_gw_cnoc_snoc,
		.num_mports = ARRAY_SIZE(mport_gw_cnoc_snoc),
		.buswidth = 8,
		.mas_hw_id = MAS_CNOC_SNOC,
		.slv_hw_id = SLV_SNOC_CNOC,
	},
	{
		.id = MSM_BUS_FAB_PERIPH_NOC,
		.gateway = 1,
		.slavep = sport_gw_snoc_pnoc,
		.num_sports = ARRAY_SIZE(sport_gw_snoc_pnoc),
		.masterp = mport_gw_pnoc_snoc,
		.num_mports = ARRAY_SIZE(mport_gw_pnoc_snoc),
		.buswidth = 8,
		.qport = qports_gw_pnoc_snoc,
		.mas_hw_id = MAS_PNOC_SNOC,
		.slv_hw_id = SLV_SNOC_PNOC,
		.mode = NOC_QOS_MODE_FIXED,
		.prio_rd = 2,
		.prio_wr = 2,
	},
	{
		.id = MSM_BUS_FAB_OCMEM_VNOC,
		.gateway = 1,
		.buswidth = 8,
		.mas_hw_id = MAS_OVNOC_SNOC,
		.slv_hw_id = SLV_SNOC_OVNOC,
	},
	{
		.id = MSM_BUS_MASTER_CRYPTO_CORE0,
		.masterp = mport_crypto_core0,
		.num_mports = ARRAY_SIZE(mport_crypto_core0),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.mode = NOC_QOS_MODE_FIXED,
		.qport = qports_crypto_c0,
		.mas_hw_id = MAS_CRYPTO_CORE0,
		.hw_sel = MSM_BUS_NOC,
		.prio_rd = 1,
		.prio_wr = 1,
	},
	{
		.id = MSM_BUS_MASTER_CRYPTO_CORE1,
		.masterp = mport_crypto_core1,
		.num_mports = ARRAY_SIZE(mport_crypto_core1),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.mode = NOC_QOS_MODE_FIXED,
		.qport = qports_crypto_c1,
		.mas_hw_id = MAS_CRYPTO_CORE1,
		.hw_sel = MSM_BUS_NOC,
		.prio_rd = 1,
		.prio_wr = 1,
	},
	{
		.id = MSM_BUS_MASTER_LPASS_PROC,
		.masterp = mport_lpass_proc,
		.num_mports = ARRAY_SIZE(mport_lpass_proc),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.qport = qports_lpass_proc,
		.mas_hw_id = MAS_LPASS_PROC,
		.mode = NOC_QOS_MODE_FIXED,
		.prio_rd = 2,
		.prio_wr = 2,
	},
	{
		.id = MSM_BUS_MASTER_MSS,
		.masterp = mport_mss,
		.num_mports = ARRAY_SIZE(mport_mss),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.mas_hw_id = MAS_MSS,
	},
	{
		.id = MSM_BUS_MASTER_MSS_NAV,
		.masterp = mport_mss_nav,
		.num_mports = ARRAY_SIZE(mport_mss_nav),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.mas_hw_id = MAS_MSS_NAV,
	},
	{
		.id = MSM_BUS_MASTER_OCMEM_DMA,
		.masterp = mport_ocmem_dma,
		.num_mports = ARRAY_SIZE(mport_ocmem_dma),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.mode = NOC_QOS_MODE_FIXED,
		.qport = qports_ocmem_dma,
		.mas_hw_id = MAS_OCMEM_DMA,
	},
	{
		.id = MSM_BUS_MASTER_WCSS,
		.masterp = mport_wcss,
		.num_mports = ARRAY_SIZE(mport_wcss),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.mas_hw_id = MAS_WCSS,
	},
	{
		.id = MSM_BUS_MASTER_QDSS_ETR,
		.masterp = mport_qdss_etr,
		.num_mports = ARRAY_SIZE(mport_qdss_etr),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.qport = qports_qdss_etr,
		.mode = NOC_QOS_MODE_FIXED,
		.mas_hw_id = MAS_QDSS_ETR,
	},
	{
		.id = MSM_BUS_MASTER_USB3,
		.masterp = mport_usb3,
		.num_mports = ARRAY_SIZE(mport_usb3),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.mode = NOC_QOS_MODE_FIXED,
		.qport = qports_usb3,
		.mas_hw_id = MAS_USB3,
		.prio_rd = 2,
		.prio_wr = 2,
		.hw_sel = MSM_BUS_NOC,
		.iface_clk_node = "msm_usb3",
	},
	{
		.id = MSM_BUS_SLAVE_AMPSS,
		.slavep = sport_kmpss,
		.num_sports = ARRAY_SIZE(sport_kmpss),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_APPSS,
	},
	{
		.id = MSM_BUS_SLAVE_LPASS,
		.slavep = sport_lpass,
		.num_sports = ARRAY_SIZE(sport_lpass),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_LPASS,
	},
	{
		.id = MSM_BUS_SLAVE_USB3,
		.slavep = sport_usb3,
		.num_sports = ARRAY_SIZE(sport_usb3),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_USB3,
	},
	{
		.id = MSM_BUS_SLAVE_WCSS,
		.slavep = sport_wcss,
		.num_sports = ARRAY_SIZE(sport_wcss),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_WCSS,
	},
	{
		.id = MSM_BUS_SLAVE_OCIMEM,
		.slavep = sport_ocimem,
		.num_sports = ARRAY_SIZE(sport_ocimem),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_OCIMEM,
	},
	{
		.id = MSM_BUS_SLAVE_SNOC_OCMEM,
		.slavep = sport_snoc_ocmem,
		.num_sports = ARRAY_SIZE(sport_snoc_ocmem),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_SNOC_OCMEM,
	},
	{
		.id = MSM_BUS_SLAVE_SERVICE_SNOC,
		.slavep = sport_service_snoc,
		.num_sports = ARRAY_SIZE(sport_service_snoc),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_SERVICE_SNOC,
	},
	{
		.id = MSM_BUS_SLAVE_QDSS_STM,
		.slavep = sport_qdss_stm,
		.num_sports = ARRAY_SIZE(sport_qdss_stm),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_QDSS_STM,
	},
};


static struct msm_bus_node_info mmss_noc_info[]  = {
	{
		.id = MSM_BUS_MASTER_GRAPHICS_3D,
		.masterp = mport_gfx3d,
		.num_mports = ARRAY_SIZE(mport_gfx3d),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.hw_sel = MSM_BUS_NOC,
		.perm_mode = NOC_QOS_PERM_MODE_BYPASS,
		.mode = NOC_QOS_MODE_BYPASS,
		.ws = 10000,
		.qport = qports_oxili,
		.mas_hw_id = MAS_GFX3D,
	},
	{
		.id = MSM_BUS_MASTER_JPEG,
		.masterp = mport_jpeg,
		.num_mports = ARRAY_SIZE(mport_jpeg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.hw_sel = MSM_BUS_NOC,
		.perm_mode = NOC_QOS_PERM_MODE_BYPASS,
		.mode = NOC_QOS_MODE_BYPASS,
		.qport = qports_gemini,
		.ws = 10000,
		.mas_hw_id = MAS_JPEG,
	},
	{
		.id = MSM_BUS_MASTER_MDP_PORT0,
		.masterp = mport_mdp,
		.num_mports = ARRAY_SIZE(mport_mdp),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.hw_sel = MSM_BUS_NOC,
		.perm_mode = NOC_QOS_PERM_MODE_BYPASS,
		.mode = NOC_QOS_MODE_BYPASS,
		.qport = qports_mdp,
		.ws = 10000,
		.mas_hw_id = MAS_MDP,
	},
	{
		.id = MSM_BUS_MASTER_VIDEO_P0,
		.masterp = mport_video_port0,
		.num_mports = ARRAY_SIZE(mport_video_port0),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.hw_sel = MSM_BUS_NOC,
		.perm_mode = NOC_QOS_PERM_MODE_BYPASS,
		.mode = NOC_QOS_MODE_BYPASS,
		.ws = 10000,
		.qport = qports_venus_p0,
		.mas_hw_id = MAS_VIDEO_P0,
	},
	{
		.id = MSM_BUS_MASTER_VIDEO_P1,
		.masterp = mport_video_port1,
		.num_mports = ARRAY_SIZE(mport_video_port1),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.hw_sel = MSM_BUS_NOC,
		.perm_mode = NOC_QOS_PERM_MODE_BYPASS,
		.mode = NOC_QOS_MODE_BYPASS,
		.ws = 10000,
		.qport = qports_venus_p1,
		.mas_hw_id = MAS_VIDEO_P1,
	},
	{
		.id = MSM_BUS_MASTER_VFE,
		.masterp = mport_vfe,
		.num_mports = ARRAY_SIZE(mport_vfe),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.hw_sel = MSM_BUS_NOC,
		.perm_mode = NOC_QOS_PERM_MODE_BYPASS,
		.mode = NOC_QOS_MODE_BYPASS,
		.ws = 10000,
		.qport = qports_vfe,
		.mas_hw_id = MAS_VFE,
	},
	{
		.id = MSM_BUS_FAB_CONFIG_NOC,
		.gateway = 1,
		.masterp = mport_gw_cnoc_mnoc_cfg,
		.num_mports = ARRAY_SIZE(mport_gw_cnoc_mnoc_cfg),
		.buswidth = 16,
		.hw_sel = MSM_BUS_RPM,
		.mas_hw_id = MAS_CNOC_MNOC_MMSS_CFG,
	},
	{
		.id = MSM_BUS_FAB_BIMC,
		.gateway = 1,
		.slavep = sport_gw_mmss_bimc,
		.num_sports = ARRAY_SIZE(sport_gw_mmss_bimc),
		.buswidth = 16,
		.hw_sel = MSM_BUS_NOC,
		.slv_hw_id = SLV_MMSS_BIMC,
	},
	{
		.id = MSM_BUS_SLAVE_CAMERA_CFG,
		.slavep = sport_camera_cfg,
		.num_sports = ARRAY_SIZE(sport_camera_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 16,
		.hw_sel = MSM_BUS_NOC,
		.slv_hw_id = SLV_CAMERA_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_DISPLAY_CFG,
		.slavep = sport_display_cfg,
		.num_sports = ARRAY_SIZE(sport_display_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 16,
		.hw_sel = MSM_BUS_NOC,
		.slv_hw_id = SLV_DISPLAY_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_OCMEM_CFG,
		.slavep = sport_ocmem_cfg,
		.num_sports = ARRAY_SIZE(sport_ocmem_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 16,
		.hw_sel = MSM_BUS_NOC,
		.slv_hw_id = SLV_OCMEM_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_CPR_CFG,
		.slavep = sport_cpr_cfg,
		.num_sports = ARRAY_SIZE(sport_cpr_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 16,
		.hw_sel = MSM_BUS_NOC,
		.slv_hw_id = SLV_CPR_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_CPR_XPU_CFG,
		.slavep = sport_cpr_xpu_cfg,
		.num_sports = ARRAY_SIZE(sport_cpr_xpu_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 16,
		.hw_sel = MSM_BUS_NOC,
		.slv_hw_id = SLV_CPR_XPU_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_MISC_CFG,
		.slavep = sport_misc_cfg,
		.num_sports = ARRAY_SIZE(sport_misc_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 16,
		.hw_sel = MSM_BUS_NOC,
		.slv_hw_id = SLV_MISC_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_MISC_XPU_CFG,
		.slavep = sport_misc_xpu_cfg,
		.num_sports = ARRAY_SIZE(sport_misc_xpu_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 16,
		.hw_sel = MSM_BUS_NOC,
		.slv_hw_id = SLV_MISC_XPU_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_VENUS_CFG,
		.slavep = sport_venus_cfg,
		.num_sports = ARRAY_SIZE(sport_venus_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 16,
		.hw_sel = MSM_BUS_NOC,
		.slv_hw_id = SLV_VENUS_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_GRAPHICS_3D_CFG,
		.slavep = sport_gfx3d_cfg,
		.num_sports = ARRAY_SIZE(sport_gfx3d_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 16,
		.hw_sel = MSM_BUS_NOC,
		.slv_hw_id = SLV_GFX3D_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_MMSS_CLK_CFG,
		.slavep = sport_mmss_clk_cfg,
		.num_sports = ARRAY_SIZE(sport_mmss_clk_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 16,
		.hw_sel = MSM_BUS_NOC,
		.slv_hw_id = SLV_MMSS_CLK_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_MMSS_CLK_XPU_CFG,
		.slavep = sport_mmss_clk_xpu_cfg,
		.num_sports = ARRAY_SIZE(sport_mmss_clk_xpu_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 16,
		.hw_sel = MSM_BUS_NOC,
		.slv_hw_id = SLV_MMSS_CLK_XPU_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_MNOC_MPU_CFG,
		.slavep = sport_mnoc_mpu_cfg,
		.num_sports = ARRAY_SIZE(sport_mnoc_mpu_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 16,
		.hw_sel = MSM_BUS_NOC,
		.slv_hw_id = SLV_MNOC_MPU_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_ONOC_MPU_CFG,
		.slavep = sport_onoc_mpu_cfg,
		.num_sports = ARRAY_SIZE(sport_onoc_mpu_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 16,
		.hw_sel = MSM_BUS_NOC,
		.slv_hw_id = SLV_ONOC_MPU_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_SERVICE_MNOC,
		.slavep = sport_service_mnoc,
		.num_sports = ARRAY_SIZE(sport_service_mnoc),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 16,
		.hw_sel = MSM_BUS_NOC,
		.slv_hw_id = SLV_SERVICE_MNOC,
	},
};

static struct msm_bus_node_info bimc_info[]  = {
	{
		.id = MSM_BUS_MASTER_AMPSS_M0,
		.masterp = mport_kmpss_m0,
		.num_mports = ARRAY_SIZE(mport_kmpss_m0),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.hw_sel = MSM_BUS_BIMC,
		.mode = NOC_QOS_MODE_FIXED,
		.qport = qports_kmpss,
		.ws = 10000,
		.mas_hw_id = MAS_APPSS_PROC,
		.prio_rd = 1,
		.prio_wr = 1,
	},
	{
		.id = MSM_BUS_MASTER_AMPSS_M1,
		.masterp = mport_kmpss_m1,
		.num_mports = ARRAY_SIZE(mport_kmpss_m1),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.hw_sel = MSM_BUS_BIMC,
		.mode = NOC_QOS_MODE_FIXED,
		.qport = qports_kmpss,
		.ws = 10000,
		.mas_hw_id = MAS_APPSS_PROC,
		.prio_rd = 1,
		.prio_wr = 1,
	},
	{
		.id = MSM_BUS_MASTER_MSS_PROC,
		.masterp = mport_mss_proc,
		.num_mports = ARRAY_SIZE(mport_mss_proc),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.hw_sel = MSM_BUS_RPM,
		.mas_hw_id = MAS_AMSS_PROC,
	},
	{
		.id = MSM_BUS_FAB_MMSS_NOC,
		.gateway = 1,
		.masterp = mport_gw_mnoc_bimc,
		.num_mports = ARRAY_SIZE(mport_gw_mnoc_bimc),
		.qport = qports_gw_mnoc_bimc,
		.buswidth = 8,
		.ws = 10000,
		.mas_hw_id = MAS_MNOC_BIMC,
		.hw_sel = MSM_BUS_BIMC,
		.mode = NOC_QOS_MODE_BYPASS,
	},
	{
		.id = MSM_BUS_FAB_SYS_NOC,
		.gateway = 1,
		.slavep = sport_gw_bimc_snoc,
		.num_sports = ARRAY_SIZE(sport_gw_bimc_snoc),
		.masterp = mport_gw_snoc_bimc,
		.num_mports = ARRAY_SIZE(mport_gw_snoc_bimc),
		.qport = qports_gw_snoc_bimc,
		.buswidth = 8,
		.ws = 10000,
		.mas_hw_id = MAS_SNOC_BIMC,
		.slv_hw_id = SLV_BIMC_SNOC,
	},
	{
		.id = MSM_BUS_SLAVE_EBI_CH0,
		.slavep = sport_ebi1,
		.num_sports = ARRAY_SIZE(sport_ebi1),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_EBI,
		.mode = NOC_QOS_MODE_BYPASS,
	},
	{
		.id = MSM_BUS_SLAVE_AMPSS_L2,
		.slavep = sport_kmpss_l2,
		.num_sports = ARRAY_SIZE(sport_kmpss_l2),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_APSS_L2,
	},
};

static struct msm_bus_node_info ocmem_noc_info[]  = {
	{
		.id = MSM_BUS_FAB_OCMEM_VNOC,
		.gateway = 1,
		.buswidth = 16,
		.mas_hw_id = MAS_OVNOC_ONOC,
		.slv_hw_id = SLV_ONOC_OVNOC,
	},
	{
		.id = MSM_BUS_MASTER_JPEG_OCMEM,
		.masterp = mport_jpeg_ocmem,
		.num_mports = ARRAY_SIZE(mport_jpeg_ocmem),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.perm_mode = NOC_QOS_PERM_MODE_FIXED,
		.mode = NOC_QOS_MODE_FIXED,
		.qport = qports_gemini_ocmem,
		.mas_hw_id = MAS_JPEG_OCMEM,
		.hw_sel = MSM_BUS_NOC,
	},
	{
		.id = MSM_BUS_MASTER_MDP_OCMEM,
		.masterp = mport_mdp_ocmem,
		.num_mports = ARRAY_SIZE(mport_mdp_ocmem),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.perm_mode = NOC_QOS_PERM_MODE_FIXED,
		.mode = NOC_QOS_MODE_FIXED,
		.mas_hw_id = MAS_MDP_OCMEM,
		.hw_sel = MSM_BUS_NOC,
	},
	{
		.id = MSM_BUS_MASTER_VIDEO_P0_OCMEM,
		.masterp = mport_video_p0_ocmem,
		.num_mports = ARRAY_SIZE(mport_video_p0_ocmem),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.perm_mode = NOC_QOS_PERM_MODE_FIXED,
		.mode = NOC_QOS_MODE_FIXED,
		.qport = qports_venus_p0_ocmem,
		.mas_hw_id = MAS_VIDEO_P0_OCMEM,
		.hw_sel = MSM_BUS_NOC,
	},
	{
		.id = MSM_BUS_MASTER_VIDEO_P1_OCMEM,
		.masterp = mport_video_p1_ocmem,
		.num_mports = ARRAY_SIZE(mport_video_p1_ocmem),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.perm_mode = NOC_QOS_PERM_MODE_FIXED,
		.mode = NOC_QOS_MODE_FIXED,
		.qport = qports_venus_p1_ocmem,
		.mas_hw_id = MAS_VIDEO_P1_OCMEM,
		.hw_sel = MSM_BUS_NOC,
	},
	{
		.id = MSM_BUS_MASTER_VFE_OCMEM,
		.masterp = mport_vfe_ocmem,
		.num_mports = ARRAY_SIZE(mport_vfe_ocmem),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.perm_mode = NOC_QOS_PERM_MODE_FIXED,
		.mode = NOC_QOS_MODE_FIXED,
		.qport = qports_vfe_ocmem,
		.mas_hw_id = MAS_VFE_OCMEM,
		.hw_sel = MSM_BUS_NOC,
		.prio_rd = 1,
		.prio_wr = 1,
	},
	{
		.id = MSM_BUS_MASTER_CNOC_ONOC_CFG,
		.masterp = mport_cnoc_onoc_cfg,
		.num_mports = ARRAY_SIZE(mport_cnoc_onoc_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 16,
		.mas_hw_id = MAS_CNOC_ONOC_CFG,
		.hw_sel = MSM_BUS_NOC,
	},
	{
		.id = MSM_BUS_SLAVE_SERVICE_ONOC,
		.slavep = sport_service_onoc,
		.num_sports = ARRAY_SIZE(sport_service_onoc),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 16,
		.slv_hw_id = SLV_SERVICE_ONOC,
	},
};

static struct msm_bus_node_info periph_noc_info[] = {
	{
		.id = MSM_BUS_MASTER_PNOC_CFG,
		.masterp = mport_pnoc_cfg,
		.num_mports = ARRAY_SIZE(mport_pnoc_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.mas_hw_id = MAS_PNOC_CFG,
	},
	{
		.id = MSM_BUS_MASTER_SDCC_1,
		.masterp = mport_sdcc_1,
		.num_mports = ARRAY_SIZE(mport_sdcc_1),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.mas_hw_id = MAS_SDCC_1,
	},
	{
		.id = MSM_BUS_MASTER_SDCC_3,
		.masterp = mport_sdcc_3,
		.num_mports = ARRAY_SIZE(mport_sdcc_3),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.mas_hw_id = MAS_SDCC_3,
	},
	{
		.id = MSM_BUS_MASTER_SDCC_4,
		.masterp = mport_sdcc_4,
		.num_mports = ARRAY_SIZE(mport_sdcc_4),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.mas_hw_id = MAS_SDCC_4,
	},
	{
		.id = MSM_BUS_MASTER_SDCC_2,
		.masterp = mport_sdcc_2,
		.num_mports = ARRAY_SIZE(mport_sdcc_2),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.mas_hw_id = MAS_SDCC_2,
	},
	{
		.id = MSM_BUS_MASTER_TSIF,
		.masterp = mport_tsif,
		.num_mports = ARRAY_SIZE(mport_tsif),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.mas_hw_id = MAS_TSIF,
	},
	{
		.id = MSM_BUS_MASTER_BAM_DMA,
		.masterp = mport_bam_dma,
		.num_mports = ARRAY_SIZE(mport_bam_dma),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.mas_hw_id = MAS_BAM_DMA,
	},
	{
		.id = MSM_BUS_MASTER_BLSP_2,
		.masterp = mport_blsp_2,
		.num_mports = ARRAY_SIZE(mport_blsp_2),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.mas_hw_id = MAS_BLSP_2,
	},
	{
		.id = MSM_BUS_MASTER_USB_HSIC,
		.masterp = mport_usb_hsic,
		.num_mports = ARRAY_SIZE(mport_usb_hsic),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.mas_hw_id = MAS_USB_HSIC,
	},
	{
		.id = MSM_BUS_MASTER_BLSP_1,
		.masterp = mport_blsp_1,
		.num_mports = ARRAY_SIZE(mport_blsp_1),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.mas_hw_id = MAS_BLSP_1,
	},
	{
		.id = MSM_BUS_MASTER_USB_HS,
		.masterp = mport_usb_hs,
		.num_mports = ARRAY_SIZE(mport_usb_hs),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.mas_hw_id = MAS_USB_HS,
	},
	{
		.id = MSM_BUS_FAB_SYS_NOC,
		.gateway = 1,
		.slavep = sport_gw_pnoc_snoc,
		.num_sports = ARRAY_SIZE(sport_gw_pnoc_snoc),
		.masterp = mport_gw_snoc_pnoc,
		.num_mports = ARRAY_SIZE(mport_gw_snoc_pnoc),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_PNOC_SNOC,
		.mas_hw_id = MAS_SNOC_PNOC,
	},
	{
		.id = MSM_BUS_SLAVE_SDCC_1,
		.slavep = sport_sdcc_1,
		.num_sports = ARRAY_SIZE(sport_sdcc_1),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_SDCC_1,
	},
	{
		.id = MSM_BUS_SLAVE_SDCC_3,
		.slavep = sport_sdcc_3,
		.num_sports = ARRAY_SIZE(sport_sdcc_3),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_SDCC_3,
	},
	{
		.id = MSM_BUS_SLAVE_SDCC_2,
		.slavep = sport_sdcc_2,
		.num_sports = ARRAY_SIZE(sport_sdcc_2),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_SDCC_2,
	},
	{
		.id = MSM_BUS_SLAVE_SDCC_4,
		.slavep = sport_sdcc_4,
		.num_sports = ARRAY_SIZE(sport_sdcc_4),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_SDCC_4,
	},
	{
		.id = MSM_BUS_SLAVE_TSIF,
		.slavep = sport_tsif,
		.num_sports = ARRAY_SIZE(sport_tsif),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_TSIF,
	},
	{
		.id = MSM_BUS_SLAVE_BAM_DMA,
		.slavep = sport_bam_dma,
		.num_sports = ARRAY_SIZE(sport_bam_dma),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_BAM_DMA,
	},
	{
		.id = MSM_BUS_SLAVE_BLSP_2,
		.slavep = sport_blsp_2,
		.num_sports = ARRAY_SIZE(sport_blsp_2),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_BLSP_2,
	},
	{
		.id = MSM_BUS_SLAVE_USB_HSIC,
		.slavep = sport_usb_hsic,
		.num_sports = ARRAY_SIZE(sport_usb_hsic),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_USB_HSIC,
	},
	{
		.id = MSM_BUS_SLAVE_BLSP_1,
		.slavep = sport_blsp_1,
		.num_sports = ARRAY_SIZE(sport_blsp_1),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_BLSP_1,
	},
	{
		.id = MSM_BUS_SLAVE_USB_HS,
		.slavep = sport_usb_hs,
		.num_sports = ARRAY_SIZE(sport_usb_hs),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_USB_HS,
	},
	{
		.id = MSM_BUS_SLAVE_PDM,
		.slavep = sport_pdm,
		.num_sports = ARRAY_SIZE(sport_pdm),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_PDM,
	},
	{
		.id = MSM_BUS_SLAVE_PERIPH_APU_CFG,
		.slavep = sport_periph_apu_cfg,
		.num_sports = ARRAY_SIZE(sport_periph_apu_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_PERIPH_APU_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_PNOC_MPU_CFG,
		.slavep = sport_pnoc_mpu_cfg,
		.num_sports = ARRAY_SIZE(sport_pnoc_mpu_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_MPU_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_PRNG,
		.slavep = sport_prng,
		.num_sports = ARRAY_SIZE(sport_prng),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_PRNG,
	},
	{
		.id = MSM_BUS_SLAVE_SERVICE_PNOC,
		.slavep = sport_service_pnoc,
		.num_sports = ARRAY_SIZE(sport_service_pnoc),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_SERVICE_PNOC,
	},
};

static struct msm_bus_node_info config_noc_info[] = {
	{
		.id = MSM_BUS_MASTER_RPM_INST,
		.masterp = mport_rpm_inst,
		.num_mports = ARRAY_SIZE(mport_rpm_inst),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.mas_hw_id = MAS_RPM_INST,
	},
	{
		.id = MSM_BUS_MASTER_RPM_DATA,
		.masterp = mport_rpm_data,
		.num_mports = ARRAY_SIZE(mport_rpm_data),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.mas_hw_id = MAS_RPM_DATA,
	},
	{
		.id = MSM_BUS_MASTER_RPM_SYS,
		.masterp = mport_rpm_sys,
		.num_mports = ARRAY_SIZE(mport_rpm_sys),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.mas_hw_id = MAS_RPM_SYS,
	},
	{
		.id = MSM_BUS_MASTER_DEHR,
		.masterp = mport_dehr,
		.num_mports = ARRAY_SIZE(mport_dehr),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.mas_hw_id = MAS_DEHR,
	},
	{
		.id = MSM_BUS_MASTER_QDSS_DAP,
		.masterp = mport_qdss_dap,
		.num_mports = ARRAY_SIZE(mport_qdss_dap),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.mas_hw_id = MAS_QDSS_DAP,
	},
	{
		.id = MSM_BUS_MASTER_SPDM,
		.masterp = mport_spdm,
		.num_mports = ARRAY_SIZE(mport_spdm),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.mas_hw_id = MAS_SPDM,
	},
	{
		.id = MSM_BUS_MASTER_TIC,
		.masterp = mport_tic,
		.num_mports = ARRAY_SIZE(mport_tic),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.mas_hw_id = MAS_TIC,
	},
	{
		.id = MSM_BUS_SLAVE_CLK_CTL,
		.slavep = sport_clk_ctl,
		.num_sports = ARRAY_SIZE(sport_clk_ctl),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_CLK_CTL,
	},
	{
		.id = MSM_BUS_SLAVE_CNOC_MSS,
		.slavep = sport_cnoc_mss,
		.num_sports = ARRAY_SIZE(sport_cnoc_mss),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_CNOC_MSS,
	},
	{
		.id = MSM_BUS_SLAVE_SECURITY,
		.slavep = sport_security,
		.num_sports = ARRAY_SIZE(sport_security),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_SECURITY,
	},
	{
		.id = MSM_BUS_SLAVE_TCSR,
		.slavep = sport_tcsr,
		.num_sports = ARRAY_SIZE(sport_tcsr),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_TCSR,
	},
	{
		.id = MSM_BUS_SLAVE_TLMM,
		.slavep = sport_tlmm,
		.num_sports = ARRAY_SIZE(sport_tlmm),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_TLMM,
	},
	{
		.id = MSM_BUS_SLAVE_CRYPTO_0_CFG,
		.slavep = sport_crypto_0_cfg,
		.num_sports = ARRAY_SIZE(sport_crypto_0_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_CRYPTO_0_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_CRYPTO_1_CFG,
		.slavep = sport_crypto_1_cfg,
		.num_sports = ARRAY_SIZE(sport_crypto_1_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_CRYPTO_1_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_IMEM_CFG,
		.slavep = sport_imem_cfg,
		.num_sports = ARRAY_SIZE(sport_imem_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_IMEM_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_MESSAGE_RAM,
		.slavep = sport_message_ram,
		.num_sports = ARRAY_SIZE(sport_message_ram),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_MESSAGE_RAM,
	},
	{
		.id = MSM_BUS_SLAVE_BIMC_CFG,
		.slavep = sport_bimc_cfg,
		.num_sports = ARRAY_SIZE(sport_bimc_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_BIMC_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_BOOT_ROM,
		.slavep = sport_boot_rom,
		.num_sports = ARRAY_SIZE(sport_boot_rom),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_BOOT_ROM,
	},
	{
		.id = MSM_BUS_SLAVE_PMIC_ARB,
		.slavep = sport_pmic_arb,
		.num_sports = ARRAY_SIZE(sport_pmic_arb),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_PMIC_ARB,
	},
	{
		.id = MSM_BUS_SLAVE_SPDM_WRAPPER,
		.slavep = sport_spdm_wrapper,
		.num_sports = ARRAY_SIZE(sport_spdm_wrapper),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_SPDM_WRAPPER,
	},
	{
		.id = MSM_BUS_SLAVE_DEHR_CFG,
		.slavep = sport_dehr_cfg,
		.num_sports = ARRAY_SIZE(sport_dehr_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_DEHR_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_MPM,
		.slavep = sport_mpm,
		.num_sports = ARRAY_SIZE(sport_mpm),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_MPM,
	},
	{
		.id = MSM_BUS_SLAVE_QDSS_CFG,
		.slavep = sport_qdss_cfg,
		.num_sports = ARRAY_SIZE(sport_qdss_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_QDSS_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_RBCPR_CFG,
		.slavep = sport_rbcpr_cfg,
		.num_sports = ARRAY_SIZE(sport_rbcpr_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_RBCPR_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_RBCPR_QDSS_APU_CFG,
		.slavep = sport_rbcpr_qdss_apu_cfg,
		.num_sports = ARRAY_SIZE(sport_rbcpr_qdss_apu_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_RBCPR_QDSS_APU_CFG,
	},
	{
		.id = MSM_BUS_FAB_SYS_NOC,
		.gateway = 1,
		.slavep = sport_gw_cnoc_snoc,
		.num_sports = ARRAY_SIZE(sport_gw_cnoc_snoc),
		.masterp = mport_gw_snoc_cnoc,
		.num_mports = ARRAY_SIZE(mport_gw_snoc_cnoc),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.mas_hw_id = MAS_SNOC_CNOC,
		.slv_hw_id = SLV_CNOC_SNOC,
	},
	{
		.id = MSM_BUS_SLAVE_CNOC_ONOC_CFG,
		.slavep = sport_cnoc_onoc_cfg,
		.num_sports = ARRAY_SIZE(sport_cnoc_onoc_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_CNOC_ONOC_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_CNOC_MNOC_MMSS_CFG,
		.slavep = sport_cnoc_mnoc_mmss_cfg,
		.num_sports = ARRAY_SIZE(sport_cnoc_mnoc_mmss_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_CNOC_MNOC_MMSS_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_CNOC_MNOC_CFG,
		.slavep = sport_cnoc_mnoc_cfg,
		.num_sports = ARRAY_SIZE(sport_cnoc_mnoc_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_CNOC_MNOC_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_PNOC_CFG,
		.slavep = sport_pnoc_cfg,
		.num_sports = ARRAY_SIZE(sport_pnoc_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_PNOC_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_SNOC_MPU_CFG,
		.slavep = sport_snoc_mpu_cfg,
		.num_sports = ARRAY_SIZE(sport_snoc_mpu_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_SNOC_MPU_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_SNOC_CFG,
		.slavep = sport_snoc_cfg,
		.num_sports = ARRAY_SIZE(sport_snoc_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_SNOC_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_EBI1_DLL_CFG,
		.slavep = sport_ebi1_dll_cfg,
		.num_sports = ARRAY_SIZE(sport_ebi1_dll_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_EBI1_DLL_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_PHY_APU_CFG,
		.slavep = sport_phy_apu_cfg,
		.num_sports = ARRAY_SIZE(sport_phy_apu_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_PHY_APU_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_EBI1_PHY_CFG,
		.slavep = sport_ebi1_phy_cfg,
		.num_sports = ARRAY_SIZE(sport_ebi1_phy_cfg),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_EBI1_PHY_CFG,
	},
	{
		.id = MSM_BUS_SLAVE_RPM,
		.slavep = sport_rpm,
		.num_sports = ARRAY_SIZE(sport_rpm),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_RPM,
	},
	{
		.id = MSM_BUS_SLAVE_SERVICE_CNOC,
		.slavep = sport_service_cnoc,
		.num_sports = ARRAY_SIZE(sport_service_cnoc),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.slv_hw_id = SLV_SERVICE_CNOC,
	},
};

/* A virtual NoC is needed for connection to OCMEM */
static struct msm_bus_node_info ocmem_vnoc_info[] = {
	{
		.id = MSM_BUS_MASTER_V_OCMEM_GFX3D,
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 8,
		.mas_hw_id = MAS_V_OCMEM_GFX3D,
	},
	{
		.id = MSM_BUS_SLAVE_OCMEM,
		.slavep = sport_ocmem,
		.num_sports = ARRAY_SIZE(sport_ocmem),
		.tier = tier2,
		.num_tiers = ARRAY_SIZE(tier2),
		.buswidth = 16,
		.slv_hw_id = SLV_OCMEM,
		.tier = tier2,
		.slaveclk[DUAL_CTX] = "ocmem_clk",
		.slaveclk[ACTIVE_CTX] = "ocmem_a_clk",
	},
	{
		.id = MSM_BUS_FAB_SYS_NOC,
		.gateway = 1,
		.buswidth = 8,
		.ws = 10000,
		.mas_hw_id = MAS_SNOC_OVNOC,
		.slv_hw_id = SLV_OVNOC_SNOC,
	},
	{
		.id = MSM_BUS_FAB_OCMEM_NOC,
		.gateway = 1,
		.buswidth = 16,
		.ws = 10000,
		.mas_hw_id = MAS_ONOC_OVNOC,
		.slv_hw_id = SLV_OVNOC_ONOC,
	},
};

static void msm_bus_board_assign_iids(struct msm_bus_fabric_registration
	*fabreg, int fabid)
{
	int i;
	for (i = 0; i < fabreg->len; i++) {
		if (!fabreg->info[i].gateway) {
			fabreg->info[i].priv_id = fabid + fabreg->info[i].id;
			if (fabreg->info[i].id < SLAVE_ID_KEY) {
				WARN(fabreg->info[i].id >= NMASTERS,
					"id %d exceeds array size!\n",
					fabreg->info[i].id);
				master_iids[fabreg->info[i].id] =
					fabreg->info[i].priv_id;
			} else {
				WARN((fabreg->info[i].id - SLAVE_ID_KEY) >=
					NSLAVES, "id %d exceeds array size!\n",
					fabreg->info[i].id);
				slave_iids[fabreg->info[i].id - (SLAVE_ID_KEY)]
					= fabreg->info[i].priv_id;
			}
		} else {
			fabreg->info[i].priv_id = fabreg->info[i].id;
		}
	}
}

static int msm_bus_board_8974_get_iid(int id)
{
	if ((id < SLAVE_ID_KEY && id >= NMASTERS) ||
		id >= (SLAVE_ID_KEY + NSLAVES)) {
		MSM_BUS_ERR("Cannot get iid. Invalid id %d passed\n", id);
		return -EINVAL;
	}

	return CHECK_ID(((id < SLAVE_ID_KEY) ? master_iids[id] :
		slave_iids[id - SLAVE_ID_KEY]), id);
}

int msm_bus_board_rpm_get_il_ids(uint16_t *id)
{
	return -ENXIO;
}

static struct msm_bus_board_algorithm msm_bus_board_algo = {
	.board_nfab = NFAB_8974,
	.get_iid = msm_bus_board_8974_get_iid,
	.assign_iids = msm_bus_board_assign_iids,
};

struct msm_bus_fabric_registration msm_bus_8974_sys_noc_pdata = {
	.id = MSM_BUS_FAB_SYS_NOC,
	.name = "msm_sys_noc",
	.info = sys_noc_info,
	.len = ARRAY_SIZE(sys_noc_info),
	.ahb = 0,
	.fabclk[DUAL_CTX] = "bus_clk",
	.fabclk[ACTIVE_CTX] = "bus_a_clk",
	.nmasters = 15,
	.nslaves = 12,
	.ntieredslaves = 0,
	.board_algo = &msm_bus_board_algo,
	.qos_freq = 4800,
	.hw_sel = MSM_BUS_NOC,
	.rpm_enabled = 1,
};

struct msm_bus_fabric_registration msm_bus_8974_mmss_noc_pdata = {
	.id = MSM_BUS_FAB_MMSS_NOC,
	.name = "msm_mmss_noc",
	.info = mmss_noc_info,
	.len = ARRAY_SIZE(mmss_noc_info),
	.ahb = 0,
	.fabclk[DUAL_CTX] = "bus_clk",
	.fabclk[ACTIVE_CTX] = "bus_a_clk",
	.nmasters = 9,
	.nslaves = 16,
	.ntieredslaves = 0,
	.board_algo = &msm_bus_board_algo,
	.qos_freq = 4800,
	.hw_sel = MSM_BUS_NOC,
	.rpm_enabled = 1,
};

struct msm_bus_fabric_registration msm_bus_8974_bimc_pdata = {
	.id = MSM_BUS_FAB_BIMC,
	.name = "msm_bimc",
	.info = bimc_info,
	.len = ARRAY_SIZE(bimc_info),
	.ahb = 0,
	.fabclk[DUAL_CTX] = "mem_clk",
	.fabclk[ACTIVE_CTX] = "mem_a_clk",
	.nmasters = 7,
	.nslaves = 4,
	.ntieredslaves = 0,
	.board_algo = &msm_bus_board_algo,
	.qos_freq = 4800,
	.hw_sel = MSM_BUS_BIMC,
	.rpm_enabled = 1,
};

struct msm_bus_fabric_registration msm_bus_8974_ocmem_noc_pdata = {
	.id = MSM_BUS_FAB_OCMEM_NOC,
	.name = "msm_ocmem_noc",
	.info = ocmem_noc_info,
	.len = ARRAY_SIZE(ocmem_noc_info),
	.ahb = 0,
	.fabclk[DUAL_CTX] = "bus_clk",
	.fabclk[ACTIVE_CTX] = "bus_a_clk",
	.nmasters = 6,
	.nslaves = 3,
	.ntieredslaves = 0,
	.board_algo = &msm_bus_board_algo,
	.qos_freq = 4800,
	.hw_sel = MSM_BUS_NOC,
	.rpm_enabled = 1,
};

struct msm_bus_fabric_registration msm_bus_8974_periph_noc_pdata = {
	.id = MSM_BUS_FAB_PERIPH_NOC,
	.name = "msm_periph_noc",
	.info = periph_noc_info,
	.len = ARRAY_SIZE(periph_noc_info),
	.ahb = 0,
	.fabclk[DUAL_CTX] = "bus_clk",
	.fabclk[ACTIVE_CTX] = "bus_a_clk",
	.nmasters = 12,
	.nslaves = 16,
	.ntieredslaves = 0,
	.board_algo = &msm_bus_board_algo,
	.hw_sel = MSM_BUS_NOC,
	.rpm_enabled = 1,
};

struct msm_bus_fabric_registration msm_bus_8974_config_noc_pdata = {
	.id = MSM_BUS_FAB_CONFIG_NOC,
	.name = "msm_config_noc",
	.info = config_noc_info,
	.len = ARRAY_SIZE(config_noc_info),
	.ahb = 0,
	.fabclk[DUAL_CTX] = "bus_clk",
	.fabclk[ACTIVE_CTX] = "bus_a_clk",
	.nmasters = 8,
	.nslaves = 30,
	.ntieredslaves = 0,
	.board_algo = &msm_bus_board_algo,
	.hw_sel = MSM_BUS_NOC,
	.rpm_enabled = 1,
};

struct msm_bus_fabric_registration msm_bus_8974_ocmem_vnoc_pdata = {
	.id = MSM_BUS_FAB_OCMEM_VNOC,
	.name = "msm_ocmem_vnoc",
	.info = ocmem_vnoc_info,
	.len = ARRAY_SIZE(ocmem_vnoc_info),
	.ahb = 0,
	.nmasters = 5,
	.nslaves = 4,
	.ntieredslaves = 0,
	.board_algo = &msm_bus_board_algo,
	.hw_sel = MSM_BUS_NOC,
	.virt = 1,
	.rpm_enabled = 1,
};

void msm_bus_board_init(struct msm_bus_fabric_registration *pdata)
{
	pdata->board_algo = &msm_bus_board_algo;
}

void msm_bus_board_set_nfab(struct msm_bus_fabric_registration *pdata,
	int nfab)
{
	if (nfab <= 0)
		return;

	msm_bus_board_algo.board_nfab = nfab;
}
