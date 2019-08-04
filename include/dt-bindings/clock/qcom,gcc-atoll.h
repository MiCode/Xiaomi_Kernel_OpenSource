/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#ifndef _DT_BINDINGS_CLK_QCOM_GCC_ATOLL_H
#define _DT_BINDINGS_CLK_QCOM_GCC_ATOLL_H

/* Dummy clocks for rate measurement */
#define MEASURE_ONLY_BIMC_CLK					0
#define MEASURE_ONLY_CNOC_CLK					1
#define MEASURE_ONLY_IPA_2X_CLK					2
#define MEASURE_ONLY_SNOC_CLK					3
#define GCC_GPLL0_MAIN_DIV_CDIV					4

/* GCC clocks */
#define GPLL0							5
#define GPLL0_OUT_EVEN						6
#define GPLL6							7
#define GPLL7							8
#define GCC_AGGRE_UFS_PHY_AXI_CLK				9
#define GCC_AGGRE_UFS_PHY_AXI_HW_CTL_CLK			10
#define GCC_AGGRE_USB3_PRIM_AXI_CLK				11
#define GCC_BOOT_ROM_AHB_CLK					12
#define GCC_CAMERA_AHB_CLK					13
#define GCC_CAMERA_HF_AXI_CLK					14
#define GCC_CAMERA_THROTTLE_HF_AXI_CLK				15
#define GCC_CAMERA_XO_CLK					16
#define GCC_CE1_AHB_CLK						17
#define GCC_CE1_AXI_CLK						18
#define GCC_CE1_CLK						19
#define GCC_CFG_NOC_USB3_PRIM_AXI_CLK				20
#define GCC_CPUSS_AHB_CLK					21
#define GCC_CPUSS_AHB_CLK_SRC					22
#define GCC_CPUSS_AHB_POSTDIV_CLK_SRC				23
#define GCC_CPUSS_GNOC_CLK					24
#define GCC_CPUSS_RBCPR_CLK					25
#define GCC_DDRSS_GPU_AXI_CLK					26
#define GCC_DISP_AHB_CLK					27
#define GCC_DISP_GPLL0_CLK_SRC					28
#define GCC_DISP_GPLL0_DIV_CLK_SRC				29
#define GCC_DISP_HF_AXI_CLK					30
#define GCC_DISP_THROTTLE_HF_AXI_CLK				31
#define GCC_DISP_XO_CLK						32
#define GCC_GP1_CLK						33
#define GCC_GP1_CLK_SRC						34
#define GCC_GP2_CLK						35
#define GCC_GP2_CLK_SRC						36
#define GCC_GP3_CLK						37
#define GCC_GP3_CLK_SRC						38
#define GCC_GPU_CFG_AHB_CLK					39
#define GCC_GPU_GPLL0_CLK_SRC					40
#define GCC_GPU_GPLL0_DIV_CLK_SRC				41
#define GCC_GPU_MEMNOC_GFX_CLK					42
#define GCC_GPU_SNOC_DVM_GFX_CLK				43
#define GCC_NPU_AXI_CLK						44
#define GCC_NPU_BWMON_AXI_CLK					45
#define GCC_NPU_BWMON_DMA_CFG_AHB_CLK				46
#define GCC_NPU_BWMON_DSP_CFG_AHB_CLK				47
#define GCC_NPU_CFG_AHB_CLK					48
#define GCC_NPU_DMA_CLK						49
#define GCC_NPU_GPLL0_CLK_SRC					50
#define GCC_NPU_GPLL0_DIV_CLK_SRC				51
#define GCC_PDM2_CLK						52
#define GCC_PDM2_CLK_SRC					53
#define GCC_PDM_AHB_CLK						54
#define GCC_PDM_XO4_CLK						55
#define GCC_PRNG_AHB_CLK					56
#define GCC_QSPI_CNOC_PERIPH_AHB_CLK				57
#define GCC_QSPI_CORE_CLK					58
#define GCC_QSPI_CORE_CLK_SRC					59
#define GCC_QUPV3_WRAP0_CORE_2X_CLK				60
#define GCC_QUPV3_WRAP0_CORE_CLK				61
#define GCC_QUPV3_WRAP0_S0_CLK					62
#define GCC_QUPV3_WRAP0_S0_CLK_SRC				63
#define GCC_QUPV3_WRAP0_S1_CLK					64
#define GCC_QUPV3_WRAP0_S1_CLK_SRC				65
#define GCC_QUPV3_WRAP0_S2_CLK					66
#define GCC_QUPV3_WRAP0_S2_CLK_SRC				67
#define GCC_QUPV3_WRAP0_S3_CLK					68
#define GCC_QUPV3_WRAP0_S3_CLK_SRC				69
#define GCC_QUPV3_WRAP0_S4_CLK					70
#define GCC_QUPV3_WRAP0_S4_CLK_SRC				71
#define GCC_QUPV3_WRAP0_S5_CLK					72
#define GCC_QUPV3_WRAP0_S5_CLK_SRC				73
#define GCC_QUPV3_WRAP1_CORE_2X_CLK				74
#define GCC_QUPV3_WRAP1_CORE_CLK				75
#define GCC_QUPV3_WRAP1_S0_CLK					76
#define GCC_QUPV3_WRAP1_S0_CLK_SRC				77
#define GCC_QUPV3_WRAP1_S1_CLK					78
#define GCC_QUPV3_WRAP1_S1_CLK_SRC				79
#define GCC_QUPV3_WRAP1_S2_CLK					80
#define GCC_QUPV3_WRAP1_S2_CLK_SRC				81
#define GCC_QUPV3_WRAP1_S3_CLK					82
#define GCC_QUPV3_WRAP1_S3_CLK_SRC				83
#define GCC_QUPV3_WRAP1_S4_CLK					84
#define GCC_QUPV3_WRAP1_S4_CLK_SRC				85
#define GCC_QUPV3_WRAP1_S5_CLK					86
#define GCC_QUPV3_WRAP1_S5_CLK_SRC				87
#define GCC_QUPV3_WRAP_0_M_AHB_CLK				88
#define GCC_QUPV3_WRAP_0_S_AHB_CLK				89
#define GCC_QUPV3_WRAP_1_M_AHB_CLK				90
#define GCC_QUPV3_WRAP_1_S_AHB_CLK				91
#define GCC_SDCC1_AHB_CLK					92
#define GCC_SDCC1_APPS_CLK					93
#define GCC_SDCC1_APPS_CLK_SRC					94
#define GCC_SDCC1_ICE_CORE_CLK					95
#define GCC_SDCC1_ICE_CORE_CLK_SRC				96
#define GCC_SDCC2_AHB_CLK					97
#define GCC_SDCC2_APPS_CLK					98
#define GCC_SDCC2_APPS_CLK_SRC					99
#define GCC_SYS_NOC_CPUSS_AHB_CLK				100
#define GCC_UFS_MEM_CLKREF_CLK					101
#define GCC_UFS_PHY_AHB_CLK					102
#define GCC_UFS_PHY_AXI_CLK					103
#define GCC_UFS_PHY_AXI_CLK_SRC					104
#define GCC_UFS_PHY_AXI_HW_CTL_CLK				105
#define GCC_UFS_PHY_ICE_CORE_CLK				106
#define GCC_UFS_PHY_ICE_CORE_CLK_SRC				107
#define GCC_UFS_PHY_ICE_CORE_HW_CTL_CLK				108
#define GCC_UFS_PHY_PHY_AUX_CLK					109
#define GCC_UFS_PHY_PHY_AUX_CLK_SRC				110
#define GCC_UFS_PHY_PHY_AUX_HW_CTL_CLK				111
#define GCC_UFS_PHY_RX_SYMBOL_0_CLK				112
#define GCC_UFS_PHY_TX_SYMBOL_0_CLK				113
#define GCC_UFS_PHY_UNIPRO_CORE_CLK				114
#define GCC_UFS_PHY_UNIPRO_CORE_CLK_SRC				115
#define GCC_UFS_PHY_UNIPRO_CORE_HW_CTL_CLK			116
#define GCC_USB30_PRIM_MASTER_CLK				117
#define GCC_USB30_PRIM_MASTER_CLK_SRC				118
#define GCC_USB30_PRIM_MOCK_UTMI_CLK				119
#define GCC_USB30_PRIM_MOCK_UTMI_CLK_SRC			120
#define GCC_USB30_PRIM_MOCK_UTMI_POSTDIV_CLK_SRC		121
#define GCC_USB30_PRIM_SLEEP_CLK				122
#define GCC_USB3_PRIM_CLKREF_CLK				123
#define GCC_USB3_PRIM_PHY_AUX_CLK				124
#define GCC_USB3_PRIM_PHY_AUX_CLK_SRC				125
#define GCC_USB3_PRIM_PHY_COM_AUX_CLK				126
#define GCC_USB3_PRIM_PHY_PIPE_CLK				127
#define GCC_USB_PHY_CFG_AHB2PHY_CLK				128
#define GCC_VIDEO_AHB_CLK					129
#define GCC_VIDEO_AXI_CLK					130
#define GCC_VIDEO_GPLL0_DIV_CLK_SRC				131
#define GCC_VIDEO_THROTTLE_AXI_CLK				132
#define GCC_VIDEO_XO_CLK					133

/* GCC resets */
#define GCC_QUSB2PHY_PRIM_BCR					0
#define GCC_QUSB2PHY_SEC_BCR					1
#define GCC_UFS_PHY_BCR						2
#define GCC_USB30_PRIM_BCR					3
#define GCC_USB3_DP_PHY_PRIM_BCR				4
#define GCC_USB3_DP_PHY_SEC_BCR					5
#define GCC_USB3_PHY_PRIM_BCR					6
#define GCC_USB3_PHY_SEC_BCR					7
#define GCC_USB3PHY_PHY_PRIM_BCR				8
#define GCC_USB3PHY_PHY_SEC_BCR					9
#define GCC_USB_PHY_CFG_AHB2PHY_BCR				10

#endif
