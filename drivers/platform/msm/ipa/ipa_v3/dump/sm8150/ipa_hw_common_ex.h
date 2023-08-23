/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#if !defined(_IPA_HW_COMMON_EX_H_)
#define _IPA_HW_COMMON_EX_H_

/* VLVL defs are available for 854 */
#define FEATURE_VLVL_DEFS                            true

/* IPAv4 version flag for Sdx24 */
#define FEATURE_IPA_HW_VERSION_4_0                   true

/* Important Platform Specific Values : IRQ_NUM, IRQ_CNT, BCR */
#define IPA_HW_BAM_IRQ_NUM                           440

/* Q6 IRQ number for IPA. Fetched from IPCatalog */
#define IPA_HW_IRQ_NUM                               441

/* Total number of different interrupts that can be enabled */
#define IPA_HW_IRQ_CNT_TOTAL                         23

/* IPAv4 BCR value */
#define IPA_HW_BCR_REG_VAL                           0x00000039

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
#define IPA_HW_PKT_PROCESS_HPS_PKT_PRS_DECIPH_UCP       0x3
#define IPA_HW_PKT_PROCESS_HPS_2_PKT_PRS_NO_DECIPH      0x4
#define IPA_HW_PKT_PROCESS_HPS_2_PKT_PRS_DECIPH         0x5
#define IPA_HW_PKT_PROCESS_HPS_PKT_PRS_NO_DECIPH_NO_UCP 0x6
#define IPA_HW_PKT_PROCESS_HPS_PKT_PRS_DECIPH_NO_UCP    0x7
#define IPA_HW_PKT_PROCESS_HPS_DMA_PARSER               0x8
#define IPA_HW_PKT_PROCESS_HPS_DMA_DECIPH_PARSER        0x9

/* DPS Sequences */
#define IPA_HW_PKT_PROCESS_DPS_DMA                      0x0
#define IPA_HW_PKT_PROCESS_DPS_DMA_WITH_DECIPH          0x1
#define IPA_HW_PKT_PROCESS_DPS_DMA_WITH_DECOMP          0x2

/* Src RSRC GRP config */
#define IPA_HW_SRC_RSRC_GRP_01_RSRC_TYPE_0           0x05050404
#define IPA_HW_SRC_RSRC_GRP_01_RSRC_TYPE_1           0x0A0A0A0A
#define IPA_HW_SRC_RSRC_GRP_01_RSRC_TYPE_2           0x0C0C0C0C
#define IPA_HW_SRC_RSRC_GRP_01_RSRC_TYPE_3           0x3F003F00
#define IPA_HW_SRC_RSRC_GRP_01_RSRC_TYPE_4           0x0E0E0E0E

#define IPA_HW_SRC_RSRC_GRP_23_RSRC_TYPE_0           0x00000101
#define IPA_HW_SRC_RSRC_GRP_23_RSRC_TYPE_1           0x00000808
#define IPA_HW_SRC_RSRC_GRP_23_RSRC_TYPE_2           0x00000808
#define IPA_HW_SRC_RSRC_GRP_23_RSRC_TYPE_3           0x3F003F00
#define IPA_HW_SRC_RSRC_GRP_23_RSRC_TYPE_4           0x00000E0E

/* Dest RSRC GRP config */
#define IPA_HW_DST_RSRC_GRP_01_RSRC_TYPE_0           0x04040404
#define IPA_HW_DST_RSRC_GRP_01_RSRC_TYPE_1           0x3F013F02

#define IPA_HW_DST_RSRC_GRP_23_RSRC_TYPE_0           0x02020303
#define IPA_HW_DST_RSRC_GRP_23_RSRC_TYPE_1           0x02000201


#define IPA_HW_RX_HPS_CLIENTS_MIN_DEPTH_0            0x00020703
#define IPA_HW_RX_HPS_CLIENTS_MAX_DEPTH_0            0x00020703

#define IPA_HW_RSRP_GRP_0                            0x0
#define IPA_HW_RSRP_GRP_1                            0x1
#define IPA_HW_RSRP_GRP_2                            0x2
#define IPA_HW_RSRP_GRP_3                            0x3

#define IPA_HW_PCIE_SRC_RSRP_GRP                     IPA_HW_RSRP_GRP_0
#define IPA_HW_PCIE_DEST_RSRP_GRP                    IPA_HW_RSRP_GRP_0

#define IPA_HW_DDR_SRC_RSRP_GRP                      IPA_HW_RSRP_GRP_1
#define IPA_HW_DDR_DEST_RSRP_GRP                     IPA_HW_RSRP_GRP_1

#define IPA_HW_SRC_RSRP_TYPE_MAX                     0x5
#define IPA_HW_DST_RSRP_TYPE_MAX                     0x2

#define GSI_HW_QSB_LOG_MISC_MAX 0x4

/* IPA Clock Bus Client name */
#define IPA_CLK_BUS_CLIENT_NAME                      "IPA_PCNOC_BUS_CLIENT"

/* Is IPA decompression feature enabled */
#define IPA_HW_IS_DECOMPRESSION_ENABLED              (1)

/* Whether to allow setting step mode on IPA when we crash or not */
#define IPA_HW_IS_STEP_MODE_ALLOWED                  (true)

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
	IPA_HW_CLK_FREQ_IPA_PEAK        = 500000,
	IPA_HW_CLK_FREQ_IPA_NOM_PLUS    = 440000,
	IPA_HW_CLK_FREQ_IPA_NOM         = 440000,
	IPA_HW_CLK_FREQ_IPA_SVS         = 250000,
	IPA_HW_CLK_FREQ_IPA_SVS_2       = 120000,

	/* Q6 CPU */
	IPA_HW_CLK_FREQ_Q6_PEAK         = 729600,
	IPA_HW_CLK_FREQ_Q6_NOM_PLUS     = 729600,
	IPA_HW_CLK_FREQ_Q6_NOM          = 729600,
	IPA_HW_CLK_FREQ_Q6_SVS          = 729600,
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
	IPA_HW_PIPE_ID_MAX
};

/* Pipe ID's of System Bam Endpoints between Q6 & IPA */
enum ipa_hw_q6_pipe_id_e {
	/* Pipes used by IPA Q6 driver */
	IPA_HW_Q6_DL_CONSUMER_PIPE_ID           = IPA_HW_PIPE_ID_3,
	IPA_HW_Q6_CTL_CONSUMER_PIPE_ID          = IPA_HW_PIPE_ID_4,
	IPA_HW_Q6_UL_PRODUCER_PIPE_ID           = IPA_HW_PIPE_ID_13,
	IPA_HW_Q6_DL_PRODUCER_PIPE_ID           = IPA_HW_PIPE_ID_14,

	IPA_HW_Q6_LTE_DL_CONSUMER_PIPE_ID       = IPA_HW_PIPE_ID_6,
	IPA_HW_Q6_LWA_DL_PRODUCER_PIPE_ID       = IPA_HW_PIPE_ID_16,
	/* Test Simulator Pipes */
	IPA_HW_Q6_SIM_UL_CONSUMER_PIPE_0_ID     = IPA_HW_PIPE_ID_0,
	IPA_HW_Q6_SIM_DL_PRODUCER_PIPE_0_ID     = IPA_HW_PIPE_ID_12,
	IPA_HW_Q6_SIM_UL_CONSUMER_PIPE_1_ID     = IPA_HW_PIPE_ID_1,
	IPA_HW_Q6_SIM_DL_PRODUCER_PIPE_1_ID     = IPA_HW_PIPE_ID_10,
	IPA_HW_Q6_SIM_UL_CONSUMER_PIPE_2_ID     = IPA_HW_PIPE_ID_2,
	IPA_HW_Q6_SIM_DL_PRODUCER_PIPE_2_ID     = IPA_HW_PIPE_ID_11,
	/* GSI UT channel SW->IPA */
	IPA_HW_Q6_GSI_UT_CONSUMER_PIPE_1_ID     = IPA_HW_PIPE_ID_5,
	/* GSI UT channel IPA->SW */
	IPA_HW_Q6_GSI_UT_PRODUCER_PIPE_1_ID     = IPA_HW_PIPE_ID_17,
	/* GSI UT channel SW->IPA */
	IPA_HW_Q6_GSI_UT_CONSUMER_PIPE_2_ID     = IPA_HW_PIPE_ID_7,
	/* GSI UT channel IPA->SW */
	IPA_HW_Q6_GSI_UT_PRODUCER_PIPE_2_ID     = IPA_HW_PIPE_ID_18,
	IPA_HW_Q6_DIAG_CONSUMER_PIPE_ID         = IPA_HW_PIPE_ID_19,
	IPA_HW_Q6_PIPE_ID_MAX                   = IPA_HW_PIPE_ID_MAX,
};

enum ipa_hw_q6_pipe_ch_id_e {
	/* Channels used by IPA Q6 driver */
	IPA_HW_Q6_DL_CONSUMER_PIPE_CH_ID           = 0,
	IPA_HW_Q6_CTL_CONSUMER_PIPE_CH_ID          = 1,
	IPA_HW_Q6_UL_PRODUCER_PIPE_CH_ID           = 3,
	IPA_HW_Q6_DL_PRODUCER_PIPE_CH_ID           = 4,

	IPA_HW_Q6_LTE_DL_CONSUMER_PIPE_CH_ID       = 2,
	IPA_HW_Q6_LWA_DL_PRODUCER_PIPE_CH_ID       = 5,
	/* Test Simulator Channels */
	IPA_HW_Q6_SIM_UL_CONSUMER_PIPE_0_CH_ID     = 6,
	IPA_HW_Q6_SIM_DL_PRODUCER_PIPE_0_CH_ID     = 8,
	IPA_HW_Q6_SIM_UL_CONSUMER_PIPE_1_CH_ID     = 9,
	IPA_HW_Q6_SIM_DL_PRODUCER_PIPE_1_CH_ID     = 10,
	IPA_HW_Q6_SIM_UL_CONSUMER_PIPE_2_CH_ID     = 11,
	IPA_HW_Q6_SIM_DL_PRODUCER_PIPE_2_CH_ID     = 12,
	/* GSI UT channel SW->IPA */
	IPA_HW_Q6_GSI_UT_CONSUMER_PIPE_1_CH_ID     = 13,
	/* GSI UT channel IPA->SW */
	IPA_HW_Q6_GSI_UT_PRODUCER_PIPE_1_CH_ID     = 14,
	/* GSI UT channel SW->IPA */
	IPA_HW_Q6_GSI_UT_CONSUMER_PIPE_2_CH_ID     = 15,
	/* GSI UT channel IPA->SW */
	IPA_HW_Q6_GSI_UT_PRODUCER_PIPE_2_CH_ID     = 16,
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
	/*  Q6 -> IPA,  LTE DL Optimized path */
	IPA_HW_Q6_LTE_DL_CONSUMER_PIPE = 4,
	/* LWA DL(Wifi to Q6) */
	IPA_HW_Q6_LWA_DL_PRODUCER_PIPE = 5,
	/* Diag status pipe IPA->Q6 */
	/* Used only when FEATURE_IPA_TEST_PER_SIM is ON */
	/* SIM Pipe IPA->Sim */
	IPA_HW_Q6_SIM_DL_PRODUCER_PIPE_0 = 7,
	/* SIM Pipe Sim->IPA */
	IPA_HW_Q6_SIM_DL_PRODUCER_PIPE_1 = 8,
	/* SIM Pipe Sim->IPA */
	IPA_HW_Q6_SIM_DL_PRODUCER_PIPE_2 = 9,
	/* SIM Pipe Sim->IPA */
	IPA_HW_Q6_SIM_UL_CONSUMER_PIPE_0 = 10,
	/* SIM B2B PROD Pipe  */
	IPA_HW_Q6_SIM_UL_CONSUMER_PIPE_1 = 11,
	/* SIM Pipe IPA->Sim */
	IPA_HW_Q6_SIM_UL_CONSUMER_PIPE_2 = 12,
	/* End FEATURE_IPA_TEST_PER_SIM */
	/* GSI UT channel SW->IPA */
	IPA_HW_Q6_GSI_UT_CONSUMER_PIPE_1 = 13,
	/* GSI UT channel IPA->SW */
	IPA_HW_Q6_GSI_UT_PRODUCER_PIPE_1 = 14,
	/* GSI UT channel SW->IPA */
	IPA_HW_Q6_GSI_UT_CONSUMER_PIPE_2 = 15,
	/* GSI UT channel IPA->SW */
	IPA_HW_Q6_GSI_UT_PRODUCER_PIPE_2 = 16,
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
#define IPA_HW_REG_SAVE_GSI_NUM_CH_CNTXT_A7          15

/*
 * Total number of channel contexts that need to be saved for UC
 */
#define IPA_HW_REG_SAVE_GSI_NUM_CH_CNTXT_UC          4

/*
 * Total number of event ring contexts that need to be saved for APPS
 */
#define IPA_HW_REG_SAVE_GSI_NUM_EVT_CNTXT_A7         12

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
#define IPA_HW_REG_SAVE_NUM_ACTIVE_PIPES             23

/*
 * Macro to set the active flag for all active pipe indexed register
 */
#define IPA_HW_REG_SAVE_CFG_ENTRY_PIPE_ENDP_ACTIVE() \
	do { \
		ipa_reg_save.ipa.pipes[0].active = true; \
		ipa_reg_save.ipa.pipes[1].active = true; \
		ipa_reg_save.ipa.pipes[2].active = true; \
		ipa_reg_save.ipa.pipes[3].active = true; \
		ipa_reg_save.ipa.pipes[4].active = true; \
		ipa_reg_save.ipa.pipes[5].active = true; \
		ipa_reg_save.ipa.pipes[6].active = true; \
		ipa_reg_save.ipa.pipes[7].active = true; \
		ipa_reg_save.ipa.pipes[8].active = true; \
		ipa_reg_save.ipa.pipes[9].active = true; \
		ipa_reg_save.ipa.pipes[10].active = true; \
		ipa_reg_save.ipa.pipes[11].active = true; \
		ipa_reg_save.ipa.pipes[12].active = true; \
		ipa_reg_save.ipa.pipes[13].active = true; \
		ipa_reg_save.ipa.pipes[14].active = true; \
		ipa_reg_save.ipa.pipes[15].active = true; \
		ipa_reg_save.ipa.pipes[16].active = true; \
		ipa_reg_save.ipa.pipes[17].active = true; \
		ipa_reg_save.ipa.pipes[18].active = true; \
		ipa_reg_save.ipa.pipes[19].active = true; \
		ipa_reg_save.ipa.pipes[20].active = true; \
		ipa_reg_save.ipa.pipes[21].active = true; \
		ipa_reg_save.ipa.pipes[22].active = true; \
	} while (0)

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
