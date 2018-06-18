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

#ifndef _DT_BINDINGS_CLK_QCOM_CAM_CC_SDMMAGPIE_H
#define _DT_BINDINGS_CLK_QCOM_CAM_CC_SDMMAGPIE_H

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
#define CAM_CC_IFE_LITE_CLK					49
#define CAM_CC_IFE_LITE_CLK_SRC					50
#define CAM_CC_IFE_LITE_CPHY_RX_CLK				51
#define CAM_CC_IFE_LITE_CSID_CLK				52
#define CAM_CC_IFE_LITE_CSID_CLK_SRC				53
#define CAM_CC_IPE_0_AHB_CLK					54
#define CAM_CC_IPE_0_AREG_CLK					55
#define CAM_CC_IPE_0_AXI_CLK					56
#define CAM_CC_IPE_0_CLK					57
#define CAM_CC_IPE_0_CLK_SRC					58
#define CAM_CC_IPE_1_AHB_CLK					59
#define CAM_CC_IPE_1_AREG_CLK					60
#define CAM_CC_IPE_1_AXI_CLK					61
#define CAM_CC_IPE_1_CLK					62
#define CAM_CC_JPEG_CLK						63
#define CAM_CC_JPEG_CLK_SRC					64
#define CAM_CC_LRME_CLK						65
#define CAM_CC_LRME_CLK_SRC					66
#define CAM_CC_MCLK0_CLK					67
#define CAM_CC_MCLK0_CLK_SRC					68
#define CAM_CC_MCLK1_CLK					69
#define CAM_CC_MCLK1_CLK_SRC					70
#define CAM_CC_MCLK2_CLK					71
#define CAM_CC_MCLK2_CLK_SRC					72
#define CAM_CC_MCLK3_CLK					73
#define CAM_CC_MCLK3_CLK_SRC					74
#define CAM_CC_PLL0						75
#define CAM_CC_PLL0_OUT_EVEN					76
#define CAM_CC_PLL0_OUT_ODD					77
#define CAM_CC_PLL1						78
#define CAM_CC_PLL1_OUT_EVEN					79
#define CAM_CC_PLL2						80
#define CAM_CC_PLL2_OUT_AUX					81
#define CAM_CC_PLL2_OUT_MAIN					82
#define CAM_CC_PLL3						83
#define CAM_CC_PLL3_OUT_EVEN					84
#define CAM_CC_PLL4						85
#define CAM_CC_PLL4_OUT_EVEN					86
#define CAM_CC_PLL_TEST_CLK					87
#define CAM_CC_QDSS_DEBUG_CLK					88
#define CAM_CC_QDSS_DEBUG_CLK_SRC				89
#define CAM_CC_QDSS_DEBUG_XO_CLK				90
#define CAM_CC_SLEEP_CLK					91
#define CAM_CC_SLEEP_CLK_SRC					92
#define CAM_CC_SLOW_AHB_CLK_SRC					93
#define CAM_CC_SPDM_BPS_CLK					94
#define CAM_CC_SPDM_IFE_0_CLK					95
#define CAM_CC_SPDM_IFE_0_CSID_CLK				96
#define CAM_CC_SPDM_IPE_0_CLK					97
#define CAM_CC_SPDM_IPE_1_CLK					98
#define CAM_CC_SPDM_JPEG_CLK					99
#define CAM_CC_XO_CLK_SRC					100

#define BPS_GDSC						0
#define IFE_0_GDSC						1
#define IFE_1_GDSC						2
#define IPE_0_GDSC						3
#define IPE_1_GDSC						4
#define TITAN_TOP_GDSC						5

#endif
