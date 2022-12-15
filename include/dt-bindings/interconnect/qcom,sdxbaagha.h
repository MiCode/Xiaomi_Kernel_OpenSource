/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __DT_BINDINGS_INTERCONNECT_QCOM_SDXBAAGHA_H
#define __DT_BINDINGS_INTERCONNECT_QCOM_SDXBAAGHA_H

#define MASTER_SYS_TCU				0
#define MASTER_LLCC				1
#define MASTER_AUDIO				2
#define MASTER_CPUCP				3
#define MASTER_PCIE_RSCC				4
#define MASTER_QDSS_BAM				5
#define MASTER_QPIC				6
#define MASTER_QUP_0				7
#define MASTER_ANOC_SNOC				8
#define MASTER_CNOC_DC_NOC				9
#define MASTER_CNOC_SNOC				10
#define MASTER_MSS_PROC				11
#define MASTER_GEM_NOC_CFG				12
#define MASTER_GEM_NOC_CNOC				13
#define MASTER_GEM_NOC_PCIE_SNOC				14
#define MASTER_ANOC_PCIE_GEM_NOC				15
#define MASTER_SNOC_SF_MEM_NOC				16
#define MASTER_QUP_CORE_0				17
#define MASTER_CRYPTO				18
#define MASTER_IPA				19
#define MASTER_MSS_NAV				20
#define MASTER_TME				21
#define MASTER_APPSS_PROC				22
#define MASTER_EMAC				23
#define MASTER_IPA_PCIE				24
#define MASTER_PCIE_0				25
#define MASTER_QDSS_DAP				26
#define MASTER_QDSS_ETR				27
#define MASTER_QDSS_ETR_1				28
#define MASTER_SDCC_4				29
#define MASTER_USB3_0				30
#define SLAVE_EBI1				512
#define SLAVE_AHB2PHY				513
#define SLAVE_AOSS				514
#define SLAVE_APPSS				515
#define SLAVE_AUDIO				516
#define SLAVE_CLK_CTL				517
#define SLAVE_RBCPR_CX_CFG				518
#define SLAVE_RBCPR_MXA_CFG				519
#define SLAVE_RBCPR_MXC_CFG				520
#define SLAVE_CRYPTO_0_CFG				521
#define SLAVE_EMAC_CFG				522
#define SLAVE_IMEM_CFG				523
#define SLAVE_IPA_CFG				524
#define SLAVE_IPC_ROUTER_CFG				525
#define SLAVE_LAGG_CFG				526
#define SLAVE_MCCC_MASTER				527
#define SLAVE_CNOC_MSS				528
#define SLAVE_PCIE_0_CFG				529
#define SLAVE_PCIE_RSC_CFG				530
#define SLAVE_PDM				531
#define SLAVE_PMU_WRAPPER_CFG				532
#define SLAVE_PRNG				533
#define SLAVE_QDSS_CFG				534
#define SLAVE_QPIC				535
#define SLAVE_QUP_0				536
#define SLAVE_SDCC_4				537
#define SLAVE_SPMI_VGI_COEX				538
#define SLAVE_TCSR				539
#define SLAVE_TLMM				540
#define SLAVE_TME_CFG				541
#define SLAVE_USB3				542
#define SLAVE_VSENSE_CTRL_CFG				543
#define SLAVE_A1NOC_CFG				544
#define SLAVE_DDRSS_CFG				545
#define SLAVE_LLCC				546
#define SLAVE_GEM_NOC_CNOC				547
#define SLAVE_SNOC_MEM_NOC_SF				548
#define SLAVE_MEM_NOC_PCIE_SNOC				549
#define SLAVE_ANOC_PCIE_GEM_NOC				550
#define SLAVE_SNOC_CNOC				551
#define SLAVE_ANOC_THROTTLE_CFG				552
#define SLAVE_QUP_CORE_0				553
#define SLAVE_BOOT_IMEM				554
#define SLAVE_IMEM				555
#define SLAVE_SERVICE_GEM_NOC				556
#define SLAVE_PCIE_0				557
#define SLAVE_QDSS_STM				558
#define SLAVE_TCU				559

#endif
