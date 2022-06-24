/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __DT_BINDINGS_INTERCONNECT_QCOM_CINDER_H
#define __DT_BINDINGS_INTERCONNECT_QCOM_CINDER_H

#define MASTER_SYS_TCU				0
#define MASTER_APPSS_PROC				1
#define MASTER_LLCC				2
#define MASTER_GIC_AHB				3
#define MASTER_QDSS_BAM				4
#define MASTER_QPIC				5
#define MASTER_QSPI_0				6
#define MASTER_QUP_0				7
#define MASTER_QUP_1				8
#define MASTER_SNOC_CFG				9
#define MASTER_ANOC_SNOC				10
#define MASTER_ANOC_GSI				11
#define MASTER_GEMNOC_ECPRI_DMA				12
#define MASTER_FEC_2_GEMNOC				13
#define MASTER_GEM_NOC_CNOC				14
#define MASTER_GEMNOC_MODEM_CNOC				15
#define MASTER_GEM_NOC_PCIE_SNOC				16
#define MASTER_ANOC_PCIE_GEM_NOC				17
#define MASTER_SNOC_GC_MEM_NOC				18
#define MASTER_SNOC_SF_MEM_NOC				19
#define MASTER_QUP_CORE_0				20
#define MASTER_QUP_CORE_1				21
#define MASTER_CRYPTO				22
#define MASTER_ECPRI_GSI				23
#define MASTER_MSS_PROC				24
#define MASTER_PIMEM				25
#define MASTER_SNOC_ECPRI_DMA				26
#define MASTER_GIC				27
#define MASTER_PCIE				28
#define MASTER_QDSS_ETR				29
#define MASTER_QDSS_ETR_1				30
#define MASTER_SDCC_1				31
#define MASTER_USB3				32
#define SLAVE_EBI1				512
#define SLAVE_AHB2PHY_SOUTH				513
#define SLAVE_AHB2PHY_NORTH				514
#define SLAVE_AHB2PHY_EAST				515
#define SLAVE_AOSS				516
#define SLAVE_CLK_CTL				517
#define SLAVE_RBCPR_CX_CFG				518
#define SLAVE_RBCPR_MX_CFG				519
#define SLAVE_CRYPTO_0_CFG				520
#define SLAVE_ECPRI_CFG				521
#define SLAVE_IMEM_CFG				522
#define SLAVE_IPC_ROUTER_CFG				523
#define SLAVE_CNOC_MSS				524
#define SLAVE_PCIE_CFG				525
#define SLAVE_PDM				526
#define SLAVE_PIMEM_CFG				527
#define SLAVE_PRNG				528
#define SLAVE_QDSS_CFG				529
#define SLAVE_QPIC				530
#define SLAVE_QSPI_0				531
#define SLAVE_QUP_0				532
#define SLAVE_QUP_1				533
#define SLAVE_SDCC_2				534
#define SLAVE_SMBUS_CFG				535
#define SLAVE_SNOC_CFG				536
#define SLAVE_TCSR				537
#define SLAVE_TLMM				538
#define SLAVE_TME_CFG				539
#define SLAVE_TSC_CFG				540
#define SLAVE_USB3_0				541
#define SLAVE_VSENSE_CTRL_CFG				542
#define SLAVE_A1NOC_SNOC				543
#define SLAVE_ANOC_SNOC_GSI				544
#define SLAVE_DDRSS_CFG				545
#define SLAVE_ECPRI_GEMNOC				546
#define SLAVE_GEM_NOC_CNOC				547
#define SLAVE_SNOC_GEM_NOC_GC				548
#define SLAVE_SNOC_GEM_NOC_SF				549
#define SLAVE_LLCC				550
#define SLAVE_MODEM_OFFLINE				551
#define SLAVE_GEMNOC_MODEM_CNOC				552
#define SLAVE_MEM_NOC_PCIE_SNOC				553
#define SLAVE_ANOC_PCIE_GEM_NOC				554
#define SLAVE_QUP_CORE_0				555
#define SLAVE_QUP_CORE_1				556
#define SLAVE_IMEM				557
#define SLAVE_PIMEM				558
#define SLAVE_SERVICE_SNOC				559
#define SLAVE_ETHERNET_SS				560
#define SLAVE_PCIE_0				561
#define SLAVE_QDSS_STM				562
#define SLAVE_TCU				563

#endif
