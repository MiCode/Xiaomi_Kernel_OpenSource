// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 */

#include <dt-bindings/interconnect/qcom,sdxbaagha.h>
#include <linux/device.h>
#include <linux/interconnect.h>
#include <linux/interconnect-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include "icc-rpmh.h"
#include "qnoc-qos.h"

static const struct regmap_config icc_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
};

static struct qcom_icc_qosbox qhm_audio_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x28000 },
	.config = &(struct qos_config) {
		.prio = 0,
		.urg_fwd = 0,
		.prio_fwd_disable = 0,
	},
};

static struct qcom_icc_node qhm_audio = {
	.name = "qhm_audio",
	.id = MASTER_AUDIO,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &qhm_audio_qos,
	.num_links = 1,
	.links = { SLAVE_A1NOC_CFG },
};

static struct qcom_icc_qosbox qhm_qdss_bam_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x26000 },
	.config = &(struct qos_config) {
		.prio = 2,
		.urg_fwd = 0,
		.prio_fwd_disable = 0,
	},
};

static struct qcom_icc_node qhm_qdss_bam = {
	.name = "qhm_qdss_bam",
	.id = MASTER_QDSS_BAM,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &qhm_qdss_bam_qos,
	.num_links = 1,
	.links = { SLAVE_A1NOC_CFG },
};

static struct qcom_icc_qosbox qhm_qpic_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x25000 },
	.config = &(struct qos_config) {
		.prio = 2,
		.urg_fwd = 0,
		.prio_fwd_disable = 0,
	},
};

static struct qcom_icc_node qhm_qpic = {
	.name = "qhm_qpic",
	.id = MASTER_QPIC,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &qhm_qpic_qos,
	.num_links = 1,
	.links = { SLAVE_A1NOC_CFG },
};

static struct qcom_icc_qosbox qhm_qup0_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x24000 },
	.config = &(struct qos_config) {
		.prio = 2,
		.urg_fwd = 0,
		.prio_fwd_disable = 0,
	},
};

static struct qcom_icc_node qhm_qup0 = {
	.name = "qhm_qup0",
	.id = MASTER_QUP_0,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &qhm_qup0_qos,
	.num_links = 1,
	.links = { SLAVE_A1NOC_CFG },
};

static struct qcom_icc_qosbox qxm_crypto_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x23000 },
	.config = &(struct qos_config) {
		.prio = 2,
		.urg_fwd = 1,
		.prio_fwd_disable = 0,
	},
};

static struct qcom_icc_node qxm_crypto = {
	.name = "qxm_crypto",
	.id = MASTER_CRYPTO,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &qxm_crypto_qos,
	.num_links = 1,
	.links = { SLAVE_A1NOC_CFG },
};

static struct qcom_icc_qosbox qxm_ipa_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x2a000 },
	.config = &(struct qos_config) {
		.prio = 2,
		.urg_fwd = 1,
		.prio_fwd_disable = 0,
	},
};

static struct qcom_icc_node qxm_ipa = {
	.name = "qxm_ipa",
	.id = MASTER_IPA,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &qxm_ipa_qos,
	.num_links = 1,
	.links = { SLAVE_A1NOC_CFG },
};

static struct qcom_icc_qosbox xm_emac_0_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x29000 },
	.config = &(struct qos_config) {
		.prio = 2,
		.urg_fwd = 0,
		.prio_fwd_disable = 0,
	},
};

static struct qcom_icc_node xm_emac_0 = {
	.name = "xm_emac_0",
	.id = MASTER_EMAC_0,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &xm_emac_0_qos,
	.num_links = 1,
	.links = { SLAVE_A1NOC_CFG },
};

static struct qcom_icc_qosbox xm_pcie3_0_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0xa000 },
	.config = &(struct qos_config) {
		.prio = 3,
		.urg_fwd = 0,
		.prio_fwd_disable = 0,
	},
};

static struct qcom_icc_node xm_pcie3_0 = {
	.name = "xm_pcie3_0",
	.id = MASTER_PCIE_0,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &xm_pcie3_0_qos,
	.num_links = 1,
	.links = { SLAVE_ANOC_PCIE_GEM_NOC },
};

static struct qcom_icc_qosbox xm_qdss_etr0_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x22000 },
	.config = &(struct qos_config) {
		.prio = 2,
		.urg_fwd = 0,
		.prio_fwd_disable = 0,
	},
};

static struct qcom_icc_node xm_qdss_etr0 = {
	.name = "xm_qdss_etr0",
	.id = MASTER_QDSS_ETR,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &xm_qdss_etr0_qos,
	.num_links = 1,
	.links = { SLAVE_A1NOC_CFG },
};

static struct qcom_icc_qosbox xm_qdss_etr1_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x21000 },
	.config = &(struct qos_config) {
		.prio = 2,
		.urg_fwd = 0,
		.prio_fwd_disable = 0,
	},
};

static struct qcom_icc_node xm_qdss_etr1 = {
	.name = "xm_qdss_etr1",
	.id = MASTER_QDSS_ETR_1,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &xm_qdss_etr1_qos,
	.num_links = 1,
	.links = { SLAVE_A1NOC_CFG },
};

static struct qcom_icc_qosbox xm_sdc4_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x27000 },
	.config = &(struct qos_config) {
		.prio = 2,
		.urg_fwd = 0,
		.prio_fwd_disable = 0,
	},
};

static struct qcom_icc_node xm_sdc4 = {
	.name = "xm_sdc4",
	.id = MASTER_SDCC_4,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &xm_sdc4_qos,
	.num_links = 1,
	.links = { SLAVE_A1NOC_CFG },
};

static struct qcom_icc_qosbox xm_usb3_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x20000 },
	.config = &(struct qos_config) {
		.prio = 2,
		.urg_fwd = 0,
		.prio_fwd_disable = 0,
	},
};

static struct qcom_icc_node xm_usb3 = {
	.name = "xm_usb3",
	.id = MASTER_USB3_0,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &xm_usb3_qos,
	.num_links = 1,
	.links = { SLAVE_A1NOC_CFG },
};

static struct qcom_icc_node qhm_pcie_rscc = {
	.name = "qhm_pcie_rscc",
	.id = MASTER_PCIE_RSCC,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 41,
	.links = { SLAVE_AHB2PHY, SLAVE_AOSS,
		   SLAVE_APPSS, SLAVE_AUDIO,
		   SLAVE_BOOT_ROM, SLAVE_CLK_CTL,
		   SLAVE_RBCPR_CX_CFG, SLAVE_RBCPR_MXA_CFG,
		   SLAVE_RBCPR_MXC_CFG, SLAVE_CRYPTO_0_CFG,
		   SLAVE_EMAC_CFG, SLAVE_IMEM_CFG,
		   SLAVE_IPA_CFG, SLAVE_IPC_ROUTER_CFG,
		   SLAVE_CNOC_MSS, SLAVE_PCIE_0_CFG,
		   SLAVE_PDM, SLAVE_PMU_WRAPPER_CFG,
		   SLAVE_PRNG, SLAVE_QDSS_CFG,
		   SLAVE_QPIC, SLAVE_QUP_0,
		   SLAVE_SDCC_4, SLAVE_SPMI_VGI_COEX,
		   SLAVE_TCSR, SLAVE_TLMM,
		   SLAVE_TME_CFG, SLAVE_USB3,
		   SLAVE_VSENSE_CTRL_CFG, SLAVE_DDRSS_CFG,
		   SLAVE_ANOC_THROTTLE_CFG, SLAVE_MCDMA_THROTTLE_CFG,
		   SLAVE_MS_NAV_THROTTLE_CFG, SLAVE_PCIE_THROTTLE_CFG,
		   SLAVE_QM_CFG, SLAVE_QM_MPU_CFG,
		   SLAVE_SNOC_THROTTLE_CFG, SLAVE_BOOT_IMEM,
		   SLAVE_IMEM, SLAVE_QDSS_STM,
		   SLAVE_TCU },
};

static struct qcom_icc_node qnm_memnoc_cnoc = {
	.name = "qnm_memnoc_cnoc",
	.id = MASTER_GEM_NOC_CNOC,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 42,
	.links = { SLAVE_AHB2PHY, SLAVE_AOSS,
		   SLAVE_APPSS, SLAVE_AUDIO,
		   SLAVE_BOOT_ROM, SLAVE_CLK_CTL,
		   SLAVE_RBCPR_CX_CFG, SLAVE_RBCPR_MXA_CFG,
		   SLAVE_RBCPR_MXC_CFG, SLAVE_CRYPTO_0_CFG,
		   SLAVE_EMAC_CFG, SLAVE_IMEM_CFG,
		   SLAVE_IPA_CFG, SLAVE_IPC_ROUTER_CFG,
		   SLAVE_CNOC_MSS, SLAVE_PCIE_0_CFG,
		   SLAVE_PCIE_RSC_CFG, SLAVE_PDM,
		   SLAVE_PMU_WRAPPER_CFG, SLAVE_PRNG,
		   SLAVE_QDSS_CFG, SLAVE_QPIC,
		   SLAVE_QUP_0, SLAVE_SDCC_4,
		   SLAVE_SPMI_VGI_COEX, SLAVE_TCSR,
		   SLAVE_TLMM, SLAVE_TME_CFG,
		   SLAVE_USB3, SLAVE_VSENSE_CTRL_CFG,
		   SLAVE_DDRSS_CFG, SLAVE_ANOC_THROTTLE_CFG,
		   SLAVE_MCDMA_THROTTLE_CFG, SLAVE_MS_NAV_THROTTLE_CFG,
		   SLAVE_PCIE_THROTTLE_CFG, SLAVE_QM_CFG,
		   SLAVE_QM_MPU_CFG, SLAVE_SNOC_THROTTLE_CFG,
		   SLAVE_BOOT_IMEM, SLAVE_IMEM,
		   SLAVE_QDSS_STM, SLAVE_TCU },
};

static struct qcom_icc_node xm_qdss_dap = {
	.name = "xm_qdss_dap",
	.id = MASTER_QDSS_DAP,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 43,
	.links = { SLAVE_AHB2PHY, SLAVE_AOSS,
		   SLAVE_APPSS, SLAVE_AUDIO,
		   SLAVE_BOOT_ROM, SLAVE_CLK_CTL,
		   SLAVE_RBCPR_CX_CFG, SLAVE_RBCPR_MXA_CFG,
		   SLAVE_RBCPR_MXC_CFG, SLAVE_CRYPTO_0_CFG,
		   SLAVE_EMAC_CFG, SLAVE_IMEM_CFG,
		   SLAVE_IPA_CFG, SLAVE_IPC_ROUTER_CFG,
		   SLAVE_CNOC_MSS, SLAVE_PCIE_0_CFG,
		   SLAVE_PCIE_RSC_CFG, SLAVE_PDM,
		   SLAVE_PMU_WRAPPER_CFG, SLAVE_PRNG,
		   SLAVE_QDSS_CFG, SLAVE_QPIC,
		   SLAVE_QUP_0, SLAVE_SDCC_4,
		   SLAVE_SPMI_VGI_COEX, SLAVE_TCSR,
		   SLAVE_TLMM, SLAVE_TME_CFG,
		   SLAVE_USB3, SLAVE_VSENSE_CTRL_CFG,
		   SLAVE_DDRSS_CFG, SLAVE_SNOC_CNOC,
		   SLAVE_ANOC_THROTTLE_CFG, SLAVE_MCDMA_THROTTLE_CFG,
		   SLAVE_MS_NAV_THROTTLE_CFG, SLAVE_PCIE_THROTTLE_CFG,
		   SLAVE_QM_CFG, SLAVE_QM_MPU_CFG,
		   SLAVE_SNOC_THROTTLE_CFG, SLAVE_BOOT_IMEM,
		   SLAVE_IMEM, SLAVE_QDSS_STM,
		   SLAVE_TCU },
};

static struct qcom_icc_node qhm_shrm_2 = {
	.name = "qhm_shrm_2",
	.id = MASTER_SHRM_2,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 8,
	.links = { SLAVE_DDRSS_REGS, SLAVE_DPCC,
		   SLAVE_LAGG_CFG, SLAVE_SHRM_2_CFG,
		   SLAVE_DDRSS_TG, SLAVE_DDRSS_MC_0,
		   SLAVE_DDRSS_MCCC_0, SLAVE_QMIP_0_CFG },
};

static struct qcom_icc_node qnm_cnoc = {
	.name = "qnm_cnoc",
	.id = MASTER_CNOC_DC_NOC,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 9,
	.links = { SLAVE_DDRSS_REGS, SLAVE_DPCC,
		   SLAVE_LAGG_CFG, SLAVE_SHRM_2_CFG,
		   SLAVE_SHRM_2_MEM, SLAVE_DDRSS_TG,
		   SLAVE_DDRSS_MC_0, SLAVE_DDRSS_MCCC_0,
		   SLAVE_QMIP_0_CFG },
};

static struct qcom_icc_node llcc_mc = {
	.name = "llcc_mc",
	.id = MASTER_LLCC,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 1,
	.links = { SLAVE_EBI1 },
};

static struct qcom_icc_qosbox qnm_pcie_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x2c000 },
	.config = &(struct qos_config) {
		.prio = 2,
		.urg_fwd = 1,
		.prio_fwd_disable = 0,
	},
};

static struct qcom_icc_node qnm_pcie = {
	.name = "qnm_pcie",
	.id = MASTER_ANOC_PCIE_GEM_NOC,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &qnm_pcie_qos,
	.num_links = 2,
	.links = { SLAVE_LLCC, SLAVE_GEM_NOC_CNOC },
};

static struct qcom_icc_qosbox qnm_snoc_sf_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x2b000 },
	.config = &(struct qos_config) {
		.prio = 0,
		.urg_fwd = 1,
		.prio_fwd_disable = 0,
	},
};

static struct qcom_icc_node qnm_snoc_sf = {
	.name = "qnm_snoc_sf",
	.id = MASTER_SNOC_SF_MEM_NOC,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &qnm_snoc_sf_qos,
	.num_links = 2,
	.links = { SLAVE_LLCC, SLAVE_GEM_NOC_CNOC },
};

static struct qcom_icc_qosbox xm_apps0_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x2d000 },
	.config = &(struct qos_config) {
		.prio = 0,
		.urg_fwd = 0,
		.prio_fwd_disable = 0,
	},
};

static struct qcom_icc_node xm_apps0 = {
	.name = "xm_apps0",
	.id = MASTER_APPSS_PROC,
	.channels = 1,
	.buswidth = 16,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &xm_apps0_qos,
	.num_links = 3,
	.links = { SLAVE_LLCC, SLAVE_GEM_NOC_CNOC,
		   SLAVE_MEM_NOC_PCIE_SNOC },
};

static struct qcom_icc_qosbox qhm_cpucp_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x13000 },
	.config = &(struct qos_config) {
		.prio = 0,
		.urg_fwd = 0,
		.prio_fwd_disable = 0,
	},
};

static struct qcom_icc_node qhm_cpucp = {
	.name = "qhm_cpucp",
	.id = MASTER_CPUCP,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &qhm_cpucp_qos,
	.num_links = 1,
	.links = { SLAVE_SNOC_MEM_NOC_SF },
};

static struct qcom_icc_node qnm_aggre_noc = {
	.name = "qnm_aggre_noc",
	.id = MASTER_ANOC_SNOC,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 2,
	.links = { SLAVE_SNOC_MEM_NOC_SF, SLAVE_PCIE_0 },
};

static struct qcom_icc_node qnm_cnoc_datapath = {
	.name = "qnm_cnoc_datapath",
	.id = MASTER_CNOC_SNOC,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 2,
	.links = { SLAVE_SNOC_MEM_NOC_SF, SLAVE_PCIE_0 },
};

static struct qcom_icc_node qnm_memnoc_pcie = {
	.name = "qnm_memnoc_pcie",
	.id = MASTER_GEM_NOC_PCIE_SNOC,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 1,
	.links = { SLAVE_PCIE_0 },
};

static struct qcom_icc_qosbox qxm_mss_nav_ce_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x12000 },
	.config = &(struct qos_config) {
		.prio = 1,
		.urg_fwd = 1,
		.prio_fwd_disable = 0,
	},
};

static struct qcom_icc_node qxm_mss_nav_ce = {
	.name = "qxm_mss_nav_ce",
	.id = MASTER_MSS_NAV,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &qxm_mss_nav_ce_qos,
	.num_links = 2,
	.links = { SLAVE_SNOC_MEM_NOC_SF, SLAVE_PCIE_0 },
};

static struct qcom_icc_qosbox qxm_tme_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x11000 },
	.config = &(struct qos_config) {
		.prio = 2,
		.urg_fwd = 0,
		.prio_fwd_disable = 0,
	},
};

static struct qcom_icc_node qxm_tme = {
	.name = "qxm_tme",
	.id = MASTER_TME,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &qxm_tme_qos,
	.num_links = 2,
	.links = { SLAVE_SNOC_MEM_NOC_SF, SLAVE_PCIE_0 },
};

static struct qcom_icc_qosbox xm_ipa2pcie_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x14000 },
	.config = &(struct qos_config) {
		.prio = 0,
		.urg_fwd = 0,
		.prio_fwd_disable = 0,
	},
};

static struct qcom_icc_node xm_ipa2pcie = {
	.name = "xm_ipa2pcie",
	.id = MASTER_IPA_PCIE,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &xm_ipa2pcie_qos,
	.num_links = 1,
	.links = { SLAVE_PCIE_0 },
};

static struct qcom_icc_node qns_a1noc = {
	.name = "qns_a1noc",
	.id = SLAVE_A1NOC_CFG,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 1,
	.links = { MASTER_ANOC_SNOC },
};

static struct qcom_icc_node qns_pcie_memnoc = {
	.name = "qns_pcie_memnoc",
	.id = SLAVE_ANOC_PCIE_GEM_NOC,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 1,
	.links = { MASTER_ANOC_PCIE_GEM_NOC },
};

static struct qcom_icc_node qhs_ahb2_phy = {
	.name = "qhs_ahb2_phy",
	.id = SLAVE_AHB2PHY,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_aoss = {
	.name = "qhs_aoss",
	.id = SLAVE_AOSS,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_apss = {
	.name = "qhs_apss",
	.id = SLAVE_APPSS,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_audio = {
	.name = "qhs_audio",
	.id = SLAVE_AUDIO,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_boot_rom = {
	.name = "qhs_boot_rom",
	.id = SLAVE_BOOT_ROM,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_clk_ctl = {
	.name = "qhs_clk_ctl",
	.id = SLAVE_CLK_CTL,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_cpr_cx = {
	.name = "qhs_cpr_cx",
	.id = SLAVE_RBCPR_CX_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_cpr_mxa = {
	.name = "qhs_cpr_mxa",
	.id = SLAVE_RBCPR_MXA_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_cpr_mxc = {
	.name = "qhs_cpr_mxc",
	.id = SLAVE_RBCPR_MXC_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_crypto_cfg = {
	.name = "qhs_crypto_cfg",
	.id = SLAVE_CRYPTO_0_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_emac0_cfg = {
	.name = "qhs_emac0_cfg",
	.id = SLAVE_EMAC_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_imem_cfg = {
	.name = "qhs_imem_cfg",
	.id = SLAVE_IMEM_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_ipa = {
	.name = "qhs_ipa",
	.id = SLAVE_IPA_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_ipc_router = {
	.name = "qhs_ipc_router",
	.id = SLAVE_IPC_ROUTER_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_mss_cfg = {
	.name = "qhs_mss_cfg",
	.id = SLAVE_CNOC_MSS,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_pcie0_cfg = {
	.name = "qhs_pcie0_cfg",
	.id = SLAVE_PCIE_0_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_pcie_rscc = {
	.name = "qhs_pcie_rscc",
	.id = SLAVE_PCIE_RSC_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_pdm = {
	.name = "qhs_pdm",
	.id = SLAVE_PDM,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_pmu_wrapper_cfg = {
	.name = "qhs_pmu_wrapper_cfg",
	.id = SLAVE_PMU_WRAPPER_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_prng = {
	.name = "qhs_prng",
	.id = SLAVE_PRNG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_qdss_cfg = {
	.name = "qhs_qdss_cfg",
	.id = SLAVE_QDSS_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_qpic = {
	.name = "qhs_qpic",
	.id = SLAVE_QPIC,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_qup0 = {
	.name = "qhs_qup0",
	.id = SLAVE_QUP_0,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_sdc4 = {
	.name = "qhs_sdc4",
	.id = SLAVE_SDCC_4,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_spmi_vgi_coex = {
	.name = "qhs_spmi_vgi_coex",
	.id = SLAVE_SPMI_VGI_COEX,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_tcsr = {
	.name = "qhs_tcsr",
	.id = SLAVE_TCSR,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_tlmm = {
	.name = "qhs_tlmm",
	.id = SLAVE_TLMM,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_tme_cfg = {
	.name = "qhs_tme_cfg",
	.id = SLAVE_TME_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_usb3 = {
	.name = "qhs_usb3",
	.id = SLAVE_USB3,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_vsense_ctrl_cfg = {
	.name = "qhs_vsense_ctrl_cfg",
	.id = SLAVE_VSENSE_CTRL_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qns_ddrss_cfg = {
	.name = "qns_ddrss_cfg",
	.id = SLAVE_DDRSS_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 1,
	.links = { MASTER_CNOC_DC_NOC },
};

static struct qcom_icc_node qns_snoc_datapath = {
	.name = "qns_snoc_datapath",
	.id = SLAVE_SNOC_CNOC,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 1,
	.links = { MASTER_CNOC_SNOC },
};

static struct qcom_icc_node qss_anoc_throttle_cfg = {
	.name = "qss_anoc_throttle_cfg",
	.id = SLAVE_ANOC_THROTTLE_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qss_modem_throttle_cfg = {
	.name = "qss_modem_throttle_cfg",
	.id = SLAVE_MCDMA_THROTTLE_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qss_mss_nav_ce_throttle_cfg = {
	.name = "qss_mss_nav_ce_throttle_cfg",
	.id = SLAVE_MS_NAV_THROTTLE_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qss_pcie_throttle_cfg = {
	.name = "qss_pcie_throttle_cfg",
	.id = SLAVE_PCIE_THROTTLE_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qss_qm_cfg = {
	.name = "qss_qm_cfg",
	.id = SLAVE_QM_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qss_qm_mpu_cfg = {
	.name = "qss_qm_mpu_cfg",
	.id = SLAVE_QM_MPU_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qss_snoc_throttle_cfg = {
	.name = "qss_snoc_throttle_cfg",
	.id = SLAVE_SNOC_THROTTLE_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qxs_boot_imem = {
	.name = "qxs_boot_imem",
	.id = SLAVE_BOOT_IMEM,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qxs_imem = {
	.name = "qxs_imem",
	.id = SLAVE_IMEM,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node xs_qdss_stm = {
	.name = "xs_qdss_stm",
	.id = SLAVE_QDSS_STM,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node xs_sys_tcu_cfg = {
	.name = "xs_sys_tcu_cfg",
	.id = SLAVE_TCU,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_ddrss_regs = {
	.name = "qhs_ddrss_regs",
	.id = SLAVE_DDRSS_REGS,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_dpcc = {
	.name = "qhs_dpcc",
	.id = SLAVE_DPCC,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_lagg = {
	.name = "qhs_lagg",
	.id = SLAVE_LAGG_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_shrm_2_csr = {
	.name = "qhs_shrm_2_csr",
	.id = SLAVE_SHRM_2_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_shrm_2_mem = {
	.name = "qhs_shrm_2_mem",
	.id = SLAVE_SHRM_2_MEM,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_tg = {
	.name = "qhs_tg",
	.id = SLAVE_DDRSS_TG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qss_mc_0 = {
	.name = "qss_mc_0",
	.id = SLAVE_DDRSS_MC_0,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qss_mccc_0 = {
	.name = "qss_mccc_0",
	.id = SLAVE_DDRSS_MCCC_0,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qss_qmip0 = {
	.name = "qss_qmip0",
	.id = SLAVE_QMIP_0_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node ebi = {
	.name = "ebi",
	.id = SLAVE_EBI1,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qns_llcc = {
	.name = "qns_llcc",
	.id = SLAVE_LLCC,
	.channels = 1,
	.buswidth = 16,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 1,
	.links = { MASTER_LLCC },
};

static struct qcom_icc_node qns_memnoc_cnoc = {
	.name = "qns_memnoc_cnoc",
	.id = SLAVE_GEM_NOC_CNOC,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 1,
	.links = { MASTER_GEM_NOC_CNOC },
};

static struct qcom_icc_node qns_pcie = {
	.name = "qns_pcie",
	.id = SLAVE_MEM_NOC_PCIE_SNOC,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 1,
	.links = { MASTER_GEM_NOC_PCIE_SNOC },
};

static struct qcom_icc_node qns_memnoc_sf = {
	.name = "qns_memnoc_sf",
	.id = SLAVE_SNOC_MEM_NOC_SF,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 1,
	.links = { MASTER_SNOC_SF_MEM_NOC },
};

static struct qcom_icc_node xs_pcie_0 = {
	.name = "xs_pcie_0",
	.id = SLAVE_PCIE_0,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_bcm bcm_acv = {
	.name = "ACV",
	.voter_idx = 0,
	.num_nodes = 1,
	.nodes = { &ebi },
};

static struct qcom_icc_bcm bcm_cn0 = {
	.name = "CN0",
	.voter_idx = 0,
	.num_nodes = 43,
	.nodes = { &qhs_ahb2_phy, &qhs_aoss,
		   &qhs_apss, &qhs_audio,
		   &qhs_boot_rom, &qhs_clk_ctl,
		   &qhs_cpr_cx, &qhs_cpr_mxa,
		   &qhs_cpr_mxc, &qhs_crypto_cfg,
		   &qhs_emac0_cfg, &qhs_imem_cfg,
		   &qhs_ipa, &qhs_ipc_router,
		   &qhs_mss_cfg, &qhs_pcie0_cfg,
		   &qhs_pcie_rscc, &qhs_pdm,
		   &qhs_pmu_wrapper_cfg, &qhs_prng,
		   &qhs_qdss_cfg, &qhs_qpic,
		   &qhs_qup0, &qhs_sdc4,
		   &qhs_spmi_vgi_coex, &qhs_tcsr,
		   &qhs_tlmm, &qhs_tme_cfg,
		   &qhs_usb3, &qhs_vsense_ctrl_cfg,
		   &qns_ddrss_cfg, &qns_snoc_datapath,
		   &qss_anoc_throttle_cfg, &qss_modem_throttle_cfg,
		   &qss_mss_nav_ce_throttle_cfg, &qss_pcie_throttle_cfg,
		   &qss_qm_cfg, &qss_qm_mpu_cfg,
		   &qss_snoc_throttle_cfg, &qxs_boot_imem,
		   &qxs_imem, &xs_qdss_stm,
		   &xs_sys_tcu_cfg },
};

static struct qcom_icc_bcm bcm_mc0 = {
	.name = "MC0",
	.voter_idx = 0,
	.num_nodes = 1,
	.nodes = { &ebi },
};

static struct qcom_icc_bcm bcm_sh0 = {
	.name = "SH0",
	.voter_idx = 0,
	.num_nodes = 1,
	.nodes = { &qns_llcc },
};

static struct qcom_icc_bcm bcm_sh1 = {
	.name = "SH1",
	.voter_idx = 0,
	.num_nodes = 2,
	.nodes = { &qns_memnoc_cnoc, &qns_pcie },
};

static struct qcom_icc_bcm bcm_sn0 = {
	.name = "SN0",
	.voter_idx = 0,
	.num_nodes = 1,
	.nodes = { &qns_memnoc_sf },
};

static struct qcom_icc_bcm bcm_sn7 = {
	.name = "SN7",
	.voter_idx = 0,
	.num_nodes = 1,
	.nodes = { &xs_pcie_0 },
};

static struct qcom_icc_bcm *aggre_noc_bcms[] = {
};

static struct qcom_icc_node *aggre_noc_nodes[] = {
	[MASTER_AUDIO] = &qhm_audio,
	[MASTER_QDSS_BAM] = &qhm_qdss_bam,
	[MASTER_QPIC] = &qhm_qpic,
	[MASTER_QUP_0] = &qhm_qup0,
	[MASTER_CRYPTO] = &qxm_crypto,
	[MASTER_IPA] = &qxm_ipa,
	[MASTER_EMAC_0] = &xm_emac_0,
	[MASTER_PCIE_0] = &xm_pcie3_0,
	[MASTER_QDSS_ETR] = &xm_qdss_etr0,
	[MASTER_QDSS_ETR_1] = &xm_qdss_etr1,
	[MASTER_SDCC_4] = &xm_sdc4,
	[MASTER_USB3_0] = &xm_usb3,
	[SLAVE_A1NOC_CFG] = &qns_a1noc,
	[SLAVE_ANOC_PCIE_GEM_NOC] = &qns_pcie_memnoc,
};

static char *aggre_noc_voters[] = {
	"hlos",
};

static struct qcom_icc_desc sdxbaagha_aggre_noc = {
	.config = &icc_regmap_config,
	.nodes = aggre_noc_nodes,
	.num_nodes = ARRAY_SIZE(aggre_noc_nodes),
	.bcms = aggre_noc_bcms,
	.num_bcms = ARRAY_SIZE(aggre_noc_bcms),
	.voters = aggre_noc_voters,
	.num_voters = ARRAY_SIZE(aggre_noc_voters),
};

static struct qcom_icc_bcm *cnoc_main_bcms[] = {
	&bcm_cn0,
};

static struct qcom_icc_node *cnoc_main_nodes[] = {
	[MASTER_PCIE_RSCC] = &qhm_pcie_rscc,
	[MASTER_GEM_NOC_CNOC] = &qnm_memnoc_cnoc,
	[MASTER_QDSS_DAP] = &xm_qdss_dap,
	[SLAVE_AHB2PHY] = &qhs_ahb2_phy,
	[SLAVE_AOSS] = &qhs_aoss,
	[SLAVE_APPSS] = &qhs_apss,
	[SLAVE_AUDIO] = &qhs_audio,
	[SLAVE_BOOT_ROM] = &qhs_boot_rom,
	[SLAVE_CLK_CTL] = &qhs_clk_ctl,
	[SLAVE_RBCPR_CX_CFG] = &qhs_cpr_cx,
	[SLAVE_RBCPR_MXA_CFG] = &qhs_cpr_mxa,
	[SLAVE_RBCPR_MXC_CFG] = &qhs_cpr_mxc,
	[SLAVE_CRYPTO_0_CFG] = &qhs_crypto_cfg,
	[SLAVE_EMAC_CFG] = &qhs_emac0_cfg,
	[SLAVE_IMEM_CFG] = &qhs_imem_cfg,
	[SLAVE_IPA_CFG] = &qhs_ipa,
	[SLAVE_IPC_ROUTER_CFG] = &qhs_ipc_router,
	[SLAVE_CNOC_MSS] = &qhs_mss_cfg,
	[SLAVE_PCIE_0_CFG] = &qhs_pcie0_cfg,
	[SLAVE_PCIE_RSC_CFG] = &qhs_pcie_rscc,
	[SLAVE_PDM] = &qhs_pdm,
	[SLAVE_PMU_WRAPPER_CFG] = &qhs_pmu_wrapper_cfg,
	[SLAVE_PRNG] = &qhs_prng,
	[SLAVE_QDSS_CFG] = &qhs_qdss_cfg,
	[SLAVE_QPIC] = &qhs_qpic,
	[SLAVE_QUP_0] = &qhs_qup0,
	[SLAVE_SDCC_4] = &qhs_sdc4,
	[SLAVE_SPMI_VGI_COEX] = &qhs_spmi_vgi_coex,
	[SLAVE_TCSR] = &qhs_tcsr,
	[SLAVE_TLMM] = &qhs_tlmm,
	[SLAVE_TME_CFG] = &qhs_tme_cfg,
	[SLAVE_USB3] = &qhs_usb3,
	[SLAVE_VSENSE_CTRL_CFG] = &qhs_vsense_ctrl_cfg,
	[SLAVE_DDRSS_CFG] = &qns_ddrss_cfg,
	[SLAVE_SNOC_CNOC] = &qns_snoc_datapath,
	[SLAVE_ANOC_THROTTLE_CFG] = &qss_anoc_throttle_cfg,
	[SLAVE_MCDMA_THROTTLE_CFG] = &qss_modem_throttle_cfg,
	[SLAVE_MS_NAV_THROTTLE_CFG] = &qss_mss_nav_ce_throttle_cfg,
	[SLAVE_PCIE_THROTTLE_CFG] = &qss_pcie_throttle_cfg,
	[SLAVE_QM_CFG] = &qss_qm_cfg,
	[SLAVE_QM_MPU_CFG] = &qss_qm_mpu_cfg,
	[SLAVE_SNOC_THROTTLE_CFG] = &qss_snoc_throttle_cfg,
	[SLAVE_BOOT_IMEM] = &qxs_boot_imem,
	[SLAVE_IMEM] = &qxs_imem,
	[SLAVE_QDSS_STM] = &xs_qdss_stm,
	[SLAVE_TCU] = &xs_sys_tcu_cfg,
};

static char *cnoc_main_voters[] = {
	"hlos",
};

static struct qcom_icc_desc sdxbaagha_cnoc_main = {
	.config = &icc_regmap_config,
	.nodes = cnoc_main_nodes,
	.num_nodes = ARRAY_SIZE(cnoc_main_nodes),
	.bcms = cnoc_main_bcms,
	.num_bcms = ARRAY_SIZE(cnoc_main_bcms),
	.voters = cnoc_main_voters,
	.num_voters = ARRAY_SIZE(cnoc_main_voters),
};

static struct qcom_icc_bcm *dc_noc_dch_bcms[] = {
};

static struct qcom_icc_node *dc_noc_dch_nodes[] = {
	[MASTER_SHRM_2] = &qhm_shrm_2,
	[MASTER_CNOC_DC_NOC] = &qnm_cnoc,
	[SLAVE_DDRSS_REGS] = &qhs_ddrss_regs,
	[SLAVE_DPCC] = &qhs_dpcc,
	[SLAVE_LAGG_CFG] = &qhs_lagg,
	[SLAVE_SHRM_2_CFG] = &qhs_shrm_2_csr,
	[SLAVE_SHRM_2_MEM] = &qhs_shrm_2_mem,
	[SLAVE_DDRSS_TG] = &qhs_tg,
	[SLAVE_DDRSS_MC_0] = &qss_mc_0,
	[SLAVE_DDRSS_MCCC_0] = &qss_mccc_0,
	[SLAVE_QMIP_0_CFG] = &qss_qmip0,
};

static char *dc_noc_dch_voters[] = {
	"hlos",
};

static struct qcom_icc_desc sdxbaagha_dc_noc_dch = {
	.config = &icc_regmap_config,
	.nodes = dc_noc_dch_nodes,
	.num_nodes = ARRAY_SIZE(dc_noc_dch_nodes),
	.bcms = dc_noc_dch_bcms,
	.num_bcms = ARRAY_SIZE(dc_noc_dch_bcms),
	.voters = dc_noc_dch_voters,
	.num_voters = ARRAY_SIZE(dc_noc_dch_voters),
};

static struct qcom_icc_bcm *mc_virt_bcms[] = {
	&bcm_acv,
	&bcm_mc0,
};

static struct qcom_icc_node *mc_virt_nodes[] = {
	[MASTER_LLCC] = &llcc_mc,
	[SLAVE_EBI1] = &ebi,
};

static char *mc_virt_voters[] = {
	"hlos",
};

static struct qcom_icc_desc sdxbaagha_mc_virt = {
	.config = &icc_regmap_config,
	.nodes = mc_virt_nodes,
	.num_nodes = ARRAY_SIZE(mc_virt_nodes),
	.bcms = mc_virt_bcms,
	.num_bcms = ARRAY_SIZE(mc_virt_bcms),
	.voters = mc_virt_voters,
	.num_voters = ARRAY_SIZE(mc_virt_voters),
};

static struct qcom_icc_bcm *mem_noc_bcms[] = {
	&bcm_sh0,
	&bcm_sh1,
};

static struct qcom_icc_node *mem_noc_nodes[] = {
	[MASTER_ANOC_PCIE_GEM_NOC] = &qnm_pcie,
	[MASTER_SNOC_SF_MEM_NOC] = &qnm_snoc_sf,
	[MASTER_APPSS_PROC] = &xm_apps0,
	[SLAVE_LLCC] = &qns_llcc,
	[SLAVE_GEM_NOC_CNOC] = &qns_memnoc_cnoc,
	[SLAVE_MEM_NOC_PCIE_SNOC] = &qns_pcie,
};

static char *mem_noc_voters[] = {
	"hlos",
};

static struct qcom_icc_desc sdxbaagha_mem_noc = {
	.config = &icc_regmap_config,
	.nodes = mem_noc_nodes,
	.num_nodes = ARRAY_SIZE(mem_noc_nodes),
	.bcms = mem_noc_bcms,
	.num_bcms = ARRAY_SIZE(mem_noc_bcms),
	.voters = mem_noc_voters,
	.num_voters = ARRAY_SIZE(mem_noc_voters),
};

static struct qcom_icc_bcm *snoc_bcms[] = {
	&bcm_sn0,
	&bcm_sn7,
};

static struct qcom_icc_node *snoc_nodes[] = {
	[MASTER_CPUCP] = &qhm_cpucp,
	[MASTER_ANOC_SNOC] = &qnm_aggre_noc,
	[MASTER_CNOC_SNOC] = &qnm_cnoc_datapath,
	[MASTER_GEM_NOC_PCIE_SNOC] = &qnm_memnoc_pcie,
	[MASTER_MSS_NAV] = &qxm_mss_nav_ce,
	[MASTER_TME] = &qxm_tme,
	[MASTER_IPA_PCIE] = &xm_ipa2pcie,
	[SLAVE_SNOC_MEM_NOC_SF] = &qns_memnoc_sf,
	[SLAVE_PCIE_0] = &xs_pcie_0,
};

static char *snoc_voters[] = {
	"hlos",
};

static struct qcom_icc_desc sdxbaagha_snoc = {
	.config = &icc_regmap_config,
	.nodes = snoc_nodes,
	.num_nodes = ARRAY_SIZE(snoc_nodes),
	.bcms = snoc_bcms,
	.num_bcms = ARRAY_SIZE(snoc_bcms),
	.voters = snoc_voters,
	.num_voters = ARRAY_SIZE(snoc_voters),
};

static int qnoc_probe(struct platform_device *pdev)
{
	int ret;

	ret = qcom_icc_rpmh_probe(pdev);
	if (ret)
		dev_err(&pdev->dev, "failed to register ICC provider\n");
	else
		dev_info(&pdev->dev, "Registered SDXBAAGHA ICC\n");

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

	return icc_provider_del(provider);
}

static const struct of_device_id qnoc_of_match[] = {
	{ .compatible = "qcom,sdxbaagha-aggre_noc",
	  .data = &sdxbaagha_aggre_noc},
	{ .compatible = "qcom,sdxbaagha-cnoc_main",
	  .data = &sdxbaagha_cnoc_main},
	{ .compatible = "qcom,sdxbaagha-dc_noc_dch",
	  .data = &sdxbaagha_dc_noc_dch},
	{ .compatible = "qcom,sdxbaagha-mc_virt",
	  .data = &sdxbaagha_mc_virt},
	{ .compatible = "qcom,sdxbaagha-mem_noc",
	  .data = &sdxbaagha_mem_noc},
	{ .compatible = "qcom,sdxbaagha-snoc",
	  .data = &sdxbaagha_snoc},
	{ }
};
MODULE_DEVICE_TABLE(of, qnoc_of_match);

static struct platform_driver qnoc_driver = {
	.probe = qnoc_probe,
	.remove = qnoc_remove,
	.driver = {
		.name = "qnoc-sdxbaagha",
		.of_match_table = qnoc_of_match,
		.sync_state = qcom_icc_rpmh_sync_state,
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

MODULE_DESCRIPTION("SDXBAAGHA NoC driver");
MODULE_LICENSE("GPL v2");
