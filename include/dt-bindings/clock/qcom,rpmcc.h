/*
 * Copyright 2015 Linaro Limited
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _DT_BINDINGS_CLK_MSM_RPMCC_H
#define _DT_BINDINGS_CLK_MSM_RPMCC_H

/* apq8064 */
#define RPM_PXO_CLK				0
#define RPM_PXO_A_CLK				1
#define RPM_CXO_CLK				2
#define RPM_CXO_A_CLK				3
#define RPM_APPS_FABRIC_CLK			4
#define RPM_APPS_FABRIC_A_CLK			5
#define RPM_CFPB_CLK				6
#define RPM_CFPB_A_CLK				7
#define RPM_QDSS_CLK				8
#define RPM_QDSS_A_CLK				9
#define RPM_DAYTONA_FABRIC_CLK			10
#define RPM_DAYTONA_FABRIC_A_CLK		11
#define RPM_EBI1_CLK				12
#define RPM_EBI1_A_CLK				13
#define RPM_MM_FABRIC_CLK			14
#define RPM_MM_FABRIC_A_CLK			15
#define RPM_MMFPB_CLK				16
#define RPM_MMFPB_A_CLK				17
#define RPM_SYS_FABRIC_CLK			18
#define RPM_SYS_FABRIC_A_CLK			19
#define RPM_SFPB_CLK				20
#define RPM_SFPB_A_CLK				21

/* msm8916 and msm8996 */
#define RPM_XO_CLK_SRC				0
#define RPM_XO_A_CLK_SRC			1
#define RPM_PCNOC_CLK				2
#define RPM_PCNOC_A_CLK				3
#define RPM_SNOC_CLK				4
#define RPM_SNOC_A_CLK				5
#define RPM_BIMC_CLK				6
#define RPM_BIMC_A_CLK				7
#define RPM_QDSS_CLK				8
#define RPM_QDSS_A_CLK				9
#define RPM_BB_CLK1				10
#define RPM_BB_CLK1_A				11
#define RPM_BB_CLK1_PIN				12
#define RPM_BB_CLK1_A_PIN			13
#define RPM_BB_CLK2				14
#define RPM_BB_CLK2_A				15
#define RPM_BB_CLK2_PIN				16
#define RPM_BB_CLK2_A_PIN			17
#define RPM_RF_CLK1				18
#define RPM_RF_CLK1_A				19
#define RPM_RF_CLK1_PIN				20
#define RPM_RF_CLK1_A_PIN			21
#define RPM_RF_CLK2				22
#define RPM_RF_CLK2_A				23
#define RPM_RF_CLK2_PIN				24
#define RPM_RF_CLK2_A_PIN			25
#define RPM_AGGR1_NOC_CLK			26
#define RPM_AGGR1_NOC_A_CLK			27
#define RPM_AGGR2_NOC_CLK			28
#define RPM_AGGR2_NOC_A_CLK			29
#define RPM_CNOC_CLK				30
#define RPM_CNOC_A_CLK				31
#define RPM_MMAXI_CLK				32
#define RPM_MMAXI_A_CLK				33
#define RPM_IPA_CLK				34
#define RPM_IPA_A_CLK				35
#define RPM_CE1_CLK				36
#define RPM_CE1_A_CLK				37
#define RPM_DIV_CLK1				38
#define RPM_DIV_CLK1_AO				39
#define RPM_DIV_CLK2				40
#define RPM_DIV_CLK2_AO				41
#define RPM_DIV_CLK3				42
#define RPM_DIV_CLK3_AO				43
#define RPM_LN_BB_CLK				44
#define RPM_LN_BB_A_CLK				45
#define RPM_LN_BB_CLK1				46
#define RPM_LN_BB_CLK1_AO			47
#define RPM_LN_BB_CLK1_PIN			48
#define RPM_LN_BB_CLK1_PIN_AO			49
#define RPM_LN_BB_CLK2				50
#define RPM_LN_BB_CLK2_AO			51
#define RPM_LN_BB_CLK2_PIN			52
#define RPM_LN_BB_CLK2_PIN_AO			53
#define RPM_LN_BB_CLK3				54
#define RPM_LN_BB_CLK3_AO			55
#define RPM_LN_BB_CLK3_PIN			56
#define RPM_LN_BB_CLK3_PIN_AO			57
#define RPM_CNOC_PERIPH_CLK			58
#define RPM_CNOC_PERIPH_A_CLK			59

/* Voter clocks */
#define  MMSSNOC_AXI_CLK			60
#define  MMSSNOC_AXI_A_CLK			61
#define  MMSSNOC_GDS_CLK			62
#define  BIMC_MSMBUS_CLK			63
#define  BIMC_MSMBUS_A_CLK			64
#define  CNOC_MSMBUS_CLK			65
#define  CNOC_MSMBUS_A_CLK			66
#define  PNOC_KEEPALIVE_A_CLK			67
#define  PNOC_MSMBUS_CLK			68
#define  PNOC_MSMBUS_A_CLK			69
#define  PNOC_PM_CLK				70
#define  PNOC_SPS_CLK				71
#define  MCD_CE1_CLK				72
#define  QCEDEV_CE1_CLK				73
#define  QCRYPTO_CE1_CLK			74
#define  QSEECOM_CE1_CLK			75
#define  SCM_CE1_CLK				76
#define  SNOC_MSMBUS_CLK			77
#define  SNOC_MSMBUS_A_CLK			78
#define  CXO_DWC3_CLK				79
#define  CXO_LPM_CLK				80
#define  CXO_OTG_CLK				81
#define  CXO_PIL_LPASS_CLK			82
#define  CXO_PIL_SSC_CLK			83
#define  CXO_PIL_CDSP_CLK			84
#define  CNOC_PERIPH_KEEPALIVE_A_CLK		85
#define  MMSSNOC_A_CLK_CPU_VOTE			86

#endif
