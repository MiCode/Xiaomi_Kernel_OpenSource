// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk-mtk.h"
#include "clk-mux.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6983-clk.h>

/* bringup config */
#define MT_CCF_BRINGUP		1
#define MT_CCF_PLL_DISABLE	0

/* Regular Number Definition */

#define AP_PLL_CON3				0x000C
#define APLL1_TUNER_CON0	0x040
#define APLL2_TUNER_CON0	0x044
#define ARMPLL_LL_CON0				0x0208
#define ARMPLL_LL_CON1				0x020c
#define ARMPLL_LL_CON3				0x0214
#define ARMPLL_BL_CON0				0x0218
#define ARMPLL_BL_CON1				0x021c
#define ARMPLL_BL_CON3				0x0224
#define TVDPLL_CON0				0x0248
#define TVDPLL_CON1				0x024c
#define TVDPLL_CON3				0x0254
#define UNIVPLL_CON0				0x0308
#define UNIVPLL_CON1				0x030c
#define UNIVPLL_CON3				0x0314
#define USBPLL_CON0				0x0318
#define USBPLL_CON1				0x031C
#define USBPLL_CON3				0x0324
#define APLL1_CON0				0x0328
#define APLL1_CON1				0x032c
#define APLL1_CON2				0x0330
#define APLL1_CON4				0x0338
#define APLL2_CON0				0x033c
#define APLL2_CON1				0x0340
#define APLL2_CON2				0x0344
#define APLL2_CON4				0x034c
#define MAINPLL_CON0				0x0350
#define MAINPLL_CON1				0x0354
#define MAINPLL_CON3				0x035c
#define MSDCPLL_CON0				0x0360
#define MSDCPLL_CON1				0x0364
#define MSDCPLL_CON3				0x036c
#define IMGPLL_CON0				0x0370
#define IMGPLL_CON1				0x0374
#define IMGPLL_CON3				0x037C
#define ADSPPLL_CON0				0x0380
#define ADSPPLL_CON1				0x0384
#define ADSPPLL_CON3				0x038c
#define MPLL_CON0				0x0390
#define MPLL_CON1				0x0394
#define MPLL_CON3				0x039c
#define MMPLL_CON0				0x03A0
#define MMPLL_CON1				0x03A4
#define MMPLL_CON3				0x03AC
#define EMIPLL_CON0				0x03B0
#define EMIPLL_CON1				0x03B4
#define EMIPLL_CON3				0x03BC

#define MFGPLL_CON0				0x008
#define MFGPLL_CON1				0x00C
#define MFGPLL_CON2				0x010
#define MFGPLL_CON3				0x014

#define MFGSCPLL_CON0				0xC08
#define MFGSCPLL_CON1				0xC0C
#define MFGSCPLL_CON2				0xC10
#define MFGSCPLL_CON3				0xC14

#define APUPLL_CON0				0x008
#define APUPLL_CON1				0x00C
#define APUPLL_CON2				0x010
#define APUPLL_CON3				0x014

#define NPUPLL_CON0				0x408
#define NPUPLL_CON1				0x40C
#define NPUPLL_CON2				0x410
#define NPUPLL_CON3				0x414

#define APUPLL1_CON0				0x808
#define APUPLL1_CON1				0x80C
#define APUPLL1_CON2				0x810
#define APUPLL1_CON3				0x814

#define APUPLL2_CON0				0xC08
#define APUPLL2_CON1				0xC0C
#define APUPLL2_CON2				0xC10
#define APUPLL2_CON3				0xC14

#define VLP_CLK_CFG_UPDATE			0x0004
#define VLP_CLK_CFG_0				0x0008
#define VLP_CLK_CFG_0_SET			0x000C
#define VLP_CLK_CFG_0_CLR			0x0010
#define VLP_CLK_CFG_1				0x0014
#define VLP_CLK_CFG_1_SET			0x0018
#define VLP_CLK_CFG_1_CLR			0x001C
#define VLP_CLK_CFG_2				0x0020
#define VLP_CLK_CFG_2_SET			0x0024
#define VLP_CLK_CFG_2_CLR			0x0028
#define VLP_CLK_CFG_3				0x002C
#define VLP_CLK_CFG_3_SET			0x0030
#define VLP_CLK_CFG_3_CLR			0x0034

#define CLK_CFG_UPDATE_0			0x0004
#define CLK_CFG_UPDATE_1			0x0008
#define CLK_CFG_UPDATE_2			0x000c
#define CLK_CFG_0				0x0010
#define CLK_CFG_0_SET				0x0014
#define CLK_CFG_0_CLR				0x0018
#define CLK_CFG_1				0x0020
#define CLK_CFG_1_SET				0x0024
#define CLK_CFG_1_CLR				0x0028
#define CLK_CFG_2				0x0030
#define CLK_CFG_2_SET				0x0034
#define CLK_CFG_2_CLR				0x0038
#define CLK_CFG_3				0x0040
#define CLK_CFG_3_SET				0x0044
#define CLK_CFG_3_CLR				0x0048
#define CLK_CFG_4				0x0050
#define CLK_CFG_4_SET				0x0054
#define CLK_CFG_4_CLR				0x0058
#define CLK_CFG_5				0x0060
#define CLK_CFG_5_SET				0x0064
#define CLK_CFG_5_CLR				0x0068
#define CLK_CFG_6				0x0070
#define CLK_CFG_6_SET				0x0074
#define CLK_CFG_6_CLR				0x0078
#define CLK_CFG_7				0x0080
#define CLK_CFG_7_SET				0x0084
#define CLK_CFG_7_CLR				0x0088
#define CLK_CFG_8				0x0090
#define CLK_CFG_8_SET				0x0094
#define CLK_CFG_8_CLR				0x0098
#define CLK_CFG_9				0x00A0
#define CLK_CFG_9_SET				0x00A4
#define CLK_CFG_9_CLR				0x00A8
#define CLK_CFG_10				0x00B0
#define CLK_CFG_10_SET				0x00B4
#define CLK_CFG_10_CLR				0x00B8
#define CLK_CFG_11				0x00C0
#define CLK_CFG_11_SET				0x00C4
#define CLK_CFG_11_CLR				0x00C8
#define CLK_CFG_12				0x00D0
#define CLK_CFG_12_SET				0x00D4
#define CLK_CFG_12_CLR				0x00D8
#define CLK_CFG_13				0x00E0
#define CLK_CFG_13_SET				0x00E4
#define CLK_CFG_13_CLR				0x00E8
#define CLK_CFG_14				0x00F0
#define CLK_CFG_14_SET				0x00F4
#define CLK_CFG_14_CLR				0x00F8
#define CLK_CFG_15				0x0100
#define CLK_CFG_15_SET				0x0104
#define CLK_CFG_15_CLR				0x0108
#define CLK_CFG_16				0x0110
#define CLK_CFG_16_SET				0x0114
#define CLK_CFG_16_CLR				0x0118
#define CLK_CFG_17				0x0120
#define CLK_CFG_17_SET				0x0124
#define CLK_CFG_17_CLR				0x0128
#define CLK_CFG_18				0x0130
#define CLK_CFG_18_SET				0x0134
#define CLK_CFG_18_CLR				0x0138
#define CLK_CFG_19				0x0140
#define CLK_CFG_19_SET				0x0144
#define CLK_CFG_19_CLR				0x0148
#define CLK_CFG_20				0x0150
#define CLK_CFG_20_SET				0x0154
#define CLK_CFG_20_CLR				0x0158
#define CLK_CFG_21				0x0160
#define CLK_CFG_21_SET				0x0164
#define CLK_CFG_21_CLR				0x0168
#define CLK_CFG_22				0x0170
#define CLK_CFG_22_SET				0x0174
#define CLK_CFG_22_CLR				0x0178
#define CLK_CFG_30				0x01F0
#define CLK_CFG_30_SET				0x01F4
#define CLK_CFG_30_CLR				0x01F8
#define CLK_AUDDIV_0				0x0320
#define CLK_AUDDIV_2				0x0328
#define CLK_AUDDIV_3				0x0334
#define CLK_AUDDIV_4				0x0338

#define VLP_CKSYS_MUX_SCP_SHIFT			0
#define VLP_CKSYS_MUX_PWRAP_ULPOSC_SHIFT		1//add
#define VLP_CKSYS_MUX_APXGPT66M_BCLK_SHIFT	2
#define VLP_CKSYS_MUX_DXCC_VLP_SHIFT		3
#define VLP_CKSYS_MUX_SPMI_P_MST_SHIFT		4//add
#define VLP_CKSYS_MUX_SPMI_M_MST_SHIFT		5//add
#define VLP_CKSYS_MUX_DVFSRC_SHIFT		6
#define VLP_CKSYS_MUX_PWM_VLP_SHIFT		7
#define VLP_CKSYS_MUX_AXI_VLP_SHIFT		8
#define VLP_CKSYS_MUX_DBGAO_26M_SHIFT		9
#define VLP_CKSYS_MUX_SYSTIMER_26M_SHIFT	10
#define VLP_CKSYS_MUX_SSPM_SHIFT		11
#define VLP_CKSYS_MUX_SSPM_F26M_SHIFT		12
#define VLP_CKSYS_MUX_APEINT_66M_SHIFT		13
#define VLP_CKSYS_MUX_SRCK_SHIFT		14
#define VLP_CKSYS_MUX_SRAMRC_SHIFT		15

#define TOP_MUX_AXI_SHIFT			0
#define TOP_MUX_PERI_HD_FAXI_SHIFT		1
#define TOP_MUX_UFS_HD_HAXI_SHIFT		2
#define TOP_MUX_BUS_AXIMEM_SHIFT		3
#define TOP_MUX_DISP0_SHIFT			4
#define TOP_MUX_DISP1_SHIFT			5
#define TOP_MUX_MDP0_SHIFT			6
#define TOP_MUX_MDP1_SHIFT			7
#define TOP_MUX_MMINFRA_SHIFT			8
#define TOP_MUX_MMUP_SHIFT			9
#define TOP_MUX_DSP_SHIFT			10
#define TOP_MUX_DSP1_SHIFT			11
#define TOP_MUX_DSP2_SHIFT			12
#define TOP_MUX_DSP3_SHIFT			13
#define TOP_MUX_DSP4_SHIFT			14
#define TOP_MUX_DSP5_SHIFT			15
#define TOP_MUX_DSP6_SHIFT			16
#define TOP_MUX_DSP7_SHIFT			17
#define TOP_MUX_IPU_IF_SHIFT			18
#define TOP_MUX_MFG_REF_SHIFT			19
#define TOP_MUX_MFGSC_REF_SHIFT			20
#define TOP_MUX_CAMTG_SHIFT			21
#define TOP_MUX_CAMTG2_SHIFT			22
#define TOP_MUX_CAMTG3_SHIFT			23
#define TOP_MUX_CAMTG4_SHIFT			24
#define TOP_MUX_CAMTG5_SHIFT			25
#define TOP_MUX_CAMTG6_SHIFT			26
#define TOP_MUX_CAMTG7_SHIFT			27
#define TOP_MUX_CAMTG8_SHIFT			28
#define TOP_MUX_UART_SHIFT			29
#define TOP_MUX_SPI_SHIFT			30
#define TOP_MUX_MSDC50_0_HCLK_SHIFT		0
#define TOP_MUX_MSDC_MACRO_SHIFT		1
#define TOP_MUX_MSDC30_1_SHIFT			2
#define TOP_MUX_MSDC30_2_SHIFT			3
#define TOP_MUX_AUDIO_SHIFT			4
#define TOP_MUX_AUD_INTBUS_SHIFT		5
#define TOP_MUX_PWRAP_ULPOSC_SHIFT		6
#define TOP_MUX_ATB_SHIFT			7
#define TOP_MUX_DP_SHIFT			8
#define TOP_MUX_DISP_PWM_SHIFT			9
#define TOP_MUX_USB_TOP_SHIFT			10
#define TOP_MUX_SSUSB_XHCI_SHIFT		11
#define TOP_MUX_USB_TOP_1P_SHIFT		12
#define TOP_MUX_SSUSB_XHCI_1P_SHIFT		13
#define TOP_MUX_I2C_SHIFT			14
#define TOP_MUX_SENINF_SHIFT			15
#define TOP_MUX_SENINF1_SHIFT			16
#define TOP_MUX_SENINF2_SHIFT			17
#define TOP_MUX_SENINF3_SHIFT			18
#define TOP_MUX_SENINF4_SHIFT			19
#define TOP_MUX_SENINF5_SHIFT			20
#define TOP_MUX_DXCC_SHIFT			21
#define TOP_MUX_AUD_ENGEN1_SHIFT		22
#define TOP_MUX_AUD_ENGEN2_SHIFT		23
#define TOP_MUX_AES_UFSFDE_SHIFT		24
#define TOP_MUX_UFS_SHIFT			25
#define TOP_MUX_UFS_MBIST_SHIFT			26
#define TOP_MUX_PEXTP_MBIST_SHIFT		27
#define TOP_MUX_AUD_1_SHIFT			28
#define TOP_MUX_AUD_2_SHIFT			29
#define TOP_MUX_ADSP_SHIFT			30
#define TOP_MUX_DPMAIF_MAIN_SHIFT		0
#define TOP_MUX_VENC_SHIFT			1
#define TOP_MUX_VDEC_SHIFT			2
#define TOP_MUX_PWM_SHIFT			3
#define TOP_MUX_AUDIO_H_SHIFT			4
#define TOP_MUX_MCUPM_SHIFT			5
#define TOP_MUX_SPMI_P_MST_SHIFT		6
#define TOP_MUX_SPMI_M_MST_SHIFT		7
#define TOP_MUX_TL_SHIFT			8
#define TOP_MUX_MEM_SUB_SHIFT			9
#define TOP_MUX_PERI_HF_FMEM_SHIFT		10
#define TOP_MUX_UFS_HF_FMEM_SHIFT		11
#define TOP_MUX_AES_MSDCFDE_SHIFT		12
#define TOP_MUX_EMI_N_SHIFT			13
#define TOP_MUX_EMI_S_SHIFT			14
#define TOP_MUX_DSI_OCC_SHIFT			15
#define TOP_MUX_DPTX_SHIFT			16
#define TOP_MUX_CCU_AHB_SHIFT			17
#define TOP_MUX_AP2CONN_HOST_SHIFT		18
#define TOP_MUX_IMG1_SHIFT			19
#define TOP_MUX_IPE_SHIFT			20
#define TOP_MUX_CAM_SHIFT			21
#define TOP_MUX_CCUSYS_SHIFT			22
#define TOP_MUX_CAMTM_SHIFT			23
#define TOP_MUX_SFLASH_SHIFT			24
#define TOP_MUX_MCU_ACP_SHIFT			25
#define TOP_MUX_TL_CK_SHIFT			26
#define TOP_MUX_DUMMY_SHIFT			31
#define HWV_PLL_SET				0x190
#define HWV_PLL_CLR				0x194
#define HWV_PLL_EN				0x1400
#define HWV_PLL_DONE				0x140C

static DEFINE_SPINLOCK(mtk_clk_lock);

static const struct mtk_fixed_factor top_divs[] = {
	FACTOR(CLK_TOP_ADSPPLL, "adsppll_ck",
		"adsppll", 1, 1),
	FACTOR(CLK_TOP_APLL1, "apll1_ck",
		"apll1", 1, 1),
	FACTOR(CLK_TOP_APLL1_D2, "apll1_d2",
		"apll1", 1, 2),
	FACTOR(CLK_TOP_APLL1_D4, "apll1_d4",
		"apll1", 1, 4),
	FACTOR(CLK_TOP_APLL1_D8, "apll1_d8",
		"apll1", 1, 8),
	FACTOR(CLK_TOP_APLL2, "apll2_ck",
		"apll2", 1, 1),
	FACTOR(CLK_TOP_APLL2_D2, "apll2_d2",
		"apll2", 1, 2),
	FACTOR(CLK_TOP_APLL2_D4, "apll2_d4",
		"apll2", 1, 4),
	FACTOR(CLK_TOP_APLL2_D8, "apll2_d8",
		"apll2", 1, 8),
	FACTOR(CLK_TOP_IMGPLL_D2, "imgpll_d2",
		"imgpll", 1, 2),
	FACTOR(CLK_TOP_IMGPLL_D5, "imgpll_d5",
		"imgpll", 1, 5),
	FACTOR(CLK_TOP_MAINPLL_D3, "mainpll_d3",
		"mainpll", 1, 3),
	FACTOR(CLK_TOP_MAINPLL_D4, "mainpll_d4",
		"mainpll", 1, 4),
	FACTOR(CLK_TOP_MAINPLL_D4_D16, "mainpll_d4_d16",
		"mainpll", 1, 64),
	FACTOR(CLK_TOP_MAINPLL_D4_D2, "mainpll_d4_d2",
		"mainpll", 1, 8),
	FACTOR(CLK_TOP_MAINPLL_D4_D4, "mainpll_d4_d4",
		"mainpll", 1, 16),
	FACTOR(CLK_TOP_MAINPLL_D4_D8, "mainpll_d4_d8",
		"mainpll", 1, 32),
	FACTOR(CLK_TOP_MAINPLL_D5, "mainpll_d5",
		"mainpll", 1, 5),
	FACTOR(CLK_TOP_MAINPLL_D5_D2, "mainpll_d5_d2",
		"mainpll", 1, 10),
	FACTOR(CLK_TOP_MAINPLL_D5_D4, "mainpll_d5_d4",
		"mainpll", 1, 20),
	FACTOR(CLK_TOP_MAINPLL_D5_D8, "mainpll_d5_d8",
		"mainpll", 1, 40),
	FACTOR(CLK_TOP_MAINPLL_D6, "mainpll_d6",
		"mainpll", 1, 6),
	FACTOR(CLK_TOP_MAINPLL_D6_D2, "mainpll_d6_d2",
		"mainpll", 1, 12),
	FACTOR(CLK_TOP_MAINPLL_D6_D4, "mainpll_d6_d4",
		"mainpll", 1, 24),
	FACTOR(CLK_TOP_MAINPLL_D6_D8, "mainpll_d6_d8",
		"mainpll", 1, 48),
	FACTOR(CLK_TOP_MAINPLL_D7, "mainpll_d7",
		"mainpll", 1, 7),
	FACTOR(CLK_TOP_MAINPLL_D7_D2, "mainpll_d7_d2",
		"mainpll", 1, 14),
	FACTOR(CLK_TOP_MAINPLL_D7_D4, "mainpll_d7_d4",
		"mainpll", 1, 28),
	FACTOR(CLK_TOP_MAINPLL_D7_D8, "mainpll_d7_d8",
		"mainpll", 1, 56),
	FACTOR(CLK_TOP_MAINPLL_D9, "mainpll_d9",
		"mainpll", 1, 9),
	FACTOR(CLK_TOP_MFGPLL, "mfgpll_ck",
		"mfgpll", 1, 1),
	FACTOR(CLK_TOP_MFGSCPLL, "mfgscpll_ck",
		"mfgscpll", 1, 1),
	FACTOR(CLK_TOP_MMPLL_D4, "mmpll_d4",
		"mmpll", 1, 4),
	FACTOR(CLK_TOP_MMPLL_D4_D2, "mmpll_d4_d2",
		"mmpll", 1, 8),
	FACTOR(CLK_TOP_MMPLL_D5, "mmpll_d5",
		"mmpll", 1, 5),
	FACTOR(CLK_TOP_MMPLL_D5_D2, "mmpll_d5_d2",
		"mmpll", 1, 10),
	FACTOR(CLK_TOP_MMPLL_D6, "mmpll_d6",
		"mmpll", 1, 6),
	FACTOR(CLK_TOP_MMPLL_D6_D2, "mmpll_d6_d2",
		"mmpll", 1, 12),
	FACTOR(CLK_TOP_MMPLL_D7, "mmpll_d7",
		"mmpll", 1, 7),
	FACTOR(CLK_TOP_MMPLL_D9, "mmpll_d9",
		"mmpll", 1, 9),
	FACTOR(CLK_TOP_MSDCPLL, "msdcpll_ck",
		"msdcpll", 1, 1),
	FACTOR(CLK_TOP_MSDCPLL_D2, "msdcpll_d2",
		"msdcpll", 1, 2),
	FACTOR(CLK_TOP_TVDPLL, "tvdpll_ck",
		"tvdpll", 1, 1),
	FACTOR(CLK_TOP_TVDPLL_D16, "tvdpll_d16",
		"tvdpll", 1, 16),
	FACTOR(CLK_TOP_TVDPLL_D2, "tvdpll_d2",
		"tvdpll", 1, 2),
	FACTOR(CLK_TOP_TVDPLL_D4, "tvdpll_d4",
		"tvdpll", 1, 4),
	FACTOR(CLK_TOP_TVDPLL_D8, "tvdpll_d8",
		"tvdpll", 1, 8),
	FACTOR(CLK_TOP_UNIVPLL_192M_D10, "univpll_192m_d10",
		"univpll", 1, 130),
	FACTOR(CLK_TOP_UNIVPLL_192M_D16, "univpll_192m_d16",
		"univpll", 1, 208),
	FACTOR(CLK_TOP_UNIVPLL_192M_D2, "univpll_192m_d2",
		"univpll", 1, 26),
	FACTOR(CLK_TOP_UNIVPLL_192M_D32, "univpll_192m_d32",
		"univpll", 1, 416),
	FACTOR(CLK_TOP_UNIVPLL_192M_D8, "univpll_192m_d8",
		"univpll", 1, 104),
	FACTOR(CLK_TOP_UNIVPLL_D3, "univpll_d3",
		"univpll", 1, 3),
	FACTOR(CLK_TOP_UNIVPLL_D4, "univpll_d4",
		"univpll", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL_D4_D2, "univpll_d4_d2",
		"univpll", 1, 8),
	FACTOR(CLK_TOP_UNIVPLL_D4_D4, "univpll_d4_d4",
		"univpll", 1, 16),
	FACTOR(CLK_TOP_UNIVPLL_D4_D8, "univpll_d4_d8",
		"univpll", 1, 32),
	FACTOR(CLK_TOP_UNIVPLL_D5, "univpll_d5",
		"univpll", 1, 5),
	FACTOR(CLK_TOP_UNIVPLL_D5_D2, "univpll_d5_d2",
		"univpll", 1, 10),
	FACTOR(CLK_TOP_UNIVPLL_D5_D4, "univpll_d5_d4",
		"univpll", 1, 20),
	FACTOR(CLK_TOP_UNIVPLL_D6, "univpll_d6",
		"univpll", 1, 6),
	FACTOR(CLK_TOP_UNIVPLL_D6_D16, "univpll_d6_d16",
		"univpll", 1, 96),
	FACTOR(CLK_TOP_UNIVPLL_D6_D2, "univpll_d6_d2",
		"univpll", 1, 12),
	FACTOR(CLK_TOP_UNIVPLL_D6_D4, "univpll_d6_d4",
		"univpll", 1, 24),
	FACTOR(CLK_TOP_UNIVPLL_D6_D8, "univpll_d6_d8",
		"univpll", 1, 48),
	FACTOR(CLK_TOP_UNIVPLL_D7, "univpll_d7",
		"univpll", 1, 7),
	FACTOR(CLK_TOP_AD_OSC, "osc_ck",
		"ulposc", 1, 1),
	FACTOR(CLK_TOP_CLKRTC, "clkrtc",
		"clk32k", 1, 1),
	FACTOR(CLK_TOP_F26M, "f26m_ck",
		"clk26m", 1, 1),
	FACTOR(CLK_TOP_F26M_CK_D2, "f26m_ck_d2",
		"clk13m", 1, 1),
	FACTOR(CLK_TOP_OSC_D10, "osc_d10",
		"ulposc", 1, 10),
	FACTOR(CLK_TOP_OSC_D16, "osc_d16",
		"ulposc", 1, 16),
	FACTOR(CLK_TOP_OSC_D2, "osc_d2",
		"ulposc", 1, 2),
	FACTOR(CLK_TOP_OSC_D4, "osc_d4",
		"ulposc", 1, 4),
	FACTOR(CLK_TOP_OSC_D7, "osc_d7",
		"ulposc", 1, 7),
	FACTOR(CLK_TOP_OSC_D8, "osc_d8",
		"ulposc", 1, 8),
	FACTOR(CLK_TOP_TCK_26M_MX9, "tck_26m_mx9_ck",
		"clk26m", 1, 1),
	FACTOR(CLK_TOP_FADSP, "adsp_ck",
		"adsp_sel", 1, 1),
	FACTOR(CLK_TOP_FAUDIO, "audio_ck",
		"audio_sel", 1, 1),
	FACTOR(CLK_TOP_FAUD_ENGEN1, "aud_engen1_ck",
		"aud_engen1_sel", 1, 1),
	FACTOR(CLK_TOP_FAUD_ENGEN2, "aud_engen2_ck",
		"aud_engen2_sel", 1, 1),
	FACTOR(CLK_TOP_FAUD_1, "aud_1_ck",
		"aud_1_sel", 1, 1),
	FACTOR(CLK_TOP_FAUDIO_H, "audio_h_ck",
		"audio_h_sel", 1, 1),
	FACTOR(CLK_TOP_FDSP, "dsp_ck",
		"dsp_sel", 1, 1),
	FACTOR(CLK_TOP_FDSP4, "dsp4_ck",
		"dsp4_sel", 1, 1),
	FACTOR(CLK_TOP_FDSP7, "dsp7_ck",
		"dsp7_sel", 1, 1),
	FACTOR(CLK_TOP_FIPU_IF, "ipu_if_ck",
		"ipu_if_sel", 1, 1),
	FACTOR(CLK_TOP_FCAM, "cam_ck",
		"cam_sel", 1, 1),
	FACTOR(CLK_TOP_FCAMTM, "fcamtm_ck",
		"camtm_sel", 1, 1),
	FACTOR(CLK_TOP_FCCUSYS, "ccusys_ck",
		"ccusys_sel", 1, 1),
	FACTOR(CLK_TOP_FCCU_AHB, "ccu_ahb_ck",
		"ccu_ahb_sel", 1, 1),
	FACTOR(CLK_TOP_FIMG1, "img1_ck",
		"img1_sel", 1, 1),
	FACTOR(CLK_TOP_FDISP0, "disp0_ck",
		"disp0_sel", 1, 1),
	FACTOR(CLK_TOP_FDISP1, "disp1_ck",
		"disp1_sel", 1, 1),
	FACTOR(CLK_TOP_FI2C, "fi2c_ck",
		"i2c_sel", 1, 1),
	FACTOR(CLK_TOP_FAXI, "axi_ck",
		"axi_sel", 1, 1),
	FACTOR(CLK_TOP_FDXCC, "dxcc_ck",
		"dxcc_sel", 1, 1),
	FACTOR(CLK_TOP_FMSDC_MACRO, "msdc_macro_ck",
		"msdc_macro_sel", 1, 1),
	FACTOR(CLK_TOP_FDPMAIF_MAIN, "dpmaif_main_ck",
		"dpmaif_main_sel", 1, 1),
	FACTOR(CLK_TOP_FMEM_SUB, "mem_sub_ck",
		"mem_sub_sel", 1, 1),
	FACTOR(CLK_TOP_FIPE, "ipe_ck",
		"ipe_sel", 1, 1),
	FACTOR(CLK_TOP_FMDP1, "mdp1_ck",
		"mdp1_sel", 1, 1),
	FACTOR(CLK_TOP_FMDP0, "mdp0_ck",
		"mdp0_sel", 1, 1),
	FACTOR(CLK_TOP_FMFG_REF, "mfg_ref_ck",
		"mfg_ref_sel", 1, 1),
	FACTOR(CLK_TOP_FMMINFRA, "fmminfra_ck",
		"mminfra_sel", 1, 1),
	FACTOR(CLK_TOP_FRTC, "frtc_ck",
		"clk32k", 1, 1),
	FACTOR(CLK_TOP_FVDEC, "vdec_ck",
		"vdec_sel", 1, 1),
	FACTOR(CLK_TOP_FVENC, "venc_ck",
		"venc_sel", 1, 1),
	FACTOR(CLK_TOP_FUART, "fuart_ck",
		"uart_sel", 1, 1),
	FACTOR(CLK_TOP_FPERI_HD_AXI, "fperi_hd_axi_ck",
		"peri_hd_faxi_sel", 1, 1),
	FACTOR(CLK_TOP_FPWM, "pwm_ck",
		"pwm_sel", 1, 1),
	FACTOR(CLK_TOP_FDISP_PWM, "fdisp_pwm_ck",
		"disp_pwm_sel", 1, 1),
	FACTOR(CLK_TOP_FSPI, "spi_ck",
		"spi_sel", 1, 1),
	FACTOR(CLK_TOP_FSFLASH, "sflash_ck",
		"sflash_sel", 1, 1),
	FACTOR(CLK_TOP_FUSB_TOP, "fusb_top_ck",
		"usb_top_sel", 1, 1),
	FACTOR(CLK_TOP_FSSUSB_XHCI, "fssusb_xhci_ck",
		"ssusb_xhci_sel", 1, 1),
	FACTOR(CLK_TOP_FUSB_TOP_1P, "fusb_top_1p_ck",
		"usb_top_1p_sel", 1, 1),
	FACTOR(CLK_TOP_FSSUSB_XHCI_1P, "fssusb_xhci_1p_ck",
		"ssusb_xhci_1p_sel", 1, 1),
	FACTOR(CLK_TOP_FMSDC30_1, "msdc30_1_ck",
		"msdc30_1_sel", 1, 1),
	FACTOR(CLK_TOP_FMSDC30_2, "msdc30_2_ck",
		"msdc30_2_sel", 1, 1),
	FACTOR(CLK_TOP_FTL, "tl_ck",
		"tl_sel", 1, 1),
};

static const char * const apxgpt66m_bclk_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
};

static const char * const dxcc_vlp_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"mainpll_d4_d4",
	"mainpll_d4_d8",
	"osc_d10",
};

static const char * const scp_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4",
	"univpll_d3",
	"mainpll_d3",
	"univpll_d6",
	"apll1_ck",
	"mainpll_d4",
	"mainpll_d7",
	"osc_d10",
};

static const char * const peri_hd_faxi_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d7_d2",
	"mainpll_d4_d2",
	"mainpll_d5_d2",
	"mainpll_d6_d2",
	"osc_d4",
};

static const char * const ufs_hd_haxi_parents[] = {
	"f26m_ck_d2",
	"mainpll_d4_d8",
	"mainpll_d7_d4",
	"mainpll_d4_d4",
	"mainpll_d5_d4",
	"mainpll_d6_d4",
	"osc_d8",
};

static const char * const bus_aximem_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d2",
	"mainpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6",
};

static const char * const axi_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d7_d2",
	"mainpll_d4_d2",
	"mainpll_d5_d2",
	"mainpll_d6_d2",
	"osc_d4",
};

static const char * const dvfsrc_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
};

static const char * const pwm_vlp_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d4",
	"clkrtc",
	"osc_d10",
	"mainpll_d4_d8",
};

static const char * const disp0_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d5_d2",
	"mmpll_d6_d2",
	"univpll_d5_d2",
	"univpll_d4_d2",
	"mmpll_d7",
	"univpll_d6",
	"mainpll_d5",
	"mainpll_d4",
	"tvdpll_ck",
	"univpll_d4",
	"mmpll_d4",
	"mmpll_d5_d2",
};

static const char * const disp1_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d5_d2",
	"mmpll_d6_d2",
	"univpll_d5_d2",
	"univpll_d4_d2",
	"mmpll_d7",
	"univpll_d6",
	"mainpll_d5",
	"mainpll_d4",
	"tvdpll_ck",
	"univpll_d4",
	"mmpll_d4",
	"mmpll_d5_d2",
};

static const char * const mdp0_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d2",
	"mmpll_d6_d2",
	"mainpll_d4_d2",
	"mmpll_d4_d2",
	"mainpll_d6",
	"univpll_d6",
	"mainpll_d5",
	"mainpll_d4",
	"tvdpll_ck",
	"univpll_d4",
	"mmpll_d4",
	"mmpll_d5_d2",
};

static const char * const mdp1_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d2",
	"mmpll_d6_d2",
	"mainpll_d4_d2",
	"mmpll_d4_d2",
	"mainpll_d6",
	"univpll_d6",
	"mainpll_d5",
	"mainpll_d4",
	"tvdpll_ck",
	"univpll_d4",
	"mmpll_d4",
	"mmpll_d5_d2",
};

static const char * const axi_vlp_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
	"osc_d2",
	"mainpll_d7_d4",
	"mainpll_d7_d2",
};

static const char * const dbgao_26m_parents[] = {
	"osc_d10",
	"tck_26m_mx9_ck",
};

static const char * const systimer_26m_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
};

static const char * const sspm_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
	"mainpll_d5_d2",
	"mainpll_d9",
	"osc_ck",
	"mainpll_d4_d2",
	"mainpll_d7",
};

static const char * const sspm_f26m_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
};

static const char * const apeint_66m_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
	"osc_d4",
};

static const char * const srck_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
};

static const char * const sramrc_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
};

static const char * const mminfra_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d2",
	"mainpll_d5_d2",
	"mmpll_d6_d2",
	"mainpll_d4_d2",
	"mmpll_d4_d2",
	"univpll_d6",
	"mainpll_d5",
	"univpll_d5",
	"mainpll_d4",
	"univpll_d4",
	"mmpll_d4",
	"mmpll_d5_d2",
};

static const char * const mmup_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d2",
	"mmpll_d4_d2",
	"univpll_d4",
	"mmpll_d4",
	"mainpll_d3",
};

static const char * const dsp_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"univpll_d4_d2",
	"univpll_d5",
	"univpll_d4",
	"mmpll_d4",
	"mainpll_d3",
	"univpll_d3",
};

static const char * const dsp1_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d5",
	"mmpll_d5",
	"univpll_d4",
	"mainpll_d3",
	"univpll_d3",
};

static const char * const dsp2_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d5",
	"mmpll_d5",
	"univpll_d4",
	"mainpll_d3",
	"univpll_d3",
};

static const char * const dsp3_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d5",
	"mmpll_d5",
	"univpll_d4",
	"mainpll_d3",
	"univpll_d3",
};

static const char * const dsp4_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"univpll_d4_d2",
	"mainpll_d4",
	"univpll_d4",
	"mmpll_d4",
	"mainpll_d3",
	"univpll_d3",
};

static const char * const dsp5_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"univpll_d4_d2",
	"mainpll_d4",
	"univpll_d4",
	"mmpll_d4",
	"mainpll_d3",
	"univpll_d3",
};

static const char * const dsp6_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"univpll_d4_d2",
	"mainpll_d4",
	"univpll_d4",
	"mmpll_d4",
	"mainpll_d3",
	"univpll_d3",
};

static const char * const dsp7_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"univpll_d4_d2",
	"univpll_d5",
	"univpll_d4",
	"mmpll_d4",
	"mainpll_d3",
	"univpll_d3",
};

static const char * const ipu_if_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"univpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6",
	"univpll_d5",
	"univpll_d4",
	"mmpll_d4",
};

static const char * const mfg_ref_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6",
	"mainpll_d5_d2",
};

static const char * const mfgsc_ref_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6",
	"mainpll_d5_d2",
};

static const char * const camtg_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_192m_d8",
	"univpll_d6_d8",
	"osc_d10",
	"univpll_d6_d16",
	"f26m_ck_d2",
	"univpll_192m_d16",
	"univpll_192m_d32",
	"univpll_192m_d10",
};

static const char * const camtg2_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_192m_d8",
	"univpll_d6_d8",
	"osc_d10",
	"univpll_d6_d16",
	"f26m_ck_d2",
	"univpll_192m_d16",
	"univpll_192m_d32",
	"univpll_192m_d10",
};

static const char * const camtg3_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_192m_d8",
	"univpll_d6_d8",
	"osc_d10",
	"univpll_d6_d16",
	"f26m_ck_d2",
	"univpll_192m_d16",
	"univpll_192m_d32",
	"univpll_192m_d10",
};

static const char * const camtg4_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_192m_d8",
	"univpll_d6_d8",
	"osc_d10",
	"univpll_d6_d16",
	"f26m_ck_d2",
	"univpll_192m_d16",
	"univpll_192m_d32",
	"univpll_192m_d10",
};

static const char * const camtg5_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_192m_d8",
	"univpll_d6_d8",
	"osc_d10",
	"univpll_d6_d16",
	"f26m_ck_d2",
	"univpll_192m_d16",
	"univpll_192m_d32",
	"univpll_192m_d10",
};

static const char * const camtg6_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_192m_d8",
	"univpll_d6_d8",
	"osc_d10",
	"univpll_d6_d16",
	"f26m_ck_d2",
	"univpll_192m_d16",
	"univpll_192m_d32",
	"univpll_192m_d10",
};

static const char * const camtg7_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_192m_d8",
	"univpll_d6_d8",
	"osc_d10",
	"univpll_d6_d16",
	"f26m_ck_d2",
	"univpll_192m_d16",
	"univpll_192m_d32",
	"univpll_192m_d10",
};

static const char * const camtg8_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_192m_d8",
	"univpll_d6_d8",
	"osc_d10",
	"univpll_d6_d16",
	"f26m_ck_d2",
	"univpll_192m_d16",
	"univpll_192m_d32",
	"univpll_192m_d10",
};

static const char * const uart_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d8",
};

static const char * const spi_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d6_d2",
	"univpll_d4_d4",
	"mainpll_d4_d4",
	"univpll_d5_d4",
	"univpll_d6_d4",
	"mainpll_d7_d4",
};

static const char * const msdc50_0_hclk_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"mainpll_d6_d2",
};

static const char * const msdc_macro_parents[] = {
	"tck_26m_mx9_ck",
	"msdcpll_ck",
	"univpll_d4_d2",
};

static const char * const msdc30_1_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d6_d2",
	"mainpll_d7_d2",
	"msdcpll_d2",
};

static const char * const msdc30_2_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d6_d2",
	"mainpll_d7_d2",
	"msdcpll_d2",
};

static const char * const audio_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d8",
	"mainpll_d7_d8",
	"mainpll_d4_d16",
};

static const char * const aud_intbus_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d7_d4",
};

static const char * const pwrap_ulposc_parents[] = {
	"osc_d10",
	"tck_26m_mx9_ck",
	"osc_d4",
	"osc_d7",
	"osc_d8",
	"osc_d16",
};

static const char * const vlp_pwrap_ulposc_parents[] = {//add
	"tck_26m_mx9_ck",
	"osc_d10",
	"osc_d7",
	"osc_d8",
	"osc_d16",
	"mainpll_d7_d8",
};

static const char * const atb_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"mainpll_d5_d2",
};

static const char * const dp_parents[] = {
	"tck_26m_mx9_ck",
	"tvdpll_d2",
	"tvdpll_d4",
	"tvdpll_d8",
	"tvdpll_d16",
};

static const char * const disp_pwm_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d4",
	"osc_d2",
	"osc_d4",
	"osc_d16",
	"univpll_d5_d4",
	"mainpll_d4_d4",
};

static const char * const usb_top_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"univpll_d6_d4",
	"univpll_d5_d2",
};

static const char * const ssusb_xhci_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"univpll_d6_d4",
	"univpll_d5_d2",
};

static const char * const usb_top_1p_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"univpll_d6_d4",
	"univpll_d5_d2",
};

static const char * const ssusb_xhci_1p_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"univpll_d6_d4",
	"univpll_d5_d2",
};

static const char * const i2c_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d8",
	"univpll_d5_d4",
	"mainpll_d4_d4",
};

static const char * const seninf_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d2",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"univpll_d7",
	"mmpll_d6",
	"univpll_d5",
};

static const char * const seninf1_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d2",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"univpll_d7",
	"mmpll_d6",
	"univpll_d5",
};

static const char * const seninf2_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d2",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"univpll_d7",
	"mmpll_d6",
	"univpll_d5",
};

static const char * const seninf3_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d2",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"univpll_d7",
	"mmpll_d6",
	"univpll_d5",
};

static const char * const seninf4_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d2",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"univpll_d7",
	"mmpll_d6",
	"univpll_d5",
};

static const char * const seninf5_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d2",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"univpll_d7",
	"mmpll_d6",
	"univpll_d5",
};

static const char * const dxcc_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"mainpll_d4_d4",
	"mainpll_d4_d8",
};

static const char * const aud_engen1_parents[] = {
	"tck_26m_mx9_ck",
	"apll1_d2",
	"apll1_d4",
	"apll1_d8",
};

static const char * const aud_engen2_parents[] = {
	"tck_26m_mx9_ck",
	"apll2_d2",
	"apll2_d4",
	"apll2_d8",
};

static const char * const aes_ufsfde_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4",
	"mainpll_d4_d2",
	"mainpll_d6",
	"mainpll_d4_d4",
	"univpll_d4_d2",
	"univpll_d6",
};

static const char * const ufs_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d4_d8",
	"univpll_d4_d4",
	"mainpll_d6_d2",
	"mainpll_d5_d2",
	"msdcpll_d2",
	"univpll_d6_d2",
};

static const char * const ufs_mbist_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"tvdpll_d2",
};

static const char * const pextp_mbist_parents[] = {
	"tck_26m_mx9_ck",
	"mmpll_d6_d2",
	"mainpll_d9",
	"univpll_d5_d2",
};

static const char * const aud_1_parents[] = {
	"tck_26m_mx9_ck",
	"apll1_ck",
};

static const char * const venc_parents[] = {
	"tck_26m_mx9_ck",
	"mmpll_d4_d2",
	"mainpll_d6",
	"univpll_d4_d2",
	"mainpll_d4_d2",
	"univpll_d6",
	"mmpll_d6",
	"mainpll_d5_d2",
	"mainpll_d6_d2",
	"mmpll_d9",
	"mmpll_d4",
	"mainpll_d4",
	"univpll_d4",
	"univpll_d5",
	"univpll_d5_d2",
	"mainpll_d5",
};

static const char * const aud_2_parents[] = {
	"tck_26m_mx9_ck",
	"apll2_ck",
};

static const char * const adsp_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d6",
	"mainpll_d5_d2",
	"univpll_d4_d4",
	"univpll_d4",
	"univpll_d6",
	"ulposc_ck",
	"adsppll_ck",
};

static const char * const dpmaif_main_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5",
	"univpll_d6",
	"univpll_d4_d4",
	"mainpll_d6",
	"mainpll_d4_d2",
	"univpll_d4_d2",
};

static const char * const vdec_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_192m_d2",
	"univpll_d5_d4",
	"mainpll_d5",
	"mainpll_d5_d2",
	"mmpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d5_d2",
	"univpll_d7",
	"mmpll_d7",
	"mmpll_d6",
	"univpll_d6",
	"tvdpll_ck",
	"imgpll_d2",
	"mmpll_d4_d2",
};

static const char * const pwm_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4_d8",
};

static const char * const audio_h_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d7",
	"apll1_ck",
	"apll2_ck",
};

static const char * const mcupm_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d2",
	"mainpll_d6_d2",
};

static const char * const spmi_p_mst_parents[] = {
	"tck_26m_mx9_ck",
	"f26m_ck_d2",
	"osc_d8",
	"osc_d10",
	"osc_d16",
	"osc_d7",
	"clkrtc",
	"mainpll_d7_d8",
	"mainpll_d6_d8",
	"mainpll_d5_d8",
};

static const char * const spmi_m_mst_parents[] = {
	"tck_26m_mx9_ck",
	"f26m_ck_d2",
	"osc_d8",
	"osc_d10",
	"osc_d16",
	"osc_d7",
	"clkrtc",
	"mainpll_d7_d8",
	"mainpll_d6_d8",
	"mainpll_d5_d8",
};

static const char * const vlp_spmi_p_mst_parents[] = {//add
	"tck_26m_mx9_ck",
	"f26m_ck_d2",
	"osc_d8",
	"osc_d10",
	"osc_d16",
	"osc_d7",
	"clkrtc",
	"mainpll_d7_d8",
	"mainpll_d6_d8",
	"mainpll_d5_d8",
};

static const char * const vlp_spmi_m_mst_parents[] = {//add
	"tck_26m_mx9_ck",
	"f26m_ck_d2",
	"osc_d8",
	"osc_d10",
	"osc_d16",
	"osc_d7",
	"clkrtc",
	"mainpll_d7_d8",
	"mainpll_d6_d8",
	"mainpll_d5_d8",
};


static const char * const tl_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"mainpll_d4_d4",
};

static const char * const mem_sub_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4_d4",
	"osc_d2",
	"mainpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6",
	"mmpll_d7",
	"mainpll_d5",
	"univpll_d5",
	"mainpll_d4",
	"univpll_d4",
};

static const char * const peri_hf_fmem_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4_d4",
	"mainpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6",
	"univpll_d6",
	"univpll_d5",
	"mainpll_d4",
};

static const char * const ufs_hf_fmem_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4_d4",
	"mainpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6",
	"univpll_d6",
	"univpll_d5",
	"mainpll_d4",
};

static const char * const aes_msdcfde_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"mainpll_d6",
	"mainpll_d4_d4",
	"univpll_d4_d2",
	"univpll_d6",
};

static const char * const dsi_occ_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d6_d2",
	"univpll_d5_d2",
	"univpll_d4_d2",
};

static const char * const dptx_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d4",
	"mainpll_d7_d2",
};

static const char * const ccu_ahb_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d2",
	"tvdpll_ck",
	"mainpll_d4",
	"univpll_d5",
	"univpll_d6",
	"univpll_d4_d2",
	"mainpll_d4_d2",
};

static const char * const ap2conn_host_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d4",
};

static const char * const img1_parents[] = {
	"tck_26m_mx9_ck",
	"imgpll_d2",
	"univpll_d4",
	"mainpll_d4",
	"mmpll_d6",
	"univpll_d6",
	"mmpll_d7",
	"mmpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d4_d2",
	"imgpll_d5",
	"mmpll_d6_d2",
};

static const char * const ipe_parents[] = {
	"tck_26m_mx9_ck",
	"imgpll_d2",
	"mainpll_d4",
	"mmpll_d6",
	"univpll_d6",
	"mainpll_d6",
	"mmpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d4_d2",
	"mmpll_d6_d2",
};

static const char * const cam_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4",
	"mmpll_d4",
	"univpll_d4",
	"mmpll_d6",
	"univpll_d6",
	"mmpll_d7",
	"univpll_d4_d2",
	"mainpll_d4_d2",
	"osc_d2",
};

static const char * const ccusys_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d2",
	"mmpll_d6",
	"mainpll_d4",
	"univpll_d5",
	"univpll_d6",
	"univpll_d4_d2",
	"mainpll_d4_d2",
};

static const char * const camtm_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d2",
	"univpll_d6_d2",
	"univpll_d6_d4",
};

static const char * const sflash_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d8",
	"univpll_d6_d8",
};

static const char * const mcu_acp_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d2",
	"mmpll_d5_d2",
	"mmpll_d7",
	"mainpll_d4",
	"univpll_d4",
};

static const char * const tl_ck_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"mainpll_d4_d4",
};

static const char * const mfg_0_parents[] = {
	"mfg_ref_sel",
	"mfgpll_ck",
};

static const char * const mfg_1_parents[] = {
	"mfgsc_ref_sel",
	"mfgscpll_ck",
};

static const char * const apll_i2s0_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel",
};

static const char * const apll_i2s1_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel",
};

static const char * const apll_i2s2_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel",
};

static const char * const apll_i2s3_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel",
};

static const char * const apll_i2s4_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel",
};

static const char * const apll_i2s5_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel",
};

static const char * const apll_i2s6_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel",
};

static const char * const apll_i2s7_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel",
};

static const char * const apll_i2s8_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel",
};

static const char * const apll_i2s9_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel",
};


static struct mtk_mux vlp_cksys_muxes[] = {
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CKSYS_APXGPT66M_BCLK_SEL /* dts */,
		"apxgpt66m_bclk_sel", apxgpt66m_bclk_parents /* parents*/,
		VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET, VLP_CLK_CFG_0_CLR, /* sta/set/clr */
		16 /* lsb */, 1 /* width */, 23 /* pdn */,
		VLP_CLK_CFG_UPDATE /* upd ofs */,
		VLP_CKSYS_MUX_APXGPT66M_BCLK_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CKSYS_DXCC_VLP_SEL /* dts */,
		"dxcc_vlp_sel", dxcc_vlp_parents /* parents*/,
		VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET, VLP_CLK_CFG_0_CLR, /* sta/set/clr */
		24 /* lsb */, 3 /* width */, 31 /* pdn */,
		VLP_CLK_CFG_UPDATE /* upd ofs */,
		VLP_CKSYS_MUX_DXCC_VLP_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CKSYS_SCP_SEL /* dts */,
		"scp_sel", scp_parents /* parents*/,
		VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET, VLP_CLK_CFG_0_CLR, /* sta/set/clr */
		0 /* lsb */, 4 /* width */, 7 /* pdn */,
		VLP_CLK_CFG_UPDATE /* upd ofs */,
		VLP_CKSYS_MUX_SCP_SHIFT /* upd shift */),
	//add
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CKSYS_PWRAP_ULPOSC_SEL /* dts */,
		"vlp_pwrap_ulposc_sel", vlp_pwrap_ulposc_parents /* parents*/,
		VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET, VLP_CLK_CFG_0_CLR, /* sta/set/clr */
		8 /* lsb */, 3 /* width */, 15 /* pdn */,
		VLP_CLK_CFG_UPDATE /* upd ofs */,
		VLP_CKSYS_MUX_PWRAP_ULPOSC_SHIFT /* upd shift */),

	MUX_GATE_CLR_SET_UPD(CLK_VLP_CKSYS_SPMI_P_MST_SEL /* dts */,
		"vlp_spmi_p_mst_sel", vlp_spmi_p_mst_parents /* parents*/,
		VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET, VLP_CLK_CFG_1_CLR, /* sta/set/clr */
		0 /* lsb */, 4 /* width */, 7 /* pdn */,
		VLP_CLK_CFG_UPDATE /* upd ofs */,
		VLP_CKSYS_MUX_SPMI_P_MST_SHIFT /* upd shift */),

	MUX_GATE_CLR_SET_UPD(CLK_VLP_CKSYS_SPMI_M_MST_SEL /* dts */,
		"vlp_spmi_m_mst_sel", vlp_spmi_m_mst_parents /* parents*/,
		VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET, VLP_CLK_CFG_1_CLR, /* sta/set/clr */
		8 /* lsb */, 4 /* width */, 15 /* pdn */,
		VLP_CLK_CFG_UPDATE /* upd ofs */,
		VLP_CKSYS_MUX_SPMI_M_MST_SHIFT /* upd shift */),
  //add

	MUX_GATE_CLR_SET_UPD(CLK_VLP_CKSYS_DVFSRC_SEL /* dts */,
		"dvfsrc_sel", dvfsrc_parents /* parents*/,
		VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET, VLP_CLK_CFG_1_CLR, /* sta/set/clr */
		16 /* lsb */, 1 /* width */, 23 /* pdn */,
		VLP_CLK_CFG_UPDATE /* upd ofs */,
		VLP_CKSYS_MUX_DVFSRC_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CKSYS_PWM_VLP_SEL /* dts */,
		"pwm_vlp_sel", pwm_vlp_parents /* parents*/,
		VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET, VLP_CLK_CFG_1_CLR, /* sta/set/clr */
		24 /* lsb */, 3 /* width */, 31 /* pdn */,
		VLP_CLK_CFG_UPDATE /* upd ofs */,
		VLP_CKSYS_MUX_PWM_VLP_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CKSYS_AXI_VLP_SEL /* dts */,
		"axi_vlp_sel", axi_vlp_parents /* parents*/,
		VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET, VLP_CLK_CFG_2_CLR, /* sta/set/clr */
		0 /* lsb */, 3 /* width */, 7 /* pdn */,
		VLP_CLK_CFG_UPDATE /* upd ofs */,
		VLP_CKSYS_MUX_AXI_VLP_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CKSYS_DBGAO_26M_SEL /* dts */,
		"dbgao_26m_sel", dbgao_26m_parents /* parents*/,
		VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET, VLP_CLK_CFG_2_CLR, /* sta/set/clr */
		8 /* lsb */, 1 /* width */, 15 /* pdn */,
		VLP_CLK_CFG_UPDATE /* upd ofs */,
		VLP_CKSYS_MUX_DBGAO_26M_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CKSYS_SYSTIMER_26M_SEL /* dts */,
		"systimer_26m_sel", systimer_26m_parents /* parents*/,
		VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET, VLP_CLK_CFG_2_CLR, /* sta/set/clr */
		16 /* lsb */, 1 /* width */, 23 /* pdn */,
		VLP_CLK_CFG_UPDATE /* upd ofs */,
		VLP_CKSYS_MUX_SYSTIMER_26M_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CKSYS_SSPM_SEL /* dts */,
		"sspm_sel", sspm_parents /* parents*/,
		VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET, VLP_CLK_CFG_2_CLR, /* sta/set/clr */
		24 /* lsb */, 3 /* width */, 31 /* pdn */,
		VLP_CLK_CFG_UPDATE /* upd ofs */,
		VLP_CKSYS_MUX_SSPM_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CKSYS_SSPM_F26M_SEL /* dts */,
		"sspm_f26m_sel", sspm_f26m_parents /* parents*/,
		VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET, VLP_CLK_CFG_3_CLR, /* sta/set/clr */
		0 /* lsb */, 1 /* width */, 7 /* pdn */,
		VLP_CLK_CFG_UPDATE /* upd ofs */,
		VLP_CKSYS_MUX_SSPM_F26M_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CKSYS_APEINT_66M_SEL /* dts */,
		"apeint_66m_sel", apeint_66m_parents /* parents*/,
		VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET, VLP_CLK_CFG_3_CLR, /* sta/set/clr */
		8 /* lsb */, 2 /* width */, 15 /* pdn */,
		VLP_CLK_CFG_UPDATE /* upd ofs */,
		VLP_CKSYS_MUX_APEINT_66M_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CKSYS_SRCK_SEL /* dts */,
		"srck_sel", srck_parents /* parents*/,
		VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET, VLP_CLK_CFG_3_CLR, /* sta/set/clr */
		16 /* lsb */, 1 /* width */, 23 /* pdn */,
		VLP_CLK_CFG_UPDATE /* upd ofs */,
		VLP_CKSYS_MUX_SRCK_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_VLP_CKSYS_SRAMRC_SEL /* dts */,
		"sramrc_sel", sramrc_parents /* parents*/,
		VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET, VLP_CLK_CFG_3_CLR, /* sta/set/clr */
		24 /* lsb */, 1 /* width */, 31 /* pdn */,
		VLP_CLK_CFG_UPDATE /* upd ofs */,
		VLP_CKSYS_MUX_SRAMRC_SHIFT /* upd shift */),
};

#define HWV_CLK_CFG_1_SET		0x000
#define HWV_CLK_CFG_1_CLR		0x004
#define HWV_CLK_CFG_1_STA		0x1C00
#define HWV_CLK_CFG_2_SET		0x010
#define HWV_CLK_CFG_2_CLR		0x014
#define HWV_CLK_CFG_2_STA		0x1C08
#define HWV_CLK_CFG_15_SET		0x040
#define HWV_CLK_CFG_15_CLR		0x044
#define HWV_CLK_CFG_15_STA		0x1C20
#define HWV_CLK_CFG_16_SET		0x050
#define HWV_CLK_CFG_16_CLR		0x054
#define HWV_CLK_CFG_16_STA		0x1C28
#define HWV_CLK_CFG_20_SET		0x070
#define HWV_CLK_CFG_20_CLR		0x074
#define HWV_CLK_CFG_20_STA		0x1C38
#define HWV_CLK_CFG_21_SET		0x080
#define HWV_CLK_CFG_21_CLR		0x084
#define HWV_CLK_CFG_21_STA		0x1C40
static struct mtk_mux top_muxes[] = {
	MUX_GATE_CLR_SET_UPD(CLK_TOP_PERI_HD_FAXI_SEL /* dts */,
		"peri_hd_faxi_sel", peri_hd_faxi_parents /* parents*/,
		CLK_CFG_0, CLK_CFG_0_SET, CLK_CFG_0_CLR, /* sta/set/clr */
		8 /* lsb */, 3 /* width */, 15 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_PERI_HD_FAXI_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_UFS_HD_HAXI_SEL /* dts */,
		"ufs_hd_haxi_sel", ufs_hd_haxi_parents /* parents*/,
		CLK_CFG_0, CLK_CFG_0_SET, CLK_CFG_0_CLR, /* sta/set/clr */
		16 /* lsb */, 3 /* width */, 23 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_UFS_HD_HAXI_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_BUS_AXIMEM_SEL /* dts */,
		"bus_aximem_sel", bus_aximem_parents /* parents*/,
		CLK_CFG_0, CLK_CFG_0_SET, CLK_CFG_0_CLR, /* sta/set/clr */
		24 /* lsb */, 3 /* width */, 31 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_BUS_AXIMEM_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AXI_SEL /* dts */,
		"axi_sel", axi_parents /* parents*/,
		CLK_CFG_0, CLK_CFG_0_SET, CLK_CFG_0_CLR, /* sta/set/clr */
		0 /* lsb */, 3 /* width */, 7 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_AXI_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DISP0_SEL /* dts */,
		"disp0_sel", disp0_parents /* parents*/,
		CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR, /* sta/set/clr */
		0 /* lsb */, 4 /* width */, 7 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_DISP0_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DISP1_SEL /* dts */,
		"disp1_sel", disp1_parents /* parents*/,
		CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR, /* sta/set/clr */
		8 /* lsb */, 4 /* width */, 15 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_DISP1_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MDP0_SEL /* dts */,
		"mdp0_sel", mdp0_parents /* parents*/,
		CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR, /* sta/set/clr */
		16 /* lsb */, 4 /* width */, 23 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_MDP0_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MDP1_SEL /* dts */,
		"mdp1_sel", mdp1_parents /* parents*/,
		CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR, /* sta/set/clr */
		24 /* lsb */, 4 /* width */, 31 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_MDP1_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MMINFRA_SEL /* dts */,
		"mminfra_sel", mminfra_parents /* parents*/,
		CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR, /* sta/set/clr */
		0 /* lsb */, 4 /* width */, 7 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_MMINFRA_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MMUP_SEL /* dts */,
		"mmup_sel", mmup_parents /* parents*/,
		CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR, /* sta/set/clr */
		8 /* lsb */, 3 /* width */, 15 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_MMUP_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DSP_SEL /* dts */,
		"dsp_sel", dsp_parents /* parents*/,
		CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR, /* sta/set/clr */
		16 /* lsb */, 3 /* width */, 23 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_DSP_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DSP1_SEL /* dts */,
		"dsp1_sel", dsp1_parents /* parents*/,
		CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR, /* sta/set/clr */
		24 /* lsb */, 3 /* width */, 31 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_DSP1_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DSP2_SEL /* dts */,
		"dsp2_sel", dsp2_parents /* parents*/,
		CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR, /* sta/set/clr */
		0 /* lsb */, 3 /* width */, 7 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_DSP2_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DSP3_SEL /* dts */,
		"dsp3_sel", dsp3_parents /* parents*/,
		CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR, /* sta/set/clr */
		8 /* lsb */, 3 /* width */, 15 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_DSP3_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DSP4_SEL /* dts */,
		"dsp4_sel", dsp4_parents /* parents*/,
		CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR, /* sta/set/clr */
		16 /* lsb */, 3 /* width */, 23 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_DSP4_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DSP5_SEL /* dts */,
		"dsp5_sel", dsp5_parents /* parents*/,
		CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR, /* sta/set/clr */
		24 /* lsb */, 3 /* width */, 31 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_DSP5_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DSP6_SEL /* dts */,
		"dsp6_sel", dsp6_parents /* parents*/,
		CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR, /* sta/set/clr */
		0 /* lsb */, 3 /* width */, 7 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_DSP6_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DSP7_SEL /* dts */,
		"dsp7_sel", dsp7_parents /* parents*/,
		CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR, /* sta/set/clr */
		8 /* lsb */, 3 /* width */, 15 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_DSP7_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_IPU_IF_SEL /* dts */,
		"ipu_if_sel", ipu_if_parents /* parents*/,
		CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR, /* sta/set/clr */
		16 /* lsb */, 3 /* width */, 23 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_IPU_IF_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MFG_REF_SEL /* dts */,
		"mfg_ref_sel", mfg_ref_parents /* parents*/,
		CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR, /* sta/set/clr */
		24 /* lsb */, 2 /* width */, 31 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_MFG_REF_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MFGSC_REF_SEL /* dts */,
		"mfgsc_ref_sel", mfgsc_ref_parents /* parents*/,
		CLK_CFG_5, CLK_CFG_5_SET, CLK_CFG_5_CLR, /* sta/set/clr */
		0 /* lsb */, 2 /* width */, 7 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_MFGSC_REF_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG_SEL /* dts */,
		"camtg_sel", camtg_parents /* parents*/,
		CLK_CFG_5, CLK_CFG_5_SET, CLK_CFG_5_CLR, /* sta/set/clr */
		8 /* lsb */, 4 /* width */, 15 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_CAMTG_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG2_SEL /* dts */,
		"camtg2_sel", camtg2_parents /* parents*/,
		CLK_CFG_5, CLK_CFG_5_SET, CLK_CFG_5_CLR, /* sta/set/clr */
		16 /* lsb */, 4 /* width */, 23 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_CAMTG2_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG3_SEL /* dts */,
		"camtg3_sel", camtg3_parents /* parents*/,
		CLK_CFG_5, CLK_CFG_5_SET, CLK_CFG_5_CLR, /* sta/set/clr */
		24 /* lsb */, 4 /* width */, 31 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_CAMTG3_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG4_SEL /* dts */,
		"camtg4_sel", camtg4_parents /* parents*/,
		CLK_CFG_6, CLK_CFG_6_SET, CLK_CFG_6_CLR, /* sta/set/clr */
		0 /* lsb */, 4 /* width */, 7 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_CAMTG4_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG5_SEL /* dts */,
		"camtg5_sel", camtg5_parents /* parents*/,
		CLK_CFG_6, CLK_CFG_6_SET, CLK_CFG_6_CLR, /* sta/set/clr */
		8 /* lsb */, 4 /* width */, 15 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_CAMTG5_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG6_SEL /* dts */,
		"camtg6_sel", camtg6_parents /* parents*/,
		CLK_CFG_6, CLK_CFG_6_SET, CLK_CFG_6_CLR, /* sta/set/clr */
		16 /* lsb */, 4 /* width */, 23 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_CAMTG6_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG7_SEL /* dts */,
		"camtg7_sel", camtg7_parents /* parents*/,
		CLK_CFG_6, CLK_CFG_6_SET, CLK_CFG_6_CLR, /* sta/set/clr */
		24 /* lsb */, 4 /* width */, 31 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_CAMTG7_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG8_SEL /* dts */,
		"camtg8_sel", camtg8_parents /* parents*/,
		CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR, /* sta/set/clr */
		0 /* lsb */, 4 /* width */, 7 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_CAMTG8_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_UART_SEL /* dts */,
		"uart_sel", uart_parents /* parents*/,
		CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR, /* sta/set/clr */
		8 /* lsb */, 1 /* width */, 15 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_UART_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI_SEL /* dts */,
		"spi_sel", spi_parents /* parents*/,
		CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR, /* sta/set/clr */
		16 /* lsb */, 3 /* width */, 23 /* pdn */,
		CLK_CFG_UPDATE_0 /* upd ofs */,
		TOP_MUX_SPI_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC50_0_HCLK_SEL /* dts */,
		"msdc50_0_hclk_sel", msdc50_0_hclk_parents /* parents*/,
		CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR, /* sta/set/clr */
		24 /* lsb */, 2 /* width */, 31 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_MSDC50_0_HCLK_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC_MACRO_SEL /* dts */,
		"msdc_macro_sel", msdc_macro_parents /* parents*/,
		CLK_CFG_8, CLK_CFG_8_SET, CLK_CFG_8_CLR, /* sta/set/clr */
		0 /* lsb */, 2 /* width */, 7 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_MSDC_MACRO_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC30_1_SEL /* dts */,
		"msdc30_1_sel", msdc30_1_parents /* parents*/,
		CLK_CFG_8, CLK_CFG_8_SET, CLK_CFG_8_CLR, /* sta/set/clr */
		8 /* lsb */, 3 /* width */, 15 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_MSDC30_1_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC30_2_SEL /* dts */,
		"msdc30_2_sel", msdc30_2_parents /* parents*/,
		CLK_CFG_8, CLK_CFG_8_SET, CLK_CFG_8_CLR, /* sta/set/clr */
		16 /* lsb */, 3 /* width */, 23 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_MSDC30_2_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUDIO_SEL /* dts */,
		"audio_sel", audio_parents /* parents*/,
		CLK_CFG_8, CLK_CFG_8_SET, CLK_CFG_8_CLR, /* sta/set/clr */
		24 /* lsb */, 2 /* width */, 31 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_AUDIO_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_INTBUS_SEL /* dts */,
		"aud_intbus_sel", aud_intbus_parents /* parents*/,
		CLK_CFG_9, CLK_CFG_9_SET, CLK_CFG_9_CLR, /* sta/set/clr */
		0 /* lsb */, 2 /* width */, 7 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_AUD_INTBUS_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_PWRAP_ULPOSC_SEL /* dts */,
		"pwrap_ulposc_sel", pwrap_ulposc_parents /* parents*/,
		CLK_CFG_9, CLK_CFG_9_SET, CLK_CFG_9_CLR, /* sta/set/clr */
		8 /* lsb */, 3 /* width */, 15 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_PWRAP_ULPOSC_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_ATB_SEL /* dts */,
		"atb_sel", atb_parents /* parents*/,
		CLK_CFG_9, CLK_CFG_9_SET, CLK_CFG_9_CLR, /* sta/set/clr */
		16 /* lsb */, 2 /* width */, 23 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_ATB_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DP_SEL /* dts */,
		"dp_sel", dp_parents /* parents*/,
		CLK_CFG_9, CLK_CFG_9_SET, CLK_CFG_9_CLR, /* sta/set/clr */
		24 /* lsb */, 3 /* width */, 31 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_DP_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DISP_PWM_SEL /* dts */,
		"disp_pwm_sel", disp_pwm_parents /* parents*/,
		CLK_CFG_10, CLK_CFG_10_SET, CLK_CFG_10_CLR, /* sta/set/clr */
		0 /* lsb */, 3 /* width */, 7 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_DISP_PWM_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USB_TOP_SEL /* dts */,
		"usb_top_sel", usb_top_parents /* parents*/,
		CLK_CFG_10, CLK_CFG_10_SET, CLK_CFG_10_CLR, /* sta/set/clr */
		8 /* lsb */, 2 /* width */, 15 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_USB_TOP_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SSUSB_XHCI_SEL /* dts */,
		"ssusb_xhci_sel", ssusb_xhci_parents /* parents*/,
		CLK_CFG_10, CLK_CFG_10_SET, CLK_CFG_10_CLR, /* sta/set/clr */
		16 /* lsb */, 2 /* width */, 23 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_SSUSB_XHCI_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USB_TOP_1P_SEL /* dts */,
		"usb_top_1p_sel", usb_top_1p_parents /* parents*/,
		CLK_CFG_10, CLK_CFG_10_SET, CLK_CFG_10_CLR, /* sta/set/clr */
		24 /* lsb */, 2 /* width */, 31 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_USB_TOP_1P_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SSUSB_XHCI_1P_SEL /* dts */,
		"ssusb_xhci_1p_sel", ssusb_xhci_1p_parents /* parents*/,
		CLK_CFG_11, CLK_CFG_11_SET, CLK_CFG_11_CLR, /* sta/set/clr */
		0 /* lsb */, 2 /* width */, 7 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_SSUSB_XHCI_1P_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_I2C_SEL /* dts */,
		"i2c_sel", i2c_parents /* parents*/,
		CLK_CFG_11, CLK_CFG_11_SET, CLK_CFG_11_CLR, /* sta/set/clr */
		8 /* lsb */, 2 /* width */, 15 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_I2C_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF_SEL /* dts */,
		"seninf_sel", seninf_parents /* parents*/,
		CLK_CFG_11, CLK_CFG_11_SET, CLK_CFG_11_CLR, /* sta/set/clr */
		16 /* lsb */, 3 /* width */, 23 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_SENINF_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF1_SEL /* dts */,
		"seninf1_sel", seninf1_parents /* parents*/,
		CLK_CFG_11, CLK_CFG_11_SET, CLK_CFG_11_CLR, /* sta/set/clr */
		24 /* lsb */, 3 /* width */, 31 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_SENINF1_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF2_SEL /* dts */,
		"seninf2_sel", seninf2_parents /* parents*/,
		CLK_CFG_12, CLK_CFG_12_SET, CLK_CFG_12_CLR, /* sta/set/clr */
		0 /* lsb */, 3 /* width */, 7 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_SENINF2_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF3_SEL /* dts */,
		"seninf3_sel", seninf3_parents /* parents*/,
		CLK_CFG_12, CLK_CFG_12_SET, CLK_CFG_12_CLR, /* sta/set/clr */
		8 /* lsb */, 3 /* width */, 15 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_SENINF3_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF4_SEL /* dts */,
		"seninf4_sel", seninf4_parents /* parents*/,
		CLK_CFG_12, CLK_CFG_12_SET, CLK_CFG_12_CLR, /* sta/set/clr */
		16 /* lsb */, 3 /* width */, 23 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_SENINF4_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENINF5_SEL /* dts */,
		"seninf5_sel", seninf5_parents /* parents*/,
		CLK_CFG_12, CLK_CFG_12_SET, CLK_CFG_12_CLR, /* sta/set/clr */
		24 /* lsb */, 3 /* width */, 31 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_SENINF5_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DXCC_SEL /* dts */,
		"dxcc_sel", dxcc_parents /* parents*/,
		CLK_CFG_13, CLK_CFG_13_SET, CLK_CFG_13_CLR, /* sta/set/clr */
		0 /* lsb */, 2 /* width */, 7 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_DXCC_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_ENGEN1_SEL /* dts */,
		"aud_engen1_sel", aud_engen1_parents /* parents*/,
		CLK_CFG_13, CLK_CFG_13_SET, CLK_CFG_13_CLR, /* sta/set/clr */
		8 /* lsb */, 2 /* width */, 15 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_AUD_ENGEN1_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_ENGEN2_SEL /* dts */,
		"aud_engen2_sel", aud_engen2_parents /* parents*/,
		CLK_CFG_13, CLK_CFG_13_SET, CLK_CFG_13_CLR, /* sta/set/clr */
		16 /* lsb */, 2 /* width */, 23 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_AUD_ENGEN2_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AES_UFSFDE_SEL /* dts */,
		"aes_ufsfde_sel", aes_ufsfde_parents /* parents*/,
		CLK_CFG_13, CLK_CFG_13_SET, CLK_CFG_13_CLR, /* sta/set/clr */
		24 /* lsb */, 3 /* width */, 31 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_AES_UFSFDE_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_UFS_SEL /* dts */,
		"ufs_sel", ufs_parents /* parents*/,
		CLK_CFG_14, CLK_CFG_14_SET, CLK_CFG_14_CLR, /* sta/set/clr */
		0 /* lsb */, 3 /* width */, 7 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_UFS_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_UFS_MBIST_SEL /* dts */,
		"ufs_mbist_sel", ufs_mbist_parents /* parents*/,
		CLK_CFG_14, CLK_CFG_14_SET, CLK_CFG_14_CLR, /* sta/set/clr */
		8 /* lsb */, 2 /* width */, 15 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_UFS_MBIST_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_PEXTP_MBIST_SEL /* dts */,
		"pextp_mbist_sel", pextp_mbist_parents /* parents*/,
		CLK_CFG_14, CLK_CFG_14_SET, CLK_CFG_14_CLR, /* sta/set/clr */
		16 /* lsb */, 2 /* width */, 23 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_PEXTP_MBIST_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_1_SEL /* dts */,
		"aud_1_sel", aud_1_parents /* parents*/,
		CLK_CFG_14, CLK_CFG_14_SET, CLK_CFG_14_CLR, /* sta/set/clr */
		24 /* lsb */, 1 /* width */, 31 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_AUD_1_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_VENC_SEL /* dts */,
		"venc_sel", venc_parents /* parents*/,
		CLK_CFG_15, CLK_CFG_15_SET, CLK_CFG_15_CLR, /* sta/set/clr */
		24 /* lsb */, 4 /* width */, 31 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_VENC_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_2_SEL /* dts */,
		"aud_2_sel", aud_2_parents /* parents*/,
		CLK_CFG_15, CLK_CFG_15_SET, CLK_CFG_15_CLR, /* sta/set/clr */
		0 /* lsb */, 1 /* width */, 7 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_AUD_2_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_ADSP_SEL /* dts */,
		"adsp_sel", adsp_parents /* parents*/,
		CLK_CFG_15, CLK_CFG_15_SET, CLK_CFG_15_CLR, /* sta/set/clr */
		8 /* lsb */, 3 /* width */, 15 /* pdn */,
		CLK_CFG_UPDATE_1 /* upd ofs */,
		TOP_MUX_ADSP_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DPMAIF_MAIN_SEL /* dts */,
		"dpmaif_main_sel", dpmaif_main_parents /* parents*/,
		CLK_CFG_15, CLK_CFG_15_SET, CLK_CFG_15_CLR, /* sta/set/clr */
		16 /* lsb */, 3 /* width */, 23 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_DPMAIF_MAIN_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_VDEC_SEL /* dts */,
		"vdec_sel", vdec_parents /* parents*/,
		CLK_CFG_16, CLK_CFG_16_SET, CLK_CFG_16_CLR, /* sta/set/clr */
		0 /* lsb */, 4 /* width */, 7 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_VDEC_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_PWM_SEL /* dts */,
		"pwm_sel", pwm_parents /* parents*/,
		CLK_CFG_16, CLK_CFG_16_SET, CLK_CFG_16_CLR, /* sta/set/clr */
		8 /* lsb */, 1 /* width */, 15 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_PWM_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUDIO_H_SEL /* dts */,
		"audio_h_sel", audio_h_parents /* parents*/,
		CLK_CFG_16, CLK_CFG_16_SET, CLK_CFG_16_CLR, /* sta/set/clr */
		16 /* lsb */, 2 /* width */, 23 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_AUDIO_H_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MCUPM_SEL /* dts */,
		"mcupm_sel", mcupm_parents /* parents*/,
		CLK_CFG_16, CLK_CFG_16_SET, CLK_CFG_16_CLR, /* sta/set/clr */
		24 /* lsb */, 2 /* width */, 31 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_MCUPM_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPMI_P_MST_SEL /* dts */,
		"spmi_p_mst_sel", spmi_p_mst_parents /* parents*/,
		CLK_CFG_17, CLK_CFG_17_SET, CLK_CFG_17_CLR, /* sta/set/clr */
		0 /* lsb */, 4 /* width */, 7 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_SPMI_P_MST_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPMI_M_MST_SEL /* dts */,
		"spmi_m_mst_sel", spmi_m_mst_parents /* parents*/,
		CLK_CFG_17, CLK_CFG_17_SET, CLK_CFG_17_CLR, /* sta/set/clr */
		8 /* lsb */, 4 /* width */, 15 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_SPMI_M_MST_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_TL_SEL /* dts */,
		"tl_sel", tl_parents /* parents*/,
		CLK_CFG_17, CLK_CFG_17_SET, CLK_CFG_17_CLR, /* sta/set/clr */
		16 /* lsb */, 2 /* width */, 23 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_TL_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MEM_SUB_SEL /* dts */,
		"mem_sub_sel", mem_sub_parents /* parents*/,
		CLK_CFG_17, CLK_CFG_17_SET, CLK_CFG_17_CLR, /* sta/set/clr */
		24 /* lsb */, 4 /* width */, 31 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_MEM_SUB_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_PERI_HF_FMEM_SEL /* dts */,
		"peri_hf_fmem_sel", peri_hf_fmem_parents /* parents*/,
		CLK_CFG_18, CLK_CFG_18_SET, CLK_CFG_18_CLR, /* sta/set/clr */
		0 /* lsb */, 3 /* width */, 7 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_PERI_HF_FMEM_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_UFS_HF_FMEM_SEL /* dts */,
		"ufs_hf_fmem_sel", ufs_hf_fmem_parents /* parents*/,
		CLK_CFG_18, CLK_CFG_18_SET, CLK_CFG_18_CLR, /* sta/set/clr */
		8 /* lsb */, 3 /* width */, 15 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_UFS_HF_FMEM_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AES_MSDCFDE_SEL /* dts */,
		"aes_msdcfde_sel", aes_msdcfde_parents /* parents*/,
		CLK_CFG_18, CLK_CFG_18_SET, CLK_CFG_18_CLR, /* sta/set/clr */
		16 /* lsb */, 3 /* width */, 23 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_AES_MSDCFDE_SHIFT /* upd shift */),

	MUX_GATE_CLR_SET_UPD(CLK_TOP_DSI_OCC_SEL /* dts */,
		"dsi_occ_sel", dsi_occ_parents /* parents*/,
		CLK_CFG_19, CLK_CFG_19_SET, CLK_CFG_19_CLR, /* sta/set/clr */
		8 /* lsb */, 2 /* width */, 15 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_DSI_OCC_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DPTX_SEL /* dts */,
		"dptx_sel", dptx_parents /* parents*/,
		CLK_CFG_19, CLK_CFG_19_SET, CLK_CFG_19_CLR, /* sta/set/clr */
		16 /* lsb */, 2 /* width */, 23 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_DPTX_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CCU_AHB_SEL /* dts */,
		"ccu_ahb_sel", ccu_ahb_parents /* parents*/,
		CLK_CFG_19, CLK_CFG_19_SET, CLK_CFG_19_CLR, /* sta/set/clr */
		24 /* lsb */, 3 /* width */, 31 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_CCU_AHB_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AP2CONN_HOST_SEL /* dts */,
		"ap2conn_host_sel", ap2conn_host_parents /* parents*/,
		CLK_CFG_20, CLK_CFG_20_SET, CLK_CFG_20_CLR, /* sta/set/clr */
		0 /* lsb */, 1 /* width */, 7 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_AP2CONN_HOST_SHIFT /* upd shift */),

	MUX_HWV(CLK_TOP_IMG1_SEL /* dts */,
		"img1_sel", img1_parents /* parents*/,
		CLK_CFG_20, CLK_CFG_20_SET, CLK_CFG_20_CLR, /* sta/set/clr */
		HWV_CLK_CFG_20_STA, HWV_CLK_CFG_20_SET, HWV_CLK_CFG_20_CLR, /* sta/set/clr */
		8 /* lsb */, 4 /* width */, 15 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_IMG1_SHIFT /* upd shift */),
	MUX_HWV(CLK_TOP_IPE_SEL /* dts */,
		"ipe_sel", ipe_parents /* parents*/,
		CLK_CFG_20, CLK_CFG_20_SET, CLK_CFG_20_CLR, /* sta/set/clr */
		HWV_CLK_CFG_20_STA, HWV_CLK_CFG_20_SET, HWV_CLK_CFG_20_CLR, /* sta/set/clr */
		16 /* lsb */, 4 /* width */, 23 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_IPE_SHIFT /* upd shift */),
	MUX_HWV(CLK_TOP_CAM_SEL /* dts */,
		"cam_sel", cam_parents /* parents*/,
		CLK_CFG_20, CLK_CFG_20_SET, CLK_CFG_20_CLR, /* sta/set/clr */
		HWV_CLK_CFG_20_STA, HWV_CLK_CFG_20_SET, HWV_CLK_CFG_20_CLR, /* sta/set/clr */
		24 /* lsb */, 4 /* width */, 31 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_CAM_SHIFT /* upd shift */),
	MUX_HWV(CLK_TOP_CCUSYS_SEL /* dts */,
		"ccusys_sel", ccusys_parents /* parents*/,
		CLK_CFG_21, CLK_CFG_21_SET, CLK_CFG_21_CLR, /* sta/set/clr */
		HWV_CLK_CFG_21_STA, HWV_CLK_CFG_21_SET, HWV_CLK_CFG_21_CLR, /* sta/set/clr */
		0 /* lsb */, 3 /* width */, 7 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_CCUSYS_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTM_SEL /* dts */,
		"camtm_sel", camtm_parents /* parents*/,
		CLK_CFG_21, CLK_CFG_21_SET, CLK_CFG_21_CLR, /* sta/set/clr */
		8 /* lsb */, 2 /* width */, 15 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_CAMTM_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SFLASH_SEL /* dts */,
		"sflash_sel", sflash_parents /* parents*/,
		CLK_CFG_21, CLK_CFG_21_SET, CLK_CFG_21_CLR, /* sta/set/clr */
		16 /* lsb */, 2 /* width */, 23 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_SFLASH_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MCU_ACP_SEL /* dts */,
		"mcu_acp_sel", mcu_acp_parents /* parents*/,
		CLK_CFG_21, CLK_CFG_21_SET, CLK_CFG_21_CLR, /* sta/set/clr */
		24 /* lsb */, 3 /* width */, 31 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_MCU_ACP_SHIFT /* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_TL_CK_SEL /* dts */,
		"tl_ck_sel", tl_ck_parents /* parents*/,
		CLK_CFG_22, CLK_CFG_22_SET, CLK_CFG_22_CLR, /* sta/set/clr */
		0 /* lsb */, 2 /* width */, 7 /* pdn */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_TL_CK_SHIFT /* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MFG_SEL_0 /* dts */,
		"mfg_sel_0", mfg_0_parents /* parents*/,
		CLK_CFG_30, CLK_CFG_30_SET, CLK_CFG_30_CLR, /* sta/set/clr */
		16 /* lsb */, 1 /* width */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_DUMMY_SHIFT /* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MFG_SEL_1 /* dts */,
		"mfg_sel_1", mfg_1_parents /* parents*/,
		CLK_CFG_30, CLK_CFG_30_SET, CLK_CFG_30_CLR, /* sta/set/clr */
		17 /* lsb */, 1 /* width */,
		CLK_CFG_UPDATE_2 /* upd ofs */,
		TOP_MUX_DUMMY_SHIFT /* upd shift */),
};

static const struct mtk_composite top_composites[] = {
	MUX(CLK_TOP_APLL_I2S0_MCK_SEL /* dts */,
		"apll_i2s0_mck_sel",
		apll_i2s0_mck_parents /* parents */,
		CLK_AUDDIV_0, 16, 1 /* ofs/lsb/width */),
	MUX(CLK_TOP_APLL_I2S1_MCK_SEL /* dts */,
		"apll_i2s1_mck_sel",
		apll_i2s1_mck_parents /* parents */,
		CLK_AUDDIV_0, 17, 1 /* ofs/lsb/width */),
	MUX(CLK_TOP_APLL_I2S2_MCK_SEL /* dts */,
		"apll_i2s2_mck_sel",
		apll_i2s2_mck_parents /* parents */,
		CLK_AUDDIV_0, 18, 1 /* ofs/lsb/width */),
	MUX(CLK_TOP_APLL_I2S3_MCK_SEL /* dts */,
		"apll_i2s3_mck_sel",
		apll_i2s3_mck_parents /* parents */,
		CLK_AUDDIV_0, 19, 1 /* ofs/lsb/width */),
	MUX(CLK_TOP_APLL_I2S4_MCK_SEL /* dts */,
		"apll_i2s4_mck_sel",
		apll_i2s4_mck_parents /* parents */,
		CLK_AUDDIV_0, 20, 1 /* ofs/lsb/width */),
	MUX(CLK_TOP_APLL_I2S5_MCK_SEL /* dts */,
		"apll_i2s5_mck_sel",
		apll_i2s5_mck_parents /* parents */,
		CLK_AUDDIV_0, 21, 1 /* ofs/lsb/width */),
	MUX(CLK_TOP_APLL_I2S6_MCK_SEL /* dts */,
		"apll_i2s6_mck_sel",
		apll_i2s6_mck_parents /* parents */,
		CLK_AUDDIV_0, 22, 1 /* ofs/lsb/width */),
	MUX(CLK_TOP_APLL_I2S7_MCK_SEL /* dts */,
		"apll_i2s7_mck_sel",
		apll_i2s7_mck_parents /* parents */,
		CLK_AUDDIV_0, 23, 1 /* ofs/lsb/width */),
	MUX(CLK_TOP_APLL_I2S8_MCK_SEL /* dts */,
		"apll_i2s8_mck_sel",
		apll_i2s8_mck_parents /* parents */,
		CLK_AUDDIV_0, 24, 1 /* ofs/lsb/width */),
	MUX(CLK_TOP_APLL_I2S9_MCK_SEL /* dts */,
		"apll_i2s9_mck_sel",
		apll_i2s9_mck_parents /* parents */,
		CLK_AUDDIV_0, 25, 1 /* ofs/lsb/width */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV0 /* dts */,
		"apll12_ck_div0", "apll_i2s0_mck_sel" /* parent */,
		0x0320, 0 /* pdn_ofs/pdb_bit */,
		CLK_AUDDIV_2, 8, 0 /* ofs/lsb/width */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV1 /* dts */,
		"apll12_ck_div1", "apll_i2s1_mck_sel" /* parent */,
		0x0320, 1 /* pdn_ofs/pdb_bit */,
		CLK_AUDDIV_2, 8, 8 /* ofs/lsb/width */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV2 /* dts */,
		"apll12_ck_div2", "apll_i2s2_mck_sel" /* parent */,
		0x0320, 2 /* pdn_ofs/pdb_bit */,
		CLK_AUDDIV_2, 8, 16 /* ofs/lsb/width */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV3 /* dts */,
		"apll12_ck_div3", "apll_i2s3_mck_sel" /* parent */,
		0x0320, 3 /* pdn_ofs/pdb_bit */,
		CLK_AUDDIV_2, 8, 24 /* ofs/lsb/width */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV4 /* dts */,
		"apll12_ck_div4", "apll_i2s4_mck_sel" /* parent */,
		0x0320, 4 /* pdn_ofs/pdb_bit */,
		CLK_AUDDIV_3, 8, 0 /* ofs/lsb/width */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV5 /* dts */,
		"apll12_ck_div5", "apll_i2s5_mck_sel" /* parent */,
		0x0320, 6 /* pdn_ofs/pdb_bit */,
		CLK_AUDDIV_3, 8, 16 /* ofs/lsb/width */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV6 /* dts */,
		"apll12_ck_div6", "apll_i2s6_mck_sel" /* parent */,
		0x0320, 7 /* pdn_ofs/pdb_bit */,
		CLK_AUDDIV_3, 8, 24 /* ofs/lsb/width */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV7 /* dts */,
		"apll12_ck_div7", "apll_i2s7_mck_sel" /* parent */,
		0x0320, 8 /* pdn_ofs/pdb_bit */,
		CLK_AUDDIV_4, 8, 0 /* ofs/lsb/width */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV8 /* dts */,
		"apll12_ck_div8", "apll_i2s8_mck_sel" /* parent */,
		0x0320, 9 /* pdn_ofs/pdb_bit */,
		CLK_AUDDIV_4, 8, 8 /* ofs/lsb/width */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV9 /* dts */,
		"apll12_ck_div9", "apll_i2s9_mck_sel" /* parent */,
		0x0320, 10 /* pdn_ofs/pdb_bit */,
		CLK_AUDDIV_4, 8, 16 /* ofs/lsb/width */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIVB /* dts */,
		"apll12_ck_divb", "apll12_ck_div4" /* parent */,
		0x0320, 5 /* pdn_ofs/pdb_bit */,
		CLK_AUDDIV_3, 8, 8 /* ofs/lsb/width */),
};

static const struct mtk_gate_regs ifr_ao_2_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x84,
	.sta_ofs = 0x90,
};

static const struct mtk_gate_regs ifr_ao_3_cg_regs = {
	.set_ofs = 0x88,
	.clr_ofs = 0x8C,
	.sta_ofs = 0x94,
};

static const struct mtk_gate_regs ifr_ao_4_cg_regs = {
	.set_ofs = 0xA4,
	.clr_ofs = 0xA8,
	.sta_ofs = 0xAC,
};

static const struct mtk_gate_regs ifr_ao_5_cg_regs = {
	.set_ofs = 0xC0,
	.clr_ofs = 0xC4,
	.sta_ofs = 0xC8,
};

static const struct mtk_gate_regs ifr_ao_6_cg_regs = {
	.set_ofs = 0xE0,
	.clr_ofs = 0xE4,
	.sta_ofs = 0xE8,
};

static const struct mtk_gate_regs peri_ao_0_cg_regs = {
	.set_ofs = 0x3c,
	.clr_ofs = 0x3c,
	.sta_ofs = 0x3c,
};

static const struct mtk_gate_regs peri_ao_1_cg_regs = {
	.set_ofs = 0x40,
	.clr_ofs = 0x40,
	.sta_ofs = 0x40,
};

static const struct mtk_gate_regs peri_ao_2_cg_regs = {
	.set_ofs = 0x44,
	.clr_ofs = 0x44,
	.sta_ofs = 0x44,
};

static const struct mtk_gate_regs top_1_cg_regs = {
	.set_ofs = 0x320,
	.clr_ofs = 0x320,
	.sta_ofs = 0x320,
};

#define GATE_IFR_AO_0(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &ifr_ao_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}
#define GATE_IFR_AO_1(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &ifr_ao_1_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}
#define GATE_IFR_AO_2(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &ifr_ao_2_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}
#define GATE_IFR_AO_3(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &ifr_ao_3_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}
#define GATE_IFR_AO_4(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &ifr_ao_4_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}
#define GATE_IFR_AO_5(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &ifr_ao_5_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}
#define GATE_IFR_AO_6(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &ifr_ao_6_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}
#define GATE_PERI_AO_0(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &peri_ao_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_no_setclr,			\
	}
#define GATE_PERI_AO_1(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &peri_ao_1_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_no_setclr,			\
	}
#define GATE_PERI_AO_2(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &peri_ao_2_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_no_setclr,			\
	}

#define GATE_TOP_1(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &top_1_cg_regs,					\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_no_setclr,			\
	}
static struct mtk_gate ifr_ao_clks[] = {
	GATE_IFR_AO_2(CLK_IFR_AO_THERM /* CLK ID */,
		"ifr_ao_therm" /* name */,
		"axi_ck" /* parent */, 10 /* bit */),
	GATE_IFR_AO_2(CLK_IFR_AO_CQ_DMA_FPC /* CLK ID */,
		"ifr_ao_dma" /* name */,
		"axi_ck" /* parent */, 28 /* bit */),
	GATE_IFR_AO_3(CLK_IFR_AO_TRNG /* CLK ID */,
		"ifr_ao_trng" /* name */,
		"axi_ck" /* parent */, 9 /* bit */),
	GATE_IFR_AO_3(CLK_IFR_AO_CPUM /* CLK ID */,
		"ifr_ao_cpum" /* name */,
		"axi_ck" /* parent */, 11 /* bit */),
	GATE_IFR_AO_3(CLK_IFR_AO_CCIF1_AP /* CLK ID */,
		"ifr_ao_ccif1_ap" /* name */,
		"axi_ck" /* parent */, 12 /* bit */),
	GATE_IFR_AO_3(CLK_IFR_AO_CCIF1_MD /* CLK ID */,
		"ifr_ao_ccif1_md" /* name */,
		"axi_ck" /* parent */, 13 /* bit */),
	GATE_IFR_AO_3(CLK_IFR_AO_CCIF_AP /* CLK ID */,
		"ifr_ao_ccif_ap" /* name */,
		"axi_ck" /* parent */, 23 /* bit */),
	GATE_IFR_AO_3(CLK_IFR_AO_DEBUGSYS /* CLK ID */,
		"ifr_ao_debugsys" /* name */,
		"axi_ck" /* parent */, 24 /* bit */),
	GATE_IFR_AO_3(CLK_IFR_AO_CCIF_MD /* CLK ID */,
		"ifr_ao_ccif_md" /* name */,
		"axi_ck" /* parent */, 26 /* bit */),
	GATE_IFR_AO_3(CLK_IFR_AO_DXCC_SEC_CORE /* CLK ID */,
		"ifr_ao_secore" /* name */,
		"dxcc_ck" /* parent */, 27 /* bit */),
	GATE_IFR_AO_3(CLK_IFR_AO_DXCC_AO /* CLK ID */,
		"ifr_ao_dxcc_ao" /* name */,
		"dxcc_ck" /* parent */, 28 /* bit */),
	GATE_IFR_AO_3(CLK_IFR_AO_DBG_TRACE /* CLK ID */,
		"ifr_ao_dbg_trace" /* name */,
		"axi_ck" /* parent */, 29 /* bit */),
	GATE_IFR_AO_4(CLK_IFR_AO_CLDMA_BCLK /* CLK ID */,
		"ifr_ao_cldmabclk" /* name */,
		"axi_ck" /* parent */, 3 /* bit */),
	GATE_IFR_AO_4(CLK_IFR_AO_CQ_DMA /* CLK ID */,
		"ifr_ao_cq_dma" /* name */,
		"axi_ck" /* parent */, 27 /* bit */),
	GATE_IFR_AO_5(CLK_IFR_AO_CCIF5_AP /* CLK ID */,
		"ifr_ao_ccif5_ap" /* name */,
		"axi_ck" /* parent */, 9 /* bit */),
	GATE_IFR_AO_5(CLK_IFR_AO_CCIF5_MD /* CLK ID */,
		"ifr_ao_ccif5_md" /* name */,
		"axi_ck" /* parent */, 10 /* bit */),
	GATE_IFR_AO_5(CLK_IFR_AO_CCIF2_AP /* CLK ID */,
		"ifr_ao_ccif2_ap" /* name */,
		"axi_ck" /* parent */, 16 /* bit */),
	GATE_IFR_AO_5(CLK_IFR_AO_CCIF2_MD /* CLK ID */,
		"ifr_ao_ccif2_md" /* name */,
		"axi_ck" /* parent */, 17 /* bit */),
	GATE_IFR_AO_5(CLK_IFR_AO_CCIF3_AP /* CLK ID */,
		"ifr_ao_ccif3_ap" /* name */,
		"axi_ck" /* parent */, 18 /* bit */),
	GATE_IFR_AO_5(CLK_IFR_AO_CCIF3_MD /* CLK ID */,
		"ifr_ao_ccif3_md" /* name */,
		"axi_ck" /* parent */, 19 /* bit */),
	GATE_IFR_AO_5(CLK_IFR_AO_FBIST2FPC /* CLK ID */,
		"ifr_ao_fbist2fpc" /* name */,
		"msdc_macro_ck" /* parent */, 24 /* bit */),
	GATE_IFR_AO_5(CLK_IFR_AO_DEVICE_APC_SYNC /* CLK ID */,
		"ifr_ao_dapc_sync" /* name */,
		"axi_ck" /* parent */, 25 /* bit */),
	GATE_IFR_AO_5(CLK_IFR_AO_DPMAIF_MAIN /* CLK ID */,
		"ifr_ao_dpmaif_main" /* name */,
		"dpmaif_main_ck" /* parent */, 26 /* bit */),
	GATE_IFR_AO_5(CLK_IFR_AO_CCIF4_AP /* CLK ID */,
		"ifr_ao_ccif4_ap" /* name */,
		"axi_ck" /* parent */, 28 /* bit */),
	GATE_IFR_AO_5(CLK_IFR_AO_CCIF4_MD /* CLK ID */,
		"ifr_ao_ccif4_md" /* name */,
		"axi_ck" /* parent */, 29 /* bit */),
	GATE_IFR_AO_6(CLK_IFR_AO_RG_MMW_DPMAIF_F26M_CK /* CLK ID */,
		"ifr_ao_dpmaif_26m" /* name */,
		"f26m_ck" /* parent */, 17 /* bit */),
	GATE_IFR_AO_6(CLK_IFR_AO_RG_HF_FMEM_SUB_CK /* CLK ID */,
		"ifr_ao_rg_hf_fmem_sub_ck" /* name */,
		"mem_sub_ck" /* parent */, 29 /* bit */),
};

static struct mtk_gate peri_ao_clks[] = {
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_UART0 /* CLK ID */,
		"peri_ao_peri_uart0" /* name */,
		"fuart_ck" /* parent */, 0 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_UART1 /* CLK ID */,
		"peri_ao_peri_uart1" /* name */,
		"fuart_ck" /* parent */, 1 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_UART2 /* CLK ID */,
		"peri_ao_peri_uart2" /* name */,
		"fuart_ck" /* parent */, 2 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_UART3 /* CLK ID */,
		"peri_ao_peri_uart3" /* name */,
		"fuart_ck" /* parent */, 3 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_PWM_HCLK /* CLK ID */,
		"peri_ao_peri_pwm_hclk" /* name */,
		"fperi_hd_axi_ck" /* parent */, 8 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_PWM_BCLK /* CLK ID */,
		"peri_ao_peri_pwm_bclk" /* name */,
		"pwm_ck" /* parent */, 9 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_PWM_FBCLK1 /* CLK ID */,
		"peri_ao_peri_pwm_fbclk1" /* name */,
		"pwm_ck" /* parent */, 10 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_PWM_FBCLK2 /* CLK ID */,
		"peri_ao_peri_pwm_fbclk2" /* name */,
		"pwm_ck" /* parent */, 11 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_PWM_FBCLK3 /* CLK ID */,
		"peri_ao_peri_pwm_fbclk3" /* name */,
		"pwm_ck" /* parent */, 12 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_PWM_FBCLK4 /* CLK ID */,
		"peri_ao_peri_pwm_fbclk4" /* name */,
		"pwm_ck" /* parent */, 13 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_BTIF_BCLK /* CLK ID */,
		"peri_ao_peri_btif_bclk" /* name */,
		"fperi_hd_axi_ck" /* parent */, 14 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_DISP_PWM0 /* CLK ID */,
		"peri_ao_peri_disp_pwm0" /* name */,
		"fdisp_pwm_ck" /* parent */, 15 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_DISP_PWM1 /* CLK ID */,
		"peri_ao_peri_disp_pwm1" /* name */,
		"fdisp_pwm_ck" /* parent */, 16 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_SPI0_BCLK /* CLK ID */,
		"peri_ao_peri_spi0_bclk" /* name */,
		"spi_ck" /* parent */, 17 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_SPI1_BCLK /* CLK ID */,
		"peri_ao_peri_spi1_bclk" /* name */,
		"spi_ck" /* parent */, 18 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_SPI2_BCLK /* CLK ID */,
		"peri_ao_peri_spi2_bclk" /* name */,
		"spi_ck" /* parent */, 19 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_SPI3_BCLK /* CLK ID */,
		"peri_ao_peri_spi3_bclk" /* name */,
		"spi_ck" /* parent */, 20 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_SPI4_BCLK /* CLK ID */,
		"peri_ao_peri_spi4_bclk" /* name */,
		"spi_ck" /* parent */, 21 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_SPI5_BCLK /* CLK ID */,
		"peri_ao_peri_spi5_bclk" /* name */,
		"spi_ck" /* parent */, 22 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_SPI6_BCLK /* CLK ID */,
		"peri_ao_peri_spi6_bclk" /* name */,
		"spi_ck" /* parent */, 23 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_SPI7_BCLK /* CLK ID */,
		"peri_ao_peri_spi7_bclk" /* name */,
		"spi_ck" /* parent */, 24 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_SPI0_HCLK /* CLK ID */,
		"peri_ao_peri_spi0_hclk" /* name */,
		"fperi_hd_axi_ck" /* parent */, 25 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_SPI1_HCLK /* CLK ID */,
		"peri_ao_peri_spi1_hclk" /* name */,
		"fperi_hd_axi_ck" /* parent */, 26 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_SPI2_HCLK /* CLK ID */,
		"peri_ao_peri_spi2_hclk" /* name */,
		"fperi_hd_axi_ck" /* parent */, 27 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_SPI3_HCLK /* CLK ID */,
		"peri_ao_peri_spi3_hclk" /* name */,
		"fperi_hd_axi_ck" /* parent */, 28 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_SPI4_HCLK /* CLK ID */,
		"peri_ao_peri_spi4_hclk" /* name */,
		"fperi_hd_axi_ck" /* parent */, 29 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_SPI5_HCLK /* CLK ID */,
		"peri_ao_peri_spi5_hclk" /* name */,
		"fperi_hd_axi_ck" /* parent */, 30 /* bit */),
	GATE_PERI_AO_0(CLK_PERI_AO_PERI_SPI6_HCLK /* CLK ID */,
		"peri_ao_peri_spi6_hclk" /* name */,
		"fperi_hd_axi_ck" /* parent */, 31 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_SPI7_HCLK /* CLK ID */,
		"peri_ao_peri_spi7_hclk" /* name */,
		"fperi_hd_axi_ck" /* parent */, 0 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_FSFLASH /* CLK ID */,
		"peri_ao_peri_fsflash" /* name */,
		"sflash_ck" /* parent */, 1 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_FSFLASH_FCLK /* CLK ID */,
		"peri_ao_peri_fsflash_fclk" /* name */,
		"fperi_hd_axi_ck" /* parent */, 2 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_FSFLASH_HCLK /* CLK ID */,
		"peri_ao_peri_fsflash_hclk" /* name */,
		"fperi_hd_axi_ck" /* parent */, 3 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_IIC /* CLK ID */,
		"peri_ao_peri_iic" /* name */,
		"fperi_hd_axi_ck" /* parent */, 4 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_APDMA /* CLK ID */,
		"peri_ao_peri_apdma" /* name */,
		"fperi_hd_axi_ck" /* parent */, 5 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_SSUSB_PCLK /* CLK ID */,
		"peri_ao_peri_ssusb_pclk" /* name */,
		"fssusb_xhci_ck" /* parent */, 6 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_SSUSB_REF /* CLK ID */,
		"peri_ao_peri_ssusb_ref" /* name */,
		"fssusb_xhci_ck" /* parent */, 7 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_SSUSB_FRMCNT /* CLK ID */,
		"peri_ao_peri_ssusb_frmcnt" /* name */,
		"fssusb_xhci_ck" /* parent */, 8 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_SSUSB_PHY /* CLK ID */,
		"peri_ao_peri_ssusb_phy" /* name */,
		"fssusb_xhci_ck" /* parent */, 9 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_SSUSB_SYS /* CLK ID */,
		"peri_ao_peri_ssusb_sys" /* name */,
		"fusb_top_ck" /* parent */, 10 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_SSUSB_XHCI /* CLK ID */,
		"peri_ao_peri_ssusb_xhci" /* name */,
		"fssusb_xhci_ck" /* parent */, 11 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_SSUSB_DMA_BUS /* CLK ID */,
		"peri_ao_peri_ssusb_dma_bus" /* name */,
		"fperi_hd_axi_ck" /* parent */, 12 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_SSUSB_MCU_BUS /* CLK ID */,
		"peri_ao_peri_ssusb_mcu_bus" /* name */,
		"fperi_hd_axi_ck" /* parent */, 13 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_SSUSB1_REF /* CLK ID */,
		"peri_ao_peri_ssusb1_ref" /* name */,
		"fusb_top_1p_ck" /* parent */, 14 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_SSUSB1_FRMCNT /* CLK ID */,
		"peri_ao_peri_ssusb1_frmcnt" /* name */,
		"fssusb_xhci_1p_ck" /* parent */, 15 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_SSUSB1_PHY /* CLK ID */,
		"peri_ao_peri_ssusb1_phy" /* name */,
		"fusb_top_ck" /* parent */, 16 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_SSUSB1_SYS /* CLK ID */,
		"peri_ao_peri_ssusb1_sys" /* name */,
		"fusb_top_1p_ck" /* parent */, 17 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_SSUSB1_XHCI /* CLK ID */,
		"peri_ao_peri_ssusb1_xhci" /* name */,
		"fssusb_xhci_1p_ck" /* parent */, 18 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_SSUSB1_DMA_BUS /* CLK ID */,
		"peri_ao_peri_ssusb1_dma_bus" /* name */,
		"fperi_hd_axi_ck" /* parent */, 19 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_SSUSB1_MCU_BUS /* CLK ID */,
		"peri_ao_peri_ssusb1_mcu_bus" /* name */,
		"fperi_hd_axi_ck" /* parent */, 20 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_MSDC1_S /* CLK ID */,
		"peri_ao_peri_msdc1_s" /* name */,
		"msdc30_1_ck" /* parent */, 21 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_MSDC1_H /* CLK ID */,
		"peri_ao_peri_msdc1_h" /* name */,
		"fperi_hd_axi_ck" /* parent */, 22 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_MSDC2_S /* CLK ID */,
		"peri_ao_peri_msdc2_s" /* name */,
		"msdc30_2_ck" /* parent */, 23 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_MSDC2_H /* CLK ID */,
		"peri_ao_peri_msdc2_h" /* name */,
		"fperi_hd_axi_ck" /* parent */, 24 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_PCIE0_26M /* CLK ID */,
		"peri_ao_peri_pcie0_26m" /* name */,
		"f26m_ck" /* parent */, 25 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_PCIE0_250M /* CLK ID */,
		"peri_ao_peri_pcie0_250m" /* name */,
		"f26m_ck" /* parent */, 26 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_PCIE0_GFMUX /* CLK ID */,
		"peri_ao_peri_pcie0_gfmux" /* name */,
		"tl_ck" /* parent */, 27 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_PCIE1_26M /* CLK ID */,
		"peri_ao_peri_pcie1_26m" /* name */,
		"f26m_ck" /* parent */, 28 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_PCIE1_250M /* CLK ID */,
		"peri_ao_peri_pcie1_250m" /* name */,
		"f26m_ck" /* parent */, 29 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_PCIE1_GFMUX /* CLK ID */,
		"peri_ao_peri_pcie1_gfmux" /* name */,
		"tl_ck" /* parent */, 30 /* bit */),
	GATE_PERI_AO_1(CLK_PERI_AO_PERI_PCIE0_MEM /* CLK ID */,
		"peri_ao_peri_pcie0_mem" /* name */,
		"peri_hf_fmem_sel" /* parent */, 31 /* bit */),
	GATE_PERI_AO_2(CLK_PERI_AO_PERI_PCIE1_MEM_CLR /* CLK ID */,
		"peri_ao_peri_pcie1_mem_clr" /* name */,
		"peri_hf_fmem_sel" /* parent */, 0 /* bit */),
	GATE_PERI_AO_2(CLK_PERI_AO_PERI_PCIE0_PCIE1_HCLK_CLR /* CLK ID */,
		"peri_ao_peri_pcie0_pcie1_hclk_clr" /* name */,
		"fperi_hd_axi_ck" /* parent */, 1 /* bit */),
};

static struct mtk_gate top_clks[] = {
	GATE_TOP_1(CLK_TOP_APLL12_DIV0_PDN /* CLK ID */,
		"top_apll12_div0_pdn" /* name */,
		"aud_1_ck" /* parent */, 0 /* bit */),
	GATE_TOP_1(CLK_TOP_APLL12_DIV1_PDN /* CLK ID */,
		"top_apll12_div1_pdn" /* name */,
		"aud_1_ck" /* parent */, 1 /* bit */),
	GATE_TOP_1(CLK_TOP_APLL12_DIV2_PDN /* CLK ID */,
		"top_apll12_div2_pdn" /* name */,
		"aud_1_ck" /* parent */, 2 /* bit */),
	GATE_TOP_1(CLK_TOP_APLL12_DIV3_PDN /* CLK ID */,
		"top_apll12_div3_pdn" /* name */,
		"aud_1_ck" /* parent */, 3 /* bit */),
	GATE_TOP_1(CLK_TOP_APLL12_DIV4_PDN /* CLK ID */,
		"top_apll12_div4_pdn" /* name */,
		"aud_1_ck" /* parent */, 4 /* bit */),
	GATE_TOP_1(CLK_TOP_APLL12_DIVB_PDN /* CLK ID */,
		"top_apll12_divb_pdn" /* name */,
		"aud_1_ck" /* parent */, 5 /* bit */),
	GATE_TOP_1(CLK_TOP_APLL12_DIV5_PDN /* CLK ID */,
		"top_apll12_div5_pdn" /* name */,
		"aud_1_ck" /* parent */, 6 /* bit */),
	GATE_TOP_1(CLK_TOP_APLL12_DIV6_PDN /* CLK ID */,
		"top_apll12_div6_pdn" /* name */,
		"aud_1_ck" /* parent */, 7 /* bit */),
	GATE_TOP_1(CLK_TOP_APLL12_DIV7_PDN /* CLK ID */,
		"top_apll12_div7_pdn" /* name */,
		"aud_1_ck" /* parent */, 8 /* bit */),
	GATE_TOP_1(CLK_TOP_APLL12_DIV8_PDN /* CLK ID */,
		"top_apll12_div8_pdn" /* name */,
		"aud_1_ck" /* parent */, 9 /* bit */),
	GATE_TOP_1(CLK_TOP_APLL12_DIV9_PDN /* CLK ID */,
		"top_apll12_div9_pdn" /* name */,
		"aud_1_ck" /* parent */, 10 /* bit */),
};

#define PLL_FMAX		(3800UL * MHZ)
#define PLL_FMIN		(1500UL * MHZ)
#define PLL_INTEGER_BITS	8

#if MT_CCF_PLL_DISABLE
#define PLL_CFLAGS		PLL_AO
#else
#define PLL_CFLAGS		(0)
#endif

#define PLL(_id, _name, _reg, _en_reg, _div_en_msk, _pll_en_bit,	\
			_pwr_reg, _flags, _rst_bar_mask,		\
			_pd_reg, _pd_shift, _tuner_reg,			\
			_tuner_en_reg, _tuner_en_bit,			\
			_pcw_reg, _pcw_shift, _pcwbits) {		\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.en_reg = _en_reg,					\
		.en_mask = _div_en_msk,					\
		.pll_en_bit = _pll_en_bit,				\
		.pwr_reg = _pwr_reg,					\
		.flags = (_flags | PLL_CFLAGS),				\
		.rst_bar_mask = _rst_bar_mask,				\
		.fmax = PLL_FMAX,					\
		.fmin = PLL_FMIN,					\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,				\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = PLL_INTEGER_BITS,				\
	}

#define PLL_HV(_id, _name, _reg, _en_reg, _div_en_msk, _pll_en_bit,	\
			_pwr_reg, _flags, _rst_bar_mask,		\
			_pd_reg, _pd_shift, _tuner_reg,			\
			_tuner_en_reg, _tuner_en_bit,			\
			_pcw_reg, _pcw_shift, _pcwbits,	\
			_hwv_set_ofs, _hwv_clr_ofs, _hwv_sta_ofs,	\
			_hwv_done_ofs, _hwv_shift) {		\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.en_reg = _en_reg,					\
		.en_mask = _div_en_msk,					\
		.pll_en_bit = _pll_en_bit,				\
		.pwr_reg = _pwr_reg,					\
		.flags = (_flags | PLL_CFLAGS | CLK_USE_HW_VOTER),	\
		.rst_bar_mask = _rst_bar_mask,				\
		.fmax = PLL_FMAX,					\
		.fmin = PLL_FMIN,					\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,				\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = PLL_INTEGER_BITS,				\
		.hwv_set_ofs = _hwv_set_ofs,	\
		.hwv_clr_ofs = _hwv_clr_ofs,	\
		.hwv_sta_ofs = _hwv_sta_ofs,	\
		.hwv_done_ofs = _hwv_done_ofs,	\
		.hwv_shift = _hwv_shift,	\
	}

static const struct mtk_pll_data apmixedsys_plls[] = {
	PLL(CLK_APMIXEDSYS_MAINPLL /* dts */,
		"mainpll" /* name */,
		MAINPLL_CON0 /* base */,
		MAINPLL_CON0, 0xff000000, 0 /* en_reg/div_en_msk/en_bit */,
		MAINPLL_CON3, HAVE_RST_BAR | PLL_AO, BIT(23) /* pwr_reg/flag/rstb_bit */,
		MAINPLL_CON1, 24 /* post div reg/bit */,
		0, 0, 0 /* tuner */,
		MAINPLL_CON1, 0, 22 /* pcw */),
	PLL_HV(CLK_APMIXEDSYS_UNIVPLL /* dts */,
		"univpll" /* name */,
		UNIVPLL_CON0 /* base */,
		UNIVPLL_CON0, 0xff000000, 0 /* en_reg/div_en_msk/en_bit */,
		UNIVPLL_CON3, HAVE_RST_BAR, BIT(23) /* pwr_reg/flag/rstb_bit */,
		UNIVPLL_CON1, 24 /* post div reg/bit */,
		0, 0, 0 /* tuner */,
		UNIVPLL_CON1, 0, 22 /* pcw */,
		HWV_PLL_SET, HWV_PLL_CLR, HWV_PLL_EN, HWV_PLL_DONE, 1),
	PLL(CLK_APMIXEDSYS_MSDCPLL /* dts */,
		"msdcpll" /* name */,
		MSDCPLL_CON0 /* base */,
		MSDCPLL_CON0, 0x00000000, 0 /* en_reg/div_en_msk/en_bit */,
		MSDCPLL_CON3, 0, BIT(0) /* pwr_reg/flag/rstb_bit */,
		MSDCPLL_CON1, 24 /* post div reg/bit */,
		0, 0, 0 /* tuner */,
		MSDCPLL_CON1, 0, 22 /* pcw */),
	PLL_HV(CLK_APMIXEDSYS_MMPLL /* dts */,
		"mmpll" /* name */,
		MMPLL_CON0 /* base */,
		MMPLL_CON0, 0xff000000, 0 /* en_reg/div_en_msk/en_bit */,
		MMPLL_CON3, HAVE_RST_BAR, BIT(23) /* pwr_reg/flag/rstb_bit */,
		MMPLL_CON1, 24 /* post div reg/bit */,
		0, 0, 0 /* tuner */,
		MMPLL_CON1, 0, 22 /* pcw */,
		HWV_PLL_SET, HWV_PLL_CLR, HWV_PLL_EN, HWV_PLL_DONE, 3),
	PLL(CLK_APMIXEDSYS_ADSPPLL /* dts */,
		"adsppll" /* name */,
		ADSPPLL_CON0 /* base */,
		ADSPPLL_CON0, 0x00000000, 0 /* en_reg/div_en_msk/en_bit */,
		ADSPPLL_CON3, 0, BIT(0) /* pwr_reg/flag/rstb_bit */,
		ADSPPLL_CON1, 24 /* post div reg/bit */,
		0, 0, 0 /* tuner */,
		ADSPPLL_CON1, 0, 22 /* pcw */),
	PLL(CLK_APMIXEDSYS_TVDPLL /* dts */,
		"tvdpll" /* name */,
		TVDPLL_CON0 /* base */,
		TVDPLL_CON0, 0x00000000, 0 /* en_reg/div_en_msk/en_bit */,
		TVDPLL_CON3, 0, BIT(0) /* pwr_reg/flag/rstb_bit */,
		TVDPLL_CON1, 24 /* post div reg/bit */,
		0, 0, 0 /* tuner */,
		TVDPLL_CON1, 0, 22 /* pcw */),
	PLL(CLK_APMIXEDSYS_APLL1 /* dts */,
		"apll1" /* name */,
		APLL1_CON0 /* base */,
		APLL1_CON0, 0x00000000, 0 /* en_reg/div_en_msk/en_bit */,
		APLL1_CON4, 0, BIT(0) /* pwr_reg/flag/rstb_bit */,
		APLL1_CON1, 24 /* post div reg/bit */,
		APLL1_TUNER_CON0, AP_PLL_CON3, 0 /* tuner */,
		APLL1_CON2, 0, 32 /* pcw */),
	PLL(CLK_APMIXEDSYS_APLL2 /* dts */,
		"apll2" /* name */,
		APLL2_CON0 /* base */,
		APLL2_CON0, 0x00000000, 0 /* en_reg/div_en_msk/en_bit */,
		APLL2_CON4, 0, BIT(0) /* pwr_reg/flag/rstb_bit */,
		APLL2_CON1, 24 /* post div reg/bit */,
		APLL2_TUNER_CON0, AP_PLL_CON3, 5 /* tuner */,
		APLL2_CON2, 0, 32 /* pcw */),
	PLL(CLK_APMIXEDSYS_MPLL, "mpll", MPLL_CON0/*base*/,
		MPLL_CON0, 0, 0/*en*/,
		MPLL_CON3/*pwr*/, PLL_AO, BIT(0)/*rstb*/,
		MPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MPLL_CON1, 0, 22/*pcw*/),
	PLL_HV(CLK_APMIXEDSYS_IMGPLL /* dts */,
		"imgpll" /* name */,
		IMGPLL_CON0 /* base */,
		IMGPLL_CON0, 0x00000000, 0 /* en_reg/div_en_msk/en_bit */,
		IMGPLL_CON3, 0, BIT(0) /* pwr_reg/flag/rstb_bit */,
		IMGPLL_CON1, 24 /* post div reg/bit */,
		0, 0, 0 /* tuner */,
		IMGPLL_CON1, 0, 22 /* pcw */,
		HWV_PLL_SET, HWV_PLL_CLR, HWV_PLL_EN, HWV_PLL_DONE, 10),
};


static int clk_mt6983_apmixedsys_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int ret = 0;
	struct device_node *node = pdev->dev.of_node;
	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_APMIXEDSYS_NR_CLK);

	mtk_clk_register_plls(node, apmixedsys_plls,
			ARRAY_SIZE(apmixedsys_plls), clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret)
		pr_notice("%s(): could not register clock provider: %d\n",
				__func__, ret);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif /* MT_CCF_BRINGUP */

	return ret;
}

static const struct mtk_pll_data mfg_ao_plls[] = {
	PLL(CLK_MFG_MFGPLL, "mfgpll", MFGPLL_CON0/*base*/,
		MFGPLL_CON0, 0, 0/*en*/,
		MFGPLL_CON3/*pwr*/, 0, BIT(0)/*rstb*/,
		MFGPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MFGPLL_CON1, 0, 22/*pcw*/),
	PLL(CLK_MFG_MFGSCPLL, "mfgscpll", MFGSCPLL_CON0/*base*/,
		MFGSCPLL_CON0, 0, 0/*en*/,
		MFGSCPLL_CON3/*pwr*/, 0, BIT(0)/*rstb*/,
		MFGSCPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MFGSCPLL_CON1, 0, 22/*pcw*/),
};


static int clk_mt6983_mfg_ao_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int ret = 0;
	struct device_node *node = pdev->dev.of_node;
	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_MFG_NR_CLK);

	mtk_clk_register_plls(node, mfg_ao_plls,
			ARRAY_SIZE(mfg_ao_plls), clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret)
		pr_notice("%s(): could not register clock provider: %d\n",
				__func__, ret);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif /* MT_CCF_BRINGUP */

	return ret;
}

static const struct mtk_pll_data apu_ao_plls[] = {
	PLL(CLK_APU_APUPLL, "apu_ao_apupll", APUPLL_CON0/*base*/,
		APUPLL_CON0, 0, 0/*en*/,
		APUPLL_CON3/*pwr*/, 0, BIT(0)/*rstb*/,
		APUPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		APUPLL_CON1, 0, 22/*pcw*/),
	PLL(CLK_APU_NPUPLL, "apu_ao_npupll", NPUPLL_CON0/*base*/,
		NPUPLL_CON0, 0, 0/*en*/,
		NPUPLL_CON3/*pwr*/, 0, BIT(0)/*rstb*/,
		NPUPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		NPUPLL_CON1, 0, 22/*pcw*/),
	PLL(CLK_APU_APUPLL1, "apu_ao_apupll1", APUPLL1_CON0/*base*/,
		APUPLL1_CON0, 0, 0/*en*/,
		APUPLL1_CON3/*pwr*/, 0, BIT(0)/*rstb*/,
		APUPLL1_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		APUPLL1_CON1, 0, 22/*pcw*/),
	PLL(CLK_APU_APUPLL2, "apu_ao_apupll2", APUPLL2_CON0/*base*/,
		APUPLL2_CON0, 0, 0/*en*/,
		APUPLL2_CON3/*pwr*/, 0, BIT(0)/*rstb*/,
		APUPLL2_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		APUPLL2_CON1, 0, 22/*pcw*/),
};

static int clk_mt6983_apu_ao_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int ret = 0;
	struct device_node *node = pdev->dev.of_node;
	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_APU_NR_CLK);

	mtk_clk_register_plls(node, apu_ao_plls,
			ARRAY_SIZE(apu_ao_plls), clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret)
		pr_notice("%s(): could not register clock provider: %d\n",
				__func__, ret);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif /* MT_CCF_BRINGUP */

	return ret;
}

static int clk_mt6983_ifr_ao_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int ret = 0;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_IFR_AO_NR_CLK);

	mtk_clk_register_gates(node, ifr_ao_clks,
			ARRAY_SIZE(ifr_ao_clks), clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret)
		pr_notice("%s(): could not register clock provider: %d\n",
				__func__, ret);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif /* MT_CCF_BRINGUP */

	return ret;
}
static int clk_mt6983_peri_ao_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int ret = 0;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_PERI_AO_NR_CLK);

	mtk_clk_register_gates(node, peri_ao_clks,
			ARRAY_SIZE(peri_ao_clks), clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret)
		pr_notice("%s(): could not register clock provider: %d\n",
				__func__, ret);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif /* MT_CCF_BRINGUP */

	return ret;
}

static int clk_mt6983_vlp_cksys_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int ret = 0;
	struct device_node *node = pdev->dev.of_node;
	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_VLP_CKSYS_NR_CLK);

	mtk_clk_register_muxes(vlp_cksys_muxes, ARRAY_SIZE(vlp_cksys_muxes),
			node, &mtk_clk_lock, clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret)
		pr_notice("%s(): could not register clock provider: %d\n",
				__func__, ret);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif /* MT_CCF_BRINGUP */

	return ret;
}
static int clk_mt6983_top_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int ret = 0;
	struct device_node *node = pdev->dev.of_node;
	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_TOP_NR_CLK);

	mtk_clk_register_factors(top_divs,
			ARRAY_SIZE(top_divs), clk_data);

	mtk_clk_register_muxes(top_muxes, ARRAY_SIZE(top_muxes),
			node, &mtk_clk_lock, clk_data);

	mtk_clk_register_composites(top_composites,
			ARRAY_SIZE(top_composites),
			base, &mtk_clk_lock, clk_data);

	mtk_clk_register_gates(node, top_clks,
			ARRAY_SIZE(top_clks), clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret)
		pr_notice("%s(): could not register clock provider: %d\n",
				__func__, ret);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif /* MT_CCF_BRINGUP */

	return ret;
}

static const struct of_device_id of_match_clk_mt6983[] = {
	{
		.compatible = "mediatek,mt6983-apmixedsys",
		.data = clk_mt6983_apmixedsys_probe,
	}, {
		.compatible = "mediatek,mt6983-topckgen",
		.data = clk_mt6983_top_probe,
	}, {
		.compatible = "mediatek,mt6983-infracfg_ao",
		.data = clk_mt6983_ifr_ao_probe,
	}, {
		.compatible = "mediatek,mt6983-pericfg_ao",
		.data = clk_mt6983_peri_ao_probe,
	}, {
		.compatible = "mediatek,mt6983-vlp_cksys",
		.data = clk_mt6983_vlp_cksys_probe,
	}, {
		.compatible = "mediatek,mt6983-mfg_pll_ctrl",
		.data = clk_mt6983_mfg_ao_probe,
	}, {
		.compatible = "mediatek,mt6983-apu_pll_ctrl",
		.data = clk_mt6983_apu_ao_probe,
	}, {
		/* sentinel */
	}
};

static int clk_mt6983_probe(struct platform_device *pdev)
{
	int (*clk_probe)(struct platform_device *pd);
	int r;

	clk_probe = of_device_get_match_data(&pdev->dev);
	if (!clk_probe)
		return -EINVAL;

	r = clk_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static struct platform_driver clk_mt6983_drv = {
	.probe = clk_mt6983_probe,
	.driver = {
		.name = "clk-mt6983",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt6983,
	},
};

static int __init clk_mt6983_init(void)
{
	return platform_driver_register(&clk_mt6983_drv);
}

static void __exit clk_mt6983_exit(void)
{
	platform_driver_unregister(&clk_mt6983_drv);
}

arch_initcall(clk_mt6983_init);
module_exit(clk_mt6983_exit);
MODULE_LICENSE("GPL");

