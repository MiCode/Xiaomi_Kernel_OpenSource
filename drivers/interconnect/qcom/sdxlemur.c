// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 */

#include <asm/div64.h>
#include <dt-bindings/interconnect/qcom,sdxlemur.h>
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

static struct qcom_icc_node llcc_mc = {
	.name = "llcc_mc",
	.id = MASTER_LLCC,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 1,
	.links = { SLAVE_EBI1 },
};

static struct qcom_icc_qosbox acm_tcu_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x10000 },
	.config = &(struct qos_config) {
		.prio = 6,
		.urg_fwd = 0,
	},
};

static struct qcom_icc_node acm_tcu = {
	.name = "acm_tcu",
	.id = MASTER_TCU_0,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &acm_tcu_qos,
	.num_links = 3,
	.links = { SLAVE_LLCC, SLAVE_MEM_NOC_SNOC,
		   SLAVE_MEM_NOC_PCIE_SNOC },
};

static struct qcom_icc_qosbox qnm_snoc_gc_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x18000 },
	.config = &(struct qos_config) {
		.prio = 0,
		.urg_fwd = 1,
	},
};

static struct qcom_icc_node qnm_snoc_gc = {
	.name = "qnm_snoc_gc",
	.id = MASTER_SNOC_GC_MEM_NOC,
	.channels = 1,
	.buswidth = 16,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &qnm_snoc_gc_qos,
	.num_links = 1,
	.links = { SLAVE_LLCC },
};

static struct qcom_icc_qosbox xm_apps_rdwr_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x13000 },
	.config = &(struct qos_config) {
		.prio = 0,
		.urg_fwd = 0,
	},
};

static struct qcom_icc_node xm_apps_rdwr = {
	.name = "xm_apps_rdwr",
	.id = MASTER_APPSS_PROC,
	.channels = 1,
	.buswidth = 16,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &xm_apps_rdwr_qos,
	.num_links = 3,
	.links = { SLAVE_LLCC, SLAVE_MEM_NOC_SNOC,
		   SLAVE_MEM_NOC_PCIE_SNOC },
};

static struct qcom_icc_qosbox qhm_audio_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x15000 },
	.config = &(struct qos_config) {
		.prio = 0,
		.urg_fwd = 0,
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
	.links = { SLAVE_ANOC_SNOC },
};

static struct qcom_icc_qosbox qhm_blsp1_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x16000 },
	.config = &(struct qos_config) {
		.prio = 0,
		.urg_fwd = 0,
	},
};

static struct qcom_icc_node qhm_blsp1 = {
	.name = "qhm_blsp1",
	.id = MASTER_BLSP_1,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &qhm_blsp1_qos,
	.num_links = 1,
	.links = { SLAVE_ANOC_SNOC },
};

static struct qcom_icc_qosbox qhm_qdss_bam_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x17000 },
	.config = &(struct qos_config) {
		.prio = 0,
		.urg_fwd = 0,
	},
};

static struct qcom_icc_node qhm_qdss_bam = {
	.name = "qhm_qdss_bam",
	.id = MASTER_QDSS_BAM,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &qhm_qdss_bam_qos,
	.num_links = 26,
	.links = { SLAVE_AOSS, SLAVE_AUDIO,
		   SLAVE_BLSP_1, SLAVE_CLK_CTL,
		   SLAVE_CRYPTO_0_CFG, SLAVE_CNOC_DDRSS,
		   SLAVE_ECC_CFG, SLAVE_IMEM_CFG,
		   SLAVE_IPA_CFG, SLAVE_CNOC_MSS,
		   SLAVE_PCIE_PARF, SLAVE_PDM,
		   SLAVE_PRNG, SLAVE_QDSS_CFG,
		   SLAVE_QPIC, SLAVE_SDCC_1,
		   SLAVE_SNOC_CFG, SLAVE_SPMI_FETCHER,
		   SLAVE_SPMI_VGI_COEX, SLAVE_TCSR,
		   SLAVE_TLMM, SLAVE_USB3,
		   SLAVE_USB3_PHY_CFG, SLAVE_SNOC_MEM_NOC_GC,
		   SLAVE_IMEM, SLAVE_TCU },
};

static struct qcom_icc_node qhm_qpic = {
	.name = "qhm_qpic",
	.id = MASTER_QPIC,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 4,
	.links = { SLAVE_AOSS, SLAVE_AUDIO,
		   SLAVE_IPA_CFG, SLAVE_ANOC_SNOC },
};

static struct qcom_icc_node qhm_snoc_cfg = {
	.name = "qhm_snoc_cfg",
	.id = MASTER_SNOC_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 1,
	.links = { SLAVE_SERVICE_SNOC },
};

static struct qcom_icc_qosbox qhm_spmi_fetcher1_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x15000 },
	.config = &(struct qos_config) {
		.prio = 0,
		.urg_fwd = 0,
	},
};

static struct qcom_icc_node qhm_spmi_fetcher1 = {
	.name = "qhm_spmi_fetcher1",
	.id = MASTER_SPMI_FETCHER,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &qhm_spmi_fetcher1_qos,
	.num_links = 2,
	.links = { SLAVE_AOSS, SLAVE_ANOC_SNOC },
};

static struct qcom_icc_qosbox qnm_aggre_noc_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x18000 },
	.config = &(struct qos_config) {
		.prio = 0,
		.urg_fwd = 0,
	},
};

static struct qcom_icc_node qnm_aggre_noc = {
	.name = "qnm_aggre_noc",
	.id = MASTER_ANOC_SNOC,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &qnm_aggre_noc_qos,
	.num_links = 29,
	.links = { SLAVE_AOSS, SLAVE_APPSS,
		   SLAVE_AUDIO, SLAVE_BLSP_1,
		   SLAVE_CLK_CTL, SLAVE_CRYPTO_0_CFG,
		   SLAVE_CNOC_DDRSS, SLAVE_ECC_CFG,
		   SLAVE_IMEM_CFG, SLAVE_IPA_CFG,
		   SLAVE_CNOC_MSS, SLAVE_PCIE_PARF,
		   SLAVE_PDM, SLAVE_PRNG,
		   SLAVE_QDSS_CFG, SLAVE_QPIC,
		   SLAVE_SDCC_1, SLAVE_SNOC_CFG,
		   SLAVE_SPMI_FETCHER, SLAVE_SPMI_VGI_COEX,
		   SLAVE_TCSR, SLAVE_TLMM,
		   SLAVE_USB3, SLAVE_USB3_PHY_CFG,
		   SLAVE_SNOC_MEM_NOC_GC, SLAVE_IMEM,
		   SLAVE_PCIE_0, SLAVE_QDSS_STM,
		   SLAVE_TCU },
};

static struct qcom_icc_qosbox qnm_ipa_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x11000 },
	.config = &(struct qos_config) {
		.prio = 0,
		.urg_fwd = 0,
	},
};

static struct qcom_icc_node qnm_ipa = {
	.name = "qnm_ipa",
	.id = MASTER_IPA,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &qnm_ipa_qos,
	.num_links = 26,
	.links = { SLAVE_AOSS, SLAVE_AUDIO,
		   SLAVE_BLSP_1, SLAVE_CLK_CTL,
		   SLAVE_CRYPTO_0_CFG, SLAVE_CNOC_DDRSS,
		   SLAVE_ECC_CFG, SLAVE_IMEM_CFG,
		   SLAVE_IPA_CFG, SLAVE_CNOC_MSS,
		   SLAVE_PCIE_PARF, SLAVE_PDM,
		   SLAVE_PRNG, SLAVE_QDSS_CFG,
		   SLAVE_QPIC, SLAVE_SDCC_1,
		   SLAVE_SNOC_CFG, SLAVE_SPMI_FETCHER,
		   SLAVE_TCSR, SLAVE_TLMM,
		   SLAVE_USB3, SLAVE_USB3_PHY_CFG,
		   SLAVE_SNOC_MEM_NOC_GC, SLAVE_IMEM,
		   SLAVE_PCIE_0, SLAVE_QDSS_STM },
};

static struct qcom_icc_node qnm_memnoc = {
	.name = "qnm_memnoc",
	.id = MASTER_MEM_NOC_SNOC,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 27,
	.links = { SLAVE_AOSS, SLAVE_APPSS,
		   SLAVE_AUDIO, SLAVE_BLSP_1,
		   SLAVE_CLK_CTL, SLAVE_CRYPTO_0_CFG,
		   SLAVE_CNOC_DDRSS, SLAVE_ECC_CFG,
		   SLAVE_IMEM_CFG, SLAVE_IPA_CFG,
		   SLAVE_CNOC_MSS, SLAVE_PCIE_PARF,
		   SLAVE_PDM, SLAVE_PRNG,
		   SLAVE_QDSS_CFG, SLAVE_QPIC,
		   SLAVE_SDCC_1, SLAVE_SNOC_CFG,
		   SLAVE_SPMI_FETCHER, SLAVE_SPMI_VGI_COEX,
		   SLAVE_TCSR, SLAVE_TLMM,
		   SLAVE_USB3, SLAVE_USB3_PHY_CFG,
		   SLAVE_IMEM, SLAVE_QDSS_STM,
		   SLAVE_TCU },
};

static struct qcom_icc_node qnm_memnoc_pcie = {
	.name = "qnm_memnoc_pcie",
	.id = MASTER_MEM_NOC_PCIE_SNOC,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 1,
	.links = { SLAVE_PCIE_0 },
};

static struct qcom_icc_qosbox qxm_crypto_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0xd000 },
	.config = &(struct qos_config) {
		.prio = 0,
		.urg_fwd = 0,
	},
};

static struct qcom_icc_node qxm_crypto = {
	.name = "qxm_crypto",
	.id = MASTER_CRYPTO,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &qxm_crypto_qos,
	.num_links = 2,
	.links = { SLAVE_AOSS, SLAVE_ANOC_SNOC },
};

static struct qcom_icc_qosbox xm_ipa2pcie_slv_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x12000 },
	.config = &(struct qos_config) {
		.prio = 0,
		.urg_fwd = 0,
	},
};

static struct qcom_icc_node xm_ipa2pcie_slv = {
	.name = "xm_ipa2pcie_slv",
	.id = MASTER_IPA_PCIE,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &xm_ipa2pcie_slv_qos,
	.num_links = 1,
	.links = { SLAVE_PCIE_0 },
};

static struct qcom_icc_qosbox xm_pcie_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0xe000 },
	.config = &(struct qos_config) {
		.prio = 0,
		.urg_fwd = 0,
	},
};

static struct qcom_icc_node xm_pcie = {
	.name = "xm_pcie",
	.id = MASTER_PCIE_0,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &xm_pcie_qos,
	.num_links = 1,
	.links = { SLAVE_ANOC_SNOC },
};

static struct qcom_icc_qosbox xm_qdss_etr_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0xf000 },
	.config = &(struct qos_config) {
		.prio = 0,
		.urg_fwd = 0,
	},
};

static struct qcom_icc_node xm_qdss_etr = {
	.name = "xm_qdss_etr",
	.id = MASTER_QDSS_ETR,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &xm_qdss_etr_qos,
	.num_links = 26,
	.links = { SLAVE_AOSS, SLAVE_AUDIO,
		   SLAVE_BLSP_1, SLAVE_CLK_CTL,
		   SLAVE_CRYPTO_0_CFG, SLAVE_CNOC_DDRSS,
		   SLAVE_ECC_CFG, SLAVE_IMEM_CFG,
		   SLAVE_IPA_CFG, SLAVE_CNOC_MSS,
		   SLAVE_PCIE_PARF, SLAVE_PDM,
		   SLAVE_PRNG, SLAVE_QDSS_CFG,
		   SLAVE_QPIC, SLAVE_SDCC_1,
		   SLAVE_SNOC_CFG, SLAVE_SPMI_FETCHER,
		   SLAVE_SPMI_VGI_COEX, SLAVE_TCSR,
		   SLAVE_TLMM, SLAVE_USB3,
		   SLAVE_USB3_PHY_CFG, SLAVE_SNOC_MEM_NOC_GC,
		   SLAVE_IMEM, SLAVE_TCU },
};

static struct qcom_icc_qosbox xm_sdc1_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x14000 },
	.config = &(struct qos_config) {
		.prio = 0,
		.urg_fwd = 0,
	},
};

static struct qcom_icc_node xm_sdc1 = {
	.name = "xm_sdc1",
	.id = MASTER_SDCC_1,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &xm_sdc1_qos,
	.num_links = 4,
	.links = { SLAVE_AOSS, SLAVE_AUDIO,
		   SLAVE_IPA_CFG, SLAVE_ANOC_SNOC },
};

static struct qcom_icc_qosbox xm_usb3_qos = {
	.regs = icc_qnoc_qos_regs[ICC_QNOC_QOSGEN_TYPE_RPMH],
	.num_ports = 1,
	.offsets = { 0x10000 },
	.config = &(struct qos_config) {
		.prio = 0,
		.urg_fwd = 0,
	},
};

static struct qcom_icc_node xm_usb3 = {
	.name = "xm_usb3",
	.id = MASTER_USB3,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.qosbox = &xm_usb3_qos,
	.num_links = 1,
	.links = { SLAVE_ANOC_SNOC },
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

static struct qcom_icc_node qns_memnoc_snoc = {
	.name = "qns_memnoc_snoc",
	.id = SLAVE_MEM_NOC_SNOC,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 1,
	.links = { MASTER_MEM_NOC_SNOC },
};

static struct qcom_icc_node qns_sys_pcie = {
	.name = "qns_sys_pcie",
	.id = SLAVE_MEM_NOC_PCIE_SNOC,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 1,
	.links = { MASTER_MEM_NOC_PCIE_SNOC },
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

static struct qcom_icc_node qhs_blsp1 = {
	.name = "qhs_blsp1",
	.id = SLAVE_BLSP_1,
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

static struct qcom_icc_node qhs_crypto0_cfg = {
	.name = "qhs_crypto0_cfg",
	.id = SLAVE_CRYPTO_0_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_ddrss_cfg = {
	.name = "qhs_ddrss_cfg",
	.id = SLAVE_CNOC_DDRSS,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_ecc_cfg = {
	.name = "qhs_ecc_cfg",
	.id = SLAVE_ECC_CFG,
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

static struct qcom_icc_node qhs_mss_cfg = {
	.name = "qhs_mss_cfg",
	.id = SLAVE_CNOC_MSS,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_pcie_parf = {
	.name = "qhs_pcie_parf",
	.id = SLAVE_PCIE_PARF,
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

static struct qcom_icc_node qhs_sdc1 = {
	.name = "qhs_sdc1",
	.id = SLAVE_SDCC_1,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_snoc_cfg = {
	.name = "qhs_snoc_cfg",
	.id = SLAVE_SNOC_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 1,
	.links = { MASTER_SNOC_CFG },
};

static struct qcom_icc_node qhs_spmi_fetcher = {
	.name = "qhs_spmi_fetcher",
	.id = SLAVE_SPMI_FETCHER,
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

static struct qcom_icc_node qhs_usb3 = {
	.name = "qhs_usb3",
	.id = SLAVE_USB3,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qhs_usb3_phy = {
	.name = "qhs_usb3_phy",
	.id = SLAVE_USB3_PHY_CFG,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node qns_aggre_noc = {
	.name = "qns_aggre_noc",
	.id = SLAVE_ANOC_SNOC,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 1,
	.links = { MASTER_ANOC_SNOC },
};

static struct qcom_icc_node qns_snoc_memnoc = {
	.name = "qns_snoc_memnoc",
	.id = SLAVE_SNOC_MEM_NOC_GC,
	.channels = 1,
	.buswidth = 16,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 1,
	.links = { MASTER_SNOC_GC_MEM_NOC },
};

static struct qcom_icc_node qxs_imem = {
	.name = "qxs_imem",
	.id = SLAVE_IMEM,
	.channels = 1,
	.buswidth = 8,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node srvc_snoc = {
	.name = "srvc_snoc",
	.id = SLAVE_SERVICE_SNOC,
	.channels = 1,
	.buswidth = 4,
	.noc_ops = &qcom_qnoc4_ops,
	.num_links = 0,
};

static struct qcom_icc_node xs_pcie = {
	.name = "xs_pcie",
	.id = SLAVE_PCIE_0,
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

static struct qcom_icc_bcm bcm_ce0 = {
	.name = "CE0",
	.voter_idx = 0,
	.num_nodes = 1,
	.nodes = { &qxm_crypto },
};

static struct qcom_icc_bcm bcm_mc0 = {
	.name = "MC0",
	.voter_idx = 0,
	.num_nodes = 1,
	.nodes = { &ebi },
};

static struct qcom_icc_bcm bcm_pn0 = {
	.name = "PN0",
	.voter_idx = 0,
	.num_nodes = 26,
	.nodes = { &qhm_snoc_cfg, &qhs_aoss,
		   &qhs_apss, &qhs_audio,
		   &qhs_blsp1, &qhs_clk_ctl,
		   &qhs_crypto0_cfg, &qhs_ddrss_cfg,
		   &qhs_ecc_cfg, &qhs_imem_cfg,
		   &qhs_ipa, &qhs_mss_cfg,
		   &qhs_pcie_parf, &qhs_pdm,
		   &qhs_prng, &qhs_qdss_cfg,
		   &qhs_qpic, &qhs_sdc1,
		   &qhs_snoc_cfg, &qhs_spmi_fetcher,
		   &qhs_spmi_vgi_coex, &qhs_tcsr,
		   &qhs_tlmm, &qhs_usb3,
		   &qhs_usb3_phy, &srvc_snoc },
};

static struct qcom_icc_bcm bcm_pn1 = {
	.name = "PN1",
	.voter_idx = 0,
	.num_nodes = 1,
	.nodes = { &xm_sdc1 },
};

static struct qcom_icc_bcm bcm_pn2 = {
	.name = "PN2",
	.voter_idx = 0,
	.num_nodes = 2,
	.nodes = { &qhm_audio, &qhm_spmi_fetcher1 },
};

static struct qcom_icc_bcm bcm_pn3 = {
	.name = "PN3",
	.voter_idx = 0,
	.num_nodes = 2,
	.nodes = { &qhm_blsp1, &qhm_qpic },
};

static struct qcom_icc_bcm bcm_pn4 = {
	.name = "PN4",
	.voter_idx = 0,
	.num_nodes = 1,
	.nodes = { &qxm_crypto },
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
	.num_nodes = 1,
	.nodes = { &qns_memnoc_snoc },
};

static struct qcom_icc_bcm bcm_sh3 = {
	.name = "SH3",
	.voter_idx = 0,
	.num_nodes = 1,
	.nodes = { &xm_apps_rdwr },
};

static struct qcom_icc_bcm bcm_sn0 = {
	.name = "SN0",
	.voter_idx = 0,
	.num_nodes = 1,
	.nodes = { &qns_snoc_memnoc },
};

static struct qcom_icc_bcm bcm_sn1 = {
	.name = "SN1",
	.voter_idx = 0,
	.num_nodes = 1,
	.nodes = { &qxs_imem },
};

static struct qcom_icc_bcm bcm_sn2 = {
	.name = "SN2",
	.voter_idx = 0,
	.num_nodes = 1,
	.nodes = { &xs_qdss_stm },
};

static struct qcom_icc_bcm bcm_sn3 = {
	.name = "SN3",
	.voter_idx = 0,
	.num_nodes = 1,
	.nodes = { &xs_sys_tcu_cfg },
};

static struct qcom_icc_bcm bcm_sn5 = {
	.name = "SN5",
	.voter_idx = 0,
	.num_nodes = 1,
	.nodes = { &xs_pcie },
};

static struct qcom_icc_bcm bcm_sn6 = {
	.name = "SN6",
	.voter_idx = 0,
	.num_nodes = 2,
	.nodes = { &qhm_qdss_bam, &xm_qdss_etr },
};

static struct qcom_icc_bcm bcm_sn7 = {
	.name = "SN7",
	.voter_idx = 0,
	.num_nodes = 4,
	.nodes = { &qnm_aggre_noc, &xm_pcie,
		   &xm_usb3, &qns_aggre_noc },
};

static struct qcom_icc_bcm bcm_sn8 = {
	.name = "SN8",
	.voter_idx = 0,
	.num_nodes = 1,
	.nodes = { &qnm_memnoc },
};

static struct qcom_icc_bcm bcm_sn9 = {
	.name = "SN9",
	.voter_idx = 0,
	.num_nodes = 1,
	.nodes = { &qnm_memnoc_pcie },
};

static struct qcom_icc_bcm bcm_sn10 = {
	.name = "SN10",
	.voter_idx = 0,
	.num_nodes = 2,
	.nodes = { &qnm_ipa, &xm_ipa2pcie_slv },
};

static struct qcom_icc_bcm *mc_virt_bcms[] = {
	&bcm_mc0,
};

static struct qcom_icc_node *mc_virt_nodes[] = {
	[MASTER_LLCC] = &llcc_mc,
	[SLAVE_EBI1] = &ebi,
};

static char *mc_virt_voters[] = {
	"hlos",
};

static struct qcom_icc_desc sdxlemur_mc_virt = {
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
	&bcm_sh3,
};

static struct qcom_icc_node *mem_noc_nodes[] = {
	[MASTER_TCU_0] = &acm_tcu,
	[MASTER_SNOC_GC_MEM_NOC] = &qnm_snoc_gc,
	[MASTER_APPSS_PROC] = &xm_apps_rdwr,
	[SLAVE_LLCC] = &qns_llcc,
	[SLAVE_MEM_NOC_SNOC] = &qns_memnoc_snoc,
	[SLAVE_MEM_NOC_PCIE_SNOC] = &qns_sys_pcie,
};

static char *mem_noc_voters[] = {
	"hlos",
};

static struct qcom_icc_desc sdxlemur_mem_noc = {
	.nodes = mem_noc_nodes,
	.num_nodes = ARRAY_SIZE(mem_noc_nodes),
	.bcms = mem_noc_bcms,
	.num_bcms = ARRAY_SIZE(mem_noc_bcms),
	.voters = mem_noc_voters,
	.num_voters = ARRAY_SIZE(mem_noc_voters),
};

static struct qcom_icc_bcm *system_noc_bcms[] = {
	&bcm_ce0,
	&bcm_pn0,
	&bcm_pn1,
	&bcm_pn2,
	&bcm_pn3,
	&bcm_pn4,
	&bcm_sn0,
	&bcm_sn1,
	&bcm_sn2,
	&bcm_sn3,
	&bcm_sn5,
	&bcm_sn6,
	&bcm_sn7,
	&bcm_sn8,
	&bcm_sn9,
	&bcm_sn10,
};

static struct qcom_icc_node *system_noc_nodes[] = {
	[MASTER_AUDIO] = &qhm_audio,
	[MASTER_BLSP_1] = &qhm_blsp1,
	[MASTER_QDSS_BAM] = &qhm_qdss_bam,
	[MASTER_QPIC] = &qhm_qpic,
	[MASTER_SNOC_CFG] = &qhm_snoc_cfg,
	[MASTER_SPMI_FETCHER] = &qhm_spmi_fetcher1,
	[MASTER_ANOC_SNOC] = &qnm_aggre_noc,
	[MASTER_IPA] = &qnm_ipa,
	[MASTER_MEM_NOC_SNOC] = &qnm_memnoc,
	[MASTER_MEM_NOC_PCIE_SNOC] = &qnm_memnoc_pcie,
	[MASTER_CRYPTO] = &qxm_crypto,
	[MASTER_IPA_PCIE] = &xm_ipa2pcie_slv,
	[MASTER_PCIE_0] = &xm_pcie,
	[MASTER_QDSS_ETR] = &xm_qdss_etr,
	[MASTER_SDCC_1] = &xm_sdc1,
	[MASTER_USB3] = &xm_usb3,
	[SLAVE_AOSS] = &qhs_aoss,
	[SLAVE_APPSS] = &qhs_apss,
	[SLAVE_AUDIO] = &qhs_audio,
	[SLAVE_BLSP_1] = &qhs_blsp1,
	[SLAVE_CLK_CTL] = &qhs_clk_ctl,
	[SLAVE_CRYPTO_0_CFG] = &qhs_crypto0_cfg,
	[SLAVE_CNOC_DDRSS] = &qhs_ddrss_cfg,
	[SLAVE_ECC_CFG] = &qhs_ecc_cfg,
	[SLAVE_IMEM_CFG] = &qhs_imem_cfg,
	[SLAVE_IPA_CFG] = &qhs_ipa,
	[SLAVE_CNOC_MSS] = &qhs_mss_cfg,
	[SLAVE_PCIE_PARF] = &qhs_pcie_parf,
	[SLAVE_PDM] = &qhs_pdm,
	[SLAVE_PRNG] = &qhs_prng,
	[SLAVE_QDSS_CFG] = &qhs_qdss_cfg,
	[SLAVE_QPIC] = &qhs_qpic,
	[SLAVE_SDCC_1] = &qhs_sdc1,
	[SLAVE_SNOC_CFG] = &qhs_snoc_cfg,
	[SLAVE_SPMI_FETCHER] = &qhs_spmi_fetcher,
	[SLAVE_SPMI_VGI_COEX] = &qhs_spmi_vgi_coex,
	[SLAVE_TCSR] = &qhs_tcsr,
	[SLAVE_TLMM] = &qhs_tlmm,
	[SLAVE_USB3] = &qhs_usb3,
	[SLAVE_USB3_PHY_CFG] = &qhs_usb3_phy,
	[SLAVE_ANOC_SNOC] = &qns_aggre_noc,
	[SLAVE_SNOC_MEM_NOC_GC] = &qns_snoc_memnoc,
	[SLAVE_IMEM] = &qxs_imem,
	[SLAVE_SERVICE_SNOC] = &srvc_snoc,
	[SLAVE_PCIE_0] = &xs_pcie,
	[SLAVE_QDSS_STM] = &xs_qdss_stm,
	[SLAVE_TCU] = &xs_sys_tcu_cfg,
};

static char *system_noc_voters[] = {
	"hlos",
};

static struct qcom_icc_desc sdxlemur_system_noc = {
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

	dev_dbg(&pdev->dev, "Registered sdxlemur ICC\n");

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
	{ .compatible = "qcom,sdxlemur-mc_virt",
	  .data = &sdxlemur_mc_virt},
	{ .compatible = "qcom,sdxlemur-mem_noc",
	  .data = &sdxlemur_mem_noc},
	{ .compatible = "qcom,sdxlemur-system_noc",
	  .data = &sdxlemur_system_noc},
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
		.name = "qnoc-sdxlemur",
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

MODULE_DESCRIPTION("SDXLEMUR NoC driver");
MODULE_LICENSE("GPL v2");
