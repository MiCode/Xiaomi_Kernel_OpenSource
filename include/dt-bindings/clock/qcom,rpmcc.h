/*
 * Copyright 2015 Linaro Limited
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

/* RPM clocks */
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

/* SMD RPM clocks */
#define RPM_SMD_XO_CLK_SRC				0
#define RPM_SMD_XO_A_CLK_SRC			1
#define RPM_SMD_PCNOC_CLK				2
#define RPM_SMD_PCNOC_A_CLK				3
#define RPM_SMD_SNOC_CLK				4
#define RPM_SMD_SNOC_A_CLK				5
#define RPM_SMD_BIMC_CLK				6
#define RPM_SMD_BIMC_A_CLK				7
#define RPM_SMD_QDSS_CLK				8
#define RPM_SMD_QDSS_A_CLK				9
#define RPM_SMD_BB_CLK1				10
#define RPM_SMD_BB_CLK1_A				11
#define RPM_SMD_BB_CLK2				12
#define RPM_SMD_BB_CLK2_A				13
#define RPM_SMD_RF_CLK1				14
#define RPM_SMD_RF_CLK1_A				15
#define RPM_SMD_RF_CLK2				16
#define RPM_SMD_RF_CLK2_A				17
#define RPM_SMD_BB_CLK1_PIN				18
#define RPM_SMD_BB_CLK1_A_PIN			19
#define RPM_SMD_BB_CLK2_PIN				20
#define RPM_SMD_BB_CLK2_A_PIN			21
#define RPM_SMD_RF_CLK1_PIN				22
#define RPM_SMD_RF_CLK1_A_PIN			23
#define RPM_SMD_RF_CLK2_PIN				24
#define RPM_SMD_RF_CLK2_A_PIN			25
#define RPM_SMD_PNOC_CLK			26
#define RPM_SMD_PNOC_A_CLK			27
#define RPM_SMD_CNOC_CLK			28
#define RPM_SMD_CNOC_A_CLK			29
#define RPM_SMD_MMSSNOC_AHB_CLK			30
#define RPM_SMD_MMSSNOC_AHB_A_CLK		31
#define RPM_SMD_GFX3D_CLK_SRC			32
#define RPM_SMD_GFX3D_A_CLK_SRC			33
#define RPM_SMD_OCMEMGX_CLK			34
#define RPM_SMD_OCMEMGX_A_CLK			35
#define RPM_SMD_CXO_D0				36
#define RPM_SMD_CXO_D0_A			37
#define RPM_SMD_CXO_D1				38
#define RPM_SMD_CXO_D1_A			39
#define RPM_SMD_CXO_A0				40
#define RPM_SMD_CXO_A0_A			41
#define RPM_SMD_CXO_A1				42
#define RPM_SMD_CXO_A1_A			43
#define RPM_SMD_CXO_A2				44
#define RPM_SMD_CXO_A2_A			45
#define RPM_SMD_DIV_CLK1			46
#define RPM_SMD_DIV_A_CLK1			47
#define RPM_SMD_DIV_CLK2			48
#define RPM_SMD_DIV_A_CLK2			49
#define RPM_SMD_DIFF_CLK			50
#define RPM_SMD_DIFF_A_CLK			51
#define RPM_SMD_CXO_D0_PIN			52
#define RPM_SMD_CXO_D0_A_PIN			53
#define RPM_SMD_CXO_D1_PIN			54
#define RPM_SMD_CXO_D1_A_PIN			55
#define RPM_SMD_CXO_A0_PIN			56
#define RPM_SMD_CXO_A0_A_PIN			57
#define RPM_SMD_CXO_A1_PIN			58
#define RPM_SMD_CXO_A1_A_PIN			59
#define RPM_SMD_CXO_A2_PIN			60
#define RPM_SMD_CXO_A2_A_PIN			61
#define RPM_SMD_QPIC_CLK			64
#define RPM_SMD_QPIC_A_CLK			65
#define RPM_SMD_CE1_CLK				66
#define RPM_SMD_CE1_A_CLK			67
#define RPM_SMD_BIMC_GPU_CLK                    68
#define RPM_SMD_BIMC_GPU_A_CLK                  69
#define RPM_SMD_LN_BB_CLK			70
#define RPM_SMD_LN_BB_CLK_A			71
#define RPM_SMD_LN_BB_CLK_PIN			72
#define RPM_SMD_LN_BB_CLK_A_PIN			73
#define RPM_SMD_RF_CLK3				74
#define RPM_SMD_RF_CLK3_A			75
#define RPM_SMD_RF_CLK3_PIN			76
#define RPM_SMD_RF_CLK3_A_PIN			77
#define PNOC_MSMBUS_CLK				78
#define PNOC_MSMBUS_A_CLK			79
#define PNOC_KEEPALIVE_A_CLK			80
#define SNOC_MSMBUS_CLK				81
#define SNOC_MSMBUS_A_CLK			82
#define BIMC_MSMBUS_CLK				83
#define BIMC_MSMBUS_A_CLK			84
#define PNOC_USB_CLK				85
#define PNOC_USB_A_CLK				86
#define SNOC_USB_CLK				87
#define SNOC_USB_A_CLK				88
#define BIMC_USB_CLK				89
#define BIMC_USB_A_CLK				90
#define SNOC_WCNSS_A_CLK			91
#define BIMC_WCNSS_A_CLK			92
#define MCD_CE1_CLK				93
#define QCEDEV_CE1_CLK				94
#define QCRYPTO_CE1_CLK				95
#define QSEECOM_CE1_CLK				96
#define SCM_CE1_CLK				97
#define CXO_SMD_OTG_CLK				98
#define CXO_SMD_LPM_CLK				99
#define CXO_SMD_PIL_PRONTO_CLK			100
#define CXO_SMD_PIL_MSS_CLK			101
#define CXO_SMD_WLAN_CLK			102
#define CXO_SMD_PIL_LPASS_CLK			103
#define CXO_SMD_PIL_CDSP_CLK			104

#endif
