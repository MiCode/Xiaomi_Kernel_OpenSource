/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __DT_BINDINGS_INTERCONNECT_QCOM_CINDER_H
#define __DT_BINDINGS_INTERCONNECT_QCOM_CINDER_H

#define MASTER_SYS_TCU				0
#define MASTER_APPSS_PROC				1
#define MASTER_DDR_PERF_MODE				2
#define MASTER_LLCC				3
#define MASTER_GIC_AHB				4
#define MASTER_QDSS_BAM				5
#define MASTER_QPIC				6
#define MASTER_QSPI_0				7
#define MASTER_QUP_0				8
#define MASTER_QUP_1				9
#define MASTER_SNOC_CFG				10
#define MASTER_ANOC_SNOC				11
#define MASTER_ANOC_GSI				12
#define MASTER_GEMNOC_ECPRI_DMA				13
#define MASTER_FEC_2_GEMNOC				14
#define MASTER_GEM_NOC_CNOC				15
#define MASTER_GEMNOC_MODEM_CNOC				16
#define MASTER_GEM_NOC_PCIE_SNOC				17
#define MASTER_ANOC_PCIE_GEM_NOC				18
#define MASTER_SNOC_GC_MEM_NOC				19
#define MASTER_SNOC_SF_MEM_NOC				20
#define MASTER_QUP_CORE_0				21
#define MASTER_QUP_CORE_1				22
#define MASTER_CRYPTO				23
#define MASTER_ECPRI_GSI				24
#define MASTER_MSS_PROC				25
#define MASTER_PIMEM				26
#define MASTER_SNOC_ECPRI_DMA				27
#define MASTER_GIC				28
#define MASTER_PCIE				29
#define MASTER_QDSS_ETR				30
#define MASTER_QDSS_ETR_1				31
#define MASTER_SDCC_1				32
#define MASTER_USB3				33
#define SLAVE_DDR_PERF_MODE				512
#define SLAVE_EBI1				513
#define SLAVE_AHB2PHY_SOUTH				514
#define SLAVE_AHB2PHY_NORTH				515
#define SLAVE_AHB2PHY_EAST				516
#define SLAVE_AOSS				517
#define SLAVE_CLK_CTL				518
#define SLAVE_RBCPR_CX_CFG				519
#define SLAVE_RBCPR_MX_CFG				520
#define SLAVE_CRYPTO_0_CFG				521
#define SLAVE_ECPRI_CFG				522
#define SLAVE_IMEM_CFG				523
#define SLAVE_IPC_ROUTER_CFG				524
#define SLAVE_CNOC_MSS				525
#define SLAVE_PCIE_CFG				526
#define SLAVE_PDM				527
#define SLAVE_PIMEM_CFG				528
#define SLAVE_PRNG				529
#define SLAVE_QDSS_CFG				530
#define SLAVE_QPIC				531
#define SLAVE_QSPI_0				532
#define SLAVE_QUP_0				533
#define SLAVE_QUP_1				534
#define SLAVE_SDCC_2				535
#define SLAVE_SMBUS_CFG				536
#define SLAVE_SNOC_CFG				537
#define SLAVE_TCSR				538
#define SLAVE_TLMM				539
#define SLAVE_TME_CFG				540
#define SLAVE_TSC_CFG				541
#define SLAVE_USB3_0				542
#define SLAVE_VSENSE_CTRL_CFG				543
#define SLAVE_A1NOC_SNOC				544
#define SLAVE_ANOC_SNOC_GSI				545
#define SLAVE_DDRSS_CFG				546
#define SLAVE_ECPRI_GEMNOC				547
#define SLAVE_GEM_NOC_CNOC				548
#define SLAVE_SNOC_GEM_NOC_GC				549
#define SLAVE_SNOC_GEM_NOC_SF				550
#define SLAVE_LLCC				551
#define SLAVE_MODEM_OFFLINE				552
#define SLAVE_GEMNOC_MODEM_CNOC				553
#define SLAVE_MEM_NOC_PCIE_SNOC				554
#define SLAVE_ANOC_PCIE_GEM_NOC				555
#define SLAVE_QUP_CORE_0				556
#define SLAVE_QUP_CORE_1				557
#define SLAVE_IMEM				558
#define SLAVE_PIMEM				559
#define SLAVE_SERVICE_SNOC				560
#define SLAVE_ETHERNET_SS				561
#define SLAVE_PCIE_0				562
#define SLAVE_QDSS_STM				563
#define SLAVE_TCU				564

#endif
