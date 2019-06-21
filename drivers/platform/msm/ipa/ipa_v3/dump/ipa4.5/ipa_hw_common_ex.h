/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */
#if !defined(_IPA_HW_COMMON_EX_H_)
#define _IPA_HW_COMMON_EX_H_

/* VLVL defs are available for 854 */
#define FEATURE_VLVL_DEFS                            true

#define FEATURE_IPA_HW_VERSION_4_5                   true

/* Important Platform Specific Values : IRQ_NUM, IRQ_CNT, BCR */
#define IPA_HW_BAM_IRQ_NUM                           639

/* Q6 IRQ number for IPA. */
#define IPA_HW_IRQ_NUM                               640

/* Total number of different interrupts that can be enabled */
#define IPA_HW_IRQ_CNT_TOTAL                         23

/* IPAv4 spare reg value */
#define IPA_HW_SPARE_1_REG_VAL                       0xC0000005

/* Whether to allow setting step mode on IPA when we crash or not */
#define IPA_CFG_HW_IS_STEP_MODE_ALLOWED              (false)

/* GSI MHI related definitions */
#define IPA_HW_GSI_MHI_CONSUMER_CHANNEL_NUM          0x0
#define IPA_HW_GSI_MHI_PRODUCER_CHANNEL_NUM          0x1

#define IPA_HW_GSI_MHI_CONSUMER_EP_NUM               0x1
#define IPA_HW_GSI_MHI_PRODUCER_EP_NUM               0x11

/* IPA ZIP WA related Macros */
#define IPA_HW_DCMP_SRC_PIPE                         0x8
#define IPA_HW_DCMP_DEST_PIPE                        0x4
#define IPA_HW_ACK_MNGR_MASK                         0x1D
#define IPA_HW_DCMP_SRC_GRP                          0x5

/* IPA Clock resource name */
#define IPA_CLK_RESOURCE_NAME                        "/clk/pcnoc"

/* IPA Clock Bus Client name */
#define IPA_CLK_BUS_CLIENT_NAME                      "IPA_PCNOC_BUS_CLIENT"

/* HPS Sequences */
#define IPA_HW_PKT_PROCESS_HPS_DMA                      0x0
#define IPA_HW_PKT_PROCESS_HPS_DMA_DECIPH_CIPHE         0x1
#define IPA_HW_PKT_PROCESS_HPS_PKT_PRS_NO_DECIPH_UCP    0x2
#define IPA_HW_PKT_PROCESS_HPS_PKT_PRS_DECIPH_UCP       0x3
#define IPA_HW_PKT_PROCESS_HPS_2_PKT_PRS_NO_DECIPH      0x4
#define IPA_HW_PKT_PROCESS_HPS_2_PKT_PRS_DECIPH         0x5
#define IPA_HW_PKT_PROCESS_HPS_PKT_PRS_NO_DECIPH_NO_UCP 0x6
#define IPA_HW_PKT_PROCESS_HPS_PKT_PRS_DECIPH_NO_UCP    0x7
#define IPA_HW_PKT_PROCESS_HPS_DMA_PARSER               0x8
#define IPA_HW_PKT_PROCESS_HPS_DMA_DECIPH_PARSER        0x9
#define IPA_HW_PKT_PROCESS_HPS_2_PKT_PRS_UCP_TWICE_NO_DECIPH  0xA
#define IPA_HW_PKT_PROCESS_HPS_2_PKT_PRS_UCP_TWICE_DECIPH     0xB
#define IPA_HW_PKT_PROCESS_HPS_3_PKT_PRS_UCP_TWICE_NO_DECIPH  0xC
#define IPA_HW_PKT_PROCESS_HPS_3_PKT_PRS_UCP_TWICE_DECIPH     0xD

/* DPS Sequences */
#define IPA_HW_PKT_PROCESS_DPS_DMA                      0x0
#define IPA_HW_PKT_PROCESS_DPS_DMA_WITH_DECIPH          0x1
#define IPA_HW_PKT_PROCESS_DPS_DMA_WITH_DECOMP          0x2
#define IPA_HW_PKT_PROCESS_DPS_DMA_WITH_CIPH            0x3

/* Src RSRC GRP config */
#define IPA_HW_SRC_RSRC_GRP_01_RSRC_TYPE_0           0x0B040803
#define IPA_HW_SRC_RSRC_GRP_01_RSRC_TYPE_1           0x0C0C0909
#define IPA_HW_SRC_RSRC_GRP_01_RSRC_TYPE_2           0x0E0E0909
#define IPA_HW_SRC_RSRC_GRP_01_RSRC_TYPE_3           0x3F003F00
#define IPA_HW_SRC_RSRC_GRP_01_RSRC_TYPE_4           0x10101616

#define IPA_HW_SRC_RSRC_GRP_23_RSRC_TYPE_0           0x01010101
#define IPA_HW_SRC_RSRC_GRP_23_RSRC_TYPE_1           0x02020202
#define IPA_HW_SRC_RSRC_GRP_23_RSRC_TYPE_2           0x04040404
#define IPA_HW_SRC_RSRC_GRP_23_RSRC_TYPE_3           0x3F003F00
#define IPA_HW_SRC_RSRC_GRP_23_RSRC_TYPE_4           0x02020606

#define IPA_HW_SRC_RSRC_GRP_45_RSRC_TYPE_0           0x00000000
#define IPA_HW_SRC_RSRC_GRP_45_RSRC_TYPE_1           0x00000000
#define IPA_HW_SRC_RSRC_GRP_45_RSRC_TYPE_2           0x00000000
#define IPA_HW_SRC_RSRC_GRP_45_RSRC_TYPE_3           0x00003F00
#define IPA_HW_SRC_RSRC_GRP_45_RSRC_TYPE_4           0x00000000

/* Dest RSRC GRP config */
#define IPA_HW_DST_RSRC_GRP_01_RSRC_TYPE_0           0x05051010
#define IPA_HW_DST_RSRC_GRP_01_RSRC_TYPE_1           0x3F013F02

#define IPA_HW_DST_RSRC_GRP_23_RSRC_TYPE_0           0x02020202
#define IPA_HW_DST_RSRC_GRP_23_RSRC_TYPE_1           0x02010201

#define IPA_HW_DST_RSRC_GRP_45_RSRC_TYPE_0           0x00000000
#define IPA_HW_DST_RSRC_GRP_45_RSRC_TYPE_1           0x00000200

#define IPA_HW_RX_HPS_CLIENTS_MIN_DEPTH_0            0x03030303
#define IPA_HW_RX_HPS_CLIENTS_MAX_DEPTH_0            0x03030303

#define IPA_HW_RSRP_GRP_0                            0x0
#define IPA_HW_RSRP_GRP_1                            0x1
#define IPA_HW_RSRP_GRP_2                            0x2
#define IPA_HW_RSRP_GRP_3                            0x3

#define IPA_HW_PCIE_SRC_RSRP_GRP                     IPA_HW_RSRP_GRP_0
#define IPA_HW_PCIE_DEST_RSRP_GRP                    IPA_HW_RSRP_GRP_0

#define IPA_HW_DDR_SRC_RSRP_GRP                      IPA_HW_RSRP_GRP_1
#define IPA_HW_DDR_DEST_RSRP_GRP                     IPA_HW_RSRP_GRP_1

#define IPA_HW_DMA_SRC_RSRP_GRP                      IPA_HW_RSRP_GRP_2
#define IPA_HW_DMA_DEST_RSRP_GRP                     IPA_HW_RSRP_GRP_2

#define IPA_HW_SRC_RSRP_TYPE_MAX 0x05
#define IPA_HW_DST_RSRP_TYPE_MAX 0x02

#define GSI_HW_QSB_LOG_MISC_MAX 0x4

/* IPA Clock Bus Client name */
#define IPA_CLK_BUS_CLIENT_NAME                      "IPA_PCNOC_BUS_CLIENT"

/* Is IPA decompression feature enabled */
#define IPA_HW_IS_DECOMPRESSION_ENABLED              (1)

/* Whether to allow setting step mode on IPA when we crash or not */
#define IPA_HW_IS_STEP_MODE_ALLOWED                  (true)

/* Max number of virtual pipes for UL QBAP provided by HW */
#define IPA_HW_MAX_VP_NUM                             (32)

/*
 * HW specific clock vote freq values in KHz
 * (BIMC/SNOC/PCNOC/IPA/Q6 CPU)
 */
enum ipa_hw_clk_freq_e {
	/* BIMC */
	IPA_HW_CLK_FREQ_BIMC_PEAK       = 518400,
	IPA_HW_CLK_FREQ_BIMC_NOM_PLUS   = 404200,
	IPA_HW_CLK_FREQ_BIMC_NOM        = 404200,
	IPA_HW_CLK_FREQ_BIMC_SVS        = 100000,

	/* PCNOC */
	IPA_HW_CLK_FREQ_PCNOC_PEAK      = 133330,
	IPA_HW_CLK_FREQ_PCNOC_NOM_PLUS  = 100000,
	IPA_HW_CLK_FREQ_PCNOC_NOM       = 100000,
	IPA_HW_CLK_FREQ_PCNOC_SVS       = 50000,

	/*IPA_HW_CLK_SNOC*/
	IPA_HW_CLK_FREQ_SNOC_PEAK       = 200000,
	IPA_HW_CLK_FREQ_SNOC_NOM_PLUS   = 150000,
	IPA_HW_CLK_FREQ_SNOC_NOM        = 150000,
	IPA_HW_CLK_FREQ_SNOC_SVS        = 85000,
	IPA_HW_CLK_FREQ_SNOC_SVS_2      = 50000,

	/* IPA */
	IPA_HW_CLK_FREQ_IPA_PEAK        = 600000,
	IPA_HW_CLK_FREQ_IPA_NOM_PLUS    = 500000,
	IPA_HW_CLK_FREQ_IPA_NOM         = 500000,
	IPA_HW_CLK_FREQ_IPA_SVS         = 250000,
	IPA_HW_CLK_FREQ_IPA_SVS_2       = 150000,

	/* Q6 CPU */
	IPA_HW_CLK_FREQ_Q6_PEAK         = 729600,
	IPA_HW_CLK_FREQ_Q6_NOM_PLUS     = 729600,
	IPA_HW_CLK_FREQ_Q6_NOM          = 729600,
	IPA_HW_CLK_FREQ_Q6_SVS          = 729600,
};

enum ipa_hw_qtimer_gran_e {
	IPA_HW_QTIMER_GRAN_0 = 0, /* granularity 0 is 10us */
	IPA_HW_QTIMER_GRAN_1 = 1, /* granularity 1 is 100us */
	IPA_HW_QTIMER_GRAN_MAX,
};

/* Pipe ID of all the IPA pipes */
enum ipa_hw_pipe_id_e {
	IPA_HW_PIPE_ID_0,
	IPA_HW_PIPE_ID_1,
	IPA_HW_PIPE_ID_2,
	IPA_HW_PIPE_ID_3,
	IPA_HW_PIPE_ID_4,
	IPA_HW_PIPE_ID_5,
	IPA_HW_PIPE_ID_6,
	IPA_HW_PIPE_ID_7,
	IPA_HW_PIPE_ID_8,
	IPA_HW_PIPE_ID_9,
	IPA_HW_PIPE_ID_10,
	IPA_HW_PIPE_ID_11,
	IPA_HW_PIPE_ID_12,
	IPA_HW_PIPE_ID_13,
	IPA_HW_PIPE_ID_14,
	IPA_HW_PIPE_ID_15,
	IPA_HW_PIPE_ID_16,
	IPA_HW_PIPE_ID_17,
	IPA_HW_PIPE_ID_18,
	IPA_HW_PIPE_ID_19,
	IPA_HW_PIPE_ID_20,
	IPA_HW_PIPE_ID_21,
	IPA_HW_PIPE_ID_22,
	IPA_HW_PIPE_ID_23,
	IPA_HW_PIPE_ID_24,
	IPA_HW_PIPE_ID_25,
	IPA_HW_PIPE_ID_26,
	IPA_HW_PIPE_ID_27,
	IPA_HW_PIPE_ID_28,
	IPA_HW_PIPE_ID_29,
	IPA_HW_PIPE_ID_30,
	IPA_HW_PIPE_ID_MAX
};

/* Pipe ID's of System Bam Endpoints between Q6 & IPA */
enum ipa_hw_q6_pipe_id_e {
	/* Pipes used by IPA Q6 driver */
	IPA_HW_Q6_DL_CONSUMER_PIPE_ID           = IPA_HW_PIPE_ID_5,
	IPA_HW_Q6_CTL_CONSUMER_PIPE_ID          = IPA_HW_PIPE_ID_6,
	IPA_HW_Q6_DL_NLO_CONSUMER_PIPE_ID       = IPA_HW_PIPE_ID_8,

	IPA_HW_Q6_UL_ACC_ACK_PRODUCER_PIPE_ID   = IPA_HW_PIPE_ID_20,
	IPA_HW_Q6_UL_PRODUCER_PIPE_ID           = IPA_HW_PIPE_ID_21,
	IPA_HW_Q6_DL_PRODUCER_PIPE_ID           = IPA_HW_PIPE_ID_17,
	IPA_HW_Q6_QBAP_STATUS_PRODUCER_PIPE_ID  = IPA_HW_PIPE_ID_18,
	IPA_HW_Q6_UL_ACC_DATA_PRODUCER_PIPE_ID  = IPA_HW_PIPE_ID_19,

	IPA_HW_Q6_UL_ACK_PRODUCER_PIPE_ID  =
	  IPA_HW_Q6_UL_ACC_ACK_PRODUCER_PIPE_ID,
	IPA_HW_Q6_UL_DATA_PRODUCER_PIPE_ID =
	  IPA_HW_Q6_UL_ACC_DATA_PRODUCER_PIPE_ID,

	IPA_HW_Q6_DMA_ASYNC_CONSUMER_PIPE_ID    = IPA_HW_PIPE_ID_4,
	IPA_HW_Q6_DMA_ASYNC_PRODUCER_PIPE_ID    = IPA_HW_PIPE_ID_29,

	/* Test Simulator Pipes */
	IPA_HW_Q6_SIM_UL_CONSUMER_PIPE_0_ID     = IPA_HW_PIPE_ID_0,
	IPA_HW_Q6_SIM_UL_CONSUMER_PIPE_1_ID     = IPA_HW_PIPE_ID_1,

	/* GSI UT channel SW->IPA */
	IPA_HW_Q6_GSI_UT_CONSUMER_PIPE_1_ID     = IPA_HW_PIPE_ID_3,
	/* GSI UT channel SW->IPA */
	IPA_HW_Q6_GSI_UT_CONSUMER_PIPE_2_ID     = IPA_HW_PIPE_ID_10,

	IPA_HW_Q6_SIM_UL_CONSUMER_PIPE_2_ID     = IPA_HW_PIPE_ID_7,

	/* GSI UT channel IPA->SW */
	IPA_HW_Q6_DIAG_CONSUMER_PIPE_ID         = IPA_HW_PIPE_ID_9,

	IPA_HW_Q6_SIM_DL_PRODUCER_PIPE_0_ID     = IPA_HW_PIPE_ID_23,
	IPA_HW_Q6_SIM_DL_PRODUCER_PIPE_1_ID     = IPA_HW_PIPE_ID_24,

	IPA_HW_Q6_SIM_DL_PRODUCER_PIPE_2_ID     = IPA_HW_PIPE_ID_25,

	/* GSI UT channel IPA->SW */
	IPA_HW_Q6_GSI_UT_PRODUCER_PIPE_1_ID     = IPA_HW_PIPE_ID_26,

	/* GSI UT channel IPA->SW */
	IPA_HW_Q6_GSI_UT_PRODUCER_PIPE_2_ID     = IPA_HW_PIPE_ID_27,
	IPA_HW_Q6_PIPE_ID_MAX                   = IPA_HW_PIPE_ID_MAX,
};

enum ipa_hw_q6_pipe_ch_id_e {
	/* Channels used by IPA Q6 driver */
	IPA_HW_Q6_DL_CONSUMER_PIPE_CH_ID                = 0,
	IPA_HW_Q6_CTL_CONSUMER_PIPE_CH_ID               = 1,
	IPA_HW_Q6_DL_NLO_CONSUMER_PIPE_CH_ID            = 2,
	IPA_HW_Q6_UL_ACC_PATH_ACK_PRODUCER_PIPE_CH_ID   = 6,
	IPA_HW_Q6_UL_PRODUCER_PIPE_CH_ID                = 7,
	IPA_HW_Q6_DL_PRODUCER_PIPE_CH_ID                = 3,
	IPA_HW_Q6_UL_ACC_PATH_DATA_PRODUCER_PIPE_CH_ID  = 5,
	IPA_HW_Q6_QBAP_STATUS_PRODUCER_PIPE_CH_ID       = 4,

	IPA_HW_Q6_DMA_ASYNC_CONSUMER_PIPE_CH_ID         = 8,
	IPA_HW_Q6_DMA_ASYNC_PRODUCER_PIPE_CH_ID         = 9,
	/* CH_ID 8 and 9 are Q6 SPARE CONSUMERs */

	/* Test Simulator Channels */
	IPA_HW_Q6_SIM_UL_CONSUMER_PIPE_0_CH_ID     = 10,
	IPA_HW_Q6_SIM_DL_PRODUCER_PIPE_0_CH_ID     = 11,
	IPA_HW_Q6_SIM_UL_CONSUMER_PIPE_1_CH_ID     = 12,
	IPA_HW_Q6_SIM_DL_PRODUCER_PIPE_1_CH_ID     = 13,
	IPA_HW_Q6_SIM_UL_CONSUMER_PIPE_2_CH_ID     = 14,
	IPA_HW_Q6_SIM_DL_PRODUCER_PIPE_2_CH_ID     = 15,
	/* GSI UT channel SW->IPA */
	IPA_HW_Q6_GSI_UT_CONSUMER_PIPE_1_CH_ID     = 16,
	/* GSI UT channel IPA->SW */
	IPA_HW_Q6_GSI_UT_PRODUCER_PIPE_1_CH_ID     = 17,
	/* GSI UT channel SW->IPA */
	IPA_HW_Q6_GSI_UT_CONSUMER_PIPE_2_CH_ID     = 18,
	/* GSI UT channel IPA->SW */
	IPA_HW_Q6_GSI_UT_PRODUCER_PIPE_2_CH_ID     = 19,
};

/* System Bam Endpoints between Q6 & IPA */
enum ipa_hw_q6_pipe_e {
	/* DL Pipe IPA->Q6 */
	IPA_HW_Q6_DL_PRODUCER_PIPE = 0,
	/* UL Pipe IPA->Q6 */
	IPA_HW_Q6_UL_PRODUCER_PIPE = 1,
	/* DL Pipe Q6->IPA */
	IPA_HW_Q6_DL_CONSUMER_PIPE = 2,
	/* CTL Pipe Q6->IPA */
	IPA_HW_Q6_CTL_CONSUMER_PIPE = 3,
	/*  Q6 -> IPA,  DL NLO  */
	IPA_HW_Q6_DL_NLO_CONSUMER_PIPE = 4,
	/* DMA ASYNC CONSUMER */
	IPA_HW_Q6_DMA_ASYNC_CONSUMER_PIPE = 5,
	/* DMA ASYNC PRODUCER */
	IPA_HW_Q6_DMA_ASYNC_PRODUCER_PIPE = 6,
	/* UL Acc Path Data Pipe IPA->Q6 */
	IPA_HW_Q6_UL_ACC_DATA_PRODUCER_PIPE = 7,
	/* UL Acc Path ACK Pipe IPA->Q6 */
	IPA_HW_Q6_UL_ACC_ACK_PRODUCER_PIPE = 8,
	/* UL Acc Path QBAP status Pipe IPA->Q6 */
	IPA_HW_Q6_QBAP_STATUS_PRODUCER_PIPE = 9,
	/* Diag status pipe IPA->Q6 */
	/* Used only when FEATURE_IPA_TEST_PER_SIM is ON */
	/* SIM Pipe IPA->Sim */
	IPA_HW_Q6_SIM_DL_PRODUCER_PIPE_0 = 10,
	/* SIM Pipe Sim->IPA */
	IPA_HW_Q6_SIM_DL_PRODUCER_PIPE_1 = 11,
	/* SIM Pipe Sim->IPA */
	IPA_HW_Q6_SIM_DL_PRODUCER_PIPE_2 = 12,
	/* SIM Pipe Sim->IPA */
	IPA_HW_Q6_SIM_UL_CONSUMER_PIPE_0 = 13,
	/* SIM B2B PROD Pipe  */
	IPA_HW_Q6_SIM_UL_CONSUMER_PIPE_1 = 14,
	/* SIM Pipe IPA->Sim */
	IPA_HW_Q6_SIM_UL_CONSUMER_PIPE_2 = 15,
	/* End FEATURE_IPA_TEST_PER_SIM */
	/* GSI UT channel SW->IPA */
	IPA_HW_Q6_GSI_UT_CONSUMER_PIPE_1 = 16,
	/* GSI UT channel IPA->SW */
	IPA_HW_Q6_GSI_UT_PRODUCER_PIPE_1 = 17,
	/* GSI UT channel SW->IPA */
	IPA_HW_Q6_GSI_UT_CONSUMER_PIPE_2 = 18,
	/* GSI UT channel IPA->SW */
	IPA_HW_Q6_GSI_UT_PRODUCER_PIPE_2 = 19,

	IPA_HW_Q6_PIPE_TOTAL
};

/* System Bam Endpoints between Q6 & IPA */
enum ipa_hw_q6_gsi_ev_e { /* In Sdx24 0..11 */
	/* DL Pipe IPA->Q6 */
	IPA_HW_Q6_DL_PRODUCER_PIPE_GSI_EV = 0,
	/* UL Pipe IPA->Q6 */
	IPA_HW_Q6_UL_PRODUCER_PIPE_GSI_EV = 1,
	/* DL Pipe Q6->IPA */
	//IPA_HW_Q6_DL_CONSUMER_PIPE_GSI_EV = 2,
	/* CTL Pipe Q6->IPA */
	//IPA_HW_Q6_CTL_CONSUMER_PIPE_GSI_EV = 3,
	/*  Q6 -> IPA,  LTE DL Optimized path */
	//IPA_HW_Q6_LTE_DL_CONSUMER_PIPE_GSI_EV = 4,
	/* LWA DL(Wifi to Q6) */
	//IPA_HW_Q6_LWA_DL_PRODUCER_PIPE_GSI_EV = 5,
	/* Diag status pipe IPA->Q6 */
	//IPA_HW_Q6_DIAG_STATUS_PRODUCER_PIPE_GSI_EV = 6,
	/* Used only when FEATURE_IPA_TEST_PER_SIM is ON */
	/* SIM Pipe IPA->Sim */
	IPA_HW_Q6_SIM_DL_PRODUCER_PIPE_0_GSI_EV = 2,
	/* SIM Pipe Sim->IPA */
	IPA_HW_Q6_SIM_DL_PRODUCER_PIPE_1_GSI_EV = 3,
	/* SIM Pipe Sim->IPA */
	IPA_HW_Q6_SIM_DL_PRODUCER_PIPE_2_GSI_EV = 4,
	/* SIM Pipe Sim->IPA */
	IPA_HW_Q6_SIM_1_GSI_EV = 5,
	IPA_HW_Q6_SIM_2_GSI_EV = 6,
	IPA_HW_Q6_SIM_3_GSI_EV = 7,
	IPA_HW_Q6_SIM_4_GSI_EV = 8,

	IPA_HW_Q6_PIPE_GSI_EV_TOTAL
};

/*
 * All the IRQ's supported by the IPA HW. Use this enum to set IRQ_EN
 * register and read IRQ_STTS register
 */
enum ipa_hw_irq_e {
	IPA_HW_IRQ_GSI_HWP                     = (1 << 25),
	IPA_HW_IRQ_GSI_IPA_IF_TLV_RCVD         = (1 << 24),
	IPA_HW_IRQ_GSI_EE_IRQ                  = (1 << 23),
	IPA_HW_IRQ_DCMP_ERR                    = (1 << 22),
	IPA_HW_IRQ_HWP_ERR                     = (1 << 21),
	IPA_HW_IRQ_RED_MARKER_ABOVE            = (1 << 20),
	IPA_HW_IRQ_YELLOW_MARKER_ABOVE         = (1 << 19),
	IPA_HW_IRQ_RED_MARKER_BELOW            = (1 << 18),
	IPA_HW_IRQ_YELLOW_MARKER_BELOW         = (1 << 17),
	IPA_HW_IRQ_BAM_IDLE_IRQ                = (1 << 16),
	IPA_HW_IRQ_TX_HOLB_DROP                = (1 << 15),
	IPA_HW_IRQ_TX_SUSPEND                  = (1 << 14),
	IPA_HW_IRQ_PROC_ERR                    = (1 << 13),
	IPA_HW_IRQ_STEP_MODE                   = (1 << 12),
	IPA_HW_IRQ_TX_ERR                      = (1 << 11),
	IPA_HW_IRQ_DEAGGR_ERR                  = (1 << 10),
	IPA_HW_IRQ_RX_ERR                      = (1 << 9),
	IPA_HW_IRQ_PROC_TO_HW_ACK_Q_NOT_EMPTY  = (1 << 8),
	IPA_HW_IRQ_HWP_RX_CMD_Q_NOT_FULL       = (1 << 7),
	IPA_HW_IRQ_HWP_IN_Q_NOT_EMPTY          = (1 << 6),
	IPA_HW_IRQ_HWP_IRQ_3                   = (1 << 5),
	IPA_HW_IRQ_HWP_IRQ_2                   = (1 << 4),
	IPA_HW_IRQ_HWP_IRQ_1                   = (1 << 3),
	IPA_HW_IRQ_HWP_IRQ_0                   = (1 << 2),
	IPA_HW_IRQ_EOT_COAL                    = (1 << 1),
	IPA_HW_IRQ_BAD_SNOC_ACCESS             = (1 << 0),
	IPA_HW_IRQ_NONE                        = 0,
	IPA_HW_IRQ_ALL                         = 0xFFFFFFFF
};

/*
 * All the IRQ sources supported by the IPA HW. Use this enum to set
 * IRQ_SRCS register
 */
enum ipa_hw_irq_srcs_e {
	IPA_HW_IRQ_SRCS_PIPE_0  = (1 << IPA_HW_PIPE_ID_0),
	IPA_HW_IRQ_SRCS_PIPE_1  = (1 << IPA_HW_PIPE_ID_1),
	IPA_HW_IRQ_SRCS_PIPE_2  = (1 << IPA_HW_PIPE_ID_2),
	IPA_HW_IRQ_SRCS_PIPE_3  = (1 << IPA_HW_PIPE_ID_3),
	IPA_HW_IRQ_SRCS_PIPE_4  = (1 << IPA_HW_PIPE_ID_4),
	IPA_HW_IRQ_SRCS_PIPE_5  = (1 << IPA_HW_PIPE_ID_5),
	IPA_HW_IRQ_SRCS_PIPE_6  = (1 << IPA_HW_PIPE_ID_6),
	IPA_HW_IRQ_SRCS_PIPE_7  = (1 << IPA_HW_PIPE_ID_7),
	IPA_HW_IRQ_SRCS_PIPE_8  = (1 << IPA_HW_PIPE_ID_8),
	IPA_HW_IRQ_SRCS_PIPE_9  = (1 << IPA_HW_PIPE_ID_9),
	IPA_HW_IRQ_SRCS_PIPE_10 = (1 << IPA_HW_PIPE_ID_10),
	IPA_HW_IRQ_SRCS_PIPE_11 = (1 << IPA_HW_PIPE_ID_11),
	IPA_HW_IRQ_SRCS_PIPE_12 = (1 << IPA_HW_PIPE_ID_12),
	IPA_HW_IRQ_SRCS_PIPE_13 = (1 << IPA_HW_PIPE_ID_13),
	IPA_HW_IRQ_SRCS_PIPE_14 = (1 << IPA_HW_PIPE_ID_14),
	IPA_HW_IRQ_SRCS_PIPE_15 = (1 << IPA_HW_PIPE_ID_15),
	IPA_HW_IRQ_SRCS_PIPE_16 = (1 << IPA_HW_PIPE_ID_16),
	IPA_HW_IRQ_SRCS_PIPE_17 = (1 << IPA_HW_PIPE_ID_17),
	IPA_HW_IRQ_SRCS_PIPE_18 = (1 << IPA_HW_PIPE_ID_18),
	IPA_HW_IRQ_SRCS_PIPE_19 = (1 << IPA_HW_PIPE_ID_19),
	IPA_HW_IRQ_SRCS_PIPE_20 = (1 << IPA_HW_PIPE_ID_20),
	IPA_HW_IRQ_SRCS_PIPE_21 = (1 << IPA_HW_PIPE_ID_21),
	IPA_HW_IRQ_SRCS_PIPE_22 = (1 << IPA_HW_PIPE_ID_22),
	IPA_HW_IRQ_SRCS_NONE    = 0,
	IPA_HW_IRQ_SRCS_ALL     = 0xFFFFFFFF,
};

/*
 * Total number of channel contexts that need to be saved for APPS
 */
#define IPA_HW_REG_SAVE_GSI_NUM_CH_CNTXT_A7          20

/*
 * Total number of channel contexts that need to be saved for UC
 */
#define IPA_HW_REG_SAVE_GSI_NUM_CH_CNTXT_UC          2

/*
 * Total number of event ring contexts that need to be saved for APPS
 */
#define IPA_HW_REG_SAVE_GSI_NUM_EVT_CNTXT_A7         19

/*
 * Total number of event ring contexts that need to be saved for UC
 */
#define IPA_HW_REG_SAVE_GSI_NUM_EVT_CNTXT_UC         1

/*
 * Total number of endpoints for which ipa_reg_save.pipes[endp_number]
 * are not saved by default (only if ipa_cfg.gen.full_reg_trace =
 * true) There is no extra endpoints in Stingray
 */
#define IPA_HW_REG_SAVE_NUM_ENDP_EXTRA               0

/*
 * Total number of endpoints for which ipa_reg_save.pipes[endp_number]
 * are always saved
 */
#define IPA_HW_REG_SAVE_NUM_ACTIVE_PIPES             IPA_HW_PIPE_ID_MAX

/*
 * SHRAM Bytes per ch
 */
#define IPA_REG_SAVE_BYTES_PER_CHNL_SHRAM         12

/*
 * Total number of rx splt cmdq's see:
 * ipa_rx_splt_cmdq_n_cmd[IPA_RX_SPLT_CMDQ_MAX]
 */
#define IPA_RX_SPLT_CMDQ_MAX 4

/*
 * Although not necessary for the numbers below, the use of round_up
 * is so that future developers know that these particular constants
 * have to be a multiple of four bytes, because the IPA memory reads
 * that they drive are always 32 bits...
 */
#define IPA_IU_ADDR   0x000A0000
#define IPA_IU_SIZE   round_up(40704, sizeof(u32))

#define IPA_SRAM_ADDR 0x00050000
#define IPA_SRAM_SIZE round_up(19232, sizeof(u32))

#define IPA_MBOX_ADDR 0x000C2000
#define IPA_MBOX_SIZE round_up(256, sizeof(u32))

#define IPA_HRAM_ADDR 0x00060000
#define IPA_HRAM_SIZE round_up(47536, sizeof(u32))

#define IPA_SEQ_ADDR  0x00081000
#define IPA_SEQ_SIZE  round_up(768, sizeof(u32))

#define IPA_GSI_ADDR  0x00006000
#define IPA_GSI_SIZE  round_up(5376, sizeof(u32))

/*
 * Macro to define a particular register cfg entry for all pipe
 * indexed register
 */
#define IPA_REG_SAVE_CFG_ENTRY_PIPE_ENDP(reg_name, var_name)	\
	{ GEN_1xVECTOR_REG_OFST(reg_name, 0), \
		(u32 *)&ipa_reg_save.ipa.pipes[0].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 1), \
		(u32 *)&ipa_reg_save.ipa.pipes[1].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 2), \
		(u32 *)&ipa_reg_save.ipa.pipes[2].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 3), \
		(u32 *)&ipa_reg_save.ipa.pipes[3].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 4), \
		(u32 *)&ipa_reg_save.ipa.pipes[4].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 5), \
		(u32 *)&ipa_reg_save.ipa.pipes[5].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 6), \
		(u32 *)&ipa_reg_save.ipa.pipes[6].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 7), \
		(u32 *)&ipa_reg_save.ipa.pipes[7].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 8), \
		(u32 *)&ipa_reg_save.ipa.pipes[8].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 9), \
		(u32 *)&ipa_reg_save.ipa.pipes[9].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 10), \
		(u32 *)&ipa_reg_save.ipa.pipes[10].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 11), \
		(u32 *)&ipa_reg_save.ipa.pipes[11].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 12), \
		(u32 *)&ipa_reg_save.ipa.pipes[12].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 13), \
		(u32 *)&ipa_reg_save.ipa.pipes[13].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 14), \
		(u32 *)&ipa_reg_save.ipa.pipes[14].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 15), \
		(u32 *)&ipa_reg_save.ipa.pipes[15].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 16), \
		(u32 *)&ipa_reg_save.ipa.pipes[16].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 17), \
		(u32 *)&ipa_reg_save.ipa.pipes[17].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 18), \
		(u32 *)&ipa_reg_save.ipa.pipes[18].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 19), \
		(u32 *)&ipa_reg_save.ipa.pipes[19].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 20), \
		(u32 *)&ipa_reg_save.ipa.pipes[20].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 21), \
		(u32 *)&ipa_reg_save.ipa.pipes[21].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 22), \
		(u32 *)&ipa_reg_save.ipa.pipes[22].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 23), \
		(u32 *)&ipa_reg_save.ipa.pipes[23].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 24), \
		(u32 *)&ipa_reg_save.ipa.pipes[24].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 25), \
		(u32 *)&ipa_reg_save.ipa.pipes[25].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 26), \
		(u32 *)&ipa_reg_save.ipa.pipes[26].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 27), \
		(u32 *)&ipa_reg_save.ipa.pipes[27].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 28), \
		(u32 *)&ipa_reg_save.ipa.pipes[28].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 29), \
		(u32 *)&ipa_reg_save.ipa.pipes[29].endp.var_name }, \
	{ GEN_1xVECTOR_REG_OFST(reg_name, 30), \
		(u32 *)&ipa_reg_save.ipa.pipes[30].endp.var_name }

/*
 * Macro to define a particular register cfg entry for the remaining
 * pipe indexed register.  In Stingray case we don't have extra
 * endpoints so it is intentially empty
 */
#define IPA_HW_REG_SAVE_CFG_ENTRY_PIPE_ENDP_EXTRA(REG_NAME, VAR_NAME)  \
	{ 0, 0 }

/*
 * Macro to set the active flag for all active pipe indexed register
 * In Stingray case we don't have extra endpoints so it is intentially
 * empty
 */
#define IPA_HW_REG_SAVE_CFG_ENTRY_PIPE_ENDP_EXTRA_ACTIVE()  \
	do { \
	} while (0)

#endif /* #if !defined(_IPA_HW_COMMON_EX_H_) */
