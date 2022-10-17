/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __DT_BINDINGS_INTERCONNECT_QCOM_SDXBAAGHA_H
#define __DT_BINDINGS_INTERCONNECT_QCOM_SDXBAAGHA_H

#define MASTER_LLCC				0
#define MASTER_AUDIO				1
#define MASTER_CPUCP				2
#define MASTER_PCIE_RSCC				3
#define MASTER_QDSS_BAM				4
#define MASTER_QPIC				5
#define MASTER_QUP_0				6
#define MASTER_SHRM_2				7
#define MASTER_ANOC_SNOC				8
#define MASTER_CNOC_DC_NOC				9
#define MASTER_CNOC_SNOC				10
#define MASTER_GEM_NOC_CNOC				11
#define MASTER_GEM_NOC_PCIE_SNOC				12
#define MASTER_ANOC_PCIE_GEM_NOC				13
#define MASTER_SNOC_SF_MEM_NOC				14
#define MASTER_CRYPTO				15
#define MASTER_IPA				16
#define MASTER_MSS_NAV				17
#define MASTER_TME				18
#define MASTER_APPSS_PROC				19
#define MASTER_EMAC_0				20
#define MASTER_IPA_PCIE				21
#define MASTER_PCIE_0				22
#define MASTER_QDSS_DAP				23
#define MASTER_QDSS_ETR				24
#define MASTER_QDSS_ETR_1				25
#define MASTER_SDCC_4				26
#define MASTER_USB3_0				27
#define SLAVE_EBI1				512
#define SLAVE_AHB2PHY				513
#define SLAVE_AOSS				514
#define SLAVE_APPSS				515
#define SLAVE_AUDIO				516
#define SLAVE_BOOT_ROM				517
#define SLAVE_CLK_CTL				518
#define SLAVE_RBCPR_CX_CFG				519
#define SLAVE_RBCPR_MXA_CFG				520
#define SLAVE_RBCPR_MXC_CFG				521
#define SLAVE_CRYPTO_0_CFG				522
#define SLAVE_DDRSS_REGS				523
#define SLAVE_DPCC				524
#define SLAVE_EMAC_CFG				525
#define SLAVE_IMEM_CFG				526
#define SLAVE_IPA_CFG				527
#define SLAVE_IPC_ROUTER_CFG				528
#define SLAVE_LAGG_CFG				529
#define SLAVE_CNOC_MSS				530
#define SLAVE_PCIE_0_CFG				531
#define SLAVE_PCIE_RSC_CFG				532
#define SLAVE_PDM				533
#define SLAVE_PMU_WRAPPER_CFG				534
#define SLAVE_PRNG				535
#define SLAVE_QDSS_CFG				536
#define SLAVE_QPIC				537
#define SLAVE_QUP_0				538
#define SLAVE_SDCC_4				539
#define SLAVE_SHRM_2_CFG				540
#define SLAVE_SHRM_2_MEM				541
#define SLAVE_SPMI_VGI_COEX				542
#define SLAVE_TCSR				543
#define SLAVE_DDRSS_TG				544
#define SLAVE_TLMM				545
#define SLAVE_TME_CFG				546
#define SLAVE_USB3				547
#define SLAVE_VSENSE_CTRL_CFG				548
#define SLAVE_A1NOC_CFG				549
#define SLAVE_DDRSS_CFG				550
#define SLAVE_LLCC				551
#define SLAVE_GEM_NOC_CNOC				552
#define SLAVE_SNOC_MEM_NOC_SF				553
#define SLAVE_MEM_NOC_PCIE_SNOC				554
#define SLAVE_ANOC_PCIE_GEM_NOC				555
#define SLAVE_SNOC_CNOC				556
#define SLAVE_ANOC_THROTTLE_CFG				557
#define SLAVE_DDRSS_MC_0				558
#define SLAVE_DDRSS_MCCC_0				559
#define SLAVE_MCDMA_THROTTLE_CFG				560
#define SLAVE_MS_NAV_THROTTLE_CFG				561
#define SLAVE_PCIE_THROTTLE_CFG				562
#define SLAVE_QM_CFG				563
#define SLAVE_QM_MPU_CFG				564
#define SLAVE_QMIP_0_CFG				565
#define SLAVE_SNOC_THROTTLE_CFG				566
#define SLAVE_BOOT_IMEM				567
#define SLAVE_IMEM				568
#define SLAVE_PCIE_0				569
#define SLAVE_QDSS_STM				570
#define SLAVE_TCU				571

#endif
