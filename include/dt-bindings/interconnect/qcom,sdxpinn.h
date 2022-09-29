/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __DT_BINDINGS_INTERCONNECT_QCOM_SDXPINN_H
#define __DT_BINDINGS_INTERCONNECT_QCOM_SDXPINN_H

#define MASTER_SYS_TCU				0
#define MASTER_APPSS_PROC				1
#define MASTER_LLCC				2
#define MASTER_AUDIO				3
#define MASTER_GIC_AHB				4
#define MASTER_PCIE_RSCC				5
#define MASTER_QDSS_BAM				6
#define MASTER_QPIC				7
#define MASTER_QUP_0				8
#define MASTER_ANOC_SNOC				9
#define MASTER_CNOC_DC_NOC				10
#define MASTER_GEM_NOC_CFG				11
#define MASTER_GEM_NOC_CNOC				12
#define MASTER_GEM_NOC_PCIE_SNOC				13
#define MASTER_MSS_PROC				14
#define MASTER_ANOC_PCIE_GEM_NOC				15
#define MASTER_SNOC_SF_MEM_NOC				16
#define MASTER_SNOC_CFG				17
#define MASTER_PCIE_ANOC_CFG				18
#define MASTER_QPIC_CORE				19
#define MASTER_QUP_CORE_0				20
#define MASTER_CRYPTO				21
#define MASTER_IPA				22
#define MASTER_MVMSS				23
#define MASTER_EMAC_0				24
#define MASTER_EMAC_1				25
#define MASTER_GIC				26
#define MASTER_IPA_PCIE				27
#define MASTER_PCIE_0				28
#define MASTER_PCIE_1				29
#define MASTER_PCIE_2				30
#define MASTER_QDSS_ETR				31
#define MASTER_QDSS_ETR_1				32
#define MASTER_SDCC_1				33
#define MASTER_SDCC_4				34
#define MASTER_USB3_0				35
#define SLAVE_EBI1				512
#define SLAVE_AUDIO				513
#define SLAVE_CLK_CTL				514
#define SLAVE_CRYPTO_0_CFG				515
#define SLAVE_IMEM_CFG				516
#define SLAVE_IPA_CFG				517
#define SLAVE_IPC_ROUTER_CFG				518
#define SLAVE_LAGG_CFG				519
#define SLAVE_MCCC_MASTER				520
#define SLAVE_CNOC_MSS				521
#define ICBDI_SLAVE_MVMSS_CFG				522
#define SLAVE_PCIE_0_CFG				523
#define SLAVE_PCIE_1_CFG				524
#define SLAVE_PCIE_2_CFG				525
#define SLAVE_PCIE_RSC_CFG				526
#define SLAVE_PDM				527
#define SLAVE_PRNG				528
#define SLAVE_QDSS_CFG				529
#define SLAVE_QPIC				530
#define SLAVE_QUP_0				531
#define SLAVE_SDCC_1				532
#define SLAVE_SDCC_4				533
#define SLAVE_SPMI_VGI_COEX				534
#define SLAVE_TCSR				535
#define SLAVE_TLMM				536
#define SLAVE_USB3				537
#define SLAVE_USB3_PHY_CFG				538
#define SLAVE_A1NOC_CFG				539
#define SLAVE_DDRSS_CFG				540
#define SLAVE_GEM_NOC_CFG				541
#define SLAVE_GEM_NOC_CNOC				542
#define SLAVE_SNOC_GEM_NOC_SF				543
#define SLAVE_LLCC				544
#define SLAVE_MEM_NOC_PCIE_SNOC				545
#define SLAVE_ANOC_PCIE_GEM_NOC				546
#define SLAVE_SNOC_CFG				547
#define SLAVE_PCIE_ANOC_CFG				548
#define SLAVE_QPIC_CORE				549
#define SLAVE_SNOOP_BWMON				550
#define SLAVE_QUP_CORE_0				551
#define SLAVE_IMEM				552
#define SLAVE_SERVICE_GEM_NOC				553
#define SLAVE_SERVICE_PCIE_ANOC				554
#define SLAVE_SERVICE_SNOC				555
#define SLAVE_PCIE_0				556
#define SLAVE_PCIE_1				557
#define SLAVE_PCIE_2				558
#define SLAVE_QDSS_STM				559
#define SLAVE_TCU				560

#endif
