/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Owen Chen <Owen.Chen@mediatek.com>
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

#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/mfd/syscon.h>

#include "clk-mtk.h"
#include "clk-gate.h"
#include "clk-mux.h"

#include <dt-bindings/clock/mt6765-clk.h>
#include <mt-plat/mtk_devinfo.h>

#define MT_CCF_BRINGUP		0
#if MT_CCF_BRINGUP
#define MT_MTCMOS_ENABLE	0
#else
#define MT_MTCMOS_ENABLE	1
#endif

#define LOW_POWER_CLK_PDN	0
/*fmeter div select 4*/
#define _DIV4_ 1

#ifdef CONFIG_ARM64
#define IOMEM(a)	((void __force __iomem *)((a)))
#endif

#define mt_reg_sync_writel(v, a) \
	do { \
		__raw_writel((v), IOMEM(a)); \
		/* insure updates are written */ \
		mb(); } \
while (0)

#define clk_readl(addr)		__raw_readl(IOMEM(addr))

#define clk_writel(addr, val)   \
	mt_reg_sync_writel(val, addr)

#define clk_setl(addr, val) \
	mt_reg_sync_writel(clk_readl(addr) | (val), addr)

#define clk_clrl(addr, val) \
	mt_reg_sync_writel(clk_readl(addr) & ~(val), addr)

#define PLL_EN			(0x1 << 0)
#define PLL_PWR_ON		(0x1 << 0)
#define PLL_ISO_EN		(0x1 << 1)
#define PLL_DIV_RSTB		(0x1 << 23)
#define PLL_SDM_PCW_CHG	(0x1 << 31)

const char *ckgen_array[] = {
"hd_faxi_ck",
"hf_fmem_ck",
"hf_fmm_ck",
"hf_fscp_ck",
"hf_fmfg_ck",
"hf_fatb_ck",
"f_fcamtg_ck",
"f_fcamtg1_ck",
"f_fcamtg2_ck",
"f_fcamtg3_ck",
"f_fuart_ck",
"hf_fspi_ck",
"hf_fmsdc50_0_hclk_ck",
"hf_fmsdc50_0_ck",
"hf_fmsdc30_1_ck",
"hf_faudio_ck",
"hf_faud_intbus_ck",
"hf_faud_1_ck",
"hf_faud_engen1_ck",
"f_fdisp_pwm_ck",
"hf_fsspm_ck",
"hf_fdxcc_ck",
"f_fusb_top_ck",
"hf_fspm_ck",
"hf_fi2c_ck",
"f_fpwm_ck",
"f_fseninf_ck",
"hf_faes_fde_ck",
"f_fpwrap_ulposc_ck",
"f_fcamtm_ck",
"NA",
"NA",
"NA",
"NA",
"NA",
"NA",
"NA",
"NA",
"NA",
"NA",
"NA",
"NA",
"NA",
"NA",
"NA",
"NA",
"NA",
"f_ufs_mp_sap_cfg_ck",
"f_ufs_tick1us_ck",
"hd_faxi_east_ck",
"hd_faxi_west_ck",
"hd_faxi_north_ck",
"hd_faxi_south_ck",
"hg_fmipicfg_tx_ck",
"fmem_ck_bfe_dcm_ch0",
"fmem_ck_aft_dcm_ch0",
"fmem_ck_bfe_dcm_ch1",
"fmem_ck_aft_dcm_ch1",
};

const char *abist_array[] = {
"AD_CSI0_DELAY_TSTCLK",
"AD_CSI1_DELAY_TSTCLK",
"UFS_MP_CLK2FREQ",
"AD_MDBPIPLL_DIV3_CK",
"AD_MDBPIPLL_DIV7_CK",
"AD_MDBRPPLL_DIV6_CK",
"AD_UNIV_624M_CK",
"AD_MAIN_H546_CK",
"AD_MAIN_H364M_CK",
"AD_MAIN_H218P4M_CK",
"AD_MAIN_H156M_CK",
"AD_UNIV_624M_CK",
"AD_UNIV_416M_CK",
"AD_UNIV_249P6M_CK",
"AD_UNIV_178P3M_CK",
"AD_MDPLL_FS26M_CK",
"AD_CSI1A_DPHY_DELAYCAL_CK",
"AD_CSI1B_DPHY_DELAYCAL_CK",
"AD_CSI2A_DPHY_DELAYCAL_CK",
"AD_CSI2B_DPHY_DELAYCAL_CK",
"AD_ARMPLL_L_CK",
"AD_ARMPLL_CK",
"AD_MAINPLL_1092M_CK",
"AD_UNIVPLL_1248M_CK",
"AD_MFGPLL_CK",
"AD_MSDCPLL_416M_CK",
"AD_MMPLL_CK",
"AD_APLL1_196P608M_CK",
"NA",
"AD_APPLLGP_TST_CK",
"AD_USB20_192M_CK",
"NA",
"NA",
"AD_VENCPLL_CK",
"AD_DSI0_MPPLL_TST_CK",
"AD_DSI0_LNTC_DSICLK",
"AD_ULPOSC1_CK",
"AD_ULPOSC2_CK",
"rtc32k_ck_i",
"mcusys_arm_clk_out_all",
"AD_ULPOSC1_SYNC_CK",
"AD_ULPOSC2_SYNC_CK",
"msdc01_in_ck",
"msdc02_in_ck",
"msdc11_in_ck",
"msdc12_in_ck",
"NA",
"NA",
"AD_CCIPLL_CK",
"AD_MPLL_208M_CK",
"AD_WBG_DIG_CK_416M",
"AD_WBG_B_DIG_CK_64M",
"AD_WBG_W_DIG_CK_160M",
"DA_USB20_48M_DIV_CK",
"DA_UNIV_48M_DIV_CK",
"DA_MPLL_104M_DIV_CK",
"DA_MPLL_52M_DIV_CK",
"DA_ARMCPU_MON_CK",
"NA",
"ckmon1_ck",
"ckmon2_ck",
"ckmon3_ck",
"ckmon4_ck",
};

static DEFINE_SPINLOCK(mt6765_clk_lock);

/* Total 12 subsys */
void __iomem *cksys_base;
void __iomem *apmixed_base;
void __iomem *mmsys_config_base;

/* CKSYS */
#define CLK_MISC_CFG_0		(cksys_base + 0x104)
#define CLK_MISC_CFG_1		(cksys_base + 0x108)
#define CLK_DBG_CFG		(cksys_base + 0x10C)
#define CLK_SCP_CFG_0		(cksys_base + 0x200)
#define CLK_SCP_CFG_1		(cksys_base + 0x204)
#define CLK26CALI_0		(cksys_base + 0x220)
#define CLK26CALI_1		(cksys_base + 0x224)

/* CG */
#define AP_PLL_CON3		(apmixed_base + 0x0C)
#define PLLON_CON0		(apmixed_base + 0x44)
#define PLLON_CON1		(apmixed_base + 0x48)

#define ARMPLL_CON0		(apmixed_base + 0x20C)
#define ARMPLL_CON1		(apmixed_base + 0x210)
#define ARMPLL_CON2		(apmixed_base + 0x214)
#define ARMPLL_CON3		(apmixed_base + 0x218)

#define ARMPLL_L_CON0		(apmixed_base + 0x21C)
#define ARMPLL_L_CON1		(apmixed_base + 0x220)
#define ARMPLL_L_CON2		(apmixed_base + 0x224)
#define ARMPLL_L_CON3		(apmixed_base + 0x228)

#define CCIPLL_CON0		(apmixed_base + 0x22C)
#define CCIPLL_CON1		(apmixed_base + 0x230)
#define CCIPLL_CON2		(apmixed_base + 0x234)
#define CCIPLL_CON3		(apmixed_base + 0x238)

#define MFGPLL_CON0		(apmixed_base + 0x24C)
#define MFGPLL_CON1		(apmixed_base + 0x250)
#define MFGPLL_CON2		(apmixed_base + 0x254)
#define MFGPLL_CON3	(apmixed_base + 0x258)

#define UNIVPLL_CON0		(apmixed_base + 0x26C)
#define UNIVPLL_CON1		(apmixed_base + 0x270)
#define UNIVPLL_CON2		(apmixed_base + 0x274)
#define UNIVPLL_CON3		(apmixed_base + 0x278)

#define MSDCPLL_CON0		(apmixed_base + 0x27C)
#define MSDCPLL_CON1		(apmixed_base + 0x280)
#define MSDCPLL_CON2		(apmixed_base + 0x284)
#define MSDCPLL_CON3		(apmixed_base + 0x288)

#define APLL1_CON0		(apmixed_base + 0x28C)
#define APLL1_CON1		(apmixed_base + 0x290)
#define APLL1_CON2		(apmixed_base + 0x294)
#define APLL1_CON3		(apmixed_base + 0x298)
#define APLL1_CON4		(apmixed_base + 0x29C)

/* MMSYS Register*/
#define MMSYS_CG_CON0			(mmsys_config_base + 0x100)

/* clk cfg update */
#define CLK_CFG_0 0x40
#define CLK_CFG_0_SET 0x44
#define CLK_CFG_0_CLR 0x48
#define CLK_CFG_1 0x50
#define CLK_CFG_1_SET 0x54
#define CLK_CFG_1_CLR 0x58
#define CLK_CFG_2 0x60
#define CLK_CFG_2_SET 0x64
#define CLK_CFG_2_CLR 0x68
#define CLK_CFG_3 0x70
#define CLK_CFG_3_SET 0x74
#define CLK_CFG_3_CLR 0x78
#define CLK_CFG_4 0x80
#define CLK_CFG_4_SET 0x84
#define CLK_CFG_4_CLR 0x88
#define CLK_CFG_5 0x90
#define CLK_CFG_5_SET 0x94
#define CLK_CFG_5_CLR 0x98
#define CLK_CFG_6 0xa0
#define CLK_CFG_6_SET 0xa4
#define CLK_CFG_6_CLR 0xa8
#define CLK_CFG_7 0xb0
#define CLK_CFG_7_SET 0xb4
#define CLK_CFG_7_CLR 0xb8
#define CLK_CFG_8 0xc0
#define CLK_CFG_8_SET 0xc4
#define CLK_CFG_8_CLR 0xc8
#define CLK_CFG_9 0xd0
#define CLK_CFG_9_SET 0xd4
#define CLK_CFG_9_CLR 0xd8
#define CLK_CFG_10 0xe0
#define CLK_CFG_10_SET 0xe4
#define CLK_CFG_10_CLR 0xe8
#define CLK_CFG_UPDATE 0x004

enum {
	CLK_UNIVPLL	= 0,
	CLK_CCIPLL	= 1,
	CLK_ARMPLL_L	= 2,
	CLK_MPLL	= 3,
	CLK_MAINPLL	= 4,
	CLK_ARMPLL	= 5,
	CLK_NR_PLL_CON0
};

#define ARMPLL_HW_CTRL		((0x1 << CLK_ARMPLL)	\
			| (0x1 << (CLK_ARMPLL + (CLK_NR_PLL_CON0)))	\
			| (0x1 << (CLK_ARMPLL + 2 * (CLK_NR_PLL_CON0)))	\
			| (0x1 << (CLK_ARMPLL + 3 * (CLK_NR_PLL_CON0)))	\
			| (0x1 << (CLK_ARMPLL + 4 * (CLK_NR_PLL_CON0))))

#define ARMPLL_L_HW_CTRL		((0x1 << CLK_ARMPLL_L)	\
			| (0x1 << (CLK_ARMPLL_L + (CLK_NR_PLL_CON0)))	\
			| (0x1 << (CLK_ARMPLL_L + 2 * (CLK_NR_PLL_CON0)))\
			| (0x1 << (CLK_ARMPLL_L + 3 * (CLK_NR_PLL_CON0)))\
			| (0x1 << (CLK_ARMPLL_L + 4 * (CLK_NR_PLL_CON0))))

static const struct mtk_fixed_clk fixed_clks[] __initconst = {
	FIXED_CLK(CLK_TOP_F_FRTC, "f_frtc_ck", "clk32k", 32768),
	FIXED_CLK(CLK_TOP_CLK26M, "clk_26m_ck", "clk26m", 26000000),
	FIXED_CLK(CLK_TOP_DMPLL, "dmpll_ck", NULL, 466000000),
};

static const struct mtk_fixed_factor top_divs[] __initconst = {
	FACTOR(CLK_TOP_SYSPLL, "syspll_ck", "mainpll", 1, 1),
	FACTOR(CLK_TOP_SYSPLL_D2, "syspll_d2", "mainpll", 1, 2),
	FACTOR(CLK_TOP_SYSPLL1_D2, "syspll1_d2", "syspll_d2", 1, 2),
	FACTOR(CLK_TOP_SYSPLL1_D4, "syspll1_d4", "syspll_d2", 1, 4),
	FACTOR(CLK_TOP_SYSPLL1_D8, "syspll1_d8", "syspll_d2", 1, 8),
	FACTOR(CLK_TOP_SYSPLL1_D16, "syspll1_d16", "syspll_d2", 1, 16),
	FACTOR(CLK_TOP_SYSPLL_D3, "syspll_d3", "mainpll", 1, 3),
	FACTOR(CLK_TOP_SYSPLL2_D2, "syspll2_d2", "syspll_d3", 1, 2),
	FACTOR(CLK_TOP_SYSPLL2_D4, "syspll2_d4", "syspll_d3", 1, 4),
	FACTOR(CLK_TOP_SYSPLL2_D8, "syspll2_d8", "syspll_d3", 1, 8),
	FACTOR(CLK_TOP_SYSPLL_D5, "syspll_d5", "mainpll", 1, 5),
	FACTOR(CLK_TOP_SYSPLL3_D2, "syspll3_d2", "syspll_d5", 1, 2),
	FACTOR(CLK_TOP_SYSPLL3_D4, "syspll3_d4", "syspll_d5", 1, 4),
	FACTOR(CLK_TOP_SYSPLL_D7, "syspll_d7", "mainpll", 1, 7),
	FACTOR(CLK_TOP_SYSPLL4_D2, "syspll4_d2", "syspll_d7", 1, 2),
	FACTOR(CLK_TOP_SYSPLL4_D4, "syspll4_d4", "syspll_d7", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL, "univpll", "univ2pll", 1, 2),
	FACTOR(CLK_TOP_USB20_192M, "usb20_192m_ck", "univpll", 2, 13),
	FACTOR(CLK_TOP_USB20_192M_D4, "usb20_192m_d4", "usb20_192m_ck", 1, 4),
	FACTOR(CLK_TOP_USB20_192M_D8, "usb20_192m_d8", "usb20_192m_ck", 1, 8),
	FACTOR(CLK_TOP_USB20_192M_D16,
		"usb20_192m_d16", "usb20_192m_ck", 1, 16),
	FACTOR(CLK_TOP_USB20_192M_D32,
		"usb20_192m_d32", "usb20_192m_ck", 1, 32),
	FACTOR(CLK_TOP_UNIVPLL_D2, "univpll_d2", "univpll", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL1_D2, "univpll1_d2", "univpll_d2", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL1_D4, "univpll1_d4", "univpll_d2", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL_D3, "univpll_d3", "univpll", 1, 3),
	FACTOR(CLK_TOP_UNIVPLL2_D2, "univpll2_d2", "univpll_d3", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL2_D4, "univpll2_d4", "univpll_d3", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL2_D8, "univpll2_d8", "univpll_d3", 1, 8),
	FACTOR(CLK_TOP_UNIVPLL2_D32, "univpll2_d32", "univpll_d3", 1, 32),
	FACTOR(CLK_TOP_UNIVPLL_D5, "univpll_d5", "univpll", 1, 5),
	FACTOR(CLK_TOP_UNIVPLL3_D2, "univpll3_d2", "univpll_d5", 1, 2),
	FACTOR(CLK_TOP_UNIVPLL3_D4, "univpll3_d4", "univpll_d5", 1, 4),
	FACTOR(CLK_TOP_MMPLL, "mmpll_ck", "mmpll", 1, 1),
	FACTOR(CLK_TOP_MMPLL_D2, "mmpll_d2", "mmpll_ck", 1, 2),
	FACTOR(CLK_TOP_MPLL, "mpll_ck", "mpll", 1, 1),
	FACTOR(CLK_TOP_DA_MPLL_104M_DIV, "mpll_104m_div", "mpll_ck", 1, 2),
	FACTOR(CLK_TOP_DA_MPLL_52M_DIV, "mpll_52m_div", "mpll_ck", 1, 4),
	FACTOR(CLK_TOP_MFGPLL, "mfgpll_ck", "mfgpll", 1, 1),
	FACTOR(CLK_TOP_MSDCPLL, "msdcpll_ck", "msdcpll", 1, 1),
	FACTOR(CLK_TOP_MSDCPLL_D2, "msdcpll_d2", "msdcpll_ck", 1, 2),
	FACTOR(CLK_TOP_APLL1, "apll1_ck", "apll1", 1, 1),
	FACTOR(CLK_TOP_APLL1_D2, "apll1_d2", "apll1_ck", 1, 2),
	FACTOR(CLK_TOP_APLL1_D4, "apll1_d4", "apll1_ck", 1, 4),
	FACTOR(CLK_TOP_APLL1_D8, "apll1_d8", "apll1_ck", 1, 8),
	FACTOR(CLK_TOP_ULPOSC1, "ulposc1_ck", "ulposc1", 1, 1),
	FACTOR(CLK_TOP_ULPOSC1_D2, "ulposc1_d2", "ulposc1_ck", 1, 2),
	FACTOR(CLK_TOP_ULPOSC1_D4, "ulposc1_d4", "ulposc1_ck", 1, 4),
	FACTOR(CLK_TOP_ULPOSC1_D8, "ulposc1_d8", "ulposc1_ck", 1, 8),
	FACTOR(CLK_TOP_ULPOSC1_D16, "ulposc1_d16", "ulposc1_ck", 1, 16),
	FACTOR(CLK_TOP_ULPOSC1_D32, "ulposc1_d32", "ulposc1_ck", 1, 32),
	/* dummy clk define, do not control
	 *this clock due to it's a ddrphy clock source
	 */
	/* FACTOR(CLK_TOP_DMPLL, "dmpll_ck", "ulposc1_ck", 1, 32), */
	FACTOR(CLK_TOP_F_F26M, "f_f26m_ck", "clk_26m_ck", 1, 1),
	FACTOR(CLK_TOP_AXI, "axi_ck", "axi_sel", 1, 1),
	FACTOR(CLK_TOP_MM, "mm_ck", "mm_sel", 1, 1),
	FACTOR(CLK_TOP_SCP, "scp_ck", "scp_sel", 1, 1),
	FACTOR(CLK_TOP_MFG, "mfg_ck", "mfg_sel", 1, 1),
	FACTOR(CLK_TOP_F_FUART, "f_fuart_ck", "uart_sel", 1, 1),
	FACTOR(CLK_TOP_SPI, "spi_ck", "spi_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC50_0, "msdc50_0_ck", "msdc50_0_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC30_1, "msdc30_1_ck", "msdc30_1_sel", 1, 1),
	FACTOR(CLK_TOP_AUDIO, "audio_ck", "audio_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_1, "aud_1_ck", "aud_1_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_ENGEN1, "aud_engen1_ck", "aud_engen1_sel", 1, 1),
	FACTOR(CLK_TOP_F_FDISP_PWM, "f_fdisp_pwm_ck", "disp_pwm_sel", 1, 1),
	FACTOR(CLK_TOP_SSPM, "sspm_ck", "sspm_sel", 1, 1),
	FACTOR(CLK_TOP_DXCC, "dxcc_ck", "dxcc_sel", 1, 1),
	FACTOR(CLK_TOP_I2C, "i2c_ck", "i2c_sel", 1, 1),
	FACTOR(CLK_TOP_F_FPWM, "f_fpwm_ck", "pwm_sel", 1, 1),
	FACTOR(CLK_TOP_F_FSENINF, "f_fseninf_ck", "seninf_sel", 1, 1),
	FACTOR(CLK_TOP_AES_FDE, "aes_fde_ck", "aes_fde_sel", 1, 1),
	FACTOR(CLK_TOP_F_BIST2FPC, "f_bist2fpc_ck", "univpll2_d2", 1, 1),
	FACTOR(CLK_TOP_ARMPLL_DIVIDER_PLL0, "arm_div_pll0", "syspll_d2", 1, 1),
	FACTOR(CLK_TOP_ARMPLL_DIVIDER_PLL1, "arm_div_pll1", "syspll_ck", 1, 1),
	FACTOR(CLK_TOP_ARMPLL_DIVIDER_PLL2, "arm_div_pll2", "univpll_d2", 1, 1),
	FACTOR(CLK_TOP_DA_USB20_48M_DIV,
		"usb20_48m_div", "usb20_192m_d4", 1, 1),
	FACTOR(CLK_TOP_DA_UNIV_48M_DIV, "univ_48m_div", "usb20_192m_d4", 1, 1),
};

static const char * const axi_parents[] __initconst = {
	"clk26m",
	"syspll_d7",
	"syspll1_d4",
	"syspll3_d2"
};

static const char * const mem_parents[] __initconst = {
	"clk26m",
	"dmpll_ck",
	"apll1_ck"
};

static const char * const mm_parents[] __initconst = {
	"clk26m",
	"mmpll_ck",
	"syspll1_d2",
	"syspll_d5",
	"syspll1_d4",
	"univpll_d5",
	"univpll1_d2",
	"mmpll_d2"
};

static const char * const scp_parents[] __initconst = {
	"clk26m",
	"syspll4_d2",
	"univpll2_d2",
	"syspll1_d2",
	"univpll1_d2",
	"syspll_d3",
	"univpll_d3"
};

static const char * const mfg_parents[] __initconst = {
	"clk26m",
	"mfgpll_ck",
	"syspll_d3",
	"univpll_d3"
};

static const char * const atb_parents[] __initconst = {
	"clk26m",
	"syspll1_d4",
	"syspll1_d2"
};

static const char * const camtg_parents[] __initconst = {
	"clk26m",
	"usb20_192m_d8",
	"univpll2_d8",
	"usb20_192m_d4",
	"univpll2_d32",
	"usb20_192m_d16",
	"usb20_192m_d32"
};

static const char * const uart_parents[] __initconst = {
	"clk26m",
	"univpll2_d8"
};

static const char * const spi_parents[] __initconst = {
	"clk26m",
	"syspll3_d2",
	"syspll4_d2",
	"syspll2_d4"
};

static const char * const msdc5hclk_parents[] __initconst = {
	"clk26m",
	"syspll1_d2",
	"univpll1_d4",
	"syspll2_d2"
};

static const char * const msdc50_0_parents[] __initconst = {
	"clk26m",
	"msdcpll_ck",
	"syspll2_d2",
	"syspll4_d2",
	"univpll1_d2",
	"syspll1_d2",
	"univpll_d5",
	"univpll1_d4"
};

static const char * const msdc30_1_parents[] __initconst = {
	"clk26m",
	"msdcpll_d2",
	"univpll2_d2",
	"syspll2_d2",
	"syspll1_d4",
	"univpll1_d4",
	"usb20_192m_d4",
	"syspll2_d4"
};

static const char * const audio_parents[] __initconst = {
	"clk26m",
	"syspll3_d4",
	"syspll4_d4",
	"syspll1_d16"
};

static const char * const aud_intbus_parents[] __initconst = {
	"clk26m",
	"syspll1_d4",
	"syspll4_d2"
};

static const char * const aud_1_parents[] __initconst = {
	"clk26m",
	"apll1_ck"
};

static const char * const aud_engen1_parents[] __initconst = {
	"clk26m",
	"apll1_d2",
	"apll1_d4",
	"apll1_d8"
};

static const char * const disp_pwm_parents[] __initconst = {
	"clk26m",
	"univpll2_d4",
	"ulposc1_d2",
	"ulposc1_d8"
};

static const char * const sspm_parents[] __initconst = {
	"clk26m",
	"syspll1_d2",
	"syspll_d3"
};

static const char * const dxcc_parents[] __initconst = {
	"clk26m",
	"syspll1_d2",
	"syspll1_d4",
	"syspll1_d8"
};

static const char * const usb_top_parents[] __initconst = {
	"clk26m",
	"univpll3_d4"
};

static const char * const spm_parents[] __initconst = {
	"clk26m",
	"syspll1_d8"
};

static const char * const i2c_parents[] __initconst = {
	"clk26m",
	"univpll3_d4",
	"univpll3_d2",
	"syspll1_d8",
	"syspll2_d8"
};

static const char * const pwm_parents[] __initconst = {
	"clk26m",
	"univpll3_d4",
	"syspll1_d8"
};

static const char * const seninf_parents[] __initconst = {
	"clk26m",
	"univpll1_d4",
	"univpll1_d2",
	"univpll2_d2"
};

static const char * const aes_fde_parents[] __initconst = {
	"clk26m",
	"msdcpll_ck",
	"univpll_d3",
	"univpll2_d2",
	"univpll1_d2",
	"syspll1_d2"
};

static const char * const ulposc_parents[] __initconst = {
	"clk26m",
	"ulposc1_d4",
	"ulposc1_d8",
	"ulposc1_d16",
	"ulposc1_d32"
};

static const char * const camtm_parents[] __initconst = {
	"clk26m",
	"univpll1_d4",
	"univpll1_d2",
	"univpll2_d2"
};

#define INVALID_UPDATE_REG 0xFFFFFFFF
#define INVALID_UPDATE_SHIFT -1
#define INVALID_MUX_GATE -1

static const struct mtk_mux top_muxes[] __initconst = {
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD_FLAGS(CLK_TOP_AXI_SEL, "axi_sel", axi_parents,
		CLK_CFG_0, CLK_CFG_0_SET, CLK_CFG_0_CLR, 0, 2, 7,
		CLK_CFG_UPDATE, 0, CLK_IS_CRITICAL),
	MUX_CLR_SET_UPD_FLAGS(CLK_TOP_MEM_SEL, "mem_sel", mem_parents,
		CLK_CFG_0, CLK_CFG_0_SET, CLK_CFG_0_CLR, 8, 2, 15,
		CLK_CFG_UPDATE, 1, CLK_IS_CRITICAL),
	MUX_CLR_SET_UPD(CLK_TOP_MM_SEL, "mm_sel", mm_parents, CLK_CFG_0,
		CLK_CFG_0_SET, CLK_CFG_0_CLR, 16, 3, 23, CLK_CFG_UPDATE, 2),
	MUX_CLR_SET_UPD(CLK_TOP_SCP_SEL, "scp_sel", scp_parents, CLK_CFG_0,
		CLK_CFG_0_SET, CLK_CFG_0_CLR, 24, 3, 31, CLK_CFG_UPDATE, 3),
	/* CLK_CFG_1 */
	MUX_CLR_SET_UPD(CLK_TOP_MFG_SEL, "mfg_sel", mfg_parents, CLK_CFG_1,
		CLK_CFG_1_SET, CLK_CFG_1_CLR, 0, 2, 7, CLK_CFG_UPDATE, 4),
	MUX_CLR_SET_UPD(CLK_TOP_ATB_SEL, "atb_sel", atb_parents, CLK_CFG_1,
		CLK_CFG_1_SET, CLK_CFG_1_CLR, 8, 2, 15, CLK_CFG_UPDATE, 5),
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG_SEL, "camtg_sel",
		camtg_parents, CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR,
		16, 3, 23, CLK_CFG_UPDATE, 6),
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG1_SEL, "camtg1_sel",
		camtg_parents, CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR,
		24, 3, 31, CLK_CFG_UPDATE, 7),
	/* CLK_CFG_2 */
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG2_SEL, "camtg2_sel",
		camtg_parents, CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR,
		0, 3, 7, CLK_CFG_UPDATE, 8),
	MUX_CLR_SET_UPD(CLK_TOP_CAMTG3_SEL, "camtg3_sel",
		camtg_parents, CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR,
		8, 3, 15, CLK_CFG_UPDATE, 9),
	MUX_CLR_SET_UPD(CLK_TOP_UART_SEL, "uart_sel", uart_parents, CLK_CFG_2,
		CLK_CFG_2_SET, CLK_CFG_2_CLR, 16, 1, 23, CLK_CFG_UPDATE, 10),
	MUX_CLR_SET_UPD(CLK_TOP_SPI_SEL, "spi_sel", spi_parents, CLK_CFG_2,
		CLK_CFG_2_SET, CLK_CFG_2_CLR, 24, 2, 31, CLK_CFG_UPDATE, 11),
	/* CLK_CFG_3 */
	MUX_CLR_SET_UPD(CLK_TOP_MSDC50_0_HCLK_SEL, "msdc5hclk",
		msdc5hclk_parents, CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR, 0,
		2, 7, CLK_CFG_UPDATE, 12),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC50_0_SEL, "msdc50_0_sel",
		msdc50_0_parents, CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR,
		8, 3, 15, CLK_CFG_UPDATE, 13),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC30_1_SEL, "msdc30_1_sel",
		msdc30_1_parents, CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR,
		16, 3, 23, CLK_CFG_UPDATE, 14),
	MUX_CLR_SET_UPD(CLK_TOP_AUDIO_SEL, "audio_sel",
		audio_parents, CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR,
		24, 2, 31, CLK_CFG_UPDATE, 15),
	/* CLK_CFG_4 */
	MUX_CLR_SET_UPD(CLK_TOP_AUD_INTBUS_SEL, "aud_intbus_sel",
		aud_intbus_parents, CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR,
		0, 2, 7, CLK_CFG_UPDATE, 16),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_1_SEL, "aud_1_sel",
		aud_1_parents, CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR,
		8, 1, 15, CLK_CFG_UPDATE, 17),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_ENGEN1_SEL, "aud_engen1_sel",
		aud_engen1_parents, CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR,
		16, 2, 23, CLK_CFG_UPDATE, 18),
	MUX_CLR_SET_UPD(CLK_TOP_DISP_PWM_SEL, "disp_pwm_sel",
		disp_pwm_parents, CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR,
		24, 2, 31, CLK_CFG_UPDATE, 19),
	/* CLK_CFG_5 */
	MUX_CLR_SET_UPD_FLAGS(CLK_TOP_SSPM_SEL, "sspm_sel", sspm_parents,
		CLK_CFG_5, CLK_CFG_5_SET, CLK_CFG_5_CLR, 0, 2, 7,
		CLK_CFG_UPDATE, 20, CLK_IS_CRITICAL),
	MUX_CLR_SET_UPD_FLAGS(CLK_TOP_DXCC_SEL, "dxcc_sel", dxcc_parents,
		CLK_CFG_5, CLK_CFG_5_SET, CLK_CFG_5_CLR, 8, 2, 15,
		CLK_CFG_UPDATE, 21, CLK_IS_CRITICAL),
	MUX_CLR_SET_UPD(CLK_TOP_USB_TOP_SEL, "usb_top_sel",
		usb_top_parents, CLK_CFG_5, CLK_CFG_5_SET, CLK_CFG_5_CLR,
		16, 1, 23, CLK_CFG_UPDATE, 22),
	MUX_CLR_SET_UPD_FLAGS(CLK_TOP_SPM_SEL, "spm_sel", spm_parents,
		CLK_CFG_5, CLK_CFG_5_SET, CLK_CFG_5_CLR, 24, 1, 31,
		CLK_CFG_UPDATE, 23, CLK_IS_CRITICAL),
	/* CLK_CFG_6 */
	MUX_CLR_SET_UPD(CLK_TOP_I2C_SEL, "i2c_sel", i2c_parents, CLK_CFG_6,
		CLK_CFG_6_SET, CLK_CFG_6_CLR, 0, 3, 7, CLK_CFG_UPDATE, 24),
	MUX_CLR_SET_UPD(CLK_TOP_PWM_SEL, "pwm_sel", pwm_parents, CLK_CFG_6,
		CLK_CFG_6_SET, CLK_CFG_6_CLR, 8, 2, 15, CLK_CFG_UPDATE, 25),
	MUX_CLR_SET_UPD(CLK_TOP_SENINF_SEL, "seninf_sel",
		seninf_parents, CLK_CFG_6, CLK_CFG_6_SET, CLK_CFG_6_CLR,
		16, 2, 23, CLK_CFG_UPDATE, 26),
	MUX_CLR_SET_UPD(CLK_TOP_AES_FDE_SEL, "aes_fde_sel",
		aes_fde_parents, CLK_CFG_6, CLK_CFG_6_SET, CLK_CFG_6_CLR,
		24, 3, 31, CLK_CFG_UPDATE, 27),
	/* CLK_CFG_7 */
	MUX_CLR_SET_UPD_FLAGS(CLK_TOP_PWRAP_ULPOSC_SEL, "ulposc_sel",
		ulposc_parents, CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR,
		0, 3, 7, CLK_CFG_UPDATE, 28, CLK_IS_CRITICAL),
	MUX_CLR_SET_UPD(CLK_TOP_CAMTM_SEL, "camtm_sel",
		camtm_parents, CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR,
		8, 2, 15, CLK_CFG_UPDATE, 29),
};

static const struct mtk_gate_regs top0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs top1_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x104,
	.sta_ofs = 0x104,
};

static const struct mtk_gate_regs top2_cg_regs = {
	.set_ofs = 0x320,
	.clr_ofs = 0x320,
	.sta_ofs = 0x320,
};

#define GATE_TOP0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_TOP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_TOP2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &top2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate top_clks[] __initconst = {
	/* TOP0 */
#if 0
	GATE_TOP0(CLK_TOP_MD_32K, "md_32k", "f_frtc_ck", 8),
	GATE_TOP0(CLK_TOP_MD_26M, "md_26m", "f_f26m_ck", 9),
	GATE_TOP0(CLK_TOP_MD2_32K, "md2_32k", "f_frtc_ck", 10),
	GATE_TOP0(CLK_TOP_MD2_26M, "md2_26m", "f_f26m_ck", 11),
#endif
	/* TOP1 */
	GATE_TOP1(CLK_TOP_ARMPLL_DIVIDER_PLL0_EN,
		"arm_div_pll0_en", "arm_div_pll0", 3),
	GATE_TOP1(CLK_TOP_ARMPLL_DIVIDER_PLL1_EN,
		"arm_div_pll1_en", "arm_div_pll1", 4),
	GATE_TOP1(CLK_TOP_ARMPLL_DIVIDER_PLL2_EN,
		"arm_div_pll2_en", "arm_div_pll2", 5),
	GATE_TOP1(CLK_TOP_FMEM_OCC_DRC_EN, "drc_en", "univpll2_d2", 6),
#if 0
	GATE_TOP1(CLK_TOP_USB20_48M_EN, "usb20_48m_en", "usb20_48m_div", 8),
	GATE_TOP1(CLK_TOP_UNIVPLL_48M_EN, "univpll_48m_en", "univ_48m_div", 9),
	GATE_TOP1(CLK_TOP_MPLL_104M_EN, "mpll_104m_en", "mpll_104m_div", 10),
	GATE_TOP1(CLK_TOP_MPLL_52M_EN, "mpll_52m_en", "mpll_52m_div", 11),
	GATE_TOP1(CLK_TOP_F_UFS_MP_SAP_CFG_EN, "ufs_sap", "f_f26m_ck", 12),
#endif
	GATE_TOP1(CLK_TOP_F_BIST2FPC_EN, "bist2fpc", "f_bist2fpc_ck", 16),
	/* TOP2 */
	GATE_TOP2(CLK_TOP_APLL12_DIV0, "apll12_div0", "aud_1_ck", 2),
	GATE_TOP2(CLK_TOP_APLL12_DIV1, "apll12_div1", "aud_1_ck", 3),
	GATE_TOP2(CLK_TOP_APLL12_DIV2, "apll12_div2", "aud_1_ck", 4),
	GATE_TOP2(CLK_TOP_APLL12_DIV3, "apll12_div3", "aud_1_ck", 5),
};

static const struct mtk_gate_regs ifr0_cg_regs = {
	.set_ofs = 0x200,
	.clr_ofs = 0x200,
	.sta_ofs = 0x200,
};

static const struct mtk_gate_regs ifr1_cg_regs = {
	.set_ofs = 0x74,
	.clr_ofs = 0x74,
	.sta_ofs = 0x74,
};

static const struct mtk_gate_regs ifr2_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x84,
	.sta_ofs = 0x90,
};

static const struct mtk_gate_regs ifr3_cg_regs = {
	.set_ofs = 0x88,
	.clr_ofs = 0x8c,
	.sta_ofs = 0x94,
};

static const struct mtk_gate_regs ifr4_cg_regs = {
	.set_ofs = 0xa4,
	.clr_ofs = 0xa8,
	.sta_ofs = 0xac,
};

static const struct mtk_gate_regs ifr5_cg_regs = {
	.set_ofs = 0xc0,
	.clr_ofs = 0xc4,
	.sta_ofs = 0xc8,
};

#define GATE_IFR0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifr0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_IFR1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifr1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_IFR2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifr2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFR3(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifr3_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFR4(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifr4_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFR5(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifr5_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate ifr_clks[] __initconst = {
	/* INFRA_TOPAXI */
	/* INFRA PERI */
	/* INFRA mode 0 */
	GATE_IFR2(CLK_IFR_ICUSB, "ifr_icusb", "axi_ck", 8),
	GATE_IFR2(CLK_IFR_GCE, "ifr_gce", "axi_ck", 9),
	GATE_IFR2(CLK_IFR_THERM, "ifr_therm", "axi_ck", 10),
	GATE_IFR2(CLK_IFR_I2C_AP, "ifr_i2c_ap", "i2c_ck", 11),
	GATE_IFR2(CLK_IFR_I2C_CCU, "ifr_i2c_ccu", "i2c_ck", 12),
	GATE_IFR2(CLK_IFR_I2C_SSPM, "ifr_i2c_sspm", "i2c_ck", 13),
	GATE_IFR2(CLK_IFR_I2C_RSV, "ifr_i2c_rsv", "i2c_ck", 14),
	GATE_IFR2(CLK_IFR_PWM_HCLK, "ifr_pwm_hclk", "axi_ck", 15),
	GATE_IFR2(CLK_IFR_PWM1, "ifr_pwm1", "f_fpwm_ck", 16),
	GATE_IFR2(CLK_IFR_PWM2, "ifr_pwm2", "f_fpwm_ck", 17),
	GATE_IFR2(CLK_IFR_PWM3, "ifr_pwm3", "f_fpwm_ck", 18),
	GATE_IFR2(CLK_IFR_PWM4, "ifr_pwm4", "f_fpwm_ck", 19),
	GATE_IFR2(CLK_IFR_PWM5, "ifr_pwm5", "f_fpwm_ck", 20),
	GATE_IFR2(CLK_IFR_PWM, "ifr_pwm", "f_fpwm_ck", 21),
	GATE_IFR2(CLK_IFR_UART0, "ifr_uart0", "f_fuart_ck", 22),
	GATE_IFR2(CLK_IFR_UART1, "ifr_uart1", "f_fuart_ck", 23),
	GATE_IFR2(CLK_IFR_GCE_26M, "ifr_gce_26m", "f_f26m_ck", 27),
	GATE_IFR2(CLK_IFR_CQ_DMA_FPC, "ifr_dma", "axi_ck", 28),
	GATE_IFR2(CLK_IFR_BTIF, "ifr_btif", "axi_ck", 31),
	/* INFRA mode 1 */
	GATE_IFR3(CLK_IFR_SPI0, "ifr_spi0", "spi_ck", 1),
	GATE_IFR3(CLK_IFR_MSDC0, "ifr_msdc0", "msdc5hclk", 2),
	GATE_IFR3(CLK_IFR_MSDC1, "ifr_msdc1", "axi_ck", 4),
	GATE_IFR3(CLK_IFR_TRNG, "ifr_trng", "axi_ck", 9),
	GATE_IFR3(CLK_IFR_AUXADC, "ifr_auxadc", "f_f26m_ck", 10),
	GATE_IFR3(CLK_IFR_CCIF1_AP, "ifr_ccif1_ap", "axi_ck", 12),
	GATE_IFR3(CLK_IFR_CCIF1_MD, "ifr_ccif1_md", "axi_ck", 13),
	GATE_IFR3(CLK_IFR_AUXADC_MD, "ifr_auxadc_md", "f_f26m_ck", 14),
	GATE_IFR3(CLK_IFR_AP_DMA, "ifr_ap_dma", "axi_ck", 18),
	GATE_IFR3(CLK_IFR_DEVICE_APC, "ifr_dapc", "axi_ck", 20),
	GATE_IFR3(CLK_IFR_CCIF_AP, "ifr_ccif_ap", "axi_ck", 23),
	GATE_IFR3(CLK_IFR_AUDIO, "ifr_audio", "axi_ck", 25),
	GATE_IFR3(CLK_IFR_CCIF_MD, "ifr_ccif_md", "axi_ck", 26),
	/* INFRA mode 2 */
	GATE_IFR4(CLK_IFR_RG_PWM_FBCLK6, "ifr_pwmfb", "f_f26m_ck", 0),
	GATE_IFR4(CLK_IFR_DISP_PWM, "ifr_disp_pwm", "f_fdisp_pwm_ck", 2),
	GATE_IFR4(CLK_IFR_CLDMA_BCLK, "ifr_cldmabclk", "axi_ck", 3),
	GATE_IFR4(CLK_IFR_AUDIO_26M_BCLK, "ifr_audio26m", "f_f26m_ck", 4),
	GATE_IFR4(CLK_IFR_SPI1, "ifr_spi1", "spi_ck", 6),
	GATE_IFR4(CLK_IFR_I2C4, "ifr_i2c4", "i2c_ck", 7),
	GATE_IFR4(CLK_IFR_SPI2, "ifr_spi2", "spi_ck", 9),
	GATE_IFR4(CLK_IFR_SPI3, "ifr_spi3", "spi_ck", 10),
	GATE_IFR4(CLK_IFR_I2C5, "ifr_i2c5", "i2c_ck", 18),
	GATE_IFR4(CLK_IFR_I2C5_ARBITER, "ifr_i2c5a", "i2c_ck", 19),
	GATE_IFR4(CLK_IFR_I2C5_IMM, "ifr_i2c5_imm", "i2c_ck", 20),
	GATE_IFR4(CLK_IFR_I2C1_ARBITER, "ifr_i2c1a", "i2c_ck", 21),
	GATE_IFR4(CLK_IFR_I2C1_IMM, "ifr_i2c1_imm", "i2c_ck", 22),
	GATE_IFR4(CLK_IFR_I2C2_ARBITER, "ifr_i2c2a", "i2c_ck", 23),
	GATE_IFR4(CLK_IFR_I2C2_IMM, "ifr_i2c2_imm", "i2c_ck", 24),
	GATE_IFR4(CLK_IFR_SPI4, "ifr_spi4", "spi_ck", 25),
	GATE_IFR4(CLK_IFR_SPI5, "ifr_spi5", "spi_ck", 26),
	GATE_IFR4(CLK_IFR_CQ_DMA, "ifr_cq_dma", "axi_ck", 27),
	GATE_IFR4(CLK_IFR_FAES_FDE, "ifr_faes_fde_ck", "aes_fde_ck", 29),
	/* INFRA mode 3 */
	GATE_IFR5(CLK_IFR_MSDC0_SELF, "ifr_msdc0sf", "msdc50_0_ck", 0),
	GATE_IFR5(CLK_IFR_MSDC1_SELF, "ifr_msdc1sf", "msdc50_0_ck", 1),
	GATE_IFR5(CLK_IFR_I2C6, "ifr_i2c6", "i2c_ck", 6),
	GATE_IFR5(CLK_IFR_AP_MSDC0, "ifr_ap_msdc0", "msdc50_0_ck", 7),
	GATE_IFR5(CLK_IFR_MD_MSDC0, "ifr_md_msdc0", "msdc50_0_ck", 8),
	GATE_IFR5(CLK_IFR_MSDC0_SRC, "ifr_msdc0_clk", "msdc50_0_ck", 9),
	GATE_IFR5(CLK_IFR_MSDC1_SRC, "ifr_msdc1_clk", "msdc30_1_ck", 10),
	/* GATE_IFR5(CLK_IFR_AES_TOP0_BCLK, "ifr_aes", "axi_ck", 16), */
	GATE_IFR5(CLK_IFR_MCU_PM_BCLK, "ifr_mcu_pm_bclk", "axi_ck", 17),
	GATE_IFR5(CLK_IFR_CCIF2_AP, "ifr_ccif2_ap", "axi_ck", 18),
	GATE_IFR5(CLK_IFR_CCIF2_MD, "ifr_ccif2_md", "axi_ck", 19),
	GATE_IFR5(CLK_IFR_CCIF3_AP, "ifr_ccif3_ap", "axi_ck", 20),
	GATE_IFR5(CLK_IFR_CCIF3_MD, "ifr_ccif3_md", "axi_ck", 21),
};

static const struct mtk_gate_regs venc_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_VENC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &venc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

static const struct mtk_gate venc_clks[] __initconst = {
	GATE_VENC(CLK_VENC_SET0_LARB, "venc_set0_larb", "mm_ck", 0),
	GATE_VENC(CLK_VENC_SET1_VENC, "venc_set1_venc", "mm_ck", 4),
	GATE_VENC(CLK_VENC_SET2_JPGENC, "jpgenc", "mm_ck", 8),
	GATE_VENC(CLK_VENC_SET3_VDEC, "venc_set3_vdec", "mm_ck", 12),
};

static const struct mtk_gate_regs cam_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM(_id, _name, _parent, _shift) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate cam_clks[] __initconst = {
	GATE_CAM(CLK_CAM_LARB3, "cam_larb3", "mm_ck", 0),/*use dummy*/
	GATE_CAM(CLK_CAM_DFP_VAD, "cam_dfp_vad", "mm_ck", 1),
	GATE_CAM(CLK_CAM, "cam", "mm_ck", 6),
	GATE_CAM(CLK_CAMTG, "camtg", "mm_ck", 7),
	GATE_CAM(CLK_CAM_SENINF, "cam_seninf", "mm_ck", 8),
	GATE_CAM(CLK_CAMSV0, "camsv0", "mm_ck", 9),
	GATE_CAM(CLK_CAMSV1, "camsv1", "mm_ck", 10),
	GATE_CAM(CLK_CAMSV2, "camsv2", "mm_ck", 11),
	GATE_CAM(CLK_CAM_CCU, "cam_ccu", "mm_ck", 12),
};

static const struct mtk_gate_regs img_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IMG(_id, _name, _parent, _shift) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &img_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate img_clks[] __initconst = {
	GATE_IMG(CLK_IMG_LARB2, "img_larb2", "mm_ck", 0),/*use dummy*/
	GATE_IMG(CLK_IMG_DIP, "img_dip", "mm_ck", 2),
	GATE_IMG(CLK_IMG_FDVT, "img_fdvt", "mm_ck", 3),
	GATE_IMG(CLK_IMG_DPE, "img_dpe", "mm_ck", 4),
	GATE_IMG(CLK_IMG_RSC, "img_rsc", "mm_ck", 5),
};

static const struct mtk_gate_regs audio0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs audio1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x4,
	.sta_ofs = 0x4,
};

#define GATE_AUDIO0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &audio0_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AUDIO1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &audio1_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate audio_clks[] __initconst = {
	/* AUDIO0 */
	GATE_AUDIO0(CLK_AUDIO_AFE, "aud_afe", "audio_ck",
		2),
	GATE_AUDIO0(CLK_AUDIO_22M, "aud_22m", "aud_engen1_ck",
		8),
	GATE_AUDIO0(CLK_AUDIO_APLL_TUNER, "aud_apll_tuner", "aud_engen1_ck",
		19),
	GATE_AUDIO0(CLK_AUDIO_ADC, "aud_adc", "audio_ck",
		24),
	GATE_AUDIO0(CLK_AUDIO_DAC, "aud_dac", "audio_ck",
		25),
	GATE_AUDIO0(CLK_AUDIO_DAC_PREDIS, "aud_dac_predis", "audio_ck",
		26),
	GATE_AUDIO0(CLK_AUDIO_TML, "aud_tml", "audio_ck",
		27),
	/* AUDIO1 */
	GATE_AUDIO1(CLK_AUDIO_I2S1_BCLK, "aud_i2s1_bclk", "audio_ck",
		4),
	GATE_AUDIO1(CLK_AUDIO_I2S2_BCLK, "aud_i2s2_bclk", "audio_ck",
		5),
	GATE_AUDIO1(CLK_AUDIO_I2S3_BCLK, "aud_i2s3_bclk", "audio_ck",
		6),
	GATE_AUDIO1(CLK_AUDIO_I2S4_BCLK, "aud_i2s4_bclk", "audio_ck",
		7),
};

static const struct mtk_gate_regs mm_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

#define GATE_MM(_id, _name, _parent, _shift) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate mm_clks[] __initconst = {
	/* MM */
	GATE_MM(CLK_MM_MDP_RDMA0, "mm_mdp_rdma0", "mm_ck", 0),
	GATE_MM(CLK_MM_MDP_CCORR0, "mm_mdp_ccorr0", "mm_ck", 1),
	GATE_MM(CLK_MM_MDP_RSZ0, "mm_mdp_rsz0", "mm_ck", 2),
	GATE_MM(CLK_MM_MDP_RSZ1, "mm_mdp_rsz1", "mm_ck", 3),
	GATE_MM(CLK_MM_MDP_TDSHP0, "mm_mdp_tdshp0", "mm_ck", 4),
	GATE_MM(CLK_MM_MDP_WROT0, "mm_mdp_wrot0", "mm_ck", 5),
	GATE_MM(CLK_MM_MDP_WDMA0, "mm_mdp_wdma0", "mm_ck", 6),
	GATE_MM(CLK_MM_DISP_OVL0, "mm_disp_ovl0", "mm_ck", 7),
	GATE_MM(CLK_MM_DISP_OVL0_2L, "mm_disp_ovl0_2l", "mm_ck", 8),
	GATE_MM(CLK_MM_DISP_RSZ0, "mm_disp_rsz0", "mm_ck", 9),
	GATE_MM(CLK_MM_DISP_RDMA0, "mm_disp_rdma0", "mm_ck", 10),
	GATE_MM(CLK_MM_DISP_WDMA0, "mm_disp_wdma0", "mm_ck", 11),
	GATE_MM(CLK_MM_DISP_COLOR0, "mm_disp_color0", "mm_ck", 12),
	GATE_MM(CLK_MM_DISP_CCORR0, "mm_disp_ccorr0", "mm_ck", 13),
	GATE_MM(CLK_MM_DISP_AAL0, "mm_disp_aal0", "mm_ck", 14),
	GATE_MM(CLK_MM_DISP_GAMMA0, "mm_disp_gamma0", "mm_ck", 15),
	GATE_MM(CLK_MM_DISP_DITHER0, "mm_disp_dither0", "mm_ck", 16),
	GATE_MM(CLK_MM_DSI0, "mm_dsi0", "mm_ck", 17),
	GATE_MM(CLK_MM_FAKE_ENG, "mm_fake_eng", "mm_ck", 18),
	GATE_MM(CLK_MM_SMI_COMMON, "mm_smi_common", "mm_ck", 19),
	GATE_MM(CLK_MM_SMI_LARB0, "mm_smi_larb0", "mm_ck", 20),
	GATE_MM(CLK_MM_SMI_COMM0, "mm_smi_comm0", "mm_ck", 21),
	GATE_MM(CLK_MM_SMI_COMM1, "mm_smi_comm1", "mm_ck", 22),
	GATE_MM(CLK_MM_CAM_MDP, "mm_cam_mdp_ck", "mm_ck", 23),
	GATE_MM(CLK_MM_SMI_IMG, "mm_smi_img_ck", "mm_ck", 24),
	GATE_MM(CLK_MM_SMI_CAM, "mm_smi_cam_ck", "mm_ck", 25),
	GATE_MM(CLK_MM_IMG_DL_RELAY, "mm_img_dl_relay", "mm_ck", 26),
	GATE_MM(CLK_MM_IMG_DL_ASYNC_TOP, "mm_imgdl_async", "mm_ck", 27),
	GATE_MM(CLK_MM_DIG_DSI, "mm_dig_dsi_ck", "mm_ck", 28),
	GATE_MM(CLK_MM_F26M_HRTWT, "mm_hrtwt", "f_f26m_ck", 29),
};


static const struct mtk_gate_regs mipi0a_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x80,
	.sta_ofs = 0x80,
};

#define GATE_MIPI0A(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mipi0a_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

static const struct mtk_gate mipi0a_clks[] __initconst = {
	GATE_MIPI0A(CLK_MIPI0A_CSR_CSI_EN_0A,
		"mipi0a_csr_0a", "f_fseninf_ck", 1),
};

static const struct mtk_gate_regs mipi0b_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x80,
	.sta_ofs = 0x80,
};

#define GATE_MIPI0B(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mipi0b_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

static const struct mtk_gate mipi0b_clks[] __initconst = {
	GATE_MIPI0B(CLK_MIPI0B_CSR_CSI_EN_0B,
		"mipi0b_csr_0b", "f_fseninf_ck", 1),
};

static const struct mtk_gate_regs mipi1a_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x80,
	.sta_ofs = 0x80,
};

#define GATE_MIPI1A(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mipi1a_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

static const struct mtk_gate mipi1a_clks[] __initconst = {
	GATE_MIPI1A(CLK_MIPI1A_CSR_CSI_EN_1A,
		"mipi1a_csr_1a", "f_fseninf_ck", 1),
};

static const struct mtk_gate_regs mipi1b_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x80,
	.sta_ofs = 0x80,
};

#define GATE_MIPI1B(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mipi1b_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

static const struct mtk_gate mipi1b_clks[] __initconst = {
	GATE_MIPI1B(CLK_MIPI1B_CSR_CSI_EN_1B,
		"mipi1b_csr_1b", "f_fseninf_ck", 1),
};

static const struct mtk_gate_regs mipi2a_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x80,
	.sta_ofs = 0x80,
};

#define GATE_MIPI2A(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mipi2a_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

static const struct mtk_gate mipi2a_clks[] __initconst = {
	GATE_MIPI2A(CLK_MIPI2A_CSR_CSI_EN_2A,
		"mipi2a_csr_2a", "f_fseninf_ck", 1),
};

static const struct mtk_gate_regs mipi2b_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x80,
	.sta_ofs = 0x80,
};

#define GATE_MIPI2B(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mipi2b_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

static const struct mtk_gate mipi2b_clks[] __initconst = {
	GATE_MIPI2B(CLK_MIPI2B_CSR_CSI_EN_2B,
		"mipi2b_csr_2b", "f_fseninf_ck", 1),
};


/* additional CCF control for mipi26M race condition(disp/camera) */
static const struct mtk_gate_regs apmixed_cg_regs = {
	.set_ofs = 0x14,
	.clr_ofs = 0x14,
	.sta_ofs = 0x14,
};

#define GATE_APMIXED(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &apmixed_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

static const struct mtk_gate apmixed_clks[] __initconst = {
	/* AUDIO0 */
#if 0
	GATE_APMIXED(CLK_APMIXED_SSUSB26M, "apmixed_ssusb26m", "f_f26m_ck",
		4),
	GATE_APMIXED(CLK_APMIXED_APPLL26M, "apmixed_appll26m", "f_f26m_ck",
		5),
#endif
	GATE_APMIXED(CLK_APMIXED_MIPIC0_26M, "apmixed_mipic026m", "f_f26m_ck",
		6),
#if 0
	GATE_APMIXED(CLK_APMIXED_MDPLLGP26M, "apmixed_mdpll26m", "f_f26m_ck",
		7),
	GATE_APMIXED(CLK_APMIXED_MMSYS_F26M, "apmixed_mmsys26m", "f_f26m_ck",
		8),
	GATE_APMIXED(CLK_APMIXED_UFS26M, "apmixed_ufs26m", "f_f26m_ck",
		9),
#endif
	GATE_APMIXED(CLK_APMIXED_MIPIC1_26M, "apmixed_mipic126m", "f_f26m_ck",
		11),
#if 0
	GATE_APMIXED(CLK_APMIXED_MEMPLL26M, "apmixed_mempll26m", "f_f26m_ck",
		13),
#endif
	GATE_APMIXED(CLK_APMIXED_CLKSQ_LVPLL_26M, "apmixed_lvpll26m",
		"f_f26m_ck", 14),
	GATE_APMIXED(CLK_APMIXED_MIPID0_26M, "apmixed_mipid026m", "f_f26m_ck",
		16),
	/* bit17 no use */
	/* GATE_APMIXED(CLK_APMIXED_MIPID1_26M, "apmixed_mipid126m",
	 *	"f_f26m_ck", 17),
	 */
};

/* FIXME: modify FMAX/FMIN/RSTBAR */
#define MT6765_PLL_FMAX		(3800UL * MHZ)
#define MT6765_PLL_FMIN		(1500UL * MHZ)

#define CON0_MT6765_RST_BAR	BIT(23)

#define PLL_INFO_NULL		(0xFF)

#define PLL_B(_id, _name, _reg, _pwr_reg, _en_mask, _flags, _pcwbits,	\
		_pcwibits, _pd_reg, _pd_shift, _tuner_reg, _tuner_en_reg,\
		_tuner_en_bit, _pcw_reg, _pcw_shift, _div_table) {\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.pwr_reg = _pwr_reg,					\
		.en_mask = _en_mask,					\
		.flags = _flags,					\
		.rst_bar_mask = CON0_MT6765_RST_BAR,			\
		.fmax = MT6765_PLL_FMAX,				\
		.fmin = MT6765_PLL_FMIN,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = _pcwibits,					\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,				\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.div_table = _div_table,				\
	}

#define PLL(_id, _name, _reg, _pwr_reg, _en_mask, _flags, _pcwbits,	\
			_pcwibits, _pd_reg, _pd_shift, _tuner_reg,	\
			_tuner_en_reg, _tuner_en_bit, _pcw_reg,	\
			_pcw_shift)	\
		PLL_B(_id, _name, _reg, _pwr_reg, _en_mask, _flags,	\
			_pcwbits, _pcwibits, _pd_reg, _pd_shift,	\
			_tuner_reg, _tuner_en_reg, _tuner_en_bit,	\
			_pcw_reg, _pcw_shift, NULL)	\

static const struct mtk_pll_data plls[] = {
	/* FIXME: need to fix flags/div_table/tuner_reg/table */
	PLL(CLK_APMIXED_ARMPLL_L, "armpll_l", 0x021C, 0x0228, BIT(0),
		PLL_AO, 22, 8, 0x0220, 24, 0, 0, 0, 0x0220, 0),
	PLL(CLK_APMIXED_ARMPLL, "armpll", 0x020C, 0x0218, BIT(0),
		PLL_AO, 22, 8, 0x0210, 24, 0, 0, 0, 0x0210, 0),
	PLL(CLK_APMIXED_CCIPLL, "ccipll", 0x022C, 0x0238, BIT(0),
		PLL_AO, 22, 8, 0x0230, 24, 0, 0, 0, 0x0230, 0),
	PLL(CLK_APMIXED_MAINPLL, "mainpll", 0x023C, 0x0248, BIT(0),
		(HAVE_RST_BAR | PLL_AO), 22, 8, 0x0240, 24, 0, 0, 0, 0x0240,
		0),
	PLL(CLK_APMIXED_MFGPLL, "mfgpll", 0x024C, 0x0258, BIT(0),
		0, 22, 8, 0x0250, 24, 0, 0, 0, 0x0250, 0),
	PLL(CLK_APMIXED_MMPLL, "mmpll", 0x025C, 0x0268, BIT(0),
		0, 22, 8, 0x0260, 24, 0, 0, 0, 0x0260, 0),
	PLL(CLK_APMIXED_UNIV2PLL, "univ2pll", 0x026C, 0x0278, BIT(0),
		HAVE_RST_BAR, 22, 8, 0x0270, 24, 0, 0, 0, 0x0270, 0),
	PLL(CLK_APMIXED_MSDCPLL, "msdcpll", 0x027C, 0x0288, BIT(0),
		0, 22, 8, 0x0280, 24, 0, 0, 0, 0x0280, 0),
	PLL(CLK_APMIXED_APLL1, "apll1", 0x028C, 0x029C, BIT(0),
		0, 32, 8, 0x0290, 24, 0x0040, 0x000C, 0, 0x0294, 0),
	PLL(CLK_APMIXED_MPLL, "mpll", 0x02A0, 0x02AC, BIT(0),
		PLL_AO, 22, 8, 0x02A4, 24, 0, 0, 0, 0x02A4, 0),
};

static const struct mtk_pll_data plls_no_armpll_ll[] = {
	/* FIXME: need to fix flags/div_table/tuner_reg/table */
	PLL(CLK_APMIXED_ARMPLL_L, "armpll_l", 0x021C, 0x0228, BIT(0),
		PLL_AO, 22, 8, 0x0220, 24, 0, 0, 0, 0x0220, 0),
	PLL(CLK_APMIXED_ARMPLL, "armpll", 0x020C, 0x0218, BIT(0),
		0, 22, 8, 0x0210, 24, 0, 0, 0, 0x0210, 0),
	PLL(CLK_APMIXED_CCIPLL, "ccipll", 0x022C, 0x0238, BIT(0),
		PLL_AO, 22, 8, 0x0230, 24, 0, 0, 0, 0x0230, 0),
	PLL(CLK_APMIXED_MAINPLL, "mainpll", 0x023C, 0x0248, BIT(0),
		(HAVE_RST_BAR | PLL_AO), 22, 8, 0x0240, 24, 0, 0, 0, 0x0240,
		0),
	PLL(CLK_APMIXED_MFGPLL, "mfgpll", 0x024C, 0x0258, BIT(0),
		0, 22, 8, 0x0250, 24, 0, 0, 0, 0x0250, 0),
	PLL(CLK_APMIXED_MMPLL, "mmpll", 0x025C, 0x0268, BIT(0),
		0, 22, 8, 0x0260, 24, 0, 0, 0, 0x0260, 0),
	PLL(CLK_APMIXED_UNIV2PLL, "univ2pll", 0x026C, 0x0278, BIT(0),
		HAVE_RST_BAR, 22, 8, 0x0270, 24, 0, 0, 0, 0x0270, 0),
	PLL(CLK_APMIXED_MSDCPLL, "msdcpll", 0x027C, 0x0288, BIT(0),
		0, 22, 8, 0x0280, 24, 0, 0, 0, 0x0280, 0),
	PLL(CLK_APMIXED_APLL1, "apll1", 0x028C, 0x029C, BIT(0),
		0, 32, 8, 0x0290, 24, 0x0040, 0x000C, 0, 0x0294, 0),
	PLL(CLK_APMIXED_MPLL, "mpll", 0x02A0, 0x02AC, BIT(0),
		PLL_AO, 22, 8, 0x02A4, 24, 0, 0, 0, 0x02A4, 0),
};

static void __init mtk_apmixedsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;
	int project_id = get_devinfo_with_index(30);

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return;
	}

	clk_data = mtk_alloc_clk_data(CLK_APMIXED_NR_CLK);

	/* FIXME: add code for APMIXEDSYS */
	if (project_id != 0x8 && project_id != 0xF)
		mtk_clk_register_plls(node, plls, ARRAY_SIZE(plls), clk_data);
	else {
		/* ARMPLL_LL disable */
		mtk_clk_register_plls(node, plls_no_armpll_ll,
			ARRAY_SIZE(plls_no_armpll_ll), clk_data);
	}
	mtk_clk_register_gates(node, apmixed_clks,
		ARRAY_SIZE(apmixed_clks), clk_data);
	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
	apmixed_base = base;

	/* MPLL, CCIPLL, MAINPLL set HW mode, TDCLKSQ, CLKSQ1 */
	clk_writel(AP_PLL_CON3, clk_readl(AP_PLL_CON3) & 0xFFFFFFE1);
	clk_writel(PLLON_CON0, clk_readl(PLLON_CON0) & 0x01041041);
	clk_writel(PLLON_CON1, clk_readl(PLLON_CON1) & 0x01041041);
}
CLK_OF_DECLARE_DRIVER(mtk_apmixedsys, "mediatek,apmixed",
		mtk_apmixedsys_init);


/* TODO: why disable critical */
static struct clk_onecell_data *mt6765_top_clk_data;

static void __init mtk_topckgen_init(struct device_node *node)
{
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return;
	}

	mt6765_top_clk_data = mtk_alloc_clk_data(CLK_TOP_NR_CLK);

	mtk_clk_register_fixed_clks(fixed_clks,
		ARRAY_SIZE(fixed_clks), mt6765_top_clk_data);

	mtk_clk_register_factors(top_divs,
		ARRAY_SIZE(top_divs), mt6765_top_clk_data);

	mtk_clk_register_muxes(top_muxes,
		ARRAY_SIZE(top_muxes), node,
		&mt6765_clk_lock, mt6765_top_clk_data);

	mtk_clk_register_gates(node, top_clks,
		ARRAY_SIZE(top_clks), mt6765_top_clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get,
		mt6765_top_clk_data);

	if (r)
		pr_info("%s(): could not register clock provider: %d\n",
			__func__, r);

	cksys_base = base;
/* FIX ME: 20171209 SPM CLK Init Flow */
	/* [4]:no need */
	clk_writel(CLK_SCP_CFG_0, clk_readl(CLK_SCP_CFG_0) | 0x3EF);
	/*[1,2,3,8]: no need*/
	clk_writel(CLK_SCP_CFG_1, clk_readl(CLK_SCP_CFG_1) | 0x1);
}
CLK_OF_DECLARE_DRIVER(mtk_topckgen, "mediatek,topckgen",
	mtk_topckgen_init);

static void __init mtk_infracfg_ao_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r;

	clk_data = mtk_alloc_clk_data(CLK_IFR_NR_CLK);

	mtk_clk_register_gates(node, ifr_clks,
		ARRAY_SIZE(ifr_clks), clk_data);
	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
}
CLK_OF_DECLARE_DRIVER(mtk_infracfg_ao, "mediatek,infracfg_ao",
	mtk_infracfg_ao_init);

static void __init mtk_audio_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r;

	clk_data = mtk_alloc_clk_data(CLK_AUDIO_NR_CLK);

	mtk_clk_register_gates(node, audio_clks,
		ARRAY_SIZE(audio_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
}
CLK_OF_DECLARE_DRIVER(mtk_audio, "mediatek,audio", mtk_audio_init);

static void __init mtk_camsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r;

	clk_data = mtk_alloc_clk_data(CLK_CAM_NR_CLK);

	mtk_clk_register_gates(node, cam_clks, ARRAY_SIZE(cam_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
}
CLK_OF_DECLARE_DRIVER(mtk_camsys, "mediatek,camsys", mtk_camsys_init);

static void __init mtk_imgsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r;

	clk_data = mtk_alloc_clk_data(CLK_IMG_NR_CLK);

	mtk_clk_register_gates(node, img_clks, ARRAY_SIZE(img_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
}
CLK_OF_DECLARE_DRIVER(mtk_imgsys, "mediatek,imgsys", mtk_imgsys_init);

static void __init mtk_mmsys_config_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return;
	}
	clk_data = mtk_alloc_clk_data(CLK_MM_NR_CLK);

	mtk_clk_register_gates(node, mm_clks, ARRAY_SIZE(mm_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
	mmsys_config_base = base;
}
CLK_OF_DECLARE_DRIVER(mtk_mmsys_config, "mediatek,mmsys_config",
		mtk_mmsys_config_init);

static void __init mtk_venc_global_con_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r;

	clk_data = mtk_alloc_clk_data(CLK_VENC_NR_CLK);

	mtk_clk_register_gates(node, venc_clks,
		ARRAY_SIZE(venc_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
}
CLK_OF_DECLARE_DRIVER(mtk_venc_global_con, "mediatek,venc_gcon",
		mtk_venc_global_con_init);

static void __init mtk_mipi0a_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r;

	clk_data = mtk_alloc_clk_data(CLK_MIPI0A_NR_CLK);

	mtk_clk_register_gates(node, mipi0a_clks,
		ARRAY_SIZE(mipi0a_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
}
CLK_OF_DECLARE_DRIVER(mtk_mipi0a, "mediatek,mipi_rx_ana_csi0a",
		mtk_mipi0a_init);

static void __init mtk_mipi0b_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r;

	clk_data = mtk_alloc_clk_data(CLK_MIPI0B_NR_CLK);

	mtk_clk_register_gates(node, mipi0b_clks,
		ARRAY_SIZE(mipi0b_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
}
CLK_OF_DECLARE_DRIVER(mtk_mipi0b, "mediatek,mipi_rx_ana_csi0b",
		mtk_mipi0b_init);

static void __init mtk_mipi1a_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r;

	clk_data = mtk_alloc_clk_data(CLK_MIPI1A_NR_CLK);

	mtk_clk_register_gates(node, mipi1a_clks,
		ARRAY_SIZE(mipi1a_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
}
CLK_OF_DECLARE_DRIVER(mtk_mipi1a, "mediatek,mipi_rx_ana_csi1a",
		mtk_mipi1a_init);

static void __init mtk_mipi1b_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r;

	clk_data = mtk_alloc_clk_data(CLK_MIPI1B_NR_CLK);

	mtk_clk_register_gates(node, mipi1b_clks,
		ARRAY_SIZE(mipi1b_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
}
CLK_OF_DECLARE_DRIVER(mtk_mipi1b, "mediatek,mipi_rx_ana_csi1b",
		mtk_mipi1b_init);

static void __init mtk_mipi2a_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r;

	clk_data = mtk_alloc_clk_data(CLK_MIPI2A_NR_CLK);

	mtk_clk_register_gates(node, mipi2a_clks,
		ARRAY_SIZE(mipi2a_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
}
CLK_OF_DECLARE_DRIVER(mtk_mipi2a, "mediatek,mipi_rx_ana_csi2a",
		mtk_mipi2a_init);

static void __init mtk_mipi2b_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	int r;

	clk_data = mtk_alloc_clk_data(CLK_MIPI2B_NR_CLK);

	mtk_clk_register_gates(node, mipi2b_clks,
		ARRAY_SIZE(mipi2b_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
}
CLK_OF_DECLARE_DRIVER(mtk_mipi2b, "mediatek,mipi_rx_ana_csi2b",
		mtk_mipi2b_init);

unsigned int mt_get_ckgen_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned int temp, clk26cali_0, clk_dbg_cfg;
	unsigned int clk_misc_cfg_0, clk26cali_1;

	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	/*sel ckgen_cksw[22] and enable freq meter
	 * sel ckgen[21:16], 01:hd_faxi_ck
	 */
	clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFFFC0FC)|(ID << 8)|(0x1));

	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	/* select divider?dvt set zero */
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF));
	clk26cali_0 = clk_readl(CLK26CALI_0);
	clk26cali_1 = clk_readl(CLK26CALI_1);
	clk_writel(CLK26CALI_0, 0x1000);
	clk_writel(CLK26CALI_0, 0x1010);

	/* wait frequency meter finish */
	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(10);
		i++;
		if (i > 10000)
			break;
	}

	temp = clk_readl(CLK26CALI_1) & 0xFFFF;

	output = (temp * 26000) / 1024; /* Khz */

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	clk_writel(CLK26CALI_0, clk26cali_0);
	clk_writel(CLK26CALI_1, clk26cali_1);

	/* print("ckgen meter[%d] = %d Khz\n", ID, output); */
	if (i > 10000)
		return 0;
	else
		return output;

}

unsigned int mt_get_abist_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned int temp, clk26cali_0, clk_dbg_cfg;
	unsigned int clk_misc_cfg_0, clk26cali_1;

	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	/* sel abist_cksw and enable freq meter sel abist */
	clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFC0FFFC)|(ID << 16));
	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	/* select divider, WAIT CONFIRM */
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF) | (0x3 << 24));
	clk26cali_0 = clk_readl(CLK26CALI_0);
	clk26cali_1 = clk_readl(CLK26CALI_1);
	clk_writel(CLK26CALI_0, 0x1000);
	clk_writel(CLK26CALI_0, 0x1010);

	/* wait frequency meter finish */
	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(10);
		i++;
		if (i > 10000)
			break;
	}

	temp = clk_readl(CLK26CALI_1) & 0xFFFF;
	output = (temp * 26000) / 1024; /* Khz */

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	clk_writel(CLK26CALI_0, clk26cali_0);
	clk_writel(CLK26CALI_1, clk26cali_1);

	/*pr_debug("%s = %d Khz\n", abist_array[ID-1], output);*/
	if (i > 10000)
		return 0;
	else
		return output * 4;
}

void mp_enter_suspend(int id, int suspend)
{
	/* mp0*/
	if (id == 0) {
		if (suspend) {
			clk_writel(PLLON_CON0, clk_readl(PLLON_CON0)
				& (~ARMPLL_HW_CTRL));
		} else {
			clk_writel(PLLON_CON0, clk_readl(PLLON_CON0)
				| ARMPLL_HW_CTRL);
		}
	} else if (id == 1) { /* mp1 */
		if (suspend) {
			clk_writel(PLLON_CON0, clk_readl(PLLON_CON0)
				& (~ARMPLL_L_HW_CTRL));
		} else {
			clk_writel(PLLON_CON0, clk_readl(PLLON_CON0)
				| ARMPLL_L_HW_CTRL);
		}
	}
}

void pll_if_on(void)
{
}

void clock_force_on(void)
{
#if 0
	/* INFRACFG */
	clk_writel(MODULE_SW_CG_0_CLR, INFRA_CG2);
	clk_writel(MODULE_SW_CG_1_CLR, INFRA_CG3);
	clk_writel(MODULE_SW_CG_2_CLR, INFRA_CG4);
	clk_writel(MODULE_SW_CG_3_CLR, INFRA_CG5);
	/* DISP CG */
	clk_writel(MMSYS_CG_CLR0, MM_DISABLE_CG);
	/* AUDIO */
	clk_writel(AUDIO_TOP_CON0,
		clk_readl(AUDIO_TOP_CON0) & ~AUDIO_DISABLE_CG0);
	clk_writel(AUDIO_TOP_CON1,
		clk_readl(AUDIO_TOP_CON1) & ~AUDIO_DISABLE_CG1);
	/* ISP */
	clk_writel(IMG_CG_CLR, IMG_DISABLE_CG);
	/* VENC not inverse */
	clk_writel(VENC_CG_SET, VENC_DISABLE_CG);
	/* CAM */
	clk_writel(CAMSYS_CG_CLR, CAMSYS_DISABLE_CG);
	/* GCE AO */
	/* MIPI */
	clk_writel(MIPI_RX_WRAPPER80_CSI0A,
		clk_readl(MIPI_RX_WRAPPER80_CSI0A) | MIPI_CSI_DISABLE_CG);
	clk_writel(MIPI_RX_WRAPPER80_CSI0B,
		clk_readl(MIPI_RX_WRAPPER80_CSI0B) | MIPI_CSI_DISABLE_CG);
	clk_writel(MIPI_RX_WRAPPER80_CSI1A,
		clk_readl(MIPI_RX_WRAPPER80_CSI1A) | MIPI_CSI_DISABLE_CG);
	clk_writel(MIPI_RX_WRAPPER80_CSI1B,
		clk_readl(MIPI_RX_WRAPPER80_CSI1B) | MIPI_CSI_DISABLE_CG);
	clk_writel(MIPI_RX_WRAPPER80_CSI2A,
		clk_readl(MIPI_RX_WRAPPER80_CSI2A) | MIPI_CSI_DISABLE_CG);
	clk_writel(MIPI_RX_WRAPPER80_CSI2B,
		clk_readl(MIPI_RX_WRAPPER80_CSI2B) | MIPI_CSI_DISABLE_CG);
#endif
}

void clock_force_off(void)
{
#if 0
	/* DISP CG */
	clk_writel(MMSYS_CG_SET0, MM_DISABLE_CG);
	/* AUDIO */
	clk_writel(AUDIO_TOP_CON0,
		clk_readl(AUDIO_TOP_CON0) | AUDIO_DISABLE_CG0);
	clk_writel(AUDIO_TOP_CON1,
		clk_readl(AUDIO_TOP_CON1) | AUDIO_DISABLE_CG1);
	/* MFG AO */
	/* ISP */
	clk_writel(IMG_CG_SET, IMG_DISABLE_CG);
	/* VENC not inverse */
	clk_writel(VENC_CG_CLR, VENC_DISABLE_CG);
	/* CAM */
	clk_writel(CAMSYS_CG_SET, CAMSYS_DISABLE_CG);
	/* GCE AO */
	/* MIPI */
	clk_writel(MIPI_RX_WRAPPER80_CSI0A,
		clk_readl(MIPI_RX_WRAPPER80_CSI0A) & ~MIPI_CSI_DISABLE_CG);
	clk_writel(MIPI_RX_WRAPPER80_CSI0B,
		clk_readl(MIPI_RX_WRAPPER80_CSI0B) & ~MIPI_CSI_DISABLE_CG);
	clk_writel(MIPI_RX_WRAPPER80_CSI1A,
		clk_readl(MIPI_RX_WRAPPER80_CSI1A) & ~MIPI_CSI_DISABLE_CG);
	clk_writel(MIPI_RX_WRAPPER80_CSI1B,
		clk_readl(MIPI_RX_WRAPPER80_CSI1B) & ~MIPI_CSI_DISABLE_CG);
	clk_writel(MIPI_RX_WRAPPER80_CSI2A,
		clk_readl(MIPI_RX_WRAPPER80_CSI2A) & ~MIPI_CSI_DISABLE_CG);
	clk_writel(MIPI_RX_WRAPPER80_CSI2B,
		clk_readl(MIPI_RX_WRAPPER80_CSI2B) & ~MIPI_CSI_DISABLE_CG);
#endif
}

void mmsys_cg_check(void)
{
	pr_notice("[MMSYS_CG_CON0]=0x%08x\n", clk_readl(MMSYS_CG_CON0));
}

void pll_force_off(void)
{
/*MFGPLL*/
	clk_clrl(MFGPLL_CON0, PLL_EN);
	clk_setl(MFGPLL_CON3, PLL_ISO_EN);
	clk_clrl(MFGPLL_CON3, PLL_PWR_ON);
/*MPLL Control by dram*/
/*UNIVPLL*/
	clk_clrl(UNIVPLL_CON0, PLL_EN);
	clk_setl(UNIVPLL_CON3, PLL_ISO_EN);
	clk_clrl(UNIVPLL_CON3, PLL_PWR_ON);
/*MSDCPLL*/
	clk_clrl(MSDCPLL_CON0, PLL_EN);
	clk_setl(MSDCPLL_CON3, PLL_ISO_EN);
	clk_clrl(MSDCPLL_CON3, PLL_PWR_ON);
/*MMPLL control by mtcmos*/
/*APLL1*/
	clk_clrl(APLL1_CON0, PLL_EN);
	clk_setl(APLL1_CON4, PLL_ISO_EN);
	clk_clrl(APLL1_CON4, PLL_PWR_ON);
}

void armpll_control(int id, int on)
{
	if (id == 1) {
		if (on) {
			mt_reg_sync_writel((clk_readl(ARMPLL_CON3))
				| (PLL_PWR_ON), ARMPLL_CON3);
			udelay(30);
			mt_reg_sync_writel((clk_readl(ARMPLL_CON3))
				& (~PLL_ISO_EN), ARMPLL_CON3);
			udelay(10);
			mt_reg_sync_writel((clk_readl(ARMPLL_CON1))
				| (PLL_SDM_PCW_CHG), ARMPLL_CON1);
			mt_reg_sync_writel((clk_readl(ARMPLL_CON0)
				& (~PLL_DIV_RSTB)) | (PLL_EN), ARMPLL_CON0);
			udelay(20);
			mt_reg_sync_writel((clk_readl(ARMPLL_CON0))
				| (PLL_DIV_RSTB), ARMPLL_CON0);
		} else {
			mt_reg_sync_writel((clk_readl(ARMPLL_CON0))
				& (~PLL_EN), ARMPLL_CON0);
			mt_reg_sync_writel((clk_readl(ARMPLL_CON3))
				| (PLL_ISO_EN), ARMPLL_CON3);
			mt_reg_sync_writel((clk_readl(ARMPLL_CON3))
				& (~PLL_PWR_ON), ARMPLL_CON3);
		}
	} else if (id == 2) {
		if (on) {
			mt_reg_sync_writel((clk_readl(ARMPLL_L_CON3))
				| (PLL_PWR_ON), ARMPLL_L_CON3);
			udelay(30);
			mt_reg_sync_writel((clk_readl(ARMPLL_L_CON3))
				& (~PLL_ISO_EN), ARMPLL_L_CON3);
			udelay(10);
			mt_reg_sync_writel((clk_readl(ARMPLL_L_CON1))
				| (PLL_SDM_PCW_CHG), ARMPLL_L_CON1);
			mt_reg_sync_writel((clk_readl(ARMPLL_L_CON0)
				& (~PLL_DIV_RSTB)) | (PLL_EN), ARMPLL_L_CON0);
			udelay(20);
			mt_reg_sync_writel((clk_readl(ARMPLL_L_CON0))
				| (PLL_DIV_RSTB), ARMPLL_L_CON0);
		} else {
			mt_reg_sync_writel((clk_readl(ARMPLL_L_CON0))
				& (~PLL_EN), ARMPLL_L_CON0);
			mt_reg_sync_writel((clk_readl(ARMPLL_L_CON3))
				| (PLL_ISO_EN), ARMPLL_L_CON3);
			mt_reg_sync_writel((clk_readl(ARMPLL_L_CON3))
				& (~PLL_PWR_ON), ARMPLL_L_CON3);
		}
	}
}

int mtk_is_mtcmos_enable(void)
{
#if MT_MTCMOS_ENABLE
	return 1;
#else
	return 0;
#endif
}

static int __init clk_mt6765_init(void)
{
	return 0;
}
arch_initcall(clk_mt6765_init);

