/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/delay.h>
#include <linux/of_address.h>

#include "clkdbg.h"
#include "clk-fmeter.h"

static DEFINE_SPINLOCK(meter_lock);
#define fmeter_lock(flags)   spin_lock_irqsave(&meter_lock, flags)
#define fmeter_unlock(flags) spin_unlock_irqrestore(&meter_lock, flags)

/*
 * clkdbg fmeter
 */

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */

#define FMCLK(_t, _i, _n) { .type = _t, .id = _i, .name = _n }

static const struct fmeter_clk fclks[] = {
	FMCLK(CKGEN,	1, "axi_sel"),
	FMCLK(CKGEN,	2, "spm_sel"),
	FMCLK(CKGEN,	3, "scp_sel"),
	FMCLK(CKGEN,	4, "bus_aximem_sel"),
	FMCLK(CKGEN,	5, "mm_sel"),
	FMCLK(CKGEN,	6, "mdp_sel"),
	FMCLK(CKGEN,	7, "img1_sel"),
	FMCLK(CKGEN,	8, "img2_sel"),
	FMCLK(CKGEN,	9, "ipe_sel"),
	FMCLK(CKGEN,	10, "dpe_sel"),
	FMCLK(CKGEN,	11, "cam_sel"),
	FMCLK(CKGEN,	12, "ccu_sel"),
	FMCLK(CKGEN,	13, "dsp_sel"),
	FMCLK(CKGEN,	14, "dsp1_sel"),
	FMCLK(CKGEN,	15, "dsp2_sel"),
	FMCLK(CKGEN,	16, "dsp5_sel"),
	FMCLK(CKGEN,	17, "dsp7_sel"),
	FMCLK(CKGEN,	18, "ipu_if_sel"),
	FMCLK(CKGEN,	19, "mfg_sel"),
	FMCLK(CKGEN,	20, "camtg_sel"),
	FMCLK(CKGEN,	21, "camtg2_sel"),
	FMCLK(CKGEN,	22, "camtg3_sel"),
	FMCLK(CKGEN,	23, "camtg4_sel"),
	FMCLK(CKGEN,	24, "camtg5_sel"),
	FMCLK(CKGEN,	25, "camtg6_sel"),
	FMCLK(CKGEN,	26, "uart_sel"),
	FMCLK(CKGEN,	27, "spi_sel"),
	FMCLK(CKGEN,	28, "msdc50_0_hclk_sel"),
	FMCLK(CKGEN,	29, "msdc50_0_sel"),
	FMCLK(CKGEN,	30, "msdc30_1_sel"),
	FMCLK(CKGEN,	31, "msdc30_2_sel"),
	FMCLK(CKGEN,	32, "audio_sel"),
	FMCLK(CKGEN,	33, "aud_intbus_sel"),
	FMCLK(CKGEN,	34, "pwrap_ulposc_sel"),
	FMCLK(CKGEN,	35, "atb_sel"),
	FMCLK(CKGEN,	36, "p_w_r_mcu_sel"),
	FMCLK(CKGEN,	37, "dpi_sel"),
	FMCLK(CKGEN,	38, "scam_sel"),
	FMCLK(CKGEN,	39, "disp_pwm_sel"),
	FMCLK(CKGEN,	40, "usb_top_sel"),
	FMCLK(CKGEN,	41, "ssusb_xhci_sel"),
	FMCLK(CKGEN,	42, "i2c_sel"),
	FMCLK(CKGEN,	43, "seninf_sel"),
	FMCLK(CKGEN,	44, "seninf1_sel"),
	FMCLK(CKGEN,	45, "seninf2_sel"),
	FMCLK(CKGEN,	46, "seninf3_sel"),
	FMCLK(CKGEN,	47, "tl_sel"),
	FMCLK(CKGEN,	48, "dxcc_sel"),
	FMCLK(CKGEN,	49, "aud_engen1_sel"),
	FMCLK(CKGEN,	50, "aud_engen2_sel"),
	FMCLK(CKGEN,	51, "aes_ufsfde_sel"),
	FMCLK(CKGEN,	52, "ufs_sel"),
	FMCLK(CKGEN,	53, "aud_1_sel"),
	FMCLK(CKGEN,	54, "aud_2_sel"),
	FMCLK(CKGEN,	55, "adsp_sel"),
	FMCLK(CKGEN,	56, "dpmaif_main_sel"),
	FMCLK(CKGEN,	57, "venc_sel"),
	FMCLK(CKGEN,	58, "vdec_sel"),
	FMCLK(CKGEN,	59, "camtm_sel"),
	FMCLK(CKGEN,	60, "pwm_sel"),
	FMCLK(CKGEN,	61, "audio_h_sel"),
	FMCLK(CKGEN,	62, "spmi_mst_sel"),
	FMCLK(CKGEN,	63, "dvfsrc_sel"),

	FMCLK(ABIST,	1,	"AD_ADSPPLL_CK"),
	FMCLK(ABIST,	2,	"AD_APLL1_CK"),
	FMCLK(ABIST,	3,	"AD_APLL2_CK"),
	FMCLK(ABIST,	4,	"AD_APPLLGP_MON_FM_CK"),
	FMCLK(ABIST,	5,	"AD_APUPLL_CK"),
	FMCLK(ABIST,	6,	"AD_ARMPLL_BL_CK"),
	FMCLK(ABIST,	7,	"AD_NPUPLL_CK"),
	FMCLK(ABIST,	10,	"AD_ARMPLL_LL_CK"),
	FMCLK(ABIST,	11,	"AD_CCIPLL_CK"),
	FMCLK(ABIST,	12,	"AD_CSI0A_CDPHY_DELAYCAL_CK"),
	FMCLK(ABIST,	13,	"AD_CSI0B_CDPHY_DELAYCAL_CK"),
	FMCLK(ABIST,	14,	"AD_CSI1A_DPHY_DELAYCAL_CK"),
	FMCLK(ABIST,	15,	"AD_CSI1B_DPHY_DELAYCAL_CK"),
	FMCLK(ABIST,	16,	"AD_CSI2A_DPHY_DELAYCAL_CK"),
	FMCLK(ABIST,	17,	"AD_CSI2B_DPHY_DELAYCAL_CK"),
	FMCLK(ABIST,	18,	"AD_CSI3A_DPHY_DELAYCAL_CK"),
	FMCLK(ABIST,	19,	"AD_CSI3B_DPHY_DELAYCAL_CK"),
	FMCLK(ABIST,	20,	"AD_DSI0_LNTC_DSICLK"),
	FMCLK(ABIST,	21,	"AD_DSI0_MPPLL_TST_CK"),
	FMCLK(ABIST,	23,	"mfgpll_ck"),
	FMCLK(ABIST,	24,	"AD_MAINPLL_CK"),
	FMCLK(ABIST,	25,	"AD_MDPLL_FS26M_CK"),
	FMCLK(ABIST,	26,	"AD_MFGPLL_CK"),
	FMCLK(ABIST,	27,	"AD_MMPLL_CK"),
	FMCLK(ABIST,	28,	"AD_MMPLL_D3_CK"),
	FMCLK(ABIST,	29,	"AD_MPLL_CK"),
	FMCLK(ABIST,	30,	"AD_MSDCPLL_CK"),
	FMCLK(ABIST,	31,	"AD_RCLRPLL_DIV4_CK_ch02"),
	FMCLK(ABIST,	32,	"AD_RCLRPLL_DIV4_CK_ch13"),
	FMCLK(ABIST,	33,	"AD_RPHYPLL_DIV4_CK_ch02"),
	FMCLK(ABIST,	34,	"AD_RPHYPLL_DIV4_CK_ch13"),
	FMCLK(ABIST,	35,	"AD_TVDPLL_CK"),
	FMCLK(ABIST,	36,	"AD_ULPOSC2_CK"),
	FMCLK(ABIST,	37,	"AD_ULPOSC_CK"),
	FMCLK(ABIST,	38,	"AD_UNIVPLL_CK"),
	FMCLK(ABIST,	39,	"AD_USB20_192M_CK"),
	FMCLK(ABIST,	40,	"AD_USBPLL_192M_CK"),
	FMCLK(ABIST,	41,	"UFS_MP_CLK2FREQ"),
	FMCLK(ABIST,	42,	"ad_wbg_dig_bpll_ck"),
	FMCLK(ABIST,	43,	"ad_wbg_dig_wpll_ck960"),
	FMCLK(ABIST,	44,	"fmem_ck_aft_dcm_ch0"),
	FMCLK(ABIST,	45,	"fmem_ck_aft_dcm_ch1"),
	FMCLK(ABIST,	46,	"fmem_ck_aft_dcm_ch2"),
	FMCLK(ABIST,	47,	"fmem_ck_aft_dcm_ch3"),
	FMCLK(ABIST,	48,	"fmem_ck_bfe_dcm_ch0"),
	FMCLK(ABIST,	49,	"fmem_ck_bfe_dcm_ch1"),
	FMCLK(ABIST,	50,	"hd_466m_fmem_ck_infrasys"),
	FMCLK(ABIST,	51,	"mcusys_arm_clk_out_all"),
	FMCLK(ABIST,	52,	"msdc01_in_ck"),
	FMCLK(ABIST,	53,	"msdc02_in_ck"),
	FMCLK(ABIST,	54,	"msdc11_in_ck"),
	FMCLK(ABIST,	55,	"msdc12_in_ck"),
	FMCLK(ABIST,	56,	"msdc21_in_ck"),
	FMCLK(ABIST,	57,	"msdc22_in_ck"),
	FMCLK(ABIST,	58,	"rtc32k_ck_i_vao"),
	FMCLK(ABIST,	60,	"ckomo1_ck"),
	FMCLK(ABIST,	61,	"ckmon2_ck"),
	FMCLK(ABIST,	62,	"ckmon3_ck"),
	FMCLK(ABIST,	63,	"ckmon4_ck"),
	{}
};

#define _CKGEN(x)		(topck_base + (x))
#define CLK_MISC_CFG_0		_CKGEN(0x140)
#define CLK_DBG_CFG		_CKGEN(0x17C)
#define CLK26CALI_0		_CKGEN(0x220)
#define CLK26CALI_1		_CKGEN(0x224)

static void __iomem *topck_base;

const struct fmeter_clk *get_fmeter_clks(void)
{
	return fclks;
}

static unsigned int mux_table[64][2] = {
	/* ID offset pdn_bit */
	{0, 0}, //dummy index
	{0x10, 7},//axi=1
	{0x10, 15},//spm
	{0x10, 23},//scp
	{0x10, 31},//aximem

	{0x20, 7},//disp
	{0x20, 15},//mdp
	{0x20, 23},//img1
	{0x20, 31},//img2

	{0x30, 7},//ipe
	{0x30, 15},//dpe
	{0x30, 23},//cam
	{0x30, 31},//ccu

	{0x40, 7},//dsp
	{0x40, 15},//dsp1
	{0x40, 23},//dsp2
	{0x40, 31},//dsp3

	{0x50, 7},//dsp7
	{0x50, 15},//ipu_if
	{0x50, 23},//mfg
	{0x50, 31},//camtg

	{0x60, 7},//camtg2
	{0x60, 15},//camtg3
	{0x60, 23},//camtg4
	{0x60, 31},//camtg5

	{0x70, 7},//camtg6
	{0x70, 15},//uart
	{0x70, 23},//spi
	{0x70, 31},//msdc50_0_hclk

	{0x80, 7},//msdc50_0
	{0x80, 15},//msdc30_1
	{0x80, 23},//msdc30_2
	{0x80, 31},//audio

	{0x90, 7},//aud_intbus
	{0x90, 15},//pwrap_ulposc
	{0x90, 23},//atb
	{0x90, 31},//sspm

	{0xA0, 7},//dpi
	{0xA0, 15},//scam
	{0xA0, 23},//disp_pwm
	{0xA0, 31},//usb top

	{0xB0, 7},//ssusb xhci
	{0xB0, 15},//i2c
	{0xB0, 23},//seninf
	{0xB0, 31},//seninf1

	{0xC0, 7},//seninf2
	{0xC0, 15},//seninf3
	{0xC0, 23},//tl
	{0xC0, 31},//dxcc

	{0xD0, 7},//aud_engen1=49
	{0xD0, 15},//aud_engen2
	{0xD0, 23},//aes ufsfde
	{0xD0, 31},//ufs

	{0xE0, 7},//aud_1
	{0xE0, 15},//aud_2
	{0xE0, 23},//adsp
	{0xE0, 31},//dpmaif

	{0xF0, 7},//venc
	{0xF0, 15},//vdec
	{0xF0, 23},//camtm
	{0xF0, 31},//pwm

	{0x100, 7},//audioh
	{0x100, 15},//spmi
	{0x100, 23},//dvfsrc=63
#if 0
	{0x100, 31},//aesmsdc
	{0x110, 7},//mcupm
	{0x110, 15},//sflash
#endif
};

unsigned int check_mux_pdn(unsigned int ID)
{
#if 0
	pr_notice("%s: ID=%d, check:%08x, %08x(%08x)\r\n",
			__func__, ID, mux_table[ID][0], BIT(mux_table[ID][1]),
			clk_readl(cksys_base + mux_table[ID][0])
				& BIT(mux_table[ID][1]));
#endif
	if ((ID > 0) && (ID < 64)) {
		if ((clk_readl(_CKGEN(mux_table[ID][0]))
		& BIT(mux_table[ID][1])))
			return 1;
		else
			return 0;
	} else
		return 1;
}

unsigned int mt_get_ckgen_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1 = 0;
	unsigned long flags;

	if (check_mux_pdn(ID)) {
		//pr_notice("ID-%d: MUX PDN, return 0.\r\n", ID);
		return 0;
	}
	fmeter_lock(flags);
	//while (clk_readl(CLK26CALI_0) & 0x1000)
	//	;

	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFFFC0FC)|(ID << 8)|(0x1));

	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF));

	clk26cali_1 = clk_readl(CLK26CALI_1);
	clk_writel(CLK26CALI_0, 0x1000);
	clk_writel(CLK26CALI_0, 0x1010);

	/* wait frequency meter finish */
	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(10);
		i++;
		if (i > 20)
			break;
	}
	/* illegal pass */
	if (i == 0) {
		clk_writel(CLK26CALI_0, 0x0000);
		//re-trigger
		clk_writel(CLK26CALI_0, 0x1000);
		clk_writel(CLK26CALI_0, 0x1010);
		while (clk_readl(CLK26CALI_0) & 0x10) {
			udelay(10);
			i++;
			if (i > 20)
				break;
		}
	}

	temp = clk_readl(CLK26CALI_1) & 0xFFFF;

	output = (temp * 26000) / 1024;

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	/*clk_writel(CLK26CALI_0, clk26cali_0);*/
	/*clk_writel(CLK26CALI_1, clk26cali_1);*/

	clk_writel(CLK26CALI_0, 0x0000);
	fmeter_unlock(flags);
	if (i > 20)
		return 0;
	else
		return output;
}

unsigned int mt_get_abist_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1 = 0;
	unsigned long flags;

	fmeter_lock(flags);
	//while (clk_readl(CLK26CALI_0) & 0x1000)
		//;
	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFC0FFFC)|(ID << 16));

	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF) | (1 << 24));

	clk26cali_1 = clk_readl(CLK26CALI_1);

	clk_writel(CLK26CALI_0, 0x1000);
	clk_writel(CLK26CALI_0, 0x1010);

	/* wait frequency meter finish */
	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(10);
		i++;
		if (i > 20)
			break;
	}
	/* illegal pass */
	if (i == 0) {
		clk_writel(CLK26CALI_0, 0x0000);
		//re-trigger
		clk_writel(CLK26CALI_0, 0x1000);
		clk_writel(CLK26CALI_0, 0x1010);
		while (clk_readl(CLK26CALI_0) & 0x10) {
			udelay(10);
			i++;
			if (i > 20)
				break;
		}
	}
	temp = clk_readl(CLK26CALI_1) & 0xFFFF;

	output = (temp * 26000) / 1024;

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	/*clk_writel(CLK26CALI_0, clk26cali_0);*/
	/*clk_writel(CLK26CALI_1, clk26cali_1);*/
	clk_writel(CLK26CALI_0, 0x0000);
	fmeter_unlock(flags);
	if (i > 20)
		return 0;
	else
		return (output * 2);
}

static int __init clk_fmeter_mt6873_init(void)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL,
		"mediatek,topckgen");
	if (node) {
		topck_base = of_iomap(node, 0);
		if (!topck_base) {
			pr_notice("%s() can't find iomem for topckgen\n",
					__func__);
			return -1;
		}
	} else {
		pr_notice("%s can't find compatible node for topckgen\n",
				__func__);
		return -1;
	}

	return 0;
}
subsys_initcall(clk_fmeter_mt6873_init);
