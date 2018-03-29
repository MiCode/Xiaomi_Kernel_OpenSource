/*
 * Copyright (C) 2015 MediaTek Inc.
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

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include <linux/clk-provider.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/module.h>
#include <linux/version.h>

#define DUMMY_REG_TEST		0
#define DUMP_INIT_STATE		0
#define CLKDBG_PM_DOMAIN	0
#define CLKDBG_8173		1
#define CLKDBG_8173_TK		0

#define TAG	"[clkdbg] "

#define clk_err(fmt, args...)	pr_err(TAG fmt, ##args)
#define clk_warn(fmt, args...)	pr_warn(TAG fmt, ##args)
#define clk_info(fmt, args...)	pr_debug(TAG fmt, ##args)
#define clk_dbg(fmt, args...)	pr_debug(TAG fmt, ##args)
#define clk_ver(fmt, args...)	pr_debug(TAG fmt, ##args)

/************************************************
 **********      register access       **********
 ************************************************/

#ifndef BIT
#define BIT(_bit_)		(u32)(1U << (_bit_))
#endif

#ifndef GENMASK
#define GENMASK(h, l)	(((1U << ((h) - (l) + 1)) - 1) << (l))
#endif

#define ALT_BITS(o, h, l, v) \
	(((o) & ~GENMASK(h, l)) | (((v) << (l)) & GENMASK(h, l)))

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */
#define clk_setl(addr, val)	clk_writel(addr, clk_readl(addr) | (val))
#define clk_clrl(addr, val)	clk_writel(addr, clk_readl(addr) & ~(val))
#define clk_writel_mask(addr, h, l, v)	\
	clk_writel(addr, (clk_readl(addr) & ~GENMASK(h, l)) | ((v) << (l)))

#define ABS_DIFF(a, b)	((a) > (b) ? (a) - (b) : (b) - (a))

/************************************************
 **********      struct definition     **********
 ************************************************/

#if CLKDBG_8173

static void __iomem *topckgen_base;	/* 0x10000000 */
static void __iomem *infrasys_base;	/* 0x10001000 */
static void __iomem *perisys_base;	/* 0x10003000 */
static void __iomem *mcucfg_base;	/* 0x10200000 */
static void __iomem *apmixedsys_base;	/* 0x10209000 */
static void __iomem *audiosys_base;	/* 0x11220000 */
static void __iomem *mfgsys_base;	/* 0x13fff000 */
static void __iomem *mmsys_base;	/* 0x14000000 */
static void __iomem *imgsys_base;	/* 0x15000000 */
static void __iomem *vdecsys_base;	/* 0x16000000 */
static void __iomem *vencsys_base;	/* 0x18000000 */
static void __iomem *vencltsys_base;	/* 0x19000000 */
static void __iomem *scpsys_base;	/* 0x10006000 */

#define TOPCKGEN_REG(ofs)	(topckgen_base + ofs)
#define INFRA_REG(ofs)		(infrasys_base + ofs)
#define PREI_REG(ofs)		(perisys_base + ofs)
#define MCUCFG_REG(ofs)		(mcucfg_base + ofs)
#define APMIXED_REG(ofs)	(apmixedsys_base + ofs)
#define AUDIO_REG(ofs)		(audiosys_base + ofs)
#define MFG_REG(ofs)		(mfgsys_base + ofs)
#define MM_REG(ofs)		(mmsys_base + ofs)
#define IMG_REG(ofs)		(imgsys_base + ofs)
#define VDEC_REG(ofs)		(vdecsys_base + ofs)
#define VENC_REG(ofs)		(vencsys_base + ofs)
#define VENCLT_REG(ofs)		(vencltsys_base + ofs)
#define SCP_REG(ofs)		(scpsys_base + ofs)

#define CLK_MISC_CFG_1		TOPCKGEN_REG(0x214)
#define CLK_MISC_CFG_2		TOPCKGEN_REG(0x218)
#define CLK26CALI_0		TOPCKGEN_REG(0x220)
#define CLK26CALI_1		TOPCKGEN_REG(0x224)
#define CLK26CALI_2		TOPCKGEN_REG(0x228)

#define CLK_CFG_0		TOPCKGEN_REG(0x040)
#define CLK_CFG_1		TOPCKGEN_REG(0x050)
#define CLK_CFG_2		TOPCKGEN_REG(0x060)
#define CLK_CFG_3		TOPCKGEN_REG(0x070)
#define CLK_CFG_4		TOPCKGEN_REG(0x080)
#define CLK_CFG_5		TOPCKGEN_REG(0x090)
#define CLK_CFG_6		TOPCKGEN_REG(0x0A0)
#define CLK_CFG_7		TOPCKGEN_REG(0x0B0)
#define CLK_CFG_8		TOPCKGEN_REG(0x100)
#define CLK_CFG_9		TOPCKGEN_REG(0x104)
#define CLK_CFG_10		TOPCKGEN_REG(0x108)
#define CLK_CFG_11		TOPCKGEN_REG(0x10C)
#define CLK_CFG_12		TOPCKGEN_REG(0x0C0)
#define CLK_CFG_13		TOPCKGEN_REG(0x0D0)

#define PLL_HP_CON0		APMIXED_REG(0xF00)

/* APMIXEDSYS Register */

#define ARMCA15PLL_CON0		APMIXED_REG(0x200)
#define ARMCA15PLL_CON1		APMIXED_REG(0x204)
#define ARMCA15PLL_PWR_CON0	APMIXED_REG(0x20C)

#define ARMCA7PLL_CON0		APMIXED_REG(0x210)
#define ARMCA7PLL_CON1		APMIXED_REG(0x214)
#define ARMCA7PLL_PWR_CON0	APMIXED_REG(0x21C)

#define MAINPLL_CON0		APMIXED_REG(0x220)
#define MAINPLL_CON1		APMIXED_REG(0x224)
#define MAINPLL_PWR_CON0	APMIXED_REG(0x22C)

#define UNIVPLL_CON0		APMIXED_REG(0x230)
#define UNIVPLL_CON1		APMIXED_REG(0x234)
#define UNIVPLL_PWR_CON0	APMIXED_REG(0x23C)

#define MMPLL_CON0		APMIXED_REG(0x240)
#define MMPLL_CON1		APMIXED_REG(0x244)
#define MMPLL_PWR_CON0		APMIXED_REG(0x24C)

#define MSDCPLL_CON0		APMIXED_REG(0x250)
#define MSDCPLL_CON1		APMIXED_REG(0x254)
#define MSDCPLL_PWR_CON0	APMIXED_REG(0x25C)

#define VENCPLL_CON0		APMIXED_REG(0x260)
#define VENCPLL_CON1		APMIXED_REG(0x264)
#define VENCPLL_PWR_CON0	APMIXED_REG(0x26C)

#define TVDPLL_CON0		APMIXED_REG(0x270)
#define TVDPLL_CON1		APMIXED_REG(0x274)
#define TVDPLL_PWR_CON0		APMIXED_REG(0x27C)

#define MPLL_CON0		APMIXED_REG(0x280)
#define MPLL_CON1		APMIXED_REG(0x284)
#define MPLL_PWR_CON0		APMIXED_REG(0x28C)

#define VCODECPLL_CON0		APMIXED_REG(0x290)
#define VCODECPLL_CON1		APMIXED_REG(0x294)
#define VCODECPLL_PWR_CON0	APMIXED_REG(0x29C)

#define APLL1_CON0		APMIXED_REG(0x2A0)
#define APLL1_CON1		APMIXED_REG(0x2A4)
#define APLL1_PWR_CON0		APMIXED_REG(0x2B0)

#define APLL2_CON0		APMIXED_REG(0x2B4)
#define APLL2_CON1		APMIXED_REG(0x2B8)
#define APLL2_PWR_CON0		APMIXED_REG(0x2C4)

#define LVDSPLL_CON0		APMIXED_REG(0x2D0)
#define LVDSPLL_CON1		APMIXED_REG(0x2D4)
#define LVDSPLL_PWR_CON0	APMIXED_REG(0x2DC)

#define MSDCPLL2_CON0		APMIXED_REG(0x2F0)
#define MSDCPLL2_CON1		APMIXED_REG(0x2F4)
#define MSDCPLL2_PWR_CON0	APMIXED_REG(0x2FC)

/* INFRASYS Register */

#define INFRA_TOPCKGEN_DCMCTL	INFRA_REG(0x0010)
#define INFRA_PDN_SET		INFRA_REG(0x0040)
#define INFRA_PDN_CLR		INFRA_REG(0x0044)
#define INFRA_PDN_STA		INFRA_REG(0x0048)

/* PERIFCG_Register */

#define PERI_PDN0_SET		PREI_REG(0x0008)
#define PERI_PDN0_CLR		PREI_REG(0x0010)
#define PERI_PDN0_STA		PREI_REG(0x0018)

#define PERI_PDN1_SET		PREI_REG(0x000C)
#define PERI_PDN1_CLR		PREI_REG(0x0014)
#define PERI_PDN1_STA		PREI_REG(0x001C)

/* MCUCFG Register */

#define MCU_26C			MCUCFG_REG(0x026C)
#define ARMPLL_JIT_CTRL		MCUCFG_REG(0x064C)

/* Audio Register */

#define AUDIO_TOP_CON0		AUDIO_REG(0x0000)

/* MFGCFG Register */

#define MFG_CG_CON		MFG_REG(0)
#define MFG_CG_SET		MFG_REG(4)
#define MFG_CG_CLR		MFG_REG(8)

/* MMSYS Register */

#define DISP_CG_CON0		MM_REG(0x100)
#define DISP_CG_SET0		MM_REG(0x104)
#define DISP_CG_CLR0		MM_REG(0x108)
#define DISP_CG_CON1		MM_REG(0x110)
#define DISP_CG_SET1		MM_REG(0x114)
#define DISP_CG_CLR1		MM_REG(0x118)

/* IMGSYS Register */
#define IMG_CG_CON		IMG_REG(0x0000)
#define IMG_CG_SET		IMG_REG(0x0004)
#define IMG_CG_CLR		IMG_REG(0x0008)

/* VDEC Register */
#define VDEC_CKEN_SET		VDEC_REG(0x0000)
#define VDEC_CKEN_CLR		VDEC_REG(0x0004)
#define LARB_CKEN_SET		VDEC_REG(0x0008)
#define LARB_CKEN_CLR		VDEC_REG(0x000C)

/* VENC Register */
#define VENC_CG_CON		VENC_REG(0x0)
#define VENC_CG_SET		VENC_REG(0x4)
#define VENC_CG_CLR		VENC_REG(0x8)

#define VENCLT_CG_CON		VENCLT_REG(0x0)
#define VENCLT_CG_SET		VENCLT_REG(0x4)
#define VENCLT_CG_CLR		VENCLT_REG(0x8)

/* SCPSYS Register */
#define SPM_VDE_PWR_CON		SCP_REG(0x210)
#define SPM_MFG_PWR_CON		SCP_REG(0x214)
#define SPM_VEN_PWR_CON		SCP_REG(0x230)
#define SPM_ISP_PWR_CON		SCP_REG(0x238)
#define SPM_DIS_PWR_CON		SCP_REG(0x23c)
#define SPM_VEN2_PWR_CON	SCP_REG(0x298)
#define SPM_AUDIO_PWR_CON	SCP_REG(0x29c)
#define SPM_MFG_2D_PWR_CON	SCP_REG(0x2c0)
#define SPM_MFG_ASYNC_PWR_CON	SCP_REG(0x2c4)
#define SPM_USB_PWR_CON		SCP_REG(0x2cc)
#define SPM_PWR_STATUS		SCP_REG(0x60c)
#define SPM_PWR_STATUS_2ND	SCP_REG(0x610)

#endif /* CLKDBG_SOC */

static bool is_valid_reg(void __iomem *addr)
{
	return ((u64)addr & 0xf0000000) || (((u64)addr >> 32) & 0xf0000000);
}

enum FMETER_TYPE {
	ABIST,
	CKGEN
};

enum ABIST_CLK {
	ABIST_CLK_NULL,

#if CLKDBG_8173

	AD_MEMPLL2_CKOUT0_PRE_ISO	= 1,
	AD_ARMCA7PLL_754M_CORE_CK	= 2,
	AD_ARMCA7PLL_502M_CORE_CK	= 3,
	AD_MAIN_H546M_CK		= 4,
	AD_MAIN_H364M_CK		= 5,
	AD_MAIN_H218P4M_CK		= 6,
	AD_MAIN_H156M_CK		= 7,
	AD_UNIV_178P3M_CK		= 8,
	AD_UNIV_48M_CK			= 9,
	AD_UNIV_624M_CK			= 10,
	AD_UNIV_416M_CK			= 11,
	AD_UNIV_249P6M_CK		= 12,
	AD_APLL1_180P6336M_CK		= 13,
	AD_APLL2_196P608M_CK		= 14,
	AD_LTEPLL_FS26M_CK		= 15,
	RTC32K_CK_I			= 16,
	AD_MMPLL_455M_CK		= 17,
	AD_VENCPLL_760M_CK		= 18,
	AD_VCODEPLL_494M_CK		= 19,
	AD_TVDPLL_594M_CK		= 20,
	AD_SSUSB_SYSPLL_125M_CK		= 21,
	AD_MSDCPLL_806M_CK		= 22,
	AD_DPICLK			= 23,
	CLKPH_MCK_O			= 24,
	AD_USB_48M_CK			= 25,
	AD_MSDCPLL2_CK			= 26,
	AD_VCODECPLL_370P5M_CK		= 27,
	AD_TVDPLL_445P5M_CK		= 30,
	SOC_ROSC			= 31,
	SOC_ROSC1			= 32,
	AD_HDMITX_CLKDIG_CTS_D4_DEBUG	= 33,
	AD_HDMITX_MON_CK_D4_DEBUG	= 34,
	AD_MIPI_26M_CK			= 35,
	AD_LTEPLL_ARMPLL26M_CK		= 36,
	AD_LETPLL_SSUSB26M_CK		= 37,
	AD_HDMITX_MON_CK		= 38,
	AD_MPLL_26M_26M_CK_CKSYS	= 39,
	HSIC_AD_480M_CK			= 40,
	AD_HDMITX_REF_CK_CKSYS		= 41,
	AD_HDMITX_CLKDIG_CTS_CKSYS	= 42,
	AD_PLLGP_TST_CK			= 43,
	AD_SSUSB_48M_CJ			= 44,
	AD_MEM_26M_CK			= 45,
	ARMPLL_OCC_MON			= 46,
	AD_ARMPLLGP_TST_CK		= 47,
	AD_MEMPLL_MONCLK		= 48,
	AD_MEMPLL2_MONCLK		= 49,
	AD_MEMPLL3_MONCLK		= 50,
	AD_MEMPLL4_MONCLK		= 51,
	AD_MEMPLL_REFCLK		= 52,
	AD_MEMPLL_FBCLK			= 53,
	AD_MEMPLL2_REFCLK		= 54,
	AD_MEMPLL2_FBCLK		= 55,
	AD_MEMPLL3_REFCLK		= 56,
	AD_MEMPLL3_FBCLK		= 57,
	AD_MEMPLL4_REFCLK		= 58,
	AD_MEMPLL4_FBCLK		= 59,
	AD_MEMPLL_TSTDIV2_CK		= 60,
	AD_MEM2MIPI_26M_CK_		= 61,
	AD_MEMPLL_MONCLK_B		= 62,
	AD_MEMPLL2_MONCLK_B		= 63,
	AD_MEMPLL3_MONCLK_B		= 65,
	AD_MEMPLL4_MONCLK_B		= 66,
	AD_MEMPLL_REFCLK_B		= 67,
	AD_MEMPLL_FBCLK_B		= 68,
	AD_MEMPLL2_REFCLK_B		= 69,
	AD_MEMPLL2_FBCLK_B		= 70,
	AD_MEMPLL3_REFCLK_B		= 71,
	AD_MEMPLL3_FBCLK_B		= 72,
	AD_MEMPLL4_REFCLK_B		= 73,
	AD_MEMPLL4_FBCLK_B		= 74,
	AD_MEMPLL_TSTDIV2_CK_B		= 75,
	AD_MEM2MIPI_26M_CK_B_		= 76,
	AD_LVDSTX1_MONCLK		= 77,
	AD_MONREF1_CK			= 78,
	AD_MONFBK1			= 79,
	AD_DSI0_LNTC_DSICLK		= 91,
	AD_DSI0_MPPLL_TST_CK		= 92,
	AD_DSI1_LNTC_DSICLK		= 93,
	AD_DSI1_MPPLL_TST_CK		= 94,
	AD_CR4_249P2M_CK		= 95,

#endif /* CLKDBG_8173 */

	ABIST_CLK_END,
};

static const char * const ABIST_CLK_NAME[] = {
#if CLKDBG_8173
	[AD_MEMPLL2_CKOUT0_PRE_ISO]	= "AD_MEMPLL2_CKOUT0_PRE_ISO",
	[AD_ARMCA7PLL_754M_CORE_CK]	= "AD_ARMCA7PLL_754M_CORE_CK",
	[AD_ARMCA7PLL_502M_CORE_CK]	= "AD_ARMCA7PLL_502M_CORE_CK",
	[AD_MAIN_H546M_CK]		= "AD_MAIN_H546M_CK",
	[AD_MAIN_H364M_CK]		= "AD_MAIN_H364M_CK",
	[AD_MAIN_H218P4M_CK]		= "AD_MAIN_H218P4M_CK",
	[AD_MAIN_H156M_CK]		= "AD_MAIN_H156M_CK",
	[AD_UNIV_178P3M_CK]		= "AD_UNIV_178P3M_CK",
	[AD_UNIV_48M_CK]		= "AD_UNIV_48M_CK",
	[AD_UNIV_624M_CK]		= "AD_UNIV_624M_CK",
	[AD_UNIV_416M_CK]		= "AD_UNIV_416M_CK",
	[AD_UNIV_249P6M_CK]		= "AD_UNIV_249P6M_CK",
	[AD_APLL1_180P6336M_CK]		= "AD_APLL1_180P6336M_CK",
	[AD_APLL2_196P608M_CK]		= "AD_APLL2_196P608M_CK",
	[AD_LTEPLL_FS26M_CK]		= "AD_LTEPLL_FS26M_CK",
	[RTC32K_CK_I]			= "rtc32k_ck_i",
	[AD_MMPLL_455M_CK]		= "AD_MMPLL_455M_CK",
	[AD_VENCPLL_760M_CK]		= "AD_VENCPLL_760M_CK",
	[AD_VCODEPLL_494M_CK]		= "AD_VCODEPLL_494M_CK",
	[AD_TVDPLL_594M_CK]		= "AD_TVDPLL_594M_CK",
	[AD_SSUSB_SYSPLL_125M_CK]	= "AD_SSUSB_SYSPLL_125M_CK",
	[AD_MSDCPLL_806M_CK]		= "AD_MSDCPLL_806M_CK",
	[AD_DPICLK]			= "AD_DPICLK",
	[CLKPH_MCK_O]			= "clkph_MCK_o",
	[AD_USB_48M_CK]			= "AD_USB_48M_CK",
	[AD_MSDCPLL2_CK]		= "AD_MSDCPLL2_CK",
	[AD_VCODECPLL_370P5M_CK]	= "AD_VCODECPLL_370P5M_CK",
	[AD_TVDPLL_445P5M_CK]		= "AD_TVDPLL_445P5M_CK",
	[SOC_ROSC]			= "soc_rosc",
	[SOC_ROSC1]			= "soc_rosc1",
	[AD_HDMITX_CLKDIG_CTS_D4_DEBUG]	= "AD_HDMITX_CLKDIG_CTS_D4_debug",
	[AD_HDMITX_MON_CK_D4_DEBUG]	= "AD_HDMITX_MON_CK_D4_debug",
	[AD_MIPI_26M_CK]		= "AD_MIPI_26M_CK",
	[AD_LTEPLL_ARMPLL26M_CK]	= "AD_LTEPLL_ARMPLL26M_CK",
	[AD_LETPLL_SSUSB26M_CK]		= "AD_LETPLL_SSUSB26M_CK",
	[AD_HDMITX_MON_CK]		= "AD_HDMITX_MON_CK",
	[AD_MPLL_26M_26M_CK_CKSYS]	= "AD_MPLL_26M_26M_CK_CKSYS",
	[HSIC_AD_480M_CK]		= "HSIC_AD_480M_CK",
	[AD_HDMITX_REF_CK_CKSYS]	= "AD_HDMITX_REF_CK_CKSYS",
	[AD_HDMITX_CLKDIG_CTS_CKSYS]	= "AD_HDMITX_CLKDIG_CTS_CKSYS",
	[AD_PLLGP_TST_CK]		= "AD_PLLGP_TST_CK",
	[AD_SSUSB_48M_CJ]		= "AD_SSUSB_48M_CJ",
	[AD_MEM_26M_CK]			= "AD_MEM_26M_CK",
	[ARMPLL_OCC_MON]		= "armpll_occ_mon",
	[AD_ARMPLLGP_TST_CK]		= "AD_ARMPLLGP_TST_CK",
	[AD_MEMPLL_MONCLK]		= "AD_MEMPLL_MONCLK",
	[AD_MEMPLL2_MONCLK]		= "AD_MEMPLL2_MONCLK",
	[AD_MEMPLL3_MONCLK]		= "AD_MEMPLL3_MONCLK",
	[AD_MEMPLL4_MONCLK]		= "AD_MEMPLL4_MONCLK",
	[AD_MEMPLL_REFCLK]		= "AD_MEMPLL_REFCLK",
	[AD_MEMPLL_FBCLK]		= "AD_MEMPLL_FBCLK",
	[AD_MEMPLL2_REFCLK]		= "AD_MEMPLL2_REFCLK",
	[AD_MEMPLL2_FBCLK]		= "AD_MEMPLL2_FBCLK",
	[AD_MEMPLL3_REFCLK]		= "AD_MEMPLL3_REFCLK",
	[AD_MEMPLL3_FBCLK]		= "AD_MEMPLL3_FBCLK",
	[AD_MEMPLL4_REFCLK]		= "AD_MEMPLL4_REFCLK",
	[AD_MEMPLL4_FBCLK]		= "AD_MEMPLL4_FBCLK",
	[AD_MEMPLL_TSTDIV2_CK]		= "AD_MEMPLL_TSTDIV2_CK",
	[AD_MEM2MIPI_26M_CK_]		= "AD_MEM2MIPI_26M_CK_",
	[AD_MEMPLL_MONCLK_B]		= "AD_MEMPLL_MONCLK_B",
	[AD_MEMPLL2_MONCLK_B]		= "AD_MEMPLL2_MONCLK_B",
	[AD_MEMPLL3_MONCLK_B]		= "AD_MEMPLL3_MONCLK_B",
	[AD_MEMPLL4_MONCLK_B]		= "AD_MEMPLL4_MONCLK_B",
	[AD_MEMPLL_REFCLK_B]		= "AD_MEMPLL_REFCLK_B",
	[AD_MEMPLL_FBCLK_B]		= "AD_MEMPLL_FBCLK_B",
	[AD_MEMPLL2_REFCLK_B]		= "AD_MEMPLL2_REFCLK_B",
	[AD_MEMPLL2_FBCLK_B]		= "AD_MEMPLL2_FBCLK_B",
	[AD_MEMPLL3_REFCLK_B]		= "AD_MEMPLL3_REFCLK_B",
	[AD_MEMPLL3_FBCLK_B]		= "AD_MEMPLL3_FBCLK_B",
	[AD_MEMPLL4_REFCLK_B]		= "AD_MEMPLL4_REFCLK_B",
	[AD_MEMPLL4_FBCLK_B]		= "AD_MEMPLL4_FBCLK_B",
	[AD_MEMPLL_TSTDIV2_CK_B]	= "AD_MEMPLL_TSTDIV2_CK_B",
	[AD_MEM2MIPI_26M_CK_B_]		= "AD_MEM2MIPI_26M_CK_B_",
	[AD_LVDSTX1_MONCLK]		= "AD_LVDSTX1_MONCLK",
	[AD_MONREF1_CK]			= "AD_MONREF1_CK",
	[AD_MONFBK1]			= "AD_MONFBK1",
	[AD_DSI0_LNTC_DSICLK]		= "AD_DSI0_LNTC_DSICLK",
	[AD_DSI0_MPPLL_TST_CK]		= "AD_DSI0_MPPLL_TST_CK",
	[AD_DSI1_LNTC_DSICLK]		= "AD_DSI1_LNTC_DSICLK",
	[AD_DSI1_MPPLL_TST_CK]		= "AD_DSI1_MPPLL_TST_CK",
	[AD_CR4_249P2M_CK]		= "AD_CR4_249P2M_CK",
#endif /* CLKDBG_8173 */
};

enum CKGEN_CLK {
	CKGEN_CLK_NULL,

#if CLKDBG_8173

	HF_FAXI_CK			= 1,
	HD_FAXI_CK			= 2,
	HF_FDPI0_CK			= 3,
	HF_FDDRPHYCFG_CK		= 4,
	HF_FMM_CK			= 5,
	F_FPWM_CK			= 6,
	HF_FVDEC_CK			= 7,
	HF_FVENC_CK			= 8,
	HF_FMFG_CK			= 9,
	HF_FCAMTG_CK			= 10,
	F_FUART_CK			= 11,
	HF_FSPI_CK			= 12,
	F_FUSB20_CK			= 13,
	F_FUSB30_CK			= 14,
	HF_FMSDC50_0_HCLK_CK		= 15,
	HF_FMSDC50_0_CK			= 16,
	HF_FMSDC30_1_CK			= 17,
	HF_FMSDC30_2_CK			= 18,
	HF_FMSDC30_3_CK			= 19,
	HF_FAUDIO_CK			= 20,
	HF_FAUD_INTBUS_CK		= 21,
	HF_FPMICSPI_CK			= 22,
	HF_FSCP_CK			= 23,
	HF_FATB_CK			= 24,
	HF_FVENC_LT_CK			= 25,
	HF_FIRDA_CK			= 26,
	HF_FCCI400_CK			= 27,
	HF_FAUD_1_CK			= 28,
	HF_FAUD_2_CK			= 29,
	HF_FMEM_MFG_IN_AS_CK		= 30,
	HF_FAXI_MFG_IN_AS_CK		= 31,
	F_FRTC_CK			= 32,
	F_F26M_CK			= 33,
	F_F32K_MD1_CK			= 34,
	F_FRTC_CONN_CK			= 35,
	HG_FMIPICFG_CK			= 36,
	HD_HAXI_NLI_CK			= 37,
	HD_QAXIDCM_CK			= 38,
	F_FFPC_CK			= 39,
	F_FCKBUS_CK_SCAN		= 40,
	F_FCKRTC_CK_SCAN		= 41,
	HF_FLVDS_PXL_CK			= 42,
	HF_FLVDS_CTS_CK			= 43,
	HF_FDPILVDS_CK			= 44,
	HF_FSPINFI_INFRA_BCLK_CK	= 45,
	HF_FHDMI_CK			= 46,
	HF_FHDCP_CK			= 47,
	HF_FMSDC50_3_HCLK_CK		= 48,
	HF_FHDCP_24M_CK			= 49,

#endif /* CLKDBG_8173 */

	CKGEN_CLK_END,
};

static const char * const CKGEN_CLK_NAME[] = {
#if CLKDBG_8173
	[HF_FAXI_CK]			= "hf_faxi_ck",
	[HD_FAXI_CK]			= "hd_faxi_ck",
	[HF_FDPI0_CK]			= "hf_fdpi0_ck",
	[HF_FDDRPHYCFG_CK]		= "hf_fddrphycfg_ck",
	[HF_FMM_CK]			= "hf_fmm_ck",
	[F_FPWM_CK]			= "f_fpwm_ck",
	[HF_FVDEC_CK]			= "hf_fvdec_ck",
	[HF_FVENC_CK]			= "hf_fvenc_ck",
	[HF_FMFG_CK]			= "hf_fmfg_ck",
	[HF_FCAMTG_CK]			= "hf_fcamtg_ck",
	[F_FUART_CK]			= "f_fuart_ck",
	[HF_FSPI_CK]			= "hf_fspi_ck",
	[F_FUSB20_CK]			= "f_fusb20_ck",
	[F_FUSB30_CK]			= "f_fusb30_ck",
	[HF_FMSDC50_0_HCLK_CK]		= "hf_fmsdc50_0_hclk_ck",
	[HF_FMSDC50_0_CK]		= "hf_fmsdc50_0_ck",
	[HF_FMSDC30_1_CK]		= "hf_fmsdc30_1_ck",
	[HF_FMSDC30_2_CK]		= "hf_fmsdc30_2_ck",
	[HF_FMSDC30_3_CK]		= "hf_fmsdc30_3_ck",
	[HF_FAUDIO_CK]			= "hf_faudio_ck",
	[HF_FAUD_INTBUS_CK]		= "hf_faud_intbus_ck",
	[HF_FPMICSPI_CK]		= "hf_fpmicspi_ck",
	[HF_FSCP_CK]			= "hf_fscp_ck",
	[HF_FATB_CK]			= "hf_fatb_ck",
	[HF_FVENC_LT_CK]		= "hf_fvenc_lt_ck",
	[HF_FIRDA_CK]			= "hf_firda_ck",
	[HF_FCCI400_CK]			= "hf_fcci400_ck",
	[HF_FAUD_1_CK]			= "hf_faud_1_ck",
	[HF_FAUD_2_CK]			= "hf_faud_2_ck",
	[HF_FMEM_MFG_IN_AS_CK]		= "hf_fmem_mfg_in_as_ck",
	[HF_FAXI_MFG_IN_AS_CK]		= "hf_faxi_mfg_in_as_ck",
	[F_FRTC_CK]			= "f_frtc_ck",
	[F_F26M_CK]			= "f_f26m_ck",
	[F_F32K_MD1_CK]			= "f_f32k_md1_ck",
	[F_FRTC_CONN_CK]		= "f_frtc_conn_ck",
	[HG_FMIPICFG_CK]		= "hg_fmipicfg_ck",
	[HD_HAXI_NLI_CK]		= "hd_haxi_nli_ck",
	[HD_QAXIDCM_CK]			= "hd_qaxidcm_ck",
	[F_FFPC_CK]			= "f_ffpc_ck",
	[F_FCKBUS_CK_SCAN]		= "f_fckbus_ck_scan",
	[F_FCKRTC_CK_SCAN]		= "f_fckrtc_ck_scan",
	[HF_FLVDS_PXL_CK]		= "hf_flvds_pxl_ck",
	[HF_FLVDS_CTS_CK]		= "hf_flvds_cts_ck",
	[HF_FDPILVDS_CK]		= "hf_fdpilvds_ck",
	[HF_FSPINFI_INFRA_BCLK_CK]	= "hf_fspinfi_infra_bclk_ck",
	[HF_FHDMI_CK]			= "hf_fhdmi_ck",
	[HF_FHDCP_CK]			= "hf_fhdcp_ck",
	[HF_FMSDC50_3_HCLK_CK]		= "hf_fmsdc50_3_hclk_ck",
	[HF_FHDCP_24M_CK]		= "hf_fhdcp_24m_ck",
#endif /* CLKDBG_8173 */
};

#if CLKDBG_8173

static void set_fmeter_divider_ca7(u32 k1)
{
	u32 v = clk_readl(CLK_MISC_CFG_1);

	v = ALT_BITS(v, 15, 8, k1);
	clk_writel(CLK_MISC_CFG_1, v);
}

static void set_fmeter_divider_ca15(u32 k1)
{
	u32 v = clk_readl(CLK_MISC_CFG_2);

	v = ALT_BITS(v, 7, 0, k1);
	clk_writel(CLK_MISC_CFG_2, v);
}

static void set_fmeter_divider(u32 k1)
{
	u32 v = clk_readl(CLK_MISC_CFG_1);

	v = ALT_BITS(v, 7, 0, k1);
	v = ALT_BITS(v, 31, 24, k1);
	clk_writel(CLK_MISC_CFG_1, v);
}

#endif /* CLKDBG_8173 */

#if CLKDBG_8173

static bool wait_fmeter_done(u32 tri_bit)
{
	static int max_wait_count;
	int wait_count = (max_wait_count > 0) ? (max_wait_count * 2 + 2) : 100;
	int i;

	/* wait fmeter */
	for (i = 0; i < wait_count && (clk_readl(CLK26CALI_0) & tri_bit); i++)
		udelay(20);

	if (!(clk_readl(CLK26CALI_0) & tri_bit)) {
		max_wait_count = max(max_wait_count, i);
		return true;
	}

	return false;
}

#endif /* CLKDBG_8173 */

static u32 fmeter_freq(enum FMETER_TYPE type, int k1, int clk)
{
#if CLKDBG_8173
	u32 cksw_ckgen[] = {21, 16, clk};
	u32 cksw_abist[] = {14, 8, clk};
	void __iomem *clk_cfg_reg =
				(type == CKGEN) ? CLK_CFG_9	: CLK_CFG_8;
	void __iomem *cnt_reg =	(type == CKGEN) ? CLK26CALI_2	: CLK26CALI_1;
	u32 *cksw_hlv =		(type == CKGEN) ? cksw_ckgen	: cksw_abist;
	u32 tri_bit =		(type == CKGEN) ? BIT(4)	: BIT(0);
	u32 clk_exc =		(type == CKGEN) ? BIT(5)	: BIT(2);

	u32 clk_misc_cfg_1;
	u32 clk_misc_cfg_2;
	u32 clk_cfg_val;
	u32 freq = 0;

	if (!is_valid_reg(topckgen_base))
		return 0;

	/* setup fmeter */
	clk_setl(CLK26CALI_0, BIT(7));			/* enable fmeter_en */
	clk_clrl(CLK26CALI_0, clk_exc);			/* set clk_exc */

	/* load_cnt = 1023 */
	clk_writel_mask(cnt_reg, 25, 16, 1023);

	clk_misc_cfg_1 = clk_readl(CLK_MISC_CFG_1);	/* backup reg value */
	clk_misc_cfg_2 = clk_readl(CLK_MISC_CFG_2);
	clk_cfg_val = clk_readl(clk_cfg_reg);

	set_fmeter_divider(k1);				/* set div (0 = /1) */
	set_fmeter_divider_ca7(k1);
	set_fmeter_divider_ca15(k1);
	clk_writel_mask(clk_cfg_reg, cksw_hlv[0], cksw_hlv[1], cksw_hlv[2]);

	clk_setl(CLK26CALI_0, tri_bit);			/* start fmeter */

	if (wait_fmeter_done(tri_bit)) {
		u32 cnt = clk_readl(cnt_reg) & 0xFFFF;

		/* freq = counter * 26M / 1024 (KHz) */
		freq = (cnt * 26000) * (k1 + 1) / 1024;
	}

	/* restore register settings */
	clk_writel(clk_cfg_reg, clk_cfg_val);
	clk_writel(CLK_MISC_CFG_2, clk_misc_cfg_2);
	clk_writel(CLK_MISC_CFG_1, clk_misc_cfg_1);

	return freq;
#else
	return 0;
#endif /* CLKDBG_8173 */
}

static u32 measure_stable_fmeter_freq(enum FMETER_TYPE type, int k1, int clk)
{
	u32 last_freq = 0;
	u32 freq;
	u32 maxfreq;

	freq = fmeter_freq(type, k1, clk);
	maxfreq = max(freq, last_freq);

	while (maxfreq > 0 && ABS_DIFF(freq, last_freq) * 100 / maxfreq > 10) {
		last_freq = freq;
		freq = fmeter_freq(type, k1, clk);
		maxfreq = max(freq, last_freq);
	}

	return freq;
}

static u32 measure_abist_freq(enum ABIST_CLK clk)
{
	return measure_stable_fmeter_freq(ABIST, 0, clk);
}

static u32 measure_ckgen_freq(enum CKGEN_CLK clk)
{
	return measure_stable_fmeter_freq(CKGEN, 0, clk);
}

#if CLKDBG_8173

static enum ABIST_CLK abist_clk[] = {
	AD_MEMPLL2_CKOUT0_PRE_ISO,
	AD_ARMCA7PLL_754M_CORE_CK,
	AD_ARMCA7PLL_502M_CORE_CK,
	AD_MAIN_H546M_CK,
	AD_MAIN_H364M_CK,
	AD_MAIN_H218P4M_CK,
	AD_MAIN_H156M_CK,
	AD_UNIV_178P3M_CK,
	AD_UNIV_48M_CK,
	AD_UNIV_624M_CK,
	AD_UNIV_416M_CK,
	AD_UNIV_249P6M_CK,
	AD_APLL1_180P6336M_CK,
	AD_APLL2_196P608M_CK,
	AD_LTEPLL_FS26M_CK,
	RTC32K_CK_I,
	AD_MMPLL_455M_CK,
	AD_VENCPLL_760M_CK,
	AD_VCODEPLL_494M_CK,
	AD_TVDPLL_594M_CK,
	AD_SSUSB_SYSPLL_125M_CK,
	AD_MSDCPLL_806M_CK,
	AD_DPICLK,
	CLKPH_MCK_O,
	AD_USB_48M_CK,
	AD_MSDCPLL2_CK,
	AD_VCODECPLL_370P5M_CK,
	AD_TVDPLL_445P5M_CK,
	SOC_ROSC,
	SOC_ROSC1,
	AD_HDMITX_CLKDIG_CTS_D4_DEBUG,
	AD_HDMITX_MON_CK_D4_DEBUG,
	AD_MIPI_26M_CK,
	AD_LTEPLL_ARMPLL26M_CK,
	AD_LETPLL_SSUSB26M_CK,
	AD_HDMITX_MON_CK,
	AD_MPLL_26M_26M_CK_CKSYS,
	HSIC_AD_480M_CK,
	AD_HDMITX_REF_CK_CKSYS,
	AD_HDMITX_CLKDIG_CTS_CKSYS,
	AD_PLLGP_TST_CK,
	AD_SSUSB_48M_CJ,
	AD_MEM_26M_CK,
	ARMPLL_OCC_MON,
	AD_ARMPLLGP_TST_CK,
	AD_MEMPLL_MONCLK,
	AD_MEMPLL2_MONCLK,
	AD_MEMPLL3_MONCLK,
	AD_MEMPLL4_MONCLK,
	AD_MEMPLL_REFCLK,
	AD_MEMPLL_FBCLK,
	AD_MEMPLL2_REFCLK,
	AD_MEMPLL2_FBCLK,
	AD_MEMPLL3_REFCLK,
	AD_MEMPLL3_FBCLK,
	AD_MEMPLL4_REFCLK,
	AD_MEMPLL4_FBCLK,
	AD_MEMPLL_TSTDIV2_CK,
	AD_MEM2MIPI_26M_CK_,
	AD_MEMPLL_MONCLK_B,
	AD_MEMPLL2_MONCLK_B,
	AD_MEMPLL3_MONCLK_B,
	AD_MEMPLL4_MONCLK_B,
	AD_MEMPLL_REFCLK_B,
	AD_MEMPLL_FBCLK_B,
	AD_MEMPLL2_REFCLK_B,
	AD_MEMPLL2_FBCLK_B,
	AD_MEMPLL3_REFCLK_B,
	AD_MEMPLL3_FBCLK_B,
	AD_MEMPLL4_REFCLK_B,
	AD_MEMPLL4_FBCLK_B,
	AD_MEMPLL_TSTDIV2_CK_B,
	AD_MEM2MIPI_26M_CK_B_,
	AD_LVDSTX1_MONCLK,
	AD_MONREF1_CK,
	AD_MONFBK1,
	AD_DSI0_LNTC_DSICLK,
	AD_DSI0_MPPLL_TST_CK,
	AD_DSI1_LNTC_DSICLK,
	AD_DSI1_MPPLL_TST_CK,
	AD_CR4_249P2M_CK,
};

static enum CKGEN_CLK ckgen_clk[] = {
	HF_FAXI_CK,
	HD_FAXI_CK,
	HF_FDPI0_CK,
	HF_FDDRPHYCFG_CK,
	HF_FMM_CK,
	F_FPWM_CK,
	HF_FVDEC_CK,
	HF_FVENC_CK,
	HF_FMFG_CK,
	HF_FCAMTG_CK,
	F_FUART_CK,
	HF_FSPI_CK,
	F_FUSB20_CK,
	F_FUSB30_CK,
	HF_FMSDC50_0_HCLK_CK,
	HF_FMSDC50_0_CK,
	HF_FMSDC30_1_CK,
	HF_FMSDC30_2_CK,
	HF_FMSDC30_3_CK,
	HF_FAUDIO_CK,
	HF_FAUD_INTBUS_CK,
	HF_FPMICSPI_CK,
	HF_FSCP_CK,
	HF_FATB_CK,
	HF_FVENC_LT_CK,
	HF_FIRDA_CK,
	HF_FCCI400_CK,
	HF_FAUD_1_CK,
	HF_FAUD_2_CK,
	HF_FMEM_MFG_IN_AS_CK,
	HF_FAXI_MFG_IN_AS_CK,
	F_FRTC_CK,
	F_F26M_CK,
	F_F32K_MD1_CK,
	F_FRTC_CONN_CK,
	HG_FMIPICFG_CK,
	HD_HAXI_NLI_CK,
	HD_QAXIDCM_CK,
	F_FFPC_CK,
	F_FCKBUS_CK_SCAN,
	F_FCKRTC_CK_SCAN,
	HF_FLVDS_PXL_CK,
	HF_FLVDS_CTS_CK,
	HF_FDPILVDS_CK,
	HF_FSPINFI_INFRA_BCLK_CK,
	HF_FHDMI_CK,
	HF_FHDCP_CK,
	HF_FMSDC50_3_HCLK_CK,
	HF_FHDCP_24M_CK,
};

static u32 measure_armpll_freq(u32 jit_ctrl)
{
	u32 freq;
	u32 mcu26c;
	u32 armpll_jit_ctrl;
	u32 top_dcmctl;

	if (!is_valid_reg(mcucfg_base) || !is_valid_reg(infrasys_base))
		return 0;

	mcu26c = clk_readl(MCU_26C);
	armpll_jit_ctrl = clk_readl(ARMPLL_JIT_CTRL);
	top_dcmctl = clk_readl(INFRA_TOPCKGEN_DCMCTL);

	clk_setl(MCU_26C, 0x8);
	clk_setl(ARMPLL_JIT_CTRL, jit_ctrl);
	clk_clrl(INFRA_TOPCKGEN_DCMCTL, 0x700);

	freq = measure_stable_fmeter_freq(ABIST, 1, ARMPLL_OCC_MON);

	clk_writel(INFRA_TOPCKGEN_DCMCTL, top_dcmctl);
	clk_writel(ARMPLL_JIT_CTRL, armpll_jit_ctrl);
	clk_writel(MCU_26C, mcu26c);

	return freq;
}

static u32 measure_ca53_freq(void)
{
	return measure_armpll_freq(0x01);
}

static u32 measure_ca57_freq(void)
{
	return measure_armpll_freq(0x11);
}

#endif /* CLKDBG_8173 */

#if DUMP_INIT_STATE

static void print_abist_clock(enum ABIST_CLK clk)
{
	u32 freq = measure_abist_freq(clk);

	clk_info("%2d: %-29s: %u\n", clk, ABIST_CLK_NAME[clk], freq);
}

static void print_ckgen_clock(enum CKGEN_CLK clk)
{
	u32 freq = measure_ckgen_freq(clk);

	clk_info("%2d: %-29s: %u\n", clk, CKGEN_CLK_NAME[clk], freq);
}

static void print_fmeter_all(void)
{
#if CLKDBG_8173
	size_t i;
	u32 old_pll_hp_con0;
	u32 freq;

	if (!is_valid_reg(apmixedsys_base))
		return;

	old_pll_hp_con0 = clk_readl(PLL_HP_CON0);

	clk_writel(PLL_HP_CON0, 0x0);		/* disable PLL hopping */
	udelay(10);

	freq = measure_ca53_freq();
	clk_info("%2d: %-29s: %u\n", 0, "CA53", freq);
	freq = measure_ca57_freq();
	clk_info("%2d: %-29s: %u\n", 0, "CA57", freq);

	for (i = 0; i < ARRAY_SIZE(abist_clk); i++)
		print_abist_clock(abist_clk[i]);

	for (i = 0; i < ARRAY_SIZE(ckgen_clk); i++)
		print_ckgen_clock(ckgen_clk[i]);

	/* restore old setting */
	clk_writel(PLL_HP_CON0, old_pll_hp_con0);
#endif /* CLKDBG_8173 */
}

#endif /* DUMP_INIT_STATE */

static void seq_print_abist_clock(enum ABIST_CLK clk,
		struct seq_file *s, void *v)
{
	u32 freq = measure_abist_freq(clk);

	seq_printf(s, "%2d: %-29s: %u\n", clk, ABIST_CLK_NAME[clk], freq);
}

static void seq_print_ckgen_clock(enum CKGEN_CLK clk,
		struct seq_file *s, void *v)
{
	u32 freq = measure_ckgen_freq(clk);

	seq_printf(s, "%2d: %-29s: %u\n", clk, CKGEN_CLK_NAME[clk], freq);
}

static int seq_print_fmeter_all(struct seq_file *s, void *v)
{
#if CLKDBG_8173

	size_t i;
	u32 old_pll_hp_con0;
	u32 freq;

	if (!is_valid_reg(apmixedsys_base))
		return 0;

	old_pll_hp_con0 = clk_readl(PLL_HP_CON0);

	clk_writel(PLL_HP_CON0, 0x0);		/* disable PLL hopping */
	udelay(10);

	freq = measure_ca53_freq();
	seq_printf(s, "%2d: %-29s: %u\n", 0, "CA53", freq);
	freq = measure_ca57_freq();
	seq_printf(s, "%2d: %-29s: %u\n", 0, "CA57", freq);

	for (i = 0; i < ARRAY_SIZE(abist_clk); i++)
		seq_print_abist_clock(abist_clk[i], s, v);

	for (i = 0; i < ARRAY_SIZE(ckgen_clk); i++)
		seq_print_ckgen_clock(ckgen_clk[i], s, v);

	/* restore old setting */
	clk_writel(PLL_HP_CON0, old_pll_hp_con0);

#endif /* CLKDBG_8173 */

	return 0;
}

struct regname {
	void __iomem *reg;
	const char *name;
};

static size_t get_regnames(struct regname *regnames, size_t size)
{
	struct regname rn[] = {
#if CLKDBG_8173
		{SPM_VDE_PWR_CON, "SPM_VDE_PWR_CON"},
		{SPM_MFG_PWR_CON, "SPM_MFG_PWR_CON"},
		{SPM_VEN_PWR_CON, "SPM_VEN_PWR_CON"},
		{SPM_ISP_PWR_CON, "SPM_ISP_PWR_CON"},
		{SPM_DIS_PWR_CON, "SPM_DIS_PWR_CON"},
		{SPM_VEN2_PWR_CON, "SPM_VEN2_PWR_CON"},
		{SPM_AUDIO_PWR_CON, "SPM_AUDIO_PWR_CON"},
		{SPM_MFG_2D_PWR_CON, "SPM_MFG_2D_PWR_CON"},
		{SPM_MFG_ASYNC_PWR_CON, "SPM_MFG_ASYNC_PWR_CON"},
		{SPM_USB_PWR_CON, "SPM_USB_PWR_CON"},
		{SPM_PWR_STATUS, "SPM_PWR_STATUS"},
		{SPM_PWR_STATUS_2ND, "SPM_PWR_STATUS_2ND"},
		{CLK_CFG_0, "CLK_CFG_0"},
		{CLK_CFG_1, "CLK_CFG_1"},
		{CLK_CFG_2, "CLK_CFG_2"},
		{CLK_CFG_3, "CLK_CFG_3"},
		{CLK_CFG_4, "CLK_CFG_4"},
		{CLK_CFG_5, "CLK_CFG_5"},
		{CLK_CFG_6, "CLK_CFG_6"},
		{CLK_CFG_7, "CLK_CFG_7"},
		{CLK_CFG_8, "CLK_CFG_8"},
		{CLK_CFG_9, "CLK_CFG_9"},
		{CLK_CFG_10, "CLK_CFG_10"},
		{CLK_CFG_11, "CLK_CFG_11"},
		{CLK_CFG_12, "CLK_CFG_12"},
		{CLK_CFG_13, "CLK_CFG_13"},
		{CLK_MISC_CFG_1, "CLK_MISC_CFG_1"},
		{CLK_MISC_CFG_2, "CLK_MISC_CFG_2"},
		{CLK26CALI_0, "CLK26CALI_0"},
		{CLK26CALI_1, "CLK26CALI_1"},
		{CLK26CALI_2, "CLK26CALI_2"},
		{PLL_HP_CON0, "PLL_HP_CON0"},
		{ARMCA15PLL_CON0, "ARMCA15PLL_CON0"},
		{ARMCA15PLL_CON1, "ARMCA15PLL_CON1"},
		{ARMCA15PLL_PWR_CON0, "ARMCA15PLL_PWR_CON0"},
		{ARMCA7PLL_CON0, "ARMCA7PLL_CON0"},
		{ARMCA7PLL_CON1, "ARMCA7PLL_CON1"},
		{ARMCA7PLL_PWR_CON0, "ARMCA7PLL_PWR_CON0"},
		{MAINPLL_CON0, "MAINPLL_CON0"},
		{MAINPLL_CON1, "MAINPLL_CON1"},
		{MAINPLL_PWR_CON0, "MAINPLL_PWR_CON0"},
		{UNIVPLL_CON0, "UNIVPLL_CON0"},
		{UNIVPLL_CON1, "UNIVPLL_CON1"},
		{UNIVPLL_PWR_CON0, "UNIVPLL_PWR_CON0"},
		{MMPLL_CON0, "MMPLL_CON0"},
		{MMPLL_CON1, "MMPLL_CON1"},
		{MMPLL_PWR_CON0, "MMPLL_PWR_CON0"},
		{MSDCPLL_CON0, "MSDCPLL_CON0"},
		{MSDCPLL_CON1, "MSDCPLL_CON1"},
		{MSDCPLL_PWR_CON0, "MSDCPLL_PWR_CON0"},
		{VENCPLL_CON0, "VENCPLL_CON0"},
		{VENCPLL_CON1, "VENCPLL_CON1"},
		{VENCPLL_PWR_CON0, "VENCPLL_PWR_CON0"},
		{TVDPLL_CON0, "TVDPLL_CON0"},
		{TVDPLL_CON1, "TVDPLL_CON1"},
		{TVDPLL_PWR_CON0, "TVDPLL_PWR_CON0"},
		{MPLL_CON0, "MPLL_CON0"},
		{MPLL_CON1, "MPLL_CON1"},
		{MPLL_PWR_CON0, "MPLL_PWR_CON0"},
		{VCODECPLL_CON0, "VCODECPLL_CON0"},
		{VCODECPLL_CON1, "VCODECPLL_CON1"},
		{VCODECPLL_PWR_CON0, "VCODECPLL_PWR_CON0"},
		{APLL1_CON0, "APLL1_CON0"},
		{APLL1_CON1, "APLL1_CON1"},
		{APLL1_PWR_CON0, "APLL1_PWR_CON0"},
		{APLL2_CON0, "APLL2_CON0"},
		{APLL2_CON1, "APLL2_CON1"},
		{APLL2_PWR_CON0, "APLL2_PWR_CON0"},
		{LVDSPLL_CON0, "LVDSPLL_CON0"},
		{LVDSPLL_CON1, "LVDSPLL_CON1"},
		{LVDSPLL_PWR_CON0, "LVDSPLL_PWR_CON0"},
		{MSDCPLL2_CON0, "MSDCPLL2_CON0"},
		{MSDCPLL2_CON1, "MSDCPLL2_CON1"},
		{MSDCPLL2_PWR_CON0, "MSDCPLL2_PWR_CON0"},
		{INFRA_PDN_STA, "INFRA_PDN_STA"},
		{PERI_PDN0_STA, "PERI_PDN0_STA"},
		{PERI_PDN1_STA, "PERI_PDN1_STA"},
		{DISP_CG_CON0, "DISP_CG_CON0"},
		{DISP_CG_CON1, "DISP_CG_CON1"},
		{IMG_CG_CON, "IMG_CG_CON"},
		{VDEC_CKEN_SET, "VDEC_CKEN_SET"},
		{LARB_CKEN_SET, "LARB_CKEN_SET"},
		{VENC_CG_CON, "VENC_CG_CON"},
		{VENCLT_CG_CON, "VENCLT_CG_CON"},
		{AUDIO_TOP_CON0, "AUDIO_TOP_CON0"},
		{MFG_CG_CON, "MFG_CG_CON"},
#endif /* CLKDBG_SOC */
	};

	size_t n = min(ARRAY_SIZE(rn), size);

	memcpy(regnames, rn, sizeof(rn[0]) * n);
	return n;
}

#if DUMP_INIT_STATE

static void print_reg(void __iomem *reg, const char *name)
{
	if (!is_valid_reg(reg))
		return;

	clk_info("%-21s: [0x%p] = 0x%08x\n", name, reg, clk_readl(reg));
}

static void print_regs(void)
{
	static struct regname rn[128];
	int i, n;

	n = get_regnames(rn, ARRAY_SIZE(rn));

	for (i = 0; i < n; i++)
		print_reg(rn[i].reg, rn[i].name);
}

#endif /* DUMP_INIT_STATE */

static void seq_print_reg(void __iomem *reg, const char *name,
		struct seq_file *s, void *v)
{
	u32 val;

	if (!is_valid_reg(reg))
		return;

	val = clk_readl(reg);

	clk_info("%-21s: [0x%p] = 0x%08x\n", name, reg, val);
	seq_printf(s, "%-21s: [0x%p] = 0x%08x\n", name, reg, val);
}

static int seq_print_regs(struct seq_file *s, void *v)
{
	static struct regname rn[128];
	int i, n;

	n = get_regnames(rn, ARRAY_SIZE(rn));

	for (i = 0; i < n; i++)
		seq_print_reg(rn[i].reg, rn[i].name, s, v);

	return 0;
}

#if CLKDBG_8173

/* ROOT */
#define clk_null		"clk_null"
#define clk26m			"clk26m"
#define clk32k			"clk32k"

#define clkph_mck_o		"clkph_mck_o"
#define dpi_ck			"dpi_ck"
#define usb_syspll_125m		"usb_syspll_125m"
#define hdmitx_dig_cts		"hdmitx_dig_cts"

/* PLL */
#define armca15pll		"armca15pll"
#define armca7pll		"armca7pll"
#define mainpll			"mainpll"
#define univpll			"univpll"
#define mmpll			"mmpll"
#define msdcpll			"msdcpll"
#define vencpll			"vencpll"
#define tvdpll			"tvdpll"
#define mpll			"mpll"
#define vcodecpll		"vcodecpll"
#define apll1			"apll1"
#define apll2			"apll2"
#define lvdspll			"lvdspll"
#define msdcpll2		"msdcpll2"
#define ref2usb_tx		"ref2usb_tx"

#define armca7pll_754m		"armca7pll_754m"
#define armca7pll_502m		"armca7pll_502m"
#define apll1_180p633m		apll1
#define apll2_196p608m		apll2
#define mmpll_455m		mmpll
#define msdcpll_806m		msdcpll
#define main_h546m		"main_h546m"
#define main_h364m		"main_h364m"
#define main_h218p4m		"main_h218p4m"
#define main_h156m		"main_h156m"
#define tvdpll_445p5m		"tvdpll_445p5m"
#define tvdpll_594m		"tvdpll_594m"
#define univ_624m		"univ_624m"
#define univ_416m		"univ_416m"
#define univ_249p6m		"univ_249p6m"
#define univ_178p3m		"univ_178p3m"
#define univ_48m		"univ_48m"
#define vcodecpll_370p5		"vcodecpll_370p5"
#define vcodecpll_494m		vcodecpll
#define vencpll_380m		vencpll
#define lvdspll_ck		lvdspll

/* DIV */
#define clkrtc_ext		"clkrtc_ext"
#define clkrtc_int		"clkrtc_int"
#define fpc_ck			"fpc_ck"
#define hdmitxpll_d2		"hdmitxpll_d2"
#define hdmitxpll_d3		"hdmitxpll_d3"
#define armca7pll_d2		"armca7pll_d2"
#define armca7pll_d3		"armca7pll_d3"
#define apll1_ck		"apll1_ck"
#define apll2_ck		"apll2_ck"
#define dmpll_ck		"dmpll_ck"
#define dmpll_d2		"dmpll_d2"
#define dmpll_d4		"dmpll_d4"
#define dmpll_d8		"dmpll_d8"
#define dmpll_d16		"dmpll_d16"
#define lvdspll_d2		"lvdspll_d2"
#define lvdspll_d4		"lvdspll_d4"
#define lvdspll_d8		"lvdspll_d8"
#define mmpll_ck		"mmpll_ck"
#define mmpll_d2		"mmpll_d2"
#define msdcpll_ck		"msdcpll_ck"
#define msdcpll_d2		"msdcpll_d2"
#define msdcpll_d4		"msdcpll_d4"
#define msdcpll2_ck		"msdcpll2_ck"
#define msdcpll2_d2		"msdcpll2_d2"
#define msdcpll2_d4		"msdcpll2_d4"
#define ssusb_phyd_125m_ck	usb_syspll_125m
#define syspll_d2		"syspll_d2"
#define syspll1_d2		"syspll1_d2"
#define syspll1_d4		"syspll1_d4"
#define syspll1_d8		"syspll1_d8"
#define syspll1_d16		"syspll1_d16"
#define syspll_d3		"syspll_d3"
#define syspll2_d2		"syspll2_d2"
#define syspll2_d4		"syspll2_d4"
#define syspll_d5		"syspll_d5"
#define syspll3_d2		"syspll3_d2"
#define syspll3_d4		"syspll3_d4"
#define syspll_d7		"syspll_d7"
#define syspll4_d2		"syspll4_d2"
#define syspll4_d4		"syspll4_d4"
#define tvdpll_445p5m_ck	tvdpll_445p5m
#define tvdpll_ck		"tvdpll_ck"
#define tvdpll_d2		"tvdpll_d2"
#define tvdpll_d4		"tvdpll_d4"
#define tvdpll_d8		"tvdpll_d8"
#define tvdpll_d16		"tvdpll_d16"
#define univpll_d2		"univpll_d2"
#define univpll1_d2		"univpll1_d2"
#define univpll1_d4		"univpll1_d4"
#define univpll1_d8		"univpll1_d8"
#define univpll_d3		"univpll_d3"
#define univpll2_d2		"univpll2_d2"
#define univpll2_d4		"univpll2_d4"
#define univpll2_d8		"univpll2_d8"
#define univpll_d5		"univpll_d5"
#define univpll3_d2		"univpll3_d2"
#define univpll3_d4		"univpll3_d4"
#define univpll3_d8		"univpll3_d8"
#define univpll_d7		"univpll_d7"
#define univpll_d26		"univpll_d26"
#define univpll_d52		"univpll_d52"
#define vcodecpll_ck		"vcodecpll_ck"
#define vencpll_ck		"vencpll_ck"
#define vencpll_d2		"vencpll_d2"
#define vencpll_d4		"vencpll_d4"

/* TOP */
#define axi_sel			"axi_sel"
#define mem_sel			"mem_sel"
#define ddrphycfg_sel		"ddrphycfg_sel"
#define mm_sel			"mm_sel"
#define pwm_sel			"pwm_sel"
#define vdec_sel		"vdec_sel"
#define venc_sel		"venc_sel"
#define mfg_sel			"mfg_sel"
#define camtg_sel		"camtg_sel"
#define uart_sel		"uart_sel"
#define spi_sel			"spi_sel"
#define usb20_sel		"usb20_sel"
#define usb30_sel		"usb30_sel"
#define msdc50_0_h_sel		"msdc50_0_h_sel"
#define msdc50_0_sel		"msdc50_0_sel"
#define msdc30_1_sel		"msdc30_1_sel"
#define msdc30_2_sel		"msdc30_2_sel"
#define msdc30_3_sel		"msdc30_3_sel"
#define audio_sel		"audio_sel"
#define aud_intbus_sel		"aud_intbus_sel"
#define pmicspi_sel		"pmicspi_sel"
#define scp_sel			"scp_sel"
#define atb_sel			"atb_sel"
#define venclt_sel		"venclt_sel"
#define dpi0_sel		"dpi0_sel"
#define irda_sel		"irda_sel"
#define cci400_sel		"cci400_sel"
#define aud_1_sel		"aud_1_sel"
#define aud_2_sel		"aud_2_sel"
#define mem_mfg_in_sel		"mem_mfg_in_sel"
#define axi_mfg_in_sel		"axi_mfg_in_sel"
#define scam_sel		"scam_sel"
#define spinfi_ifr_sel		"spinfi_ifr_sel"
#define hdmi_sel		"hdmi_sel"
#define dpilvds_sel		"dpilvds_sel"
#define msdc50_2_h_sel		"msdc50_2_h_sel"
#define hdcp_sel		"hdcp_sel"
#define hdcp_24m_sel		"hdcp_24m_sel"
#define rtc_sel			"rtc_sel"

#define axi_ck			axi_sel
#define mfg_ck			mfg_sel

/* INFRA */
#define infra_pmicwrap		"infra_pmicwrap"
#define infra_pmicspi		"infra_pmicspi"
#define infra_cec		"infra_cec"
#define infra_kp		"infra_kp"
#define infra_cpum		"infra_cpum"
#define infra_m4u		"infra_m4u"
#define infra_l2c_sram		"infra_l2c_sram"
#define infra_gce		"infra_gce"
#define infra_audio		"infra_audio"
#define infra_smi		"infra_smi"
#define infra_dbgclk		"infra_dbgclk"

/* PERI0 */
#define peri_nfiecc		"peri_nfiecc"
#define peri_i2c5		"peri_i2c5"
#define peri_spi0		"peri_spi0"
#define peri_auxadc		"peri_auxadc"
#define peri_i2c4		"peri_i2c4"
#define peri_i2c3		"peri_i2c3"
#define peri_i2c2		"peri_i2c2"
#define peri_i2c1		"peri_i2c1"
#define peri_i2c0		"peri_i2c0"
#define peri_uart3		"peri_uart3"
#define peri_uart2		"peri_uart2"
#define peri_uart1		"peri_uart1"
#define peri_uart0		"peri_uart0"
#define peri_irda		"peri_irda"
#define peri_nli_arb		"peri_nli_arb"
#define peri_msdc30_3		"peri_msdc30_3"
#define peri_msdc30_2		"peri_msdc30_2"
#define peri_msdc30_1		"peri_msdc30_1"
#define peri_msdc30_0		"peri_msdc30_0"
#define peri_ap_dma		"peri_ap_dma"
#define peri_usb1		"peri_usb1"
#define peri_usb0		"peri_usb0"
#define peri_pwm		"peri_pwm"
#define peri_pwm7		"peri_pwm7"
#define peri_pwm6		"peri_pwm6"
#define peri_pwm5		"peri_pwm5"
#define peri_pwm4		"peri_pwm4"
#define peri_pwm3		"peri_pwm3"
#define peri_pwm2		"peri_pwm2"
#define peri_pwm1		"peri_pwm1"
#define peri_therm		"peri_therm"
#define peri_nfi		"peri_nfi"

/* PERI1 */
#define peri_i2c6		"peri_i2c6"
#define peri_irrx		"peri_irrx"
#define peri_spi		"peri_spi"

/* MFG */
#define mfg_axi			"mfg_axi"
#define mfg_mem			"mfg_mem"
#define mfg_g3d			"mfg_g3d"
#define mfg_26m			"mfg_26m"

/* IMG */
#define img_fd			"img_fd"
#define img_cam_sv		"img_cam_sv"
#define img_sen_cam		"img_sen_cam"
#define img_sen_tg		"img_sen_tg"
#define img_cam_cam		"img_cam_cam"
#define img_cam_smi		"img_cam_smi"
#define img_larb2_smi		"img_larb2_smi"

/* MM0 */
#define mm_smi_common		"mm_smi_common"
#define mm_smi_larb0		"mm_smi_larb0"
#define mm_cam_mdp		"mm_cam_mdp"
#define mm_mdp_rdma0		"mm_mdp_rdma0"
#define mm_mdp_rdma1		"mm_mdp_rdma1"
#define mm_mdp_rsz0		"mm_mdp_rsz0"
#define mm_mdp_rsz1		"mm_mdp_rsz1"
#define mm_mdp_rsz2		"mm_mdp_rsz2"
#define mm_mdp_tdshp0		"mm_mdp_tdshp0"
#define mm_mdp_tdshp1		"mm_mdp_tdshp1"
#define mm_mdp_wdma		"mm_mdp_wdma"
#define mm_mdp_wrot0		"mm_mdp_wrot0"
#define mm_mdp_wrot1		"mm_mdp_wrot1"
#define mm_fake_eng		"mm_fake_eng"
#define mm_mutex_32k		"mm_mutex_32k"
#define mm_disp_ovl0		"mm_disp_ovl0"
#define mm_disp_ovl1		"mm_disp_ovl1"
#define mm_disp_rdma0		"mm_disp_rdma0"
#define mm_disp_rdma1		"mm_disp_rdma1"
#define mm_disp_rdma2		"mm_disp_rdma2"
#define mm_disp_wdma0		"mm_disp_wdma0"
#define mm_disp_wdma1		"mm_disp_wdma1"
#define mm_disp_color0		"mm_disp_color0"
#define mm_disp_color1		"mm_disp_color1"
#define mm_disp_aal		"mm_disp_aal"
#define mm_disp_gamma		"mm_disp_gamma"
#define mm_disp_ufoe		"mm_disp_ufoe"
#define mm_disp_split0		"mm_disp_split0"
#define mm_disp_split1		"mm_disp_split1"
#define mm_disp_merge		"mm_disp_merge"
#define mm_disp_od		"mm_disp_od"

/* MM1 */
#define mm_disp_pwm0mm		"mm_disp_pwm0mm"
#define mm_disp_pwm026m		"mm_disp_pwm026m"
#define mm_disp_pwm1mm		"mm_disp_pwm1mm"
#define mm_disp_pwm126m		"mm_disp_pwm126m"
#define mm_dsi0_engine		"mm_dsi0_engine"
#define mm_dsi0_digital		"mm_dsi0_digital"
#define mm_dsi1_engine		"mm_dsi1_engine"
#define mm_dsi1_digital		"mm_dsi1_digital"
#define mm_dpi_pixel		"mm_dpi_pixel"
#define mm_dpi_engine		"mm_dpi_engine"
#define mm_dpi1_pixel		"mm_dpi1_pixel"
#define mm_dpi1_engine		"mm_dpi1_engine"
#define mm_hdmi_pixel		"mm_hdmi_pixel"
#define mm_hdmi_pllck		"mm_hdmi_pllck"
#define mm_hdmi_audio		"mm_hdmi_audio"
#define mm_hdmi_spdif		"mm_hdmi_spdif"
#define mm_lvds_pixel		"mm_lvds_pixel"
#define mm_lvds_cts		"mm_lvds_cts"
#define mm_smi_larb4		"mm_smi_larb4"
#define mm_hdmi_hdcp		"mm_hdmi_hdcp"
#define mm_hdmi_hdcp24m		"mm_hdmi_hdcp24m"

/* VDEC */
#define vdec_cken		"vdec_cken"
#define vdec_larb_cken		"vdec_larb_cken"

/* VENC */
#define venc_cke0		"venc_cke0"
#define venc_cke1		"venc_cke1"
#define venc_cke2		"venc_cke2"
#define venc_cke3		"venc_cke3"

/* VENCLT */
#define venclt_cke0		"venclt_cke0"
#define venclt_cke1		"venclt_cke1"

/* AUDIO */
#define aud_ahb_idle_in		"aud_ahb_idle_in"
#define aud_ahb_idle_ex		"aud_ahb_idle_ex"
#define aud_tml			"aud_tml"
#define aud_dac_predis		"aud_dac_predis"
#define aud_dac			"aud_dac"
#define aud_adc			"aud_adc"
#define aud_adda2		"aud_adda2"
#define aud_adda3		"aud_adda3"
#define aud_spdf		"aud_spdf"
#define aud_hdmi		"aud_hdmi"
#define aud_apll_tnr		"aud_apll_tnr"
#define aud_apll2_tnr		"aud_apll2_tnr"
#define aud_spdf2		"aud_spdf2"
#define aud_24m			"aud_24m"
#define aud_22m			"aud_22m"
#define aud_i2s			"aud_i2s"
#define aud_afe			"aud_afe"

/* SCP */
#define pg_vde			"pg_vde"
#define pg_mfg			"pg_mfg"
#define pg_ven			"pg_ven"
#define pg_isp			"pg_isp"
#define pg_dis			"pg_dis"
#define pg_ven2			"pg_ven2"
#define pg_audio		"pg_audio"
#define pg_mfg_2d		"pg_mfg_2d"
#define pg_mfg_async		"pg_mfg_async"
#define pg_usb			"pg_usb"

#endif /* CLKDBG_8173 */

static const char * const *get_all_clk_names(size_t *num)
{
	static const char * const clks[] = {
#if CLKDBG_8173
		/* ROOT */
		clk_null,
		clk26m,
		clk32k,

		clkph_mck_o,
#if CLKDBG_8173_TK
		dpi_ck,
#endif
		usb_syspll_125m,
		hdmitx_dig_cts,

		/* PLL */
		armca15pll,
		armca7pll,
		mainpll,
		univpll,
		mmpll,
		msdcpll,
		vencpll,
		tvdpll,
		mpll,
		vcodecpll,
		apll1,
		apll2,
		lvdspll,
		msdcpll2,
		ref2usb_tx,

		armca7pll_754m,
		armca7pll_502m,
		main_h546m,
		main_h364m,
		main_h218p4m,
		main_h156m,
		tvdpll_445p5m,
		tvdpll_594m,
		univ_624m,
		univ_416m,
		univ_249p6m,
		univ_178p3m,
		univ_48m,
		vcodecpll_370p5,

		/* DIV */
		clkrtc_ext,
		clkrtc_int,
		fpc_ck,
		hdmitxpll_d2,
		hdmitxpll_d3,
		armca7pll_d2,
		armca7pll_d3,
		apll1_ck,
		apll2_ck,
		dmpll_ck,
		dmpll_d2,
		dmpll_d4,
		dmpll_d8,
		dmpll_d16,
		lvdspll_d2,
		lvdspll_d4,
		lvdspll_d8,
		mmpll_ck,
		mmpll_d2,
		msdcpll_ck,
		msdcpll_d2,
		msdcpll_d4,
		msdcpll2_ck,
		msdcpll2_d2,
		msdcpll2_d4,
		syspll_d2,
		syspll1_d2,
		syspll1_d4,
		syspll1_d8,
		syspll1_d16,
		syspll_d3,
		syspll2_d2,
		syspll2_d4,
		syspll_d5,
		syspll3_d2,
		syspll3_d4,
		syspll_d7,
		syspll4_d2,
		syspll4_d4,
		tvdpll_ck,
		tvdpll_d2,
		tvdpll_d4,
		tvdpll_d8,
		tvdpll_d16,
		univpll_d2,
		univpll1_d2,
		univpll1_d4,
		univpll1_d8,
		univpll_d3,
		univpll2_d2,
		univpll2_d4,
		univpll2_d8,
		univpll_d5,
		univpll3_d2,
		univpll3_d4,
		univpll3_d8,
		univpll_d7,
		univpll_d26,
		univpll_d52,
		vcodecpll_ck,
		vencpll_ck,
		vencpll_d2,
		vencpll_d4,

		/* TOP */
		axi_sel,
		mem_sel,
		ddrphycfg_sel,
		mm_sel,
		pwm_sel,
		vdec_sel,
		venc_sel,
		mfg_sel,
		camtg_sel,
		uart_sel,
		spi_sel,
		usb20_sel,
		usb30_sel,
		msdc50_0_h_sel,
		msdc50_0_sel,
		msdc30_1_sel,
		msdc30_2_sel,
		msdc30_3_sel,
		audio_sel,
		aud_intbus_sel,
		pmicspi_sel,
		scp_sel,
		atb_sel,
		venclt_sel,
		dpi0_sel,
		irda_sel,
		cci400_sel,
		aud_1_sel,
		aud_2_sel,
		mem_mfg_in_sel,
		axi_mfg_in_sel,
		scam_sel,
		spinfi_ifr_sel,
		hdmi_sel,
		dpilvds_sel,
		msdc50_2_h_sel,
		hdcp_sel,
		hdcp_24m_sel,
		rtc_sel,

		/* INFRA */
		infra_pmicwrap,
		infra_pmicspi,
		infra_cec,
		infra_kp,
		infra_cpum,
		infra_m4u,
		infra_l2c_sram,
		infra_gce,
		infra_audio,
		infra_smi,
		infra_dbgclk,

		/* PERI0 */
		peri_nfiecc,
		peri_i2c5,
		peri_spi0,
		peri_auxadc,
		peri_i2c4,
		peri_i2c3,
		peri_i2c2,
		peri_i2c1,
		peri_i2c0,
		peri_uart3,
		peri_uart2,
		peri_uart1,
		peri_uart0,
		peri_irda,
		peri_nli_arb,
		peri_msdc30_3,
		peri_msdc30_2,
		peri_msdc30_1,
		peri_msdc30_0,
		peri_ap_dma,
		peri_usb1,
		peri_usb0,
		peri_pwm,
		peri_pwm7,
		peri_pwm6,
		peri_pwm5,
		peri_pwm4,
		peri_pwm3,
		peri_pwm2,
		peri_pwm1,
		peri_therm,
		peri_nfi,

		/* PERI1 */
		peri_i2c6,
		peri_irrx,
		peri_spi,

#if CLKDBG_8173_TK
		/* MFG */
		mfg_axi,
		mfg_mem,
		mfg_g3d,
		mfg_26m,
#endif /* CLKDBG_8173_TK */

		/* IMG */
		img_fd,
		img_cam_sv,
		img_sen_cam,
		img_sen_tg,
		img_cam_cam,
		img_cam_smi,
		img_larb2_smi,

		/* MM0 */
		mm_smi_common,
		mm_smi_larb0,
		mm_cam_mdp,
		mm_mdp_rdma0,
		mm_mdp_rdma1,
		mm_mdp_rsz0,
		mm_mdp_rsz1,
		mm_mdp_rsz2,
		mm_mdp_tdshp0,
		mm_mdp_tdshp1,
		mm_mdp_wdma,
		mm_mdp_wrot0,
		mm_mdp_wrot1,
		mm_fake_eng,
		mm_mutex_32k,
		mm_disp_ovl0,
		mm_disp_ovl1,
		mm_disp_rdma0,
		mm_disp_rdma1,
		mm_disp_rdma2,
		mm_disp_wdma0,
		mm_disp_wdma1,
		mm_disp_color0,
		mm_disp_color1,
		mm_disp_aal,
		mm_disp_gamma,
		mm_disp_ufoe,
		mm_disp_split0,
		mm_disp_split1,
		mm_disp_merge,
		mm_disp_od,

		/* MM1 */
		mm_disp_pwm0mm,
		mm_disp_pwm026m,
		mm_disp_pwm1mm,
		mm_disp_pwm126m,
		mm_dsi0_engine,
		mm_dsi0_digital,
		mm_dsi1_engine,
		mm_dsi1_digital,
		mm_dpi_pixel,
		mm_dpi_engine,
		mm_dpi1_pixel,
		mm_dpi1_engine,
		mm_hdmi_pixel,
		mm_hdmi_pllck,
		mm_hdmi_audio,
		mm_hdmi_spdif,
		mm_lvds_pixel,
		mm_lvds_cts,
		mm_smi_larb4,
		mm_hdmi_hdcp,
		mm_hdmi_hdcp24m,

		/* VDEC */
		vdec_cken,
		vdec_larb_cken,

		/* VENC */
		venc_cke0,
		venc_cke1,
		venc_cke2,
		venc_cke3,

		/* VENCLT */
		venclt_cke0,
		venclt_cke1,

#if CLKDBG_8173_TK
		/* AUDIO */
		aud_ahb_idle_in,
		aud_ahb_idle_ex,
		aud_tml,
		aud_dac_predis,
		aud_dac,
		aud_adc,
		aud_adda2,
		aud_adda3,
		aud_spdf,
		aud_hdmi,
		aud_apll_tnr,
		aud_apll2_tnr,
		aud_spdf2,
		aud_24m,
		aud_22m,
		aud_i2s,
		aud_afe,

		pg_vde,
		pg_mfg,
		pg_ven,
		pg_isp,
		pg_dis,
		pg_ven2,
		pg_audio,
		pg_mfg_2d,
		pg_mfg_async,
		pg_usb,
#endif /* CLKDBG_8173_TK */

#endif /* CLKDBG_SOC */
	};

	*num = ARRAY_SIZE(clks);

	return clks;
}

static void dump_clk_state(const char *clkname, struct seq_file *s)
{
	struct clk *c = __clk_lookup(clkname);
	struct clk *p = IS_ERR_OR_NULL(c) ? NULL : __clk_get_parent(c);

	if (IS_ERR_OR_NULL(c)) {
		seq_printf(s, "[%17s: NULL]\n", clkname);
		return;
	}

	seq_printf(s, "[%-17s: %3s, %3d, %3d, %10ld, %17s]\n",
		__clk_get_name(c),
		(__clk_is_enabled(c) || __clk_is_prepared(c)) ? "ON" : "off",
		__clk_is_prepared(c),
		__clk_get_enable_count(c),
		__clk_get_rate(c),
		p ? __clk_get_name(p) : "");
}

static int clkdbg_dump_state_all(struct seq_file *s, void *v)
{
	int i;
	size_t num;

	const char * const *clks = get_all_clk_names(&num);

	pr_debug("\n");
	for (i = 0; i < num; i++)
		dump_clk_state(clks[i], s);

	return 0;
}

static char last_cmd[128] = "null";

static int clkdbg_prepare_enable(struct seq_file *s, void *v)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *clk_name;
	struct clk *clk;
	int r;

	strcpy(cmd, last_cmd);

	ign = strsep(&c, " ");
	clk_name = strsep(&c, " ");

	seq_printf(s, "clk_prepare_enable(%s): ", clk_name);

	clk = __clk_lookup(clk_name);
	if (IS_ERR_OR_NULL(clk)) {
		seq_printf(s, "clk_get(): 0x%p\n", clk);
		return PTR_ERR(clk);
	}

	r = clk_prepare_enable(clk);
	seq_printf(s, "%d\n", r);

	return r;
}

static int clkdbg_disable_unprepare(struct seq_file *s, void *v)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *clk_name;
	struct clk *clk;

	strcpy(cmd, last_cmd);

	ign = strsep(&c, " ");
	clk_name = strsep(&c, " ");

	seq_printf(s, "clk_disable_unprepare(%s): ", clk_name);

	clk = __clk_lookup(clk_name);
	if (IS_ERR_OR_NULL(clk)) {
		seq_printf(s, "clk_get(): 0x%p\n", clk);
		return PTR_ERR(clk);
	}

	clk_disable_unprepare(clk);
	seq_puts(s, "0\n");

	return 0;
}

static int clkdbg_set_parent(struct seq_file *s, void *v)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *clk_name;
	char *parent_name;
	struct clk *clk;
	struct clk *parent;
	int r;

	strcpy(cmd, last_cmd);

	ign = strsep(&c, " ");
	clk_name = strsep(&c, " ");
	parent_name = strsep(&c, " ");

	seq_printf(s, "clk_set_parent(%s, %s): ", clk_name, parent_name);

	clk = __clk_lookup(clk_name);
	if (IS_ERR_OR_NULL(clk)) {
		seq_printf(s, "clk_get(): 0x%p\n", clk);
		return PTR_ERR(clk);
	}

	parent = __clk_lookup(parent_name);
	if (IS_ERR_OR_NULL(parent)) {
		seq_printf(s, "clk_get(): 0x%p\n", parent);
		return PTR_ERR(parent);
	}

	clk_prepare_enable(clk);
	r = clk_set_parent(clk, parent);
	clk_disable_unprepare(clk);
	seq_printf(s, "%d\n", r);

	return r;
}

static int clkdbg_set_rate(struct seq_file *s, void *v)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *clk_name;
	char *rate_str;
	struct clk *clk;
	unsigned long rate;
	int r;

	strcpy(cmd, last_cmd);

	ign = strsep(&c, " ");
	clk_name = strsep(&c, " ");
	rate_str = strsep(&c, " ");
	r = kstrtoul(rate_str, 0, &rate);

	seq_printf(s, "clk_set_rate(%s, %lu): %d: ", clk_name, rate, r);

	clk = __clk_lookup(clk_name);
	if (IS_ERR_OR_NULL(clk)) {
		seq_printf(s, "clk_get(): 0x%p\n", clk);
		return PTR_ERR(clk);
	}

	r = clk_set_rate(clk, rate);
	seq_printf(s, "%d\n", r);

	return r;
}

void *reg_from_str(const char *str)
{
	if (sizeof(void *) == sizeof(unsigned long)) {
		unsigned long v;

		if (kstrtoul(str, 0, &v) == 0)
			return (void *)((uintptr_t)v);
	} else if (sizeof(void *) == sizeof(unsigned long long)) {
		unsigned long long v;

		if (kstrtoull(str, 0, &v) == 0)
			return (void *)((uintptr_t)v);
	} else {
		unsigned long long v;

		clk_warn("unexpected pointer size: sizeof(void *): %zu\n",
			sizeof(void *));

		if (kstrtoull(str, 0, &v) == 0)
			return (void *)((uintptr_t)v);
	}

	clk_warn("%s(): parsing error: %s\n", __func__, str);

	return NULL;
}

static int parse_reg_val_from_cmd(void __iomem **preg, unsigned long *pval)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *reg_str;
	char *val_str;
	int r = 0;

	strcpy(cmd, last_cmd);

	ign = strsep(&c, " ");
	reg_str = strsep(&c, " ");
	val_str = strsep(&c, " ");

	if (preg)
		*preg = reg_from_str(reg_str);

	if (pval)
		r = kstrtoul(val_str, 0, pval);

	return r;
}

static int clkdbg_reg_read(struct seq_file *s, void *v)
{
	void __iomem *reg;
	unsigned long val;

	parse_reg_val_from_cmd(&reg, NULL);
	seq_printf(s, "readl(0x%p): ", reg);

	val = clk_readl(reg);
	seq_printf(s, "0x%08x\n", (u32)val);

	return 0;
}

static int clkdbg_reg_write(struct seq_file *s, void *v)
{
	void __iomem *reg;
	unsigned long val;

	parse_reg_val_from_cmd(&reg, &val);
	seq_printf(s, "writel(0x%p, 0x%08x): ", reg, (u32)val);

	clk_writel(reg, val);
	val = clk_readl(reg);
	seq_printf(s, "0x%08x\n", (u32)val);

	return 0;
}

static int clkdbg_reg_set(struct seq_file *s, void *v)
{
	void __iomem *reg;
	unsigned long val;

	parse_reg_val_from_cmd(&reg, &val);
	seq_printf(s, "writel(0x%p, 0x%08x): ", reg, (u32)val);

	clk_setl(reg, val);
	val = clk_readl(reg);
	seq_printf(s, "0x%08x\n", (u32)val);

	return 0;
}

static int clkdbg_reg_clr(struct seq_file *s, void *v)
{
	void __iomem *reg;
	unsigned long val;

	parse_reg_val_from_cmd(&reg, &val);
	seq_printf(s, "writel(0x%p, 0x%08x): ", reg, (u32)val);

	clk_clrl(reg, val);
	val = clk_readl(reg);
	seq_printf(s, "0x%08x\n", (u32)val);

	return 0;
}

#if CLKDBG_PM_DOMAIN

/*
 * pm_domain support
 */

static struct generic_pm_domain **get_all_pm_domain(int *numpd)
{
	static struct generic_pm_domain *pds[10];
	const int maxpd = ARRAY_SIZE(pds);
#if CLKDBG_8173
	const char *cmp = "mediatek,mt8173-scpsys";
#endif

	struct device_node *node;
	int i;

	node = of_find_compatible_node(NULL, NULL, cmp);

	if (!node) {
		clk_err("node '%s' not found!\n", cmp);
		return NULL;
	}

	for (i = 0; i < maxpd; i++) {
		struct of_phandle_args pa;

		pa.np = node;
		pa.args[0] = i;
		pa.args_count = 1;
		pds[i] = of_genpd_get_from_provider(&pa);

		if (IS_ERR(pds[i]))
			break;
	}

	if (numpd)
		*numpd = i;

	return pds;
}

static struct generic_pm_domain *genpd_from_name(const char *name)
{
	int i;
	int numpd;
	struct generic_pm_domain **pds = get_all_pm_domain(&numpd);

	for (i = 0; i < numpd; i++) {
		struct generic_pm_domain *pd = pds[i];

		if (IS_ERR_OR_NULL(pd))
			continue;

		if (strcmp(name, pd->name) == 0)
			return pd;
	}

	return NULL;
}

static struct platform_device *pdev_from_name(const char *name)
{
	int i;
	int numpd;
	struct generic_pm_domain **pds = get_all_pm_domain(&numpd);

	for (i = 0; i < numpd; i++) {
		struct pm_domain_data *pdd;
		struct generic_pm_domain *pd = pds[i];

		if (IS_ERR_OR_NULL(pd))
			continue;

		list_for_each_entry(pdd, &pd->dev_list, list_node) {
			struct device *dev = pdd->dev;
			struct platform_device *pdev = to_platform_device(dev);

			if (strcmp(name, pdev->name) == 0)
				return pdev;
		}
	}

	return NULL;
}

static void seq_print_all_genpd(struct seq_file *s)
{
	static const char * const gpd_status_name[] = {
		"ACTIVE",
		"WAIT_MASTER",
		"BUSY",
		"REPEAT",
		"POWER_OFF",
	};

	static const char * const prm_status_name[] = {
		"active",
		"resuming",
		"suspended",
		"suspending",
	};

	int i;
	int numpd;
	struct generic_pm_domain **pds = get_all_pm_domain(&numpd);

	seq_puts(s, "domain_on [pmd_name  status]\n");
	seq_puts(s, "\tdev_on (dev_name usage_count, disable, status)\n");
	seq_puts(s, "------------------------------------------------------\n");

	for (i = 0; i < numpd; i++) {
		struct pm_domain_data *pdd;
		struct generic_pm_domain *pd = pds[i];

		if (IS_ERR_OR_NULL(pd)) {
			seq_printf(s, "pd[%d]: 0x%p\n", i, pd);
			continue;
		}

		seq_printf(s, "%c [%-9s %11s]\n",
			(pd->status == GPD_STATE_ACTIVE) ? '+' : '-',
			pd->name, gpd_status_name[pd->status]);

		list_for_each_entry(pdd, &pd->dev_list, list_node) {
			struct device *dev = pdd->dev;
			struct platform_device *pdev = to_platform_device(dev);

			seq_printf(s, "\t%c (%-16s %3d, %d, %10s)\n",
				pm_runtime_active(dev) ? '+' : '-',
				pdev->name,
				atomic_read(&dev->power.usage_count),
				dev->power.disable_depth,
				prm_status_name[dev->power.runtime_status]);
		}
	}
}

static int clkdbg_dump_genpd(struct seq_file *s, void *v)
{
	seq_print_all_genpd(s);

	return 0;
}

static int clkdbg_pm_genpd_poweron(struct seq_file *s, void *v)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *genpd_name;
	struct generic_pm_domain *pd;

	strcpy(cmd, last_cmd);

	ign = strsep(&c, " ");
	genpd_name = strsep(&c, " ");

	seq_printf(s, "pm_genpd_poweron(%s): ", genpd_name);

	pd = genpd_from_name(genpd_name);
	if (pd) {
		int r = pm_genpd_poweron(pd);

		seq_printf(s, "%d\n", r);
	} else {
		seq_puts(s, "NULL\n");
	}

	return 0;
}

static int clkdbg_pm_genpd_poweroff_unused(struct seq_file *s, void *v)
{
	seq_puts(s, "pm_genpd_poweroff_unused()\n");
	pm_genpd_poweroff_unused();

	return 0;
}

static int clkdbg_pm_runtime_enable(struct seq_file *s, void *v)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *dev_name;
	struct platform_device *pdev;

	strcpy(cmd, last_cmd);

	ign = strsep(&c, " ");
	dev_name = strsep(&c, " ");

	seq_printf(s, "pm_runtime_enable(%s): ", dev_name);

	pdev = pdev_from_name(dev_name);
	if (pdev) {
		pm_runtime_enable(&pdev->dev);
		seq_puts(s, "\n");
	} else {
		seq_puts(s, "NULL\n");
	}

	return 0;
}

static int clkdbg_pm_runtime_disable(struct seq_file *s, void *v)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *dev_name;
	struct platform_device *pdev;

	strcpy(cmd, last_cmd);

	ign = strsep(&c, " ");
	dev_name = strsep(&c, " ");

	seq_printf(s, "pm_runtime_disable(%s): ", dev_name);

	pdev = pdev_from_name(dev_name);
	if (pdev) {
		pm_runtime_disable(&pdev->dev);
		seq_puts(s, "\n");
	} else {
		seq_puts(s, "NULL\n");
	}

	return 0;
}

static int clkdbg_pm_runtime_get_sync(struct seq_file *s, void *v)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *dev_name;
	struct platform_device *pdev;

	strcpy(cmd, last_cmd);

	ign = strsep(&c, " ");
	dev_name = strsep(&c, " ");

	seq_printf(s, "pm_runtime_get_sync(%s): ", dev_name);

	pdev = pdev_from_name(dev_name);
	if (pdev) {
		int r = pm_runtime_get_sync(&pdev->dev);

		seq_printf(s, "%d\n", r);
	} else {
		seq_puts(s, "NULL\n");
	}

	return 0;
}

static int clkdbg_pm_runtime_put_sync(struct seq_file *s, void *v)
{
	char cmd[sizeof(last_cmd)];
	char *c = cmd;
	char *ign;
	char *dev_name;
	struct platform_device *pdev;

	strcpy(cmd, last_cmd);

	ign = strsep(&c, " ");
	dev_name = strsep(&c, " ");

	seq_printf(s, "pm_runtime_put_sync(%s): ", dev_name);

	pdev = pdev_from_name(dev_name);
	if (pdev) {
		int r = pm_runtime_put_sync(&pdev->dev);

		seq_printf(s, "%d\n", r);
	} else {
		seq_puts(s, "NULL\n");
	}

	return 0;
}

#endif /* CLKDBG_PM_DOMAIN */

#define CMDFN(_cmd, _fn) {	\
	.cmd = _cmd,		\
	.fn = _fn,		\
}

static int clkdbg_show(struct seq_file *s, void *v)
{
	static const struct {
		const char	*cmd;
		int (*fn)(struct seq_file *, void *);
	} cmds[] = {
		CMDFN("dump_regs", seq_print_regs),
		CMDFN("dump_state", clkdbg_dump_state_all),
		CMDFN("fmeter", seq_print_fmeter_all),
		CMDFN("prepare_enable", clkdbg_prepare_enable),
		CMDFN("disable_unprepare", clkdbg_disable_unprepare),
		CMDFN("set_parent", clkdbg_set_parent),
		CMDFN("set_rate", clkdbg_set_rate),
		CMDFN("reg_read", clkdbg_reg_read),
		CMDFN("reg_write", clkdbg_reg_write),
		CMDFN("reg_set", clkdbg_reg_set),
		CMDFN("reg_clr", clkdbg_reg_clr),
#if CLKDBG_PM_DOMAIN
		CMDFN("dump_genpd", clkdbg_dump_genpd),
		CMDFN("pm_genpd_poweron", clkdbg_pm_genpd_poweron),
		CMDFN("pm_genpd_poweroff_unused",
			clkdbg_pm_genpd_poweroff_unused),
		CMDFN("pm_runtime_enable", clkdbg_pm_runtime_enable),
		CMDFN("pm_runtime_disable", clkdbg_pm_runtime_disable),
		CMDFN("pm_runtime_get_sync", clkdbg_pm_runtime_get_sync),
		CMDFN("pm_runtime_put_sync", clkdbg_pm_runtime_put_sync),
#endif /* CLKDBG_PM_DOMAIN */
	};

	int i;
	char cmd[sizeof(last_cmd)];

	pr_debug("last_cmd: %s\n", last_cmd);

	strcpy(cmd, last_cmd);

	for (i = 0; i < ARRAY_SIZE(cmds); i++) {
		char *c = cmd;
		char *token = strsep(&c, " ");

		if (strcmp(cmds[i].cmd, token) == 0)
			return cmds[i].fn(s, v);
	}

	return 0;
}

static int clkdbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, clkdbg_show, NULL);
}

static ssize_t clkdbg_write(
		struct file *file,
		const char __user *buffer,
		size_t count,
		loff_t *data)
{
	char desc[sizeof(last_cmd)];
	int len = 0;

	pr_debug("count: %zu\n", count);
	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';
	strcpy(last_cmd, desc);
	if (last_cmd[len - 1] == '\n')
		last_cmd[len - 1] = 0;

	return count;
}

static const struct file_operations clkdbg_fops = {
	.owner		= THIS_MODULE,
	.open		= clkdbg_open,
	.read		= seq_read,
	.write		= clkdbg_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void __iomem *get_reg(struct device_node *np, int index)
{
#if DUMMY_REG_TEST
	return kzalloc(PAGE_SIZE, GFP_KERNEL);
#else
	return of_iomap(np, index);
#endif
}

static int __init get_base_from_node(
			const char *cmp, int idx, void __iomem **pbase)
{
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, cmp);

	if (!node) {
		clk_warn("node '%s' not found!\n", cmp);
		return -1;
	}

	*pbase = get_reg(node, idx);
	clk_info("%s base: 0x%p\n", cmp, *pbase);

	return 0;
}

static void __init init_iomap(void)
{
#if CLKDBG_8173
	get_base_from_node("mediatek,mt8173-topckgen", 0, &topckgen_base);
	get_base_from_node("mediatek,mt8173-infracfg", 0, &infrasys_base);
	get_base_from_node("mediatek,mt8173-pericfg", 0, &perisys_base);
	get_base_from_node("mediatek,mt8173-mcucfg", 0, &mcucfg_base);
	get_base_from_node("mediatek,mt8173-apmixedsys", 0, &apmixedsys_base);
	get_base_from_node("mediatek,mt8173-mfgsys", 0, &mfgsys_base);
	get_base_from_node("mediatek,mt8173-mmsys", 0, &mmsys_base);
	get_base_from_node("mediatek,mt8173-imgsys", 0, &imgsys_base);
	get_base_from_node("mediatek,mt8173-vdecsys", 0, &vdecsys_base);
	get_base_from_node("mediatek,mt8173-vencsys", 0, &vencsys_base);
	get_base_from_node("mediatek,mt8173-vencltsys", 0, &vencltsys_base);
#if CLKDBG_8173_TK
	get_base_from_node("mediatek,mt8173-audiosys", 0, &audiosys_base);
	get_base_from_node("mediatek,mt8173-scpsys-pg", 1, &scpsys_base);
#else /* !CLKDBG_8173_TK */
	get_base_from_node("mediatek,mt8173-soc-machine", 0, &audiosys_base);
	get_base_from_node("mediatek,mt8173-scpsys", 0, &scpsys_base);
#endif /* CLKDBG_8173_TK */

#endif /* CLKDBG_SOC */
}

/*
 * clkdbg pm_domain support
 */

static const struct of_device_id clkdbg_id_table[] = {
	{ .compatible = "mediatek,clkdbg-pd",},
	{ },
};
MODULE_DEVICE_TABLE(of, clkdbg_id_table);

static int clkdbg_probe(struct platform_device *pdev)
{
	clk_info("%s():%d: pdev: %s\n", __func__, __LINE__, pdev->name);

	return 0;
}

static int clkdbg_remove(struct platform_device *pdev)
{
	clk_info("%s():%d: pdev: %s\n", __func__, __LINE__, pdev->name);

	return 0;
}

static int clkdbg_pd_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	clk_info("%s():%d: pdev: %s\n", __func__, __LINE__, pdev->name);

	return 0;
}

static int clkdbg_pd_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	clk_info("%s():%d: pdev: %s\n", __func__, __LINE__, pdev->name);

	return 0;
}

static const struct dev_pm_ops clkdbg_pd_pm_ops = {
	.runtime_suspend = clkdbg_pd_runtime_suspend,
	.runtime_resume = clkdbg_pd_runtime_resume,
};

static struct platform_driver clkdbg_driver = {
	.probe		= clkdbg_probe,
	.remove		= clkdbg_remove,
	.driver		= {
		.name	= "clkdbg",
		.owner	= THIS_MODULE,
		.of_match_table = clkdbg_id_table,
		.pm = &clkdbg_pd_pm_ops,
	},
};

module_platform_driver(clkdbg_driver);

/*
 * pm_domain sample code
 */

static struct platform_device *my_pdev;

int power_on_before_work(void)
{
	return pm_runtime_get_sync(&my_pdev->dev);
}

int power_off_after_work(void)
{
	return pm_runtime_put_sync(&my_pdev->dev);
}

static const struct of_device_id my_id_table[] = {
	{ .compatible = "mediatek,my-device",},
	{ },
};
MODULE_DEVICE_TABLE(of, my_id_table);

static int my_probe(struct platform_device *pdev)
{
	pm_runtime_enable(&pdev->dev);

	my_pdev = pdev;

	return 0;
}

static int my_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);

	return 0;
}

static struct platform_driver my_driver = {
	.probe		= my_probe,
	.remove		= my_remove,
	.driver		= {
		.name	= "my_driver",
		.owner	= THIS_MODULE,
		.of_match_table = my_id_table,
	},
};

module_platform_driver(my_driver);

/*
 * init functions
 */

int __init mt_clkdbg_init(void)
{
	init_iomap();

#if DUMP_INIT_STATE
	print_regs();
	print_fmeter_all();
#endif /* DUMP_INIT_STATE */

	return 0;
}
arch_initcall(mt_clkdbg_init);

int __init mt_clkdbg_debug_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create("clkdbg", 0, 0, &clkdbg_fops);
	if (!entry)
		return -ENOMEM;

	return 0;
}
module_init(mt_clkdbg_debug_init);
