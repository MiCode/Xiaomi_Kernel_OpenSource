/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef _DT_BINDINGS_CLK_QCOM_CAM_CC_SDMSHRIKE_H
#define _DT_BINDINGS_CLK_QCOM_CAM_CC_SDMSHRIKE_H

#define CAM_CC_BPS_AHB_CLK					0
#define CAM_CC_BPS_AREG_CLK					1
#define CAM_CC_BPS_AXI_CLK					2
#define CAM_CC_BPS_CLK						3
#define CAM_CC_BPS_CLK_SRC					4
#define CAM_CC_CAMNOC_AXI_CLK					5
#define CAM_CC_CAMNOC_AXI_CLK_SRC				6
#define CAM_CC_CAMNOC_DCD_XO_CLK				7
#define CAM_CC_CCI_0_CLK					8
#define CAM_CC_CCI_0_CLK_SRC					9
#define CAM_CC_CCI_1_CLK					10
#define CAM_CC_CCI_1_CLK_SRC					11
#define CAM_CC_CCI_2_CLK					12
#define CAM_CC_CCI_2_CLK_SRC					13
#define CAM_CC_CCI_3_CLK					14
#define CAM_CC_CCI_3_CLK_SRC					15
#define CAM_CC_CORE_AHB_CLK					16
#define CAM_CC_CPAS_AHB_CLK					17
#define CAM_CC_CPHY_RX_CLK_SRC					18
#define CAM_CC_CSI0PHYTIMER_CLK					19
#define CAM_CC_CSI0PHYTIMER_CLK_SRC				20
#define CAM_CC_CSI1PHYTIMER_CLK					21
#define CAM_CC_CSI1PHYTIMER_CLK_SRC				22
#define CAM_CC_CSI2PHYTIMER_CLK					23
#define CAM_CC_CSI2PHYTIMER_CLK_SRC				24
#define CAM_CC_CSI3PHYTIMER_CLK					25
#define CAM_CC_CSI3PHYTIMER_CLK_SRC				26
#define CAM_CC_CSIPHY0_CLK					27
#define CAM_CC_CSIPHY1_CLK					28
#define CAM_CC_CSIPHY2_CLK					29
#define CAM_CC_CSIPHY3_CLK					30
#define CAM_CC_FAST_AHB_CLK_SRC					31
#define CAM_CC_FD_CORE_CLK					32
#define CAM_CC_FD_CORE_CLK_SRC					33
#define CAM_CC_FD_CORE_UAR_CLK					34
#define CAM_CC_GDSC_CLK						35
#define CAM_CC_ICP_AHB_CLK					36
#define CAM_CC_ICP_CLK						37
#define CAM_CC_ICP_CLK_SRC					38
#define CAM_CC_IFE_0_AXI_CLK					39
#define CAM_CC_IFE_0_CLK					40
#define CAM_CC_IFE_0_CLK_SRC					41
#define CAM_CC_IFE_0_CPHY_RX_CLK				42
#define CAM_CC_IFE_0_CSID_CLK					43
#define CAM_CC_IFE_0_CSID_CLK_SRC				44
#define CAM_CC_IFE_0_DSP_CLK					45
#define CAM_CC_IFE_1_AXI_CLK					46
#define CAM_CC_IFE_1_CLK					47
#define CAM_CC_IFE_1_CLK_SRC					48
#define CAM_CC_IFE_1_CPHY_RX_CLK				49
#define CAM_CC_IFE_1_CSID_CLK					50
#define CAM_CC_IFE_1_CSID_CLK_SRC				51
#define CAM_CC_IFE_1_DSP_CLK					52
#define CAM_CC_IFE_2_AXI_CLK					53
#define CAM_CC_IFE_2_CLK					54
#define CAM_CC_IFE_2_CLK_SRC					55
#define CAM_CC_IFE_2_CPHY_RX_CLK				56
#define CAM_CC_IFE_2_CSID_CLK					57
#define CAM_CC_IFE_2_CSID_CLK_SRC				58
#define CAM_CC_IFE_2_DSP_CLK					59
#define CAM_CC_IFE_3_AXI_CLK					60
#define CAM_CC_IFE_3_CLK					61
#define CAM_CC_IFE_3_CLK_SRC					62
#define CAM_CC_IFE_3_CPHY_RX_CLK				63
#define CAM_CC_IFE_3_CSID_CLK					64
#define CAM_CC_IFE_3_CSID_CLK_SRC				65
#define CAM_CC_IFE_3_DSP_CLK					66
#define CAM_CC_IFE_LITE_0_CLK					67
#define CAM_CC_IFE_LITE_0_CLK_SRC				68
#define CAM_CC_IFE_LITE_0_CPHY_RX_CLK				69
#define CAM_CC_IFE_LITE_0_CSID_CLK				70
#define CAM_CC_IFE_LITE_0_CSID_CLK_SRC				71
#define CAM_CC_IFE_LITE_1_CLK					72
#define CAM_CC_IFE_LITE_1_CLK_SRC				73
#define CAM_CC_IFE_LITE_1_CPHY_RX_CLK				74
#define CAM_CC_IFE_LITE_1_CSID_CLK				75
#define CAM_CC_IFE_LITE_1_CSID_CLK_SRC				76
#define CAM_CC_IFE_LITE_2_CLK					77
#define CAM_CC_IFE_LITE_2_CLK_SRC				78
#define CAM_CC_IFE_LITE_2_CPHY_RX_CLK				79
#define CAM_CC_IFE_LITE_2_CSID_CLK				80
#define CAM_CC_IFE_LITE_2_CSID_CLK_SRC				81
#define CAM_CC_IFE_LITE_3_CLK					82
#define CAM_CC_IFE_LITE_3_CLK_SRC				83
#define CAM_CC_IFE_LITE_3_CPHY_RX_CLK				84
#define CAM_CC_IFE_LITE_3_CSID_CLK				85
#define CAM_CC_IFE_LITE_3_CSID_CLK_SRC				86
#define CAM_CC_IPE_0_AHB_CLK					87
#define CAM_CC_IPE_0_AREG_CLK					88
#define CAM_CC_IPE_0_AXI_CLK					89
#define CAM_CC_IPE_0_CLK					90
#define CAM_CC_IPE_0_CLK_SRC					91
#define CAM_CC_IPE_1_AHB_CLK					92
#define CAM_CC_IPE_1_AREG_CLK					93
#define CAM_CC_IPE_1_AXI_CLK					94
#define CAM_CC_IPE_1_CLK					95
#define CAM_CC_JPEG_CLK						96
#define CAM_CC_JPEG_CLK_SRC					97
#define CAM_CC_LRME_CLK						98
#define CAM_CC_LRME_CLK_SRC					99
#define CAM_CC_MCLK0_CLK					100
#define CAM_CC_MCLK0_CLK_SRC					101
#define CAM_CC_MCLK1_CLK					102
#define CAM_CC_MCLK1_CLK_SRC					103
#define CAM_CC_MCLK2_CLK					104
#define CAM_CC_MCLK2_CLK_SRC					105
#define CAM_CC_MCLK3_CLK					106
#define CAM_CC_MCLK3_CLK_SRC					107
#define CAM_CC_MCLK4_CLK					108
#define CAM_CC_MCLK4_CLK_SRC					109
#define CAM_CC_MCLK5_CLK					110
#define CAM_CC_MCLK5_CLK_SRC					111
#define CAM_CC_MCLK6_CLK					112
#define CAM_CC_MCLK6_CLK_SRC					113
#define CAM_CC_MCLK7_CLK					114
#define CAM_CC_MCLK7_CLK_SRC					115
#define CAM_CC_PLL0						116
#define CAM_CC_PLL0_OUT_EVEN					117
#define CAM_CC_PLL0_OUT_ODD					118
#define CAM_CC_PLL1						119
#define CAM_CC_PLL1_OUT_EVEN					120
#define CAM_CC_PLL2						121
#define CAM_CC_PLL2_OUT_MAIN					122
#define CAM_CC_PLL3						123
#define CAM_CC_PLL3_OUT_EVEN					124
#define CAM_CC_PLL4						125
#define CAM_CC_PLL4_OUT_EVEN					126
#define CAM_CC_PLL5						127
#define CAM_CC_PLL5_OUT_EVEN					128
#define CAM_CC_PLL6						129
#define CAM_CC_PLL6_OUT_EVEN					130
#define CAM_CC_QDSS_DEBUG_CLK					131
#define CAM_CC_QDSS_DEBUG_CLK_SRC				132
#define CAM_CC_QDSS_DEBUG_XO_CLK				133
#define CAM_CC_SLOW_AHB_CLK_SRC					134

#define BPS_GDSC						0
#define IFE_0_GDSC						1
#define IFE_1_GDSC						2
#define IFE_2_GDSC						3
#define IFE_3_GDSC						4
#define IPE_0_GDSC						5
#define IPE_1_GDSC						6
#define TITAN_TOP_GDSC						7

#define CAM_CC_BPS_BCR						0
#define CAM_CC_CAMNOC_BCR					1
#define CAM_CC_CCI_BCR						2
#define CAM_CC_CPAS_BCR						3
#define CAM_CC_CSI0PHY_BCR					4
#define CAM_CC_CSI1PHY_BCR					5
#define CAM_CC_CSI2PHY_BCR					6
#define CAM_CC_CSI3PHY_BCR					7
#define CAM_CC_FD_BCR						8
#define CAM_CC_ICP_BCR						9
#define CAM_CC_IFE_0_BCR					10
#define CAM_CC_IFE_1_BCR					11
#define CAM_CC_IFE_2_BCR					12
#define CAM_CC_IFE_3_BCR					13
#define CAM_CC_IFE_LITE_0_BCR					14
#define CAM_CC_IFE_LITE_1_BCR					15
#define CAM_CC_IFE_LITE_2_BCR					16
#define CAM_CC_IFE_LITE_3_BCR					17
#define CAM_CC_IPE_0_BCR					18
#define CAM_CC_IPE_1_BCR					19
#define CAM_CC_JPEG_BCR						20
#define CAM_CC_LRME_BCR						21
#define CAM_CC_MCLK0_BCR					22
#define CAM_CC_MCLK1_BCR					23
#define CAM_CC_MCLK2_BCR					24
#define CAM_CC_MCLK3_BCR					25
#define CAM_CC_MCLK4_BCR					26
#define CAM_CC_MCLK5_BCR					27
#define CAM_CC_MCLK6_BCR					28
#define CAM_CC_MCLK7_BCR					29
#define CAM_CC_QDSS_DEBUG_BCR					30

#endif
