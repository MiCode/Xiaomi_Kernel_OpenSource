// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 *
 */

#include <dt-bindings/interconnect/qcom,sdxnightjar.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/interconnect-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <linux/soc/qcom/smd-rpm.h>
#include <soc/qcom/rpm-smd.h>

#include "icc-rpm.h"
#include "rpm-ids.h"

static LIST_HEAD(qnoc_probe_list);
static DEFINE_MUTEX(probe_list_lock);

static int probe_count;

static const struct clk_bulk_data bus_clocks[] = {
	{ .id = "bus" },
	{ .id = "bus_a" },
};

static struct qcom_icc_node apps_proc = {
	.name = "apps_proc",
	.id = MASTER_AMPSS_M0,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 3,
	.links = { SLAVE_EBI_CH0, BIMC_SNOC_SLV,
		   BIMC_SNOC_1_SLV },
};

static struct qcom_icc_node mas_snoc_bimc = {
	.name = "mas_snoc_bimc",
	.id = SNOC_BIMC_MAS,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_SNOC_BIMC,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { SLAVE_EBI_CH0 },
};

static struct qcom_icc_node tcu_0 = {
	.name = "tcu_0",
	.id = MASTER_TCU_0,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 3,
	.links = { SLAVE_EBI_CH0, BIMC_SNOC_SLV,
		   BIMC_SNOC_1_SLV },
};

static struct qcom_icc_node mas_audio = {
	.name = "mas_audio",
	.id = MASTER_AUDIO,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = ICBID_MASTER_AUDIO,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { PNOC_M_0 },
};

static struct qcom_icc_node mas_blsp_1 = {
	.name = "mas_blsp_1",
	.id = MASTER_BLSP_1,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = ICBID_MASTER_BLSP_1,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { PNOC_M_1 },
};

static struct qcom_icc_node qpic = {
	.name = "qpic",
	.id = MASTER_QPIC,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_QPIC,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { PNOC_M_1 },
};

static struct qcom_icc_node crypto = {
	.name = "crypto",
	.id = MASTER_CRYPTO_CORE_0,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { PNOC_INT_1 },
};

static struct qcom_icc_node mas_sdcc_1 = {
	.name = "mas_sdcc_1",
	.id = MASTER_SDCC_1,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_SDCC_1,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { PNOC_INT_1 },
};

static struct qcom_icc_node mas_spmi_fetcher = {
	.name = "mas_spmi_fetcher",
	.id = MASTER_SPMI_FETCHER,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { PNOC_M_0 },
};

static struct qcom_icc_node mas_snoc_pcnoc = {
	.name = "mas_snoc_pcnoc",
	.id = SNOC_PNOC_MAS,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_SNOC_PCNOC,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { PNOC_INT_5 },
};

static struct qcom_icc_node pcnoc_m_0 = {
	.name = "pcnoc_m_0",
	.id = PNOC_M_0,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_PCNOC_M_0,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { PNOC_INT_2 },
};

static struct qcom_icc_node pcnoc_m_1 = {
	.name = "pcnoc_m_1",
	.id = PNOC_M_1,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_PCNOC_M_1,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { PNOC_INT_2 },
};

static struct qcom_icc_node pcnoc_int_0 = {
	.name = "pcnoc_int_0",
	.id = PNOC_INT_0,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_PCNOC_INT_0,
	.slv_rpm_id = -1,
	.num_links = 6,
	.links = { PNOC_SLV_0, PNOC_SLV_1,
		   PNOC_SLV_2, PNOC_SLV_3,
		   PNOC_SLV_4, PNOC_SLV_7 },
};

static struct qcom_icc_node pcnoc_int_1 = {
	.name = "pcnoc_int_1",
	.id = PNOC_INT_1,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_PCNOC_INT_1,
	.slv_rpm_id = -1,
	.num_links = 2,
	.links = { PNOC_SNOC_SLV, PNOC_INT_5 },
};

static struct qcom_icc_node pcnoc_int_2 = {
	.name = "pcnoc_int_2",
	.id = PNOC_INT_2,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_PCNOC_INT_2,
	.slv_rpm_id = -1,
	.num_links = 2,
	.links = { PNOC_INT_4, PNOC_INT_5 },
};

static struct qcom_icc_node pcnoc_int_4 = {
	.name = "pcnoc_int_4",
	.id = PNOC_INT_4,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_PCNOC_INT_4,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { PNOC_SNOC_SLV },
};

static struct qcom_icc_node pcnoc_int_5 = {
	.name = "pcnoc_int_5",
	.id = PNOC_INT_5,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_PCNOC_INT_5,
	.slv_rpm_id = -1,
	.num_links = 2,
	.links = { PNOC_INT_0, PNOC_INT_6 },
};

static struct qcom_icc_node pcnoc_int_6 = {
	.name = "pcnoc_int_6",
	.id = PNOC_INT_6,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_PCNOC_INT_6,
	.slv_rpm_id = -1,
	.num_links = 2,
	.links = { PNOC_SLV_8, SLAVE_TCU },
};

static struct qcom_icc_node pcnoc_s_0 = {
	.name = "pcnoc_s_0",
	.id = PNOC_SLV_0,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = ICBID_MASTER_PCNOC_S_0,
	.slv_rpm_id = -1,
	.num_links = 2,
	.links = { SLAVE_TCSR, SLAVE_TLMM },
};

static struct qcom_icc_node pcnoc_s_1 = {
	.name = "pcnoc_s_1",
	.id = PNOC_SLV_1,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = ICBID_MASTER_PCNOC_S_1,
	.slv_rpm_id = -1,
	.num_links = 4,
	.links = { SLAVE_CRYPTO_0_CFG, SLAVE_MESSAGE_RAM,
		   SLAVE_PDM, SLAVE_PRNG },
};

static struct qcom_icc_node pcnoc_s_2 = {
	.name = "pcnoc_s_2",
	.id = PNOC_SLV_2,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = ICBID_MASTER_PCNOC_S_2,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { SLAVE_PMIC_ARB },
};

static struct qcom_icc_node pcnoc_s_3 = {
	.name = "pcnoc_s_3",
	.id = PNOC_SLV_3,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = ICBID_MASTER_PCNOC_S_3,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { SLAVE_SNOC_CFG },
};

static struct qcom_icc_node pcnoc_s_4 = {
	.name = "pcnoc_s_4",
	.id = PNOC_SLV_4,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = ICBID_MASTER_PCNOC_S_4,
	.slv_rpm_id = -1,
	.num_links = 4,
	.links = { SLAVE_AUDIO, SLAVE_PCIE_PARF,
		   SLAVE_USB3_PHY_CFG, SLAVE_SPMI_FETCHER },
};

static struct qcom_icc_node pcnoc_s_7 = {
	.name = "pcnoc_s_7",
	.id = PNOC_SLV_7,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = ICBID_MASTER_PCNOC_S_7,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { SLAVE_IPA_CFG },
};

static struct qcom_icc_node pcnoc_s_8 = {
	.name = "pcnoc_s_8",
	.id = PNOC_SLV_8,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = ICBID_MASTER_PCNOC_S_8,
	.slv_rpm_id = -1,
	.num_links = 4,
	.links = { SLAVE_SDCC_1, SLAVE_BLSP_1,
		   SLAVE_QPIC, SLAVE_DCC_CFG },
};

static struct qcom_icc_node qdss_bam = {
	.name = "qdss_bam",
	.id = MASTER_QDSS_BAM,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 4,
	.links = { SLAVE_USB3, SLAVE_OCIMEM,
		   SNOC_BIMC_SLV, SNOC_PNOC_SLV },
};

static struct qcom_icc_node mas_bimc_snoc = {
	.name = "mas_bimc_snoc",
	.id = BIMC_SNOC_MAS,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_BIMC_SNOC,
	.slv_rpm_id = -1,
	.num_links = 6,
	.links = { SLAVE_APPSS, SLAVE_USB3,
		   SLAVE_OCIMEM, SLAVE_QDSS_STM,
		   SLAVE_CATS_128, SNOC_PNOC_SLV },
};

static struct qcom_icc_node mas_bimc_snoc_1_pcie = {
	.name = "mas_bimc_snoc_1_pcie",
	.id = BIMC_SNOC_1_MAS,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_BIMC_SNOC_1,
	.slv_rpm_id = -1,
	.num_links = 1,
	.links = { SLAVE_PCIE_0 },
};

static struct qcom_icc_node ipa = {
	.name = "ipa",
	.id = MASTER_IPA,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_IPA,
	.slv_rpm_id = -1,
	.num_links = 6,
	.links = { SLAVE_USB3, SLAVE_OCIMEM,
		   SLAVE_QDSS_STM, SNOC_BIMC_SLV,
		   SLAVE_PCIE_0, SNOC_PNOC_SLV },
};

static struct qcom_icc_node mas_usb3 = {
	.name = "mas_usb3",
	.id = MASTER_USB3,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 5,
	.links = { SLAVE_USB3, SLAVE_OCIMEM,
		   SLAVE_QDSS_STM, SNOC_BIMC_SLV,
		   SNOC_PNOC_SLV },
};

static struct qcom_icc_node mas_pcnoc_snoc = {
	.name = "mas_pcnoc_snoc",
	.id = PNOC_SNOC_MAS,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_PNOC_SNOC,
	.slv_rpm_id = -1,
	.num_links = 6,
	.links = { SLAVE_APPSS, SLAVE_USB3,
		   SLAVE_OCIMEM, SLAVE_QDSS_STM,
		   SNOC_BIMC_SLV, SLAVE_PCIE_0 },
};

static struct qcom_icc_node qdss_etr = {
	.name = "qdss_etr",
	.id = MASTER_QDSS_ETR,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 4,
	.links = { SLAVE_USB3, SLAVE_OCIMEM,
		   SNOC_BIMC_SLV, SNOC_PNOC_SLV },
};

static struct qcom_icc_node mas_pcie_0 = {
	.name = "mas_pcie_0",
	.id = MASTER_PCIE,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = ICBID_MASTER_PCIE_0,
	.slv_rpm_id = -1,
	.num_links = 5,
	.links = { SLAVE_APPSS, SLAVE_OCIMEM,
		   SLAVE_QDSS_STM, SNOC_BIMC_SLV,
		   SNOC_PNOC_SLV },
};

static struct qcom_icc_node ebi = {
	.name = "ebi",
	.id = SLAVE_EBI_CH0,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_EBI1,
	.num_links = 0,
};

static struct qcom_icc_node slv_bimc_snoc = {
	.name = "slv_bimc_snoc",
	.id = BIMC_SNOC_SLV,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_BIMC_SNOC,
	.num_links = 1,
	.links = { BIMC_SNOC_MAS },
};

static struct qcom_icc_node slv_bimc_snoc_1_pcie = {
	.name = "slv_bimc_snoc_1_pcie",
	.id = BIMC_SNOC_1_SLV,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_BIMC_SNOC_1,
	.num_links = 1,
	.links = { BIMC_SNOC_1_MAS },
};

static struct qcom_icc_node tcsr = {
	.name = "tcsr",
	.id = SLAVE_TCSR,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_TCSR,
	.num_links = 0,
};

static struct qcom_icc_node tlmm = {
	.name = "tlmm",
	.id = SLAVE_TLMM,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_TLMM,
	.num_links = 0,
};

static struct qcom_icc_node crypto_0_cfg = {
	.name = "crypto_0_cfg",
	.id = SLAVE_CRYPTO_0_CFG,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node message_ram = {
	.name = "message_ram",
	.id = SLAVE_MESSAGE_RAM,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_MESSAGE_RAM,
	.num_links = 0,
};

static struct qcom_icc_node pdm = {
	.name = "pdm",
	.id = SLAVE_PDM,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_PDM,
	.num_links = 0,
};

static struct qcom_icc_node prng = {
	.name = "prng",
	.id = SLAVE_PRNG,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node pmic_arb = {
	.name = "pmic_arb",
	.id = SLAVE_PMIC_ARB,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_PMIC_ARB,
	.num_links = 0,
};

static struct qcom_icc_node snoc_cfg = {
	.name = "snoc_cfg",
	.id = SLAVE_SNOC_CFG,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_SNOC_CFG,
	.num_links = 0,
};

static struct qcom_icc_node slv_sdcc_1 = {
	.name = "slv_sdcc_1",
	.id = SLAVE_SDCC_1,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_SDCC_1,
	.num_links = 0,
};

static struct qcom_icc_node slv_blsp_1 = {
	.name = "slv_blsp_1",
	.id = SLAVE_BLSP_1,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_BLSP_1,
	.num_links = 0,
};

static struct qcom_icc_node dcc_cfg = {
	.name = "dcc_cfg",
	.id = SLAVE_DCC_CFG,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_DCC_CFG,
	.num_links = 0,
};

static struct qcom_icc_node slv_audio = {
	.name = "slv_audio",
	.id = SLAVE_AUDIO,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_AUDIO,
	.num_links = 0,
};

static struct qcom_icc_node slv_spmi_fetcher = {
	.name = "slv_spmi_fetcher",
	.id = SLAVE_SPMI_FETCHER,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node tcu = {
	.name = "tcu",
	.id = SLAVE_TCU,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_pcnoc_snoc = {
	.name = "slv_pcnoc_snoc",
	.id = PNOC_SNOC_SLV,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_PCNOC_SNOC,
	.num_links = 1,
	.links = { PNOC_SNOC_MAS },
};

static struct qcom_icc_node pcie_parf = {
	.name = "pcie_parf",
	.id = SLAVE_PCIE_PARF,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_PCIE_PARF,
	.num_links = 0,
};

static struct qcom_icc_node usb3_phy_cfg = {
	.name = "usb3_phy_cfg",
	.id = SLAVE_USB3_PHY_CFG,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_USB3_PHY_CFG,
	.num_links = 0,
};

static struct qcom_icc_node qpic_cfg = {
	.name = "qpic_cfg",
	.id = SLAVE_QPIC,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_QPIC,
	.num_links = 0,
};

static struct qcom_icc_node ipa_cfg = {
	.name = "ipa_cfg",
	.id = SLAVE_IPA_CFG,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_IPA_CFG,
	.num_links = 0,
};

static struct qcom_icc_node apss_ahb = {
	.name = "apss_ahb",
	.id = SLAVE_APPSS,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node slv_snoc_bimc = {
	.name = "slv_snoc_bimc",
	.id = SNOC_BIMC_SLV,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_SNOC_BIMC,
	.num_links = 1,
	.links = { SNOC_BIMC_MAS },
};

static struct qcom_icc_node imem = {
	.name = "imem",
	.id = SLAVE_OCIMEM,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_IMEM,
	.num_links = 0,
};

static struct qcom_icc_node slv_snoc_pcnoc = {
	.name = "slv_snoc_pcnoc",
	.id = SNOC_PNOC_SLV,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_SNOC_PCNOC,
	.num_links = 1,
	.links = { SNOC_PNOC_MAS },
};

static struct qcom_icc_node qdss_stm = {
	.name = "qdss_stm",
	.id = SLAVE_QDSS_STM,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_QDSS_STM,
	.num_links = 0,
};

static struct qcom_icc_node slv_pcie_0 = {
	.name = "slv_pcie_0",
	.id = SLAVE_PCIE_0,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_PCIE_0,
	.num_links = 0,
};

static struct qcom_icc_node slv_usb3 = {
	.name = "slv_usb3",
	.id = SLAVE_USB3,
	.channels = 1,
	.buswidth = 4,
	.mas_rpm_id = -1,
	.slv_rpm_id = ICBID_SLAVE_USB3,
	.num_links = 0,
};

static struct qcom_icc_node cats_0 = {
	.name = "cats_0",
	.id = SLAVE_CATS_128,
	.channels = 1,
	.buswidth = 8,
	.mas_rpm_id = -1,
	.slv_rpm_id = -1,
	.num_links = 0,
};

static struct qcom_icc_node *bimc_nodes[] = {
	[MASTER_AMPSS_M0] = &apps_proc,
	[SNOC_BIMC_MAS] = &mas_snoc_bimc,
	[MASTER_TCU_0] = &tcu_0,
	[SLAVE_EBI_CH0] = &ebi,
	[BIMC_SNOC_SLV] = &slv_bimc_snoc,
	[BIMC_SNOC_1_SLV] = &slv_bimc_snoc_1_pcie,
};

static struct qcom_icc_desc sdxnightjar_bimc = {
	.nodes = bimc_nodes,
	.num_nodes = ARRAY_SIZE(bimc_nodes),
};

static struct qcom_icc_node *pcnoc_nodes[] = {
	[MASTER_AUDIO] = &mas_audio,
	[MASTER_BLSP_1] = &mas_blsp_1,
	[MASTER_QPIC] = &qpic,
	[MASTER_CRYPTO_CORE_0] = &crypto,
	[MASTER_SDCC_1] = &mas_sdcc_1,
	[MASTER_SPMI_FETCHER] = &mas_spmi_fetcher,
	[SNOC_PNOC_MAS] = &mas_snoc_pcnoc,
	[PNOC_M_0] = &pcnoc_m_0,
	[PNOC_M_1] = &pcnoc_m_1,
	[PNOC_INT_0] = &pcnoc_int_0,
	[PNOC_INT_1] = &pcnoc_int_1,
	[PNOC_INT_2] = &pcnoc_int_2,
	[PNOC_INT_4] = &pcnoc_int_4,
	[PNOC_INT_5] = &pcnoc_int_5,
	[PNOC_INT_6] = &pcnoc_int_6,
	[PNOC_SLV_0] = &pcnoc_s_0,
	[PNOC_SLV_1] = &pcnoc_s_1,
	[PNOC_SLV_2] = &pcnoc_s_2,
	[PNOC_SLV_3] = &pcnoc_s_3,
	[PNOC_SLV_4] = &pcnoc_s_4,
	[PNOC_SLV_7] = &pcnoc_s_7,
	[PNOC_SLV_8] = &pcnoc_s_8,
	[SLAVE_TCSR] = &tcsr,
	[SLAVE_TLMM] = &tlmm,
	[SLAVE_CRYPTO_0_CFG] = &crypto_0_cfg,
	[SLAVE_MESSAGE_RAM] = &message_ram,
	[SLAVE_PDM] = &pdm,
	[SLAVE_PRNG] = &prng,
	[SLAVE_PMIC_ARB] = &pmic_arb,
	[SLAVE_SNOC_CFG] = &snoc_cfg,
	[SLAVE_SDCC_1] = &slv_sdcc_1,
	[SLAVE_BLSP_1] = &slv_blsp_1,
	[SLAVE_DCC_CFG] = &dcc_cfg,
	[SLAVE_AUDIO] = &slv_audio,
	[SLAVE_SPMI_FETCHER] = &slv_spmi_fetcher,
	[SLAVE_TCU] = &tcu,
	[PNOC_SNOC_SLV] = &slv_pcnoc_snoc,
	[SLAVE_PCIE_PARF] = &pcie_parf,
	[SLAVE_USB3_PHY_CFG] = &usb3_phy_cfg,
	[SLAVE_QPIC] = &qpic_cfg,
	[SLAVE_IPA_CFG] = &ipa_cfg,
};

static struct qcom_icc_desc sdxnightjar_pcnoc = {
	.nodes = pcnoc_nodes,
	.num_nodes = ARRAY_SIZE(pcnoc_nodes),
};

static struct qcom_icc_node *snoc_nodes[] = {
	[MASTER_QDSS_BAM] = &qdss_bam,
	[BIMC_SNOC_MAS] = &mas_bimc_snoc,
	[BIMC_SNOC_1_MAS] = &mas_bimc_snoc_1_pcie,
	[MASTER_IPA] = &ipa,
	[MASTER_USB3] = &mas_usb3,
	[PNOC_SNOC_MAS] = &mas_pcnoc_snoc,
	[MASTER_QDSS_ETR] = &qdss_etr,
	[MASTER_PCIE] = &mas_pcie_0,
	[SLAVE_APPSS] = &apss_ahb,
	[SNOC_BIMC_SLV] = &slv_snoc_bimc,
	[SLAVE_OCIMEM] = &imem,
	[SNOC_PNOC_SLV] = &slv_snoc_pcnoc,
	[SLAVE_QDSS_STM] = &qdss_stm,
	[SLAVE_PCIE_0] = &slv_pcie_0,
	[SLAVE_USB3] = &slv_usb3,
	[SLAVE_CATS_128] = &cats_0,
};

static struct qcom_icc_desc sdxnightjar_snoc = {
	.nodes = snoc_nodes,
	.num_nodes = ARRAY_SIZE(snoc_nodes),
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
	struct device *dev = &pdev->dev;
	const struct qcom_icc_desc *desc;
	struct icc_onecell_data *data;
	struct icc_provider *provider;
	struct qcom_icc_node **qnodes;
	struct qcom_icc_provider *qp;
	struct icc_node *node, *tmp;
	size_t num_nodes, i;
	int ret;

	desc = of_device_get_match_data(dev);
	if (!desc)
		return -EINVAL;

	qnodes = desc->nodes;
	num_nodes = desc->num_nodes;

	qp = devm_kzalloc(dev, sizeof(*qp), GFP_KERNEL);
	if (!qp)
		return -ENOMEM;

	data = devm_kzalloc(dev, struct_size(data, nodes, num_nodes),
			    GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	qp->bus_clks = devm_kmemdup(dev, bus_clocks, sizeof(bus_clocks),
				    GFP_KERNEL);
	if (!qp->bus_clks)
		return -ENOMEM;

	qp->num_clks = ARRAY_SIZE(bus_clocks);
	ret = devm_clk_bulk_get(dev, qp->num_clks, qp->bus_clks);
	if (ret)
		return ret;

	ret = clk_bulk_prepare_enable(qp->num_clks, qp->bus_clks);
	if (ret)
		return ret;

	provider = &qp->provider;
	provider->dev = dev;
	provider->set = qcom_icc_rpm_set;
	provider->pre_aggregate = qcom_icc_rpm_pre_aggregate;
	provider->aggregate = qcom_icc_rpm_aggregate;
	provider->xlate = of_icc_xlate_onecell;
	INIT_LIST_HEAD(&provider->nodes);
	provider->data = data;
	qp->dev = &pdev->dev;

	qp->init = true;
	qp->keepalive = of_property_read_bool(dev->of_node, "qcom,keepalive");

	if (of_property_read_u32(dev->of_node, "qcom,util-factor",
				&qp->util_factor))
		qp->util_factor = DEFAULT_UTIL_FACTOR;

	qp->regmap = qcom_icc_map(pdev, desc);
	if (IS_ERR(qp->regmap))
		return PTR_ERR(qp->regmap);

	ret = icc_provider_add(provider);
	if (ret) {
		dev_err(dev, "error adding interconnect provider: %d\n", ret);
		clk_bulk_disable_unprepare(qp->num_clks, qp->bus_clks);
		return ret;
	}

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

		for (j = 0; j < qnodes[i]->num_links; j++)
			icc_link_create(node, qnodes[i]->links[j]);

		data->nodes[i] = node;
	}
	data->num_nodes = num_nodes;

	platform_set_drvdata(pdev, qp);

	mutex_lock(&probe_list_lock);
	list_add_tail(&qp->probe_list, &qnoc_probe_list);
	mutex_unlock(&probe_list_lock);

	dev_info(dev, "Registered SDXNIGHTJAR ICC\n");

	return 0;
err:
	list_for_each_entry_safe(node, tmp, &provider->nodes, node_list) {
		icc_node_del(node);
		icc_node_destroy(node->id);
	}
	clk_bulk_disable_unprepare(qp->num_clks, qp->bus_clks);
	icc_provider_del(provider);

	return ret;
}

static int qnoc_remove(struct platform_device *pdev)
{
	struct qcom_icc_provider *qp = platform_get_drvdata(pdev);
	struct icc_provider *provider = &qp->provider;
	struct icc_node *n, *tmp;

	list_for_each_entry_safe(n, tmp, &provider->nodes, node_list) {
		icc_node_del(n);
		icc_node_destroy(n->id);
	}
	clk_bulk_disable_unprepare(qp->num_clks, qp->bus_clks);

	return icc_provider_del(provider);
}

static const struct of_device_id qnoc_of_match[] = {
	{ .compatible = "qcom,sdxnightjar-bimc",
	  .data = &sdxnightjar_bimc},
	{ .compatible = "qcom,sdxnightjar-pcnoc",
	  .data = &sdxnightjar_pcnoc},
	{ .compatible = "qcom,sdxnightjar-snoc",
	  .data = &sdxnightjar_snoc},
	{ }
};
MODULE_DEVICE_TABLE(of, qnoc_of_match);

static void qnoc_sync_state(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct qcom_icc_provider *qp = platform_get_drvdata(pdev);
	int ret = 0, i;

	mutex_lock(&probe_list_lock);
	probe_count++;

	if (probe_count < ARRAY_SIZE(qnoc_of_match) - 1) {
		mutex_unlock(&probe_list_lock);
		return;
	}

	list_for_each_entry(qp, &qnoc_probe_list, probe_list) {
		qp->init = false;

		if (!qp->keepalive)
			continue;

		for (i = 0; i < RPM_NUM_CXT; i++) {
			if (i == RPM_ACTIVE_CXT) {
				if (qp->bus_clk_cur_rate[i] == 0)
					ret = clk_set_rate(qp->bus_clks[i].clk,
						RPM_CLK_MIN_LEVEL);
				else
					ret = clk_set_rate(qp->bus_clks[i].clk,
						qp->bus_clk_cur_rate[i]);

				if (ret)
					pr_err("%s clk_set_rate error: %d\n",
						qp->bus_clks[i].id, ret);
			}
		}
	}

	mutex_unlock(&probe_list_lock);

	pr_err("SDXNIGHTJAR ICC Sync State done\n");
}

static struct platform_driver qnoc_driver = {
	.probe = qnoc_probe,
	.remove = qnoc_remove,
	.driver = {
		.name = "qnoc-sdxnightjar",
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

MODULE_DESCRIPTION("sdxnightjar NoC driver");
MODULE_LICENSE("GPL v2");
