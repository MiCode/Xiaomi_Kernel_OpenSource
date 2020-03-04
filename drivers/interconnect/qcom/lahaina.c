// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 *
 */

#include <asm/div64.h>
#include <dt-bindings/interconnect/qcom,lahaina.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/interconnect.h>
#include <linux/interconnect-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/sort.h>

#include "icc-rpmh.h"
#include "bcm-voter.h"
#include "qnoc-qos.h"

static LIST_HEAD(qnoc_probe_list);
static DEFINE_MUTEX(probe_list_lock);

static int probe_count;

DEFINE_QNODE_QOS(qhm_qspi, 2, 0, 1, 65536);
DEFINE_QNODE_QOS(qhm_qup1, 2, 0, 1, 69632);
DEFINE_QNODE_QOS(xm_sdc4, 2, 0, 1, 73728);
DEFINE_QNODE_QOS(xm_ufs_mem, 2, 0, 1, 77824);
DEFINE_QNODE_QOS(xm_usb3_0, 2, 0, 1, 81920);
DEFINE_QNODE_QOS(xm_usb3_1, 2, 0, 1, 86016);
DEFINE_QNODE_QOS(qhm_qdss_bam, 2, 0, 1, 98304);
DEFINE_QNODE_QOS(qhm_qup0, 2, 0, 1, 102400);
DEFINE_QNODE_QOS(qhm_qup2, 2, 0, 1, 106496);
DEFINE_QNODE_QOS(qxm_crypto, 2, 1, 1, 118784);
DEFINE_QNODE_QOS(qxm_ipa, 2, 1, 0, 65536);
DEFINE_QNODE_QOS(xm_pcie3_0, 2, 0, 1, 77824);
DEFINE_QNODE_QOS(xm_pcie3_1, 2, 0, 1, 81920);
DEFINE_QNODE_QOS(xm_qdss_etr, 2, 0, 1, 86016);
DEFINE_QNODE_QOS(xm_sdc2, 2, 0, 1, 90112);
DEFINE_QNODE_QOS(xm_ufs_card, 2, 0, 1, 94208);
DEFINE_QNODE_QOS(alm_gpu_tcu, 1, 0, 1, 655360);
DEFINE_QNODE_QOS(alm_sys_tcu, 6, 0, 1, 659456);
DEFINE_QNODE_QOS(qnm_cmpnoc, 0, 1, 2, 135168, 397312);
DEFINE_QNODE_QOS(qnm_gpu, 0, 0, 2, 139264, 401408);
DEFINE_QNODE_QOS(qnm_mnoc_hf, 0, 1, 1, 143360, 405504);
DEFINE_QNODE_QOS(qnm_mnoc_sf, 0, 1, 1, 147456, 409600);
DEFINE_QNODE_QOS(qnm_pcie, 2, 1, 1, 663552);
DEFINE_QNODE_QOS(qnm_snoc_gc, 0, 1, 1, 667648);
DEFINE_QNODE_QOS(qnm_snoc_sf, 0, 1, 1, 671744);
DEFINE_QNODE_QOS(qnm_camnoc_hf, 0, 1, 2, 65536, 65920);
DEFINE_QNODE_QOS(qnm_camnoc_icp, 5, 1, 1, 69632);
DEFINE_QNODE_QOS(qnm_camnoc_sf, 0, 1, 2, 73728, 73856);
DEFINE_QNODE_QOS(qnm_video0, 0, 1, 1, 81920);
DEFINE_QNODE_QOS(qnm_video1, 0, 1, 1, 82048);
DEFINE_QNODE_QOS(qnm_video_cvp, 0, 1, 1, 86016);
DEFINE_QNODE_QOS(qxm_mdp0, 0, 1, 1, 90112);
DEFINE_QNODE_QOS(qxm_mdp1, 0, 1, 1, 90240);
DEFINE_QNODE_QOS(qxm_rot, 0, 1, 1, 94208);
DEFINE_QNODE_QOS(qxm_pimem, 2, 1, 1, 45056);
DEFINE_QNODE_QOS(xm_gic, 2, 0, 1, 53248);

DEFINE_QNODE(qhm_qspi, MASTER_QSPI_0, 1, 4, &qhm_qspi_qos, 1,
		SLAVE_A1NOC_SNOC);
DEFINE_QNODE(qhm_qup1, MASTER_QUP_1, 1, 4, &qhm_qup1_qos, 1,
		SLAVE_A1NOC_SNOC);
DEFINE_QNODE(qnm_a1noc_cfg, MASTER_A1NOC_CFG, 1, 4, NULL, 1,
		SLAVE_SERVICE_A1NOC);
DEFINE_QNODE(xm_sdc4, MASTER_SDCC_4, 1, 8, &xm_sdc4_qos, 1,
		SLAVE_A1NOC_SNOC);
DEFINE_QNODE(xm_ufs_mem, MASTER_UFS_MEM, 1, 8, &xm_ufs_mem_qos, 1,
		SLAVE_A1NOC_SNOC);
DEFINE_QNODE(xm_usb3_0, MASTER_USB3_0, 1, 8, &xm_usb3_0_qos, 1,
		SLAVE_A1NOC_SNOC);
DEFINE_QNODE(xm_usb3_1, MASTER_USB3_1, 1, 8, &xm_usb3_1_qos, 1,
		SLAVE_A1NOC_SNOC);
DEFINE_QNODE(qhm_qdss_bam, MASTER_QDSS_BAM, 1, 4, &qhm_qdss_bam_qos, 1,
		SLAVE_A2NOC_SNOC);
DEFINE_QNODE(qhm_qup0, MASTER_QUP_0, 1, 4, &qhm_qup0_qos, 1,
		SLAVE_A2NOC_SNOC);
DEFINE_QNODE(qhm_qup2, MASTER_QUP_2, 1, 4, &qhm_qup2_qos, 1,
		SLAVE_A2NOC_SNOC);
DEFINE_QNODE(qnm_a2noc_cfg, MASTER_A2NOC_CFG, 1, 4, NULL, 1,
		SLAVE_SERVICE_A2NOC);
DEFINE_QNODE(qxm_crypto, MASTER_CRYPTO, 1, 8, &qxm_crypto_qos, 1,
		SLAVE_A2NOC_SNOC);
DEFINE_QNODE(qxm_ipa, MASTER_IPA, 1, 8, &qxm_ipa_qos, 1,
		SLAVE_A2NOC_SNOC);
DEFINE_QNODE(xm_pcie3_0, MASTER_PCIE_0, 1, 8, &xm_pcie3_0_qos, 1,
		SLAVE_ANOC_PCIE_GEM_NOC);
DEFINE_QNODE(xm_pcie3_1, MASTER_PCIE_1, 1, 8, &xm_pcie3_1_qos, 1,
		SLAVE_ANOC_PCIE_GEM_NOC);
DEFINE_QNODE(xm_qdss_etr, MASTER_QDSS_ETR, 1, 8, &xm_qdss_etr_qos, 1,
		SLAVE_A2NOC_SNOC);
DEFINE_QNODE(xm_sdc2, MASTER_SDCC_2, 1, 8, &xm_sdc2_qos, 1,
		SLAVE_A2NOC_SNOC);
DEFINE_QNODE(xm_ufs_card, MASTER_UFS_CARD, 1, 8, &xm_ufs_card_qos, 1,
		SLAVE_A2NOC_SNOC);
DEFINE_QNODE(qup0_core_master, MASTER_QUP_CORE_0, 1, 4, NULL, 1,
		SLAVE_QUP_CORE_0);
DEFINE_QNODE(qup1_core_master, MASTER_QUP_CORE_1, 1, 4, NULL, 1,
		SLAVE_QUP_CORE_1);
DEFINE_QNODE(qup2_core_master, MASTER_QUP_CORE_2, 1, 4, NULL, 1,
		SLAVE_QUP_CORE_2);
DEFINE_QNODE(qnm_gemnoc_cnoc, MASTER_GEM_NOC_CNOC, 1, 16, NULL, 56,
		SLAVE_AHB2PHY_SOUTH, SLAVE_AHB2PHY_NORTH,
		SLAVE_AOSS, SLAVE_APPSS,
		SLAVE_CAMERA_CFG, SLAVE_CLK_CTL,
		SLAVE_CDSP_CFG, SLAVE_RBCPR_CX_CFG,
		SLAVE_RBCPR_MMCX_CFG, SLAVE_RBCPR_MX_CFG,
		SLAVE_CRYPTO_0_CFG, SLAVE_CX_RDPM,
		SLAVE_DCC_CFG, SLAVE_DISPLAY_CFG,
		SLAVE_GFX3D_CFG, SLAVE_HWKM,
		SLAVE_IMEM_CFG, SLAVE_IPA_CFG,
		SLAVE_IPC_ROUTER_CFG, SLAVE_LPASS,
		SLAVE_CNOC_MSS, SLAVE_MX_RDPM,
		SLAVE_PCIE_0_CFG, SLAVE_PCIE_1_CFG,
		SLAVE_PDM, SLAVE_PIMEM_CFG,
		SLAVE_PKA_WRAPPER_CFG, SLAVE_PMU_WRAPPER_CFG,
		SLAVE_QDSS_CFG, SLAVE_QSPI_0,
		SLAVE_QUP_0, SLAVE_QUP_1,
		SLAVE_QUP_2, SLAVE_SDCC_2,
		SLAVE_SDCC_4, SLAVE_SECURITY,
		SLAVE_SPSS_CFG, SLAVE_TCSR,
		SLAVE_TLMM, SLAVE_UFS_CARD_CFG,
		SLAVE_UFS_MEM_CFG, SLAVE_USB3_0,
		SLAVE_USB3_1, SLAVE_VENUS_CFG,
		SLAVE_VSENSE_CTRL_CFG, SLAVE_A1NOC_CFG,
		SLAVE_A2NOC_CFG, SLAVE_DDRSS_CFG,
		SLAVE_CNOC_MNOC_CFG, SLAVE_SNOC_CFG,
		SLAVE_BOOT_IMEM, SLAVE_IMEM,
		SLAVE_PIMEM, SLAVE_SERVICE_CNOC,
		SLAVE_QDSS_STM, SLAVE_TCU);
DEFINE_QNODE(qnm_gemnoc_pcie, MASTER_GEM_NOC_PCIE_SNOC, 1, 8, NULL, 2,
		SLAVE_PCIE_0, SLAVE_PCIE_1);
DEFINE_QNODE(xm_qdss_dap, MASTER_QDSS_DAP, 1, 8, NULL, 56,
		SLAVE_AHB2PHY_SOUTH, SLAVE_AHB2PHY_NORTH,
		SLAVE_AOSS, SLAVE_APPSS,
		SLAVE_CAMERA_CFG, SLAVE_CLK_CTL,
		SLAVE_CDSP_CFG, SLAVE_RBCPR_CX_CFG,
		SLAVE_RBCPR_MMCX_CFG, SLAVE_RBCPR_MX_CFG,
		SLAVE_CRYPTO_0_CFG, SLAVE_CX_RDPM,
		SLAVE_DCC_CFG, SLAVE_DISPLAY_CFG,
		SLAVE_GFX3D_CFG, SLAVE_HWKM,
		SLAVE_IMEM_CFG, SLAVE_IPA_CFG,
		SLAVE_IPC_ROUTER_CFG, SLAVE_LPASS,
		SLAVE_CNOC_MSS, SLAVE_MX_RDPM,
		SLAVE_PCIE_0_CFG, SLAVE_PCIE_1_CFG,
		SLAVE_PDM, SLAVE_PIMEM_CFG,
		SLAVE_PKA_WRAPPER_CFG, SLAVE_PMU_WRAPPER_CFG,
		SLAVE_QDSS_CFG, SLAVE_QSPI_0,
		SLAVE_QUP_0, SLAVE_QUP_1,
		SLAVE_QUP_2, SLAVE_SDCC_2,
		SLAVE_SDCC_4, SLAVE_SECURITY,
		SLAVE_SPSS_CFG, SLAVE_TCSR,
		SLAVE_TLMM, SLAVE_UFS_CARD_CFG,
		SLAVE_UFS_MEM_CFG, SLAVE_USB3_0,
		SLAVE_USB3_1, SLAVE_VENUS_CFG,
		SLAVE_VSENSE_CTRL_CFG, SLAVE_A1NOC_CFG,
		SLAVE_A2NOC_CFG, SLAVE_DDRSS_CFG,
		SLAVE_CNOC_MNOC_CFG, SLAVE_SNOC_CFG,
		SLAVE_BOOT_IMEM, SLAVE_IMEM,
		SLAVE_PIMEM, SLAVE_SERVICE_CNOC,
		SLAVE_QDSS_STM, SLAVE_TCU);
DEFINE_QNODE(qnm_cnoc_dc_noc, MASTER_CNOC_DC_NOC, 1, 4, NULL, 2,
		SLAVE_LLCC_CFG, SLAVE_GEM_NOC_CFG);
DEFINE_QNODE(alm_gpu_tcu, MASTER_GPU_TCU, 1, 8, &alm_gpu_tcu_qos, 2,
		SLAVE_GEM_NOC_CNOC, SLAVE_LLCC);
DEFINE_QNODE(alm_sys_tcu, MASTER_SYS_TCU, 1, 8, &alm_sys_tcu_qos, 2,
		SLAVE_GEM_NOC_CNOC, SLAVE_LLCC);
DEFINE_QNODE(chm_apps, MASTER_APPSS_PROC, 2, 32, NULL, 3,
		SLAVE_GEM_NOC_CNOC, SLAVE_LLCC,
		SLAVE_MEM_NOC_PCIE_SNOC);
DEFINE_QNODE(qnm_cmpnoc, MASTER_COMPUTE_NOC, 2, 32, &qnm_cmpnoc_qos, 2,
		SLAVE_GEM_NOC_CNOC, SLAVE_LLCC);
DEFINE_QNODE(qnm_gemnoc_cfg, MASTER_GEM_NOC_CFG, 1, 4, NULL, 5,
		SLAVE_MSS_PROC_MS_MPU_CFG, SLAVE_MCDMA_MS_MPU_CFG,
		SLAVE_SERVICE_GEM_NOC_1, SLAVE_SERVICE_GEM_NOC_2,
		SLAVE_SERVICE_GEM_NOC);
DEFINE_QNODE(qnm_gpu, MASTER_GFX3D, 2, 32, &qnm_gpu_qos, 2,
		SLAVE_GEM_NOC_CNOC, SLAVE_LLCC);
DEFINE_QNODE(qnm_mnoc_hf, MASTER_MNOC_HF_MEM_NOC, 2, 32, &qnm_mnoc_hf_qos, 1,
		SLAVE_LLCC);
DEFINE_QNODE(qnm_mnoc_sf, MASTER_MNOC_SF_MEM_NOC, 2, 32, &qnm_mnoc_sf_qos, 2,
		SLAVE_GEM_NOC_CNOC, SLAVE_LLCC);
DEFINE_QNODE(qnm_pcie, MASTER_ANOC_PCIE_GEM_NOC, 1, 16, &qnm_pcie_qos, 2,
		SLAVE_GEM_NOC_CNOC, SLAVE_LLCC);
DEFINE_QNODE(qnm_snoc_gc, MASTER_SNOC_GC_MEM_NOC, 1, 8, &qnm_snoc_gc_qos, 1,
		SLAVE_LLCC);
DEFINE_QNODE(qnm_snoc_sf, MASTER_SNOC_SF_MEM_NOC, 1, 16, &qnm_snoc_sf_qos, 3,
		SLAVE_GEM_NOC_CNOC, SLAVE_LLCC,
		SLAVE_MEM_NOC_PCIE_SNOC);
DEFINE_QNODE(qhm_config_noc, MASTER_CNOC_LPASS_AG_NOC, 1, 4, NULL, 6,
		SLAVE_LPASS_CORE_CFG, SLAVE_LPASS_LPI_CFG,
		SLAVE_LPASS_MPU_CFG, SLAVE_LPASS_TOP_CFG,
		SLAVE_SERVICES_LPASS_AML_NOC, SLAVE_SERVICE_LPASS_AG_NOC);
DEFINE_QNODE(llcc_mc, MASTER_LLCC, 4, 4, NULL, 1,
		SLAVE_EBI1);
DEFINE_QNODE(qnm_camnoc_hf, MASTER_CAMNOC_HF, 2, 32, &qnm_camnoc_hf_qos, 1,
		SLAVE_MNOC_HF_MEM_NOC);
DEFINE_QNODE(qnm_camnoc_icp, MASTER_CAMNOC_ICP, 1, 8, &qnm_camnoc_icp_qos, 1,
		SLAVE_MNOC_SF_MEM_NOC);
DEFINE_QNODE(qnm_camnoc_sf, MASTER_CAMNOC_SF, 2, 32, &qnm_camnoc_sf_qos, 1,
		SLAVE_MNOC_SF_MEM_NOC);
DEFINE_QNODE(qnm_mnoc_cfg, MASTER_CNOC_MNOC_CFG, 1, 4, NULL, 1,
		SLAVE_SERVICE_MNOC);
DEFINE_QNODE(qnm_video0, MASTER_VIDEO_P0, 1, 32, &qnm_video0_qos, 1,
		SLAVE_MNOC_SF_MEM_NOC);
DEFINE_QNODE(qnm_video1, MASTER_VIDEO_P1, 1, 32, &qnm_video1_qos, 1,
		SLAVE_MNOC_SF_MEM_NOC);
DEFINE_QNODE(qnm_video_cvp, MASTER_VIDEO_PROC, 1, 32, &qnm_video_cvp_qos, 1,
		SLAVE_MNOC_SF_MEM_NOC);
DEFINE_QNODE(qxm_mdp0, MASTER_MDP0, 1, 32, &qxm_mdp0_qos, 1,
		SLAVE_MNOC_HF_MEM_NOC);
DEFINE_QNODE(qxm_mdp1, MASTER_MDP1, 1, 32, &qxm_mdp1_qos, 1,
		SLAVE_MNOC_HF_MEM_NOC);
DEFINE_QNODE(qxm_rot, MASTER_ROTATOR, 1, 32, &qxm_rot_qos, 1,
		SLAVE_MNOC_SF_MEM_NOC);
DEFINE_QNODE(qhm_nsp_noc_config, MASTER_CDSP_NOC_CFG, 1, 4, NULL, 1,
		SLAVE_SERVICE_NSP_NOC);
DEFINE_QNODE(qxm_nsp, MASTER_CDSP_PROC, 2, 32, NULL, 1,
		SLAVE_CDSP_MEM_NOC);
DEFINE_QNODE(qnm_aggre1_noc, MASTER_A1NOC_SNOC, 1, 16, NULL, 1,
		SLAVE_SNOC_GEM_NOC_SF);
DEFINE_QNODE(qnm_aggre2_noc, MASTER_A2NOC_SNOC, 1, 16, NULL, 1,
		SLAVE_SNOC_GEM_NOC_SF);
DEFINE_QNODE(qnm_snoc_cfg, MASTER_SNOC_CFG, 1, 4, NULL, 1,
		SLAVE_SERVICE_SNOC);
DEFINE_QNODE(qxm_pimem, MASTER_PIMEM, 1, 8, &qxm_pimem_qos, 1,
		SLAVE_SNOC_GEM_NOC_GC);
DEFINE_QNODE(xm_gic, MASTER_GIC, 1, 8, &xm_gic_qos, 1,
		SLAVE_SNOC_GEM_NOC_GC);
DEFINE_QNODE(qnm_mnoc_hf_disp, MASTER_MNOC_HF_MEM_NOC_DISP, 2, 32, NULL, 1,
		SLAVE_LLCC_DISP);
DEFINE_QNODE(qnm_mnoc_sf_disp, MASTER_MNOC_SF_MEM_NOC_DISP, 2, 32, NULL, 1,
		SLAVE_LLCC_DISP);
DEFINE_QNODE(llcc_mc_disp, MASTER_LLCC_DISP, 4, 4, NULL, 1,
		SLAVE_EBI1_DISP);
DEFINE_QNODE(qxm_mdp0_disp, MASTER_MDP0_DISP, 1, 32, NULL, 1,
		SLAVE_MNOC_HF_MEM_NOC_DISP);
DEFINE_QNODE(qxm_mdp1_disp, MASTER_MDP1_DISP, 1, 32, NULL, 1,
		SLAVE_MNOC_HF_MEM_NOC_DISP);
DEFINE_QNODE(qxm_rot_disp, MASTER_ROTATOR_DISP, 1, 32, NULL, 1,
		SLAVE_MNOC_SF_MEM_NOC_DISP);

DEFINE_QNODE(qns_a1noc_snoc, SLAVE_A1NOC_SNOC, 1, 16, NULL, 1,
		MASTER_A1NOC_SNOC);
DEFINE_QNODE(srvc_aggre1_noc, SLAVE_SERVICE_A1NOC, 1, 4, NULL, 0);
DEFINE_QNODE(qns_a2noc_snoc, SLAVE_A2NOC_SNOC, 1, 16, NULL, 1,
		MASTER_A2NOC_SNOC);
DEFINE_QNODE(qns_pcie_mem_noc, SLAVE_ANOC_PCIE_GEM_NOC, 1, 16, NULL, 1,
		MASTER_ANOC_PCIE_GEM_NOC);
DEFINE_QNODE(srvc_aggre2_noc, SLAVE_SERVICE_A2NOC, 1, 4, NULL, 0);
DEFINE_QNODE(qup0_core_slave, SLAVE_QUP_CORE_0, 1, 4, NULL, 0);
DEFINE_QNODE(qup1_core_slave, SLAVE_QUP_CORE_1, 1, 4, NULL, 0);
DEFINE_QNODE(qup2_core_slave, SLAVE_QUP_CORE_2, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_ahb2phy0, SLAVE_AHB2PHY_SOUTH, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_ahb2phy1, SLAVE_AHB2PHY_NORTH, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_aoss, SLAVE_AOSS, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_apss, SLAVE_APPSS, 1, 8, NULL, 0);
DEFINE_QNODE(qhs_camera_cfg, SLAVE_CAMERA_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_clk_ctl, SLAVE_CLK_CTL, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_compute_cfg, SLAVE_CDSP_CFG, 1, 4, NULL, 1,
		MASTER_CDSP_NOC_CFG);
DEFINE_QNODE(qhs_cpr_cx, SLAVE_RBCPR_CX_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_cpr_mmcx, SLAVE_RBCPR_MMCX_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_cpr_mx, SLAVE_RBCPR_MX_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_crypto0_cfg, SLAVE_CRYPTO_0_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_cx_rdpm, SLAVE_CX_RDPM, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_dcc_cfg, SLAVE_DCC_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_display_cfg, SLAVE_DISPLAY_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_gpuss_cfg, SLAVE_GFX3D_CFG, 1, 8, NULL, 0);
DEFINE_QNODE(qhs_hwkm, SLAVE_HWKM, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_imem_cfg, SLAVE_IMEM_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_ipa, SLAVE_IPA_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_ipc_router, SLAVE_IPC_ROUTER_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_lpass_cfg, SLAVE_LPASS, 1, 4, NULL, 1,
		MASTER_CNOC_LPASS_AG_NOC);
DEFINE_QNODE(qhs_mss_cfg, SLAVE_CNOC_MSS, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_mx_rdpm, SLAVE_MX_RDPM, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_pcie0_cfg, SLAVE_PCIE_0_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_pcie1_cfg, SLAVE_PCIE_1_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_pdm, SLAVE_PDM, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_pimem_cfg, SLAVE_PIMEM_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_pka_wrapper_cfg, SLAVE_PKA_WRAPPER_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_pmu_wrapper_cfg, SLAVE_PMU_WRAPPER_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_qdss_cfg, SLAVE_QDSS_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_qspi, SLAVE_QSPI_0, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_qup0, SLAVE_QUP_0, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_qup1, SLAVE_QUP_1, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_qup2, SLAVE_QUP_2, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_sdc2, SLAVE_SDCC_2, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_sdc4, SLAVE_SDCC_4, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_security, SLAVE_SECURITY, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_spss_cfg, SLAVE_SPSS_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_tcsr, SLAVE_TCSR, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_tlmm, SLAVE_TLMM, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_ufs_card_cfg, SLAVE_UFS_CARD_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_ufs_mem_cfg, SLAVE_UFS_MEM_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_usb3_0, SLAVE_USB3_0, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_usb3_1, SLAVE_USB3_1, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_venus_cfg, SLAVE_VENUS_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_vsense_ctrl_cfg, SLAVE_VSENSE_CTRL_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qns_a1_noc_cfg, SLAVE_A1NOC_CFG, 1, 4, NULL, 1,
		MASTER_A1NOC_CFG);
DEFINE_QNODE(qns_a2_noc_cfg, SLAVE_A2NOC_CFG, 1, 4, NULL, 1,
		MASTER_A2NOC_CFG);
DEFINE_QNODE(qns_ddrss_cfg, SLAVE_DDRSS_CFG, 1, 4, NULL, 1,
		MASTER_CNOC_DC_NOC);
DEFINE_QNODE(qns_mnoc_cfg, SLAVE_CNOC_MNOC_CFG, 1, 4, NULL, 1,
		MASTER_CNOC_MNOC_CFG);
DEFINE_QNODE(qns_snoc_cfg, SLAVE_SNOC_CFG, 1, 4, NULL, 1,
		MASTER_SNOC_CFG);
DEFINE_QNODE(qxs_boot_imem, SLAVE_BOOT_IMEM, 1, 8, NULL, 0);
DEFINE_QNODE(qxs_imem, SLAVE_IMEM, 1, 8, NULL, 0);
DEFINE_QNODE(qxs_pimem, SLAVE_PIMEM, 1, 8, NULL, 0);
DEFINE_QNODE(srvc_cnoc, SLAVE_SERVICE_CNOC, 1, 4, NULL, 0);
DEFINE_QNODE(xs_pcie_0, SLAVE_PCIE_0, 1, 8, NULL, 0);
DEFINE_QNODE(xs_pcie_1, SLAVE_PCIE_1, 1, 8, NULL, 0);
DEFINE_QNODE(xs_qdss_stm, SLAVE_QDSS_STM, 1, 4, NULL, 0);
DEFINE_QNODE(xs_sys_tcu_cfg, SLAVE_TCU, 1, 8, NULL, 0);
DEFINE_QNODE(qhs_llcc, SLAVE_LLCC_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qns_gemnoc, SLAVE_GEM_NOC_CFG, 1, 4, NULL, 1,
		MASTER_GEM_NOC_CFG);
DEFINE_QNODE(qhs_mdsp_ms_mpu_cfg, SLAVE_MSS_PROC_MS_MPU_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_modem_ms_mpu_cfg, SLAVE_MCDMA_MS_MPU_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qns_gem_noc_cnoc, SLAVE_GEM_NOC_CNOC, 1, 16, NULL, 1,
		MASTER_GEM_NOC_CNOC);
DEFINE_QNODE(qns_llcc, SLAVE_LLCC, 4, 16, NULL, 1,
		MASTER_LLCC);
DEFINE_QNODE(qns_pcie, SLAVE_MEM_NOC_PCIE_SNOC, 1, 8, NULL, 1,
		MASTER_GEM_NOC_PCIE_SNOC);
DEFINE_QNODE(srvc_even_gemnoc, SLAVE_SERVICE_GEM_NOC_1, 1, 4, NULL, 0);
DEFINE_QNODE(srvc_odd_gemnoc, SLAVE_SERVICE_GEM_NOC_2, 1, 4, NULL, 0);
DEFINE_QNODE(srvc_sys_gemnoc, SLAVE_SERVICE_GEM_NOC, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_lpass_core, SLAVE_LPASS_CORE_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_lpass_lpi, SLAVE_LPASS_LPI_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_lpass_mpu, SLAVE_LPASS_MPU_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(qhs_lpass_top, SLAVE_LPASS_TOP_CFG, 1, 4, NULL, 0);
DEFINE_QNODE(srvc_niu_aml_noc, SLAVE_SERVICES_LPASS_AML_NOC, 1, 4, NULL, 0);
DEFINE_QNODE(srvc_niu_lpass_agnoc, SLAVE_SERVICE_LPASS_AG_NOC, 1, 4, NULL, 0);
DEFINE_QNODE(ebi, SLAVE_EBI1, 4, 4, NULL, 0);
DEFINE_QNODE(qns_mem_noc_hf, SLAVE_MNOC_HF_MEM_NOC, 2, 32, NULL, 1,
		MASTER_MNOC_HF_MEM_NOC);
DEFINE_QNODE(qns_mem_noc_sf, SLAVE_MNOC_SF_MEM_NOC, 2, 32, NULL, 1,
		MASTER_MNOC_SF_MEM_NOC);
DEFINE_QNODE(srvc_mnoc, SLAVE_SERVICE_MNOC, 1, 4, NULL, 0);
DEFINE_QNODE(qns_nsp_gemnoc, SLAVE_CDSP_MEM_NOC, 2, 32, NULL, 1,
		MASTER_COMPUTE_NOC);
DEFINE_QNODE(service_nsp_noc, SLAVE_SERVICE_NSP_NOC, 1, 4, NULL, 0);
DEFINE_QNODE(qns_gemnoc_gc, SLAVE_SNOC_GEM_NOC_GC, 1, 8, NULL, 1,
		MASTER_SNOC_GC_MEM_NOC);
DEFINE_QNODE(qns_gemnoc_sf, SLAVE_SNOC_GEM_NOC_SF, 1, 16, NULL, 1,
		MASTER_SNOC_SF_MEM_NOC);
DEFINE_QNODE(srvc_snoc, SLAVE_SERVICE_SNOC, 1, 4, NULL, 0);
DEFINE_QNODE(qns_llcc_disp, SLAVE_LLCC_DISP, 4, 16, NULL, 1,
		MASTER_LLCC_DISP);
DEFINE_QNODE(ebi_disp, SLAVE_EBI1_DISP, 4, 4, NULL, 0);
DEFINE_QNODE(qns_mem_noc_hf_disp, SLAVE_MNOC_HF_MEM_NOC_DISP, 2, 32, NULL, 1,
		MASTER_MNOC_HF_MEM_NOC_DISP);
DEFINE_QNODE(qns_mem_noc_sf_disp, SLAVE_MNOC_SF_MEM_NOC_DISP, 2, 32, NULL, 1,
		MASTER_MNOC_SF_MEM_NOC_DISP);

DEFINE_QBCM(bcm_acv, "ACV", false, 0, 1,
		&ebi);
DEFINE_QBCM(bcm_ce0, "CE0", false, 0, 1,
		&qxm_crypto);
DEFINE_QBCM(bcm_cn0, "CN0", true, 0, 2,
		&qnm_gemnoc_cnoc, &qnm_gemnoc_pcie);
DEFINE_QBCM(bcm_cn1, "CN1", false, 0, 47,
		&xm_qdss_dap, &qhs_ahb2phy0, &qhs_ahb2phy1,
		&qhs_aoss, &qhs_apss, &qhs_camera_cfg,
		&qhs_clk_ctl, &qhs_compute_cfg, &qhs_cpr_cx,
		&qhs_cpr_mmcx, &qhs_cpr_mx, &qhs_crypto0_cfg,
		&qhs_cx_rdpm, &qhs_dcc_cfg, &qhs_display_cfg,
		&qhs_gpuss_cfg, &qhs_hwkm, &qhs_imem_cfg,
		&qhs_ipa, &qhs_ipc_router, &qhs_mss_cfg,
		&qhs_mx_rdpm, &qhs_pcie0_cfg, &qhs_pcie1_cfg,
		&qhs_pimem_cfg, &qhs_pka_wrapper_cfg, &qhs_pmu_wrapper_cfg,
		&qhs_qdss_cfg, &qhs_qup0, &qhs_qup1,
		&qhs_qup2, &qhs_security, &qhs_spss_cfg,
		&qhs_tcsr, &qhs_tlmm, &qhs_ufs_card_cfg,
		&qhs_ufs_mem_cfg, &qhs_usb3_0, &qhs_usb3_1,
		&qhs_venus_cfg, &qhs_vsense_ctrl_cfg, &qns_a1_noc_cfg,
		&qns_a2_noc_cfg, &qns_ddrss_cfg, &qns_mnoc_cfg,
		&qns_snoc_cfg, &srvc_cnoc);
DEFINE_QBCM(bcm_cn2, "CN2", false, 0, 5,
		&qhs_lpass_cfg, &qhs_pdm, &qhs_qspi,
		&qhs_sdc2, &qhs_sdc4);
DEFINE_QBCM(bcm_co0, "CO0", false, 0, 1,
		&qns_nsp_gemnoc);
DEFINE_QBCM(bcm_co3, "CO3", false, 0, 1,
		&qxm_nsp);
DEFINE_QBCM(bcm_mc0, "MC0", true, 0, 1,
		&ebi);
DEFINE_QBCM(bcm_mm0, "MM0", true, 0, 1,
		&qns_mem_noc_hf);
DEFINE_QBCM(bcm_mm1, "MM1", false, 0, 3,
		&qnm_camnoc_hf, &qxm_mdp0, &qxm_mdp1);
DEFINE_QBCM(bcm_mm4, "MM4", false, 0, 1,
		&qns_mem_noc_sf);
DEFINE_QBCM(bcm_mm5, "MM5", false, 0, 6,
		&qnm_camnoc_icp, &qnm_camnoc_sf, &qnm_video0,
		&qnm_video1, &qnm_video_cvp, &qxm_rot);
DEFINE_QBCM(bcm_qup0, "QUP0", false, 0, 1,
		&qup0_core_slave);
DEFINE_QBCM(bcm_qup1, "QUP1", false, 0, 1,
		&qup1_core_slave);
DEFINE_QBCM(bcm_qup2, "QUP2", false, 0, 1,
		&qup2_core_slave);
DEFINE_QBCM(bcm_sh0, "SH0", true, 0, 1,
		&qns_llcc);
DEFINE_QBCM(bcm_sh2, "SH2", false, 0, 2,
		&alm_gpu_tcu, &alm_sys_tcu);
DEFINE_QBCM(bcm_sh3, "SH3", false, 0, 1,
		&qnm_cmpnoc);
DEFINE_QBCM(bcm_sh4, "SH4", false, 0, 1,
		&chm_apps);
DEFINE_QBCM(bcm_sn0, "SN0", true, 0, 1,
		&qns_gemnoc_sf);
DEFINE_QBCM(bcm_sn2, "SN2", false, 0, 1,
		&qns_gemnoc_gc);
DEFINE_QBCM(bcm_sn3, "SN3", false, 0, 1,
		&qxs_pimem);
DEFINE_QBCM(bcm_sn4, "SN4", false, 0, 1,
		&xs_qdss_stm);
DEFINE_QBCM(bcm_sn5, "SN5", false, 0, 1,
		&xm_pcie3_0);
DEFINE_QBCM(bcm_sn6, "SN6", false, 0, 1,
		&xm_pcie3_1);
DEFINE_QBCM(bcm_sn7, "SN7", false, 0, 1,
		&qnm_aggre1_noc);
DEFINE_QBCM(bcm_sn8, "SN8", false, 0, 1,
		&qnm_aggre2_noc);
DEFINE_QBCM(bcm_sn14, "SN14", false, 0, 1,
		&qns_pcie_mem_noc);
DEFINE_QBCM(bcm_acv_disp, "ACV", false, 1, 1,
		&ebi_disp);
DEFINE_QBCM(bcm_mc0_disp, "MC0", false, 1, 1,
		&ebi_disp);
DEFINE_QBCM(bcm_mm0_disp, "MM0", false, 1, 1,
		&qns_mem_noc_hf_disp);
DEFINE_QBCM(bcm_mm1_disp, "MM1", false, 1, 2,
		&qxm_mdp0_disp, &qxm_mdp1_disp);
DEFINE_QBCM(bcm_mm4_disp, "MM4", false, 1, 1,
		&qns_mem_noc_sf_disp);
DEFINE_QBCM(bcm_mm5_disp, "MM5", false, 1, 1,
		&qxm_rot_disp);
DEFINE_QBCM(bcm_sh0_disp, "SH0", false, 1, 1,
		&qns_llcc_disp);

static struct qcom_icc_bcm *aggre1_noc_bcms[] = {
};

static struct qcom_icc_node *aggre1_noc_nodes[] = {
	[MASTER_QSPI_0] = &qhm_qspi,
	[MASTER_QUP_1] = &qhm_qup1,
	[MASTER_A1NOC_CFG] = &qnm_a1noc_cfg,
	[MASTER_SDCC_4] = &xm_sdc4,
	[MASTER_UFS_MEM] = &xm_ufs_mem,
	[MASTER_USB3_0] = &xm_usb3_0,
	[MASTER_USB3_1] = &xm_usb3_1,
	[SLAVE_A1NOC_SNOC] = &qns_a1noc_snoc,
	[SLAVE_SERVICE_A1NOC] = &srvc_aggre1_noc,
};

static char *aggre1_noc_voters[] = {
	"hlos",
};

static struct qcom_icc_desc lahaina_aggre1_noc = {
	.nodes = aggre1_noc_nodes,
	.num_nodes = ARRAY_SIZE(aggre1_noc_nodes),
	.bcms = aggre1_noc_bcms,
	.num_bcms = ARRAY_SIZE(aggre1_noc_bcms),
	.voters = aggre1_noc_voters,
	.num_voters = ARRAY_SIZE(aggre1_noc_voters),
};

static struct qcom_icc_bcm *aggre2_noc_bcms[] = {
	&bcm_ce0,
	&bcm_sn5,
	&bcm_sn6,
	&bcm_sn14,
};

static struct qcom_icc_node *aggre2_noc_nodes[] = {
	[MASTER_QDSS_BAM] = &qhm_qdss_bam,
	[MASTER_QUP_0] = &qhm_qup0,
	[MASTER_QUP_2] = &qhm_qup2,
	[MASTER_A2NOC_CFG] = &qnm_a2noc_cfg,
	[MASTER_CRYPTO] = &qxm_crypto,
	[MASTER_IPA] = &qxm_ipa,
	[MASTER_PCIE_0] = &xm_pcie3_0,
	[MASTER_PCIE_1] = &xm_pcie3_1,
	[MASTER_QDSS_ETR] = &xm_qdss_etr,
	[MASTER_SDCC_2] = &xm_sdc2,
	[MASTER_UFS_CARD] = &xm_ufs_card,
	[SLAVE_A2NOC_SNOC] = &qns_a2noc_snoc,
	[SLAVE_ANOC_PCIE_GEM_NOC] = &qns_pcie_mem_noc,
	[SLAVE_SERVICE_A2NOC] = &srvc_aggre2_noc,
};

static char *aggre2_noc_voters[] = {
	"hlos",
};

static struct qcom_icc_desc lahaina_aggre2_noc = {
	.nodes = aggre2_noc_nodes,
	.num_nodes = ARRAY_SIZE(aggre2_noc_nodes),
	.bcms = aggre2_noc_bcms,
	.num_bcms = ARRAY_SIZE(aggre2_noc_bcms),
	.voters = aggre2_noc_voters,
	.num_voters = ARRAY_SIZE(aggre2_noc_voters),
};

static struct qcom_icc_bcm *clk_virt_bcms[] = {
	&bcm_qup0,
	&bcm_qup1,
	&bcm_qup2,
};

static struct qcom_icc_node *clk_virt_nodes[] = {
	[MASTER_QUP_CORE_0] = &qup0_core_master,
	[MASTER_QUP_CORE_1] = &qup1_core_master,
	[MASTER_QUP_CORE_2] = &qup2_core_master,
	[SLAVE_QUP_CORE_0] = &qup0_core_slave,
	[SLAVE_QUP_CORE_1] = &qup1_core_slave,
	[SLAVE_QUP_CORE_2] = &qup2_core_slave,
};

static char *clk_virt_voters[] = {
	"hlos",
};

static struct qcom_icc_desc lahaina_clk_virt = {
	.nodes = clk_virt_nodes,
	.num_nodes = ARRAY_SIZE(clk_virt_nodes),
	.bcms = clk_virt_bcms,
	.num_bcms = ARRAY_SIZE(clk_virt_bcms),
	.voters = clk_virt_voters,
	.num_voters = ARRAY_SIZE(clk_virt_voters),
};

static struct qcom_icc_bcm *config_noc_bcms[] = {
	&bcm_cn0,
	&bcm_cn1,
	&bcm_cn2,
	&bcm_sn3,
	&bcm_sn4,
};

static struct qcom_icc_node *config_noc_nodes[] = {
	[MASTER_GEM_NOC_CNOC] = &qnm_gemnoc_cnoc,
	[MASTER_GEM_NOC_PCIE_SNOC] = &qnm_gemnoc_pcie,
	[MASTER_QDSS_DAP] = &xm_qdss_dap,
	[SLAVE_AHB2PHY_SOUTH] = &qhs_ahb2phy0,
	[SLAVE_AHB2PHY_NORTH] = &qhs_ahb2phy1,
	[SLAVE_AOSS] = &qhs_aoss,
	[SLAVE_APPSS] = &qhs_apss,
	[SLAVE_CAMERA_CFG] = &qhs_camera_cfg,
	[SLAVE_CLK_CTL] = &qhs_clk_ctl,
	[SLAVE_CDSP_CFG] = &qhs_compute_cfg,
	[SLAVE_RBCPR_CX_CFG] = &qhs_cpr_cx,
	[SLAVE_RBCPR_MMCX_CFG] = &qhs_cpr_mmcx,
	[SLAVE_RBCPR_MX_CFG] = &qhs_cpr_mx,
	[SLAVE_CRYPTO_0_CFG] = &qhs_crypto0_cfg,
	[SLAVE_CX_RDPM] = &qhs_cx_rdpm,
	[SLAVE_DCC_CFG] = &qhs_dcc_cfg,
	[SLAVE_DISPLAY_CFG] = &qhs_display_cfg,
	[SLAVE_GFX3D_CFG] = &qhs_gpuss_cfg,
	[SLAVE_HWKM] = &qhs_hwkm,
	[SLAVE_IMEM_CFG] = &qhs_imem_cfg,
	[SLAVE_IPA_CFG] = &qhs_ipa,
	[SLAVE_IPC_ROUTER_CFG] = &qhs_ipc_router,
	[SLAVE_LPASS] = &qhs_lpass_cfg,
	[SLAVE_CNOC_MSS] = &qhs_mss_cfg,
	[SLAVE_MX_RDPM] = &qhs_mx_rdpm,
	[SLAVE_PCIE_0_CFG] = &qhs_pcie0_cfg,
	[SLAVE_PCIE_1_CFG] = &qhs_pcie1_cfg,
	[SLAVE_PDM] = &qhs_pdm,
	[SLAVE_PIMEM_CFG] = &qhs_pimem_cfg,
	[SLAVE_PKA_WRAPPER_CFG] = &qhs_pka_wrapper_cfg,
	[SLAVE_PMU_WRAPPER_CFG] = &qhs_pmu_wrapper_cfg,
	[SLAVE_QDSS_CFG] = &qhs_qdss_cfg,
	[SLAVE_QSPI_0] = &qhs_qspi,
	[SLAVE_QUP_0] = &qhs_qup0,
	[SLAVE_QUP_1] = &qhs_qup1,
	[SLAVE_QUP_2] = &qhs_qup2,
	[SLAVE_SDCC_2] = &qhs_sdc2,
	[SLAVE_SDCC_4] = &qhs_sdc4,
	[SLAVE_SECURITY] = &qhs_security,
	[SLAVE_SPSS_CFG] = &qhs_spss_cfg,
	[SLAVE_TCSR] = &qhs_tcsr,
	[SLAVE_TLMM] = &qhs_tlmm,
	[SLAVE_UFS_CARD_CFG] = &qhs_ufs_card_cfg,
	[SLAVE_UFS_MEM_CFG] = &qhs_ufs_mem_cfg,
	[SLAVE_USB3_0] = &qhs_usb3_0,
	[SLAVE_USB3_1] = &qhs_usb3_1,
	[SLAVE_VENUS_CFG] = &qhs_venus_cfg,
	[SLAVE_VSENSE_CTRL_CFG] = &qhs_vsense_ctrl_cfg,
	[SLAVE_A1NOC_CFG] = &qns_a1_noc_cfg,
	[SLAVE_A2NOC_CFG] = &qns_a2_noc_cfg,
	[SLAVE_DDRSS_CFG] = &qns_ddrss_cfg,
	[SLAVE_CNOC_MNOC_CFG] = &qns_mnoc_cfg,
	[SLAVE_SNOC_CFG] = &qns_snoc_cfg,
	[SLAVE_BOOT_IMEM] = &qxs_boot_imem,
	[SLAVE_IMEM] = &qxs_imem,
	[SLAVE_PIMEM] = &qxs_pimem,
	[SLAVE_SERVICE_CNOC] = &srvc_cnoc,
	[SLAVE_PCIE_0] = &xs_pcie_0,
	[SLAVE_PCIE_1] = &xs_pcie_1,
	[SLAVE_QDSS_STM] = &xs_qdss_stm,
	[SLAVE_TCU] = &xs_sys_tcu_cfg,
};

static char *config_noc_voters[] = {
	"hlos",
};

static struct qcom_icc_desc lahaina_config_noc = {
	.nodes = config_noc_nodes,
	.num_nodes = ARRAY_SIZE(config_noc_nodes),
	.bcms = config_noc_bcms,
	.num_bcms = ARRAY_SIZE(config_noc_bcms),
	.voters = config_noc_voters,
	.num_voters = ARRAY_SIZE(config_noc_voters),
};

static struct qcom_icc_bcm *dc_noc_bcms[] = {
};

static struct qcom_icc_node *dc_noc_nodes[] = {
	[MASTER_CNOC_DC_NOC] = &qnm_cnoc_dc_noc,
	[SLAVE_LLCC_CFG] = &qhs_llcc,
	[SLAVE_GEM_NOC_CFG] = &qns_gemnoc,
};

static char *dc_noc_voters[] = {
	"hlos",
};

static struct qcom_icc_desc lahaina_dc_noc = {
	.nodes = dc_noc_nodes,
	.num_nodes = ARRAY_SIZE(dc_noc_nodes),
	.bcms = dc_noc_bcms,
	.num_bcms = ARRAY_SIZE(dc_noc_bcms),
	.voters = dc_noc_voters,
	.num_voters = ARRAY_SIZE(dc_noc_voters),
};

static struct qcom_icc_bcm *gem_noc_bcms[] = {
	&bcm_sh0,
	&bcm_sh2,
	&bcm_sh3,
	&bcm_sh4,
	&bcm_sh0_disp,
};

static struct qcom_icc_node *gem_noc_nodes[] = {
	[MASTER_GPU_TCU] = &alm_gpu_tcu,
	[MASTER_SYS_TCU] = &alm_sys_tcu,
	[MASTER_APPSS_PROC] = &chm_apps,
	[MASTER_COMPUTE_NOC] = &qnm_cmpnoc,
	[MASTER_GEM_NOC_CFG] = &qnm_gemnoc_cfg,
	[MASTER_GFX3D] = &qnm_gpu,
	[MASTER_MNOC_HF_MEM_NOC] = &qnm_mnoc_hf,
	[MASTER_MNOC_SF_MEM_NOC] = &qnm_mnoc_sf,
	[MASTER_GEM_NOC_PCIE_SNOC] = &qnm_pcie,
	[MASTER_SNOC_GC_MEM_NOC] = &qnm_snoc_gc,
	[MASTER_SNOC_SF_MEM_NOC] = &qnm_snoc_sf,
	[SLAVE_MSS_PROC_MS_MPU_CFG] = &qhs_mdsp_ms_mpu_cfg,
	[SLAVE_MCDMA_MS_MPU_CFG] = &qhs_modem_ms_mpu_cfg,
	[SLAVE_GEM_NOC_CNOC] = &qns_gem_noc_cnoc,
	[SLAVE_LLCC] = &qns_llcc,
	[SLAVE_MEM_NOC_PCIE_SNOC] = &qns_pcie,
	[SLAVE_SERVICE_GEM_NOC_1] = &srvc_even_gemnoc,
	[SLAVE_SERVICE_GEM_NOC_2] = &srvc_odd_gemnoc,
	[SLAVE_SERVICE_GEM_NOC] = &srvc_sys_gemnoc,
	[MASTER_MNOC_HF_MEM_NOC_DISP] = &qnm_mnoc_hf_disp,
	[MASTER_MNOC_SF_MEM_NOC_DISP] = &qnm_mnoc_sf_disp,
	[SLAVE_LLCC_DISP] = &qns_llcc_disp,
};

static char *gem_noc_voters[] = {
	"hlos",
	"disp",
};

static struct qcom_icc_desc lahaina_gem_noc = {
	.nodes = gem_noc_nodes,
	.num_nodes = ARRAY_SIZE(gem_noc_nodes),
	.bcms = gem_noc_bcms,
	.num_bcms = ARRAY_SIZE(gem_noc_bcms),
	.voters = gem_noc_voters,
	.num_voters = ARRAY_SIZE(gem_noc_voters),
};

static struct qcom_icc_bcm *lpass_ag_noc_bcms[] = {
};

static struct qcom_icc_node *lpass_ag_noc_nodes[] = {
	[MASTER_CNOC_LPASS_AG_NOC] = &qhm_config_noc,
	[SLAVE_LPASS_CORE_CFG] = &qhs_lpass_core,
	[SLAVE_LPASS_LPI_CFG] = &qhs_lpass_lpi,
	[SLAVE_LPASS_MPU_CFG] = &qhs_lpass_mpu,
	[SLAVE_LPASS_TOP_CFG] = &qhs_lpass_top,
	[SLAVE_SERVICES_LPASS_AML_NOC] = &srvc_niu_aml_noc,
	[SLAVE_SERVICE_LPASS_AG_NOC] = &srvc_niu_lpass_agnoc,
};

static char *lpass_ag_noc_voters[] = {
	"hlos",
};

static struct qcom_icc_desc lahaina_lpass_ag_noc = {
	.nodes = lpass_ag_noc_nodes,
	.num_nodes = ARRAY_SIZE(lpass_ag_noc_nodes),
	.bcms = lpass_ag_noc_bcms,
	.num_bcms = ARRAY_SIZE(lpass_ag_noc_bcms),
	.voters = lpass_ag_noc_voters,
	.num_voters = ARRAY_SIZE(lpass_ag_noc_voters),
};

static struct qcom_icc_bcm *mc_virt_bcms[] = {
	&bcm_acv,
	&bcm_mc0,
	&bcm_acv_disp,
	&bcm_mc0_disp,
};

static struct qcom_icc_node *mc_virt_nodes[] = {
	[MASTER_LLCC] = &llcc_mc,
	[SLAVE_EBI1] = &ebi,
	[MASTER_LLCC_DISP] = &llcc_mc_disp,
	[SLAVE_EBI1_DISP] = &ebi_disp,
};

static char *mc_virt_voters[] = {
	"hlos",
	"disp",
};

static struct qcom_icc_desc lahaina_mc_virt = {
	.nodes = mc_virt_nodes,
	.num_nodes = ARRAY_SIZE(mc_virt_nodes),
	.bcms = mc_virt_bcms,
	.num_bcms = ARRAY_SIZE(mc_virt_bcms),
	.voters = mc_virt_voters,
	.num_voters = ARRAY_SIZE(mc_virt_voters),
};

static struct qcom_icc_bcm *mmss_noc_bcms[] = {
	&bcm_mm0,
	&bcm_mm1,
	&bcm_mm4,
	&bcm_mm5,
	&bcm_mm0_disp,
	&bcm_mm1_disp,
	&bcm_mm4_disp,
	&bcm_mm5_disp,
};

static struct qcom_icc_node *mmss_noc_nodes[] = {
	[MASTER_CAMNOC_HF] = &qnm_camnoc_hf,
	[MASTER_CAMNOC_ICP] = &qnm_camnoc_icp,
	[MASTER_CAMNOC_SF] = &qnm_camnoc_sf,
	[MASTER_CNOC_MNOC_CFG] = &qnm_mnoc_cfg,
	[MASTER_VIDEO_P0] = &qnm_video0,
	[MASTER_VIDEO_P1] = &qnm_video1,
	[MASTER_VIDEO_PROC] = &qnm_video_cvp,
	[MASTER_MDP0] = &qxm_mdp0,
	[MASTER_MDP1] = &qxm_mdp1,
	[MASTER_ROTATOR] = &qxm_rot,
	[SLAVE_MNOC_HF_MEM_NOC] = &qns_mem_noc_hf,
	[SLAVE_MNOC_SF_MEM_NOC] = &qns_mem_noc_sf,
	[SLAVE_SERVICE_MNOC] = &srvc_mnoc,
	[MASTER_MDP0_DISP] = &qxm_mdp0_disp,
	[MASTER_MDP1_DISP] = &qxm_mdp1_disp,
	[MASTER_ROTATOR_DISP] = &qxm_rot_disp,
	[SLAVE_MNOC_HF_MEM_NOC_DISP] = &qns_mem_noc_hf_disp,
	[SLAVE_MNOC_SF_MEM_NOC_DISP] = &qns_mem_noc_sf_disp,
};

static char *mmss_noc_voters[] = {
	"hlos",
	"disp",
};

static struct qcom_icc_desc lahaina_mmss_noc = {
	.nodes = mmss_noc_nodes,
	.num_nodes = ARRAY_SIZE(mmss_noc_nodes),
	.bcms = mmss_noc_bcms,
	.num_bcms = ARRAY_SIZE(mmss_noc_bcms),
	.voters = mmss_noc_voters,
	.num_voters = ARRAY_SIZE(mmss_noc_voters),
};

static struct qcom_icc_bcm *nsp_noc_bcms[] = {
	&bcm_co0,
	&bcm_co3,
};

static struct qcom_icc_node *nsp_noc_nodes[] = {
	[MASTER_CDSP_NOC_CFG] = &qhm_nsp_noc_config,
	[MASTER_CDSP_PROC] = &qxm_nsp,
	[SLAVE_CDSP_MEM_NOC] = &qns_nsp_gemnoc,
	[SLAVE_SERVICE_NSP_NOC] = &service_nsp_noc,
};

static char *nsp_noc_voters[] = {
	"hlos",
};

static struct qcom_icc_desc lahaina_nsp_noc = {
	.nodes = nsp_noc_nodes,
	.num_nodes = ARRAY_SIZE(nsp_noc_nodes),
	.bcms = nsp_noc_bcms,
	.num_bcms = ARRAY_SIZE(nsp_noc_bcms),
	.voters = nsp_noc_voters,
	.num_voters = ARRAY_SIZE(nsp_noc_voters),
};

static struct qcom_icc_bcm *system_noc_bcms[] = {
	&bcm_sn0,
	&bcm_sn2,
	&bcm_sn7,
	&bcm_sn8,
};

static struct qcom_icc_node *system_noc_nodes[] = {
	[MASTER_A1NOC_SNOC] = &qnm_aggre1_noc,
	[MASTER_A2NOC_SNOC] = &qnm_aggre2_noc,
	[MASTER_SNOC_CFG] = &qnm_snoc_cfg,
	[MASTER_PIMEM] = &qxm_pimem,
	[MASTER_GIC] = &xm_gic,
	[SLAVE_SNOC_GEM_NOC_GC] = &qns_gemnoc_gc,
	[SLAVE_SNOC_GEM_NOC_SF] = &qns_gemnoc_sf,
	[SLAVE_SERVICE_SNOC] = &srvc_snoc,
};

static char *system_noc_voters[] = {
	"hlos",
};

static struct qcom_icc_desc lahaina_system_noc = {
	.nodes = system_noc_nodes,
	.num_nodes = ARRAY_SIZE(system_noc_nodes),
	.bcms = system_noc_bcms,
	.num_bcms = ARRAY_SIZE(system_noc_bcms),
	.voters = system_noc_voters,
	.num_voters = ARRAY_SIZE(system_noc_voters),
};

static const struct regmap_config icc_regmap_config = {
	.reg_bits       = 32,
	.reg_stride     = 4,
	.val_bits       = 32,
};

static struct regmap *
qcom_icc_map(struct platform_device *pdev, const struct qcom_icc_desc *desc)
{
	void __iomem *base;
	struct resource *res;
	struct device *dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return NULL;

	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base))
		return ERR_CAST(base);

	return devm_regmap_init_mmio(dev, base, &icc_regmap_config);
}

static int qnoc_probe(struct platform_device *pdev)
{
	const struct qcom_icc_desc *desc;
	struct icc_onecell_data *data;
	struct icc_provider *provider;
	struct qcom_icc_node **qnodes;
	struct qcom_icc_provider *qp;
	struct icc_node *node;
	size_t num_nodes, i;
	int ret;

	desc = of_device_get_match_data(&pdev->dev);
	if (!desc)
		return -EINVAL;

	qnodes = desc->nodes;
	num_nodes = desc->num_nodes;

	qp = devm_kzalloc(&pdev->dev, sizeof(*qp), GFP_KERNEL);
	if (!qp)
		return -ENOMEM;

	data = devm_kcalloc(&pdev->dev, num_nodes, sizeof(*node), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	provider = &qp->provider;
	provider->dev = &pdev->dev;
	provider->set = qcom_icc_set;
	provider->pre_aggregate = qcom_icc_pre_aggregate;
	provider->aggregate = qcom_icc_aggregate;
	provider->xlate = of_icc_xlate_onecell;
	INIT_LIST_HEAD(&provider->nodes);
	provider->data = data;

	qp->dev = &pdev->dev;
	qp->bcms = desc->bcms;
	qp->num_bcms = desc->num_bcms;

	qp->num_voters = desc->num_voters;
	qp->voters = devm_kcalloc(&pdev->dev, qp->num_voters,
				  sizeof(*qp->voters), GFP_KERNEL);
	if (!qp->voters)
		return -ENOMEM;

	for (i = 0; i < qp->num_voters; i++) {
		qp->voters[i] = of_bcm_voter_get(qp->dev, desc->voters[i]);
		if (IS_ERR(qp->voters[i]))
			return PTR_ERR(qp->voters[i]);
	}

	qp->regmap = qcom_icc_map(pdev, desc);
	if (IS_ERR(qp->regmap))
		return PTR_ERR(qp->regmap);

	ret = icc_provider_add(provider);
	if (ret) {
		dev_err(&pdev->dev, "error adding interconnect provider\n");
		return ret;
	}

	qp->num_clks = devm_clk_bulk_get_all(qp->dev, &qp->clks);
	if (qp->num_clks < 0)
		return qp->num_clks;

	for (i = 0; i < num_nodes; i++) {
		size_t j;

		if (!qnodes[i])
			continue;

		qnodes[i]->regmap = dev_get_regmap(qp->dev, NULL);

		node = icc_node_create(qnodes[i]->id);
		if (IS_ERR(node)) {
			ret = PTR_ERR(node);
			goto err;
		}

		node->name = qnodes[i]->name;
		node->data = qnodes[i];
		icc_node_add(node, provider);

		dev_dbg(&pdev->dev, "registered node %pK %s %d\n", node,
			qnodes[i]->name, node->id);

		/* populate links */
		for (j = 0; j < qnodes[i]->num_links; j++)
			icc_link_create(node, qnodes[i]->links[j]);

		data->nodes[i] = node;
	}
	data->num_nodes = num_nodes;

	for (i = 0; i < qp->num_bcms; i++)
		qcom_icc_bcm_init(qp->bcms[i], &pdev->dev);

	platform_set_drvdata(pdev, qp);

	dev_dbg(&pdev->dev, "Registered LAHAINA ICC\n");

	mutex_lock(&probe_list_lock);
	list_add_tail(&qp->probe_list, &qnoc_probe_list);
	mutex_unlock(&probe_list_lock);

	return ret;
err:
	list_for_each_entry(node, &provider->nodes, node_list) {
		icc_node_del(node);
		icc_node_destroy(node->id);
	}

	clk_bulk_put_all(qp->num_clks, qp->clks);

	icc_provider_del(provider);
	return ret;
}

static int qnoc_remove(struct platform_device *pdev)
{
	struct qcom_icc_provider *qp = platform_get_drvdata(pdev);
	struct icc_provider *provider = &qp->provider;
	struct icc_node *n;

	list_for_each_entry(n, &provider->nodes, node_list) {
		icc_node_del(n);
		icc_node_destroy(n->id);
	}

	clk_bulk_put_all(qp->num_clks, qp->clks);

	return icc_provider_del(provider);
}

static const struct of_device_id qnoc_of_match[] = {
	{ .compatible = "qcom,lahaina-aggre1_noc",
	  .data = &lahaina_aggre1_noc},
	{ .compatible = "qcom,lahaina-aggre2_noc",
	  .data = &lahaina_aggre2_noc},
	{ .compatible = "qcom,lahaina-clk_virt",
	  .data = &lahaina_clk_virt},
	{ .compatible = "qcom,lahaina-config_noc",
	  .data = &lahaina_config_noc},
	{ .compatible = "qcom,lahaina-dc_noc",
	  .data = &lahaina_dc_noc},
	{ .compatible = "qcom,lahaina-gem_noc",
	  .data = &lahaina_gem_noc},
	{ .compatible = "qcom,lahaina-lpass_ag_noc",
	  .data = &lahaina_lpass_ag_noc},
	{ .compatible = "qcom,lahaina-mc_virt",
	  .data = &lahaina_mc_virt},
	{ .compatible = "qcom,lahaina-mmss_noc",
	  .data = &lahaina_mmss_noc},
	{ .compatible = "qcom,lahaina-nsp_noc",
	  .data = &lahaina_nsp_noc},
	{ .compatible = "qcom,lahaina-system_noc",
	  .data = &lahaina_system_noc},
	{ }
};
MODULE_DEVICE_TABLE(of, qnoc_of_match);

static void qnoc_sync_state(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct qcom_icc_provider *qp = platform_get_drvdata(pdev);
	struct qcom_icc_bcm *bcm;
	struct bcm_voter *voter;

	mutex_lock(&probe_list_lock);
	probe_count++;

	if (probe_count < ARRAY_SIZE(qnoc_of_match) - 1) {
		mutex_unlock(&probe_list_lock);
		return;
	}

	list_for_each_entry(qp, &qnoc_probe_list, probe_list) {
		int i;

		for (i = 0; i < qp->num_voters; i++)
			qcom_icc_bcm_voter_clear_init(qp->voters[i]);

		for (i = 0; i < qp->num_bcms; i++) {
			bcm = qp->bcms[i];
			if (!bcm->keepalive)
				continue;

			voter = qp->voters[bcm->voter_idx];
			qcom_icc_bcm_voter_add(voter, bcm);
			qcom_icc_bcm_voter_commit(voter);
		}
	}

	mutex_unlock(&probe_list_lock);
}

static struct platform_driver qnoc_driver = {
	.probe = qnoc_probe,
	.remove = qnoc_remove,
	.driver = {
		.name = "qnoc-lahaina",
		.of_match_table = qnoc_of_match,
		.sync_state = qnoc_sync_state,
	},
};

static int __init qnoc_driver_init(void)
{
	return platform_driver_register(&qnoc_driver);
}
core_initcall(qnoc_driver_init);

static void __exit qnoc_driver_exit(void)
{
	platform_driver_unregister(&qnoc_driver);
}
module_exit(qnoc_driver_exit);

MODULE_DESCRIPTION("Lahaina NoC driver");
MODULE_LICENSE("GPL v2");
