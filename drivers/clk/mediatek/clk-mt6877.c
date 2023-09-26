/*
 * Copyright (c) 2021 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk-mtk.h"
#include "clk-mux.h"
#include "clk-gate.h"
#include "clk-mt6877.h"

#include <dt-bindings/clock/mt6877-clk.h>
#include <memory/mediatek/dramc.h>

/* bringup config */
#define MT_CCF_BRINGUP		1
#define MT_CCF_MUX_DISABLE	0

/* TOPCK MUX SEL REG */
#define CLK_CFG_UPDATE				0x0004
#define CLK_CFG_UPDATE1				0x0008
#define CLK_CFG_UPDATE2				0x000c
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
#define CLK_CFG_20				0x0120
#define CLK_CFG_20_SET				0x0124
#define CLK_CFG_20_CLR				0x0128
#define CLK_CFG_17				0x0180
#define CLK_CFG_17_SET				0x0184
#define CLK_CFG_17_CLR				0x0188
#define CLK_AUDDIV_0				0x0320

/* TOPCK MUX SHIFT */
#define TOP_MUX_AXI_SHIFT			0
#define TOP_MUX_SPM_SHIFT			1
#define TOP_MUX_SCP_SHIFT			2
#define TOP_MUX_BUS_AXIMEM_SHIFT		3
#define TOP_MUX_DISP0_SHIFT			4
#define TOP_MUX_MDP0_SHIFT			5
#define TOP_MUX_IMG1_SHIFT			6
#define TOP_MUX_IPE_SHIFT			7
#define TOP_MUX_DPE_SHIFT			8
#define TOP_MUX_CAM_SHIFT			9
#define TOP_MUX_CCU_SHIFT			10
#define TOP_MUX_DSP_SHIFT			11
#define TOP_MUX_DSP1_SHIFT			12
#define TOP_MUX_DSP2_SHIFT			13
#define TOP_MUX_DSP4_SHIFT			15
#define TOP_MUX_DSP7_SHIFT			18
#define TOP_MUX_CAMTG_SHIFT			19
#define TOP_MUX_CAMTG2_SHIFT			20
#define TOP_MUX_CAMTG3_SHIFT			21
#define TOP_MUX_CAMTG4_SHIFT			22
#define TOP_MUX_CAMTG5_SHIFT			23
#define TOP_MUX_UART_SHIFT			24
#define TOP_MUX_SPI_SHIFT			25
#define TOP_MUX_MSDC50_0_HCLK_SHIFT		26
#define TOP_MUX_MSDC50_0_SHIFT			27
#define TOP_MUX_MSDC30_1_SHIFT			28
#define TOP_MUX_AUDIO_SHIFT			29
#define TOP_MUX_AUD_INTBUS_SHIFT		30
#define TOP_MUX_PWRAP_ULPOSC_SHIFT		0
#define TOP_MUX_ATB_SHIFT			1
#define TOP_MUX_SSPM_SHIFT			2
#define TOP_MUX_DISP_PWM_SHIFT			4
#define TOP_MUX_USB_TOP_SHIFT			5
#define TOP_MUX_SSUSB_XHCI_SHIFT		6
#define TOP_MUX_I2C_SHIFT			9
#define TOP_MUX_SENINF_SHIFT			10
#define TOP_MUX_SENINF1_SHIFT			11
#define TOP_MUX_SENINF2_SHIFT			12
#define TOP_MUX_SENINF3_SHIFT			13
#define TOP_MUX_DXCC_SHIFT			14
#define TOP_MUX_AUD_ENGEN1_SHIFT		15
#define TOP_MUX_AUD_ENGEN2_SHIFT		16
#define TOP_MUX_AES_UFSFDE_SHIFT		17
#define TOP_MUX_UFS_SHIFT			18
#define TOP_MUX_AUD_1_SHIFT			19
#define TOP_MUX_AUD_2_SHIFT			20
#define TOP_MUX_ADSP_SHIFT			21
#define TOP_MUX_DPMAIF_MAIN_SHIFT		22
#define TOP_MUX_VENC_SHIFT			23
#define TOP_MUX_VDEC_SHIFT			24
#define TOP_MUX_CAMTM_SHIFT			25
#define TOP_MUX_PWM_SHIFT			26
#define TOP_MUX_AUDIO_H_SHIFT			27
#define TOP_MUX_MCUPM_SHIFT			28
#define TOP_MUX_SPMI_M_MST_SHIFT		30
#define TOP_MUX_DVFSRC_SHIFT			0
#define TOP_MUX_MEM_SUB_SHIFT			1
#define TOP_MUX_AES_MSDCFDE_SHIFT		2
#define TOP_MUX_UFS_MBIST_SHIFT			5
#define TOP_MUX_AP2CONN_HOST_SHIFT		6
#define TOP_MUX_MSDC_NEW_RX_SHIFT		7

/* TOPCK DIVIDER REG */
#define CLK_AUDDIV_2				0x0328
#define CLK_AUDDIV_3				0x0334
#define CLK_AUDDIV_4				0x0338

/* APMIXED PLL REG */
#define AP_PLL_CON3				0x00C
#define ARMPLL_LL_CON0				0x208
#define ARMPLL_LL_CON1				0x20C
#define ARMPLL_LL_CON2				0x210
#define ARMPLL_LL_CON3				0x214
#define ARMPLL_BL_CON0				0x218
#define ARMPLL_BL_CON1				0x21C
#define ARMPLL_BL_CON2				0x220
#define ARMPLL_BL_CON3				0x224
#define CCIPLL_CON0				0x238
#define CCIPLL_CON1				0x23C
#define CCIPLL_CON2				0x240
#define CCIPLL_CON3				0x244
#define MAINPLL_CON0				0x350
#define MAINPLL_CON1				0x354
#define MAINPLL_CON2				0x358
#define MAINPLL_CON3				0x35C
#define UNIVPLL_CON0				0x308
#define UNIVPLL_CON1				0x30C
#define UNIVPLL_CON2				0x310
#define UNIVPLL_CON3				0x314
#define MSDCPLL_CON0				0x360
#define MSDCPLL_CON1				0x364
#define MSDCPLL_CON2				0x368
#define MSDCPLL_CON3				0x36C
#define MMPLL_CON0				0x3A0
#define MMPLL_CON1				0x3A4
#define MMPLL_CON2				0x3A8
#define MMPLL_CON3				0x3AC
#define ADSPPLL_CON0				0x380
#define ADSPPLL_CON1				0x384
#define ADSPPLL_CON2				0x388
#define ADSPPLL_CON3				0x38C
#define TVDPLL_CON0				0x248
#define TVDPLL_CON1				0x24C
#define TVDPLL_CON2				0x250
#define TVDPLL_CON3				0x254
#define APLL1_CON0				0x328
#define APLL1_CON1				0x32C
#define APLL1_CON2				0x330
#define APLL1_CON3				0x334
#define APLL1_CON4				0x338
#define APLL2_CON0				0x33C
#define APLL2_CON1				0x340
#define APLL2_CON2				0x344
#define APLL2_CON3				0x348
#define APLL2_CON4				0x34C
#define MPLL_CON0				0x390
#define MPLL_CON1				0x394
#define MPLL_CON2				0x398
#define MPLL_CON3				0x39C
#define USBPLL_CON0				0x318
#define USBPLL_CON1				0x31C
#define USBPLL_CON2				0x320
#define USBPLL_CON3				0x324
#define IMGPLL_CON0				0x370
#define IMGPLL_CON1				0x374
#define IMGPLL_CON2				0x378
#define IMGPLL_CON3				0x37C

static DEFINE_SPINLOCK(mt6877_clk_lock);

static const struct mtk_pll_data *plls_data[PLL_SYS_NUM];
static void __iomem *plls_base[PLL_SYS_NUM];

static const struct mtk_fixed_factor top_divs[] = {
	FACTOR(CLK_TOP_MFGPLL1, "mfgpll1_ck",
			"mfg_ao_mfgpll1", 1, 1),
	FACTOR(CLK_TOP_MFGPLL4, "mfgpll4_ck",
			"mfg_ao_mfgpll4", 1, 1),
	FACTOR(CLK_TOP_MAINPLL_D4, "mainpll_d4",
			"mainpll", 1, 4),
	FACTOR(CLK_TOP_MAINPLL_D4_D2, "mainpll_d4_d2",
			"mainpll", 1, 8),
	FACTOR(CLK_TOP_MAINPLL_D4_D4, "mainpll_d4_d4",
			"mainpll", 1, 16),
	FACTOR(CLK_TOP_MAINPLL_D4_D8, "mainpll_d4_d8",
			"mainpll", 1, 32),
	FACTOR(CLK_TOP_MAINPLL_D4_D16, "mainpll_d4_d16",
			"mainpll", 1, 64),
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
	FACTOR(CLK_TOP_UNIVPLL, "univpll_ck",
			"univpll", 1, 1),
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
	FACTOR(CLK_TOP_UNIVPLL_D6_D2, "univpll_d6_d2",
			"univpll", 1, 12),
	FACTOR(CLK_TOP_UNIVPLL_D6_D4, "univpll_d6_d4",
			"univpll", 1, 24),
	FACTOR(CLK_TOP_UNIVPLL_D6_D8, "univpll_d6_d8",
			"univpll", 1, 48),
	FACTOR(CLK_TOP_UNIVPLL_D6_D16, "univpll_d6_d16",
			"univpll", 1, 96),
	FACTOR(CLK_TOP_UNIVPLL_D7, "univpll_d7",
			"univpll", 1, 7),
	FACTOR(CLK_TOP_UNIVPLL_D7_D2, "univpll_d7_d2",
			"univpll", 1, 14),
	FACTOR(CLK_TOP_UNIVPLL_192M_D2, "univpll_192m_d2",
			"univpll", 1, 26),
	FACTOR(CLK_TOP_UNIVPLL_192M_D4, "univpll_192m_d4",
			"univpll", 1, 52),
	FACTOR(CLK_TOP_UNIVPLL_192M_D8, "univpll_192m_d8",
			"univpll", 1, 104),
	FACTOR(CLK_TOP_UNIVPLL_192M_D16, "univpll_192m_d16",
			"univpll", 1, 208),
	FACTOR(CLK_TOP_UNIVPLL_192M_D32, "univpll_192m_d32",
			"univpll", 1, 416),
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
	FACTOR(CLK_TOP_ADSPPLL, "adsppll_ck",
			"adsppll", 1, 1),
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
	FACTOR(CLK_TOP_TVDPLL, "tvdpll_ck",
			"tvdpll", 1, 1),
	FACTOR(CLK_TOP_TVDPLL_D2, "tvdpll_d2",
			"tvdpll", 1, 2),
	FACTOR(CLK_TOP_MSDCPLL, "msdcpll_ck",
			"msdcpll", 1, 1),
	FACTOR(CLK_TOP_MSDCPLL_D2, "msdcpll_d2",
			"msdcpll", 1, 2),
	FACTOR(CLK_TOP_MSDCPLL_D4, "msdcpll_d4",
			"msdcpll", 1, 4),
	FACTOR(CLK_TOP_CLKRTC, "clkrtc",
			"clk32k", 1, 1),
	FACTOR(CLK_TOP_TCK_26M_MX9, "tck_26m_mx9_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_F26M_CK_D2, "f26m_d2",
			"clk26m", 1, 2),
	FACTOR(CLK_TOP_OSC, "osc_ck",
			"ulposc", 1, 1),
	FACTOR(CLK_TOP_OSC_D2, "osc_d2",
			"ulposc", 1, 2),
	FACTOR(CLK_TOP_OSC_D4, "osc_d4",
			"ulposc", 1, 4),
	FACTOR(CLK_TOP_OSC_D7, "osc_d7",
			"ulposc", 1, 7),
	FACTOR(CLK_TOP_OSC_D8, "osc_d8",
			"ulposc", 1, 8),
	FACTOR(CLK_TOP_OSC_D16, "osc_d16",
			"ulposc", 1, 16),
	FACTOR(CLK_TOP_OSC_D10, "osc_d10",
			"ulposc", 1, 10),
	FACTOR(CLK_TOP_ULPOSC, "ulposc_ck",
			"ulposc", 1, 1),
	FACTOR(CLK_TOP_F26M, "f26m_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_AXI, "axi_ck",
			"axi_sel", 1, 1),
	FACTOR(CLK_TOP_DISP0, "disp0_ck",
			"disp0_sel", 1, 1),
	FACTOR(CLK_TOP_MDP0, "mdp0_ck",
			"mdp0_sel", 1, 1),
	FACTOR(CLK_TOP_IMG1, "img1_ck",
			"img1_sel", 1, 1),
	FACTOR(CLK_TOP_IPE, "ipe_ck",
			"ipe_sel", 1, 1),
	FACTOR(CLK_TOP_DPE, "dpe_ck",
			"dpe_sel", 1, 1),
	FACTOR(CLK_TOP_CAM, "cam_ck",
			"cam_sel", 1, 1),
	FACTOR(CLK_TOP_DSP, "dsp_ck",
			"dsp_sel", 1, 1),
	FACTOR(CLK_TOP_DSP1, "dsp1_ck",
			"dsp1_sel", 1, 1),
	FACTOR(CLK_TOP_DSP2, "dsp2_ck",
			"dsp2_sel", 1, 1),
	FACTOR(CLK_TOP_DSP4, "dsp4_ck",
			"dsp4_sel", 1, 1),
	FACTOR(CLK_TOP_DSP7, "dsp7_ck",
			"dsp7_sel", 1, 1),
	FACTOR(CLK_TOP_FUART, "fuart_ck",
			"uart_sel", 1, 1),
	FACTOR(CLK_TOP_SPI, "spi_ck",
			"spi_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC50_0_HCLK, "msdc5hclk_ck",
			"msdc5hclk_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC50_0, "msdc50_0_ck",
			"msdc50_0_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC30_1, "msdc30_1_ck",
			"msdc30_1_sel", 1, 1),
	FACTOR(CLK_TOP_AUDIO, "audio_ck",
			"audio_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_INTBUS, "aud_intbus_ck",
			"aud_intbus_sel", 1, 1),
	FACTOR(CLK_TOP_FPWRAP_ULPOSC, "fpwrap_ulposc_ck",
			"pwrap_ulposc_sel", 1, 1),
	FACTOR(CLK_TOP_FDISP_PWM, "fdisp_pwm_ck",
			"disp_pwm_sel", 1, 1),
	FACTOR(CLK_TOP_FUSB_TOP, "fusb_ck",
			"usb_sel", 1, 1),
	FACTOR(CLK_TOP_FSSUSB_XHCI, "fssusb_xhci_ck",
			"ssusb_xhci_sel", 1, 1),
	FACTOR(CLK_TOP_I2C, "i2c_ck",
			"i2c_sel", 1, 1),
	FACTOR(CLK_TOP_DXCC, "dxcc_ck",
			"dxcc_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_ENGEN1, "aud_engen1_ck",
			"aud_engen1_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_ENGEN2, "aud_engen2_ck",
			"aud_engen2_sel", 1, 1),
	FACTOR(CLK_TOP_AES_UFSFDE, "aes_ufsfde_ck",
			"aes_ufsfde_sel", 1, 1),
	FACTOR(CLK_TOP_UFS, "ufs_ck",
			"ufs_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_1, "aud_1_ck",
			"aud_1_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_2, "aud_2_ck",
			"aud_2_sel", 1, 1),
	FACTOR(CLK_TOP_ADSP, "adsp_ck",
			"adsp_sel", 1, 1),
	FACTOR(CLK_TOP_DPMAIF_MAIN, "dpmaif_main_ck",
			"dpmaif_main_sel", 1, 1),
	FACTOR(CLK_TOP_VENC, "venc_ck",
			"venc_sel", 1, 1),
	FACTOR(CLK_TOP_VDEC, "vdec_ck",
			"vdec_sel", 1, 1),
	FACTOR(CLK_TOP_PWM, "pwm_ck",
			"pwm_sel", 1, 1),
	FACTOR(CLK_TOP_AUDIO_H, "audio_h_ck",
			"audio_h_sel", 1, 1),
	FACTOR(CLK_TOP_MEM_SUB, "mem_sub_ck",
			"mem_sub_sel", 1, 1),
	FACTOR(CLK_TOP_AES_MSDCFDE, "aes_msdcfde_ck",
			"aes_msdcfde_sel", 1, 1),
	FACTOR(CLK_TOP_UFS_MBIST, "ufs_mbist_ck",
			"ufs_mbist_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC_NEW_RX, "msdc_new_rx_ck",
			"msdc_new_rx_sel", 1, 1),
	FACTOR(CLK_TOP_MFG_INTERNAL1, "mfg_internal1_ck",
			"mfg_internal1_sel", 1, 1),
	FACTOR(CLK_TOP_MFG_INTERNAL2, "mfg_internal2_ck",
			"mfg_internal2_sel", 1, 1),
	FACTOR(CLK_TOP_FI2C_PSEUDO, "fi2c_pseudo_ck",
			"ifrao_i2c_pseudo", 1, 1),
};

static const char * const axi_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d7_d2",
	"mainpll_d4_d2",
	"mainpll_d5_d2",
	"mainpll_d6_d2",
	"osc_d4"
};

static const char * const spm_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
	"mainpll_d7_d4",
	"clkrtc"
};

static const char * const scp_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4",
	"adsppll_ck",
	"mainpll_d6",
	"univpll_d6",
	"mainpll_d4_d2",
	"apll1_ck",
	"mainpll_d7"
};

static const char * const bus_aximem_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d2",
	"mainpll_d4_d2",
	"mainpll_d5_d2",
	"mainpll_d6"
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
	"mainpll_d4",
	"tvdpll_ck",
	"mmpll_d5_d2",
	"univpll_d4"
};

static const char * const mdp0_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d2",
	"mmpll_d6_d2",
	"mainpll_d4_d2",
	"mmpll_d4_d2",
	"mainpll_d6",
	"mainpll_d5",
	"mainpll_d4",
	"tvdpll_ck",
	"univpll_d4",
	"mmpll_d5_d2"
};

static const char * const img1_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4",
	"mainpll_d4",
	"univpll_d5",
	"mmpll_d6",
	"univpll_d6",
	"mainpll_d6",
	"mmpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d4_d2",
	"mmpll_d5_d2"
};

static const char * const ipe_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4",
	"mmpll_d6",
	"univpll_d6",
	"mainpll_d6",
	"mmpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d4_d2",
	"mmpll_d5_d2"
};

static const char * const dpe_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4",
	"mmpll_d6",
	"mainpll_d6",
	"univpll_d5_d2"
};

static const char * const cam_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4",
	"mmpll_d6",
	"univpll_d4",
	"univpll_d5",
	"univpll_d6",
	"mmpll_d7",
	"univpll_d4_d2",
	"mainpll_d4_d2",
	"tvdpll_d2"
};

static const char * const ccu_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d6",
	"tvdpll_ck",
	"mainpll_d4",
	"univpll_d5",
	"mmpll_d7",
	"univpll_d4_d2",
	"mmpll_d5_d2"
};

static const char * const dsp_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"mmpll_d4_d2",
	"univpll_d5",
	"mmpll_d5",
	"univpll_d4",
	"mmpll_d4"
};

static const char * const dsp1_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"mmpll_d4_d2",
	"univpll_d5",
	"mmpll_d5",
	"univpll_d4"
};

static const char * const dsp2_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"mmpll_d4_d2",
	"univpll_d5",
	"mmpll_d5",
	"univpll_d4"
};

static const char * const dsp4_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"univpll_d4_d2",
	"mmpll_d4_d2",
	"univpll_d5",
	"mainpll_d4",
	"univpll_d4",
	"mmpll_d4"
};

static const char * const dsp7_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"mmpll_d4_d2",
	"univpll_d5",
	"mmpll_d5",
	"univpll_d4",
	"mmpll_d4"
};

static const char * const camtg_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_192m_d8",
	"univpll_d6_d8",
	"univpll_192m_d4",
	"univpll_d6_d16",
	"f26m_d2",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const camtg2_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_192m_d8",
	"univpll_d6_d8",
	"univpll_192m_d4",
	"univpll_d6_d16",
	"f26m_d2",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const camtg3_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_192m_d8",
	"univpll_d6_d8",
	"univpll_192m_d4",
	"univpll_d6_d16",
	"f26m_d2",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const camtg4_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_192m_d8",
	"univpll_d6_d8",
	"univpll_192m_d4",
	"univpll_d6_d16",
	"f26m_d2",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const camtg5_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_192m_d8",
	"univpll_d6_d8",
	"univpll_192m_d4",
	"univpll_d6_d16",
	"f26m_d2",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const uart_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d8"
};

static const char * const spi_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d4",
	"mainpll_d6_d4",
	"msdcpll_d4"
};

static const char * const msdc5hclk_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"mainpll_d6_d2"
};

static const char * const msdc50_0_parents[] = {
	"tck_26m_mx9_ck",
	"msdcpll_ck",
	"msdcpll_d2",
	"univpll_d4_d4",
	"mainpll_d6_d2",
	"univpll_d4_d2"
};

static const char * const msdc30_1_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d6_d2",
	"mainpll_d7_d2",
	"msdcpll_d2"
};

static const char * const audio_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d8",
	"mainpll_d7_d8",
	"mainpll_d4_d16"
};

static const char * const aud_intbus_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d7_d4"
};

static const char * const pwrap_ulposc_parents[] = {
	"osc_d10",
	"tck_26m_mx9_ck",
	"osc_d4",
	"osc_d7",
	"osc_d8",
	"osc_d16"
};

static const char * const atb_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"mainpll_d5_d2"
};

static const char * const sspm_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d6_d2",
	"mainpll_d5_d2",
	"mainpll_d9",
	"mainpll_d4_d2",
	"mainpll_d7"
};

static const char * const disp_pwm_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d4",
	"osc_d2",
	"osc_d4",
	"osc_d16"
};

static const char * const usb_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"univpll_d6_d4",
	"univpll_d5_d2"
};

static const char * const ssusb_xhci_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"univpll_d6_d4",
	"univpll_d5_d2"
};

static const char * const i2c_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d8",
	"univpll_d5_d4"
};

static const char * const seninf_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4_d4",
	"univpll_d6_d2",
	"univpll_d4_d2",
	"tvdpll_d2",
	"mmpll_d7",
	"mmpll_d6",
	"univpll_d5"
};

static const char * const seninf1_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4_d4",
	"univpll_d6_d2",
	"univpll_d4_d2",
	"tvdpll_d2",
	"mmpll_d7",
	"mmpll_d6",
	"univpll_d5"
};

static const char * const seninf2_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4_d4",
	"univpll_d6_d2",
	"univpll_d4_d2",
	"tvdpll_d2",
	"mmpll_d7",
	"mmpll_d6",
	"univpll_d5"
};

static const char * const seninf3_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4_d4",
	"univpll_d6_d2",
	"univpll_d4_d2",
	"tvdpll_d2",
	"mmpll_d7",
	"mmpll_d6",
	"univpll_d5"
};

static const char * const dxcc_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"mainpll_d4_d4",
	"mainpll_d4_d8"
};

static const char * const aud_engen1_parents[] = {
	"tck_26m_mx9_ck",
	"apll1_d2",
	"apll1_d4",
	"apll1_d8"
};

static const char * const aud_engen2_parents[] = {
	"tck_26m_mx9_ck",
	"apll2_d2",
	"apll2_d4",
	"apll2_d8"
};

static const char * const aes_ufsfde_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4",
	"mainpll_d4_d2",
	"mainpll_d6",
	"mainpll_d4_d4",
	"univpll_d4_d2",
	"univpll_d6"
};

static const char * const ufs_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d4_d8",
	"univpll_d4_d4",
	"mainpll_d6_d2",
	"mainpll_d5_d2",
	"msdcpll_d2"
};

static const char * const aud_1_parents[] = {
	"tck_26m_mx9_ck",
	"apll1_ck"
};

static const char * const aud_2_parents[] = {
	"tck_26m_mx9_ck",
	"apll2_ck"
};

static const char * const adsp_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d6",
	"mainpll_d5_d2",
	"univpll_d4_d4",
	"univpll_d4",
	"univpll_d6",
	"ulposc_ck",
	"adsppll_ck"
};

static const char * const dpmaif_main_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4_d4",
	"mainpll_d6",
	"mainpll_d4_d2",
	"univpll_d4_d2"
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
	"univpll_d4_d4",
	"mainpll_d4",
	"univpll_d4",
	"univpll_d5",
	"univpll_d5_d2",
	"mainpll_d5"
};

static const char * const vdec_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_192m_d2",
	"univpll_d5_d4",
	"mainpll_d5",
	"mainpll_d5_d2",
	"mmpll_d6_d2",
	"univpll_d5_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"univpll_d7",
	"mmpll_d7",
	"mmpll_d6",
	"univpll_d5",
	"mainpll_d4",
	"univpll_d4",
	"univpll_d6"
};

static const char * const camtm_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4_d4",
	"univpll_d6_d2",
	"univpll_d6_d4"
};

static const char * const pwm_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4_d8"
};

static const char * const audio_h_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d7_d2",
	"apll1_ck",
	"apll2_ck"
};

static const char * const mcupm_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d5_d2",
	"mainpll_d6_d2"
};

static const char * const spmi_m_mst_parents[] = {
	"tck_26m_mx9_ck",
	"f26m_d2",
	"osc_d8",
	"osc_d10",
	"osc_d16",
	"osc_d7",
	"clkrtc",
	"mainpll_d7_d8",
	"mainpll_d6_d8",
	"mainpll_d5_d8"
};

static const char * const dvfsrc_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10"
};

static const char * const mem_sub_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4_d4",
	"mainpll_d6_d2",
	"mainpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6",
	"mmpll_d7",
	"mainpll_d5",
	"univpll_d5",
	"mainpll_d4"
};

static const char * const aes_msdcfde_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"mainpll_d6",
	"mainpll_d4_d4",
	"univpll_d4_d2",
	"univpll_d6"
};

static const char * const ufs_mbist_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"tvdpll_d2"
};

static const char * const mfg_internal2_parents[] = {
	"mfg_internal1_ck",
	"mfgpll1_ck"
};

static const char * const mfg_internal1_parents[] = {
	"tck_26m_mx9_ck",
	"mfgpll4_ck"
};

static const char * const ap2conn_host_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d4"
};

static const char * const msdc_new_rx_parents[] = {
	"tck_26m_mx9_ck",
	"msdcpll_ck",
	"msdcpll_d2",
	"univpll_d4_d4",
	"mainpll_d6_d2",
	"univpll_d4_d2"
};

static const char * const apll_i2s0_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2s1_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2s2_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2s3_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2s4_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2s5_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2s6_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2s7_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2s8_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2s9_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const struct mtk_mux top_muxes[] = {
#if MT_CCF_MUX_DISABLE
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD(CLK_TOP_AXI_SEL/* dts */, "axi_sel",
		axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AXI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SPM_SEL/* dts */, "spm_sel",
		spm_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SCP_SEL/* dts */, "scp_sel",
		scp_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SCP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_BUS_AXIMEM_SEL/* dts */, "bus_aximem_sel",
		bus_aximem_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_BUS_AXIMEM_SHIFT/* upd shift */),
	/* CLK_CFG_1 */
	MUX_CLR_SET_UPD(CLK_TOP_DISP0_SEL/* dts */, "disp0_sel",
		disp0_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 4/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DISP0_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MDP0_SEL/* dts */, "mdp0_sel",
		mdp0_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 4/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MDP0_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_IMG1_SEL/* dts */, "img1_sel",
		img1_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 4/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_IMG1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_IPE_SEL/* dts */, "ipe_sel",
		ipe_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 4/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_IPE_SHIFT/* upd shift */),
	/* CLK_CFG_2 */
	MUX_CLR_SET_UPD(CLK_TOP_DPE_SEL/* dts */, "dpe_sel",
		dpe_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DPE_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CAM_SEL/* dts */, "cam_sel",
		cam_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 4/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CCU_SEL/* dts */, "ccu_sel",
		ccu_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CCU_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DSP_SEL/* dts */, "dsp_sel",
		dsp_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DSP_SHIFT/* upd shift */),
	/* CLK_CFG_3 */
	MUX_CLR_SET_UPD(CLK_TOP_DSP1_SEL/* dts */, "dsp1_sel",
		dsp1_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DSP1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DSP2_SEL/* dts */, "dsp2_sel",
		dsp2_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DSP2_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DSP4_SEL/* dts */, "dsp4_sel",
		dsp4_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DSP4_SHIFT/* upd shift */),
	/* CLK_CFG_4 */
	MUX_CLR_SET_UPD(CLK_TOP_DSP7_SEL/* dts */, "dsp7_sel",
		dsp7_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DSP7_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG_SEL/* dts */, "camtg_sel",
		camtg_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG_SHIFT/* upd shift */),
	/* CLK_CFG_5 */
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG2_SEL/* dts */, "camtg2_sel",
		camtg2_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG2_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG3_SEL/* dts */, "camtg3_sel",
		camtg3_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG3_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG4_SEL/* dts */, "camtg4_sel",
		camtg4_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG4_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG5_SEL/* dts */, "camtg5_sel",
		camtg5_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG5_SHIFT/* upd shift */),
	/* CLK_CFG_6 */
	MUX_CLR_SET_UPD(CLK_TOP_UART_SEL/* dts */, "uart_sel",
		uart_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 0/* lsb */, 1/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_UART_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SPI_SEL/* dts */, "spi_sel",
		spi_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 8/* lsb */, 2/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC50_0_HCLK_SEL/* dts */, "msdc5hclk_sel",
		msdc5hclk_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 2/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC50_0_HCLK_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC50_0_SEL/* dts */, "msdc50_0_sel",
		msdc50_0_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 24/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC50_0_SHIFT/* upd shift */),
	/* CLK_CFG_7 */
	MUX_CLR_SET_UPD(CLK_TOP_MSDC30_1_SEL/* dts */, "msdc30_1_sel",
		msdc30_1_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC30_1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUDIO_SEL/* dts */, "audio_sel",
		audio_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 8/* lsb */, 2/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AUDIO_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_INTBUS_SEL/* dts */, "aud_intbus_sel",
		aud_intbus_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 16/* lsb */, 2/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AUD_INTBUS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_PWRAP_ULPOSC_SEL/* dts */, "pwrap_ulposc_sel",
		pwrap_ulposc_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 24/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_PWRAP_ULPOSC_SHIFT/* upd shift */),
	/* CLK_CFG_8 */
	MUX_CLR_SET_UPD(CLK_TOP_ATB_SEL/* dts */, "atb_sel",
		atb_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 0/* lsb */, 2/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_ATB_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SSPM_SEL/* dts */, "sspm_sel",
		sspm_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 8/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SSPM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DISP_PWM_SEL/* dts */, "disp_pwm_sel",
		disp_pwm_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 24/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_DISP_PWM_SHIFT/* upd shift */),
	/* CLK_CFG_9 */
	MUX_CLR_SET_UPD(CLK_TOP_USB_TOP_SEL/* dts */, "usb_sel",
		usb_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 0/* lsb */, 2/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_USB_TOP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SSUSB_XHCI_SEL/* dts */, "ssusb_xhci_sel",
		ssusb_xhci_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 8/* lsb */, 2/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SSUSB_XHCI_SHIFT/* upd shift */),
	/* CLK_CFG_10 */
	MUX_CLR_SET_UPD(CLK_TOP_I2C_SEL/* dts */, "i2c_sel",
		i2c_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 2/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_I2C_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SENINF_SEL/* dts */, "seninf_sel",
		seninf_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SENINF_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SENINF1_SEL/* dts */, "seninf1_sel",
		seninf1_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SENINF1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SENINF2_SEL/* dts */, "seninf2_sel",
		seninf2_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 24/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SENINF2_SHIFT/* upd shift */),
	/* CLK_CFG_11 */
	MUX_CLR_SET_UPD(CLK_TOP_SENINF3_SEL/* dts */, "seninf3_sel",
		seninf3_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 0/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SENINF3_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DXCC_SEL/* dts */, "dxcc_sel",
		dxcc_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 8/* lsb */, 2/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_DXCC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_ENGEN1_SEL/* dts */, "aud_engen1_sel",
		aud_engen1_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 16/* lsb */, 2/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_ENGEN1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_ENGEN2_SEL/* dts */, "aud_engen2_sel",
		aud_engen2_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 24/* lsb */, 2/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_ENGEN2_SHIFT/* upd shift */),
	/* CLK_CFG_12 */
	MUX_CLR_SET_UPD(CLK_TOP_AES_UFSFDE_SEL/* dts */, "aes_ufsfde_sel",
		aes_ufsfde_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 0/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AES_UFSFDE_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_UFS_SEL/* dts */, "ufs_sel",
		ufs_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 8/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_UFS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_1_SEL/* dts */, "aud_1_sel",
		aud_1_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 16/* lsb */, 1/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_2_SEL/* dts */, "aud_2_sel",
		aud_2_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 24/* lsb */, 1/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_2_SHIFT/* upd shift */),
	/* CLK_CFG_13 */
	MUX_CLR_SET_UPD(CLK_TOP_ADSP_SEL/* dts */, "adsp_sel",
		adsp_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 0/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_ADSP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DPMAIF_MAIN_SEL/* dts */, "dpmaif_main_sel",
		dpmaif_main_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 8/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_DPMAIF_MAIN_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_VENC_SEL/* dts */, "venc_sel",
		venc_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 16/* lsb */, 4/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_VENC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_VDEC_SEL/* dts */, "vdec_sel",
		vdec_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 24/* lsb */, 4/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_VDEC_SHIFT/* upd shift */),
	/* CLK_CFG_14 */
	MUX_CLR_SET_UPD(CLK_TOP_CAMTM_SEL/* dts */, "camtm_sel",
		camtm_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 0/* lsb */, 2/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_CAMTM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_PWM_SEL/* dts */, "pwm_sel",
		pwm_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 8/* lsb */, 1/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_PWM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUDIO_H_SEL/* dts */, "audio_h_sel",
		audio_h_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 16/* lsb */, 2/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUDIO_H_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MCUPM_SEL/* dts */, "mcupm_sel",
		mcupm_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 24/* lsb */, 2/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_MCUPM_SHIFT/* upd shift */),
	/* CLK_CFG_15 */
	MUX_CLR_SET_UPD(CLK_TOP_SPMI_M_MST_SEL/* dts */, "spmi_m_mst_sel",
		spmi_m_mst_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 8/* lsb */, 4/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SPMI_M_MST_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DVFSRC_SEL/* dts */, "dvfsrc_sel",
		dvfsrc_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 16/* lsb */, 1/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_DVFSRC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MEM_SUB_SEL/* dts */, "mem_sub_sel",
		mem_sub_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 24/* lsb */, 4/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_MEM_SUB_SHIFT/* upd shift */),
	/* CLK_CFG_16 */
	MUX_CLR_SET_UPD(CLK_TOP_AES_MSDCFDE_SEL/* dts */, "aes_msdcfde_sel",
		aes_msdcfde_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 0/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_AES_MSDCFDE_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_UFS_MBIST_SEL/* dts */, "ufs_mbist_sel",
		ufs_mbist_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 24/* lsb */, 2/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_UFS_MBIST_SHIFT/* upd shift */),
	/* CLK_CFG_20 */
	MUX_CLR_SET_UPD(CLK_TOP_MFG_INTERNAL2_SEL/* dts */, "mfg_internal2_sel",
		mfg_internal2_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 18/* lsb */, 1/* width */,
		INV_BIT/* pdn bit */, INV_OFS/* upd ofs */,
		INV_BIT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MFG_INTERNAL1_SEL/* dts */, "mfg_internal1_sel",
		mfg_internal1_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 19/* lsb */, 1/* width */,
		INV_BIT/* pdn bit */, INV_OFS/* upd ofs */,
		INV_BIT/* upd shift */),
	/* CLK_CFG_17 */
	MUX_CLR_SET_UPD(CLK_TOP_AP2CONN_HOST_SEL/* dts */, "ap2conn_host_sel",
		ap2conn_host_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 0/* lsb */, 1/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_AP2CONN_HOST_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC_NEW_RX_SEL/* dts */, "msdc_new_rx_sel",
		msdc_new_rx_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 8/* lsb */, 3/* width */,
		INV_BIT/* pdn bit */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_MSDC_NEW_RX_SHIFT/* upd shift */),
#else
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD_FLAGS(CLK_TOP_AXI_SEL/* dts */, "axi_sel",
		axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AXI_SHIFT/* upd shift */, CLK_IS_CRITICAL),
	MUX_CLR_SET_UPD_FLAGS(CLK_TOP_SPM_SEL/* dts */, "spm_sel",
		spm_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPM_SHIFT/* upd shift */, CLK_IS_CRITICAL),
	MUX_CLR_SET_UPD(CLK_TOP_SCP_SEL/* dts */, "scp_sel",
		scp_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SCP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FLAGS(CLK_TOP_BUS_AXIMEM_SEL/* dts */, "bus_aximem_sel",
		bus_aximem_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_BUS_AXIMEM_SHIFT/* upd shift */, CLK_IS_CRITICAL),
	/* CLK_CFG_1 */
	MUX_CLR_SET_UPD(CLK_TOP_DISP0_SEL/* dts */, "disp0_sel",
		disp0_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 4/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DISP0_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MDP0_SEL/* dts */, "mdp0_sel",
		mdp0_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 4/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MDP0_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_IMG1_SEL/* dts */, "img1_sel",
		img1_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 4/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_IMG1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_IPE_SEL/* dts */, "ipe_sel",
		ipe_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 4/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_IPE_SHIFT/* upd shift */),
	/* CLK_CFG_2 */
	MUX_CLR_SET_UPD(CLK_TOP_DPE_SEL/* dts */, "dpe_sel",
		dpe_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DPE_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CAM_SEL/* dts */, "cam_sel",
		cam_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 4/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CCU_SEL/* dts */, "ccu_sel",
		ccu_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CCU_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DSP_SEL/* dts */, "dsp_sel",
		dsp_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DSP_SHIFT/* upd shift */),
	/* CLK_CFG_3 */
	MUX_CLR_SET_UPD(CLK_TOP_DSP1_SEL/* dts */, "dsp1_sel",
		dsp1_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DSP1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DSP2_SEL/* dts */, "dsp2_sel",
		dsp2_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DSP2_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DSP4_SEL/* dts */, "dsp4_sel",
		dsp4_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DSP4_SHIFT/* upd shift */),
	/* CLK_CFG_4 */
	MUX_CLR_SET_UPD(CLK_TOP_DSP7_SEL/* dts */, "dsp7_sel",
		dsp7_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DSP7_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG_SEL/* dts */, "camtg_sel",
		camtg_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG_SHIFT/* upd shift */),
	/* CLK_CFG_5 */
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG2_SEL/* dts */, "camtg2_sel",
		camtg2_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG2_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG3_SEL/* dts */, "camtg3_sel",
		camtg3_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG3_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG4_SEL/* dts */, "camtg4_sel",
		camtg4_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG4_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG5_SEL/* dts */, "camtg5_sel",
		camtg5_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_CAMTG5_SHIFT/* upd shift */),
	/* CLK_CFG_6 */
	MUX_CLR_SET_UPD(CLK_TOP_UART_SEL/* dts */, "uart_sel",
		uart_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 0/* lsb */, 1/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_UART_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FLAGS(CLK_TOP_SPI_SEL/* dts */, "spi_sel",
		spi_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI_SHIFT/* upd shift */, CLK_IS_CRITICAL),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC50_0_HCLK_SEL/* dts */, "msdc5hclk_sel",
		msdc5hclk_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC50_0_HCLK_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC50_0_SEL/* dts */, "msdc50_0_sel",
		msdc50_0_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC50_0_SHIFT/* upd shift */),
	/* CLK_CFG_7 */
	MUX_CLR_SET_UPD(CLK_TOP_MSDC30_1_SEL/* dts */, "msdc30_1_sel",
		msdc30_1_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC30_1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUDIO_SEL/* dts */, "audio_sel",
		audio_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AUDIO_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_INTBUS_SEL/* dts */, "aud_intbus_sel",
		aud_intbus_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AUD_INTBUS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_PWRAP_ULPOSC_SEL/* dts */, "pwrap_ulposc_sel",
		pwrap_ulposc_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_PWRAP_ULPOSC_SHIFT/* upd shift */),
	/* CLK_CFG_8 */
	MUX_CLR_SET_UPD_FLAGS(CLK_TOP_ATB_SEL/* dts */, "atb_sel",
		atb_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_ATB_SHIFT/* upd shift */, CLK_IS_CRITICAL),
	MUX_CLR_SET_UPD_FLAGS(CLK_TOP_SSPM_SEL/* dts */, "sspm_sel",
		sspm_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SSPM_SHIFT/* upd shift */, CLK_IS_CRITICAL),
	MUX_CLR_SET_UPD(CLK_TOP_DISP_PWM_SEL/* dts */, "disp_pwm_sel",
		disp_pwm_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_DISP_PWM_SHIFT/* upd shift */),
	/* CLK_CFG_9 */
	MUX_CLR_SET_UPD(CLK_TOP_USB_TOP_SEL/* dts */, "usb_sel",
		usb_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_USB_TOP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SSUSB_XHCI_SEL/* dts */, "ssusb_xhci_sel",
		ssusb_xhci_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SSUSB_XHCI_SHIFT/* upd shift */),
	/* CLK_CFG_10 */
	MUX_CLR_SET_UPD(CLK_TOP_I2C_SEL/* dts */, "i2c_sel",
		i2c_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_I2C_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SENINF_SEL/* dts */, "seninf_sel",
		seninf_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SENINF_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SENINF1_SEL/* dts */, "seninf1_sel",
		seninf1_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SENINF1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SENINF2_SEL/* dts */, "seninf2_sel",
		seninf2_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SENINF2_SHIFT/* upd shift */),
	/* CLK_CFG_11 */
	MUX_CLR_SET_UPD(CLK_TOP_SENINF3_SEL/* dts */, "seninf3_sel",
		seninf3_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SENINF3_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FLAGS(CLK_TOP_DXCC_SEL/* dts */, "dxcc_sel",
		dxcc_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_DXCC_SHIFT/* upd shift */, CLK_IS_CRITICAL),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_ENGEN1_SEL/* dts */, "aud_engen1_sel",
		aud_engen1_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_ENGEN1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_ENGEN2_SEL/* dts */, "aud_engen2_sel",
		aud_engen2_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_ENGEN2_SHIFT/* upd shift */),
	/* CLK_CFG_12 */
	MUX_CLR_SET_UPD(CLK_TOP_AES_UFSFDE_SEL/* dts */, "aes_ufsfde_sel",
		aes_ufsfde_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AES_UFSFDE_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_UFS_SEL/* dts */, "ufs_sel",
		ufs_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_UFS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_1_SEL/* dts */, "aud_1_sel",
		aud_1_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 16/* lsb */, 1/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_2_SEL/* dts */, "aud_2_sel",
		aud_2_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 24/* lsb */, 1/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUD_2_SHIFT/* upd shift */),
	/* CLK_CFG_13 */
	MUX_CLR_SET_UPD(CLK_TOP_ADSP_SEL/* dts */, "adsp_sel",
		adsp_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_ADSP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DPMAIF_MAIN_SEL/* dts */, "dpmaif_main_sel",
		dpmaif_main_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_DPMAIF_MAIN_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_VENC_SEL/* dts */, "venc_sel",
		venc_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 16/* lsb */, 4/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_VENC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_VDEC_SEL/* dts */, "vdec_sel",
		vdec_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 24/* lsb */, 4/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_VDEC_SHIFT/* upd shift */),
	/* CLK_CFG_14 */
	MUX_CLR_SET_UPD(CLK_TOP_CAMTM_SEL/* dts */, "camtm_sel",
		camtm_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_CAMTM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_PWM_SEL/* dts */, "pwm_sel",
		pwm_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_PWM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUDIO_H_SEL/* dts */, "audio_h_sel",
		audio_h_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_AUDIO_H_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FLAGS(CLK_TOP_MCUPM_SEL/* dts */, "mcupm_sel",
		mcupm_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_MCUPM_SHIFT/* upd shift */, CLK_IS_CRITICAL),
	/* CLK_CFG_15 */
	MUX_CLR_SET_UPD(CLK_TOP_SPMI_M_MST_SEL/* dts */, "spmi_m_mst_sel",
		spmi_m_mst_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 8/* lsb */, 4/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SPMI_M_MST_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD_FLAGS(CLK_TOP_DVFSRC_SEL/* dts */, "dvfsrc_sel",
		dvfsrc_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 16/* lsb */, 1/* width */,
		23/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_DVFSRC_SHIFT/* upd shift */, CLK_IS_CRITICAL),
	MUX_CLR_SET_UPD_FLAGS(CLK_TOP_MEM_SUB_SEL/* dts */, "mem_sub_sel",
		mem_sub_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 24/* lsb */, 4/* width */,
		31/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_MEM_SUB_SHIFT/* upd shift */, CLK_IS_CRITICAL),
	/* CLK_CFG_16 */
	MUX_CLR_SET_UPD(CLK_TOP_AES_MSDCFDE_SEL/* dts */, "aes_msdcfde_sel",
		aes_msdcfde_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_AES_MSDCFDE_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_UFS_MBIST_SEL/* dts */, "ufs_mbist_sel",
		ufs_mbist_parents/* parent */, CLK_CFG_16, CLK_CFG_16_SET,
		CLK_CFG_16_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_UFS_MBIST_SHIFT/* upd shift */),
	/* CLK_CFG_20 */
	MUX_CLR_SET_UPD(CLK_TOP_MFG_INTERNAL2_SEL/* dts */, "mfg_internal2_sel",
		mfg_internal2_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 18/* lsb */, 1/* width */,
		INV_BIT/* pdn */, INV_OFS/* upd ofs */,
		INV_BIT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MFG_INTERNAL1_SEL/* dts */, "mfg_internal1_sel",
		mfg_internal1_parents/* parent */, CLK_CFG_20, CLK_CFG_20_SET,
		CLK_CFG_20_CLR/* set parent */, 19/* lsb */, 1/* width */,
		INV_BIT/* pdn */, INV_OFS/* upd ofs */,
		INV_BIT/* upd shift */),
	/* CLK_CFG_17 */
	MUX_CLR_SET_UPD_FLAGS(CLK_TOP_AP2CONN_HOST_SEL/* dts */, "ap2conn_host_sel",
		ap2conn_host_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 0/* lsb */, 1/* width */,
		7/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_AP2CONN_HOST_SHIFT/* upd shift */, CLK_IS_CRITICAL),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC_NEW_RX_SEL/* dts */, "msdc_new_rx_sel",
		msdc_new_rx_parents/* parent */, CLK_CFG_17, CLK_CFG_17_SET,
		CLK_CFG_17_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_MSDC_NEW_RX_SHIFT/* upd shift */),
#endif /* MT_CCF_MUX_DISABLE */
};

static const struct mtk_composite top_composites[] = {
	/* CLK_AUDDIV_0 */
	MUX(CLK_TOP_APLL_I2S0_MCK_SEL/* dts */, "apll_i2s0_mck_sel",
		apll_i2s0_mck_parents/* parent */, 0x0320/* ofs */,
		16/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2S1_MCK_SEL/* dts */, "apll_i2s1_mck_sel",
		apll_i2s1_mck_parents/* parent */, 0x0320/* ofs */,
		17/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2S2_MCK_SEL/* dts */, "apll_i2s2_mck_sel",
		apll_i2s2_mck_parents/* parent */, 0x0320/* ofs */,
		18/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2S3_MCK_SEL/* dts */, "apll_i2s3_mck_sel",
		apll_i2s3_mck_parents/* parent */, 0x0320/* ofs */,
		19/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2S4_MCK_SEL/* dts */, "apll_i2s4_mck_sel",
		apll_i2s4_mck_parents/* parent */, 0x0320/* ofs */,
		20/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2S5_MCK_SEL/* dts */, "apll_i2s5_mck_sel",
		apll_i2s5_mck_parents/* parent */, 0x0320/* ofs */,
		21/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2S6_MCK_SEL/* dts */, "apll_i2s6_mck_sel",
		apll_i2s6_mck_parents/* parent */, 0x0320/* ofs */,
		22/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2S7_MCK_SEL/* dts */, "apll_i2s7_mck_sel",
		apll_i2s7_mck_parents/* parent */, 0x0320/* ofs */,
		23/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2S8_MCK_SEL/* dts */, "apll_i2s8_mck_sel",
		apll_i2s8_mck_parents/* parent */, 0x0320/* ofs */,
		24/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2S9_MCK_SEL/* dts */, "apll_i2s9_mck_sel",
		apll_i2s9_mck_parents/* parent */, 0x0320/* ofs */,
		25/* lsb */, 1/* width */),
#if MT_CCF_MUX_DISABLE
	/* CLK_AUDDIV_2 */
	DIV_GATE(CLK_TOP_APLL12_CK_DIV0/* dts */, "apll12_div0"/* ccf */,
		"apll_i2s0_mck_sel"/* parent */, INV_OFS/* pdn ofs */,
		INV_BIT/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV1/* dts */, "apll12_div1"/* ccf */,
		"apll_i2s1_mck_sel"/* parent */, INV_OFS/* pdn ofs */,
		INV_BIT/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV2/* dts */, "apll12_div2"/* ccf */,
		"apll_i2s2_mck_sel"/* parent */, INV_OFS/* pdn ofs */,
		INV_BIT/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		16/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV3/* dts */, "apll12_div3"/* ccf */,
		"apll_i2s3_mck_sel"/* parent */, INV_OFS/* pdn ofs */,
		INV_BIT/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		24/* lsb */),
	/* CLK_AUDDIV_3 */
	DIV_GATE(CLK_TOP_APLL12_CK_DIV4/* dts */, "apll12_div4"/* ccf */,
		"apll_i2s4_mck_sel"/* parent */, INV_OFS/* pdn ofs */,
		INV_BIT/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIVB/* dts */, "apll12_divb"/* ccf */,
		"apll12_div4"/* parent */, INV_OFS/* pdn ofs */,
		INV_BIT/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV5/* dts */, "apll12_div5"/* ccf */,
		"apll_i2s5_mck_sel"/* parent */, INV_OFS/* pdn ofs */,
		INV_BIT/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		16/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV6/* dts */, "apll12_div6"/* ccf */,
		"apll_i2s6_mck_sel"/* parent */, INV_OFS/* pdn ofs */,
		INV_BIT/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		24/* lsb */),
	/* CLK_AUDDIV_4 */
	DIV_GATE(CLK_TOP_APLL12_CK_DIV7/* dts */, "apll12_div7"/* ccf */,
		"apll_i2s7_mck_sel"/* parent */, INV_OFS/* pdn ofs */,
		INV_BIT/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV8/* dts */, "apll12_div8"/* ccf */,
		"apll_i2s8_mck_sel"/* parent */, INV_OFS/* pdn ofs */,
		INV_BIT/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV9/* dts */, "apll12_div9"/* ccf */,
		"apll_i2s9_mck_sel"/* parent */, INV_OFS/* pdn ofs */,
		INV_BIT/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		16/* lsb */),
#else
	/* CLK_AUDDIV_2 */
	DIV_GATE(CLK_TOP_APLL12_CK_DIV0/* dts */, "apll12_div0"/* ccf */,
		"apll_i2s0_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		0/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV1/* dts */, "apll12_div1"/* ccf */,
		"apll_i2s1_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		1/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV2/* dts */, "apll12_div2"/* ccf */,
		"apll_i2s2_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		2/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		16/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV3/* dts */, "apll12_div3"/* ccf */,
		"apll_i2s3_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		3/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		24/* lsb */),
	/* CLK_AUDDIV_3 */
	DIV_GATE(CLK_TOP_APLL12_CK_DIV4/* dts */, "apll12_div4"/* ccf */,
		"apll_i2s4_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		4/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIVB/* dts */, "apll12_divb"/* ccf */,
		"apll12_div4"/* parent */, 0x0320/* pdn ofs */,
		5/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV5/* dts */, "apll12_div5"/* ccf */,
		"apll_i2s5_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		6/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		16/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV6/* dts */, "apll12_div6"/* ccf */,
		"apll_i2s6_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		7/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		24/* lsb */),
	/* CLK_AUDDIV_4 */
	DIV_GATE(CLK_TOP_APLL12_CK_DIV7/* dts */, "apll12_div7"/* ccf */,
		"apll_i2s7_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		8/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV8/* dts */, "apll12_div8"/* ccf */,
		"apll_i2s8_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		9/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV9/* dts */, "apll12_div9"/* ccf */,
		"apll_i2s9_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		10/* pdn bit */, CLK_AUDDIV_4/* ofs */, 8/* width */,
		16/* lsb */),
#endif /* MT_CCF_MUX_DISABLE */
};

static const struct mtk_gate_regs ifrao0_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x84,
	.sta_ofs = 0x90,
};

static const struct mtk_gate_regs ifrao1_cg_regs = {
	.set_ofs = 0x88,
	.clr_ofs = 0x8c,
	.sta_ofs = 0x94,
};

static const struct mtk_gate_regs ifrao2_cg_regs = {
	.set_ofs = 0xa4,
	.clr_ofs = 0xa8,
	.sta_ofs = 0xac,
};

static const struct mtk_gate_regs ifrao3_cg_regs = {
	.set_ofs = 0xc0,
	.clr_ofs = 0xc4,
	.sta_ofs = 0xc8,
};

static const struct mtk_gate_regs ifrao4_cg_regs = {
	.set_ofs = 0xe0,
	.clr_ofs = 0xe4,
	.sta_ofs = 0xe8,
};

#define GATE_IFRAO0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFRAO1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFRAO2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFRAO3(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao3_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFRAO4(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao4_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate ifrao_clks[] = {
	/* IFRAO0 */
	GATE_IFRAO0(CLK_IFRAO_PMIC_TMR, "ifrao_pmic_tmr",
			"fpwrap_ulposc_ck"/* parent */, 0),
	GATE_IFRAO0(CLK_IFRAO_PMIC_AP, "ifrao_pmic_ap",
			"fpwrap_ulposc_ck"/* parent */, 1),
	GATE_IFRAO0(CLK_IFRAO_PMIC_MD, "ifrao_pmic_md",
			"fpwrap_ulposc_ck"/* parent */, 2),
	GATE_IFRAO0(CLK_IFRAO_PMIC_CONN, "ifrao_pmic_conn",
			"fpwrap_ulposc_ck"/* parent */, 3),
	GATE_IFRAO0(CLK_IFRAO_APXGPT, "ifrao_apxgpt",
			"axi_ck"/* parent */, 6),
	GATE_IFRAO0(CLK_IFRAO_GCE, "ifrao_gce",
			"axi_ck"/* parent */, 8),
	GATE_IFRAO0(CLK_IFRAO_GCE2, "ifrao_gce2",
			"axi_ck"/* parent */, 9),
	GATE_IFRAO0(CLK_IFRAO_THERM, "ifrao_therm",
			"axi_ck"/* parent */, 10),
	GATE_IFRAO0(CLK_IFRAO_I2C_PSEUDO, "ifrao_i2c_pseudo",
			"i2c_ck"/* parent */, 11),
	GATE_IFRAO0(CLK_IFRAO_PWM_HCLK, "ifrao_pwm_hclk",
			"axi_ck"/* parent */, 15),
	GATE_IFRAO0(CLK_IFRAO_PWM1, "ifrao_pwm1",
			"pwm_ck"/* parent */, 16),
	GATE_IFRAO0(CLK_IFRAO_PWM2, "ifrao_pwm2",
			"pwm_ck"/* parent */, 17),
	GATE_IFRAO0(CLK_IFRAO_PWM3, "ifrao_pwm3",
			"pwm_ck"/* parent */, 18),
	GATE_IFRAO0(CLK_IFRAO_PWM4, "ifrao_pwm4",
			"pwm_ck"/* parent */, 19),
	GATE_IFRAO0(CLK_IFRAO_PWM, "ifrao_pwm",
			"pwm_ck"/* parent */, 21),
	GATE_IFRAO0(CLK_IFRAO_UART0, "ifrao_uart0",
			"fuart_ck"/* parent */, 22),
	GATE_IFRAO0(CLK_IFRAO_UART1, "ifrao_uart1",
			"fuart_ck"/* parent */, 23),
	GATE_IFRAO0(CLK_IFRAO_UART2, "ifrao_uart2",
			"fuart_ck"/* parent */, 24),
	GATE_IFRAO0(CLK_IFRAO_UART3, "ifrao_uart3",
			"fuart_ck"/* parent */, 25),
	GATE_IFRAO0(CLK_IFRAO_GCE_26M, "ifrao_gce_26m",
			"axi_ck"/* parent */, 27),
	GATE_IFRAO0(CLK_IFRAO_BTIF, "ifrao_btif",
			"axi_ck"/* parent */, 31),
	/* IFRAO1 */
	GATE_IFRAO1(CLK_IFRAO_SPI0, "ifrao_spi0",
			"spi_ck"/* parent */, 1),
	GATE_IFRAO1(CLK_IFRAO_MSDC0, "ifrao_msdc0",
			"axi_ck"/* parent */, 2),
	GATE_IFRAO1(CLK_IFRAO_MSDC1, "ifrao_msdc1",
			"axi_ck"/* parent */, 4),
	GATE_IFRAO1(CLK_IFRAO_MSDC0_SRC, "ifrao_msdc0_clk",
			"msdc50_0_ck"/* parent */, 6),
	GATE_IFRAO1(CLK_IFRAO_AUXADC, "ifrao_auxadc",
			"f26m_ck"/* parent */, 10),
	GATE_IFRAO1(CLK_IFRAO_CPUM, "ifrao_cpum",
			"axi_ck"/* parent */, 11),
	GATE_IFRAO1(CLK_IFRAO_CCIF1_AP, "ifrao_ccif1_ap",
			"axi_ck"/* parent */, 12),
	GATE_IFRAO1(CLK_IFRAO_CCIF1_MD, "ifrao_ccif1_md",
			"axi_ck"/* parent */, 13),
	GATE_IFRAO1(CLK_IFRAO_AUXADC_MD, "ifrao_auxadc_md",
			"f26m_ck"/* parent */, 14),
	GATE_IFRAO1(CLK_IFRAO_MSDC1_SRC, "ifrao_msdc1_clk",
			"msdc30_1_ck"/* parent */, 16),
	GATE_IFRAO1(CLK_IFRAO_MSDC0_AES, "ifrao_msdc0_aes_clk",
			"msdc50_0_ck"/* parent */, 17),
	GATE_IFRAO1(CLK_IFRAO_DEVICE_APC, "ifrao_dapc",
			"axi_ck"/* parent */, 20),
	GATE_IFRAO1(CLK_IFRAO_CCIF_AP, "ifrao_ccif_ap",
			"axi_ck"/* parent */, 23),
	GATE_IFRAO1(CLK_IFRAO_AUDIO, "ifrao_audio",
			"axi_ck"/* parent */, 25),
	GATE_IFRAO1(CLK_IFRAO_CCIF_MD, "ifrao_ccif_md",
			"axi_ck"/* parent */, 26),
	/* IFRAO2 */
	GATE_IFRAO2(CLK_IFRAO_SSUSB, "ifrao_ssusb",
			"fusb_ck"/* parent */, 1),
	GATE_IFRAO2(CLK_IFRAO_DISP_PWM, "ifrao_disp_pwm",
			"fdisp_pwm_ck"/* parent */, 2),
	GATE_IFRAO2(CLK_IFRAO_CLDMA_BCLK, "ifrao_cldmabclk",
			"axi_ck"/* parent */, 3),
	GATE_IFRAO2(CLK_IFRAO_AUDIO_26M_BCLK, "ifrao_audio26m",
			"f26m_ck"/* parent */, 4),
	GATE_IFRAO2(CLK_IFRAO_SPI1, "ifrao_spi1",
			"spi_ck"/* parent */, 6),
	GATE_IFRAO2(CLK_IFRAO_SPI2, "ifrao_spi2",
			"spi_ck"/* parent */, 9),
	GATE_IFRAO2(CLK_IFRAO_SPI3, "ifrao_spi3",
			"spi_ck"/* parent */, 10),
	GATE_IFRAO2(CLK_IFRAO_UNIPRO_SYSCLK, "ifrao_unipro_sysclk",
			"ufs_ck"/* parent */, 11),
	GATE_IFRAO2(CLK_IFRAO_UFS_MP_SAP_BCLK, "ifrao_ufs_bclk",
			"f26m_ck"/* parent */, 13),
	GATE_IFRAO2(CLK_IFRAO_APDMA, "ifrao_apdma",
			"axi_ck"/* parent */, 18),
	GATE_IFRAO2(CLK_IFRAO_SPI4, "ifrao_spi4",
			"spi_ck"/* parent */, 25),
	GATE_IFRAO2(CLK_IFRAO_SPI5, "ifrao_spi5",
			"spi_ck"/* parent */, 26),
	GATE_IFRAO2(CLK_IFRAO_CQ_DMA, "ifrao_cq_dma",
			"axi_ck"/* parent */, 27),
	GATE_IFRAO2(CLK_IFRAO_UFS, "ifrao_ufs",
			"ufs_ck"/* parent */, 28),
	GATE_IFRAO2(CLK_IFRAO_AES_UFSFDE, "ifrao_aes_ufsfde",
			"aes_ufsfde_ck"/* parent */, 29),
	GATE_IFRAO2(CLK_IFRAO_SSUSB_XHCI, "ifrao_ssusb_xhci",
			"fssusb_xhci_ck"/* parent */, 31),
	/* IFRAO3 */
	GATE_IFRAO3(CLK_IFRAO_AP_MSDC0, "ifrao_ap_msdc0",
			"msdc50_0_ck"/* parent */, 7),
	GATE_IFRAO3(CLK_IFRAO_MD_MSDC0, "ifrao_md_msdc0",
			"msdc50_0_ck"/* parent */, 8),
	GATE_IFRAO3(CLK_IFRAO_CCIF5_MD, "ifrao_ccif5_md",
			"axi_ck"/* parent */, 10),
	GATE_IFRAO3(CLK_IFRAO_CCIF2_AP, "ifrao_ccif2_ap",
			"axi_ck"/* parent */, 16),
	GATE_IFRAO3(CLK_IFRAO_CCIF2_MD, "ifrao_ccif2_md",
			"axi_ck"/* parent */, 17),
	GATE_IFRAO3(CLK_IFRAO_FBIST2FPC, "ifrao_fbist2fpc",
			"msdc50_0_ck"/* parent */, 24),
	GATE_IFRAO3(CLK_IFRAO_DPMAIF_MAIN, "ifrao_dpmaif_main",
			"dpmaif_main_ck"/* parent */, 26),
	GATE_IFRAO3(CLK_IFRAO_CCIF4_MD, "ifrao_ccif4_md",
			"axi_ck"/* parent */, 29),
	GATE_IFRAO3(CLK_IFRAO_SPI6_CK, "ifrao_spi6_ck",
			"spi_ck"/* parent */, 30),
	GATE_IFRAO3(CLK_IFRAO_SPI7_CK, "ifrao_spi7_ck",
			"spi_ck"/* parent */, 31),
	/* IFRAO4 */
	GATE_IFRAO4(CLK_IFRAO_RG_AES_CK_0P_CK, "ifrao_aes_0p_ck",
			"aes_msdcfde_ck"/* parent */, 18),
};

#define PLL(_id, _name, _reg, _en_reg, _en_mask,		\
			_pwr_reg, _flags, _rst_bar_mask,		\
			_pd_reg, _pd_shift, _tuner_reg,			\
			_tuner_en_reg, _tuner_en_bit,			\
			_pcw_reg, _pcw_shift, _pcwbits) {		\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.en_reg = _en_reg,					\
		.en_mask = _en_mask,					\
		.pwr_reg = _pwr_reg,					\
		.flags = (_flags | PLL_CFLAGS),				\
		.rst_bar_mask = _rst_bar_mask,				\
		.fmax = MT6877_PLL_FMAX,				\
		.fmin = MT6877_PLL_FMIN,				\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,			\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6877_INTEGER_BITS,			\
	}

#define PLL_B(_id, _name, _reg, _en_mask, _pwr_reg, _flags,		\
			_rst_bar_mask, _pd_reg, _pd_shift,		\
			_tuner_reg, _tuner_en_reg, _tuner_en_bit,	\
			_pcw_reg, _pcw_shift, _pcwbits)			\
		PLL(_id, _name, _reg, 0, _en_mask, 0,			\
			_pwr_reg,	 _flags, _rst_bar_mask,		\
			_pd_reg, _pd_shift, _tuner_reg,			\
			_tuner_en_reg, _tuner_en_bit,			\
			_pcw_reg, _pcw_shift, _pcwbits)

static const struct mtk_pll_data apmixed_plls[] = {
	PLL(CLK_APMIXED_ARMPLL_LL, "armpll_ll", ARMPLL_LL_CON0/*base*/,
		ARMPLL_LL_CON0, BIT(0)/*en*/,
		ARMPLL_LL_CON3/*pwr*/, PLL_AO, BIT(0)/*rstb*/,
		ARMPLL_LL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		ARMPLL_LL_CON1, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_ARMPLL_BL, "armpll_bl", ARMPLL_BL_CON0/*base*/,
		ARMPLL_BL_CON0, BIT(0)/*en*/,
		ARMPLL_BL_CON3/*pwr*/, PLL_AO, BIT(0)/*rstb*/,
		ARMPLL_BL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		ARMPLL_BL_CON1, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_CCIPLL, "ccipll", CCIPLL_CON0/*base*/,
		CCIPLL_CON0, BIT(0)/*en*/,
		CCIPLL_CON3/*pwr*/, PLL_AO, BIT(0)/*rstb*/,
		CCIPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		CCIPLL_CON1, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_MAINPLL, "mainpll", MAINPLL_CON0/*base*/,
		MAINPLL_CON0, BIT(0)/*en*/,
		MAINPLL_CON3/*pwr*/, HAVE_RST_BAR | PLL_AO, BIT(23)/*rstb*/,
		MAINPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MAINPLL_CON1, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_UNIVPLL, "univpll", UNIVPLL_CON0/*base*/,
		UNIVPLL_CON0, BIT(0)/*en*/,
		UNIVPLL_CON3/*pwr*/, HAVE_RST_BAR, BIT(23)/*rstb*/,
		UNIVPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		UNIVPLL_CON1, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_MSDCPLL, "msdcpll", MSDCPLL_CON0/*base*/,
		MSDCPLL_CON0, BIT(0)/*en*/,
		MSDCPLL_CON3/*pwr*/, 0, BIT(0)/*rstb*/,
		MSDCPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MSDCPLL_CON1, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_MMPLL, "mmpll", MMPLL_CON0/*base*/,
		MMPLL_CON0, BIT(0)/*en*/,
		MMPLL_CON3/*pwr*/, HAVE_RST_BAR, BIT(23)/*rstb*/,
		MMPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MMPLL_CON1, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_ADSPPLL, "adsppll", ADSPPLL_CON0/*base*/,
		ADSPPLL_CON0, BIT(0)/*en*/,
		ADSPPLL_CON3/*pwr*/, 0, BIT(0)/*rstb*/,
		ADSPPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		ADSPPLL_CON1, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_TVDPLL, "tvdpll", TVDPLL_CON0/*base*/,
		TVDPLL_CON0, BIT(0)/*en*/,
		TVDPLL_CON3/*pwr*/, 0, BIT(0)/*rstb*/,
		TVDPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		TVDPLL_CON1, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_APLL1, "apll1", APLL1_CON0/*base*/,
		APLL1_CON0, BIT(0)/*en*/,
		APLL1_CON4/*pwr*/, 0, BIT(0)/*rstb*/,
		APLL1_CON1, 24/*pd*/,
		0, AP_PLL_CON3, 0/*tuner*/,
		APLL1_CON2, 0, 32/*pcw*/),
	PLL(CLK_APMIXED_APLL2, "apll2", APLL2_CON0/*base*/,
		APLL2_CON0, BIT(0)/*en*/,
		APLL2_CON4/*pwr*/, 0, BIT(0)/*rstb*/,
		APLL2_CON1, 24/*pd*/,
		0, AP_PLL_CON3, 5/*tuner*/,
		APLL2_CON2, 0, 32/*pcw*/),
	PLL(CLK_APMIXED_MPLL, "mpll", MPLL_CON0/*base*/,
		MPLL_CON0, BIT(0)/*en*/,
		MPLL_CON3/*pwr*/, PLL_AO, BIT(0)/*rstb*/,
		MPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MPLL_CON1, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_USBPLL, "usbpll", USBPLL_CON0/*base*/,
		USBPLL_CON0, BIT(0)/*en*/,
		USBPLL_CON3/*pwr*/, 0, BIT(0)/*rstb*/,
		USBPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		USBPLL_CON1, 0, 22/*pcw*/),
};

static const struct mtk_pll_data lp5_apmixed_plls[] = {
	PLL(CLK_APMIXED_ARMPLL_LL, "armpll_ll", ARMPLL_LL_CON0/*base*/,
		ARMPLL_LL_CON0, BIT(0)/*en*/,
		ARMPLL_LL_CON3/*pwr*/, PLL_AO, BIT(0)/*rstb*/,
		ARMPLL_LL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		ARMPLL_LL_CON1, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_ARMPLL_BL, "armpll_bl", ARMPLL_BL_CON0/*base*/,
		ARMPLL_BL_CON0, BIT(0)/*en*/,
		ARMPLL_BL_CON3/*pwr*/, PLL_AO, BIT(0)/*rstb*/,
		ARMPLL_BL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		ARMPLL_BL_CON1, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_CCIPLL, "ccipll", CCIPLL_CON0/*base*/,
		CCIPLL_CON0, BIT(0)/*en*/,
		CCIPLL_CON3/*pwr*/, PLL_AO, BIT(0)/*rstb*/,
		CCIPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		CCIPLL_CON1, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_MAINPLL, "mainpll", MAINPLL_CON0/*base*/,
		MAINPLL_CON0, BIT(0)/*en*/,
		MAINPLL_CON3/*pwr*/, HAVE_RST_BAR | PLL_AO, BIT(23)/*rstb*/,
		MAINPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MAINPLL_CON1, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_UNIVPLL, "univpll", IMGPLL_CON0/*base*/,
		IMGPLL_CON0, BIT(0)/*en*/,
		IMGPLL_CON3/*pwr*/, 0, BIT(0)/*rstb*/,
		UNIVPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		UNIVPLL_CON1, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_MSDCPLL, "msdcpll", MSDCPLL_CON0/*base*/,
		MSDCPLL_CON0, BIT(0)/*en*/,
		MSDCPLL_CON3/*pwr*/, 0, BIT(0)/*rstb*/,
		MSDCPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MSDCPLL_CON1, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_MMPLL, "mmpll", MMPLL_CON0/*base*/,
		MMPLL_CON0, BIT(0)/*en*/,
		MMPLL_CON3/*pwr*/, HAVE_RST_BAR, BIT(23)/*rstb*/,
		MMPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MMPLL_CON1, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_ADSPPLL, "adsppll", ADSPPLL_CON0/*base*/,
		ADSPPLL_CON0, BIT(0)/*en*/,
		ADSPPLL_CON3/*pwr*/, 0, BIT(0)/*rstb*/,
		ADSPPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		ADSPPLL_CON1, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_TVDPLL, "tvdpll", TVDPLL_CON0/*base*/,
		TVDPLL_CON0, BIT(0)/*en*/,
		TVDPLL_CON3/*pwr*/, 0, BIT(0)/*rstb*/,
		TVDPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		TVDPLL_CON1, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_APLL1, "apll1", APLL1_CON0/*base*/,
		APLL1_CON0, BIT(0)/*en*/,
		APLL1_CON4/*pwr*/, 0, BIT(0)/*rstb*/,
		APLL1_CON1, 24/*pd*/,
		0, AP_PLL_CON3, 0/*tuner*/,
		APLL1_CON2, 0, 32/*pcw*/),
	PLL(CLK_APMIXED_APLL2, "apll2", APLL2_CON0/*base*/,
		APLL2_CON0, BIT(0)/*en*/,
		APLL2_CON4/*pwr*/, 0, BIT(0)/*rstb*/,
		APLL2_CON1, 24/*pd*/,
		0, AP_PLL_CON3, 5/*tuner*/,
		APLL2_CON2, 0, 32/*pcw*/),
	PLL(CLK_APMIXED_MPLL, "mpll", MPLL_CON0/*base*/,
		MPLL_CON0, BIT(0)/*en*/,
		MPLL_CON3/*pwr*/, PLL_AO, BIT(0)/*rstb*/,
		MPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MPLL_CON1, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_USBPLL, "usbpll", USBPLL_CON0/*base*/,
		USBPLL_CON0, BIT(0)/*en*/,
		USBPLL_CON3/*pwr*/, 0, BIT(0)/*rstb*/,
		USBPLL_CON1, 24/*pd*/,
		0, 0, 0/*tuner*/,
		USBPLL_CON1, 0, 22/*pcw*/),
};

int clk_mt6877_pll_registration(enum subsys_id id,
		const struct mtk_pll_data *plls,
		struct platform_device *pdev,
		int num_plls)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(num_plls);

	mtk_clk_register_plls(node, plls, num_plls,
			clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	plls_data[id] = plls;
	plls_base[id] = base;

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static int clk_mt6877_apmixed_probe(struct platform_device *pdev)
{
	struct device_node *node;
	unsigned int dram_type;
	int ret = 0;

	node = of_find_compatible_node(NULL, NULL,
		"mediatek,mt6877-dramc");
	if (node) {
		ret = of_property_read_u32(node, "dram_type", &dram_type);
		if (ret) {
			pr_err("%s(): get property failed\n", __func__);
			return -EINVAL;
		}

		if (dram_type == TYPE_LPDDR5)
			ret = clk_mt6877_pll_registration(APMIXEDSYS,
					lp5_apmixed_plls, pdev, ARRAY_SIZE(apmixed_plls));
		else
			ret = clk_mt6877_pll_registration(APMIXEDSYS,
					apmixed_plls, pdev, ARRAY_SIZE(apmixed_plls));
	} else {
		pr_err("%s(): get dram node failed\n", __func__);
		return PTR_ERR(node);
	}

	return ret;
}

static int clk_mt6877_ifrao_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_IFRAO_NR_CLK);

	mtk_clk_register_gates(node, ifrao_clks, ARRAY_SIZE(ifrao_clks),
			clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static struct clk_onecell_data *mt6877_top_clk_data;

static int clk_mt6877_top_probe(struct platform_device *pdev)
{
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	mt6877_top_clk_data = mtk_alloc_clk_data(CLK_TOP_NR_CLK);

	mtk_clk_register_factors(top_divs, ARRAY_SIZE(top_divs),
			mt6877_top_clk_data);

	mtk_clk_register_muxes(top_muxes, ARRAY_SIZE(top_muxes), node,
			&mt6877_clk_lock, mt6877_top_clk_data);

	mtk_clk_register_composites(top_composites, ARRAY_SIZE(top_composites),
			base, &mt6877_clk_lock, mt6877_top_clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get,
			mt6877_top_clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

/* for suspend LDVT only */
void pll_force_off_internal(const struct mtk_pll_data *plls,
		void __iomem *base)
{
	void __iomem *rst_reg, *en_reg, *pwr_reg;

	for (; plls->name; plls++) {
		/* do not pwrdn the AO PLLs */
		if ((plls->flags & PLL_AO) == PLL_AO)
			continue;

		if ((plls->flags & HAVE_RST_BAR) == HAVE_RST_BAR) {
			rst_reg = base + plls->rst_bar_reg;
			writel(readl(rst_reg) & ~plls->rst_bar_mask,
				rst_reg);
		}

		en_reg = base + plls->en_reg;

		pwr_reg = base + plls->pwr_reg;

		writel(readl(en_reg) & ~plls->en_mask,
				en_reg);
		writel(readl(pwr_reg) | plls->iso_mask,
				pwr_reg);
		writel(readl(pwr_reg) & ~plls->pwron_mask,
				pwr_reg);
	}
}

void pll_force_off(void)
{
	int i;

	for (i = 0; i < PLL_SYS_NUM; i++)
		pll_force_off_internal(plls_data[i], plls_base[i]);
}

static const struct of_device_id of_match_clk_mt6877[] = {
	{
		.compatible = "mediatek,mt6877-apmixedsys",
		.data = clk_mt6877_apmixed_probe,
	}, {
		.compatible = "mediatek,mt6877-infracfg_ao",
		.data = clk_mt6877_ifrao_probe,
	}, {
		.compatible = "mediatek,mt6877-topckgen",
		.data = clk_mt6877_top_probe,
	}, {
		/* sentinel */
	}
};

static int clk_mt6877_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6877_drv = {
	.probe = clk_mt6877_probe,
	.driver = {
		.name = "clk-mt6877",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt6877,
	},
};

static int __init clk_mt6877_init(void)
{
	return platform_driver_register(&clk_mt6877_drv);
}

arch_initcall_sync(clk_mt6877_init);

