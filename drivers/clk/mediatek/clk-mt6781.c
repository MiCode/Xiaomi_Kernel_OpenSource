/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Wendell Lin <wendell.lin@mediatek.com>
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
#include "clk-mt6781-pg.h"

#include <dt-bindings/clock/mt6781-clk.h>

#define MT_CCF_BRINGUP	0
#ifdef CONFIG_ARM64
#define IOMEM(a)	((void __force __iomem *)((a)))
#endif

static DEFINE_SPINLOCK(mipi_lock);
#define apmixed_mipi_lock(flags)   spin_lock_irqsave(&mipi_lock, flags)
#define apmixed_mipi_unlock(flags) spin_unlock_irqrestore(&mipi_lock, flags)

#define mt_reg_sync_writel(v, a) \
	do { \
		__raw_writel((v), IOMEM(a)); \
		/* sync up */ \
		mb(); } \
while (0)

#define clk_readl(addr)			__raw_readl(IOMEM(addr))

#define clk_writel(addr, val)   \
	mt_reg_sync_writel(val, addr)

#define clk_setl(addr, val) \
	mt_reg_sync_writel(clk_readl(addr) | (val), addr)

#define clk_clrl(addr, val) \
	mt_reg_sync_writel(clk_readl(addr) & ~(val), addr)

#define INV_BIT	-1
#define PLL_EN  (0x1 << 0)
#define PLL_PWR_ON  (0x1 << 0)
#define PLL_ISO_EN  (0x1 << 1)
#define ADSPPLL_DIV_RSTB  (0x1 << 23)

static DEFINE_SPINLOCK(mt6781_clk_lock);

/* Total 10 subsys */
void __iomem *cksys_base;
void __iomem *infracfg_base;
void __iomem *apmixed_base;
void __iomem *audio_base;
void __iomem *cam_base;
void __iomem *cam_ra_base;
void __iomem *cam_rb_base;
void __iomem *img1_base;
void __iomem *img2_base;
void __iomem *ipe_base;
void __iomem *mfgcfg_base;
void __iomem *mmsys_config_base;
void __iomem *mdpsys_config_base;
void __iomem *venc_gcon_base;
void __iomem *vdec_gcon_base;
void __iomem *imp_base;

/* CKSYS */
#define CLK_CFG_UPDATE		(cksys_base + 0x004)
#define CLK_CFG_0		(cksys_base + 0x040)
#define CLK_CFG_1		(cksys_base + 0x050)
#define CLK_CFG_6		(cksys_base + 0x0a0)
#define CLK_CFG_6_SET		(cksys_base + 0x0a4)
#define CLK_CFG_6_CLR		(cksys_base + 0x0a8)
#define CLK_CFG_9		(cksys_base + 0x0d0)
#define CLK_MISC_CFG_0		(cksys_base + 0x140)
#define CLK_DBG_CFG		(cksys_base + 0x17C)
#define CLK_SCP_CFG_0		(cksys_base + 0x200)
#define CLK_SCP_CFG_1		(cksys_base + 0x210)
#define CLK26CALI_0		(cksys_base + 0x220)
#define CLK26CALI_1		(cksys_base + 0x224)

/* CG */
#define INFRA_PDN_SET0		(infracfg_base + 0x0080)
#define INFRA_PDN_CLR0		(infracfg_base + 0x0084)
#define INFRA_PDN_STA0		(infracfg_base + 0x0090)
#define INFRA_PDN_SET1		(infracfg_base + 0x0088)
#define INFRA_PDN_CLR1		(infracfg_base + 0x008C)
#define INFRA_PDN_STA1		(infracfg_base + 0x0094)
#define INFRA_PDN_SET2		(infracfg_base + 0x00A4)
#define INFRA_PDN_CLR2		(infracfg_base + 0x00A8)
#define INFRA_PDN_STA2		(infracfg_base + 0x00AC)
#define INFRA_PDN_SET3		(infracfg_base + 0x00C0)
#define INFRA_PDN_CLR3		(infracfg_base + 0x00C4)
#define INFRA_PDN_STA3		(infracfg_base + 0x00C8)
#define INFRA_TOPAXI_SI0_CTL	(infracfg_base + 0x0200)

#define AP_PLL_CON2		(apmixed_base + 0x0008)
#define AP_PLL_CON3		(apmixed_base + 0x000C)
#define AP_PLL_CON4		(apmixed_base + 0x0010)
#define AP_PLL_CON6		(apmixed_base + 0x0018)
#define AP_PLL_CON8		(apmixed_base + 0x0020)

#define ARMPLL_LL_CON0		(apmixed_base + 0x0200)
#define ARMPLL_LL_CON1		(apmixed_base + 0x0204)
#define ARMPLL_LL_PWR_CON0	(apmixed_base + 0x020C)
#define ARMPLL_BL_CON0		(apmixed_base + 0x0210)
#define ARMPLL_BL_CON1		(apmixed_base + 0x0214)
#define ARMPLL_BL_PWR_CON0	(apmixed_base + 0x021C)
#define ARMPLL_BB_CON0		(apmixed_base + 0x0220)
#define ARMPLL_BB_CON1		(apmixed_base + 0x0224)
#define ARMPLL_BB_PWR_CON0	(apmixed_base + 0x022C)
#define MAINPLL_CON0		(apmixed_base + 0x0230)
#define MAINPLL_PWR_CON0	(apmixed_base + 0x023C)
#define UNIVPLL_CON0		(apmixed_base + 0x0240)
#define UNIVPLL_CON1		(apmixed_base + 0x0244)
#define UNIVPLL_PWR_CON0	(apmixed_base + 0x024C)
#define MFGPLL_CON0		(apmixed_base + 0x0250)
#define MFGPLL_PWR_CON0		(apmixed_base + 0x025C)
#define MSDCPLL_CON0		(apmixed_base + 0x0260)
#define MSDCPLL_PWR_CON0	(apmixed_base + 0x026C)
#define MMPLL_CON0		(apmixed_base + 0x0280)
#define MMPLL_CON1		(apmixed_base + 0x0284)
#define MMPLL_PWR_CON0		(apmixed_base + 0x028C)
#define CCIPLL_CON0		(apmixed_base + 0x02A0)
#define CCIPLL_PWR_CON0		(apmixed_base + 0x02AC)
#define ADSPPLL_CON0		(apmixed_base + 0x02B0)
#define ADSPPLL_PWR_CON0	(apmixed_base + 0x02BC)
#define APLL1_CON0		(apmixed_base + 0x02C0)
#define APLL1_CON1		(apmixed_base + 0x02C4)
#define APLL1_CON2		(apmixed_base + 0x02C8)
#define APLL1_PWR_CON0		(apmixed_base + 0x02D0)
#define APLL2_CON0		(apmixed_base + 0x02D4)
#define APLL2_CON1		(apmixed_base + 0x02D8)
#define APLL2_CON2		(apmixed_base + 0x02DC)
#define APLL2_PWR_CON0		(apmixed_base + 0x02E4)


#define CK_CFG_UPDATE	0x04
#define CK_CFG_UPDATE1	0x08
#define CK_CFG_0	0x40
#define CK_CFG_0_SET	0x44
#define CK_CFG_0_CLR	0x48
#define CK_CFG_1	0x50
#define CK_CFG_1_SET	0x54
#define CK_CFG_1_CLR	0x58
#define CK_CFG_2	0x60
#define CK_CFG_2_SET	0x64
#define CK_CFG_2_CLR	0x68
#define CK_CFG_3	0x70
#define CK_CFG_3_SET	0x74
#define CK_CFG_3_CLR	0x78
#define CK_CFG_4	0x80
#define CK_CFG_4_SET	0x84
#define CK_CFG_4_CLR	0x88
#define CK_CFG_5	0x90
#define CK_CFG_5_SET	0x94
#define CK_CFG_5_CLR	0x98
#define CK_CFG_6	0xa0
#define CK_CFG_6_SET	0xa4
#define CK_CFG_6_CLR	0xa8
#define CK_CFG_7	0xb0
#define CK_CFG_7_SET	0xb4
#define CK_CFG_7_CLR	0xb8
#define CK_CFG_8	0xc0
#define CK_CFG_8_SET	0xc4
#define CK_CFG_8_CLR	0xc8
#define CK_CFG_9	0xd0
#define CK_CFG_9_SET	0xd4
#define CK_CFG_9_CLR	0xd8
#define CK_CFG_10	0xe0
#define CK_CFG_10_SET	0xe4
#define CK_CFG_10_CLR	0xe8
#define CK_CFG_11	0xec
#define CK_CFG_11_SET	0xf0
#define CK_CFG_11_CLR	0xf4
#define CK_CFG_12	0x100
#define CK_CFG_12_SET	0x104
#define CK_CFG_12_CLR	0x108

static const struct mtk_fixed_clk fixed_clks[] __initconst = {
	FIXED_CLK(TOP_OSC, "ulposc", NULL, 250000000),
};

static const struct mtk_fixed_factor top_divs[] __initconst = {
	FACTOR(TOP_MAINPLL_CK, "mainpll_ck", "mainpll", 1,
		1),
	FACTOR(TOP_MAINPLL_D2, "mainpll_d2", "mainpll_ck", 1,
		2),
	FACTOR(TOP_MAINPLL_D2_D2, "mainpll_d2_d2", "mainpll_d2", 1,
		2),
	FACTOR(TOP_MAINPLL_D2_D4, "mainpll_d2_d4", "mainpll_d2", 1,
		4),
	FACTOR(TOP_MAINPLL_D2_D8, "mainpll_d2_d8", "mainpll_d2", 1,
		8),
	FACTOR(TOP_MAINPLL_D2_D16, "mainpll_d2_d16", "mainpll_d2", 1,
		16),

	FACTOR(TOP_MAINPLL_D3, "mainpll_d3", "mainpll", 1,
		3),
	FACTOR(TOP_MAINPLL_D3_D2, "mainpll_d3_d2", "mainpll_d3", 1,
		2),
	FACTOR(TOP_MAINPLL_D3_D4, "mainpll_d3_d4", "mainpll_d3", 1,
		4),
	FACTOR(TOP_MAINPLL_D3_D8, "mainpll_d3_d8", "mainpll_d3", 1,
		8),

	FACTOR(TOP_MAINPLL_D5, "mainpll_d5", "mainpll", 1,
		5),
	FACTOR(TOP_MAINPLL_D5_D2, "mainpll_d5_d2", "mainpll_d5", 1,
		2),
	FACTOR(TOP_MAINPLL_D5_D4, "mainpll_d5_d4", "mainpll_d5", 1,
		4),

	FACTOR(TOP_MAINPLL_D7, "mainpll_d7", "mainpll", 1,
		7),
	FACTOR(TOP_MAINPLL_D7_D2, "mainpll_d7_d2", "mainpll_d7", 1,
		2),
	FACTOR(TOP_MAINPLL_D7_D4, "mainpll_d7_d4", "mainpll_d7", 1,
		4),

	FACTOR(TOP_UNIVPLL_CK, "univpll", "univ2pll", 1,
		2),
	FACTOR(TOP_UNIVPLL_D2, "univpll_d2", "univpll", 1,
		2),
	FACTOR(TOP_UNIVPLL_D2_D2, "univpll_d2_d2", "univpll_d2", 1,
		2),
	FACTOR(TOP_UNIVPLL_D2_D4, "univpll_d2_d4", "univpll_d2", 1,
		4),
	FACTOR(TOP_UNIVPLL_D2_D8, "univpll_d2_d8", "univpll_d2", 1,
		8),


	FACTOR(TOP_UNIVPLL_D3, "univpll_d3", "univpll", 1,
		3),
	FACTOR(TOP_UNIVPLL_D3_D2, "univpll_d3_d2", "univpll_d3", 1,
		2),
	FACTOR(TOP_UNIVPLL_D3_D4, "univpll_d3_d4", "univpll_d3", 1,
		4),
	FACTOR(TOP_UNIVPLL_D3_D8, "univpll_d3_d8", "univpll_d3", 1,
		8),
	FACTOR(TOP_UNIVPLL_D3_D16, "univpll_d3_d16", "univpll_d3", 1,
		16),
	FACTOR(TOP_UNIVPLL_D3_D32, "univpll_d3_d32", "univpll_d3", 1,
		32),

	FACTOR(TOP_UNIVPLL_D5, "univpll_d5", "univpll", 1,
		5),
	FACTOR(TOP_UNIVPLL_D5_D2, "univpll_d5_d2", "univpll_d5", 1,
		2),
	FACTOR(TOP_UNIVPLL_D5_D4, "univpll_d5_d4", "univpll_d5", 1,
		4),
	FACTOR(TOP_UNIVPLL_D5_D8, "univpll_d5_d8", "univpll_d5", 1,
		8),

	FACTOR(TOP_UNIVPLL_D7, "univpll_d7", "univpll", 1,
		7),

	FACTOR(TOP_UNIVP_192M_CK, "univpll_192m_ck", "univ2pll", 1,
		13),
	FACTOR(TOP_UNIVP_192M_D2, "univpll_192m_d2", "univpll_192m_ck", 1,
		2),
	FACTOR(TOP_UNIVP_192M_D4, "univpll_192m_d4", "univpll_192m_ck", 1,
		4),
	FACTOR(TOP_UNIVP_192M_D8, "univpll_192m_d8", "univpll_192m_ck", 1,
		8),
	FACTOR(TOP_UNIVP_192M_D16, "univpll_192m_d16", "univpll_192m_ck", 1,
		16),
	FACTOR(TOP_UNIVP_192M_D32, "univpll_192m_d32", "univpll_192m_ck", 1,
		32),

	FACTOR(TOP_APLL1_CK, "apll1_ck", "apll1", 1,
		1),
	FACTOR(TOP_APLL1_D2, "apll1_d2", "apll1", 1,
		2),
	FACTOR(TOP_APLL1_D4, "apll1_d4", "apll1", 1,
		4),
	FACTOR(TOP_APLL1_D8, "apll1_d8", "apll1", 1,
		8),
	FACTOR(TOP_APLL2_CK, "apll2_ck", "apll2", 1,
		1),
	FACTOR(TOP_APLL2_D2, "apll2_d2", "apll2", 1,
		2),
	FACTOR(TOP_APLL2_D4, "apll2_d4", "apll2", 1,
		4),
	FACTOR(TOP_APLL2_D8, "apll2_d8", "apll2", 1,
		8),

	FACTOR(TOP_MMPLL_CK, "mmpll_ck", "mmpll", 1,
		1),
	FACTOR(TOP_MMPLL_D2, "mmpll_d2", "mmpll", 1,
		2),
	FACTOR(TOP_MMPLL_D4, "mmpll_d4", "mmpll", 1,
		4),
	FACTOR(TOP_MMPLL_D4_D2, "mmpll_d4_d2", "mmpll_d4", 1,
		2),
	FACTOR(TOP_MMPLL_D5, "mmpll_d5", "mmpll", 1,
		5),
	FACTOR(TOP_MMPLL_D5_D2, "mmpll_d5_d2", "mmpll_d5", 1,
		2),
	FACTOR(TOP_MMPLL_D6, "mmpll_d6", "mmpll", 1,
		6),
	FACTOR(TOP_MMPLL_D7, "mmpll_d7", "mmpll", 1,
		7),

	FACTOR(TOP_MFGPLL_CK, "mfgpll_ck", "mfgpll", 1,
		1),
	FACTOR(TOP_ADSPPLL_CK, "adsppll_ck", "adsppll", 1,
		1),
	FACTOR(TOP_ADSPPLL_D2, "adsppll_d2", "adsppll", 1,
		2),
	FACTOR(TOP_ADSPPLL_D4, "adsppll_d4", "adsppll", 1,
		4),
	FACTOR(TOP_ADSPPLL_D5, "adsppll_d5", "adsppll", 1,
		5),
	FACTOR(TOP_ADSPPLL_D6, "adsppll_d6", "adsppll", 1,
		6),
	FACTOR(TOP_ADSPPLL_D8, "adsppll_d8", "adsppll", 1,
		8),

	FACTOR(TOP_MSDCPLL_CK, "msdcpll_ck", "msdcpll", 1,
		1),
	FACTOR(TOP_MSDCPLL_D2, "msdcpll_d2", "msdcpll", 1,
		2),
	FACTOR(TOP_MSDCPLL_D4, "msdcpll_d4", "msdcpll", 1,
		4),

	FACTOR(TOP_OSC_D2, "osc_d2", "ulposc", 1,
		2),
	FACTOR(TOP_OSC_D4, "osc_d4", "ulposc", 1,
		4),
	FACTOR(TOP_OSC_D8, "osc_d8", "ulposc", 1,
		8),
	FACTOR(TOP_OSC_D10, "osc_d10", "ulposc", 1,
		10),
	FACTOR(TOP_OSC_D16, "osc_d16", "ulposc", 1,
		16),
	FACTOR(TOP_OSC_D32, "osc_d32", "ulposc", 1,
		32),
};

static const char * const axi_parents[] __initconst = {
	"clk26m",
	"mainpll_d7",
	"mainpll_d2_d4",
	"univpll_d7"
};

static const char * const scp_parents[] __initconst = {
	"clk26m",
	"mainpll_d2_d4",
	"mainpll_d5",
	"mainpll_d2_d2",
	"mainpll_d3",
	"univpll_d3",
};

static const char * const mfg_parents[] __initconst = {
	"clk26m",
	"mfgpll_ck",
	"mainpll_d3",
	"mainpll_d5"
};

static const char * const camtg_parents[] __initconst = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d3_d8",
	"univpll_192m_d4",
	"univpll_d3_d32",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const camtg1_parents[] __initconst = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d3_d8",
	"univpll_192m_d4",
	"univpll_d3_d32",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const camtg2_parents[] __initconst = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d3_d8",
	"univpll_192m_d4",
	"univpll_d3_d32",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const camtg3_parents[] __initconst = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d3_d8",
	"univpll_192m_d4",
	"univpll_d3_d32",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const camtg4_parents[] __initconst = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d3_d8",
	"univpll_192m_d4",
	"univpll_d3_d32",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const camtg5_parents[] __initconst = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d3_d8",
	"univpll_192m_d4",
	"univpll_d3_d32",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const camtg6_parents[] __initconst = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d3_d8",
	"univpll_192m_d4",
	"univpll_d3_d32",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const uart_parents[] __initconst = {
	"clk26m",
	"univpll_d3_d8"
};

static const char * const spi_parents[] __initconst = {
	"clk26m",
	"mainpll_d5_d4",
	"mainpll_d3_d4",
	"mainpll_d5_d2",
	"mainpll_d2_d4",
	"mainpll_d7",
	"mainpll_d3_d2",
	"mainpll_d5"
};

static const char * const msdc50_0_hclk_parents[] __initconst = {
	"clk26m",
	"mainpll_d2_d2",
	"mainpll_d7",
	"mainpll_d3_d2"
};

static const char * const msdc50_0_parents[] __initconst = {
	"clk26m",
	"msdcpll_ck",
	"univpll_d3",
	"msdcpll_d2",
	"mainpll_d7",
	"mainpll_d3_d2",
	"univpll_d2_d2"
};

static const char * const msdc30_1_parents[] __initconst = {
	"clk26m",
	"msdcpll_d2",
	"univpll_d3_d2",
	"mainpll_d3_d2",
	"mainpll_d7"
};

static const char * const audio_parents[] __initconst = {
	"clk26m",
	"mainpll_d5_d4",
	"mainpll_d7_d4",
	"mainpll_d2_d16"
};

static const char * const aud_intbus_parents[] __initconst = {
	"clk26m",
	"mainpll_d2_d4",
	"mainpll_d7_d2"
};

static const char * const aud_1_parents[] __initconst = {
	"clk26m",
	"apll1_ck"
};

static const char * const aud_2_parents[] __initconst = {
	"clk26m",
	"apll2_ck"
};

static const char * const aud_eng1_parents[] __initconst = {
	"clk26m",
	"apll1_d2",
	"apll1_d4",
	"apll1_d8"
};

static const char * const aud_eng2_parents[] __initconst = {
	"clk26m",
	"apll2_d2",
	"apll2_d4",
	"apll2_d8"
};

static const char * const disppwm_parents[] __initconst = {
	"clk26m",
	"univpll_d5_d2",
	"univpll_d3_d4",
	"osc_d2",
	"osc_d8"
};

static const char * const sspm_parents[] __initconst = {
	"clk26m",
	"mainpll_d2_d2",
	"mainpll_d3_d2",
	"mainpll_d5",
	"mainpll_d3"
};

static const char * const dxcc_parents[] __initconst = {
	"clk26m",
	"mainpll_d2_d2",
	"mainpll_d2_d4"
};

static const char * const usb_top_parents[] __initconst = {
	"clk26m",
	"univpll_d5_d4",
	"univpll_d5_d2"
};


static const char * const srck_parents[] __initconst = {
	"clk26m",
	"osc_d10"
};

static const char * const spm_parents[] __initconst = {
	"clk26m",/* clkrtc */
	"osc_d10",
	"clk26m",
	"mainpll_d7_d2"
};

static const char * const i2c_parents[] __initconst = {
	"clk26m",
	"univpll_d5_d4",
	"univpll_d3_d4",
	"univpll_d5_d2"
};

static const char * const pwm_parents[] __initconst = {
	"clk26m",
	"univpll_d3_d8",
	"univpll_d3_d4",
	"univpll_d2_d4"
};

static const char * const seninf_parents[] __initconst = {
	"clk26m",
	"univpll_d2_d4",
	"univpll_d2_d2",
	"univpll_d3_d2"
};

static const char * const seninf1_parents[] __initconst = {
	"clk26m",
	"univpll_d2_d4",
	"univpll_d2_d2",
	"univpll_d3_d2"
};

static const char * const seninf2_parents[] __initconst = {
	"clk26m",
	"univpll_d2_d4",
	"univpll_d2_d2",
	"univpll_d3_d2"
};

static const char * const seninf3_parents[] __initconst = {
	"clk26m",
	"univpll_d2_d4",
	"univpll_d2_d2",
	"univpll_d3_d2"
};

static const char * const aes_msdcfde_parents[] __initconst = {
	"clk26m",
	"univpll_d3",
	"mainpll_d3",
	"univpll_d2_d2",
	"mainpll_d2_d2",
	"mainpll_d2_d4"
};

static const char * const fpwrap_ulposc_parents[] __initconst = {
	"clk26m",
	"univpll_d5_d4",
	"osc_d4",
	"osc_d8",
	"osc_d10",
	"osc_d16",
	"osc_d32"
};

static const char * const camtm_parents[] __initconst = {
	"clk26m",
	"univpll_d2_d4",
	"univpll_d2_d2",
	"univpll_d3_d2"
};

static const char * const venc_parents[] __initconst = {
	"clk26m",
	"mmpll_ck",
	"mainpll_d2_d2",
	"mainpll_d2",
	"univpll_d3",
	"univpll_d2_d2",
	"mainpll_d3",
	"mmpll_ck"
};

static const char * const cam_parents[] __initconst = {
	"clk26m",
	"mainpll_d2",
	"mainpll_d2_d2",
	"univpll_d3",
	"mainpll_d3",
	"mmpll_ck",
	"univpll_d5",
	"univpll_d2_d2",
	"mmpll_d2"
};

static const char * const img1_parents[] __initconst = {
	"clk26m",
	"mainpll_d2",
	"mainpll_d2_d2",
	"univpll_d3",
	"mainpll_d3",
	"mmpll_ck",
	"univpll_d5",
	"univpll_d2_d2",
	"mmpll_d2"
};

static const char * const ipe_parents[] __initconst = {
	"clk26m",
	"mainpll_d2",
	"mainpll_d2_d2",
	"univpll_d3",
	"mainpll_d3",
	"mmpll_ck",
	"univpll_d5",
	"univpll_d2_d2",
	"mmpll_d2"
};

static const char * const dpmaif_parents[] __initconst = {
	"clk26m",
	"univpll_d2_d2",
	"mainpll_d3",
	"mainpll_d2_d2",
	"univpll_d3_d2"
};

static const char * const vdec_parents[] __initconst = {
	"clk26m",
	"mainpll_d3",
	"mainpll_d2_d2",
	"univpll_d5",
	"mainpll_d2",
	"univpll_d3",
	"univpll_d2_d2"
};

static const char * const disp_parents[] __initconst = {
	"clk26m",
	"univpll_d3_d2",
	"mainpll_d5",
	"univpll_d5",
	"univpll_d2_d2",
	"mainpll_d3",
	"univpll_d3",
	"mainpll_d2",
	"mmpll_ck"
};

static const char * const mdp_parents[] __initconst = {
	"clk26m",
	"mainpll_d5",
	"univpll_d5",
	"mainpll_d2_d2",
	"univpll_d2_d2",
	"mainpll_d3",
	"univpll_d3",
	"mainpll_d2",
	"mmpll_ck"
};

static const char * const audio_h_parents[] __initconst = {
	"clk26m",
	"univpll_d7",
	"apll1_ck",
	"apll2_ck"
};

static const char * const ufs_parents[] __initconst = {
	"clk26m",
	"mainpll_d7",
	"univpll_d2_d4",
	"mainpll_d2_d4"
};

static const char * const aes_fde_parents[] __initconst = {
	"clk26m",
	"univpll_d3",
	"mainpll_d2_d2",
	"univpll_d5"
};

static const char * const adsp_parents[] __initconst = {
	"clk26m",
	"adsppll_ck",
	"adsppll_d2",
	"adsppll_d4",
	"adsppll_d8"
};

static const char * const dvfsrc_parents[] __initconst = {
	"clk26m",
	"osc_d10"
};

static const char * const dsi_occ_parents[] __initconst = {
	"clk26m",
	"univpll_d3_d2",
	"mpll_ck",
	"mainpll_d5"
};

static const char * const spmi_mst_parents[] __initconst = {
	"clk26m",
	"univpll_d5_d4",
	"osc_d4",
	"osc_d8",
	"osc_d10",
	"osc_d16",
	"osc_d32"
};

static const char * const i2s0_m_ck_parents[] __initconst = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const i2s1_m_ck_parents[] __initconst = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const i2s2_m_ck_parents[] __initconst = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const i2s3_m_ck_parents[] __initconst = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const i2s4_m_ck_parents[] __initconst = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const i2s5_m_ck_parents[] __initconst = {
	"aud_1_sel",
	"aud_2_sel"
};


static const struct mtk_mux top_muxes[] __initconst = {
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD(TOP_MUX_AXI, "axi_sel", axi_parents,
		CK_CFG_0, CK_CFG_0_SET, CK_CFG_0_CLR,
		0, 2, INV_BIT,
		0x004, 0),

	MUX_CLR_SET_UPD(TOP_MUX_SCP, "scp_sel", scp_parents,
		CK_CFG_0, CK_CFG_0_SET, CK_CFG_0_CLR,
		8, 3, INV_BIT,
		0x004, 1),

	MUX_CLR_SET_UPD(TOP_MUX_MFG, "mfg_sel", mfg_parents,
		CK_CFG_0, CK_CFG_0_SET, CK_CFG_0_CLR,
		16, 2, INV_BIT,
		0x004, 2),

	MUX_CLR_SET_UPD(TOP_MUX_CAMTG, "camtg_sel", camtg_parents,
		CK_CFG_0, CK_CFG_0_SET, CK_CFG_0_CLR,
		24, 3, INV_BIT,
		0x004, 3),

	/* CLK_CFG_1 */
	MUX_CLR_SET_UPD(TOP_MUX_CAMTG1, "camtg1_sel", camtg1_parents,
		CK_CFG_1, CK_CFG_1_SET, CK_CFG_1_CLR,
		0, 3, INV_BIT,
		0x004, 4),

	MUX_CLR_SET_UPD(TOP_MUX_CAMTG2, "camtg2_sel", camtg2_parents,
		CK_CFG_1, CK_CFG_1_SET, CK_CFG_1_CLR,
		8, 3, INV_BIT,
		0x004, 5),

	MUX_CLR_SET_UPD(TOP_MUX_CAMTG3, "camtg3_sel", camtg3_parents,
		CK_CFG_1, CK_CFG_1_SET, CK_CFG_1_CLR,
		16, 3, INV_BIT,
		0x004, 6),

	MUX_CLR_SET_UPD(TOP_MUX_CAMTG4, "camtg4_sel", camtg4_parents,
		CK_CFG_1, CK_CFG_1_SET, CK_CFG_1_CLR,
		24, 3, INV_BIT,
		0x004, 7),

	/* CLK_CFG_2 */
	MUX_CLR_SET_UPD(TOP_MUX_CAMTG5, "camtg5_sel", camtg5_parents,
		CK_CFG_2, CK_CFG_2_SET, CK_CFG_2_CLR,
		0, 3, INV_BIT,
		0x004, 8),

	MUX_CLR_SET_UPD(TOP_MUX_CAMTG6, "camtg6_sel", camtg6_parents,
		CK_CFG_2, CK_CFG_2_SET, CK_CFG_2_CLR,
		8, 3, INV_BIT,
		0x004, 9),

	MUX_CLR_SET_UPD(TOP_MUX_UART, "uart_sel", uart_parents,
		CK_CFG_2, CK_CFG_2_SET, CK_CFG_2_CLR,
		16, 1, INV_BIT,
		0x004, 10),

	MUX_CLR_SET_UPD(TOP_MUX_SPI, "spi_sel", spi_parents,
		CK_CFG_2, CK_CFG_2_SET, CK_CFG_2_CLR,
		24, 3, INV_BIT,
		0x004, 11),

	/* CLK_CFG_3 */
	MUX_CLR_SET_UPD(TOP_MUX_MSDC50_0_HCLK, "msdc50_0_hclk_sel", msdc50_0_hclk_parents,
		CK_CFG_3, CK_CFG_3_SET, CK_CFG_3_CLR,
		0, 2, INV_BIT,
		0x004, 12),

	MUX_CLR_SET_UPD(TOP_MUX_MSDC50_0, "msdc50_0_sel", msdc50_0_parents,
		CK_CFG_3, CK_CFG_3_SET, CK_CFG_3_CLR,
		8, 3, INV_BIT,
		0x004, 13),

	MUX_CLR_SET_UPD(TOP_MUX_MSDC30_1, "msdc30_1_sel", msdc30_1_parents,
		CK_CFG_3, CK_CFG_3_SET, CK_CFG_3_CLR,
		16, 3, INV_BIT,
		0x004, 14),

	MUX_CLR_SET_UPD(TOP_MUX_AUDIO, "audio_sel", audio_parents,
		CK_CFG_3, CK_CFG_3_SET, CK_CFG_3_CLR,
		24, 2, INV_BIT,
		0x004, 15),
	/* CLK_CFG_4 */
	MUX_CLR_SET_UPD(TOP_MUX_AUD_INTBUS, "aud_intbus_sel", aud_intbus_parents,
		CK_CFG_4, CK_CFG_4_SET, CK_CFG_4_CLR,
		0, 2, INV_BIT,
		0x004, 16),

	MUX_CLR_SET_UPD(TOP_MUX_AUD_1, "aud_1_sel", aud_1_parents,
		CK_CFG_4, CK_CFG_4_SET, CK_CFG_4_CLR,
		8, 1, INV_BIT,
		0x004, 17),

	MUX_CLR_SET_UPD(TOP_MUX_AUD_2, "aud_2_sel", aud_2_parents,
		CK_CFG_4, CK_CFG_4_SET, CK_CFG_4_CLR,
		16, 1, INV_BIT,
		0x004, 18),

	MUX_CLR_SET_UPD(TOP_MUX_AUD_ENG1, "aud_eng1_sel", aud_eng1_parents,
		CK_CFG_4, CK_CFG_4_SET, CK_CFG_4_CLR,
		24, 2, INV_BIT,
		0x004, 19),

	/* CLK_CFG_5 */
	MUX_CLR_SET_UPD(TOP_MUX_AUD_ENG2, "aud_eng2_sel", aud_eng2_parents,
		CK_CFG_5, CK_CFG_5_SET, CK_CFG_5_CLR,
		0, 2, INV_BIT,
		0x004, 20),

	MUX_CLR_SET_UPD(TOP_MUX_DISP_PWM, "disppwm_sel", disppwm_parents,
		CK_CFG_5, CK_CFG_5_SET, CK_CFG_5_CLR,
		8, 3, INV_BIT,
		0x004, 21),

	MUX_CLR_SET_UPD(TOP_MUX_SSPM, "sspm_sel",
		sspm_parents,
		CK_CFG_5, CK_CFG_5_SET, CK_CFG_5_CLR,
		16, 3, INV_BIT,
		0x004, 22),

	MUX_CLR_SET_UPD(TOP_MUX_DXCC, "dxcc_sel",
		dxcc_parents,
		CK_CFG_5, CK_CFG_5_SET, CK_CFG_5_CLR,
		24, 2, INV_BIT,
		0x004, 23),

	/* CLK_CFG_6 */
	MUX_CLR_SET_UPD(TOP_MUX_USB_TOP, "usb_top_sel",
		usb_top_parents,
		CK_CFG_6, CK_CFG_6_SET, CK_CFG_6_CLR,
		0, 2, INV_BIT,
		0x004, 24),

	MUX_CLR_SET_UPD(TOP_MUX_SRCK, "srck_sel", srck_parents,
		CK_CFG_6, CK_CFG_6_SET, CK_CFG_6_CLR,
		8, 2, INV_BIT,
		0x004, 25),

	MUX_CLR_SET_UPD(TOP_MUX_SPM, "spm_sel",
		spm_parents,
		CK_CFG_6, CK_CFG_6_SET, CK_CFG_6_CLR,
		16, 2, INV_BIT,
		0x004, 26),

	MUX_CLR_SET_UPD(TOP_MUX_I2C, "i2c_sel",
		i2c_parents,
		CK_CFG_6, CK_CFG_6_SET, CK_CFG_6_CLR,
		24, 2, INV_BIT,
		0x004, 27),

	/* CLK_CFG_7 */
	MUX_CLR_SET_UPD(TOP_MUX_PWM, "pwm_sel",
		pwm_parents,
		CK_CFG_7, CK_CFG_7_SET, CK_CFG_7_CLR,
		0, 2, INV_BIT,
		0x004, 28),

	MUX_CLR_SET_UPD(TOP_MUX_SENINF, "seninf_sel",
		seninf_parents,
		CK_CFG_7, CK_CFG_7_SET, CK_CFG_7_CLR,
		8, 2, INV_BIT,
		0x004, 29),

	MUX_CLR_SET_UPD(TOP_MUX_SENINF1, "seninf1_sel",
		seninf1_parents,
		CK_CFG_7, CK_CFG_7_SET, CK_CFG_7_CLR,
		16, 2, INV_BIT,
		0x004, 30),

	MUX_CLR_SET_UPD(TOP_MUX_SENINF2, "seninf2_sel",
		seninf2_parents,
		CK_CFG_7, CK_CFG_7_SET, CK_CFG_7_CLR,
		24, 2, INV_BIT,
		0x008, 0),

	/* CLK_CFG_8 */
	MUX_CLR_SET_UPD(TOP_MUX_SENINF3, "seninf3_sel",
		seninf3_parents,
		CK_CFG_8, CK_CFG_8_SET, CK_CFG_8_CLR,
		0, 2, INV_BIT,
		0x008, 1),

	MUX_CLR_SET_UPD(TOP_MUX_AES_MSDCFDE, "aes_msdcfde_sel",
		aes_msdcfde_parents,
		CK_CFG_8, CK_CFG_8_SET, CK_CFG_8_CLR,
		8, 3, INV_BIT,
		0x008, 2),

	MUX_CLR_SET_UPD(TOP_MUX_FPWRAP_ULPOSC, "fpwrap_ulposc_sel",
		fpwrap_ulposc_parents,
		CK_CFG_8, CK_CFG_8_SET, CK_CFG_8_CLR,
		16, 3, INV_BIT,
		0x008, 3),

	MUX_CLR_SET_UPD(TOP_MUX_CAMTM, "camtm_sel", camtm_parents,
		CK_CFG_8, CK_CFG_8_SET, CK_CFG_8_CLR,
		24, 1, INV_BIT,
		0x008, 4),

	/* CLK_CFG_9 */
	MUX_CLR_SET_UPD(TOP_MUX_VENC, "venc_sel", venc_parents,
		CK_CFG_9, CK_CFG_9_SET, CK_CFG_9_CLR,
		0, 3, INV_BIT,
		0x008, 5),

	MUX_CLR_SET_UPD(TOP_MUX_CAM, "cam_sel", cam_parents,
		CK_CFG_9, CK_CFG_9_SET, CK_CFG_9_CLR,
		8, 4, INV_BIT,
		0x008, 6),

	MUX_CLR_SET_UPD(TOP_MUX_IMG1, "img1_sel", img1_parents,
		CK_CFG_9, CK_CFG_9_SET, CK_CFG_9_CLR,
		16, 4, INV_BIT,
		0x008, 7),

	MUX_CLR_SET_UPD(TOP_MUX_IPE, "ipe_sel", ipe_parents,
		CK_CFG_9, CK_CFG_9_SET, CK_CFG_9_CLR,
		24, 4, INV_BIT,
		0x008, 8),

	/* CLK_CFG_10 */
	MUX_CLR_SET_UPD(TOP_MUX_DPMAIF, "dpmaif_sel", dpmaif_parents,
		CK_CFG_10, CK_CFG_10_SET, CK_CFG_10_CLR,
		0, 3, INV_BIT,
		0x008, 9),

	MUX_CLR_SET_UPD(TOP_MUX_VDEC, "vdec_sel",
		vdec_parents,
		CK_CFG_10, CK_CFG_10_SET, CK_CFG_10_CLR,
		8, 3, INV_BIT,
		0x008, 10),

	MUX_CLR_SET_UPD(TOP_MUX_DISP, "disp_sel",
		disp_parents,
		CK_CFG_10, CK_CFG_10_SET, CK_CFG_10_CLR,
		16, 4, INV_BIT,
		0x008, 11),

	MUX_CLR_SET_UPD(TOP_MUX_MDP, "mdp_sel",
		mdp_parents,
		CK_CFG_10, CK_CFG_10_SET, CK_CFG_10_CLR,
		24, 4, INV_BIT,
		0x008, 12),

	/* CLK_CFG_11 */
	MUX_CLR_SET_UPD(TOP_MUX_AUDIO_H, "audio_h_sel", audio_h_parents,
		CK_CFG_11, CK_CFG_11_SET, CK_CFG_11_CLR,
		0, 2, INV_BIT,
		0x008, 13),

	MUX_CLR_SET_UPD(TOP_MUX_UFS, "ufs_sel", ufs_parents,
		CK_CFG_11, CK_CFG_11_SET, CK_CFG_11_CLR,
		8, 2, INV_BIT,
		0x008, 14),

	MUX_CLR_SET_UPD(TOP_MUX_AES_FDE, "aes_fde_sel", aes_fde_parents,
		CK_CFG_11, CK_CFG_11_SET, CK_CFG_11_CLR,
		16, 2, INV_BIT,
		0x008, 15),

	MUX_CLR_SET_UPD(TOP_MUX_ADSP, "adsp_sel", adsp_parents,
		CK_CFG_11, CK_CFG_11_SET, CK_CFG_11_CLR,
		24, 3, INV_BIT,
		0x008, 16),

	/* CLK_CFG_12 */
	MUX_CLR_SET_UPD(TOP_MUX_DVFSRC, "dvfsrc_sel", dvfsrc_parents,
		CK_CFG_12, CK_CFG_12_SET, CK_CFG_12_CLR,
		0, 1, INV_BIT,
		0x008, 17),

	MUX_CLR_SET_UPD(TOP_MUX_DSI_OCC, "dsi_occ_sel", dsi_occ_parents,
		CK_CFG_12, CK_CFG_12_SET, CK_CFG_12_CLR,
		8, 2, INV_BIT,
		0x008, 18),

	MUX_CLR_SET_UPD(TOP_MUX_SPMI_MST, "spmi_mst_sel", spmi_mst_parents,
		CK_CFG_12, CK_CFG_12_SET, CK_CFG_12_CLR,
		16, 3, INV_BIT,
		0x008, 19),
};

static const struct mtk_composite top_audmuxes[] __initconst = {

	MUX(TOP_I2S0_M_SEL, "i2s0_m_ck_sel", i2s0_m_ck_parents, 0x320, 8, 1),
	MUX(TOP_I2S1_M_SEL, "i2s1_m_ck_sel", i2s1_m_ck_parents, 0x320, 9, 1),
	MUX(TOP_I2S2_M_SEL, "i2s2_m_ck_sel", i2s2_m_ck_parents, 0x320, 10, 1),
	MUX(TOP_I2S3_M_SEL, "i2s3_m_ck_sel", i2s3_m_ck_parents, 0x320, 11, 1),
	MUX(TOP_I2S4_M_SEL, "i2s4_m_ck_sel", i2s4_m_ck_parents, 0x320, 12, 1),
	MUX(TOP_I2S5_M_SEL, "i2s5_m_ck_sel", i2s5_m_ck_parents, 0x328, 20, 1),

	DIV_GATE(TOP_APLL12_DIV0, "apll12_div0", "i2s0_m_ck_sel",
		0x320, 2, 0x324, 8, 0),
	DIV_GATE(TOP_APLL12_DIV1, "apll12_div1", "i2s1_m_ck_sel",
		0x320, 3, 0x324, 8, 8),
	DIV_GATE(TOP_APLL12_DIV2, "apll12_div2", "i2s2_m_ck_sel",
		0x320, 4, 0x324, 8, 16),
	DIV_GATE(TOP_APLL12_DIV3, "apll12_div3", "i2s3_m_ck_sel",
		0x320, 5, 0x324, 8, 24),

	DIV_GATE(TOP_APLL12_DIV4, "apll12_div4", "i2s4_m_ck_sel",
		0x320, 6, 0x328, 8, 0),
	DIV_GATE(TOP_APLL12_DIVB, "apll12_divb", "apll12_div4",
		0x320, 7, 0x328, 8, 8),
	DIV_GATE(TOP_APLL12_DIV5, "apll12_div5", "i2s5_m_ck_sel",
		0x328, 16, 0x328, 4, 28),
};

/* TODO: remove audio clocks after audio driver ready */

static int mtk_cg_bit_is_cleared(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 val = 0;

	regmap_read(cg->regmap, cg->sta_ofs, &val);

	val &= BIT(cg->bit);

	return val == 0;
}

static int mtk_cg_bit_is_set(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 val = 0;

	regmap_read(cg->regmap, cg->sta_ofs, &val);

	val &= BIT(cg->bit);

	return val != 0;
}

static void mtk_cg_set_bit(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);

	regmap_update_bits(cg->regmap, cg->sta_ofs, BIT(cg->bit), BIT(cg->bit));
}

static void mtk_cg_clr_bit(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);

	regmap_update_bits(cg->regmap, cg->sta_ofs, BIT(cg->bit), 0);
}

static int mtk_cg_enable(struct clk_hw *hw)
{
	mtk_cg_clr_bit(hw);

	return 0;
}



static void mtk_cg_disable(struct clk_hw *hw)
{
	/*struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);*/

	mtk_cg_set_bit(hw);
}

static int mtk_cg_bit_is_cleared_dummy(struct clk_hw *hw)
{
	return 0;
}

static int mtk_cg_enable_dummy(struct clk_hw *hw)
{
	return 0;
}

static void mtk_cg_disable_dummy(struct clk_hw *hw)
{
}

static int mtk_cg_enable_inv(struct clk_hw *hw)
{
	mtk_cg_set_bit(hw);

	return 0;
}

static void mtk_cg_disable_inv(struct clk_hw *hw)
{
	/*struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);*/

	mtk_cg_clr_bit(hw);
}

static void mtk_cg_disable_inv_dummy(struct clk_hw *hw)
{
}


const struct clk_ops mtk_clk_gate_ops = {
	.is_enabled	= mtk_cg_bit_is_cleared,
	.enable		= mtk_cg_enable,
	.disable	= mtk_cg_disable,
};

const struct clk_ops mtk_clk_gate_ops_dummy = {
	.is_enabled	= mtk_cg_bit_is_cleared,
	.enable		= mtk_cg_enable,
	.disable	= mtk_cg_disable_dummy,
};

const struct clk_ops mtk_clk_gate_ops_dummy_all = {
	.is_enabled	= mtk_cg_bit_is_cleared_dummy,
	.enable		= mtk_cg_enable_dummy,
	.disable	= mtk_cg_disable_dummy,
};

const struct clk_ops mtk_clk_gate_ops_inv = {
	.is_enabled	= mtk_cg_bit_is_set,
	.enable		= mtk_cg_enable_inv,
	.disable	= mtk_cg_disable_inv,
};

const struct clk_ops mtk_clk_gate_ops_inv_dummy = {
	.is_enabled	= mtk_cg_bit_is_set,
	.enable		= mtk_cg_enable_inv,
	.disable	= mtk_cg_disable_inv_dummy,
};

const struct clk_ops mtk_clk_gate_ops_setclr_inv_dummy = {
	.is_enabled	= mtk_cg_bit_is_set,
	.enable		= mtk_cg_enable_inv,
	.disable	= mtk_cg_disable_inv_dummy,
};


static const struct mtk_gate_regs infra0_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x84,
	.sta_ofs = 0x90,
};

static const struct mtk_gate_regs infra1_cg_regs = {
	.set_ofs = 0x88,
	.clr_ofs = 0x8c,
	.sta_ofs = 0x94,
};

static const struct mtk_gate_regs infra2_cg_regs = {
	.set_ofs = 0xa4,
	.clr_ofs = 0xa8,
	.sta_ofs = 0xac,
};

static const struct mtk_gate_regs infra3_cg_regs = {
	.set_ofs = 0xc0,
	.clr_ofs = 0xc4,
	.sta_ofs = 0xc8,
};

#define GATE_INFRA0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra0_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_INFRA1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra1_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_INFRA2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra2_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_INFRA3(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra3_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_INFRA0_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra0_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_dummy,		\
	}

#define GATE_INFRA1_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra1_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_dummy,		\
	}

#define GATE_INFRA2_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra2_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_dummy,		\
	}

#define GATE_INFRA3_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra3_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_dummy,		\
	}

static const struct mtk_gate infra_clks[] __initconst = {
	/* INFRA0 */
	//GATE_INFRA0(INFRACFG_AO_PMIC_CG_TMR, "infra_pmic_tmr",
	//"axi_sel", 0),
	//GATE_INFRA0(INFRACFG_AO_PMIC_CG_AP, "infra_pmic_ap",
	//	"axi_sel", 1),
	//GATE_INFRA0(INFRACFG_AO_PMIC_CG_MD, "infra_pmic_md",
	//	"axi_sel", 2),
	//GATE_INFRA0(INFRACFG_AO_PMIC_CG_CONN, "infra_pmic_conn",
	//	"axi_sel", 3),
	//GATE_INFRA0(INFRACFG_AO_SCPSYS_CG, "infra_scp",
	//	"axi_sel", 4),
	GATE_INFRA0(INFRACFG_AO_SEJ_CG, "infra_sej",
		"clk26m", 5),
	GATE_INFRA0(INFRACFG_AO_APXGPT_CG, "infra_apxgpt",
		"axi_sel", 6),
	GATE_INFRA0(INFRACFG_AO_ICUSB_CG, "infra_icusb",
		"axi_sel", 8),
	GATE_INFRA0(INFRACFG_AO_GCE_CG, "infra_gce",
		"axi_sel", 9),
	GATE_INFRA0(INFRACFG_AO_THERM_CG, "infra_therm",
		"axi_sel", 10),
	GATE_INFRA0(INFRACFG_AO_I2C_AP_CG, "infra_i2c_ap",
		"i2c_sel", 11),
	GATE_INFRA0(INFRACFG_AO_I2C_CCU_CG, "infra_i2c_ccu",
		"i2c_sel", 12),
	GATE_INFRA0(INFRACFG_AO_I2C_SSPM_CG, "infra_i2c_sspm",
		"i2c_sel", 13),
	GATE_INFRA0(INFRACFG_AO_I2C_RSV_CG, "infra_i2c_rsv",
		"i2c_sel", 14),
	GATE_INFRA0(INFRACFG_AO_PWM_HCLK_CG, "infra_pwm_hclk",
		"pwm_sel", 15),
	GATE_INFRA0(INFRACFG_AO_PWM1_CG, "infra_pwm1",
		"pwm_sel", 16),
	GATE_INFRA0(INFRACFG_AO_PWM2_CG, "infra_pwm2",
		"pwm_sel", 17),
	GATE_INFRA0(INFRACFG_AO_PWM3_CG, "infra_pwm3",
		"pwm_sel", 18),
	GATE_INFRA0(INFRACFG_AO_PWM4_CG, "infra_pwm4",
		"pwm_sel", 19),
	GATE_INFRA0(INFRACFG_AO_PWM5_CG, "infra_pwm5",
		"pwm_sel", 20),
	GATE_INFRA0(INFRACFG_AO_PWM_CG, "infra_pwm",
		"pwm_sel", 21),
	GATE_INFRA0(INFRACFG_AO_UART0_CG, "infra_uart0",
		"uart_sel", 22),
	GATE_INFRA0(INFRACFG_AO_UART1_CG, "infra_uart1",
		"uart_sel", 23),
	GATE_INFRA0(INFRACFG_AO_UART2_CG, "infra_uart2",
		"uart_sel", 24),
	GATE_INFRA0(INFRACFG_AO_UART3_CG, "infra_uart3",
		"uart_sel", 25),
	GATE_INFRA0(INFRACFG_AO_GCE_26M_CG, "infra_gce_26m",
		"axi_sel", 27),
	//GATE_INFRA0(INFRACFG_AO_CQ_DMA_FPC_CG, "infra_cqdma_fpc",
		//"axi_sel", 28),
	GATE_INFRA0(INFRACFG_AO_BTIF_CG, "infra_btif",
		"axi_sel", 31),
	/* INFRA1 */
	GATE_INFRA1(INFRACFG_AO_SPI0_CG, "infra_spi0",
		"spi_sel", 1),
	GATE_INFRA1(INFRACFG_AO_MSDC0_CG, "infra_msdc0",
		"msdc50_hclk_sel", 2),
	GATE_INFRA1(INFRACFG_AO_MSDCFDE_CG, "infra_msdcfde",
		"aes_msdcfde_sel", 3),
	GATE_INFRA1(INFRACFG_AO_MSDC1_CG, "infra_msdc1",
		"axi_sel", 4),
	GATE_INFRA1(INFRACFG_AO_MSDC2_CG, "infra_msdc2",
		"axi_sel", 5),
	GATE_INFRA1(INFRACFG_AO_MSDC0_SCK_CG, "infra_msdc0_sck",
		"msdc50_0_sel", 6),
	//GATE_INFRA1(INFRACFG_AO_DVFSRC_CG, "infra_dvfsrc",
	//	"clk26m", 7),
	//GATE_INFRA1(INFRACFG_AO_GCPU_CG, "infra_gcpu",
	//	"axi_sel", 8),
	GATE_INFRA1(INFRACFG_AO_TRNG_CG, "infra_trng",
		"axi_sel", 9),
	GATE_INFRA1(INFRACFG_AO_AUXADC_CG, "infra_auxadc",
		"clk26m", 10),
	GATE_INFRA1(INFRACFG_AO_CPUM_CG, "infra_cpum",
		"axi_sel", 11),
	GATE_INFRA1(INFRACFG_AO_CCIF1_AP_CG, "infra_ccif1_ap",
		"axi_sel", 12),
	GATE_INFRA1(INFRACFG_AO_CCIF1_MD_CG, "infra_ccif1_md",
		"axi_sel", 13),
	//GATE_INFRA1(INFRACFG_AO_AUXADC_MD_CG, "infra_auxadc_md",
	//	"clk26m", 14),
	GATE_INFRA1(INFRACFG_AO_MSDC1_SCK_CG, "infra_msdc1_sck",
		"msdc30_1_sel", 16),
	GATE_INFRA1(INFRACFG_AO_MSDC2_SCK_CG, "infra_msdc2_sck",
		"msdc30_2_sel", 17),
	GATE_INFRA1(INFRACFG_AO_AP_DMA_CG, "infra_apdma",
		"axi_sel", 18),
	//GATE_INFRA1(INFRACFG_AO_XIU_CG, "infra_xiu",
	//	"axi_sel", 19),
	GATE_INFRA1(INFRACFG_AO_DEVICE_APC_CG, "infra_devapc",
		"axi_sel", 20),
	GATE_INFRA1(INFRACFG_AO_CCIF_AP_CG, "infra_ccif_ap",
		"axi_sel", 23),
	//GATE_INFRA1(INFRACFG_AO_DEBUGSYS_CG, "infra_debugsys",
	//	"axi_sel", 24),
	GATE_INFRA1(INFRACFG_AO_AUDIO_CG, "infra_audio",
		"axi_sel", 25),
	GATE_INFRA1(INFRACFG_AO_CCIF_MD_CG, "infra_ccif_md",
		"axi_sel", 26),
	GATE_INFRA1(INFRACFG_AO_DXCC_SEC_CORE_CG, "infra_dxcc_sec_core",
		"dxcc_sel", 27),
	//GATE_INFRA1(INFRACFG_AO_DXCC_AO_CG, "infra_dxcc_ao",
	//	"dxcc_sel", 28),
	GATE_INFRA1(INFRACFG_AO_IMP_IIC_CG, "infra_imp_iic",
		"i2c_sel", 29),
	//GATE_INFRA1(INFRACFG_AO_DEVMPU_BCLK_CG, "infra_devmpu_bclk",
	//	"axi_sel", 30),
	//GATE_INFRA1(INFRACFG_AO_DRAMC_F26M_CG, "infra_dramc_f26m",
	//	"clk26m", 31),
	/* INFRA2 */
	GATE_INFRA2(INFRACFG_AO_PWM_BCLK6_CG, "infra_pwm_bclk6",
		"clk26m", 0),
	GATE_INFRA2(INFRACFG_AO_USB_CG, "infra_usb",
		"usb_top_sel", 1),
	GATE_INFRA2(INFRACFG_AO_DISP_PWM_CG, "infra_disppwm",
		"disppwm_sel", 2),
	GATE_INFRA2(INFRACFG_AO_CLDMA_BCLK_CG, "infra_cldma_bclk",
		"axi_sel", 3),
	GATE_INFRA2(INFRACFG_AO_AUDIO_26M_BCLK_CK,
		"infracfg_ao_audio_26m_bclk_ck", "clk26m", 4),
	GATE_INFRA2(INFRACFG_AO_SPI1_CG, "infra_spi1",
		"spi_sel", 6),
	GATE_INFRA2(INFRACFG_AO_I2C4_CG, "infra_i2c4",
		"i2c_sel", 7),
	GATE_INFRA2(INFRACFG_AO_MODEM_TEMP_SHARE_CG,
		"infra_md_tmp_share",
		"clk26m", 8),
	GATE_INFRA2(INFRACFG_AO_SPI2_CG, "infra_spi2",
		"spi_sel", 9),
	GATE_INFRA2(INFRACFG_AO_SPI3_CG, "infra_spi3",
		"spi_sel", 10),
	GATE_INFRA2(INFRACFG_AO_UNIPRO_SCK_CG, "infra_unipro_sck",
		"ufs_sel", 11),
	GATE_INFRA2(INFRACFG_AO_UNIPRO_TICK_CG, "infra_unipro_tick",
		"ufs_sel", 12),
	GATE_INFRA2(INFRACFG_AO_UFS_MP_SAP_BCLK_CG,
		"infra_ufs_mp_sap_bck",
		"ufs_sel", 13),
	GATE_INFRA2(INFRACFG_AO_MD32_BCLK_CG, "infra_md32_bclk",
		"axi_sel", 14),
	GATE_INFRA2(INFRACFG_AO_SSPM_CG, "infra_sspm",
		"sspm_sel", 15),
	GATE_INFRA2(INFRACFG_AO_UNIPRO_MBIST_CG, "infra_unipro_mbist",
		"axi_sel", 16),
	GATE_INFRA2(INFRACFG_AO_SSPM_BUS_HCLK_CG, "infra_sspm_bus_hclk",
		"axi_sel", 17),
	GATE_INFRA2(INFRACFG_AO_I2C5_CG, "infra_i2c5",
		"i2c_sel", 18),
	GATE_INFRA2(INFRACFG_AO_I2C5_ARBITER_CG, "infra_i2c5_arbiter",
		"i2c_sel", 19),
	GATE_INFRA2(INFRACFG_AO_I2C5_IMM_CG, "infra_i2c5_imm",
		"i2c_sel", 20),
	GATE_INFRA2(INFRACFG_AO_I2C1_ARBITER_CG, "infra_i2c1_arbiter",
		"i2c_sel", 21),
	GATE_INFRA2(INFRACFG_AO_I2C1_IMM_CG, "infra_i2c1_imm",
		"i2c_sel", 22),
	GATE_INFRA2(INFRACFG_AO_I2C2_ARBITER_CG, "infra_i2c2_arbiter",
		"i2c_sel", 23),
	GATE_INFRA2(INFRACFG_AO_I2C2_IMM_CG, "infra_i2c2_imm",
		"i2c_sel", 24),
	GATE_INFRA2(INFRACFG_AO_SPI4_CG, "infra_spi4",
		"spi_sel", 25),
	GATE_INFRA2(INFRACFG_AO_SPI5_CG, "infra_spi5",
		"spi_sel", 26),
	GATE_INFRA2(INFRACFG_AO_CQ_DMA_CG, "infra_cqdma",
		"axi_sel", 27),
	GATE_INFRA2(INFRACFG_AO_BIST2FPC_CG, "infra_bist2fpc",
		"unknown_sel", 28),
	GATE_INFRA2(INFRACFG_AO_AES_UFS_CG, "infra_aes_ufs",
		"aes_fde_sel", 29),
	GATE_INFRA2(INFRACFG_AO_UFS_CG, "infra_ufs",
		"ufs_sel", 30),
	GATE_INFRA2(INFRACFG_AO_UFS_TICK_CG, "infra_ufs_tick",
		"unknown_sel", 31),

	/* INFRA3 */
	GATE_INFRA3(INFRACFG_AO_MSDC0_SELF_CG, "infra_msdc0_self",
		"msdc50_0_sel", 0),
	GATE_INFRA3(INFRACFG_AO_MSDC1_SELF_CG, "infra_msdc1_self",
		"msdc50_0_sel", 1),
	GATE_INFRA3(INFRACFG_AO_MSDC2_SELF_CG, "infra_msdc2_self",
		"msdc50_0_sel", 2),
	GATE_INFRA3(INFRACFG_AO_SSPM_26M_SELF_CG, "infra_sspm_26m_self",
		"clk26m", 3),
	GATE_INFRA3(INFRACFG_AO_SSPM_32K_SELF_CG, "infra_sspm_32k_self",
		"clk26m", 4),
	GATE_INFRA3(INFRACFG_AO_UFS_AXI_CG, "infra_ufs_axi",
		"axi_sel", 5),
	GATE_INFRA3(INFRACFG_AO_I2C6_CG, "infra_i2c6",
		"i2c_sel", 6),
	GATE_INFRA3(INFRACFG_AO_AP_MSDC0_CG, "infra_ap_msdc0",
		"msdc50_hclk_sel", 7),
	GATE_INFRA3(INFRACFG_AO_MD_MSDC0_CG, "infra_md_msdc0",
		"msdc50_hclk_sel", 8),


	GATE_INFRA3(INFRACFG_AO_MSDC0_SRCLK_CG, "infra_msdc0_srclk",
		"msdc50_0_sel", 9),
	GATE_INFRA3(INFRACFG_AO_MSDC1_SRCLK_CG, "infra_msdc1_srclk",
		"msdc30_1_sel", 10),

	GATE_INFRA3(INFRACFG_AO_PWRAP_TMR_FO_CG, "infra_pwrap_tmr_fo",
		"axi_sel", 12),
	GATE_INFRA3(INFRACFG_AO_PWRAP_SPI_FO_CG, "infra_pwrap_spi_fo",
		"axi_sel", 13),
	GATE_INFRA3(INFRACFG_AO_PWRAP_SYS_FO_CG, "infra_pwrap_sys_fo",
		"axi_sel", 14),

	GATE_INFRA3(INFRACFG_AO_SEJ_F13M_CG, "infra_sej_f13m",
		"clk26m", 15),
	GATE_INFRA3(INFRACFG_AO_AES_TOP0_BCLK_CG, "infra_aes_top0_bclk",
		"clk26m", 16),
	GATE_INFRA3(INFRACFG_AO_MCUPM_BCLK_CG, "infra_mcupm_bclk",
		"clk26m", 17),

	GATE_INFRA3(INFRACFG_AO_CCIF2_AP_CG, "infra_ccif2_ap",
		"axi_sel", 18),
	GATE_INFRA3(INFRACFG_AO_CCIF2_MD_CG, "infra_ccif2_md",
		"axi_sel", 19),
	GATE_INFRA3(INFRACFG_AO_CCIF3_AP_CG, "infra_ccif3_ap",
		"axi_sel", 20),
	GATE_INFRA3(INFRACFG_AO_CCIF3_MD_CG, "infra_ccif3_md",
		"axi_sel", 21),


	GATE_INFRA3(INFRACFG_AO_FADSP_26M_CG, "infra_fadsp_26m",
		"adsp_sel", 22),
	GATE_INFRA3(INFRACFG_AO_FADSP_32K_CG, "infra_fadsp_32k",
		"adsp_sel", 23),

	GATE_INFRA3(INFRACFG_AO_CCIF4_AP_CG, "infra_ccif4_ap",
		"axi_sel", 24),
	GATE_INFRA3(INFRACFG_AO_CCIF4_MD_CG, "infra_ccif4_md",
		"axi_sel", 25),

	GATE_INFRA3(INFRACFG_AO_DPMAIF_CK, "infra_dpmaif",
		"dpmaif_sel", 26),
	GATE_INFRA3(INFRACFG_AO_FADSP_CG, "infra_fadsp",
		"adsp_sel", 27),
};

static const struct mtk_gate_regs mfg_cfg_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_MFG_CFG(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mfg_cfg_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MFG_CFG_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mfg_cfg_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_dummy,		\
	}

static const struct mtk_gate mfg_cfg_clks[] __initconst = {
#if MT_CCF_BRINGUP
	GATE_MFG_CFG_DUMMY(MFGCFG_BG3D, "mfg_cfg_bg3d", "mfg_sel", 0)
#else
	GATE_MFG_CFG(MFGCFG_BG3D, "mfg_cfg_bg3d", "mfg_sel", 0)
#endif
};

static const struct mtk_gate_regs apmixed_cg_regs = {
	.set_ofs = 0x20,
	.clr_ofs = 0x20,
	.sta_ofs = 0x20,
};

#define GATE_APMIXED(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &apmixed_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_inv,		\
	}

#define GATE_APMIXED_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &apmixed_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_inv_dummy,	\
	}

static const struct mtk_gate apmixed_clks[] __initconst = {
	GATE_APMIXED(APMIXED_SSUSB26M, "apmixed_ssusb26m", "clk26m",
		4),
	GATE_APMIXED(APMIXED_APPLL26M, "apmixed_appll26m", "clk26m",
		5),
	GATE_APMIXED(APMIXED_MIPIC0_26M, "apmixed_mipic026m", "clk26m",
		6),
	GATE_APMIXED(APMIXED_MDPLLGP26M, "apmixed_mdpll26m", "clk26m",
		7),
	GATE_APMIXED(APMIXED_MMSYS_F26M, "apmixed_mmsys26m", "clk26m",
		8),
	GATE_APMIXED(APMIXED_UFS26M, "apmixed_ufs26m", "clk26m",
		9),
	GATE_APMIXED(APMIXED_MIPIC1_26M, "apmixed_mipic126m", "clk26m",
		11),
	GATE_APMIXED(APMIXED_MEMPLL26M, "apmixed_mempll26m", "clk26m",
		13),
	GATE_APMIXED(APMIXED_CLKSQ_LVPLL_26M, "apmixed_lvpll26m", "clk26m",
		14),
	GATE_APMIXED(APMIXED_MIPID0_26M, "apmixed_mipid026m", "clk26m",
		16),
	GATE_APMIXED(APMIXED_MIPID1_26M, "apmixed_mipid126m", "clk26m",
		17),
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

static const struct mtk_gate_regs audio2_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0x8,
	.sta_ofs = 0x8,
};

#define GATE_AUDIO0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &audio0_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops,		\
	}

#define GATE_AUDIO1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &audio1_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops,		\
	}

#define GATE_AUDIO2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &audio2_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops,		\
	}

#define GATE_AUDIO0_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &audio0_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_dummy,		\
	}

#define GATE_AUDIO1_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &audio1_cg_regs,		\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_dummy,		\
	}

static const struct mtk_gate audio_clks[] __initconst = {
	/* AUDIO0 */
	GATE_AUDIO0(AUDIO_AFE, "aud_afe", "audio_sel",
		2),
	GATE_AUDIO0(AUDIO_22M, "aud_22m", "aud_eng1_sel",
		8),
	GATE_AUDIO0(AUDIO_24M, "aud_24m", "aud_eng2_sel",
		9),
	GATE_AUDIO0(AUDIO_APLL2_TUNER, "aud_apll2_tuner", "aud_eng2_sel",
		18),
	GATE_AUDIO0(AUDIO_APLL_TUNER, "aud_apll_tuner", "aud_eng1_sel",
		19),
	GATE_AUDIO0(AUDIO_TDM, "aud_tdm", "aud_eng1_sel",
		20),
	GATE_AUDIO0(AUDIO_ADC, "aud_adc", "audio_sel",
		24),
	GATE_AUDIO0(AUDIO_DAC, "aud_dac", "audio_sel",
		25),
	GATE_AUDIO0(AUDIO_DAC_PREDIS, "aud_dac_predis", "audio_sel",
		26),
	GATE_AUDIO0(AUDIO_TML, "aud_tml", "audio_sel",
		27),
	GATE_AUDIO0(AUDIO_NLE, "aud_nle", "audio_sel",
		28),
	/* AUDIO1: hf_faudio_ck/hf_faud_engen1_ck/hf_faud_engen2_ck */
	GATE_AUDIO1(AUDIO_I2S1_BCLK_SW, "aud_i2s1_bclk", "audio_sel",
		4),
	GATE_AUDIO1(AUDIO_I2S2_BCLK_SW, "aud_i2s2_bclk", "audio_sel",
		5),
	GATE_AUDIO1(AUDIO_I2S3_BCLK_SW, "aud_i2s3_bclk", "audio_sel",
		6),
	GATE_AUDIO1(AUDIO_I2S4_BCLK_SW, "aud_i2s4_bclk", "audio_sel",
		7),
	GATE_AUDIO1(AUDIO_I2S5_BCLK_SW, "aud_i2s5_bclk", "audio_sel",
		8),

	GATE_AUDIO1(AUDIO_CONN_I2S_ASRC, "aud_conn_i2s", "audio_sel",
		12),
	GATE_AUDIO1(AUDIO_GENERAL1_ASRC, "aud_general1", "audio_sel",
		13),
	GATE_AUDIO1(AUDIO_GENERAL2_ASRC, "aud_general2", "audio_sel",
		14),
	GATE_AUDIO1(AUDIO_DAC_HIRES, "aud_dac_hires", "audio_h_sel",
		15),
	GATE_AUDIO1(AUDIO_ADC_HIRES, "aud_adc_hires", "audio_h_sel",
		16),
	GATE_AUDIO1(AUDIO_ADC_HIRES_TML, "aud_adc_hires_tml", "audio_h_sel",
		17),

	GATE_AUDIO1(AUDIO_PDN_ADDA6_ADC, "aud_pdn_adda6_adc", "audio_sel",
		20),
	GATE_AUDIO1(AUDIO_ADDA6_ADC_HIRES, "aud_adda6_adc_hires", "audio_h_sel",
		21),
	GATE_AUDIO1(AUDIO_3RD_DAC, "aud_3rd_dac", "audio_sel",
		28),
	GATE_AUDIO1(AUDIO_3RD_DAC_PREDIS, "aud_3rd_dac_predis", "audio_sel",
		29),
	GATE_AUDIO1(AUDIO_3RD_DAC_TML, "aud_3rd_dac_tml", "audio_sel",
		30),
	GATE_AUDIO1(AUDIO_3RD_DAC_HIRES, "aud_3rd_dac_hires", "audio_h_sel",
		31),
	/* AUDIO2 */
	GATE_AUDIO2(AUDIO_ETDM_IN1_BCLK_SW, "aud_etdm_in1_bclk", "audio_sel",
		4),
	GATE_AUDIO2(AUDIO_ETDM_OUT1_BCLK_SW, "aud_etdm_out1_bclk", "audio_sel",
		5),
};

static const struct mtk_gate_regs cam_m_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_M(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_m_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}


#define GATE_CAM_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &cam_m_cg_regs,			\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_dummy,	\
	}

static const struct mtk_gate cam_m_clks[] = {
	GATE_CAM_DUMMY(CLK_CAM_M_LARB13, "cam_m_larb13",
			"cam_sel"/* parent */, 0),
	GATE_CAM_M(CLK_CAM_M_DFP_VAD, "cam_m_dfpvad",
			"cam_sel"/* parent */, 1),
	GATE_CAM_DUMMY(CLK_CAM_M_LARB14, "cam_m_larb14",
			"cam_sel"/* parent */, 2),
	GATE_CAM_M(CLK_CAM_M_RESERVED0, "cam_m_reserved0",
			"cam_sel"/* parent */, 3),
	GATE_CAM_DUMMY(CLK_CAM_M_CAM, "cam_m_cam",
			"cam_sel"/* parent */, 6),
	GATE_CAM_M(CLK_CAM_M_CAMTG, "cam_m_camtg",
			"cam_sel"/* parent */, 7),
	GATE_CAM_M(CLK_CAM_M_SENINF, "cam_m_seninf",
			"cam_sel"/* parent */, 8),
	GATE_CAM_M(CLK_CAM_M_CAMSV1, "cam_m_camsv1",
			"cam_sel"/* parent */, 10),
	GATE_CAM_M(CLK_CAM_M_CAMSV2, "cam_m_camsv2",
			"cam_sel"/* parent */, 11),
	GATE_CAM_M(CLK_CAM_M_CAMSV3, "cam_m_camsv3",
			"cam_sel"/* parent */, 12),
	GATE_CAM_M(CLK_CAM_M_CCU0, "cam_m_ccu0",
			"cam_sel"/* parent */, 13),
	GATE_CAM_M(CLK_CAM_M_CCU1, "cam_m_ccu1",
			"cam_sel"/* parent */, 14),
	GATE_CAM_M(CLK_CAM_M_MRAW0, "cam_m_mraw0",
			"cam_sel"/* parent */, 15),
	GATE_CAM_M(CLK_CAM_M_RESERVED2, "cam_m_reserved2",
			"cam_sel"/* parent */, 16),
	GATE_CAM_M(CLK_CAM_M_FAKE_ENG, "cam_m_fake_eng",
			"cam_sel"/* parent */, 17),
	GATE_CAM_DUMMY(CLK_CAM_M_CCU_GALS, "cam_m_ccu_gals",
			"cam_sel"/* parent */, 18),
	GATE_CAM_DUMMY(CLK_CAM_M_CAM2MM_GALS, "cam_m_cam2mm_gals",
			"cam_sel"/* parent */, 19),
};

static const struct mtk_gate_regs cam_ra_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_RA(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_ra_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_RA_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_ra_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_dummy,	\
	}

static const struct mtk_gate cam_ra_clks[] = {
	GATE_CAM_RA_DUMMY(CLK_CAM_RA_LARBX, "cam_ra_larbx",
			"cam_sel"/* parent */, 0),
	GATE_CAM_RA_DUMMY(CLK_CAM_RA_CAM, "cam_ra_cam",
			"cam_sel"/* parent */, 1),
	GATE_CAM_RA(CLK_CAM_RA_CAMTG, "cam_ra_camtg",
			"cam_sel"/* parent */, 2),
};

static const struct mtk_gate_regs cam_rb_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_RB(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_rb_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_RB_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_rb_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_dummy,	\
	}

static const struct mtk_gate cam_rb_clks[] = {
	GATE_CAM_RB_DUMMY(CLK_CAM_RB_LARBX, "cam_rb_larbx",
			"cam_sel"/* parent */, 0),
	GATE_CAM_RB_DUMMY(CLK_CAM_RB_CAM, "cam_rb_cam",
			"cam_sel"/* parent */, 1),
	GATE_CAM_RB(CLK_CAM_RB_CAMTG, "cam_rb_camtg",
			"cam_sel"/* parent */, 2),
};

static const struct mtk_gate_regs imgsys1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IMGSYS1(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &imgsys1_cg_regs,		\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_setclr,\
	}

#define GATE_IMGSYS1_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &imgsys1_cg_regs,		\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_dummy,	\
	}

static const struct mtk_gate imgsys1_clks[] __initconst = {
	GATE_IMGSYS1_DUMMY(CLK_IMGSYS1_LARB9, "imgsys1_larb9",
			"img1_sel"/* parent */, 0),
	GATE_IMGSYS1_DUMMY(CLK_IMGSYS1_LARB10, "imgsys1_larb10",
			"img1_sel"/* parent */, 1),
	GATE_IMGSYS1(CLK_IMGSYS1_DIP, "imgsys1_dip",
			"img1_sel"/* parent */, 2),
	GATE_IMGSYS1_DUMMY(CLK_IMGSYS1_GALS, "imgsys1_gals",
			"img1_sel"/* parent */, 12),
};

static const struct mtk_gate_regs imgsys2_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IMGSYS2(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &imgsys2_cg_regs,		\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_setclr,\
	}

#define GATE_IMGSYS2_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &imgsys2_cg_regs,		\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_dummy,	\
	}

static const struct mtk_gate imgsys2_clks[] __initconst = {
	GATE_IMGSYS2_DUMMY(CLK_IMGSYS2_LARB9, "imgsys2_larb9",
			"img1_sel"/* parent */, 0),
	GATE_IMGSYS2_DUMMY(CLK_IMGSYS2_LARB10, "imgsys2_larb10",
			"img1_sel"/* parent */, 1),
	GATE_IMGSYS2(CLK_IMGSYS2_MFB, "imgsys2_mfb",
			"img1_sel"/* parent */, 6),
	GATE_IMGSYS2(CLK_IMGSYS2_WPE, "imgsys2_wpe",
			"img1_sel"/* parent */, 7),
	GATE_IMGSYS2(CLK_IMGSYS2_MSS, "imgsys2_mss",
			"img1_sel"/* parent */, 8),
	GATE_IMGSYS2_DUMMY(CLK_IMGSYS2_GALS, "imgsys2_gals",
			"img1_sel"/* parent */, 12),
};

static const struct mtk_gate_regs ipe_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IPE(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ipe_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IPE_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ipe_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_dummy,	\
	}

static const struct mtk_gate ipe_clks[] = {
	GATE_IPE_DUMMY(CLK_IPE_LARB19, "ipe_larb19",
			"ipe_sel"/* parent */, 0),
	GATE_IPE_DUMMY(CLK_IPE_LARB20, "ipe_larb20",
			"ipe_sel"/* parent */, 1),
	GATE_IPE_DUMMY(CLK_IPE_SMI_SUBCOM, "ipe_smi_subcom",
			"ipe_sel"/* parent */, 2),
	GATE_IPE(CLK_IPE_FD, "ipe_fd",
			"ipe_sel"/* parent */, 3),
	GATE_IPE(CLK_IPE_FE, "ipe_fe",
			"ipe_sel"/* parent */, 4),
	GATE_IPE(CLK_IPE_RSC, "ipe_rsc",
			"ipe_sel"/* parent */, 5),
	GATE_IPE(CLK_IPE_DPE, "ipe_dpe",
			"ipe_sel"/* parent */, 6),
	GATE_IPE_DUMMY(CLK_IPE_GALS, "ipe_gals",
			"img1_sel"/* parent */, 8),
};


static const struct mtk_gate_regs mm0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mm1_cg_regs = {
	.set_ofs = 0x1a4,
	.clr_ofs = 0x1a8,
	.sta_ofs = 0x1a0,
};

#define GATE_MM0(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &mm0_cg_regs,		\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MM0_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &mm0_cg_regs,		\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_dummy,	\
	}

#define GATE_MM1(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &mm1_cg_regs,		\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MM1_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &mm1_cg_regs,		\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_dummy,	\
	}

static const struct mtk_gate mm_clks[] __initconst = {
	/* MM0 */
	GATE_MM0(CLK_MM_DISP_MUTEX0, "mm_disp_mutex0",
			"disp_sel"/* parent */, 0),
	GATE_MM0(CLK_MM_APB_BUS, "mm_apb_bus",
			"disp_sel"/* parent */, 1),
	GATE_MM0(CLK_MM_DISP_OVL0, "mm_disp_ovl0",
			"disp_sel"/* parent */, 2),
	GATE_MM0(CLK_MM_DISP_RDMA0, "mm_disp_rdma0",
			"disp_sel"/* parent */, 3),
	GATE_MM0(CLK_MM_DISP_OVL0_2L, "mm_disp_ovl0_2l",
			"disp_sel"/* parent */, 4),
	GATE_MM0(CLK_MM_DISP_WDMA0, "mm_disp_wdma0",
			"disp_sel"/* parent */, 5),
	GATE_MM0(CLK_MM_DISP_CCORR1, "mm_disp_ccorr1",
			"disp_sel"/* parent */, 6),
	GATE_MM0(CLK_MM_DISP_RSZ0, "mm_disp_rsz0",
			"disp_sel"/* parent */, 7),
	GATE_MM0(CLK_MM_DISP_AAL0, "mm_disp_aal0",
			"disp_sel"/* parent */, 8),
	GATE_MM0(CLK_MM_DISP_CCORR0, "mm_disp_ccorr0",
			"disp_sel"/* parent */, 9),
	GATE_MM0(CLK_MM_DISP_COLOR0, "mm_disp_color0",
			"disp_sel"/* parent */, 10),
	GATE_MM0_DUMMY(CLK_MM_SMI_INFRA, "mm_smi_infra",
			"disp_sel"/* parent */, 11),
	GATE_MM0(CLK_MM_DISP_DSC_WRAP, "mm_disp_dsc_wrap",
			"disp_sel"/* parent */, 12),
	GATE_MM0(CLK_MM_DISP_GAMMA0, "mm_disp_gamma0",
			"disp_sel"/* parent */, 13),
	GATE_MM0(CLK_MM_DISP_POSTMASK0, "mm_disp_postmask0",
			"disp_sel"/* parent */, 14),
	GATE_MM0(CLK_MM_DISP_SPR0, "mm_disp_spr0",
			"disp_sel"/* parent */, 15),
	GATE_MM0(CLK_MM_DISP_DITHER0, "mm_disp_dither0",
			"disp_sel"/* parent */, 16),
	GATE_MM0_DUMMY(CLK_MM_SMI_COMMON, "mm_smi_common",
			"disp_sel"/* parent */, 17),
	GATE_MM0(CLK_MM_DISP_CM0, "mm_disp_cm0",
			"disp_sel"/* parent */, 18),
	GATE_MM0(CLK_MM_DSI0, "mm_dsi0",
			"disp_sel"/* parent */, 19),
	GATE_MM0(CLK_MM_DISP_FAKE_ENG0, "mm_disp_fake_eng0",
			"disp_sel"/* parent */, 20),
	GATE_MM0(CLK_MM_DISP_FAKE_ENG1, "mm_disp_fake_eng1",
			"disp_sel"/* parent */, 21),
	GATE_MM0_DUMMY(CLK_MM_SMI_GALS, "mm_smi_gals",
			"disp_sel"/* parent */, 22),
	GATE_MM0_DUMMY(CLK_MM_SMI_IOMMU, "mm_smi_iommu",
			"disp_sel"/* parent */, 24),
	/* MM1 */
	GATE_MM1(CLK_MM_DSI0_DSI_CK_DOMAIN, "mm_dsi0_dsi_domain",
			"disp_sel"/* parent */, 0),
	GATE_MM1(CLK_MM_DISP_26M, "mm_disp_26m_ck",
			"disp_sel"/* parent */, 10),
};

static const struct mtk_gate_regs mdp0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mdp1_cg_regs = {
	.set_ofs = 0x124,
	.clr_ofs = 0x128,
	.sta_ofs = 0x120,
};

#define GATE_MDP0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mdp0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MDP0_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mdp0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_dummy,	\
	}

#define GATE_MDP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mdp1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate mdp_clks[] = {
	/* MDP0 */
	GATE_MDP0(CLK_MDP_RDMA0, "mdp_rdma0",
			"mdp_sel"/* parent */, 0),
	GATE_MDP0(CLK_MDP_TDSHP0, "mdp_tdshp0",
			"mdp_sel"/* parent */, 1),
	GATE_MDP0(CLK_MDP_IMG_DL_ASYNC0, "mdp_img_dl_async0",
			"mdp_sel"/* parent */, 2),
	GATE_MDP0(CLK_MDP_IMG_DL_ASYNC1, "mdp_img_dl_async1",
			"mdp_sel"/* parent */, 3),
	GATE_MDP0(CLK_MDP_RDMA1, "mdp_rdma1",
			"mdp_sel"/* parent */, 4),
	GATE_MDP0(CLK_MDP_TDSHP1, "mdp_tdshp1",
			"mdp_sel"/* parent */, 5),
	GATE_MDP0_DUMMY(CLK_MDP_SMI0, "mdp_smi0",
			"mdp_sel"/* parent */, 6),
	GATE_MDP0(CLK_MDP_APB_BUS, "mdp_apb_bus",
			"mdp_sel"/* parent */, 7),
	GATE_MDP0(CLK_MDP_WROT0, "mdp_wrot0",
			"mdp_sel"/* parent */, 8),
	GATE_MDP0(CLK_MDP_RSZ0, "mdp_rsz0",
			"mdp_sel"/* parent */, 9),
	GATE_MDP0(CLK_MDP_HDR0, "mdp_hdr0",
			"mdp_sel"/* parent */, 10),
	GATE_MDP0(CLK_MDP_MUTEX0, "mdp_mutex0",
			"mdp_sel"/* parent */, 11),
	GATE_MDP0(CLK_MDP_WROT1, "mdp_wrot1",
			"mdp_sel"/* parent */, 12),
	GATE_MDP0(CLK_MDP_RSZ1, "mdp_rsz1",
			"mdp_sel"/* parent */, 13),
	GATE_MDP0(CLK_MDP_FAKE_ENG0, "mdp_fake_eng0",
			"mdp_sel"/* parent */, 14),
	GATE_MDP0(CLK_MDP_AAL0, "mdp_aal0",
			"mdp_sel"/* parent */, 15),
	GATE_MDP0(CLK_MDP_AAL1, "mdp_aal1",
			"mdp_sel"/* parent */, 16),
	GATE_MDP0(CLK_MDP_COLOR0, "mdp_color0",
			"mdp_sel"/* parent */, 17),
	/* MDP1 */
	GATE_MDP1(CLK_MDP_IMG_DL_RELAY0_ASYNC0, "mdp_img_dl_rel0_as0",
			"mdp_sel"/* parent */, 0),
	GATE_MDP1(CLK_MDP_IMG_DL_RELAY1_ASYNC1, "mdp_img_dl_rel1_as1",
			"mdp_sel"/* parent */, 8),
};

static const struct mtk_gate_regs vdec0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs vdec1_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xc,
	.sta_ofs = 0x8,
};

static const struct mtk_gate_regs vdec2_cg_regs = {
	.set_ofs = 0x200,
	.clr_ofs = 0x204,
	.sta_ofs = 0x200,
};

#define GATE_VDEC0_I(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vdec0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDEC1_I(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vdec1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDEC2_I(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vdec2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDEC0_I_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vdec0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv_dummy,	\
	}

#define GATE_VDEC1_I_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vdec1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv_dummy,	\
	}

#define GATE_VDEC2_I_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vdec2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv_dummy,	\
	}

static const struct mtk_gate vdec_clks[] __initconst = {
#if 1//MT_CCF_BRINGUP
	/* VDEC0 */
	GATE_VDEC0_I_DUMMY(VDEC_VDEC, "vdec_cken", "vdec_sel", 0),
	/* VDEC1 */
	GATE_VDEC1_I_DUMMY(VDEC_LARB1, "vdec_larb1_cken", "vdec_sel", 0),
	/* VDEC2 */
	GATE_VDEC2_I_DUMMY(VDEC_LAT, "vdec_lat_cken", "vdec_sel", 0),
#else
	/* VDEC0 */
	GATE_VDEC0_I(VDEC_VDEC, "vdec_cken", "vdec_sel", 0),
	/* VDEC1 */
	GATE_VDEC1_I(VDEC_LARB1, "vdec_larb1_cken", "vdec_sel", 0),
	/* VDEC2 */
	GATE_VDEC2_I(VDEC_LAT, "vdec_lat_cken", "vdec_sel", 0),
#endif
};

static const struct mtk_gate_regs venc_global_con_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_VENC_GLOBAL_CON(_id, _name, _parent, _shift) {	\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &venc_global_con_cg_regs,		\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_inv,		\
	}

#define GATE_VENC_GLOBAL_CON_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &venc_global_con_cg_regs,		\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_inv_dummy,	\
	}

static const struct mtk_gate venc_global_con_clks[] __initconst = {
#if 1//MT_CCF_BRINGUP
	GATE_VENC_GLOBAL_CON_DUMMY(VENC_GCON_LARB, "venc_larb",
		"venc_sel", 0),
	GATE_VENC_GLOBAL_CON_DUMMY(VENC_GCON_VENC, "venc_venc",
		"venc_sel", 4),
	GATE_VENC_GLOBAL_CON_DUMMY(VENC_GCON_JPGENC, "venc_jpgenc",
		"venc_sel", 8),
	GATE_VENC_GLOBAL_CON_DUMMY(VENC_GCON_GALS, "venc_gals",
		"venc_sel", 28),
#else
	GATE_VENC_GLOBAL_CON_DUMMY(VENC_GCON_LARB, "venc_larb",
		"venc_sel", 0),
	GATE_VENC_GLOBAL_CON_DUMMY(VENC_GCON_VENC, "venc_venc",
		"venc_sel", 4),
	GATE_VENC_GLOBAL_CON_DUMMY(VENC_GCON_JPGENC, "venc_jpgenc",
		"venc_sel", 8),
	GATE_VENC_GLOBAL_CON_DUMMY(VENC_GCON_GALS, "venc_gals",
		"venc_sel", 28),
#endif
};

static const struct mtk_gate_regs imp_cg_regs = {
	.set_ofs = 0xe08,
	.clr_ofs = 0xe04,
	.sta_ofs = 0xe00,
};

#define GATE_IMP(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &imp_cg_regs,		\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_setclr,\
	}

#define GATE_IMP_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &imp_cg_regs,		\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_dummy,	\
	}

static const struct mtk_gate imp_clks[] __initconst = {
	GATE_IMP(CLK_IMP_AP_CLOCK_RO_I2C0, "imp_ap_i2c0",
			"infra_imp_iic"/* parent */, 0),
	GATE_IMP(CLK_IMP_AP_CLOCK_RO_I2C1, "imp_ap_i2c1",
			"infra_imp_iic"/* parent */, 1),
	GATE_IMP(CLK_IMP_AP_CLOCK_RO_I2C2, "imp_ap_i2c2",
			"infra_imp_iic"/* parent */, 2),
	GATE_IMP(CLK_IMP_AP_CLOCK_RO_I2C3, "imp_ap_i2c3",
			"infra_imp_iic"/* parent */, 3),
	GATE_IMP(CLK_IMP_AP_CLOCK_RO_I2C4, "imp_ap_i2c4",
			"infra_imp_iic"/* parent */, 4),
	GATE_IMP(CLK_IMP_AP_CLOCK_RO_I2C5, "imp_ap_i2c5",
			"infra_imp_iic"/* parent */, 5),
	GATE_IMP(CLK_IMP_AP_CLOCK_RO_I2C6, "imp_ap_i2c6",
			"infra_imp_iic"/* parent */, 6),
	GATE_IMP(CLK_IMP_AP_CLOCK_RO_I2C7, "imp_ap_i2c7",
			"infra_imp_iic"/* parent */, 7),
	GATE_IMP(CLK_IMP_AP_CLOCK_RO_I2C8, "imp_ap_i2c8",
			"infra_imp_iic"/* parent */, 8),
	GATE_IMP(CLK_IMP_AP_CLOCK_RO_I2C9, "imp_ap_i2c9",
			"infra_imp_iic"/* parent */, 9),
};

static void __init mtk_topckgen_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return;
	}

	clk_data = mtk_alloc_clk_data(TOP_NR_CLK);

	mtk_clk_register_fixed_clks(fixed_clks,
		ARRAY_SIZE(fixed_clks), clk_data);

	mtk_clk_register_factors(top_divs, ARRAY_SIZE(top_divs), clk_data);
	mtk_clk_register_muxes(top_muxes,
		ARRAY_SIZE(top_muxes), node, &mt6781_clk_lock, clk_data);
	mtk_clk_register_composites(top_audmuxes, ARRAY_SIZE(top_audmuxes),
		base, &mt6781_clk_lock, clk_data);
	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	cksys_base = base;

	/* Need Confirm */
	clk_writel(CLK_SCP_CFG_0, clk_readl(CLK_SCP_CFG_0) | 0x3FF);
	clk_writel(CLK_SCP_CFG_1, clk_readl(CLK_SCP_CFG_1) | 0x11);

#if 0
	clk_writel(cksys_base + CK_CFG_0_CLR, 0x80000000);
	clk_writel(cksys_base + CK_CFG_0_SET, 0x80000000);

	clk_writel(cksys_base + CK_CFG_1_CLR, 0x80808080);
	clk_writel(cksys_base + CK_CFG_1_SET, 0x80808080);

	clk_writel(cksys_base + CK_CFG_2_CLR, 0x80808080);
	clk_writel(cksys_base + CK_CFG_2_SET, 0x80808080);

	clk_writel(cksys_base + CK_CFG_3_CLR, 0x80808080);
	clk_writel(cksys_base + CK_CFG_3_SET, 0x80808080);

	clk_writel(cksys_base + CK_CFG_4_CLR, 0x80808080);
	clk_writel(cksys_base + CK_CFG_4_SET, 0x80808080);

	clk_writel(cksys_base + CK_CFG_5_CLR, 0x80808080);
	clk_writel(cksys_base + CK_CFG_5_SET, 0x80808080);

	clk_writel(cksys_base + CK_CFG_6_CLR, 0x00808080);/*pwrap_ulposc*/
	clk_writel(cksys_base + CK_CFG_6_SET, 0x00808080);

	clk_writel(cksys_base + CK_CFG_7_CLR, 0x80800080);/*sspm*/
	clk_writel(cksys_base + CK_CFG_7_SET, 0x80800080);

	clk_writel(cksys_base + CK_CFG_8_CLR, 0x00808080);/*spm*/
	clk_writel(cksys_base + CK_CFG_8_SET, 0x00808080);

	clk_writel(cksys_base + CK_CFG_9_CLR, 0x80808080);
	clk_writel(cksys_base + CK_CFG_9_SET, 0x80808080);

	clk_writel(cksys_base + CK_CFG_10_CLR, 0x80808080);
	clk_writel(cksys_base + CK_CFG_10_SET, 0x80808000);/*dxcc*/

	clk_writel(cksys_base + CK_CFG_11_CLR, 0x80808080);
	clk_writel(cksys_base + CK_CFG_11_SET, 0x80808080);

	clk_writel(cksys_base + CK_CFG_12_CLR, 0x80808080);
	clk_writel(cksys_base + CK_CFG_12_SET, 0x80808080);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_topckgen, "mediatek,topckgen", mtk_topckgen_init);

static void __init mtk_infracfg_ao_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return;
	}

	clk_data = mtk_alloc_clk_data(INFRACFG_AO_NR_CLK);

	mtk_clk_register_gates(node, infra_clks,
		ARRAY_SIZE(infra_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	infracfg_base = base;
	#if 1
	/* Need Confirm */
	clk_writel(INFRA_TOPAXI_SI0_CTL, clk_readl(INFRA_TOPAXI_SI0_CTL) | 0x2);
	pr_notice("%s: infra mfg debug: %08x\n",
			__func__, clk_readl(INFRA_TOPAXI_SI0_CTL));
	#endif
	/*mtk_clk_enable_critical();*/
#if MT_CCF_BRINGUP
#else
	//clk_writel(INFRA_PDN_SET0, INFRA_CG0);
	//clk_writel(INFRA_PDN_SET1, INFRA_CG1);
	//clk_writel(INFRA_PDN_SET2, INFRA_CG2);
	//clk_writel(INFRA_PDN_SET3, INFRA_CG3);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_infracfg_ao, "mediatek,infracfg_ao",
		mtk_infracfg_ao_init);

/* FIXME: modify FMAX */
#define MT6781_PLL_FMAX		(3800UL * MHZ)
#define MT6781_PLL_FMIN		(1500UL * MHZ)
#define MT6781_INTEGER_BITS		8

/*#define CON0_MT6781_RST_BAR	BIT(24), not fixed */

#define PLL_B(_id, _name, _reg, _pwr_reg, _en_mask, _flags,	\
			_rst_bar_mask, _pcwbits, _pd_reg, _pd_shift,	\
			_tuner_reg, _tuner_en_reg, _tuner_en_bit,	\
			_pcw_reg, _pcw_shift, _div_table) {		\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.pwr_reg = _pwr_reg,					\
		.en_mask = _en_mask,					\
		.flags = _flags,					\
		.rst_bar_mask = _rst_bar_mask,				\
		.fmax = MT6781_PLL_FMAX,				\
		.fmin = MT6781_PLL_FMIN,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6781_INTEGER_BITS,			\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,				\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.div_table = _div_table,				\
	}

#define PLL(_id, _name, _reg, _pwr_reg, _en_mask,			\
			_flags, _rst_bar_mask,				\
			_pcwbits,					\
			_pd_reg, _pd_shift,				\
			_tuner_reg, _tuner_en_reg, _tuner_en_bit,	\
			_pcw_reg, _pcw_shift)				\
		PLL_B(_id, _name, _reg, _pwr_reg, _en_mask,		\
			_flags,	_rst_bar_mask,				\
			_pcwbits,					\
			_pd_reg, _pd_shift,				\
			_tuner_reg, _tuner_en_reg, _tuner_en_bit,	\
			_pcw_reg, _pcw_shift, NULL)

static const struct mtk_pll_data plls[] = {
	/* FIXME: need to fix flags/div_table/tuner_reg/table */
#if MT_CCF_BRINGUP
	PLL(APMIXED_MAINPLL, "mainpll", 0x0230, 0x023C, BIT(0),
		(HAVE_RST_BAR | PLL_AO), BIT(24),
		22,
		0x0234, 24,
		0, 0, 0,
		0x0234, 0),
	PLL(APMIXED_UNIV2PLL, "univ2pll", 0x0240, 0x024C, BIT(0),
		(HAVE_RST_BAR | PLL_AO), BIT(24),
		22,
		0x0244, 24,
		0, 0, 0,
		0x0244, 0),

	PLL(APMIXED_MFGPLL, "mfgpll", 0x0250, 0x025C, BIT(0),
		PLL_AO, 0,
		22,
		0x0254, 24,
		0, 0, 0,
		0x0254, 0),

	PLL(APMIXED_MSDCPLL, "msdcpll", 0x0260, 0x026C, BIT(0),
		PLL_AO, 0,
		22,
		0x0264, 24,
		0, 0, 0,
		0x0264, 0),

	PLL(APMIXED_ADSPPLL, "adsppll", 0x02b0, 0x02bC, BIT(0),
		(HAVE_RST_BAR | PLL_AO), BIT(23),
		22,
		0x02b4, 24,
		0, 0, 0,
		0x02b4, 0),

	PLL(APMIXED_MMPLL, "mmpll", 0x0280, 0x028C, BIT(0),
		(HAVE_RST_BAR | PLL_AO), BIT(23),
		22,
		0x0284, 24,
		0, 0, 0,
		0x0284, 0),
	PLL(APMIXED_APLL1, "apll1", 0x02C0, 0x02D0, BIT(0),
		PLL_AO, 0,
		32,
		0x02C0, 1,
		0, 0x14, 0,
		0x02C4, 0),

	PLL(APMIXED_APLL2, "apll2", 0x02D4, 0x02E4, BIT(0),
		PLL_AO, 0,
		32,
		0x02D4, 1,
		0, 0x14, 1,
		0x02D8, 0),

#else
	PLL(APMIXED_MAINPLL, "mainpll", 0x0230, 0x023C, BIT(0),
		(HAVE_RST_BAR | PLL_AO), BIT(24),
		22,
		0x0234, 24,
		0, 0, 0,
		0x0234, 0),

	PLL(APMIXED_UNIV2PLL, "univ2pll", 0x0240, 0x024C, BIT(0),
		(HAVE_RST_BAR), BIT(24),
		22,
		0x0244, 24,
		0, 0, 0,
		0x0244, 0),

	PLL(APMIXED_MFGPLL, "mfgpll", 0x0250, 0x025C, BIT(0),
		0, 0,
		22,
		0x0254, 24,
		0, 0, 0,
		0x0254, 0),

	PLL(APMIXED_MSDCPLL, "msdcpll", 0x0260, 0x026C, BIT(0),
		0, 0,
		22,
		0x0264, 24,
		0, 0, 0,
		0x0264, 0),

	PLL(APMIXED_ADSPPLL, "adsppll", 0x02b0, 0x02bC, BIT(0),
		(HAVE_RST_BAR), BIT(23),
		22,
		0x02b4, 24,
		0, 0, 0,
		0x02b4, 0),

	PLL(APMIXED_MMPLL, "mmpll", 0x0280, 0x028C, BIT(0),
		(HAVE_RST_BAR), BIT(23),
		22,
		0x0284, 24,
		0, 0, 0,
		0x0284, 0),

	PLL(APMIXED_APLL1, "apll1", 0x02C0, 0x02D0, BIT(0),
		0, 0,
		32,
		0x02C0, 1,
		0, 0x14, 0,
		0x02C4, 0),

	PLL(APMIXED_APLL2, "apll2", 0x02D4, 0x02E4, BIT(0),
		0, 0,
		32,
		0x02D4, 1,
		0, 0x14, 1,
		0x02D8, 0),
#endif
};

static void __init mtk_apmixedsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return;
	}

	clk_data = mtk_alloc_clk_data(APMIXED_NR_CLK);

	/* FIXME: add code for APMIXEDSYS */
	mtk_clk_register_plls(node, plls, ARRAY_SIZE(plls), clk_data);
	mtk_clk_register_gates(node, apmixed_clks,
		ARRAY_SIZE(apmixed_clks), clk_data);
	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	apmixed_base = base;
	/* ARMPLL4, MPLL, CCIPLL, EMIPLL, MAINPLL */
	/*clk_writel(AP_PLL_CON3, clk_readl(AP_PLL_CON3) & 0xee2b8ae2);*/
	/*clk_writel(AP_PLL_CON4, clk_readl(AP_PLL_CON4) & 0xee2b8ae2);*/
	/* MPLL, CCIPLL, MAINPLL, TDCLKSQ, CLKSQ1 */
	/*clk_writel(AP_PLL_CON3, clk_readl(AP_PLL_CON3) & 0x007d5550);*/
	/*clk_writel(AP_PLL_CON4, clk_readl(AP_PLL_CON4) & 0x7f5);*/
	clk_writel(AP_PLL_CON3, 0xFFFF7770);
	clk_writel(AP_PLL_CON4, 0xFFFAA007);
	/* [17] = 0 */
	clk_writel(AP_PLL_CON6, clk_readl(AP_PLL_CON6) & 0xfffdffff);
	#if 0 /* race condition access */
	/*[4]SSUSB26M, [11][6]MIPIC camera, [8] mm26m*/
	clk_writel(AP_PLL_CON8, clk_readl(AP_PLL_CON8) & 0xfffff6af);
	#endif

/*APLL1*/
	clk_clrl(APLL1_CON0, PLL_EN);
	clk_setl(APLL1_PWR_CON0, PLL_ISO_EN);
	clk_clrl(APLL1_PWR_CON0, PLL_PWR_ON);
/*APLL2*/
	clk_clrl(APLL2_CON0, PLL_EN);
	clk_setl(APLL2_PWR_CON0, PLL_ISO_EN);
	clk_clrl(APLL2_PWR_CON0, PLL_PWR_ON);
/*ADSPPLL*/
	/* clk_clrl(ADSPPLL_CON0, ADSPPLL_DIV_RSTB); */
	clk_clrl(ADSPPLL_CON0, PLL_EN);
	clk_setl(ADSPPLL_PWR_CON0, PLL_ISO_EN);
	clk_clrl(ADSPPLL_PWR_CON0, PLL_PWR_ON);
	clk_clrl(ADSPPLL_CON0, ADSPPLL_DIV_RSTB);/*move to front*/
#if 0 /* note srt_bar should do before disable */
/*MMPLL*/
	/* clk_clrl(MMPLL_CON0, MMPLL_DIV_RSTB); */
	clk_clrl(MMPLL_CON0, PLL_EN);
	clk_setl(MMPLL_PWR_CON0, PLL_ISO_EN);
	clk_clrl(MMPLL_PWR_CON0, PLL_PWR_ON);
/*MFGPLL*/
	clk_clrl(MFGPLL_CON0, PLL_EN);
	clk_setl(MFGPLL_PWR_CON0, PLL_ISO_EN);
	clk_clrl(MFGPLL_PWR_CON0, PLL_PWR_ON);
/*UNIVPLL*/
	/* clk_clrl(UNIVPLL_CON0, MMPLL_DIV_RSTB); */
	clk_clrl(UNIVPLL_CON0, PLL_EN);
	clk_setl(UNIVPLL_PWR_CON0, PLL_ISO_EN);
	clk_clrl(UNIVPLL_PWR_CON0, PLL_PWR_ON);
/*MSDCPLL*/
	clk_clrl(MSDCPLL_CON0, PLL_EN);
	clk_setl(MSDCPLL_PWR_CON0, PLL_ISO_EN);
	clk_clrl(MSDCPLL_PWR_CON0, PLL_PWR_ON);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_apmixedsys, "mediatek,apmixed",
		mtk_apmixedsys_init);


static void __init mtk_audio_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return;
	}

	clk_data = mtk_alloc_clk_data(AUDIO_NR_CLK);

	mtk_clk_register_gates(node, audio_clks,
		ARRAY_SIZE(audio_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	audio_base = base;
}
CLK_OF_DECLARE_DRIVER(mtk_audio, "mediatek,audio", mtk_audio_init);

static void __init mtk_camsys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return;
	}
	clk_data = mtk_alloc_clk_data(CLK_CAM_M_NR_CLK);

	mtk_clk_register_gates(node, cam_m_clks, ARRAY_SIZE(cam_m_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	cam_base = base;
}
CLK_OF_DECLARE_DRIVER(mtk_camsys, "mediatek,camsys", mtk_camsys_init);

static void __init mtk_camsys_rawa_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return;
	}
	clk_data = mtk_alloc_clk_data(CLK_CAM_RA_NR_CLK);

	mtk_clk_register_gates(node, cam_ra_clks, ARRAY_SIZE(cam_ra_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	cam_ra_base = base;
}
CLK_OF_DECLARE_DRIVER(mtk_camsys_rawa, "mediatek,camsys_rawa", mtk_camsys_rawa_init);

static void __init mtk_camsys_rawb_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return;
	}
	clk_data = mtk_alloc_clk_data(CLK_CAM_RB_NR_CLK);

	mtk_clk_register_gates(node, cam_rb_clks, ARRAY_SIZE(cam_rb_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	cam_rb_base = base;
}
CLK_OF_DECLARE_DRIVER(mtk_camsys_rawb, "mediatek,camsys_rawb", mtk_camsys_rawb_init);

static void __init mtk_imgsys1_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return;
	}
	clk_data = mtk_alloc_clk_data(CLK_IMGSYS1_NR_CLK);

	mtk_clk_register_gates(node, imgsys1_clks, ARRAY_SIZE(imgsys1_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	img1_base = base;
}
CLK_OF_DECLARE_DRIVER(mtk_imgsys1, "mediatek,imgsys1", mtk_imgsys1_init);

static void __init mtk_imgsys2_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return;
	}
	clk_data = mtk_alloc_clk_data(CLK_IMGSYS2_NR_CLK);

	mtk_clk_register_gates(node, imgsys2_clks, ARRAY_SIZE(imgsys2_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	img2_base = base;
}
CLK_OF_DECLARE_DRIVER(mtk_imgsys2, "mediatek,imgsys2", mtk_imgsys2_init);

static void __init mtk_ipesys_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return;
	}
	clk_data = mtk_alloc_clk_data(CLK_IPE_NR_CLK);

	mtk_clk_register_gates(node, ipe_clks, ARRAY_SIZE(ipe_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	ipe_base = base;
}
CLK_OF_DECLARE_DRIVER(mtk_ipesys, "mediatek,ipesys", mtk_ipesys_init);

static void __init mtk_mfg_cfg_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return;
	}

	clk_data = mtk_alloc_clk_data(MFGCFG_NR_CLK);

	mtk_clk_register_gates(node, mfg_cfg_clks,
		ARRAY_SIZE(mfg_cfg_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	mfgcfg_base = base;

#if MT_CCF_BRINGUP
	/*clk_writel(MFG_CG_CLR, MFG_CG);*/
#endif

}
CLK_OF_DECLARE_DRIVER(mtk_mfg_cfg, "mediatek,mfgcfg", mtk_mfg_cfg_init);

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

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	mmsys_config_base = base;
}
CLK_OF_DECLARE_DRIVER(mtk_mmsys_config, "mediatek,mmsys_config",
		mtk_mmsys_config_init);

static void __init mtk_mdpsys_config_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return;
	}
	clk_data = mtk_alloc_clk_data(CLK_MDP_NR_CLK);

	mtk_clk_register_gates(node, mdp_clks, ARRAY_SIZE(mdp_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	mdpsys_config_base = base;
}
CLK_OF_DECLARE_DRIVER(mtk_mdpsys_config, "mediatek,mdpsys_config",
		mtk_mdpsys_config_init);

static void __init mtk_vdec_top_global_con_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return;
	}
	clk_data = mtk_alloc_clk_data(VDEC_GCON_NR_CLK);

	mtk_clk_register_gates(node, vdec_clks,
		ARRAY_SIZE(vdec_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	vdec_gcon_base = base;
}
CLK_OF_DECLARE_DRIVER(mtk_vdec_top_global_con, "mediatek,vdec_gcon",
	mtk_vdec_top_global_con_init);

static void __init mtk_venc_global_con_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return;
	}
	clk_data = mtk_alloc_clk_data(VENC_GCON_NR_CLK);

	mtk_clk_register_gates(node, venc_global_con_clks,
		ARRAY_SIZE(venc_global_con_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	venc_gcon_base = base;
}
CLK_OF_DECLARE_DRIVER(mtk_venc_global_con, "mediatek,venc_gcon",
		mtk_venc_global_con_init);

static void __init mtk_imp_iic_wrap_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return;
	}
	clk_data = mtk_alloc_clk_data(CLK_IMP_NR_CLK);

	mtk_clk_register_gates(node, imp_clks, ARRAY_SIZE(imp_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	imp_base = base;
}
CLK_OF_DECLARE_DRIVER(mtk_imp_iic_wrap, "mediatek,imp_iic_wrap", mtk_imp_iic_wrap_init);

void check_seninf_ck(void)
{
	/* confirm seninf clk */
	pr_notice("%s: CLK_CFG_9 = 0x%08x\r\n", __func__, clk_readl(CLK_CFG_9));
	pr_notice("%s: UNIVPLL_CON0 = 0x%08x\r\n", __func__,
		clk_readl(UNIVPLL_CON0));
	pr_notice("%s: UNIVPLL_CON1 = 0x%08x\r\n", __func__,
		clk_readl(UNIVPLL_CON1));
	pr_notice("%s: AP_PLL_CON2 = 0x%08x\r\n", __func__,
		clk_readl(AP_PLL_CON2));
}
#if 0
void mipic_26m_en(int en)
{
	/* [6] MIPIC0 26M */
	/* [11] MIPIC1 26M */
	if (en)
		clk_writel(AP_PLL_CON8, clk_readl(AP_PLL_CON8) | 0x00000840);
	else
		clk_writel(AP_PLL_CON8, clk_readl(AP_PLL_CON8) & 0xfffff7bf);
}
#endif
/*
 * module_idx mapping
 * display  ->    0
 * camera   ->    1
 */
void mipi_26m_en(unsigned int module_idx, int en)
{
	unsigned long flags;

	apmixed_mipi_lock(flags);

	if (module_idx == 0) {
		/* [17:16] MIPID 26M */
		if (en)
			clk_writel(AP_PLL_CON8,
				clk_readl(AP_PLL_CON8) | 0x00030000);
		else
			clk_writel(AP_PLL_CON8,
				clk_readl(AP_PLL_CON8) & 0xfffcffff);
	} else if (module_idx == 1) {
		/* [6] MIPIC0 26M */
		/* [11] MIPIC1 26M */
		if (en)
			clk_writel(AP_PLL_CON8,
			clk_readl(AP_PLL_CON8) | 0x00000840);
		else
			clk_writel(AP_PLL_CON8,
				clk_readl(AP_PLL_CON8) & 0xfffff7bf);
	} else {
	}

	apmixed_mipi_unlock(flags);
}

#if 0
unsigned int mt_get_ckgen_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1 = 0;

	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFFFC0FC)|(ID << 8)|(0x1));

	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF) | (3 << 24));

	clk26cali_1 = clk_readl(CLK26CALI_1);
	clk_writel(CLK26CALI_0, 0x1000);
	clk_writel(CLK26CALI_0, 0x1010);

	/* wait frequency meter finish */
	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(10);
		i++;
		if (i > 30)
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
			if (i > 30)
				break;
		}
	}

	temp = clk_readl(CLK26CALI_1) & 0xFFFF;

	output = (temp * 26000) / 1024;

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	clk_writel(CLK26CALI_0, 0x0000);

	/*print("ckgen meter[%d] = %d Khz\n", ID, output);*/
	if (i > 30)
		return 0;
	if ((output * 4) < 25000) {
		pr_notice("%s: CLK_DBG_CFG = 0x%x, CLK_MISC_CFG_0 = 0x%x, CLK26CALI_0 = 0x%x, CLK26CALI_1 = 0x%x\n",
			__func__,
			clk_readl(CLK_DBG_CFG),
			clk_readl(CLK_MISC_CFG_0),
			clk_readl(CLK26CALI_0),
			clk_readl(CLK26CALI_1));
	}
	return (output * 4);
}

unsigned int mt_get_abist_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned long flags;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1 = 0;

	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFC0FFFC)|(ID << 16));

	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF) | (3 << 24));

	clk26cali_1 = clk_readl(CLK26CALI_1);

	clk_writel(CLK26CALI_0, 0x1000);
	clk_writel(CLK26CALI_0, 0x1010);

	/* wait frequency meter finish */
	while (clk_readl(CLK26CALI_0) & 0x10) {
		udelay(10);
		i++;
		if (i > 30)
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
			if (i > 30)
				break;
		}
	}
	temp = clk_readl(CLK26CALI_1) & 0xFFFF;

	output = (temp * 26000) / 1024;

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	clk_writel(CLK26CALI_0, 0x0000);

	if (i > 30)
		return 0;
	if ((output * 4) < 25000) {
		pr_notice("%s: CLK_DBG_CFG = 0x%x, CLK_MISC_CFG_0 = 0x%x, CLK26CALI_0 = 0x%x, CLK26CALI_1 = 0x%x\n",
			__func__,
			clk_readl(CLK_DBG_CFG),
			clk_readl(CLK_MISC_CFG_0),
			clk_readl(CLK26CALI_0),
			clk_readl(CLK26CALI_1));
	}
	return (output * 4);
}
#endif

#define ARMPLL_LL_SEL            ((0x1 << 19) \
					  |(0x1 << 20) \
					  |(0x1 << 21) \
					  |(0x1 << 22))
#define ARMPLL_LL_SEL1            ((0x1 << 13))

#define ARMPLL_BL_SEL            ((0x1 << 4) \
					  |(0x1 << 8) \
					  |(0x1 << 12) \
					  |(0x1 << 16))
#define ARMPLL_BL_SEL1            ((0x1 << 0))

#define ARMPLL_BB_SEL            ((0x1 << 27) \
					  |(0x1 << 28) \
					  |(0x1 << 29) \
					  |(0x1 << 30))
#define ARMPLL_BB_SEL1            ((0x1 << 17))


#if 1
void mp_enter_suspend(int id, int suspend)
{
	/* mp0*/
	if (id == 0) {
		if (suspend) {
			/* mp0 enter suspend */
			clk_writel(AP_PLL_CON3,
				clk_readl(AP_PLL_CON3) & ~ARMPLL_LL_SEL);
			clk_writel(AP_PLL_CON4,
				clk_readl(AP_PLL_CON4) & ~ARMPLL_LL_SEL1);
		} else {
			/* mp0 leave suspend */
			clk_writel(AP_PLL_CON3,
				clk_readl(AP_PLL_CON3) | ARMPLL_LL_SEL);
			clk_writel(AP_PLL_CON4,
				clk_readl(AP_PLL_CON4) | ARMPLL_LL_SEL1);
		}
	} else if (id == 1) { /* mp1 */
		if (suspend) {
			/* mp1 enter suspend */
			clk_writel(AP_PLL_CON3,
				clk_readl(AP_PLL_CON3) & ~ARMPLL_BL_SEL);
			clk_writel(AP_PLL_CON4,
				clk_readl(AP_PLL_CON4) & ~ARMPLL_BL_SEL1);
		} else {
			/* mp1 leave suspend */
			clk_writel(AP_PLL_CON3,
				clk_readl(AP_PLL_CON3) | ARMPLL_BL_SEL);
			clk_writel(AP_PLL_CON4,
				clk_readl(AP_PLL_CON4) | ARMPLL_BL_SEL1);
		}
	} else if (id == 2) { /* mp2 */
		if (suspend) {
			/* mp2 enter suspend */
			clk_writel(AP_PLL_CON3,
				clk_readl(AP_PLL_CON3) & ~ARMPLL_BB_SEL);
			clk_writel(AP_PLL_CON4,
				clk_readl(AP_PLL_CON4) & ~ARMPLL_BB_SEL1);
		} else {
			/* mp2 leave suspend */
			clk_writel(AP_PLL_CON3,
				clk_readl(AP_PLL_CON3) | ARMPLL_BB_SEL);
			clk_writel(AP_PLL_CON4,
				clk_readl(AP_PLL_CON4) | ARMPLL_BB_SEL1);
		}
	}
}
#endif

#define UNIV_192M            ((0x1 << 29) \
					  |(0x1 << 30) \
					  |(0x1 << 31))

void univpll_192m_en(int en)
{
	if (en)
		clk_writel(AP_PLL_CON2, clk_readl(AP_PLL_CON2) | UNIV_192M);
	else
		clk_writel(AP_PLL_CON2, clk_readl(AP_PLL_CON2) & ~UNIV_192M);
}

void pll_if_on(void)//check need to add bug_on
{
	if (clk_readl(ARMPLL_LL_CON0) & 0x1)
		pr_notice("suspend warning: ARMPLL_LL is on!!!\n");
	if (clk_readl(ARMPLL_BL_CON0) & 0x1)
		pr_notice("suspend warning: ARMPLL_BL is on!!!\n");
	if (clk_readl(UNIVPLL_CON0) & 0x1)
		pr_notice("suspend warning: UNIVPLL is on!!!\n");
	if (clk_readl(MFGPLL_CON0) & 0x1)
		pr_notice("suspend warning: MFGPLL is on!!!\n");
	if (clk_readl(MMPLL_CON0) & 0x1)
		pr_notice("suspend warning: MMPLL is on!!!\n");
	if (clk_readl(ADSPPLL_CON0) & 0x1)
		pr_notice("suspend warning: ADSPPLL is on!!!\n");
	if (clk_readl(MSDCPLL_CON0) & 0x1)
		pr_notice("suspend warning: MSDCPLL is on!!!\n");
	if (clk_readl(APLL1_CON0) & 0x1)
		pr_notice("suspend warning: APLL1 is on!!!\n");
	if (clk_readl(APLL2_CON0) & 0x1)
		pr_notice("suspend warning: APLL2 is on!!!\n");

#if 0
	pr_notice("%s: AP_PLL_CON3 = 0x%08x\r\n", __func__,
		clk_readl(AP_PLL_CON3));
	pr_notice("%s: AP_PLL_CON4 = 0x%08x\r\n", __func__,
		clk_readl(AP_PLL_CON4));
	pr_notice("%s: ARMPLL_LL = %dHZ\r\n", __func__, mt_get_abist_freq(22));
	pr_notice("%s: ARMPLL_L = %dHZ\r\n", __func__, mt_get_abist_freq(20));
	pr_notice("%s: UNIVPLL = %dHZ\r\n", __func__, mt_get_abist_freq(24));
	pr_notice("%s: MFGPLL = %dHZ\r\n", __func__, mt_get_abist_freq(25));
	pr_notice("%s: MMPLL = %dHZ\r\n", __func__, mt_get_abist_freq(27));
	pr_notice("%s: MSDCPLL = %dHZ\r\n", __func__, mt_get_abist_freq(26));
	pr_notice("%s: APLL1 = %dHZ\r\n", __func__, mt_get_abist_freq(28));
	pr_notice("%s: APLL2 = %dHZ\r\n", __func__, mt_get_abist_freq(29));
#endif
}

void pll_force_off(void)
{
/*MMPLL*/
	clk_clrl(MMPLL_CON0, PLL_EN);
	clk_setl(MMPLL_PWR_CON0, PLL_ISO_EN);
	clk_clrl(MMPLL_PWR_CON0, PLL_PWR_ON);
/*MSDCPLL*/
	clk_clrl(MSDCPLL_CON0, PLL_EN);
	clk_setl(MSDCPLL_PWR_CON0, PLL_ISO_EN);
	clk_clrl(MSDCPLL_PWR_CON0, PLL_PWR_ON);
/*MFGPLL*/
	clk_clrl(MFGPLL_CON0, PLL_EN);
	clk_setl(MFGPLL_PWR_CON0, PLL_ISO_EN);
	clk_clrl(MFGPLL_PWR_CON0, PLL_PWR_ON);
/*UNIVPLL*/
	clk_clrl(UNIVPLL_CON0, PLL_EN);
	clk_setl(UNIVPLL_PWR_CON0, PLL_ISO_EN);
	clk_clrl(UNIVPLL_PWR_CON0, PLL_PWR_ON);
/*APLL1*/
	clk_clrl(APLL1_CON0, PLL_EN);
	clk_setl(APLL1_PWR_CON0, PLL_ISO_EN);
	clk_clrl(APLL1_PWR_CON0, PLL_PWR_ON);
/*APLL2*/
	clk_clrl(APLL2_CON0, PLL_EN);
	clk_setl(APLL2_PWR_CON0, PLL_ISO_EN);
	clk_clrl(APLL2_PWR_CON0, PLL_PWR_ON);
/*ADSPPLL*/
	clk_clrl(ADSPPLL_CON0, PLL_EN);
	clk_setl(ADSPPLL_PWR_CON0, PLL_ISO_EN);
	clk_clrl(ADSPPLL_PWR_CON0, PLL_PWR_ON);
	clk_clrl(ADSPPLL_CON0, ADSPPLL_DIV_RSTB);
}

void armpll_control(int id, int on)
{
	if (id == 1) {
		if (on) {
			mt_reg_sync_writel((clk_readl(ARMPLL_LL_PWR_CON0) |
				0x01), ARMPLL_LL_PWR_CON0);
			udelay(100);
			mt_reg_sync_writel((clk_readl(ARMPLL_LL_PWR_CON0) &
				0xfffffffd), ARMPLL_LL_PWR_CON0);
			udelay(10);
			mt_reg_sync_writel((clk_readl(ARMPLL_LL_CON1) |
				0x80000000), ARMPLL_LL_CON1);
			mt_reg_sync_writel((clk_readl(ARMPLL_LL_CON0) |
				0x01), ARMPLL_LL_CON0);
			udelay(100);
		} else {
			mt_reg_sync_writel((clk_readl(ARMPLL_LL_CON0) &
				0xfffffffe), ARMPLL_LL_CON0);
			mt_reg_sync_writel((clk_readl(ARMPLL_LL_PWR_CON0) |
				0x00000002), ARMPLL_LL_PWR_CON0);
			mt_reg_sync_writel((clk_readl(ARMPLL_LL_PWR_CON0) &
				0xfffffffe), ARMPLL_LL_PWR_CON0);
		}
	} else if (id == 2) {
		if (on) {
			mt_reg_sync_writel((clk_readl(ARMPLL_BL_PWR_CON0) |
				0x01), ARMPLL_BL_PWR_CON0);
			udelay(100);
			mt_reg_sync_writel((clk_readl(ARMPLL_BL_PWR_CON0) &
				0xfffffffd), ARMPLL_BL_PWR_CON0);
			udelay(10);
			mt_reg_sync_writel((clk_readl(ARMPLL_BL_CON1) |
				0x80000000), ARMPLL_BL_CON1);
			mt_reg_sync_writel((clk_readl(ARMPLL_BL_CON0) |
				0x01), ARMPLL_BL_CON0);
			udelay(100);
		} else {
			mt_reg_sync_writel((clk_readl(ARMPLL_BL_CON0) &
				0xfffffffe), ARMPLL_BL_CON0);
			mt_reg_sync_writel((clk_readl(ARMPLL_BL_PWR_CON0) |
				0x00000002), ARMPLL_BL_PWR_CON0);
			mt_reg_sync_writel((clk_readl(ARMPLL_BL_PWR_CON0) &
				0xfffffffe), ARMPLL_BL_PWR_CON0);
		}
	} else if (id == 3) {
		if (on) {
			mt_reg_sync_writel((clk_readl(ARMPLL_BB_PWR_CON0) |
				0x01), ARMPLL_BB_PWR_CON0);
			udelay(100);
			mt_reg_sync_writel((clk_readl(ARMPLL_BB_PWR_CON0) &
				0xfffffffd), ARMPLL_BB_PWR_CON0);
			udelay(10);
			mt_reg_sync_writel((clk_readl(ARMPLL_BB_CON1) |
				0x80000000), ARMPLL_BB_CON1);
			mt_reg_sync_writel((clk_readl(ARMPLL_BB_CON0) |
				0x01), ARMPLL_BB_CON0);
			udelay(100);
		} else {
			mt_reg_sync_writel((clk_readl(ARMPLL_BB_CON0) &
				0xfffffffe), ARMPLL_BB_CON0);
			mt_reg_sync_writel((clk_readl(ARMPLL_BB_PWR_CON0) |
				0x00000002), ARMPLL_BB_PWR_CON0);
			mt_reg_sync_writel((clk_readl(ARMPLL_BB_PWR_CON0) &
				0xfffffffe), ARMPLL_BB_PWR_CON0);
		}
	}
}

static int __init clk_mt6781_init(void)
{
	/*timer_ready = true;*/
	/*mtk_clk_enable_critical();*/

	return 0;
}
arch_initcall(clk_mt6781_init);

