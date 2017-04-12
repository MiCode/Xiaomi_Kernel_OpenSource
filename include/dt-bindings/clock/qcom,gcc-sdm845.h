/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#ifndef _DT_BINDINGS_CLK_MSM_GCC_SDM845_H
#define _DT_BINDINGS_CLK_MSM_GCC_SDM845_H

/* GCC clock registers */
#define GCC_AGGRE_NOC_PCIE_TBU_CLK				0
#define GCC_AGGRE_UFS_CARD_AXI_CLK				1
#define GCC_AGGRE_UFS_PHY_AXI_CLK				2
#define GCC_AGGRE_USB3_PRIM_AXI_CLK				3
#define GCC_AGGRE_USB3_SEC_AXI_CLK				4
#define GCC_BOOT_ROM_AHB_CLK					5
#define GCC_CAMERA_AHB_CLK					6
#define GCC_CAMERA_AXI_CLK					7
#define GCC_CAMERA_XO_CLK					8
#define GCC_CE1_AHB_CLK						9
#define GCC_CE1_AXI_CLK						10
#define GCC_CE1_CLK						11
#define GCC_CFG_NOC_USB3_PRIM_AXI_CLK				12
#define GCC_CFG_NOC_USB3_SEC_AXI_CLK				13
#define GCC_CPUSS_AHB_CLK					14
#define GCC_CPUSS_AHB_CLK_SRC					15
#define GCC_CPUSS_DVM_BUS_CLK					16
#define GCC_CPUSS_GNOC_CLK					17
#define GCC_CPUSS_RBCPR_CLK					18
#define GCC_CPUSS_RBCPR_CLK_SRC					19
#define GCC_CXO_TX1_CLKREF_CLK					20
#define GCC_DDRSS_GPU_AXI_CLK					21
#define GCC_DISP_AHB_CLK					22
#define GCC_DISP_AXI_CLK					23
#define GCC_DISP_GPLL0_CLK_SRC					24
#define GCC_DISP_GPLL0_DIV_CLK_SRC				25
#define GCC_DISP_XO_CLK						26
#define GCC_GP1_CLK						27
#define GCC_GP1_CLK_SRC						28
#define GCC_GP2_CLK						29
#define GCC_GP2_CLK_SRC						30
#define GCC_GP3_CLK						31
#define GCC_GP3_CLK_SRC						32
#define GCC_GPU_CFG_AHB_CLK					33
#define GCC_GPU_GPLL0_CLK_SRC					34
#define GCC_GPU_GPLL0_DIV_CLK_SRC				35
#define GCC_GPU_MEMNOC_GFX_CLK					36
#define GCC_GPU_SNOC_DVM_GFX_CLK				37
#define GCC_MSS_AXIS2_CLK					38
#define GCC_MSS_CFG_AHB_CLK					39
#define GCC_MSS_GPLL0_DIV_CLK_SRC				40
#define GCC_MSS_MFAB_AXIS_CLK					41
#define GCC_MSS_Q6_MEMNOC_AXI_CLK				42
#define GCC_MSS_SNOC_AXI_CLK					43
#define GCC_PCIE_0_AUX_CLK					44
#define GCC_PCIE_0_AUX_CLK_SRC					45
#define GCC_PCIE_0_CFG_AHB_CLK					46
#define GCC_PCIE_0_CLKREF_CLK					47
#define GCC_PCIE_0_MSTR_AXI_CLK					48
#define GCC_PCIE_0_PIPE_CLK					49
#define GCC_PCIE_0_SLV_AXI_CLK					50
#define GCC_PCIE_0_SLV_Q2A_AXI_CLK				51
#define GCC_PCIE_1_AUX_CLK					52
#define GCC_PCIE_1_AUX_CLK_SRC					53
#define GCC_PCIE_1_CFG_AHB_CLK					54
#define GCC_PCIE_1_CLKREF_CLK					55
#define GCC_PCIE_1_MSTR_AXI_CLK					56
#define GCC_PCIE_1_PIPE_CLK					57
#define GCC_PCIE_1_SLV_AXI_CLK					58
#define GCC_PCIE_1_SLV_Q2A_AXI_CLK				59
#define GCC_PCIE_PHY_AUX_CLK					60
#define GCC_PCIE_PHY_REFGEN_CLK					61
#define GCC_PCIE_PHY_REFGEN_CLK_SRC				62
#define GCC_PDM2_CLK						63
#define GCC_PDM2_CLK_SRC					64
#define GCC_PDM_AHB_CLK						65
#define GCC_PDM_XO4_CLK						66
#define GCC_PRNG_AHB_CLK					67
#define GCC_QMIP_CAMERA_AHB_CLK					68
#define GCC_QMIP_DISP_AHB_CLK					69
#define GCC_QMIP_VIDEO_AHB_CLK					70
#define GCC_QUPV3_WRAP0_S0_CLK					71
#define GCC_QUPV3_WRAP0_S0_CLK_SRC				72
#define GCC_QUPV3_WRAP0_S1_CLK					73
#define GCC_QUPV3_WRAP0_S1_CLK_SRC				74
#define GCC_QUPV3_WRAP0_S2_CLK					75
#define GCC_QUPV3_WRAP0_S2_CLK_SRC				76
#define GCC_QUPV3_WRAP0_S3_CLK					77
#define GCC_QUPV3_WRAP0_S3_CLK_SRC				78
#define GCC_QUPV3_WRAP0_S4_CLK					79
#define GCC_QUPV3_WRAP0_S4_CLK_SRC				80
#define GCC_QUPV3_WRAP0_S5_CLK					81
#define GCC_QUPV3_WRAP0_S5_CLK_SRC				82
#define GCC_QUPV3_WRAP0_S6_CLK					83
#define GCC_QUPV3_WRAP0_S6_CLK_SRC				84
#define GCC_QUPV3_WRAP0_S7_CLK					85
#define GCC_QUPV3_WRAP0_S7_CLK_SRC				86
#define GCC_QUPV3_WRAP1_S0_CLK					87
#define GCC_QUPV3_WRAP1_S0_CLK_SRC				88
#define GCC_QUPV3_WRAP1_S1_CLK					89
#define GCC_QUPV3_WRAP1_S1_CLK_SRC				90
#define GCC_QUPV3_WRAP1_S2_CLK					91
#define GCC_QUPV3_WRAP1_S2_CLK_SRC				92
#define GCC_QUPV3_WRAP1_S3_CLK					93
#define GCC_QUPV3_WRAP1_S3_CLK_SRC				94
#define GCC_QUPV3_WRAP1_S4_CLK					95
#define GCC_QUPV3_WRAP1_S4_CLK_SRC				96
#define GCC_QUPV3_WRAP1_S5_CLK					97
#define GCC_QUPV3_WRAP1_S5_CLK_SRC				98
#define GCC_QUPV3_WRAP1_S6_CLK					99
#define GCC_QUPV3_WRAP1_S6_CLK_SRC				100
#define GCC_QUPV3_WRAP1_S7_CLK					101
#define GCC_QUPV3_WRAP1_S7_CLK_SRC				102
#define GCC_QUPV3_WRAP_0_M_AHB_CLK				103
#define GCC_QUPV3_WRAP_0_S_AHB_CLK				104
#define GCC_QUPV3_WRAP_1_M_AHB_CLK				105
#define GCC_QUPV3_WRAP_1_S_AHB_CLK				106
#define GCC_RX1_USB2_CLKREF_CLK					107
#define GCC_RX2_QLINK_CLKREF_CLK				108
#define GCC_RX3_MODEM_CLKREF_CLK				109
#define GCC_SDCC2_AHB_CLK					110
#define GCC_SDCC2_APPS_CLK					111
#define GCC_SDCC2_APPS_CLK_SRC					112
#define GCC_SDCC4_AHB_CLK					113
#define GCC_SDCC4_APPS_CLK					114
#define GCC_SDCC4_APPS_CLK_SRC					115
#define GCC_SYS_NOC_CPUSS_AHB_CLK				116
#define GCC_TSIF_AHB_CLK					117
#define GCC_TSIF_INACTIVITY_TIMERS_CLK				118
#define GCC_TSIF_REF_CLK					119
#define GCC_TSIF_REF_CLK_SRC					120
#define GCC_UFS_CARD_AHB_CLK					121
#define GCC_UFS_CARD_AXI_CLK					122
#define GCC_UFS_CARD_AXI_CLK_SRC				123
#define GCC_UFS_CARD_CLKREF_CLK					124
#define GCC_UFS_CARD_ICE_CORE_CLK				125
#define GCC_UFS_CARD_ICE_CORE_CLK_SRC				126
#define GCC_UFS_CARD_PHY_AUX_CLK				127
#define GCC_UFS_CARD_PHY_AUX_CLK_SRC				128
#define GCC_UFS_CARD_RX_SYMBOL_0_CLK				129
#define GCC_UFS_CARD_RX_SYMBOL_1_CLK				130
#define GCC_UFS_CARD_TX_SYMBOL_0_CLK				131
#define GCC_UFS_CARD_UNIPRO_CORE_CLK				132
#define GCC_UFS_CARD_UNIPRO_CORE_CLK_SRC			133
#define GCC_UFS_MEM_CLKREF_CLK					134
#define GCC_UFS_PHY_AHB_CLK					135
#define GCC_UFS_PHY_AXI_CLK					136
#define GCC_UFS_PHY_AXI_CLK_SRC					137
#define GCC_UFS_PHY_ICE_CORE_CLK				138
#define GCC_UFS_PHY_ICE_CORE_CLK_SRC				139
#define GCC_UFS_PHY_PHY_AUX_CLK					140
#define GCC_UFS_PHY_PHY_AUX_CLK_SRC				141
#define GCC_UFS_PHY_RX_SYMBOL_0_CLK				142
#define GCC_UFS_PHY_RX_SYMBOL_1_CLK				143
#define GCC_UFS_PHY_TX_SYMBOL_0_CLK				144
#define GCC_UFS_PHY_UNIPRO_CORE_CLK				145
#define GCC_UFS_PHY_UNIPRO_CORE_CLK_SRC				146
#define GCC_USB30_PRIM_MASTER_CLK				147
#define GCC_USB30_PRIM_MASTER_CLK_SRC				148
#define GCC_USB30_PRIM_MOCK_UTMI_CLK				149
#define GCC_USB30_PRIM_MOCK_UTMI_CLK_SRC			150
#define GCC_USB30_PRIM_SLEEP_CLK				151
#define GCC_USB30_SEC_MASTER_CLK				152
#define GCC_USB30_SEC_MASTER_CLK_SRC				153
#define GCC_USB30_SEC_MOCK_UTMI_CLK				154
#define GCC_USB30_SEC_MOCK_UTMI_CLK_SRC				155
#define GCC_USB30_SEC_SLEEP_CLK					156
#define GCC_USB3_PRIM_CLKREF_CLK				157
#define GCC_USB3_PRIM_PHY_AUX_CLK				158
#define GCC_USB3_PRIM_PHY_AUX_CLK_SRC				159
#define GCC_USB3_PRIM_PHY_COM_AUX_CLK				160
#define GCC_USB3_PRIM_PHY_PIPE_CLK				161
#define GCC_USB3_SEC_CLKREF_CLK					162
#define GCC_USB3_SEC_PHY_AUX_CLK				163
#define GCC_USB3_SEC_PHY_AUX_CLK_SRC				164
#define GCC_USB3_SEC_PHY_COM_AUX_CLK				165
#define GCC_USB3_SEC_PHY_PIPE_CLK				166
#define GCC_USB_PHY_CFG_AHB2PHY_CLK				167
#define GCC_VIDEO_AHB_CLK					168
#define GCC_VIDEO_AXI_CLK					169
#define GCC_VIDEO_XO_CLK					170
#define GPLL0							171
#define GPLL0_OUT_EVEN						172
#define GPLL0_OUT_MAIN						173
#define GPLL1							174
#define GPLL1_OUT_MAIN						175

/* GCC reset clocks */
#define GCC_GPU_BCR						0
#define GCC_MMSS_BCR						1
#define GCC_PCIE_0_BCR						2
#define GCC_PCIE_1_BCR						3
#define GCC_PCIE_PHY_BCR					4
#define GCC_PDM_BCR						5
#define GCC_PRNG_BCR						6
#define GCC_QUPV3_WRAPPER_0_BCR					7
#define GCC_QUPV3_WRAPPER_1_BCR					8
#define GCC_QUSB2PHY_PRIM_BCR					9
#define GCC_QUSB2PHY_SEC_BCR					10
#define GCC_SDCC2_BCR						11
#define GCC_SDCC4_BCR						12
#define GCC_TSIF_BCR						13
#define GCC_UFS_CARD_BCR					14
#define GCC_UFS_PHY_BCR						15
#define GCC_USB30_PRIM_BCR					16
#define GCC_USB30_SEC_BCR					17
#define GCC_USB3_PHY_PRIM_BCR					18
#define GCC_USB3PHY_PHY_PRIM_BCR				19
#define GCC_USB3_DP_PHY_PRIM_BCR				20
#define GCC_USB3_PHY_SEC_BCR					21
#define GCC_USB3PHY_PHY_SEC_BCR					22
#define GCC_USB3_DP_PHY_SEC_BCR					23
#define GCC_USB_PHY_CFG_AHB2PHY_BCR				24

/* Dummy clocks for rate measurement */
#define MEASURE_ONLY_SNOC_CLK					0
#define MEASURE_ONLY_CNOC_CLK					1
#define MEASURE_ONLY_BIMC_CLK					2
#define MEASURE_ONLY_IPA_2X_CLK					3

#endif
