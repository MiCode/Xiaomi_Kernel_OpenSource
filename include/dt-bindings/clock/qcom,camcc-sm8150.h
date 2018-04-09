/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#ifndef _DT_BINDINGS_CLK_QCOM_CAM_CC_SM8150_H
#define _DT_BINDINGS_CLK_QCOM_CAM_CC_SM8150_H

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
#define CAM_CC_CORE_AHB_CLK					12
#define CAM_CC_CPAS_AHB_CLK					13
#define CAM_CC_CPHY_RX_CLK_SRC					14
#define CAM_CC_CSI0PHYTIMER_CLK					15
#define CAM_CC_CSI0PHYTIMER_CLK_SRC				16
#define CAM_CC_CSI1PHYTIMER_CLK					17
#define CAM_CC_CSI1PHYTIMER_CLK_SRC				18
#define CAM_CC_CSI2PHYTIMER_CLK					19
#define CAM_CC_CSI2PHYTIMER_CLK_SRC				20
#define CAM_CC_CSI3PHYTIMER_CLK					21
#define CAM_CC_CSI3PHYTIMER_CLK_SRC				22
#define CAM_CC_CSIPHY0_CLK					23
#define CAM_CC_CSIPHY1_CLK					24
#define CAM_CC_CSIPHY2_CLK					25
#define CAM_CC_CSIPHY3_CLK					26
#define CAM_CC_FAST_AHB_CLK_SRC					27
#define CAM_CC_FD_CORE_CLK					28
#define CAM_CC_FD_CORE_CLK_SRC					29
#define CAM_CC_FD_CORE_UAR_CLK					30
#define CAM_CC_GDSC_CLK						31
#define CAM_CC_ICP_AHB_CLK					32
#define CAM_CC_ICP_CLK						33
#define CAM_CC_ICP_CLK_SRC					34
#define CAM_CC_IFE_0_AXI_CLK					35
#define CAM_CC_IFE_0_CLK					36
#define CAM_CC_IFE_0_CLK_SRC					37
#define CAM_CC_IFE_0_CPHY_RX_CLK				38
#define CAM_CC_IFE_0_CSID_CLK					39
#define CAM_CC_IFE_0_CSID_CLK_SRC				40
#define CAM_CC_IFE_0_DSP_CLK					41
#define CAM_CC_IFE_1_AXI_CLK					42
#define CAM_CC_IFE_1_CLK					43
#define CAM_CC_IFE_1_CLK_SRC					44
#define CAM_CC_IFE_1_CPHY_RX_CLK				45
#define CAM_CC_IFE_1_CSID_CLK					46
#define CAM_CC_IFE_1_CSID_CLK_SRC				47
#define CAM_CC_IFE_1_DSP_CLK					48
#define CAM_CC_IFE_LITE_0_CLK					49
#define CAM_CC_IFE_LITE_0_CLK_SRC				50
#define CAM_CC_IFE_LITE_0_CPHY_RX_CLK				51
#define CAM_CC_IFE_LITE_0_CSID_CLK				52
#define CAM_CC_IFE_LITE_0_CSID_CLK_SRC				53
#define CAM_CC_IFE_LITE_1_CLK					54
#define CAM_CC_IFE_LITE_1_CLK_SRC				55
#define CAM_CC_IFE_LITE_1_CPHY_RX_CLK				56
#define CAM_CC_IFE_LITE_1_CSID_CLK				57
#define CAM_CC_IFE_LITE_1_CSID_CLK_SRC				58
#define CAM_CC_IPE_0_AHB_CLK					59
#define CAM_CC_IPE_0_AREG_CLK					60
#define CAM_CC_IPE_0_AXI_CLK					61
#define CAM_CC_IPE_0_CLK					62
#define CAM_CC_IPE_0_CLK_SRC					63
#define CAM_CC_IPE_1_AHB_CLK					64
#define CAM_CC_IPE_1_AREG_CLK					65
#define CAM_CC_IPE_1_AXI_CLK					66
#define CAM_CC_IPE_1_CLK					67
#define CAM_CC_JPEG_CLK						68
#define CAM_CC_JPEG_CLK_SRC					69
#define CAM_CC_LRME_CLK						70
#define CAM_CC_LRME_CLK_SRC					71
#define CAM_CC_MCLK0_CLK					72
#define CAM_CC_MCLK0_CLK_SRC					73
#define CAM_CC_MCLK1_CLK					74
#define CAM_CC_MCLK1_CLK_SRC					75
#define CAM_CC_MCLK2_CLK					76
#define CAM_CC_MCLK2_CLK_SRC					77
#define CAM_CC_MCLK3_CLK					78
#define CAM_CC_MCLK3_CLK_SRC					79
#define CAM_CC_PLL0						80
#define CAM_CC_PLL0_OUT_EVEN					81
#define CAM_CC_PLL0_OUT_ODD					82
#define CAM_CC_PLL1						83
#define CAM_CC_PLL1_OUT_EVEN					84
#define CAM_CC_PLL2						85
#define CAM_CC_PLL2_OUT_MAIN					86
#define CAM_CC_PLL3						87
#define CAM_CC_PLL3_OUT_EVEN					88
#define CAM_CC_PLL4						89
#define CAM_CC_PLL4_OUT_EVEN					90
#define CAM_CC_PLL_TEST_CLK					91
#define CAM_CC_QDSS_DEBUG_CLK					92
#define CAM_CC_QDSS_DEBUG_CLK_SRC				93
#define CAM_CC_QDSS_DEBUG_XO_CLK				94
#define CAM_CC_SLOW_AHB_CLK_SRC					95

#endif
