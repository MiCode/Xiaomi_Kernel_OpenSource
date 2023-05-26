/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"
#include "clk-mux.h"
#include "clk-mt6785-pg.h"

#include <dt-bindings/clock/mt6785-clk.h>

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

#define PLL_EN  (0x1 << 0)
#define PLL_PWR_ON  (0x1 << 0)
#define PLL_ISO_EN  (0x1 << 1)
#define ADSPPLL_DIV_RSTB  (0x1 << 23)

static DEFINE_SPINLOCK(mt6785_clk_lock);

/* Total 10 subsys */
void __iomem *cksys_base;
void __iomem *infracfg_base;
void __iomem *apmixed_base;
void __iomem *audio_base;
void __iomem *cam_base;
void __iomem *img_base;
void __iomem *ipe_base;
void __iomem *mfgcfg_base;
void __iomem *mmsys_config_base;
void __iomem *venc_gcon_base;
void __iomem *vdec_gcon_base;
void __iomem *apu_vcore_base;
void __iomem *apu_conn_base;
void __iomem *apu0_base;
void __iomem *apu1_base;
void __iomem *apu_mdla_base;


/* CKSYS */
#define CLK_CFG_UPDATE		(cksys_base + 0x004)
#define CLK_CFG_0		(cksys_base + 0x020)
#define CLK_CFG_1		(cksys_base + 0x030)
#define CLK_CFG_6		(cksys_base + 0x080)
#define CLK_CFG_6_SET		(cksys_base + 0x084)
#define CLK_CFG_6_CLR		(cksys_base + 0x088)
#define CLK_CFG_9		(cksys_base + 0x0B0)
#define CLK_MISC_CFG_0		(cksys_base + 0x110)
#define CLK_DBG_CFG		(cksys_base + 0x10C)
#define CLK_SCP_CFG_0		(cksys_base + 0x200)
#define CLK_SCP_CFG_1		(cksys_base + 0x210)
#define CLK26CALI_0		(cksys_base + 0x220)
#define CLK26CALI_1		(cksys_base + 0x224)
#define TOPCK_CAM_PDN		31

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
#define TVDPLL_CON0		(apmixed_base + 0x0270)
#define TVDPLL_PWR_CON0		(apmixed_base + 0x027C)
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
#define APUPLL_CON0		(apmixed_base + 0x02EC)
#define APUPLL_PWR_CON0		(apmixed_base + 0x02F8)

#define AUDIO_TOP_CON0		(audio_base + 0x0000)
#define AUDIO_TOP_CON1		(audio_base + 0x0004)

#define CAMSYS_CG_CON		(cam_base + 0x0000)
#define CAMSYS_CG_SET		(cam_base + 0x0004)
#define CAMSYS_CG_CLR		(cam_base + 0x0008)

#define IMG_CG_CON		(img_base + 0x0000)
#define IMG_CG_SET		(img_base + 0x0004)
#define IMG_CG_CLR		(img_base + 0x0008)

#define MFG_CG_CON              (mfgcfg_base + 0x0000)
#define MFG_CG_SET              (mfgcfg_base + 0x0004)
#define MFG_CG_CLR              (mfgcfg_base + 0x0008)

#define MM_CG_CON0            (mmsys_config_base + 0x100)
#define MM_CG_SET0            (mmsys_config_base + 0x104)
#define MM_CG_CLR0            (mmsys_config_base + 0x108)
#define MM_CG_CON1            (mmsys_config_base + 0x110)
#define MM_CG_SET1            (mmsys_config_base + 0x114)
#define MM_CG_CLR1            (mmsys_config_base + 0x118)

#define VENC_CG_CON		(venc_gcon_base + 0x0000)
#define VENC_CG_SET		(venc_gcon_base + 0x0004)
#define VENC_CG_CLR		(venc_gcon_base + 0x0008)

#define VDEC_CKEN_SET           (vdec_gcon_base + 0x0000)
#define VDEC_CKEN_CLR           (vdec_gcon_base + 0x0004)
#define LARB1_CKEN_SET          (vdec_gcon_base + 0x0008)
#define LARB1_CKEN_CLR          (vdec_gcon_base + 0x000C)

#define APU_VCORE_CG_CON              (apu_vcore_base + 0x0000)
#define APU_VCORE_CG_SET              (apu_vcore_base + 0x0004)
#define APU_VCORE_CG_CLR              (apu_vcore_base + 0x0008)

#define APU_CONN_CG_CON              (apu_conn_base + 0x0000)
#define APU_CONN_CG_SET              (apu_conn_base + 0x0004)
#define APU_CONN_CG_CLR              (apu_conn_base + 0x0008)

#define APU_CORE0_CG_CON              (apu0_base + 0x0000)
#define APU_CORE0_CG_SET              (apu0_base + 0x0004)
#define APU_CORE0_CG_CLR              (apu0_base + 0x0008)

#define APU_CORE1_CG_CON              (apu1_base + 0x0000)
#define APU_CORE1_CG_SET              (apu1_base + 0x0004)
#define APU_CORE1_CG_CLR              (apu1_base + 0x0008)



#define INFRA_CG0 0x03aff900/*[25:23][21][19:15][13:11][8]*/
/*#define INFRA_CG0 0x03af8100 [25:23][21][19:15][i2c][8]*/
#define INFRA_CG1 0x0a040806/*[27][25][18][11][2][1]*/
#define INFRA_CG2 0x00000881/*[11][7:6adb_reboot][4atf][0]*/
#define INFRA_CG3 0x080001c7/*[27][2:0], [8:6]*/

#define CAMSYS_CG 0x3FD5
#define IMG_CG	0x3FF
#define MFG_CG	0x1

#define MM_CG0	0x00000020/*[5]*/
#define MM_CG1  0x00003f7c/*[13:8][6:2]*/

#define VENC_CG 0x10000111 /* inverse */
#define VDEC_CG 0x1 /* inverse */
#define VDEC_LARB1_CG 0x1 /* inverse */
#define APU_CONN_CG 0xff
#define APU_CORE0_CG 0x7
#define APU_CORE1_CG 0x7
#define APU_VCORE_CG 0xf


#define CK_CFG_UPDATE	0x04
#define CK_CFG_UPDATE1	0x08
#define CK_CFG_0	0x20
#define CK_CFG_0_SET	0x24
#define CK_CFG_0_CLR	0x28
#define CK_CFG_1	0x30
#define CK_CFG_1_SET	0x34
#define CK_CFG_1_CLR	0x38
#define CK_CFG_2	0x40
#define CK_CFG_2_SET	0x44
#define CK_CFG_2_CLR	0x48
#define CK_CFG_3	0x50
#define CK_CFG_3_SET	0x54
#define CK_CFG_3_CLR	0x58
#define CK_CFG_4	0x60
#define CK_CFG_4_SET	0x64
#define CK_CFG_4_CLR	0x68
#define CK_CFG_5	0x70
#define CK_CFG_5_SET	0x74
#define CK_CFG_5_CLR	0x78
#define CK_CFG_6	0x80
#define CK_CFG_6_SET	0x84
#define CK_CFG_6_CLR	0x88
#define CK_CFG_7	0x90
#define CK_CFG_7_SET	0x94
#define CK_CFG_7_CLR	0x98
#define CK_CFG_8	0xA0
#define CK_CFG_8_SET	0xA4
#define CK_CFG_8_CLR	0xA8
#define CK_CFG_9	0xB0
#define CK_CFG_9_SET	0xB4
#define CK_CFG_9_CLR	0xB8
#define CK_CFG_10	0xC0
#define CK_CFG_10_SET	0xC4
#define CK_CFG_10_CLR	0xC8
#define CK_CFG_11	0xD0
#define CK_CFG_11_SET	0xD4
#define CK_CFG_11_CLR	0xD8
#define CK_CFG_12	0xE0
#define CK_CFG_12_SET	0xE4
#define CK_CFG_12_CLR	0xE8
#define CK_CFG_13	0xF0
#define CK_CFG_13_SET	0xF4
#define CK_CFG_13_CLR	0xF8


static const struct mtk_fixed_clk fixed_clks[] = {
	FIXED_CLK(TOP_CLK26M, "f_f26m_ck", "clk26m", 26000000),
};

static const struct mtk_fixed_factor top_divs[] = {
	FACTOR(TOP_CLK13M, "clk13m", "clk26m", 1,
		2),
	FACTOR(TOP_F26M_CK_D2, "csw_f26m_ck_d2", "clk26m", 1,
		2),
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
	FACTOR(TOP_TVDPLL_CK, "tvdpll_ck", "tvdpll", 1,
		1),
	FACTOR(TOP_TVDPLL_D2, "tvdpll_d2", "tvdpll_ck", 1,
		2),
	FACTOR(TOP_TVDPLL_D4, "tvdpll_d4", "tvdpll", 1,
		4),
	FACTOR(TOP_TVDPLL_D8, "tvdpll_d8", "tvdpll", 1,
		8),
	FACTOR(TOP_TVDPLL_D16, "tvdpll_d16", "tvdpll", 1,
		16),

	FACTOR(TOP_MMPLL_CK, "mmpll_ck", "mmpll", 1,
		1),
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
	FACTOR(TOP_ADSPPLL_D4, "adsppll_d4", "adsppll", 1,
		4),
	FACTOR(TOP_ADSPPLL_D5, "adsppll_d5", "adsppll", 1,
		5),
	FACTOR(TOP_ADSPPLL_D6, "adsppll_d6", "adsppll", 1,
		6),

	FACTOR(TOP_MSDCPLL_CK, "msdcpll_ck", "msdcpll", 1,
		1),
	FACTOR(TOP_MSDCPLL_D2, "msdcpll_d2", "msdcpll", 1,
		2),
	FACTOR(TOP_MSDCPLL_D4, "msdcpll_d4", "msdcpll", 1,
		4),

	FACTOR(TOP_AD_OSC_CK, "ad_osc_ck", "osc", 1,
		1),
	FACTOR(TOP_OSC_D2, "osc_d2", "osc", 1,
		2),
	FACTOR(TOP_OSC_D4, "osc_d4", "osc", 1,
		4),
	FACTOR(TOP_OSC_D8, "osc_d8", "osc", 1,
		8),
	FACTOR(TOP_OSC_D10, "osc_d10", "osc", 1,
		10),
	FACTOR(TOP_OSC_D16, "osc_d16", "osc", 1,
		16),
	FACTOR(TOP_AD_OSC2_CK, "ad_osc2_ck", "osc2", 1,
		1),
	FACTOR(TOP_OSC2_D3, "osc2_d3", "osc2", 1,
		3),

	FACTOR(TOP_TVDPLL_MAINPLL_D2_CK, "tvdpll_mainpll_d2_ck", "tvdpll", 1,
		1),

	FACTOR(TOP_FMEM_466M_CK, "fmem_466m_ck", "fmem", 1,
		1),
	FACTOR(TOP_APUPLL_CK, "apupll_ck", "apupll", 1,
		1),
};

static const char * const axi_parents[] = {
	"clk26m",
	"mainpll_d2_d4",
	"mainpll_d7",
	"osc_d4"
};

static const char * const mm_parents[] = {
	"clk26m",
	"adsppll_d5",
	"mmpll_d7",
	"mmpll_d5_d2",
	"mainpll_d2_d2",
	"tvdpll_mainpll_d2_ck"
};

static const char * const scp_parents[] = {
	"clk26m",
	"mainpll_d2_d2",
	"mainpll_d2_d4",
	"mainpll_d3",
	"univpll_d3",
	"mainpll_d5",
	"mainpll_d3_d2",
	"osc2_d3"
};

static const char * const img_parents[] = {
	"clk26m",
	"mainpll_d2",
	"mmpll_d6",
	"adsppll_d5",
	"mainpll_d3",
	"mmpll_d5_d2",
	"tvdpll_mainpll_d2_ck",
	"mainpll_d2_d2"
};

static const char * const ipe_parents[] = {
	"clk26m",
	"mainpll_d2",
	"mmpll_d6",
	"adsppll_d5",
	"mainpll_d3",
	"mmpll_d5_d2",
	"tvdpll_mainpll_d2_ck",
	"mainpll_d2_d2"
};

static const char * const dpe_parents[] = {
	"clk26m",
	"mainpll_d2",
	"mmpll_d6",
	"adsppll_d5",
	"mainpll_d3",
	"mmpll_d5_d2",
	"tvdpll_mainpll_d2_ck",
	"mainpll_d2_d2"
};

static const char * const cam_parents[] = {
	"clk26m",
	"mainpll_d2",
	"mmpll_d6",
	"mainpll_d3",
	"mmpll_d7",
	"univpll_d3",
	"mmpll_d5_d2",
	"adsppll_d5",
	"tvdpll_mainpll_d2_ck",
	"univpll_d3_d2"
};

static const char * const ccu_parents[] = {
	"clk26m",
	"mainpll_d2",
	"mmpll_d6",
	"mainpll_d3",
	"mmpll_d7",
	"univpll_d3",
	"mmpll_d5_d2",
	"mainpll_d2_d2",
	"adsppll_d5",
	"univpll_d3_d2"
};

static const char * const dsp_parents[] = {
	"clk26m",
	"univpll_d3_d8",
	"univpll_d3_d4",
	"mainpll_d2_d4",
	"univpll_d3_d2",
	"mainpll_d2_d2",
	"univpll_d2_d2",
	"mainpll_d3",
	"univpll_d3",
	"mmpll_d7",
	"mmpll_d6",
	"adsppll_d5",
	"tvdpll_ck",
	"univpll_d2",
	"adsppll_d4",
	"apupll_ck"
};

static const char * const dsp1_parents[] = {
	"clk26m",
	"univpll_d3_d8",
	"univpll_d3_d4",
	"mainpll_d2_d4",
	"univpll_d3_d2",
	"mainpll_d2_d2",
	"univpll_d2_d2",
	"mainpll_d3",
	"univpll_d3",
	"mmpll_d7",
	"mmpll_d6",
	"adsppll_d5",
	"tvdpll_ck",
	"univpll_d2",
	"adsppll_d4",
	"apupll_ck"
};

static const char * const dsp2_parents[] = {
	"clk26m",
	"univpll_d3_d8",
	"univpll_d3_d4",
	"mainpll_d2_d4",
	"univpll_d3_d2",
	"mainpll_d2_d2",
	"univpll_d2_d2",
	"mainpll_d3",
	"univpll_d3",
	"mmpll_d7",
	"mmpll_d6",
	"adsppll_d5",
	"tvdpll_ck",
	"univpll_d2",
	"adsppll_d4",
	"apupll_ck"
};

static const char * const dsp3_parents[] = {
	"clk26m",
	"univpll_d3_d8",
	"mainpll_d2_d4",
	"univpll_d3_d2",
	"mainpll_d2_d2",
	"univpll_d2_d2",
	"mainpll_d3",
	"univpll_d3",
	"mmpll_d7",
	"mmpll_d6",
	"mainpll_d2",
	"tvdpll_ck",
	"tvdpll_mainpll_d2_ck",
	"univpll_d2",
	"adsppll_d4",
	"mmpll_d4"
};

static const char * const ipu_if_parents[] = {
	"clk26m",
	"univpll_d3_d8",
	"univpll_d3_d4",
	"mainpll_d2_d4",
	"univpll_d3_d2",
	"mainpll_d2_d2",
	"univpll_d2_d2",
	"mainpll_d3",
	"univpll_d3",
	"mmpll_d7",
	"mmpll_d6",
	"adsppll_d5",
	"tvdpll_ck",
	"univpll_d2",
	"adsppll_d4",
	"apupll_ck"
};

static const char * const mfg_parents[] = {
	"clk26m",
	"mfgpll_ck",
	"univpll_d3",
	"mainpll_d5"
};

static const char * const f52m_mfg_parents[] = {
	"clk26m",
	"univpll_d3_d2",
	"univpll_d3_d4",
	"univpll_d3_d8"
};

static const char * const camtg_parents[] = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d3_d8",
	"univpll_192m_d4",
	"univpll_d3_d16",
	"csw_f26m_ck_d2",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const camtg2_parents[] = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d3_d8",
	"univpll_192m_d4",
	"univpll_d3_d16",
	"csw_f26m_ck_d2",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const camtg3_parents[] = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d3_d8",
	"univpll_192m_d4",
	"univpll_d3_d16",
	"csw_f26m_ck_d2",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const camtg4_parents[] = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d3_d8",
	"univpll_192m_d4",
	"univpll_d3_d16",
	"csw_f26m_ck_d2",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const camtg5_parents[] = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d3_d8",
	"univpll_192m_d4",
	"univpll_d3_d16",
	"csw_f26m_ck_d2",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const uart_parents[] = {
	"clk26m",
	"univpll_d3_d8"
};

static const char * const spi_parents[] = {
	"clk26m",
	"mainpll_d5_d2",
	"mainpll_d3_d4",
	"msdcpll_d4"
};

static const char * const msdc50_hclk_parents[] = {
	"clk26m",
	"mainpll_d2_d2",
	"mainpll_d3_d2"
};

static const char * const msdc50_0_parents[] = {
	"clk26m",
	"msdcpll_ck",
	"msdcpll_d2",
	"univpll_d2_d4",
	"mainpll_d3_d2",
	"univpll_d2_d2"
};

static const char * const msdc30_1_parents[] = {
	"clk26m",
	"univpll_d3_d2",
	"mainpll_d3_d2",
	"mainpll_d7",
	"msdcpll_d2"
};

static const char * const audio_parents[] = {
	"clk26m",
	"mainpll_d5_d4",
	"mainpll_d7_d4",
	"mainpll_d2_d16"
};

static const char * const aud_intbus_parents[] = {
	"clk26m",
	"mainpll_d2_d4",
	"mainpll_d7_d2"
};


static const char * const fpwrap_ulposc_parents[] = {
	"osc_d10",
	"clk26m",
	"osc_d4",
	"osc_d8",
	"osc_d16"
};

static const char * const atb_parents[] = {
	"clk26m",
	"mainpll_d2_d2",
	"mainpll_d5"
};

static const char * const sspm_parents[] = {
	"clk26m",
	"univpll_d2_d4",
	"mainpll_d2_d2",
	"univpll_d2_d2",
	"mainpll_d3"
};

static const char * const dpi0_parents[] = {
	"clk26m",
	"tvdpll_d2",
	"tvdpll_d4",
	"tvdpll_d8",
	"tvdpll_d16"
};

static const char * const scam_parents[] = {
	"clk26m",
	"mainpll_d5_d2"
};

static const char * const disppwm_parents[] = {
	"clk26m",
	"univpll_d3_d4",
	"osc_d2",
	"osc_d4",
	"osc_d16"
};

static const char * const usb_top_parents[] = {
	"clk26m",
	"univpll_d5_d4",
	"univpll_d3_d4",
	"univpll_d5_d2"
};


static const char * const ssusb_top_xhci_parents[] = {
	"clk26m",
	"univpll_d5_d4",
	"univpll_d3_d4",
	"univpll_d5_d2"
};

static const char * const spm_parents[] = {
	"clk26m",
	"osc_d8",
	"mainpll_d2_d8"
};

static const char * const i2c_parents[] = {
	"clk26m",
	"mainpll_d2_d8",
	"univpll_d5_d2"
};

static const char * const seninf_parents[] = {
	"clk26m",
	"univpll_d7",
	"univpll_d3_d2",
	"univpll_d2_d2"
};

static const char * const seninf1_parents[] = {
	"clk26m",
	"univpll_d7",
	"univpll_d3_d2",
	"univpll_d2_d2"
};

static const char * const seninf2_parents[] = {
	"clk26m",
	"univpll_d7",
	"univpll_d3_d2",
	"univpll_d2_d2"
};

static const char * const dxcc_parents[] = {
	"clk26m",
	"mainpll_d2_d2",
	"mainpll_d2_d4",
	"mainpll_d2_d8"
};

static const char * const aud_eng1_parents[] = {
	"clk26m",
	"apll1_d2",
	"apll1_d4",
	"apll1_d8"
};

static const char * const aud_eng2_parents[] = {
	"clk26m",
	"apll2_d2",
	"apll2_d4",
	"apll2_d8"
};

static const char * const faes_ufsfde_parents[] = {
	"clk26m",
	"mainpll_d2",
	"mainpll_d2_d2",
	"mainpll_d3",
	"mainpll_d2_d4",
	"univpll_d3"
};

static const char * const fufs_parents[] = {
	"clk26m",
	"mainpll_d2_d4",
	"mainpll_d2_d8",
	"mainpll_d2_d16"
};

static const char * const aud_1_parents[] = {
	"clk26m",
	"apll1_ck"
};

static const char * const aud_2_parents[] = {
	"clk26m",
	"apll2_ck"
};

static const char * const adsp_parents[] = {
	"clk26m",
	"mainpll_d3",
	"univpll_d2_d4",
	"univpll_d2",
	"mmpll_d4",
	"adsppll_d4",
	"adsppll_d6"
};

static const char * const dpmaif_parents[] = {
	"clk26m",
	"univpll_d2_d4",
	"mainpll_d3",
	"mainpll_d2_d2",
	"univpll_d2_d2",
	"univpll_d3"
};

static const char * const venc_parents[] = {
	"clk26m",
	"mmpll_d7",
	"mainpll_d3",
	"univpll_d2_d2",
	"mainpll_d2_d2",
	"univpll_d3",
	"mmpll_d6",
	"mainpll_d5",
	"mainpll_d3_d2",
	"mmpll_d4_d2",
	"univpll_d2_d4",
	"mmpll_d5",
	"univpll_192m_d2"

};

static const char * const vdec_parents[] = {
	"clk26m",
	"univpll_d2_d4",
	"mainpll_d3",
	"univpll_d2_d2",
	"mainpll_d2_d2",
	"univpll_d3",
	"univpll_d5",
	"univpll_d5_d2",
	"mainpll_d2",
	"univpll_d2",
	"univpll_192m_d2"
};

static const char * const camtm_parents[] = {
	"clk26m",
	"univpll_d7",
	"univpll_d3_d2",
	"univpll_d2_d2"
};

static const char * const pwm_parents[] = {
	"clk26m",
	"univpll_d2_d8"
};

static const char * const audio_h_parents[] = {
	"clk26m",
	"univpll_d7",
	"apll1_ck",
	"apll2_ck"
};



static const char * const i2s0_m_ck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const i2s1_m_ck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const i2s2_m_ck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const i2s3_m_ck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const i2s4_m_ck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const i2s5_m_ck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};


static const struct mtk_mux top_muxes[] = {
#if MT_CCF_BRINGUP
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_AXI, "axi_sel", axi_parents,
		CK_CFG_0, CK_CFG_0_SET, CK_CFG_0_CLR,
		0, 2, 7,
		0x004, 0, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_MM, "mm_sel", mm_parents,
		CK_CFG_0, CK_CFG_0_SET, CK_CFG_0_CLR,
		8, 3, 15,
		0x004, 1, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_SCP, "scp_sel", scp_parents,
		CK_CFG_0, CK_CFG_0_SET, CK_CFG_0_CLR,
		16, 3, 23,
		0x004, 2, CLK_IS_CRITICAL),

	/* CLK_CFG_1 */
	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_IMG, "img_sel", img_parents,
		CK_CFG_1, CK_CFG_1_SET, CK_CFG_1_CLR,
		0, 3, 7,
		0x004, 4, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_CAM, "cam_sel", cam_parents,
		CK_CFG_1, CK_CFG_1_SET, CK_CFG_1_CLR,
		24, 4, 31,
		0x004, 7, CLK_IS_CRITICAL),

	/* CLK_CFG_2 */
	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_DSP, "dsp_sel", dsp_parents,
		CK_CFG_2, CK_CFG_2_SET, CK_CFG_2_CLR,
		8, 4, 15,
		0x004, 9, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_DSP1, "dsp1_sel", dsp1_parents,
		CK_CFG_2, CK_CFG_2_SET, CK_CFG_2_CLR,
		16, 4, 23,
		0x004, 10, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_DSP2, "dsp2_sel", dsp2_parents,
		CK_CFG_2, CK_CFG_2_SET, CK_CFG_2_CLR,
		24, 4, 31,
		0x004, 11, CLK_IS_CRITICAL),

	/* CLK_CFG_3 */
	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_IPU_IF, "ipu_if_sel", ipu_if_parents,
		CK_CFG_3, CK_CFG_3_SET, CK_CFG_3_CLR,
		8, 4, 15,
		0x004, 13, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_MFG, "mfg_sel", mfg_parents,
		CK_CFG_3, CK_CFG_3_SET, CK_CFG_3_CLR,
		16, 2, 23,
		0x004, 14, CLK_IS_CRITICAL),

	/* CLK_CFG_4 */
	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_CAMTG, "camtg_sel", camtg_parents,
		CK_CFG_4, CK_CFG_4_SET, CK_CFG_4_CLR,
		0, 3, 7,
		0x004, 16, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_CAMTG2, "camtg2_sel", camtg2_parents,
		CK_CFG_4, CK_CFG_4_SET, CK_CFG_4_CLR,
		8, 3, 15,
		0x004, 17, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_CAMTG3, "camtg3_sel", camtg3_parents,
		CK_CFG_4, CK_CFG_4_SET, CK_CFG_4_CLR,
		16, 3, 23,
		0x004, 18, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_CAMTG4, "camtg4_sel", camtg4_parents,
		CK_CFG_4, CK_CFG_4_SET, CK_CFG_4_CLR,
		24, 3, 31,
		0x004, 19, CLK_IS_CRITICAL),

	/* CLK_CFG_5 */
	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_UART, "uart_sel", uart_parents,
		CK_CFG_5, CK_CFG_5_SET, CK_CFG_5_CLR,
		0, 1, 7,
		0x004, 20, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_SPI, "spi_sel", spi_parents,
		CK_CFG_5, CK_CFG_5_SET, CK_CFG_5_CLR,
		8, 2, 15,
		0x004, 21, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_MSDC50_0_HCLK, "msdc50_hclk_sel",
		msdc50_hclk_parents,
		CK_CFG_5, CK_CFG_5_SET, CK_CFG_5_CLR,
		16, 2, 23,
		0x004, 22, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_MSDC50_0, "msdc50_0_sel",
		msdc50_0_parents,
		CK_CFG_5, CK_CFG_5_SET, CK_CFG_5_CLR,
		24, 3, 31,
		0x004, 23, CLK_IS_CRITICAL),

	/* CLK_CFG_6 */
	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_MSDC30_1, "msdc30_1_sel",
		msdc30_1_parents,
		CK_CFG_6, CK_CFG_6_SET, CK_CFG_6_CLR,
		0, 3, 7,
		0x004, 24, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_AUDIO, "audio_sel", audio_parents,
		CK_CFG_6, CK_CFG_6_SET, CK_CFG_6_CLR,
		8, 2, 15,
		0x004, 25, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_AUD_INTBUS, "aud_intbus_sel",
		aud_intbus_parents,
		CK_CFG_6, CK_CFG_6_SET, CK_CFG_6_CLR,
		16, 2, 23,
		0x004, 26, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_FPWRAP_ULPOSC, "fpwrap_ulposc_sel",
		fpwrap_ulposc_parents,
		CK_CFG_6, CK_CFG_6_SET, CK_CFG_6_CLR,
		24, 3, 31,
		0x004, 27, CLK_IS_CRITICAL),

	/* CLK_CFG_7 */
	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_ATB, "atb_sel", atb_parents,
		CK_CFG_7, CK_CFG_7_SET, CK_CFG_7_CLR,
		0, 2, 7,
		0x004, 28, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_SSPM, "sspm_sel", sspm_parents,
		CK_CFG_7, CK_CFG_7_SET, CK_CFG_7_CLR,
		8, 3, 15,
		0x004, 29, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_DPI0, "dpi0_sel", dpi0_parents,
		CK_CFG_7, CK_CFG_7_SET, CK_CFG_7_CLR,
		16, 3, 23,
		0x004, 30, CLK_IS_CRITICAL),

	/* CLK_CFG_8 */
	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_DISP_PWM, "disppwm_sel", disppwm_parents,
		CK_CFG_8, CK_CFG_8_SET, CK_CFG_8_CLR,
		0, 3, 7,
		0x008, 1, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_USB_TOP, "usb_top_sel", usb_top_parents,
		CK_CFG_8, CK_CFG_8_SET, CK_CFG_8_CLR,
		8, 2, 15,
		0x008, 2, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_SSUSB_TOP_XHCI, "ssusb_top_xhci_sel",
		ssusb_top_xhci_parents,
		CK_CFG_8, CK_CFG_8_SET, CK_CFG_8_CLR,
		16, 2, 23,
		0x008, 3, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_SPM, "spm_sel", spm_parents,
		CK_CFG_8, CK_CFG_8_SET, CK_CFG_8_CLR,
		24, 2, 31,
		0x008, 4, CLK_IS_CRITICAL),

	/* CLK_CFG_9 */
	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_I2C, "i2c_sel", i2c_parents,
		CK_CFG_9, CK_CFG_9_SET, CK_CFG_9_CLR,
		0, 2, 7,
		0x008, 5, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_SENINF, "seninf_sel", seninf_parents,
		CK_CFG_9, CK_CFG_9_SET, CK_CFG_9_CLR,
		8, 2, 15,
		0x008, 6, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_SENINF1, "seninf1_sel", seninf1_parents,
		CK_CFG_9, CK_CFG_9_SET, CK_CFG_9_CLR,
		16, 2, 23,
		0x008, 7, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_SENINF2, "seninf2_sel", seninf2_parents,
		CK_CFG_9, CK_CFG_9_SET, CK_CFG_9_CLR,
		24, 2, 31,
		0x008, 8, CLK_IS_CRITICAL),

	/* CLK_CFG_10 */
	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_DXCC, "dxcc_sel", dxcc_parents,
		CK_CFG_10, CK_CFG_10_SET, CK_CFG_10_CLR,
		0, 2, 7,
		0x008, 9, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_AUD_ENG1, "aud_eng1_sel",
		aud_eng1_parents,
		CK_CFG_10, CK_CFG_10_SET, CK_CFG_10_CLR,
		8, 2, 15,
		0x008, 10, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_AUD_ENG2, "aud_eng2_sel",
		aud_eng2_parents,
		CK_CFG_10, CK_CFG_10_SET, CK_CFG_10_CLR,
		16, 2, 23,
		0x008, 11, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_FAES_UFSFDE, "faes_ufsfde_sel",
		faes_ufsfde_parents,
		CK_CFG_10, CK_CFG_10_SET, CK_CFG_10_CLR,
		24, 3, 31,
		0x008, 12, CLK_IS_CRITICAL),

	/* CLK_CFG_11 */
	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_FUFS, "fufs_sel", fufs_parents,
		CK_CFG_11, CK_CFG_11_SET, CK_CFG_11_CLR,
		0, 2, 7,
		0x008, 13, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_AUD_1, "aud_1_sel", aud_1_parents,
		CK_CFG_11, CK_CFG_11_SET, CK_CFG_11_CLR,
		8, 1, 15,
		0x008, 14, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_AUD_2, "aud_2_sel", aud_2_parents,
		CK_CFG_11, CK_CFG_11_SET, CK_CFG_11_CLR,
		16, 1, 23,
		0x008, 15, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_ADSP, "adsp_sel", adsp_parents,
		CK_CFG_11, CK_CFG_11_SET, CK_CFG_11_CLR,
		24, 3, 31,
		0x008, 16, CLK_IS_CRITICAL),

	/* CLK_CFG_12 */
	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_DPMAIF, "dpmaif_sel", dpmaif_parents,
		CK_CFG_12, CK_CFG_12_SET, CK_CFG_12_CLR,
		0, 3, 7,
		0x008, 17, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_VENC, "venc_sel", venc_parents,
		CK_CFG_12, CK_CFG_12_SET, CK_CFG_12_CLR,
		8, 4, 15,
		0x008, 18, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_VDEC, "vdec_sel", vdec_parents,
		CK_CFG_12, CK_CFG_12_SET, CK_CFG_12_CLR,
		16, 4, 23,
		0x008, 19, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_CAMTM, "camtm_sel", camtm_parents,
		CK_CFG_12, CK_CFG_12_SET, CK_CFG_12_CLR,
		24, 2, 31,
		0x004, 20, CLK_IS_CRITICAL),

	/* CLK_CFG_13 */
	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_PWM, "pwm_sel", pwm_parents,
		CK_CFG_13, CK_CFG_13_SET, CK_CFG_13_CLR,
		0, 1, 7,
		0x008, 21, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_AUDIO_H, "audio_h_sel", audio_h_parents,
		CK_CFG_13, CK_CFG_13_SET, CK_CFG_13_CLR,
		8, 2, 15,
		0x008, 22, CLK_IS_CRITICAL),

	MUX_CLR_SET_UPD_FLAGS(TOP_MUX_CAMTG5, "camtg5_sel", camtg5_parents,
		CK_CFG_13, CK_CFG_13_SET, CK_CFG_13_CLR,
		24, 3, 31,
		0x008, 24, CLK_IS_CRITICAL),
#else
	/* CLK_CFG_0 */
	MUX_GATE_CLR_SET_UPD_FLAGS(TOP_MUX_AXI, "axi_sel", axi_parents,
		CK_CFG_0, CK_CFG_0_SET, CK_CFG_0_CLR,
		0, 2, 7,
		0x004, 0, CLK_IS_CRITICAL),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_MM, "mm_sel", mm_parents,
		CK_CFG_0, CK_CFG_0_SET, CK_CFG_0_CLR,
		8, 3, 15,
		0x004, 1),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_SCP, "scp_sel", scp_parents,
		CK_CFG_0, CK_CFG_0_SET, CK_CFG_0_CLR,
		16, 3, 23,
		0x004, 2),

	/* CLK_CFG_1 */
	MUX_GATE_CLR_SET_UPD(TOP_MUX_IMG, "img_sel", img_parents,
		CK_CFG_1, CK_CFG_1_SET, CK_CFG_1_CLR,
		0, 3, 7,
		0x004, 4),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_CAM, "cam_sel", cam_parents,
		CK_CFG_1, CK_CFG_1_SET, CK_CFG_1_CLR,
		24, 4, 31,
		0x004, 7),

	/* CLK_CFG_2 */
	MUX_GATE_CLR_SET_UPD(TOP_MUX_DSP, "dsp_sel", dsp_parents,
		CK_CFG_2, CK_CFG_2_SET, CK_CFG_2_CLR,
		8, 4, 15,
		0x004, 9),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_DSP1, "dsp1_sel", dsp1_parents,
		CK_CFG_2, CK_CFG_2_SET, CK_CFG_2_CLR,
		16, 4, 23,
		0x004, 10),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_DSP2, "dsp2_sel", dsp2_parents,
		CK_CFG_2, CK_CFG_2_SET, CK_CFG_2_CLR,
		24, 4, 31,
		0x004, 11),

	/* CLK_CFG_3 */
	MUX_GATE_CLR_SET_UPD(TOP_MUX_IPU_IF, "ipu_if_sel", ipu_if_parents,
		CK_CFG_3, CK_CFG_3_SET, CK_CFG_3_CLR,
		8, 4, 15,
		0x004, 13),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_MFG, "mfg_sel", mfg_parents,
		CK_CFG_3, CK_CFG_3_SET, CK_CFG_3_CLR,
		16, 2, 23,
		0x004, 14),

	/* CLK_CFG_4 */
	MUX_GATE_CLR_SET_UPD(TOP_MUX_CAMTG, "camtg_sel", camtg_parents,
		CK_CFG_4, CK_CFG_4_SET, CK_CFG_4_CLR,
		0, 3, 7,
		0x004, 16),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_CAMTG2, "camtg2_sel", camtg2_parents,
		CK_CFG_4, CK_CFG_4_SET, CK_CFG_4_CLR,
		8, 3, 15,
		0x004, 17),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_CAMTG3, "camtg3_sel", camtg3_parents,
		CK_CFG_4, CK_CFG_4_SET, CK_CFG_4_CLR,
		16, 3, 23,
		0x004, 18),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_CAMTG4, "camtg4_sel", camtg4_parents,
		CK_CFG_4, CK_CFG_4_SET, CK_CFG_4_CLR,
		24, 3, 31,
		0x004, 19),

	/* CLK_CFG_5 */
	MUX_GATE_CLR_SET_UPD(TOP_MUX_UART, "uart_sel", uart_parents,
		CK_CFG_5, CK_CFG_5_SET, CK_CFG_5_CLR,
		0, 1, 7,
		0x004, 20),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_SPI, "spi_sel", spi_parents,
		CK_CFG_5, CK_CFG_5_SET, CK_CFG_5_CLR,
		8, 2, 15,
		0x004, 21),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_MSDC50_0_HCLK, "msdc50_hclk_sel",
		msdc50_hclk_parents,
		CK_CFG_5, CK_CFG_5_SET, CK_CFG_5_CLR,
		16, 2, 23,
		0x004, 22),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_MSDC50_0, "msdc50_0_sel",
		msdc50_0_parents,
		CK_CFG_5, CK_CFG_5_SET, CK_CFG_5_CLR,
		24, 3, 31,
		0x004, 23),

	/* CLK_CFG_6 */
	MUX_GATE_CLR_SET_UPD(TOP_MUX_MSDC30_1, "msdc30_1_sel",
		msdc30_1_parents,
		CK_CFG_6, CK_CFG_6_SET, CK_CFG_6_CLR,
		0, 3, 7,
		0x004, 24),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_AUDIO, "audio_sel", audio_parents,
		CK_CFG_6, CK_CFG_6_SET, CK_CFG_6_CLR,
		8, 2, 15,
		0x004, 25),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_AUD_INTBUS, "aud_intbus_sel",
		aud_intbus_parents,
		CK_CFG_6, CK_CFG_6_SET, CK_CFG_6_CLR,
		16, 2, 23,
		0x004, 26),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_FPWRAP_ULPOSC, "fpwrap_ulposc_sel",
		fpwrap_ulposc_parents,
		CK_CFG_6, CK_CFG_6_SET, CK_CFG_6_CLR,
		24, 3, 31,
		0x004, 27),

	/* CLK_CFG_7 */
	MUX_GATE_CLR_SET_UPD(TOP_MUX_ATB, "atb_sel", atb_parents,
		CK_CFG_7, CK_CFG_7_SET, CK_CFG_7_CLR,
		0, 2, 7,
		0x004, 28),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_SSPM, "sspm_sel", sspm_parents,
		CK_CFG_7, CK_CFG_7_SET, CK_CFG_7_CLR,
		8, 3, 15,
		0x004, 29),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_DPI0, "dpi0_sel", dpi0_parents,
		CK_CFG_7, CK_CFG_7_SET, CK_CFG_7_CLR,
		16, 3, 23,
		0x004, 30),

	/* CLK_CFG_8 */
	MUX_GATE_CLR_SET_UPD(TOP_MUX_DISP_PWM, "disppwm_sel", disppwm_parents,
		CK_CFG_8, CK_CFG_8_SET, CK_CFG_8_CLR,
		0, 3, 7,
		0x008, 1),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_USB_TOP, "usb_top_sel", usb_top_parents,
		CK_CFG_8, CK_CFG_8_SET, CK_CFG_8_CLR,
		8, 2, 15,
		0x008, 2),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_SSUSB_TOP_XHCI, "ssusb_top_xhci_sel",
		ssusb_top_xhci_parents,
		CK_CFG_8, CK_CFG_8_SET, CK_CFG_8_CLR,
		16, 2, 23,
		0x008, 3),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_SPM, "spm_sel", spm_parents,
		CK_CFG_8, CK_CFG_8_SET, CK_CFG_8_CLR,
		24, 2, 31,
		0x008, 4),

	/* CLK_CFG_9 */
	MUX_GATE_CLR_SET_UPD(TOP_MUX_I2C, "i2c_sel", i2c_parents,
		CK_CFG_9, CK_CFG_9_SET, CK_CFG_9_CLR,
		0, 2, 7,
		0x008, 5),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_SENINF, "seninf_sel", seninf_parents,
		CK_CFG_9, CK_CFG_9_SET, CK_CFG_9_CLR,
		8, 2, 15,
		0x008, 6),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_SENINF1, "seninf1_sel", seninf1_parents,
		CK_CFG_9, CK_CFG_9_SET, CK_CFG_9_CLR,
		16, 2, 23,
		0x008, 7),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_SENINF2, "seninf2_sel", seninf2_parents,
		CK_CFG_9, CK_CFG_9_SET, CK_CFG_9_CLR,
		24, 2, 31,
		0x008, 8),

	/* CLK_CFG_10 */
	MUX_GATE_CLR_SET_UPD(TOP_MUX_DXCC, "dxcc_sel", dxcc_parents,
		CK_CFG_10, CK_CFG_10_SET, CK_CFG_10_CLR,
		0, 2, 7,
		0x008, 9),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_AUD_ENG1, "aud_eng1_sel",
		aud_eng1_parents,
		CK_CFG_10, CK_CFG_10_SET, CK_CFG_10_CLR,
		8, 2, 15,
		0x008, 10),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_AUD_ENG2, "aud_eng2_sel",
		aud_eng2_parents,
		CK_CFG_10, CK_CFG_10_SET, CK_CFG_10_CLR,
		16, 2, 23,
		0x008, 11),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_FAES_UFSFDE, "faes_ufsfde_sel",
		faes_ufsfde_parents,
		CK_CFG_10, CK_CFG_10_SET, CK_CFG_10_CLR,
		24, 3, 31,
		0x008, 12),

	/* CLK_CFG_11 */
	MUX_GATE_CLR_SET_UPD(TOP_MUX_FUFS, "fufs_sel", fufs_parents,
		CK_CFG_11, CK_CFG_11_SET, CK_CFG_11_CLR,
		0, 2, 7,
		0x008, 13),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_AUD_1, "aud_1_sel", aud_1_parents,
		CK_CFG_11, CK_CFG_11_SET, CK_CFG_11_CLR,
		8, 1, 15,
		0x008, 14),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_AUD_2, "aud_2_sel", aud_2_parents,
		CK_CFG_11, CK_CFG_11_SET, CK_CFG_11_CLR,
		16, 1, 23,
		0x008, 15),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_ADSP, "adsp_sel", adsp_parents,
		CK_CFG_11, CK_CFG_11_SET, CK_CFG_11_CLR,
		24, 3, 31,
		0x008, 16),

	/* CLK_CFG_12 */
	MUX_GATE_CLR_SET_UPD(TOP_MUX_DPMAIF, "dpmaif_sel", dpmaif_parents,
		CK_CFG_12, CK_CFG_12_SET, CK_CFG_12_CLR,
		0, 3, 7,
		0x008, 17),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_VENC, "venc_sel", venc_parents,
		CK_CFG_12, CK_CFG_12_SET, CK_CFG_12_CLR,
		8, 4, 15,
		0x008, 18),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_VDEC, "vdec_sel", vdec_parents,
		CK_CFG_12, CK_CFG_12_SET, CK_CFG_12_CLR,
		16, 4, 23,
		0x008, 19),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_CAMTM, "camtm_sel", camtm_parents,
		CK_CFG_12, CK_CFG_12_SET, CK_CFG_12_CLR,
		24, 2, 31,
		0x004, 20),

	/* CLK_CFG_13 */
	MUX_GATE_CLR_SET_UPD(TOP_MUX_PWM, "pwm_sel", pwm_parents,
		CK_CFG_13, CK_CFG_13_SET, CK_CFG_13_CLR,
		0, 1, 7,
		0x008, 21),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_AUDIO_H, "audio_h_sel", audio_h_parents,
		CK_CFG_13, CK_CFG_13_SET, CK_CFG_13_CLR,
		8, 2, 15,
		0x008, 22),

	MUX_GATE_CLR_SET_UPD(TOP_MUX_CAMTG5, "camtg5_sel", camtg5_parents,
		CK_CFG_13, CK_CFG_13_SET, CK_CFG_13_CLR,
		24, 3, 31,
		0x008, 24),
#endif
};

static const struct mtk_composite top_audmuxes[] = {

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

static int mtk_cg_enable_check(struct clk_hw *hw)
{
	unsigned int cam_mux_pdn = clk_readl(CLK_CFG_1) >> TOPCK_CAM_PDN;

	if ((!cam_mux_pdn) && cam_if_on())
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

static void mtk_cg_disable_check(struct clk_hw *hw)
{
	unsigned int cam_mux_pdn = clk_readl(CLK_CFG_1) >> TOPCK_CAM_PDN;

	if ((!cam_mux_pdn) && cam_if_on())
		mtk_cg_set_bit(hw);
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

static int mtk_cg_enable_dump(struct clk_hw *hw)
{
	mtk_cg_clr_bit(hw);

	/*if (!strcmp(__clk_get_name(hw->clk), "mm_mdp_tdshp"))*/
	pr_notice("%s: %s (%08x)\r\n", __func__,
		__clk_get_name(hw->clk), clk_readl(MM_CG_CON0));

	return 0;
}

static void mtk_cg_disable_dump(struct clk_hw *hw)
{
	mtk_cg_set_bit(hw);

	/*if (!strcmp(__clk_get_name(hw->clk), "mm_mdp_tdshp"))*/
	pr_notice("%s: %s (%08x)\r\n", __func__,
		__clk_get_name(hw->clk), clk_readl(MM_CG_CON0));
}

const struct clk_ops mtk_clk_gate_ops = {
	.is_enabled	= mtk_cg_bit_is_cleared,
	.enable		= mtk_cg_enable,
	.disable	= mtk_cg_disable,
};

const struct clk_ops mtk_clk_gate_ops_check = {
	.is_enabled	= mtk_cg_bit_is_cleared,
	.enable		= mtk_cg_enable_check,
	.disable	= mtk_cg_disable_check,
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

/*
const struct clk_ops mtk_clk_gate_ops_setclr_inv_dummy = {
	.is_enabled	= mtk_cg_bit_is_set,
	.enable		= mtk_cg_enable_inv,
	.disable	= mtk_cg_disable_inv_dummy,
};
*/

const struct clk_ops mtk_clk_gate_ops_setclr_dump = {
	.is_enabled	= mtk_cg_bit_is_cleared,
	.enable		= mtk_cg_enable_dump,
	.disable	= mtk_cg_disable_dump,
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

static const struct mtk_gate infra_clks[] = {
#if MT_CCF_BRINGUP
	/* INFRA0 */
	GATE_INFRA0_DUMMY(INFRACFG_AO_PMIC_CG_TMR, "infra_pmic_tmr",
		"axi_sel", 0),
	GATE_INFRA0_DUMMY(INFRACFG_AO_PMIC_CG_AP, "infra_pmic_ap",
		"axi_sel", 1),
	GATE_INFRA0_DUMMY(INFRACFG_AO_PMIC_CG_MD, "infra_pmic_md",
		"axi_sel", 2),
	GATE_INFRA0_DUMMY(INFRACFG_AO_PMIC_CG_CONN, "infra_pmic_conn",
		"axi_sel", 3),
	GATE_INFRA0_DUMMY(INFRACFG_AO_SCPSYS_CG, "infra_scp",
		"axi_sel", 4),
	GATE_INFRA0_DUMMY(INFRACFG_AO_SEJ_CG, "infra_sej",
		"f_f26m_ck", 5),
	GATE_INFRA0_DUMMY(INFRACFG_AO_APXGPT_CG, "infra_apxgpt",
		"axi_sel", 6),
	GATE_INFRA0_DUMMY(INFRACFG_AO_ICUSB_CG, "infra_icusb",
		"axi_sel", 8),
	GATE_INFRA0_DUMMY(INFRACFG_AO_GCE_CG, "infra_gce",
		"axi_sel", 9),
	GATE_INFRA0_DUMMY(INFRACFG_AO_THERM_CG, "infra_therm",
		"axi_sel", 10),
	GATE_INFRA0_DUMMY(INFRACFG_AO_I2C0_CG, "infra_i2c0",
		"i2c_sel", 11),
	GATE_INFRA0_DUMMY(INFRACFG_AO_I2C1_CG, "infra_i2c1",
		"i2c_sel", 12),
	GATE_INFRA0_DUMMY(INFRACFG_AO_I2C2_CG, "infra_i2c2",
		"i2c_sel", 13),
	GATE_INFRA0_DUMMY(INFRACFG_AO_I2C3_CG, "infra_i2c3",
		"i2c_sel", 14),
	GATE_INFRA0_DUMMY(INFRACFG_AO_PWM_HCLK_CG, "infra_pwm_hclk",
		"pwm_sel", 15),
	GATE_INFRA0_DUMMY(INFRACFG_AO_PWM1_CG, "infra_pwm1",
		"pwm_sel", 16),
	GATE_INFRA0_DUMMY(INFRACFG_AO_PWM2_CG, "infra_pwm2",
		"pwm_sel", 17),
	GATE_INFRA0_DUMMY(INFRACFG_AO_PWM3_CG, "infra_pwm3",
		"pwm_sel", 18),
	GATE_INFRA0_DUMMY(INFRACFG_AO_PWM4_CG, "infra_pwm4",
		"pwm_sel", 19),
	GATE_INFRA0_DUMMY(INFRACFG_AO_PWM_CG, "infra_pwm",
		"pwm_sel", 21),
	GATE_INFRA0_DUMMY(INFRACFG_AO_UART0_CG, "infra_uart0",
		"uart_sel", 22),
	GATE_INFRA0_DUMMY(INFRACFG_AO_UART1_CG, "infra_uart1",
		"uart_sel", 23),
	GATE_INFRA0_DUMMY(INFRACFG_AO_UART2_CG, "infra_uart2",
		"uart_sel", 24),
	GATE_INFRA0_DUMMY(INFRACFG_AO_UART3_CG, "infra_uart3",
		"uart_sel", 25),
	GATE_INFRA0_DUMMY(INFRACFG_AO_GCE_26M_CG, "infra_gce_26m",
		"axi_sel", 27),
	GATE_INFRA0_DUMMY(INFRACFG_AO_CQ_DMA_FPC_CG, "infra_cqdma_fpc",
		"axi_sel", 28),
	GATE_INFRA0_DUMMY(INFRACFG_AO_BTIF_CG, "infra_btif",
		"axi_sel", 31),
	/* INFRA1 */
	GATE_INFRA1_DUMMY(INFRACFG_AO_SPI0_CG, "infra_spi0",
		"spi_sel", 1),
	GATE_INFRA1_DUMMY(INFRACFG_AO_MSDC0_CG, "infra_msdc0",
		"msdc50_hclk_sel", 2),
	GATE_INFRA1_DUMMY(INFRACFG_AO_MSDC1_CG, "infra_msdc1",
		"axi_sel", 4),
	GATE_INFRA1_DUMMY(INFRACFG_AO_MSDC2_CG, "infra_msdc2",
		"axi_sel", 5),
	GATE_INFRA1_DUMMY(INFRACFG_AO_MSDC0_SCK_CG, "infra_msdc0_sck",
		"msdc50_0_sel", 6),
	GATE_INFRA1_DUMMY(INFRACFG_AO_DVFSRC_CG, "infra_dvfsrc",
		"f_f26m_ck", 7),
	GATE_INFRA1_DUMMY(INFRACFG_AO_GCPU_CG, "infra_gcpu",
		"axi_sel", 8),
	GATE_INFRA1_DUMMY(INFRACFG_AO_TRNG_CG, "infra_trng",
		"axi_sel", 9),
	GATE_INFRA1_DUMMY(INFRACFG_AO_AUXADC_CG, "infra_auxadc",
		"f_f26m_ck", 10),
	GATE_INFRA1_DUMMY(INFRACFG_AO_CPUM_CG, "infra_cpum",
		"axi_sel", 11),
	GATE_INFRA1_DUMMY(INFRACFG_AO_CCIF1_AP_CG, "infra_ccif1_ap",
		"axi_sel", 12),
	GATE_INFRA1_DUMMY(INFRACFG_AO_CCIF1_MD_CG, "infra_ccif1_md",
		"axi_sel", 13),
	GATE_INFRA1_DUMMY(INFRACFG_AO_AUXADC_MD_CG, "infra_auxadc_md",
		"f_f26m_ck", 14),
	GATE_INFRA1_DUMMY(INFRACFG_AO_MSDC1_SCK_CG, "infra_msdc1_sck",
		"msdc30_1_sel", 16),
	GATE_INFRA1_DUMMY(INFRACFG_AO_MSDC2_SCK_CG, "infra_msdc2_sck",
		"msdc30_2_sel", 17),
	GATE_INFRA1_DUMMY(INFRACFG_AO_AP_DMA_CG, "infra_apdma",
		"axi_sel", 18),
	GATE_INFRA1_DUMMY(INFRACFG_AO_XIU_CG, "infra_xiu",
		"axi_sel", 19),
	GATE_INFRA1_DUMMY(INFRACFG_AO_DEVICE_APC_CG, "infra_devapc",
		"axi_sel", 20),
	GATE_INFRA1_DUMMY(INFRACFG_AO_CCIF_AP_CG, "infra_ccif_ap",
		"axi_sel", 23),
	GATE_INFRA1_DUMMY(INFRACFG_AO_DEBUGSYS_CG, "infra_debugsys",
		"axi_sel", 24),
	GATE_INFRA1_DUMMY(INFRACFG_AO_AUDIO_CG, "infra_audio",
		"axi_sel", 25),
	GATE_INFRA1_DUMMY(INFRACFG_AO_CCIF_MD_CG, "infra_ccif_md",
		"axi_sel", 26),
	GATE_INFRA1_DUMMY(INFRACFG_AO_DXCC_SEC_CORE_CG, "infra_dxcc_sec_core",
		"dxcc_sel", 27),
	GATE_INFRA1_DUMMY(INFRACFG_AO_DXCC_AO_CG, "infra_dxcc_ao",
		"dxcc_sel", 28),
	GATE_INFRA1_DUMMY(INFRACFG_AO_DEVMPU_BCLK_CG, "infra_devmpu_bclk",
		"axi_sel", 30),
	GATE_INFRA1_DUMMY(INFRACFG_AO_DRAMC_F26M_CG, "infra_dramc_f26m",
		"f_f26m_ck", 31),
	/* INFRA2 */
	GATE_INFRA2_DUMMY(INFRACFG_AO_IRTX_CG, "infra_irtx",
		"f_f26m_ck", 0),
	GATE_INFRA2_DUMMY(INFRACFG_AO_USB_CG, "infra_usb",
		"usb_top_sel", 1),
	GATE_INFRA2_DUMMY(INFRACFG_AO_DISP_PWM_CG, "infra_disppwm",
		"axi_sel", 2),
	GATE_INFRA2_DUMMY(INFRACFG_AO_AUDIO_26M_BCLK_CK,
		"infracfg_ao_audio_26m_bclk_ck", "f_f26m_ck", 4),
	GATE_INFRA2_DUMMY(INFRACFG_AO_SPI1_CG, "infra_spi1",
		"spi_sel", 6),
	GATE_INFRA2_DUMMY(INFRACFG_AO_I2C4_CG, "infra_i2c4",
		"i2c_sel", 7),
	GATE_INFRA2_DUMMY(INFRACFG_AO_MODEM_TEMP_SHARE_CG,
		"infra_md_tmp_share",
		"f_f26m_ck", 8),
	GATE_INFRA2_DUMMY(INFRACFG_AO_SPI2_CG, "infra_spi2",
		"spi_sel", 9),
	GATE_INFRA2_DUMMY(INFRACFG_AO_SPI3_CG, "infra_spi3",
		"spi_sel", 10),
	GATE_INFRA2_DUMMY(INFRACFG_AO_UNIPRO_SCK_CG, "infra_unipro_sck",
		"fufs_sel", 11),
	GATE_INFRA2_DUMMY(INFRACFG_AO_UNIPRO_TICK_CG, "infra_unipro_tick",
		"fufs_sel", 12),
	GATE_INFRA2_DUMMY(INFRACFG_AO_UFS_MP_SAP_BCLK_CG,
		"infra_ufs_mp_sap_bck",
		"fufs_sel", 13),
	GATE_INFRA2_DUMMY(INFRACFG_AO_MD32_BCLK_CG, "infra_md32_bclk",
		"axi_sel", 14),
	GATE_INFRA2_DUMMY(INFRACFG_AO_SSPM_CG, "infra_sspm",
		"sspm_sel", 15),
	GATE_INFRA2_DUMMY(INFRACFG_AO_UNIPRO_MBIST_CG, "infra_unipro_mbist",
		"axi_sel", 16),
	GATE_INFRA2_DUMMY(INFRACFG_AO_SSPM_BUS_HCLK_CG, "infra_sspm_bus_hclk",
		"axi_sel", 17),
	GATE_INFRA2_DUMMY(INFRACFG_AO_I2C5_CG, "infra_i2c5",
		"i2c_sel", 18),
	GATE_INFRA2_DUMMY(INFRACFG_AO_I2C5_ARBITER_CG, "infra_i2c5_arbiter",
		"i2c_sel", 19),
	GATE_INFRA2_DUMMY(INFRACFG_AO_I2C5_IMM_CG, "infra_i2c5_imm",
		"i2c_sel", 20),
	GATE_INFRA2_DUMMY(INFRACFG_AO_I2C1_ARBITER_CG, "infra_i2c1_arbiter",
		"i2c_sel", 21),
	GATE_INFRA2_DUMMY(INFRACFG_AO_I2C1_IMM_CG, "infra_i2c1_imm",
		"i2c_sel", 22),
	GATE_INFRA2_DUMMY(INFRACFG_AO_I2C2_ARBITER_CG, "infra_i2c2_arbiter",
		"i2c_sel", 23),
	GATE_INFRA2_DUMMY(INFRACFG_AO_I2C2_IMM_CG, "infra_i2c2_imm",
		"i2c_sel", 24),
	GATE_INFRA2_DUMMY(INFRACFG_AO_SPI4_CG, "infra_spi4",
		"spi_sel", 25),
	GATE_INFRA2_DUMMY(INFRACFG_AO_SPI5_CG, "infra_spi5",
		"spi_sel", 26),
	GATE_INFRA2_DUMMY(INFRACFG_AO_CQ_DMA_CG, "infra_cqdma",
		"axi_sel", 27),
	GATE_INFRA2_DUMMY(INFRACFG_AO_UFS_CG, "infra_ufs",
		"fufs_sel", 28),
	GATE_INFRA2_DUMMY(INFRACFG_AO_AES_UFSFDE_CG, "infra_aes_ufsfde",
		"faes_ufsfde_sel", 29),
	GATE_INFRA2_DUMMY(INFRACFG_AO_UFS_TICK_CG, "infra_ufs_tick",
		"fufs_sel", 30),
	GATE_INFRA2_DUMMY(INFRACFG_AO_SSUSB_XHCI_CG, "infra_ssusb_xhci",
		"ssusb_top_xhci_sel", 31),
	/* INFRA3 */
	GATE_INFRA3_DUMMY(INFRACFG_AO_MSDC0_SELF_CG, "infra_msdc0_self",
		"msdc50_0_sel", 0),
	GATE_INFRA3_DUMMY(INFRACFG_AO_MSDC1_SELF_CG, "infra_msdc1_self",
		"msdc50_0_sel", 1),
	GATE_INFRA3_DUMMY(INFRACFG_AO_MSDC2_SELF_CG, "infra_msdc2_self",
		"msdc50_0_sel", 2),
	GATE_INFRA3_DUMMY(INFRACFG_AO_SSPM_26M_SELF_CG, "infra_sspm_26m_self",
		"f_f26m_ck", 3),
	GATE_INFRA3_DUMMY(INFRACFG_AO_SSPM_32K_SELF_CG, "infra_sspm_32k_self",
		"f_f26m_ck", 4),
	GATE_INFRA3_DUMMY(INFRACFG_AO_UFS_AXI_CG, "infra_ufs_axi",
		"axi_sel", 5),
	GATE_INFRA3_DUMMY(INFRACFG_AO_I2C6_CG, "infra_i2c6",
		"i2c_sel", 6),
	GATE_INFRA3_DUMMY(INFRACFG_AO_AP_MSDC0_CG, "infra_ap_msdc0",
		"msdc50_hclk_sel", 7),
	GATE_INFRA3_DUMMY(INFRACFG_AO_MD_MSDC0_CG, "infra_md_msdc0",
		"msdc50_hclk_sel", 8),

	GATE_INFRA3_DUMMY(INFRACFG_AO_FADSP_26M_CG, "infra_adsp_26m",
		"axi_sel", 12),
	GATE_INFRA3_DUMMY(INFRACFG_AO_FADSP_32K_CG, "infra_adsp_32k",
		"axi_sel", 13),


	GATE_INFRA3_DUMMY(INFRACFG_AO_CCIF2_AP_CG, "infra_ccif2_ap",
		"axi_sel", 16),
	GATE_INFRA3_DUMMY(INFRACFG_AO_CCIF2_MD_CG, "infra_ccif2_md",
		"axi_sel", 17),
	GATE_INFRA3_DUMMY(INFRACFG_AO_CCIF3_AP_CG, "infra_ccif3_ap",
		"axi_sel", 18),
	GATE_INFRA3_DUMMY(INFRACFG_AO_CCIF3_MD_CG, "infra_ccif3_md",
		"axi_sel", 19),
	GATE_INFRA3_DUMMY(INFRACFG_AO_SEJ_F13M_CG, "infra_sej_f13m",
		"f_f26m_ck", 20),
	GATE_INFRA3_DUMMY(INFRACFG_AO_AES_BCLK_CG, "infra_aes_bclk",
		"axi_sel", 21),
	GATE_INFRA3_DUMMY(INFRACFG_AO_I2C7_CG, "infra_i2c7",
		"i2c_sel", 22),
	GATE_INFRA3_DUMMY(INFRACFG_AO_I2C8_CG, "infra_i2c8",
		"i2c_sel", 23),
	GATE_INFRA3_DUMMY(INFRACFG_AO_FBIST2FPC_CG, "infra_fbist2fpc",
		"msdc50_0_sel", 24),

	GATE_INFRA3_DUMMY(INFRACFG_AO_DEVICE_APC_SYNC_CG, "infra_devapc_sync",
		"axi_sel", 25),

	GATE_INFRA3_DUMMY(INFRACFG_AO_DPMAIF_CK, "infra_dpmaif",
		"dpmaif_sel", 26),
	GATE_INFRA3_DUMMY(INFRACFG_AO_FADSP_CG, "infra_fadsp",
		"adsp_sel", 27),
	GATE_INFRA3_DUMMY(INFRACFG_AO_CCIF4_AP_CG, "infra_ccif4_ap",
		"axi_sel", 28),
	GATE_INFRA3_DUMMY(INFRACFG_AO_CCIF4_MD_CG, "infra_ccif4_md",
		"axi_sel", 29),
	GATE_INFRA3_DUMMY(INFRACFG_AO_SPI6_CG, "infra_spi6",
		"spi_sel", 30),
	GATE_INFRA3_DUMMY(INFRACFG_AO_SPI7_CG, "infra_spi7",
		"spi_sel", 31),

#else
	/* INFRA0 */
	GATE_INFRA0(INFRACFG_AO_PMIC_CG_TMR, "infra_pmic_tmr",
		"axi_sel", 0),
	GATE_INFRA0(INFRACFG_AO_PMIC_CG_AP, "infra_pmic_ap",
		"axi_sel", 1),
	GATE_INFRA0(INFRACFG_AO_PMIC_CG_MD, "infra_pmic_md",
		"axi_sel", 2),
	GATE_INFRA0(INFRACFG_AO_PMIC_CG_CONN, "infra_pmic_conn",
		"axi_sel", 3),
	GATE_INFRA0(INFRACFG_AO_SCPSYS_CG, "infra_scp",
		"axi_sel", 4),
	GATE_INFRA0(INFRACFG_AO_SEJ_CG, "infra_sej",
		"f_f26m_ck", 5),
	GATE_INFRA0(INFRACFG_AO_APXGPT_CG, "infra_apxgpt",
		"axi_sel", 6),
	GATE_INFRA0(INFRACFG_AO_ICUSB_CG, "infra_icusb",
		"axi_sel", 8),
	GATE_INFRA0(INFRACFG_AO_GCE_CG, "infra_gce",
		"axi_sel", 9),
	GATE_INFRA0(INFRACFG_AO_THERM_CG, "infra_therm",
		"axi_sel", 10),
	GATE_INFRA0(INFRACFG_AO_I2C0_CG, "infra_i2c0",
		"i2c_sel", 11),
	GATE_INFRA0(INFRACFG_AO_I2C1_CG, "infra_i2c1",
		"i2c_sel", 12),
	GATE_INFRA0(INFRACFG_AO_I2C2_CG, "infra_i2c2",
		"i2c_sel", 13),
	GATE_INFRA0(INFRACFG_AO_I2C3_CG, "infra_i2c3",
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
	GATE_INFRA0(INFRACFG_AO_CQ_DMA_FPC_CG, "infra_cqdma_fpc",
		"axi_sel", 28),
	GATE_INFRA0(INFRACFG_AO_BTIF_CG, "infra_btif",
		"axi_sel", 31),
	/* INFRA1 */
	GATE_INFRA1(INFRACFG_AO_SPI0_CG, "infra_spi0",
		"spi_sel", 1),
	GATE_INFRA1(INFRACFG_AO_MSDC0_CG, "infra_msdc0",
		"msdc50_hclk_sel", 2),
	GATE_INFRA1(INFRACFG_AO_MSDC1_CG, "infra_msdc1",
		"axi_sel", 4),
	GATE_INFRA1(INFRACFG_AO_MSDC2_CG, "infra_msdc2",
		"axi_sel", 5),
	GATE_INFRA1(INFRACFG_AO_MSDC0_SCK_CG, "infra_msdc0_sck",
		"msdc50_0_sel", 6),
	GATE_INFRA1(INFRACFG_AO_DVFSRC_CG, "infra_dvfsrc",
		"f_f26m_ck", 7),
	GATE_INFRA1(INFRACFG_AO_GCPU_CG, "infra_gcpu",
		"axi_sel", 8),
	GATE_INFRA1(INFRACFG_AO_TRNG_CG, "infra_trng",
		"axi_sel", 9),
	GATE_INFRA1(INFRACFG_AO_AUXADC_CG, "infra_auxadc",
		"f_f26m_ck", 10),
	GATE_INFRA1(INFRACFG_AO_CPUM_CG, "infra_cpum",
		"axi_sel", 11),
	GATE_INFRA1(INFRACFG_AO_CCIF1_AP_CG, "infra_ccif1_ap",
		"axi_sel", 12),
	GATE_INFRA1(INFRACFG_AO_CCIF1_MD_CG, "infra_ccif1_md",
		"axi_sel", 13),
	GATE_INFRA1(INFRACFG_AO_AUXADC_MD_CG, "infra_auxadc_md",
		"f_f26m_ck", 14),
	GATE_INFRA1(INFRACFG_AO_MSDC1_SCK_CG, "infra_msdc1_sck",
		"msdc30_1_sel", 16),
	GATE_INFRA1(INFRACFG_AO_MSDC2_SCK_CG, "infra_msdc2_sck",
		"msdc30_2_sel", 17),
	GATE_INFRA1(INFRACFG_AO_AP_DMA_CG, "infra_apdma",
		"axi_sel", 18),
	GATE_INFRA1(INFRACFG_AO_XIU_CG, "infra_xiu",
		"axi_sel", 19),
	GATE_INFRA1(INFRACFG_AO_DEVICE_APC_CG, "infra_device_apc",
		"axi_sel", 20),
	GATE_INFRA1(INFRACFG_AO_CCIF_AP_CG, "infra_ccif_ap",
		"axi_sel", 23),
	GATE_INFRA1(INFRACFG_AO_DEBUGSYS_CG, "infra_debugsys",
		"axi_sel", 24),
	GATE_INFRA1(INFRACFG_AO_AUDIO_CG, "infra_audio",
		"axi_sel", 25),
	GATE_INFRA1(INFRACFG_AO_CCIF_MD_CG, "infra_ccif_md",
		"axi_sel", 26),
	GATE_INFRA1(INFRACFG_AO_DXCC_SEC_CORE_CG, "infra_dxcc_sec_core",
		"dxcc_sel", 27),
	GATE_INFRA1(INFRACFG_AO_DXCC_AO_CG, "infra_dxcc_ao",
		"dxcc_sel", 28),
	GATE_INFRA1(INFRACFG_AO_DEVMPU_BCLK_CG, "infra_devmpu_bclk",
		"axi_sel", 30),
	GATE_INFRA1(INFRACFG_AO_DRAMC_F26M_CG, "infra_dramc_f26m",
		"f_f26m_ck", 31),
	/* INFRA2 */
	GATE_INFRA2(INFRACFG_AO_IRTX_CG, "infra_irtx",
		"f_f26m_ck", 0),
	GATE_INFRA2(INFRACFG_AO_USB_CG, "infra_usb",
		"usb_top_sel", 1),
	GATE_INFRA2(INFRACFG_AO_DISP_PWM_CG, "infra_disppwm",
		"axi_sel", 2),
	GATE_INFRA2(INFRACFG_AO_AUDIO_26M_BCLK_CK,
		"infracfg_ao_audio_26m_bclk_ck", "f_f26m_ck", 4),
	GATE_INFRA2(INFRACFG_AO_SPI1_CG, "infra_spi1",
		"spi_sel", 6),
	GATE_INFRA2(INFRACFG_AO_I2C4_CG, "infra_i2c4",
		"i2c_sel", 7),
	GATE_INFRA2(INFRACFG_AO_MODEM_TEMP_SHARE_CG, "infra_md_tmp_share",
		"f_f26m_ck", 8),
	GATE_INFRA2(INFRACFG_AO_SPI2_CG, "infra_spi2",
		"spi_sel", 9),
	GATE_INFRA2(INFRACFG_AO_SPI3_CG, "infra_spi3",
		"spi_sel", 10),
	GATE_INFRA2(INFRACFG_AO_UNIPRO_SCK_CG, "infra_unipro_sck",
		"fufs_sel", 11),
	GATE_INFRA2(INFRACFG_AO_UNIPRO_TICK_CG, "infra_unipro_tick",
		"fufs_sel", 12),
	GATE_INFRA2(INFRACFG_AO_UFS_MP_SAP_BCLK_CG, "infra_ufs_mp_sap_bck",
		"fufs_sel", 13),
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
	GATE_INFRA2(INFRACFG_AO_UFS_CG, "infra_ufs",
		"fufs_sel", 28),
	GATE_INFRA2(INFRACFG_AO_AES_UFSFDE_CG, "infra_aes_ufsfde",
		"faes_ufsfde_sel", 29),
	GATE_INFRA2(INFRACFG_AO_UFS_TICK_CG, "infra_ufs_tick",
		"fufs_sel", 30),
	GATE_INFRA2(INFRACFG_AO_SSUSB_XHCI_CG, "infra_ssusb_xhci",
		"ssusb_top_xhci_sel", 31),
	/* INFRA3 */
	GATE_INFRA3(INFRACFG_AO_MSDC0_SELF_CG, "infra_msdc0_self",
		"msdc50_0_sel", 0),
	GATE_INFRA3(INFRACFG_AO_MSDC1_SELF_CG, "infra_msdc1_self",
		"msdc50_0_sel", 1),
	GATE_INFRA3(INFRACFG_AO_MSDC2_SELF_CG, "infra_msdc2_self",
		"msdc50_0_sel", 2),
	GATE_INFRA3(INFRACFG_AO_SSPM_26M_SELF_CG, "infra_sspm_26m_self",
		"f_f26m_ck", 3),
	GATE_INFRA3(INFRACFG_AO_SSPM_32K_SELF_CG, "infra_sspm_32k_self",
		"f_f26m_ck", 4),
	GATE_INFRA3(INFRACFG_AO_UFS_AXI_CG, "infra_ufs_axi",
		"axi_sel", 5),
	GATE_INFRA3(INFRACFG_AO_I2C6_CG, "infra_i2c6",
		"i2c_sel", 6),
	GATE_INFRA3(INFRACFG_AO_AP_MSDC0_CG, "infra_ap_msdc0",
		"msdc50_hclk_sel", 7),
	GATE_INFRA3(INFRACFG_AO_MD_MSDC0_CG, "infra_md_msdc0",
		"msdc50_hclk_sel", 8),

	GATE_INFRA3(INFRACFG_AO_FADSP_26M_CG, "infra_adsp_26m",
		"axi_sel", 12),
	GATE_INFRA3(INFRACFG_AO_FADSP_32K_CG, "infra_adsp_32k",
		"axi_sel", 13),

	GATE_INFRA3(INFRACFG_AO_CCIF2_AP_CG, "infra_ccif2_ap",
		"axi_sel", 16),
	GATE_INFRA3(INFRACFG_AO_CCIF2_MD_CG, "infra_ccif2_md",
		"axi_sel", 17),
	GATE_INFRA3(INFRACFG_AO_CCIF3_AP_CG, "infra_ccif3_ap",
		"axi_sel", 18),
	GATE_INFRA3(INFRACFG_AO_CCIF3_MD_CG, "infra_ccif3_md",
		"axi_sel", 19),
	GATE_INFRA3(INFRACFG_AO_SEJ_F13M_CG, "infra_sej_f13m",
		"f_f26m_ck", 20),
	GATE_INFRA3(INFRACFG_AO_AES_BCLK_CG, "infra_aes_bclk",
		"axi_sel", 21),
	GATE_INFRA3(INFRACFG_AO_I2C7_CG, "infra_i2c7",
		"i2c_sel", 22),
	GATE_INFRA3(INFRACFG_AO_I2C8_CG, "infra_i2c8",
		"i2c_sel", 23),
	GATE_INFRA3(INFRACFG_AO_FBIST2FPC_CG, "infra_fbist2fpc",
		"msdc50_0_sel", 24),

	GATE_INFRA3(INFRACFG_AO_DEVICE_APC_SYNC_CG, "infra_devapc_sync",
		"axi_sel", 25),

	GATE_INFRA3(INFRACFG_AO_DPMAIF_CK, "infra_dpmaif",
		"dpmaif_sel", 26),
	GATE_INFRA3(INFRACFG_AO_FADSP_CG, "infra_fadsp",
		"adsp_sel", 27),
	GATE_INFRA3(INFRACFG_AO_CCIF4_AP_CG, "infra_ccif4_ap",
		"axi_sel", 28),
	GATE_INFRA3(INFRACFG_AO_CCIF4_MD_CG, "infra_ccif4_md",
		"axi_sel", 29),
	GATE_INFRA3(INFRACFG_AO_SPI6_CG, "infra_spi6",
		"spi_sel", 30),
	GATE_INFRA3(INFRACFG_AO_SPI7_CG, "infra_spi7",
		"spi_sel", 31),
#endif
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

static const struct mtk_gate mfg_cfg_clks[] = {
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

static const struct mtk_gate apmixed_clks[] = {
#if MT_CCF_BRINGUP
	GATE_APMIXED_DUMMY(APMIXED_SSUSB26M, "apmixed_ssusb26m", "f_f26m_ck",
		4),
	GATE_APMIXED_DUMMY(APMIXED_APPLL26M, "apmixed_appll26m", "f_f26m_ck",
		5),
	GATE_APMIXED_DUMMY(APMIXED_MIPIC0_26M, "apmixed_mipic026m", "f_f26m_ck",
		6),
	GATE_APMIXED_DUMMY(APMIXED_MDPLLGP26M, "apmixed_mdpll26m", "f_f26m_ck",
		7),
	GATE_APMIXED_DUMMY(APMIXED_MMSYS_F26M, "apmixed_mmsys26m", "f_f26m_ck",
		8),
	GATE_APMIXED_DUMMY(APMIXED_UFS26M, "apmixed_ufs26m", "f_f26m_ck",
		9),
	GATE_APMIXED_DUMMY(APMIXED_MIPIC1_26M, "apmixed_mipic126m", "f_f26m_ck",
		11),
	GATE_APMIXED_DUMMY(APMIXED_MEMPLL26M, "apmixed_mempll26m", "f_f26m_ck",
		13),
	GATE_APMIXED_DUMMY(APMIXED_CLKSQ_LVPLL_26M, "apmixed_lvpll26m",
		"f_f26m_ck", 14),
	GATE_APMIXED_DUMMY(APMIXED_MIPID0_26M, "apmixed_mipid026m", "f_f26m_ck",
		16),
	GATE_APMIXED_DUMMY(APMIXED_MIPID1_26M, "apmixed_mipid126m", "f_f26m_ck",
		17),
#else
	GATE_APMIXED(APMIXED_SSUSB26M, "apmixed_ssusb26m", "f_f26m_ck",
		4),
	GATE_APMIXED(APMIXED_APPLL26M, "apmixed_appll26m", "f_f26m_ck",
		5),
	GATE_APMIXED(APMIXED_MIPIC0_26M, "apmixed_mipic026m", "f_f26m_ck",
		6),
	GATE_APMIXED(APMIXED_MDPLLGP26M, "apmixed_mdpll26m", "f_f26m_ck",
		7),
	GATE_APMIXED(APMIXED_MMSYS_F26M, "apmixed_mmsys26m", "f_f26m_ck",
		8),
	GATE_APMIXED(APMIXED_UFS26M, "apmixed_ufs26m", "f_f26m_ck",
		9),
	GATE_APMIXED(APMIXED_MIPIC1_26M, "apmixed_mipic126m", "f_f26m_ck",
		11),
	GATE_APMIXED(APMIXED_MEMPLL26M, "apmixed_mempll26m", "f_f26m_ck",
		13),
	GATE_APMIXED(APMIXED_CLKSQ_LVPLL_26M, "apmixed_lvpll26m", "f_f26m_ck",
		14),
	GATE_APMIXED(APMIXED_MIPID0_26M, "apmixed_mipid026m", "f_f26m_ck",
		16),
	GATE_APMIXED(APMIXED_MIPID1_26M, "apmixed_mipid126m", "f_f26m_ck",
		17),
#endif
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

static const struct mtk_gate audio_clks[] = {
#if MT_CCF_BRINGUP
	/* AUDIO0 */
	GATE_AUDIO0_DUMMY(AUDIO_AFE, "aud_afe", "audio_sel",
		2),
	GATE_AUDIO0_DUMMY(AUDIO_22M, "aud_22m", "aud_eng1_sel",
		8),
	GATE_AUDIO0(AUDIO_24M, "aud_24m", "aud_eng2_sel",
		9),
	GATE_AUDIO0_DUMMY(AUDIO_APLL2_TUNER, "aud_apll2_tuner", "aud_eng2_sel",
		18),
	GATE_AUDIO0_DUMMY(AUDIO_APLL_TUNER, "aud_apll_tuner", "aud_eng1_sel",
		19),
	GATE_AUDIO0_DUMMY(AUDIO_TDM, "aud_tdm", "aud_eng1_sel",
		20),
	GATE_AUDIO0_DUMMY(AUDIO_ADC, "aud_adc", "audio_sel",
		24),
	GATE_AUDIO0_DUMMY(AUDIO_DAC, "aud_dac", "audio_sel",
		25),
	GATE_AUDIO0_DUMMY(AUDIO_DAC_PREDIS, "aud_dac_predis", "audio_sel",
		26),
	GATE_AUDIO0_DUMMY(AUDIO_TML, "aud_tml", "audio_sel",
		27),
	GATE_AUDIO0_DUMMY(AUDIO_NLE, "aud_nle", "audio_sel",
		28),
	/* AUDIO1: hf_faudio_ck/hf_faud_engen1_ck/hf_faud_engen2_ck */
	GATE_AUDIO1_DUMMY(AUDIO_I2S1_BCLK_SW, "aud_i2s1_bclk", "audio_sel",
		4),
	GATE_AUDIO1_DUMMY(AUDIO_I2S2_BCLK_SW, "aud_i2s2_bclk", "audio_sel",
		5),
	GATE_AUDIO1_DUMMY(AUDIO_I2S3_BCLK_SW, "aud_i2s3_bclk", "audio_sel",
		6),
	GATE_AUDIO1_DUMMY(AUDIO_I2S4_BCLK_SW, "aud_i2s4_bclk", "audio_sel",
		7),
	GATE_AUDIO1_DUMMY(AUDIO_I2S5_BCLK_SW, "aud_i2s5_bclk", "audio_sel",
		8),

	GATE_AUDIO1_DUMMY(AUDIO_CONN_I2S_ASRC, "aud_conn_i2s", "audio_sel",
		12),
	GATE_AUDIO1_DUMMY(AUDIO_GENERAL1_ASRC, "aud_general1", "audio_sel",
		13),
	GATE_AUDIO1_DUMMY(AUDIO_GENERAL2_ASRC, "aud_general2", "audio_sel",
		14),
	GATE_AUDIO1_DUMMY(AUDIO_DAC_HIRES, "aud_dac_hires", "audio_h_sel",
		15),
	GATE_AUDIO1_DUMMY(AUDIO_ADC_HIRES, "aud_adc_hires", "audio_h_sel",
		16),
	GATE_AUDIO1_DUMMY(AUDIO_ADC_HIRES_TML, "aud_adc_hires_tml",
		"audio_h_sel",
		17),

	GATE_AUDIO1_DUMMY(AUDIO_PDN_ADDA6_ADC, "aud_pdn_adda6_adc",
		"audio_sel",
		20),
	GATE_AUDIO1_DUMMY(AUDIO_ADDA6_ADC_HIRES, "aud_adda6_adc_hires",
		"audio_h_sel",
		21),
	GATE_AUDIO1_DUMMY(AUDIO_3RD_DAC, "aud_3rd_dac", "audio_sel",
		28),
	GATE_AUDIO1_DUMMY(AUDIO_3RD_DAC_PREDIS, "aud_3rd_dac_predis",
		"audio_sel",
		29),
	GATE_AUDIO1_DUMMY(AUDIO_3RD_DAC_TML, "aud_3rd_dac_tml",
		"audio_sel",
		30),
	GATE_AUDIO1_DUMMY(AUDIO_3RD_DAC_HIRES, "aud_3rd_dac_hires",
		"audio_h_sel",
		31),
#else
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
#endif
};

static const struct mtk_gate_regs cam_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &cam_cg_regs,		\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_CHECK(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &cam_cg_regs,		\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_check,	\
	}

#define GATE_CAM_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &cam_cg_regs,		\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_dummy,	\
	}

static const struct mtk_gate cam_clks[] = {
#if MT_CCF_BRINGUP
	GATE_CAM_DUMMY(CAMSYS_LARB6_CGPDN, "camsys_larb6", "cam_sel", 0),
	GATE_CAM_DUMMY(CAMSYS_LARB7_CGPDN, "camsys_larb7", "cam_sel", 2),
	GATE_CAM_DUMMY(CAMSYS_GALS_CGPDN, "camsys_gals", "cam_sel", 4),
	GATE_CAM_DUMMY(CAMSYS_CAM_CGPDN, "camsys_cam", "cam_sel", 6),
	GATE_CAM_DUMMY(CAMSYS_CAMTG_CGPDN, "camsys_camtg", "cam_sel", 7),
	GATE_CAM_DUMMY(CAMSYS_SENINF_CGPDN, "camsys_seninf", "cam_sel", 8),
	GATE_CAM_DUMMY(CAMSYS_CAMSV0_CGPDN, "camsys_camsv0", "cam_sel", 9),
	GATE_CAM_DUMMY(CAMSYS_CAMSV1_CGPDN, "camsys_camsv1", "cam_sel", 10),
	GATE_CAM_DUMMY(CAMSYS_CCU_CGPDN, "camsys_ccu", "cam_sel", 12),
	GATE_CAM_DUMMY(CAMSYS_FAKE_ENG_CGPDN, "camsys_fake_eng", "cam_sel", 13),
#else
	GATE_CAM_DUMMY(CAMSYS_LARB6_CGPDN, "camsys_larb6", "cam_sel", 0),
	GATE_CAM_DUMMY(CAMSYS_LARB7_CGPDN, "camsys_larb7", "cam_sel", 2),
	GATE_CAM_DUMMY(CAMSYS_GALS_CGPDN, "camsys_gals", "cam_sel", 4),
	GATE_CAM_CHECK(CAMSYS_CAM_CGPDN, "camsys_cam", "cam_sel", 6),
	GATE_CAM_CHECK(CAMSYS_CAMTG_CGPDN, "camsys_camtg", "cam_sel", 7),
	GATE_CAM_CHECK(CAMSYS_SENINF_CGPDN, "camsys_seninf", "cam_sel", 8),
	GATE_CAM_CHECK(CAMSYS_CAMSV0_CGPDN, "camsys_camsv0", "cam_sel", 9),
	GATE_CAM_CHECK(CAMSYS_CAMSV1_CGPDN, "camsys_camsv1", "cam_sel", 10),
	GATE_CAM_CHECK(CAMSYS_CCU_CGPDN, "camsys_ccu", "cam_sel", 12),
	GATE_CAM_CHECK(CAMSYS_FAKE_ENG_CGPDN, "camsys_fake_eng", "cam_sel", 13),
#endif
};

static const struct mtk_gate_regs img_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IMG(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &img_cg_regs,		\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_setclr,\
	}

#define GATE_IMG_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &img_cg_regs,		\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_dummy,	\
	}

static const struct mtk_gate img_clks[] = {
#if 1//MT_CCF_BRINGUP
	GATE_IMG_DUMMY(IMG_LARB5, "imgsys_larb5", "img_sel", 0),
	GATE_IMG_DUMMY(IMG_LARB4, "imgsys_larb4", "img_sel", 1),
	GATE_IMG_DUMMY(IMG_DIP, "imgsys_dip", "img_sel", 2),
	GATE_IMG_DUMMY(IMG_FDVT, "imgsys_fdvt", "img_sel", 3),
	GATE_IMG_DUMMY(IMG_DPE, "imgsys_dpe", "img_sel", 4),
	GATE_IMG_DUMMY(IMG_RSC, "imgsys_rsc", "img_sel", 5),
	GATE_IMG_DUMMY(IMG_MFB, "imgsys_mfb", "img_sel", 6),
	GATE_IMG_DUMMY(IMG_WPE_A, "imgsys_wpe_a", "img_sel", 7),
	GATE_IMG_DUMMY(IMG_WPE_B, "imgsys_wpe_b", "img_sel", 8),
	GATE_IMG_DUMMY(IMG_OWE, "imgsys_owe", "img_sel", 9),
#else
	GATE_IMG(IMG_LARB5, "imgsys_larb5", "img_sel", 0),
	GATE_IMG(IMG_LARB4, "imgsys_larb4", "img_sel", 1),
	GATE_IMG(IMG_DIP, "imgsys_dip", "img_sel", 2),
	GATE_IMG(IMG_FDVT, "imgsys_fdvt", "img_sel", 3),
	GATE_IMG(IMG_DPE, "imgsys_dpe", "img_sel", 4),
	GATE_IMG(IMG_RSC, "imgsys_rsc", "img_sel", 5),
	GATE_IMG(IMG_MFB, "imgsys_mfb", "img_sel", 6),
	GATE_IMG(IMG_WPE_A, "imgsys_wpe_a", "img_sel", 7),
	GATE_IMG(IMG_WPE_B, "imgsys_wpe_b", "img_sel", 8),
	GATE_IMG(IMG_OWE, "imgsys_owe", "img_sel", 9),
#endif
};


static const struct mtk_gate_regs mm0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mm1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
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

#define GATE_MM0_DUMP(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &mm0_cg_regs,		\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_setclr_dump,	\
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

static const struct mtk_gate mm_clks[] = {
#if MT_CCF_BRINGUP
	/* MM0 */
	GATE_MM0_DUMMY(MMSYS_SMI_COMMON, "mm_smi_common", "mm_sel", 0),
	GATE_MM0_DUMMY(MMSYS_SMI_LARB0, "mm_smi_larb0", "mm_sel", 1),
	GATE_MM0_DUMMY(MMSYS_SMI_LARB1, "mm_smi_larb1", "mm_sel", 2),
	GATE_MM0_DUMMY(MMSYS_GALS_COMM0, "mm_gals_comm0", "mm_sel", 3),
	GATE_MM0_DUMMY(MMSYS_GALS_COMM1, "mm_gals_comm1", "mm_sel", 4),

	GATE_MM0_DUMMY(MMSYS_GALS_CCU2MM, "mm_gals_ccu2mm", "mm_sel", 5),
	GATE_MM0_DUMMY(MMSYS_GALS_IPU12MM, "mm_gals_ipu12mm", "mm_sel", 6),
	GATE_MM0_DUMMY(MMSYS_GALS_IMG2MM, "mm_gals_img2mm", "mm_sel", 7),


	GATE_MM0_DUMMY(MMSYS_GALS_CAM2MM, "mm_gals_cam2mm", "mm_sel", 8),
	GATE_MM0_DUMMY(MMSYS_GALS_IPU2MM, "mm_gals_ipu2mm", "mm_sel", 9),
	GATE_MM0_DUMMY(MMSYS_MDP_DL_TXCK, "mm_mdp_dl_txck", "mm_sel", 10),
	GATE_MM0_DUMMY(MMSYS_IPU_DL_TXCK, "mm_ipu_dl_txck", "mm_sel", 11),
	GATE_MM0_DUMMY(MMSYS_MDP_RDMA0, "mm_mdp_rdma0", "mm_sel", 12),
	GATE_MM0_DUMMY(MMSYS_MDP_RDMA1, "mm_mdp_rdma1", "mm_sel", 13),
	GATE_MM0_DUMMY(MMSYS_MDP_RSZ0, "mm_mdp_rsz0", "mm_sel", 14),
	GATE_MM0_DUMMY(MMSYS_MDP_RSZ1, "mm_mdp_rsz1", "mm_sel", 15),
	GATE_MM0_DUMMY(MMSYS_MDP_TDSHP, "mm_mdp_tdshp", "mm_sel", 16),
	GATE_MM0_DUMMY(MMSYS_MDP_WROT0, "mm_mdp_wrot0", "mm_sel", 17),
	GATE_MM0_DUMMY(MMSYS_MDP_WROT1, "mm_mdp_wrot1", "mm_sel", 18),

	GATE_MM0_DUMMY(MMSYS_FAKE_ENG, "mm_fake_eng", "mm_sel", 19),
	GATE_MM0_DUMMY(MMSYS_DISP_OVL0, "mm_disp_ovl0", "mm_sel", 20),
	GATE_MM0_DUMMY(MMSYS_DISP_OVL0_2L, "mm_disp_ovl0_2l", "mm_sel", 21),
	GATE_MM0_DUMMY(MMSYS_DISP_OVL1_2L, "mm_disp_ovl1_2l", "mm_sel", 22),
	GATE_MM0_DUMMY(MMSYS_DISP_RDMA0, "mm_disp_rdma0", "mm_sel", 23),
	GATE_MM0_DUMMY(MMSYS_DISP_RDMA1, "mm_disp_rdma1", "mm_sel", 24),
	GATE_MM0_DUMMY(MMSYS_DISP_WDMA0, "mm_disp_wdma0", "mm_sel", 25),
	GATE_MM0_DUMMY(MMSYS_DISP_COLOR0, "mm_disp_color0", "mm_sel", 26),
	GATE_MM0_DUMMY(MMSYS_DISP_CCORR0, "mm_disp_ccorr0", "mm_sel", 27),
	GATE_MM0_DUMMY(MMSYS_DISP_AAL0, "mm_disp_aal0", "mm_sel", 28),
	GATE_MM0_DUMMY(MMSYS_DISP_GAMMA0, "mm_disp_gamma0", "mm_sel", 29),
	GATE_MM0_DUMMY(MMSYS_DISP_DITHER0, "mm_disp_dither0", "mm_sel", 30),
	GATE_MM0_DUMMY(MMSYS_DISP_SPLIT, "mm_disp_split", "mm_sel", 31),
	/* MM1 */
	GATE_MM1_DUMMY(MMSYS_DSI0_MM_CK, "mm_dsi0_mmck", "mm_sel", 0),
	GATE_MM1_DUMMY(MMSYS_DSI0_IF_CK, "mm_dsi0_ifck", "mm_sel", 1),
	GATE_MM1_DUMMY(MMSYS_DPI_MM_CK, "mm_dpi_mmck", "mm_sel", 2),
	GATE_MM1_DUMMY(MMSYS_DPI_IF_CK, "mm_dpi_ifck", "dpi0_sel", 3),
	GATE_MM1_DUMMY(MMSYS_FAKE_ENG2, "mm_fake_eng2", "mm_sel", 4),
	GATE_MM1_DUMMY(MMSYS_MDP_DL_RX_CK, "mm_mdp_dl_rxck", "mm_sel", 5),
	GATE_MM1_DUMMY(MMSYS_IPU_DL_RX_CK, "mm_ipu_dl_rxck", "mm_sel", 6),
	GATE_MM1_DUMMY(MMSYS_26M, "mm_26m", "f_f26m_ck", 7),
	GATE_MM1_DUMMY(MMSYS_MMSYS_R2Y, "mm_mmsys_r2y", "mm_sel", 8),
	GATE_MM1_DUMMY(MMSYS_DISP_RSZ, "mm_disp_rsz", "mm_sel", 9),
	GATE_MM1_DUMMY(MMSYS_MDP_AAL, "mm_mdp_aal", "mm_sel", 10),
	GATE_MM1_DUMMY(MMSYS_MDP_HDR, "mm_mdp_hdr", "mm_sel", 11),
	GATE_MM1_DUMMY(MMSYS_DBI_MM_CK, "mm_dbi_mmck", "mm_sel", 12),
	GATE_MM1_DUMMY(MMSYS_DBI_IF_CK, "mm_dbi_ifck", "dpi0_sel", 13),

	GATE_MM1_DUMMY(MMSYS_DISP_POSTMASK0, "mm_disp_pm0", "mm_sel", 14),
	GATE_MM1_DUMMY(MMSYS_DISP_HRT_BW, "mm_disp_hrt_bw", "mm_sel", 15),
	GATE_MM1_DUMMY(MMSYS_DISP_OVL_FBDC, "mm_disp_ovl_fbdc", "mm_sel", 16),
#else
	/* MM0 */
	GATE_MM0_DUMMY(MMSYS_SMI_COMMON, "mm_smi_common", "mm_sel", 0),
	GATE_MM0_DUMMY(MMSYS_SMI_LARB0, "mm_smi_larb0", "mm_sel", 1),
	GATE_MM0_DUMMY(MMSYS_SMI_LARB1, "mm_smi_larb1", "mm_sel", 2),
	GATE_MM0_DUMMY(MMSYS_GALS_COMM0, "mm_gals_comm0", "mm_sel", 3),
	GATE_MM0_DUMMY(MMSYS_GALS_COMM1, "mm_gals_comm1", "mm_sel", 4),

	GATE_MM0_DUMMY(MMSYS_GALS_CCU2MM, "mm_gals_ccu2mm", "mm_sel", 5),
	GATE_MM0_DUMMY(MMSYS_GALS_IPU12MM, "mm_gals_ipu12mm", "mm_sel", 6),
	GATE_MM0_DUMMY(MMSYS_GALS_IMG2MM, "mm_gals_img2mm", "mm_sel", 7),


	GATE_MM0_DUMMY(MMSYS_GALS_CAM2MM, "mm_gals_cam2mm", "mm_sel", 8),
	GATE_MM0_DUMMY(MMSYS_GALS_IPU2MM, "mm_gals_ipu2mm", "mm_sel", 9),
	GATE_MM0(MMSYS_MDP_DL_TXCK, "mm_mdp_dl_txck", "mm_sel", 10),
	GATE_MM0(MMSYS_IPU_DL_TXCK, "mm_ipu_dl_txck", "mm_sel", 11),
	GATE_MM0(MMSYS_MDP_RDMA0, "mm_mdp_rdma0", "mm_sel", 12),
	GATE_MM0(MMSYS_MDP_RDMA1, "mm_mdp_rdma1", "mm_sel", 13),
	GATE_MM0(MMSYS_MDP_RSZ0, "mm_mdp_rsz0", "mm_sel", 14),
	GATE_MM0(MMSYS_MDP_RSZ1, "mm_mdp_rsz1", "mm_sel", 15),
	GATE_MM0(MMSYS_MDP_TDSHP, "mm_mdp_tdshp", "mm_sel", 16),
	GATE_MM0(MMSYS_MDP_WROT0, "mm_mdp_wrot0", "mm_sel", 17),
	GATE_MM0(MMSYS_MDP_WROT1, "mm_mdp_wrot1", "mm_sel", 18),

	GATE_MM0(MMSYS_FAKE_ENG, "mm_fake_eng", "mm_sel", 19),
	GATE_MM0(MMSYS_DISP_OVL0, "mm_disp_ovl0", "mm_sel", 20),
	GATE_MM0(MMSYS_DISP_OVL0_2L, "mm_disp_ovl0_2l", "mm_sel", 21),
	GATE_MM0(MMSYS_DISP_OVL1_2L, "mm_disp_ovl1_2l", "mm_sel", 22),
	GATE_MM0(MMSYS_DISP_RDMA0, "mm_disp_rdma0", "mm_sel", 23),
	GATE_MM0(MMSYS_DISP_RDMA1, "mm_disp_rdma1", "mm_sel", 24),
	GATE_MM0(MMSYS_DISP_WDMA0, "mm_disp_wdma0", "mm_sel", 25),
	GATE_MM0(MMSYS_DISP_COLOR0, "mm_disp_color0", "mm_sel", 26),
	GATE_MM0(MMSYS_DISP_CCORR0, "mm_disp_ccorr0", "mm_sel", 27),
	GATE_MM0(MMSYS_DISP_AAL0, "mm_disp_aal0", "mm_sel", 28),
	GATE_MM0(MMSYS_DISP_GAMMA0, "mm_disp_gamma0", "mm_sel", 29),
	GATE_MM0(MMSYS_DISP_DITHER0, "mm_disp_dither0", "mm_sel", 30),
	GATE_MM0(MMSYS_DISP_SPLIT, "mm_disp_split", "mm_sel", 31),
	/* MM1 */
	GATE_MM1(MMSYS_DSI0_MM_CK, "mm_dsi0_mmck", "mm_sel", 0),
	GATE_MM1(MMSYS_DSI0_IF_CK, "mm_dsi0_ifck", "mm_sel", 1),
	GATE_MM1(MMSYS_DPI_MM_CK, "mm_dpi_mmck", "mm_sel", 2),
	GATE_MM1(MMSYS_DPI_IF_CK, "mm_dpi_ifck", "dpi0_sel", 3),
	GATE_MM1(MMSYS_FAKE_ENG2, "mm_fake_eng2", "mm_sel", 4),
	GATE_MM1(MMSYS_MDP_DL_RX_CK, "mm_mdp_dl_rxck", "mm_sel", 5),
	GATE_MM1(MMSYS_IPU_DL_RX_CK, "mm_ipu_dl_rxck", "mm_sel", 6),
	GATE_MM1(MMSYS_26M, "mm_26m", "f_f26m_ck", 7),
	GATE_MM1(MMSYS_MMSYS_R2Y, "mm_mmsys_r2y", "mm_sel", 8),
	GATE_MM1(MMSYS_DISP_RSZ, "mm_disp_rsz", "mm_sel", 9),
	GATE_MM1(MMSYS_MDP_AAL, "mm_mdp_aal", "mm_sel", 10),
	GATE_MM1(MMSYS_MDP_HDR, "mm_mdp_hdr", "mm_sel", 11),
	GATE_MM1(MMSYS_DBI_MM_CK, "mm_dbi_mmck", "mm_sel", 12),
	GATE_MM1(MMSYS_DBI_IF_CK, "mm_dbi_ifck", "dpi0_sel", 13),

	GATE_MM1(MMSYS_DISP_POSTMASK0, "mm_disp_pm0", "mm_sel", 14),
	GATE_MM1(MMSYS_DISP_HRT_BW, "mm_disp_hrt_bw", "mm_sel", 15),
	GATE_MM1(MMSYS_DISP_OVL_FBDC, "mm_disp_ovl_fbdc", "mm_sel", 16),
#endif
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

static const struct mtk_gate vdec_clks[] = {
#if 1//MT_CCF_BRINGUP
	/* VDEC0 */
	GATE_VDEC0_I_DUMMY(VDEC_VDEC, "vdec_cken", "vdec_sel", 0),
	/* VDEC1 */
	GATE_VDEC1_I_DUMMY(VDEC_LARB1, "vdec_larb1_cken", "vdec_sel", 0),
#else
	/* VDEC0 */
	GATE_VDEC0_I(VDEC_VDEC, "vdec_cken", "vdec_sel", 0),
	/* VDEC1 */
	GATE_VDEC1_I(VDEC_LARB1, "vdec_larb1_cken", "vdec_sel", 0),
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

static const struct mtk_gate venc_global_con_clks[] = {
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

static const struct mtk_gate_regs apu_conn_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_APU_CONN(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &apu_conn_cg_regs,	\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_setclr,\
	}

#define GATE_APU_CONN_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &apu_conn_cg_regs,	\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_dummy,	\
	}

static const struct mtk_gate apu_conn_clks[] = {
	GATE_APU_CONN_DUMMY(APU_CONN_APU_CG, "apu_conn_apu", "dsp1_sel", 0),
	GATE_APU_CONN_DUMMY(APU_CONN_AHB_CG, "apu_conn_ahb", "dsp_sel", 1),
	GATE_APU_CONN_DUMMY(APU_CONN_AXI_CG, "apu_conn_axi", "dsp_sel", 2),
	GATE_APU_CONN_DUMMY(APU_CONN_ISP_CG, "apu_conn_isp", "dsp_sel", 3),
	GATE_APU_CONN_DUMMY(APU_CONN_CAM_ADL_CG, "apu_conn_cam_adl",
		"dsp_sel", 4),
	GATE_APU_CONN_DUMMY(APU_CONN_IMG_ADL_CG, "apu_conn_img_adl",
		"dsp_sel", 5),
	GATE_APU_CONN_DUMMY(APU_CONN_EMI_26M_CG, "apu_conn_emi_26m",
		"dsp_sel", 6),
	GATE_APU_CONN_DUMMY(APU_CONN_VPU_UDI_CG, "apu_conn_vpu_udi",
		"dsp_sel", 7),
};

static const struct mtk_gate_regs apu0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_APU0(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &apu0_cg_regs,		\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_setclr,\
	}

#define GATE_APU0_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &apu0_cg_regs,		\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_dummy,	\
	}

static const struct mtk_gate apu0_clks[] = {
#if MT_CCF_BRINGUP
	GATE_APU0_DUMMY(APU0_APU_CG, "apu0_apu", "dsp1_sel", 0),
	GATE_APU0_DUMMY(APU0_AXI_M_CG, "apu0_axi", "dsp1_sel", 1),
	GATE_APU0_DUMMY(APU0_JTAG_CG, "apu0_jtag", "dsp1_sel", 2),
#else
	GATE_APU0(APU0_APU_CG, "apu0_apu", "dsp1_sel", 0),
	GATE_APU0(APU0_AXI_M_CG, "apu0_axi", "dsp1_sel", 1),
	GATE_APU0(APU0_JTAG_CG, "apu0_jtag", "dsp1_sel", 2),
#endif
};

static const struct mtk_gate_regs apu1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_APU1(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &apu1_cg_regs,		\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_setclr,\
	}

#define GATE_APU1_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &apu1_cg_regs,		\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_dummy,	\
	}

static const struct mtk_gate apu1_clks[] = {
#if MT_CCF_BRINGUP
	GATE_APU1_DUMMY(APU1_APU_CG, "apu1_apu", "dsp2_sel", 0),
	GATE_APU1_DUMMY(APU1_AXI_M_CG, "apu1_axi", "dsp2_sel", 1),
	GATE_APU1_DUMMY(APU1_JTAG_CG, "apu1_jtag", "dsp2_sel", 2),
#else
	GATE_APU1(APU1_APU_CG, "apu1_apu", "dsp2_sel", 0),
	GATE_APU1(APU1_AXI_M_CG, "apu1_axi", "dsp2_sel", 1),
	GATE_APU1(APU1_JTAG_CG, "apu1_jtag", "dsp2_sel", 2),
#endif
};

static const struct mtk_gate_regs apu_vcore_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_APU_VCORE(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &apu_vcore_cg_regs,	\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_setclr,\
	}

#define GATE_APU_VCORE_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,			\
		.name = _name,			\
		.parent_name = _parent,		\
		.regs = &apu_vcore_cg_regs,	\
		.shift = _shift,		\
		.ops = &mtk_clk_gate_ops_dummy_all,	\
	}

static const struct mtk_gate apu_vcore_clks[] = {
#if 1/*MT_CCF_BRINGUP*/
	GATE_APU_VCORE_DUMMY(APU_VCORE_AHB_CG, "apu_vcore_ahb",
		"ipu_if_sel", 0),
	GATE_APU_VCORE_DUMMY(APU_VCORE_AXI_CG, "apu_vcore_axi",
		"ipu_if_sel", 1),
	GATE_APU_VCORE_DUMMY(APU_VCORE_ADL_CG, "apu_vcore_adl",
		"ipu_if_sel", 2),
	GATE_APU_VCORE_DUMMY(APU_VCORE_QOS_CG, "apu_vcore_qos",
		"ipu_if_sel", 3),
#else
	GATE_APU_VCORE(APU_VCORE_AHB_CG, "apu_vcore_ahb", "ipu_if_sel", 0),
	GATE_APU_VCORE(APU_VCORE_AXI_CG, "apu_vcore_axi", "ipu_if_sel", 1),
	GATE_APU_VCORE(APU_VCORE_ADL_CG, "apu_vcore_adl", "ipu_if_sel", 2),
	GATE_APU_VCORE(APU_VCORE_QOS_CG, "apu_vcore_qos", "ipu_if_sel", 3),
#endif
};

static int  mtk_topckgen_init(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	struct device_node *node = pdev->dev.of_node;
	int r;
	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return -EINVAL;
	}

	clk_data = mtk_alloc_clk_data(TOP_NR_CLK);

	mtk_clk_register_fixed_clks(fixed_clks,
		ARRAY_SIZE(fixed_clks), clk_data);

	mtk_clk_register_factors(top_divs, ARRAY_SIZE(top_divs), clk_data);
	mtk_clk_register_muxes(top_muxes,
		ARRAY_SIZE(top_muxes), node, &mt6785_clk_lock, clk_data);
	mtk_clk_register_composites(top_audmuxes, ARRAY_SIZE(top_audmuxes),
		base, &mt6785_clk_lock, clk_data);
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

#if 1
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

	clk_writel(cksys_base + CK_CFG_13_CLR, 0x80008080);/*busaximem*/
	clk_writel(cksys_base + CK_CFG_13_SET, 0x80008080);
#endif
	return r;
}

static int mtk_infracfg_ao_init(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;
	struct device_node *node = pdev->dev.of_node;
	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return -EINVAL;
	}

	clk_data = mtk_alloc_clk_data(INFRACFG_AO_NR_CLK);
	if (!clk_data) {
		pr_notice("%s(): alloc clk data failed\n", __func__);
		return -ENOMEM;
	}

	mtk_clk_register_gates(node, infra_clks,
		ARRAY_SIZE(infra_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	infracfg_base = base;
	/* Need Confirm */
	clk_writel(INFRA_TOPAXI_SI0_CTL, clk_readl(INFRA_TOPAXI_SI0_CTL) | 0x2);
	pr_notice("%s: infra mfg debug: %08x\n",
			__func__, clk_readl(INFRA_TOPAXI_SI0_CTL));
	/*mtk_clk_enable_critical();*/
#if MT_CCF_BRINGUP
#else
	clk_writel(INFRA_PDN_SET0, INFRA_CG0);
	clk_writel(INFRA_PDN_SET1, INFRA_CG1);
	clk_writel(INFRA_PDN_SET2, INFRA_CG2);
	clk_writel(INFRA_PDN_SET3, INFRA_CG3);
#endif
	return r;
}


/* FIXME: modify FMAX */
#define MT6785_PLL_FMAX		(3800UL * MHZ)
#define MT6785_PLL_FMIN		(1500UL * MHZ)
#define MT6785_INTEGER_BITS		8

/*#define CON0_MT6785_RST_BAR	BIT(24), not fixed */

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
		.fmax = MT6785_PLL_FMAX,				\
		.fmin = MT6785_PLL_FMIN,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6785_INTEGER_BITS,			\
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

	PLL(APMIXED_TVDPLL, "tvdpll", 0x0270, 0x027C, BIT(0),
		PLL_AO, 0,
		22,
		0x0274, 24,
		0, 0, 0,
		0x0274, 0),

	PLL(APMIXED_APUPLL, "apupll", 0x02EC, 0x02F8, BIT(0),
		PLL_AO, 0,
		22,
		0x02F0, 24,
		0, 0, 0,
		0x02F0, 0),

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

	PLL(APMIXED_TVDPLL, "tvdpll", 0x0270, 0x027C, BIT(0),
		0, 0,
		22,
		0x0274, 24,
		0, 0, 0,
		0x0274, 0),

	PLL(APMIXED_APUPLL, "apupll", 0x02EC, 0x02F8, BIT(0),
		0, 0,
		22,
		0x02F0, 24,
		0, 0, 0,
		0x02F0, 0),

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

static int  mtk_apmixedsys_init(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	struct device_node *node = pdev->dev.of_node;
	int r;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return -EINVAL;
	}

	clk_data = mtk_alloc_clk_data(APMIXED_NR_CLK);
	if (!clk_data) {
		pr_notice("%s(): alloc clk data failed\n", __func__);
		return -ENOMEM;
	}

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
	clk_writel(AP_PLL_CON3, clk_readl(AP_PLL_CON3) & 0x00044440);
	clk_writel(AP_PLL_CON4, clk_readl(AP_PLL_CON4) & 0x004);
	/* [17] = 0 */
	clk_writel(AP_PLL_CON6, clk_readl(AP_PLL_CON6) & 0xfffdffff);
	#if 0 /* race condition access */
	/*[4]SSUSB26M, [11][6]MIPIC camera, [8] mm26m*/
	clk_writel(AP_PLL_CON8, clk_readl(AP_PLL_CON8) & 0xfffff6af);
	#endif

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
/*TVDPLL*/
	clk_clrl(TVDPLL_CON0, PLL_EN);
	clk_setl(TVDPLL_PWR_CON0, PLL_ISO_EN);
	clk_clrl(TVDPLL_PWR_CON0, PLL_PWR_ON);
#endif
/*MSDCPLL*/
	clk_clrl(MSDCPLL_CON0, PLL_EN);
	clk_setl(MSDCPLL_PWR_CON0, PLL_ISO_EN);
	clk_clrl(MSDCPLL_PWR_CON0, PLL_PWR_ON);
/*APUPLL*/
	clk_clrl(APUPLL_CON0, PLL_EN);
	clk_setl(APUPLL_PWR_CON0, PLL_ISO_EN);
	clk_clrl(APUPLL_PWR_CON0, PLL_PWR_ON);
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
	return r;
}


static int  mtk_audio_init(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;
	struct device_node *node = pdev->dev.of_node;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return -EINVAL;
	}

	clk_data = mtk_alloc_clk_data(AUDIO_NR_CLK);
	if (!clk_data) {
		pr_notice("%s(): alloc clk data failed\n", __func__);
		return -ENOMEM;
	}

	mtk_clk_register_gates(node, audio_clks,
		ARRAY_SIZE(audio_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	audio_base = base;

#if MT_CCF_BRINGUP
	/*clk_writel(AUDIO_TOP_CON0, AUDIO_DISABLE_CG0);*/
	/*clk_writel(AUDIO_TOP_CON1, AUDIO_DISABLE_CG1);*/
#endif
	return r;

}

static int mtk_camsys_init(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;
	struct device_node *node = pdev->dev.of_node;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return -EINVAL;
	}
	clk_data = mtk_alloc_clk_data(CAMSYS_NR_CLK);
	if (!clk_data) {
		pr_notice("%s(): alloc clk data failed\n", __func__);
		return -ENOMEM;
	}

	mtk_clk_register_gates(node, cam_clks, ARRAY_SIZE(cam_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	cam_base = base;

#if MT_CCF_BRINGUP
	clk_writel(CAMSYS_CG_CLR, CAMSYS_CG);
#endif
	return r;
}

static int mtk_imgsys_init(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;
	struct device_node *node = pdev->dev.of_node;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return -EINVAL;
	}
	clk_data = mtk_alloc_clk_data(IMG_NR_CLK);
	if (!clk_data) {
		pr_notice("%s(): alloc clk data failed\n", __func__);
		return -ENOMEM;
	}

	mtk_clk_register_gates(node, img_clks, ARRAY_SIZE(img_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	img_base = base;

#if MT_CCF_BRINGUP
	clk_writel(IMG_CG_CLR, IMG_CG);
#endif
	return r;
}

static int mtk_mfg_cfg_init(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;
	struct device_node *node = pdev->dev.of_node;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return -EINVAL;
	}

	clk_data = mtk_alloc_clk_data(MFGCFG_NR_CLK);
	if (!clk_data) {
		pr_notice("%s(): alloc clk data failed\n", __func__);
		return -ENOMEM;
	}

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
	return r;
}

static int mtk_mmsys_config_init(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;
	struct device_node *node = pdev->dev.of_node;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return -EINVAL;
	}
	clk_data = mtk_alloc_clk_data(MMSYS_CONFIG_NR_CLK);
	if (!clk_data) {
		pr_notice("%s(): alloc clk data failed\n", __func__);
		return -ENOMEM;
	}

	mtk_clk_register_gates(node, mm_clks, ARRAY_SIZE(mm_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	mmsys_config_base = base;
#if MT_CCF_BRINGUP
	/*clk_writel(MM_CG_CLR0, MM_CG0);*/
	/*clk_writel(MM_CG_CLR1, MM_CG1);*/
#else
#endif
	return r;
}

static int mtk_vdec_top_global_con_init(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;
	struct device_node *node = pdev->dev.of_node;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return -EINVAL;
	}
	clk_data = mtk_alloc_clk_data(VDEC_GCON_NR_CLK);
	if (!clk_data) {
		pr_notice("%s(): alloc clk data failed\n", __func__);
		return -ENOMEM;
	}

	mtk_clk_register_gates(node, vdec_clks,
		ARRAY_SIZE(vdec_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	vdec_gcon_base = base;

#if MT_CCF_BRINGUP
	clk_writel(VDEC_CKEN_SET, VDEC_CG);
	clk_writel(LARB1_CKEN_SET, VDEC_LARB1_CG);
#endif
	return r;
}

static int mtk_venc_global_con_init(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;
	struct device_node *node = pdev->dev.of_node;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return -EINVAL;
	}
	clk_data = mtk_alloc_clk_data(VENC_GCON_NR_CLK);
	if (!clk_data) {
		pr_notice("%s(): alloc clk data failed\n", __func__);
		return -ENOMEM;
	}

	mtk_clk_register_gates(node, venc_global_con_clks,
		ARRAY_SIZE(venc_global_con_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	venc_gcon_base = base;

#if MT_CCF_BRINGUP
	clk_writel(VENC_CG_SET, VENC_CG);
#endif
	return r;
}

static int mtk_apu_conn_init(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;
	struct device_node *node = pdev->dev.of_node;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return -EINVAL;
	}
	clk_data = mtk_alloc_clk_data(APU_CONN_NR_CLK);
	if (!clk_data) {
		pr_notice("%s(): alloc clk data failed\n", __func__);
		return -ENOMEM;
	}

	mtk_clk_register_gates(node, apu_conn_clks,
		ARRAY_SIZE(apu_conn_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	apu_conn_base = base;

#if MT_CCF_BRINGUP
	/*clk_writel(APU_CONN_CG_CLR, APU_CONN_CG);*/
#endif
	return r;
}

static int mtk_apu0_init(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;
	struct device_node *node = pdev->dev.of_node;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return -EINVAL;
	}
	clk_data = mtk_alloc_clk_data(APU0_NR_CLK);
	if (!clk_data) {
		pr_notice("%s(): alloc clk data failed\n", __func__);
		return -ENOMEM;
	}

	mtk_clk_register_gates(node, apu0_clks,
		ARRAY_SIZE(apu0_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	apu0_base = base;

#if MT_CCF_BRINGUP
	/*clk_writel(APU_CORE0_CG_CLR, APU_CORE0_CG);*/
#endif
	return r;
}

static int mtk_apu1_init(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;
	struct device_node *node = pdev->dev.of_node;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return -EINVAL;
	}
	clk_data = mtk_alloc_clk_data(APU1_NR_CLK);
	if (!clk_data) {
		pr_notice("%s(): alloc clk data failed\n", __func__);
		return -ENOMEM;
	}

	mtk_clk_register_gates(node, apu1_clks,
		ARRAY_SIZE(apu1_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	apu1_base = base;

#if MT_CCF_BRINGUP
	/*clk_writel(APU_CORE1_CG_CLR, APU_CORE1_CG);*/
#endif
	return r;
}

static int mtk_apu_vcore_init(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;
	struct device_node *node = pdev->dev.of_node;

	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return -EINVAL;
	}
	clk_data = mtk_alloc_clk_data(APU_VCORE_NR_CLK);
	if (!clk_data) {
		pr_notice("%s(): alloc clk data failed\n", __func__);
		return -ENOMEM;
	}

	mtk_clk_register_gates(node, apu_vcore_clks,
		ARRAY_SIZE(apu_vcore_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
		kfree(clk_data);
	}
	apu_vcore_base = base;

#if MT_CCF_BRINGUP
	/*clk_writel(APU_VCORE_CG_CLR, APU_VCORE_CG);*/
#endif
	return r;
}

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

unsigned int mt_get_ckgen_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1 = 0;

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

	temp = clk_readl(CLK26CALI_1) & 0xFFFF;

	output = (temp * 26000) / 1024;

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	/*clk_writel(CLK26CALI_0, clk26cali_0);*/
	/*clk_writel(CLK26CALI_1, clk26cali_1);*/

	clk_writel(CLK26CALI_0, 0x0000);
	/*print("ckgen meter[%d] = %d Khz\n", ID, output);*/
	if (i > 20)
		return 0;
	else
		return output;

}

unsigned int mt_get_abist_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned int temp, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1 = 0;

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

	temp = clk_readl(CLK26CALI_1) & 0xFFFF;

	output = (temp * 26000) / 1024;

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0);
	/*clk_writel(CLK26CALI_0, clk26cali_0);*/
	/*clk_writel(CLK26CALI_1, clk26cali_1);*/
	clk_writel(CLK26CALI_0, 0x0000);
	/*pr_debug("%s = %d Khz\n", abist_array[ID-1], output);*/
	if (i > 20)
		return 0;
	else
		return (output * 2);
}
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
	if (clk_readl(TVDPLL_CON0) & 0x1)
		pr_notice("suspend warning: TVDPLL is on!!!\n");
	if (clk_readl(APLL1_CON0) & 0x1)
		pr_notice("suspend warning: APLL1 is on!!!\n");
	if (clk_readl(APLL2_CON0) & 0x1)
		pr_notice("suspend warning: APLL2 is on!!!\n");
	if (clk_readl(APUPLL_CON0) & 0x1)
		pr_notice("suspend warning: APUPLL is on!!!\n");

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
	pr_notice("%s: TVDPLL = %dHZ\r\n", __func__, mt_get_abist_freq(34));
	pr_notice("%s: APLL1 = %dHZ\r\n", __func__, mt_get_abist_freq(28));
	pr_notice("%s: APLL2 = %dHZ\r\n", __func__, mt_get_abist_freq(29));
#endif
}

void clock_force_off(void)
{
	/*DISP CG*/
	clk_writel(MM_CG_SET0, MM_CG0);
	clk_writel(MM_CG_SET1, MM_CG1);
	/*AUDIO*/
	/*clk_writel(AUDIO_TOP_CON0, AUDIO_CG0);*/
	/*clk_writel(AUDIO_TOP_CON1, AUDIO_CG1);*/
	/*MFG*/
	clk_writel(MFG_CG_SET, MFG_CG);
	/*ISP*/
	clk_writel(IMG_CG_SET, IMG_CG);
	/*VENC inverse*/
	clk_writel(VENC_CG_CLR, VENC_CG);
	/*CAM*/
	clk_writel(CAMSYS_CG_SET, CAMSYS_CG);
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
/*APUPLL*/
	clk_clrl(APUPLL_CON0, PLL_EN);
	clk_setl(APUPLL_PWR_CON0, PLL_ISO_EN);
	clk_clrl(APUPLL_PWR_CON0, PLL_PWR_ON);
/*TVDPLL*/
	clk_clrl(TVDPLL_CON0, PLL_EN);
	clk_setl(TVDPLL_PWR_CON0, PLL_ISO_EN);
	clk_clrl(TVDPLL_PWR_CON0, PLL_PWR_ON);

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

void check_mm0_clk_sts(void)
{
	/* confirm mm0 clk */
	pr_notice("MM_CG = 0x%08x, 0x%08x\n", clk_readl(MM_CG_CON0),
		clk_readl(MM_CG_CON1));
	pr_notice("CLK_CFG_0 = 0x%08x\r\n", clk_readl(CLK_CFG_0));
	pr_notice("MMPLL_CON0 = 0x%08x, 0x%08x\r\n", clk_readl(MMPLL_CON0),
		clk_readl(MMPLL_CON1));
}

void check_img_clk_sts(void)
{
	/* confirm mm0 clk */
	pr_notice("IMG_CG_CON = 0x%08x\n", clk_readl(IMG_CG_CON));
}

void check_ven_clk_sts(void)
{
	/* confirm mm0 clk */
	pr_notice("VENC_CG_CON = 0x%08x\n", clk_readl(VENC_CG_CON));
}
#if 1
void check_cam_clk_sts(void)
{
	/* confirm mm0 clk */
	pr_notice("CLK_CFG_1 = 0x%08x\n", clk_readl(CLK_CFG_1));
	pr_notice("cam freq = %d\n", mt_get_ckgen_freq(8));
	pr_notice("MM_CG_CON0 = 0x%08x\n", clk_readl(MM_CG_CON0));
	pr_notice("CAMSYS_CG_CON = 0x%08x\n", clk_readl(CAMSYS_CG_CON));
}
#endif

void aud_intbus_mux_sel(unsigned int aud_idx)
{
	clk_writel(CLK_CFG_6_CLR, 0x00030000);/*[17:16]*/
	clk_writel(CLK_CFG_6_SET, aud_idx << 16);
	clk_writel(CLK_CFG_UPDATE, 0x04000000);/*[26]*/
}

static const struct of_device_id of_match_clk_mt6785[] = {
	{
		.compatible = "mediatek,apmixed",
		.data = mtk_apmixedsys_init,
	}, {
		.compatible = "mediatek,topckgen",
		.data = mtk_topckgen_init,
	}, {
		.compatible = "mediatek,infracfg_ao",
		.data = mtk_infracfg_ao_init,
	}, {
		.compatible = "mediatek,audio",
		.data = mtk_audio_init,
	}, {
		.compatible = "mediatek,mt6785-camsys",
		.data = mtk_camsys_init,
	}, {
		.compatible = "mediatek,mt6785-imgsys",
		.data = mtk_imgsys_init,
	}, {
		.compatible = "mediatek,mmsys_config",
		.data = mtk_mmsys_config_init,
	}, {
		.compatible = "mediatek,mfgcfg",
		.data = mtk_mfg_cfg_init,
	}, {
		.compatible = "mediatek,venc_gcon",
		.data = mtk_venc_global_con_init,
	}, {
		.compatible =  "mediatek,vdec_gcon",
		.data = mtk_vdec_top_global_con_init,
	}, {
		.compatible = "mediatek,apu_vcore",
		.data = mtk_apu_vcore_init,
	}, {
		.compatible = "mediatek,apu1",
		.data = mtk_apu1_init,
	}, {
		.compatible = "mediatek,apu0",
		.data = mtk_apu0_init,
	}, {
		.compatible = "mediatek,apu_conn",
		.data = mtk_apu_conn_init,
	}
};

static int clk_mt6785_probe(struct platform_device *pdev)
{
	int (*clk_probe)(struct platform_device *d);
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


static struct platform_driver clk_mt6785_drv = {
	.probe = clk_mt6785_probe,
	.driver = {
		.name = "clk-mt6785",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt6785,
	},
};

static int __init clk_mt6785_init(void)
{
	/*timer_ready = true;*/
	/*mtk_clk_enable_critical();*/
	return platform_driver_register(&clk_mt6785_drv);
}

static void __exit clk_mt6785_exit(void)
{
	pr_debug("%s: skipped for mtcmos control disable\n", __func__);
}

arch_initcall(clk_mt6785_init);
module_exit(clk_mt6785_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MTK");
MODULE_DESCRIPTION("MTK CCF  Driverv1.0");
