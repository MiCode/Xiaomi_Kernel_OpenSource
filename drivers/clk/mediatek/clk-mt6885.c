/*
 * Copyright (c) 2019 MediaTek Inc.
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
#include <linux/slab.h>
#include <dt-bindings/clock/mt6885-clk.h>

#include "clk-gate.h"
#include "clk-mtk.h"
#include "clk-mt6885-pg.h"
#include "clk-mux.h"
#include <mt-plat/aee.h>

#define MT_CCF_BRINGUP		0
#ifdef CONFIG_ARM64
#define IOMEM(a)	((void __force __iomem *)((a)))
#endif

int __attribute__((weak)) get_sw_req_vcore_opp(void)
{
	return -1;
}

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


static DEFINE_SPINLOCK(mt6885_clk_lock);

#define CAMSYS_CG_CON		(cam_base + 0x0000)
#define CAMSYS_RAWA_CG_CON	(cam_rawa_base + 0x0000)
#define CAMSYS_RAWB_CG_CON	(cam_rawb_base + 0x0000)
#define CAMSYS_RAWC_CG_CON	(cam_rawc_base + 0x0000)
#define PWR_STATUS		(spm_base_debug + 0x016C)

void mtk_ccf_cam_debug(const char *str1, const char *str2,
							const char *str3)
{
#if 0
	unsigned int rate = mt_get_ckgen_freq(1);

	rate = mt_get_ckgen_freq(11);
	pr_notice("[CCF] %-24s %-27s %-3s cg:0x%08x, raw a:0x%08x, b:0x%08x, c:0x%08x, rate:%d, pwrsta:%08x\n",
		str1 ? str1 : "N/A",
		str3 ? str3 : "N/A",
		str2 ? str2 : "N/A",
		clk_readl(PWR_STATUS) & (0x1UL << 23) ?
				clk_readl(CAMSYS_CG_CON) : 0xDEADBEEF,
		clk_readl(PWR_STATUS) & (0x1UL << 24) ?
				clk_readl(CAMSYS_RAWA_CG_CON) : 0xDEADBEEF,
		clk_readl(PWR_STATUS) & (0x1UL << 25) ?
				clk_readl(CAMSYS_RAWB_CG_CON) : 0xDEADBEEF,
		clk_readl(PWR_STATUS) & (0x1UL << 26) ?
				clk_readl(CAMSYS_RAWC_CG_CON) : 0xDEADBEEF,
		rate,
		clk_readl(PWR_STATUS));
#endif
}


/* CKSYS */
#define CLK_MISC_CFG_0		(cksys_base + 0x140)
#define CLK_MISC_CFG_1		(cksys_base + 0x150)
#define CLK_DBG_CFG		(cksys_base + 0x17C)
#define CLK_SCP_CFG_0		(cksys_base + 0x200)
#define CLK_SCP_CFG_1		(cksys_base + 0x210)
#define CLK26CALI_0		(cksys_base + 0x220)
#define CLK26CALI_1		(cksys_base + 0x224)

/* CG */
#define INFRA_PDN_SET0          (infracfg_base + 0x0080)
#define INFRA_PDN_CLR0          (infracfg_base + 0x0084)
#define INFRA_PDN_SET1          (infracfg_base + 0x0088)
#define INFRA_PDN_CLR1          (infracfg_base + 0x008C)
#define INFRA_PDN_SET2          (infracfg_base + 0x00A4)
#define INFRA_PDN_CLR2          (infracfg_base + 0x00A8)
#define INFRA_PDN_SET3          (infracfg_base + 0x00C0)
#define INFRA_PDN_CLR3          (infracfg_base + 0x00C4)
#define INFRA_TOPAXI_SI0_CTL	(infracfg_base + 0x0200)

#define PLLON_CON0		(apmixed_base + 0x050)
#define PLLON_CON1		(apmixed_base + 0x054)
#define PLLON_CON2		(apmixed_base + 0x058)
#define PLLON_CON3		(apmixed_base + 0x05C)

#define ARMPLL_LL_CON0          (apmixed_base + 0x208)
#define ARMPLL_LL_CON1          (apmixed_base + 0x20C)
#define ARMPLL_LL_CON2          (apmixed_base + 0x210)
#define ARMPLL_LL_CON3          (apmixed_base + 0x214)
#define ARMPLL_BL0_CON0		(apmixed_base + 0x218)
#define ARMPLL_BL0_CON1		(apmixed_base + 0x21C)
#define ARMPLL_BL0_CON2		(apmixed_base + 0x220)
#define ARMPLL_BL0_CON3		(apmixed_base + 0x224)
#define ARMPLL_BL1_CON0		(apmixed_base + 0x228)
#define ARMPLL_BL1_CON1		(apmixed_base + 0x22C)
#define ARMPLL_BL1_CON2		(apmixed_base + 0x230)
#define ARMPLL_BL1_CON3		(apmixed_base + 0x234)
#define ARMPLL_BL2_CON0		(apmixed_base + 0x238)
#define ARMPLL_BL2_CON1		(apmixed_base + 0x23C)
#define ARMPLL_BL2_CON2		(apmixed_base + 0x240)
#define ARMPLL_BL2_CON3		(apmixed_base + 0x244)
#define ARMPLL_BL3_CON0		(apmixed_base + 0x248)
#define ARMPLL_BL3_CON1		(apmixed_base + 0x24C)
#define ARMPLL_BL3_CON2		(apmixed_base + 0x250)
#define ARMPLL_BL3_CON3		(apmixed_base + 0x254)
#define CCIPLL_CON0		(apmixed_base + 0x258)
#define CCIPLL_CON1		(apmixed_base + 0x25C)
#define CCIPLL_CON2		(apmixed_base + 0x260)
#define CCIPLL_CON3		(apmixed_base + 0x264)
#define MFGPLL_CON0		(apmixed_base + 0x268)
#define MFGPLL_CON1		(apmixed_base + 0x26C)
#define MFGPLL_CON3		(apmixed_base + 0x274)
#define UNIVPLL_CON0		(apmixed_base + 0x308)
#define UNIVPLL_CON1		(apmixed_base + 0x30C)
#define UNIVPLL_CON3		(apmixed_base + 0x314)
#define APLL1_CON0		(apmixed_base + 0x318)
#define APLL1_CON1		(apmixed_base + 0x31C)
#define APLL1_CON2		(apmixed_base + 0x320)
#define APLL1_CON3		(apmixed_base + 0x324)
#define APLL1_CON4		(apmixed_base + 0x328)
#define APLL2_CON0		(apmixed_base + 0x32C)
#define APLL2_CON1		(apmixed_base + 0x330)
#define APLL2_CON2		(apmixed_base + 0x334)
#define APLL2_CON3		(apmixed_base + 0x338)
#define APLL2_CON4		(apmixed_base + 0x33C)
#define MAINPLL_CON0            (apmixed_base + 0x340)
#define MAINPLL_CON1            (apmixed_base + 0x344)
#define MAINPLL_CON3		(apmixed_base + 0x34C)
#define MSDCPLL_CON0		(apmixed_base + 0x350)
#define MSDCPLL_CON1		(apmixed_base + 0x354)
#define MSDCPLL_CON3		(apmixed_base + 0x35C)
#define MMPLL_CON0		(apmixed_base + 0x360)
#define MMPLL_CON1		(apmixed_base + 0x364)
#define MMPLL_CON3		(apmixed_base + 0x36C)
#define ADSPPLL_CON0		(apmixed_base + 0x370)
#define ADSPPLL_CON1		(apmixed_base + 0x374)
#define ADSPPLL_CON3		(apmixed_base + 0x37C)
#define TVDPLL_CON0		(apmixed_base + 0x380)
#define TVDPLL_CON1		(apmixed_base + 0x384)
#define TVDPLL_CON3		(apmixed_base + 0x38C)
#define MPLL_CON0		(apmixed_base + 0x390)
#define MPLL_CON1		(apmixed_base + 0x394)
#define MPLL_CON3		(apmixed_base + 0x39C)
#define APUPLL_CON0		(apmixed_base + 0x3A0)
#define APUPLL_CON1		(apmixed_base + 0x3A4)
#define APUPLL_CON3		(apmixed_base + 0x3AC)

#define AUDIO_TOP_CON0		(audio_base + 0x0000)
#define AUDIO_TOP_CON1		(audio_base + 0x0004)
#define AUDIO_TOP_CON2		(audio_base + 0x0008)

#define CAMSYS_CG_CON		(cam_base + 0x0000)
#define CAMSYS_CG_SET		(cam_base + 0x0004)
#define CAMSYS_CG_CLR		(cam_base + 0x0008)

#define CAMSYS_RAWA_CG_CON	(cam_rawa_base + 0x0000)
#define CAMSYS_RAWA_CG_SET	(cam_rawa_base + 0x0004)
#define CAMSYS_RAWA_CG_CLR	(cam_rawa_base + 0x0008)

#define CAMSYS_RAWB_CG_CON	(cam_rawb_base + 0x0000)
#define CAMSYS_RAWB_CG_SET	(cam_rawb_base + 0x0004)
#define CAMSYS_RAWB_CG_CLR	(cam_rawb_base + 0x0008)

#define CAMSYS_RAWC_CG_CON	(cam_rawc_base + 0x0000)
#define CAMSYS_RAWC_CG_SET	(cam_rawc_base + 0x0004)
#define CAMSYS_RAWC_CG_CLR	(cam_rawc_base + 0x0008)

#define IMG1_CG_CON		(img1_base + 0x0000)
#define IMG1_CG_SET		(img1_base + 0x0004)
#define IMG1_CG_CLR		(img1_base + 0x0008)

#define IMG2_CG_CON		(img2_base + 0x0000)
#define IMG2_CG_SET		(img2_base + 0x0004)
#define IMG2_CG_CLR		(img2_base + 0x0008)

#define IIC_WRAP_C_CG_CON	(iic_wrap_c_base + 0x0e00)
#define IIC_WRAP_C_CG_SET	(iic_wrap_c_base + 0x0e08)
#define IIC_WRAP_C_CG_CLR	(iic_wrap_c_base + 0x0e04)
#define IIC_WRAP_E_CG_CON	(iic_wrap_e_base + 0x0e00)
#define IIC_WRAP_E_CG_SET	(iic_wrap_e_base + 0x0e08)
#define IIC_WRAP_E_CG_CLR	(iic_wrap_e_base + 0x0e04)
#define IIC_WRAP_N_CG_CON	(iic_wrap_n_base + 0x0e00)
#define IIC_WRAP_N_CG_SET	(iic_wrap_n_base + 0x0e08)
#define IIC_WRAP_N_CG_CLR	(iic_wrap_n_base + 0x0e04)
#define IIC_WRAP_S_CG_CON	(iic_wrap_s_base + 0x0e00)
#define IIC_WRAP_S_CG_SET	(iic_wrap_s_base + 0x0e08)
#define IIC_WRAP_S_CG_CLR	(iic_wrap_s_base + 0x0e04)

#define IPE_CG_CON		(ipe_base + 0x0000)
#define IPE_CG_SET		(ipe_base + 0x0004)
#define IPE_CG_CLR		(ipe_base + 0x0008)

#define MFG_CG_CON		(mfgcfg_base + 0x0000)
#define MFG_CG_SET		(mfgcfg_base + 0x0004)
#define MFG_CG_CLR		(mfgcfg_base + 0x0008)

#define MM_CG_CON0		(mmsys_config_base + 0x100)
#define MM_CG_SET0		(mmsys_config_base + 0x104)
#define MM_CG_CLR0		(mmsys_config_base + 0x108)
#define MM_CG_CON1		(mmsys_config_base + 0x110)
#define MM_CG_SET1		(mmsys_config_base + 0x114)
#define MM_CG_CLR1		(mmsys_config_base + 0x118)
#define MM_CG_CON2		(mmsys_config_base + 0x1a0)
#define MM_CG_SET2		(mmsys_config_base + 0x1a4)
#define MM_CG_CLR2		(mmsys_config_base + 0x1a8)

#define MSDC0_CG_CON0		(msdc0_base + 0x0b4)

#define PERICFG_CG_CON0		(pericfg_base + 0x020c)

#define VENC_C1_CG_CON		(venc_c1_gcon_base + 0x0000)
#define VENC_C1_CG_SET		(venc_c1_gcon_base + 0x0004)
#define VENC_C1_CG_CLR		(venc_c1_gcon_base + 0x0008)
#define VENC_CG_CON		(venc_gcon_base + 0x0000)
#define VENC_CG_SET		(venc_gcon_base + 0x0004)
#define VENC_CG_CLR		(venc_gcon_base + 0x0008)

#define VDEC_CKEN_SET		(vdec_gcon_base + 0x0000)
#define VDEC_CKEN_CLR		(vdec_gcon_base + 0x0004)
#define VDEC_LARB1_CKEN_SET	(vdec_gcon_base + 0x0008)
#define VDEC_LARB1_CKEN_CLR	(vdec_gcon_base + 0x000C)
#define VDEC_LAT_CKEN_SET	(vdec_gcon_base + 0x0200)
#define VDEC_LAT_CKEN_CLR	(vdec_gcon_base + 0x0204)

#define VDEC_SOC_CKEN_SET	(vdec_soc_gcon_base + 0x0000)
#define VDEC_SOC_CKEN_CLR	(vdec_soc_gcon_base + 0x0004)
#define VDEC_SOC_LARB1_CKEN_SET	(vdec_soc_gcon_base + 0x0008)
#define VDEC_SOC_LARB1_CKEN_CLR	(vdec_soc_gcon_base + 0x000C)
#define VDEC_SOC_LAT_CKEN_SET	(vdec_soc_gcon_base + 0x0200)
#define VDEC_SOC_LAT_CKEN_CLR	(vdec_soc_gcon_base + 0x0204)

#define MDP_CG_CON0		(mdp_base + 0x100)
#define MDP_CG_SET0		(mdp_base + 0x104)
#define MDP_CG_CLR0		(mdp_base + 0x108)
#define MDP_CG_CON1		(mdp_base + 0x110)
#define MDP_CG_SET1		(mdp_base + 0x114)
#define MDP_CG_CLR1		(mdp_base + 0x118)
#define MDP_CG_CON2		(mdp_base + 0x120)
#define MDP_CG_SET2		(mdp_base + 0x124)
#define MDP_CG_CLR2		(mdp_base + 0x128)

#define APU_VCORE_CG_CON	(apu_vcore_base + 0x0000)
#define APU_VCORE_CG_SET	(apu_vcore_base + 0x0004)
#define APU_VCORE_CG_CLR	(apu_vcore_base + 0x0008)

#define APU_CONN_CG_CON		(apu_conn_base + 0x0000)
#define APU_CONN_CG_SET		(apu_conn_base + 0x0004)
#define APU_CONN_CG_CLR		(apu_conn_base + 0x0008)

#define APU_CORE0_CG_CON	(apu0_base + 0x0100)
#define APU_CORE0_CG_SET	(apu0_base + 0x0104)
#define APU_CORE0_CG_CLR	(apu0_base + 0x0108)

#define APU_CORE1_CG_CON	(apu1_base + 0x0100)
#define APU_CORE1_CG_SET	(apu1_base + 0x0104)
#define APU_CORE1_CG_CLR	(apu1_base + 0x0108)

#define APU_CORE2_CG_CON	(apu2_base + 0x0100)
#define APU_CORE2_CG_SET	(apu2_base + 0x0104)
#define APU_CORE2_CG_CLR	(apu2_base + 0x0108)

#define APU_MDLA0_CG_CON	(apu_mdla0_base + 0x0000)
#define APU_MDLA0_CG_SET	(apu_mdla0_base + 0x0004)
#define APU_MDLA0_CG_CLR	(apu_mdla0_base + 0x0008)

#define APU_MDLA1_CG_CON	(apu_mdla1_base + 0x0000)
#define APU_MDLA1_CG_SET	(apu_mdla1_base + 0x0004)
#define APU_MDLA1_CG_CLR	(apu_mdla1_base + 0x0008)

#define GCE_CG_CON		(gce_base + 0xF0)
#define MDP_GCE_CG_CON		(mdp_gce_base + 0xF0)

/* MT6885 Subsys CG init settings */

#define APU_CORE0_CG	0x7
#define APU_CORE1_CG	0x7
#define APU_CORE2_CG	0x7
#define APU_VCORE_CG	0xf
#define APU_CONN_CG	0xff
#define APU_MDLA0_CG	0x3FFF
#define APU_MDLA1_CG	0x3FFF

#define AUDIO_CG0	0x1F1C0304
#define AUDIO_CG1	0xF033F0F0
#define AUDIO_CG2	0x5B

#define CAMSYS_CG	0x3FFCF
#define CAMRAWA_CG	0x7
#define CAMRAWB_CG	0x7
#define CAMRAWC_CG	0x7

#define GCE_CG		0x00010000
#define MDP_GCE_CG	0x00010000

#define IMG1_CG		0x1c7
#define IMG2_CG		0x1c7

#define IIC_WRAP_C_CG	0xF
#define IIC_WRAP_E_CG	0x3
#define IIC_WRAP_N_CG	0x3
#define IIC_WRAP_S_CG	0x1F

#define IPE_CG		0x7F
#define MFG_CG		0x1
#define MSDC0_CG	0x00400000
#define PERICFG_CG	0x80000000

#define VDEC_CKEN_CG		0x00000111
#define VDEC_LARB1_CKEN_CG	0x1
#define VDEC_LAT_CKEN_CG	0x00000111

#define VDEC_SOC_CKEN_CG	0x00000111
#define VDEC_SOC_LARB1_CKEN_CG	0x1
#define VDEC_SOC_LAT_CKEN_CG	0x00000111

#define VENC_C1_CG	0x10011111
#define VENC_CG		0x10011111

#define MDP_CG0		0xFFFFFFFF
#define MDP_CG1		0x13FF
#define MDP_CG2		0x303

#define INFRA_CG0	0x032F8000	/* pwm: 21~15, uart:24,25 */
#define INFRA_CG1	0x00000800	/* cpum: 11 */
#define INFRA_CG2	0x0
#define INFRA_CG3	0x0

static const struct mtk_fixed_clk fixed_clks[] __initconst = {
	FIXED_CLK(TOP_CLK26M, "f26m_sel", "clk26m", 26000000),
};

static const struct mtk_fixed_factor top_divs[] __initconst = {
	FACTOR(TOP_CLK13M, "clk13m", "clk26m", 1, 2),
	FACTOR(TOP_F26M_CK_D2, "csw_f26m_ck_d2", "clk26m", 1, 2),

	FACTOR(TOP_MAINPLL_CK, "mainpll_ck", "mainpll", 1, 1),
	FACTOR(TOP_MAINPLL_D2, "mainpll_d2", "mainpll", 1, 2),
	FACTOR(TOP_MAINPLL_D3, "mainpll_d3", "mainpll", 1, 3),
	FACTOR(TOP_MAINPLL_D4, "mainpll_d4", "mainpll", 1, 4),
	FACTOR(TOP_MAINPLL_D4_D2, "mainpll_d4_d2", "mainpll", 1, 8),
	FACTOR(TOP_MAINPLL_D4_D4, "mainpll_d4_d4", "mainpll", 1, 16),
	FACTOR(TOP_MAINPLL_D4_D8, "mainpll_d4_d8", "mainpll", 1, 32),
	FACTOR(TOP_MAINPLL_D4_D16, "mainpll_d4_d16", "mainpll", 1, 64),
	FACTOR(TOP_MAINPLL_D5, "mainpll_d5", "mainpll", 1, 5),
	FACTOR(TOP_MAINPLL_D5_D2, "mainpll_d5_d2", "mainpll", 1, 10),
	FACTOR(TOP_MAINPLL_D5_D4, "mainpll_d5_d4", "mainpll", 1, 20),
	FACTOR(TOP_MAINPLL_D5_D8, "mainpll_d5_d8", "mainpll", 1, 40),
	FACTOR(TOP_MAINPLL_D6, "mainpll_d6", "mainpll", 1, 6),
	FACTOR(TOP_MAINPLL_D6_D2, "mainpll_d6_d2", "mainpll", 1, 12),
	FACTOR(TOP_MAINPLL_D6_D4, "mainpll_d6_d4", "mainpll", 1, 24),
	FACTOR(TOP_MAINPLL_D6_D8, "mainpll_d6_d8", "mainpll", 1, 48),
	FACTOR(TOP_MAINPLL_D7, "mainpll_d7", "mainpll", 1, 7),
	FACTOR(TOP_MAINPLL_D7_D2, "mainpll_d7_d2", "mainpll", 1, 14),
	FACTOR(TOP_MAINPLL_D7_D4, "mainpll_d7_d4", "mainpll", 1, 28),
	FACTOR(TOP_MAINPLL_D7_D8, "mainpll_d7_d8", "mainpll", 1, 56),
	FACTOR(TOP_MAINPLL_D9, "mainpll_d9", "mainpll", 1, 9),

	FACTOR(TOP_UNIVPLL_CK, "univpll_ck", "univpll", 1, 1),
	FACTOR(TOP_UNIVPLL_D2, "univpll_d2", "univpll", 1, 2),
	FACTOR(TOP_UNIVPLL_D3, "univpll_d3", "univpll", 1, 3),
	FACTOR(TOP_UNIVPLL_D4, "univpll_d4", "univpll", 1, 4),
	FACTOR(TOP_UNIVPLL_D4_D2, "univpll_d4_d2", "univpll", 1, 8),
	FACTOR(TOP_UNIVPLL_D4_D4, "univpll_d4_d4", "univpll", 1, 16),
	FACTOR(TOP_UNIVPLL_D4_D8, "univpll_d4_d8", "univpll", 1, 32),
	FACTOR(TOP_UNIVPLL_D5, "univpll_d5", "univpll", 1, 5),
	FACTOR(TOP_UNIVPLL_D5_D2, "univpll_d5_d2", "univpll", 1, 10),
	FACTOR(TOP_UNIVPLL_D5_D4, "univpll_d5_d4", "univpll", 1, 20),
	FACTOR(TOP_UNIVPLL_D5_D8, "univpll_d5_d8", "univpll", 1, 40),
	FACTOR(TOP_UNIVPLL_D5_D16, "univpll_d5_d16", "univpll", 1, 80),
	FACTOR(TOP_UNIVPLL_D6, "univpll_d6", "univpll", 1, 6),
	FACTOR(TOP_UNIVPLL_D6_D2, "univpll_d6_d2", "univpll", 1, 12),
	FACTOR(TOP_UNIVPLL_D6_D4, "univpll_d6_d4", "univpll", 1, 24),
	FACTOR(TOP_UNIVPLL_D6_D8, "univpll_d6_d8", "univpll", 1, 48),
	FACTOR(TOP_UNIVPLL_D6_D16, "univpll_d6_d16", "univpll", 1, 96),
	FACTOR(TOP_UNIVPLL_D7, "univpll_d7", "univpll", 1, 7),
	FACTOR(TOP_UNIVPLL_D7_D2, "univpll_d7_d2", "univpll", 1, 14),

	FACTOR(TOP_UNIVP_192M_CK, "univpll_192m_ck", "univpll", 1,
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

	FACTOR(TOP_APLL1_CK, "apll1_ck", "apll1", 1, 1),
	FACTOR(TOP_APLL1_D2, "apll1_d2", "apll1", 1, 2),
	FACTOR(TOP_APLL1_D4, "apll1_d4", "apll1", 1, 4),
	FACTOR(TOP_APLL1_D8, "apll1_d8", "apll1", 1, 8),

	FACTOR(TOP_APLL2_CK, "apll2_ck", "apll2", 1, 1),
	FACTOR(TOP_APLL2_D2, "apll2_d2", "apll2", 1, 2),
	FACTOR(TOP_APLL2_D4, "apll2_d4", "apll2", 1, 4),
	FACTOR(TOP_APLL2_D8, "apll2_d8", "apll2", 1, 8),

	FACTOR(TOP_MMPLL_D3, "mmpll_d3", "mmpll", 1, 3),
	FACTOR(TOP_MMPLL_D4, "mmpll_d4", "mmpll", 1, 4),
	FACTOR(TOP_MMPLL_D4_D2, "mmpll_d4_d2", "mmpll", 1, 8),
	FACTOR(TOP_MMPLL_D4_D4, "mmpll_d4_d4", "mmpll", 1, 16),
	FACTOR(TOP_MMPLL_D5, "mmpll_d5", "mmpll", 1, 5),
	FACTOR(TOP_MMPLL_D5_D2, "mmpll_d5_d2", "mmpll", 1, 10),
	FACTOR(TOP_MMPLL_D5_D4, "mmpll_d5_d4", "mmpll", 1, 20),
	FACTOR(TOP_MMPLL_D6, "mmpll_d6", "mmpll", 1, 6),
	FACTOR(TOP_MMPLL_D6_D2, "mmpll_d6_d2", "mmpll", 1, 12),
	FACTOR(TOP_MMPLL_D7, "mmpll_d7", "mmpll", 1, 7),
	FACTOR(TOP_MMPLL_D9, "mmpll_d9", "mmpll", 1, 9),
	FACTOR(TOP_APUPLL_CK, "apupll_ck", "apupll", 1, 2),
	FACTOR(TOP_TVDPLL_CK, "tvdpll_ck", "tvdpll", 1, 1),
	FACTOR(TOP_TVDPLL_D2, "tvdpll_d2", "tvdpll", 1, 2),
	FACTOR(TOP_TVDPLL_D4, "tvdpll_d4", "tvdpll", 1, 4),
	FACTOR(TOP_TVDPLL_D8, "tvdpll_d8", "tvdpll", 1, 8),
	FACTOR(TOP_TVDPLL_D16, "tvdpll_d16", "tvdpll", 1, 16),

	/* missing in clk table*/
	FACTOR(TOP_MFGPLL_CK, "mfgpll_ck", "mfgpll", 1,	1),

	/* missing in clk table*/
	FACTOR(TOP_ADSPPLL_CK, "adsppll_ck", "adsppll", 1, 1),

	FACTOR(TOP_MSDCPLL_CK, "msdcpll_ck", "msdcpll", 1, 1),
	FACTOR(TOP_MSDCPLL_D2, "msdcpll_d2", "msdcpll", 1, 2),
	FACTOR(TOP_MSDCPLL_D4, "msdcpll_d4", "msdcpll", 1, 4),
	FACTOR(TOP_MSDCPLL_D8, "msdcpll_d8", "msdcpll", 1, 8),
	FACTOR(TOP_MSDCPLL_D16, "msdcpll_d16", "msdcpll", 1, 16),

	/* MT6885: "ulposc" not found in clktable*/
	FACTOR(TOP_AD_OSC_CK, "ad_osc_ck", "ulposc", 1, 1),
	FACTOR(TOP_OSC_D2, "osc_d2", "ulposc", 1, 2),
	FACTOR(TOP_OSC_D4, "osc_d4", "ulposc", 1, 4),
	FACTOR(TOP_OSC_D8, "osc_d8", "ulposc", 1, 8),
	FACTOR(TOP_OSC_D16, "osc_d16", "ulposc", 1, 16),
	FACTOR(TOP_OSC_D10, "osc_d10", "ulposc", 1, 10),
	FACTOR(TOP_OSC_D20, "osc_d20", "ulposc", 1, 20),
	FACTOR(TOP_AD_OSC_CK_2, "ad_osc_ck_2", "ulposc", 1, 1),
	FACTOR(TOP_OSC2_D2, "osc2_d2", "ulposc", 1, 2),
	FACTOR(TOP_OSC2_D3, "osc2_d3", "ulposc", 1, 3),

	FACTOR(TOP_TVDPLL_MAINPLL_D2_CK, "tvdpll_mainpll_d2_ck",
		"tvdpll", 1, 2),
};

static const char * const axi_parents[] __initconst = {
	"clk26m",
	"mainpll_d4_d4",
	"mainpll_d7_d2",
	"mainpll_d4_d2",
	"mainpll_d5_d2",
	"mainpll_d6_d2",
	"osc_d4"
};

static const char * const spm_parents[] __initconst = {
	"clk26m",
	"osc_d10",
	"mainpll_d7_d4",
	"clkrtc"
};

static const char * const scp_parents[] __initconst = {
	"clk26m",
	"univpll_d5",
	"mainpll_d6_d2",
	"mainpll_d6",
	"univpll_d6",
	"mainpll_d4_d2",
	"mainpll_d5_d2",
	"univpll_d4_d2"
};

static const char * const bus_aximem_parents[] __initconst = {
	"clk26m",
	"mainpll_d7_d2",
	"mainpll_d4_d2",
	"mainpll_d5_d2",
	"mainpll_d6"
};

static const char * const disp_parents[] __initconst = {
	"clk26m",
	"univpll_d6_d2",
	"mainpll_d5_d2",
	"mmpll_d6_d2",
	"univpll_d5_d2",
	"univpll_d4_d2",
	"mmpll_d7",
	"univpll_d6",
	"mainpll_d4",
	"mmpll_d5_d2"
};

static const char * const mdp_parents[] __initconst = {
	"clk26m",
	"mainpll_d5_d2",
	"mmpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d6",
	"univpll_d6",
	"mainpll_d4",
	"tvdpll_ck",
	"univpll_d4",
	"mmpll_d5_d2"
};

static const char * const img1_parents[] __initconst = {
	"clk26m",
	"univpll_d4",
	"tvdpll_ck",
	"mainpll_d4",
	"univpll_d5",
	"mmpll_d6",
	"univpll_d6",
	"mainpll_d6",
	"mmpll_d4_d2",
	"mainpll_d4_d2",
	"mmpll_d6_d2",
	"mmpll_d5_d2"
};

static const char * const img2_parents[] __initconst = {
	"clk26m",
	"univpll_d4",
	"tvdpll_ck",
	"mainpll_d4",
	"univpll_d5",
	"mmpll_d6",
	"univpll_d6",
	"mainpll_d6",
	"mmpll_d4_d2",
	"mainpll_d4_d2",
	"mmpll_d6_d2",
	"mmpll_d5_d2"
};

static const char * const ipe_parents[] __initconst = {
	"clk26m",
	"mainpll_d4",
	"mmpll_d6",
	"univpll_d6",
	"mainpll_d6",
	"univpll_d4_d2",
	"mainpll_d4_d2",
	"mmpll_d6_d2",
	"mmpll_d5_d2"
};

static const char * const dpe_parents[] __initconst = {
	"clk26m",
	"mainpll_d4",
	"mmpll_d6",
	"univpll_d6",
	"mainpll_d6",
	"univpll_d4_d2",
	"univpll_d5_d2",
	"mmpll_d6_d2"
};

static const char * const cam_parents[] __initconst = {
	"clk26m",
	"mainpll_d4",
	"mmpll_d6",
	"univpll_d4",
	"univpll_d5",
	"univpll_d6",
	"mmpll_d7",
	"univpll_d4_d2",
	"mainpll_d4_d2",
	"univpll_d6_d2"
};

static const char * const ccu_parents[] __initconst = {
	"clk26m",
	"mainpll_d4",
	"mmpll_d6",
	"mainpll_d6",
	"mmpll_d7",
	"univpll_d4_d2",
	"mmpll_d6_d2",
	"mmpll_d5_d2",
	"univpll_d5",
	"univpll_d6_d2"
};

static const char * const dsp_parents[] __initconst = {
	"clk26m",
	"mainpll_d4_d4",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d6",
	"mmpll_d7",
	"mmpll_d6",
	"univpll_d5",
	"mmpll_d5",
	"tvdpll_ck",
	"tvdpll_mainpll_d2_ck",
	"univpll_d4",
	"mainpll_d3",
	"apupll_ck",
	"mmpll_d4"
};

static const char * const dsp1_parents[] __initconst = {
	"clk26m",
	"mainpll_d4_d4",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d6",
	"mmpll_d7",
	"mmpll_d6",
	"univpll_d5",
	"mmpll_d5",
	"tvdpll_ck",
	"tvdpll_mainpll_d2_ck",
	"univpll_d4",
	"mainpll_d3",
	"apupll_ck",
	"univpll_d3"
};

static const char * const dsp2_parents[] __initconst = {
	"clk26m",
	"mainpll_d4_d4",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d6",
	"mmpll_d7",
	"mmpll_d6",
	"univpll_d5",
	"mmpll_d5",
	"tvdpll_ck",
	"tvdpll_mainpll_d2_ck",
	"univpll_d4",
	"mainpll_d3",
	"apupll_ck",
	"univpll_d3"
};

static const char * const dsp3_parents[] __initconst = {
	"clk26m",
	"mainpll_d4_d4",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d6",
	"mmpll_d7",
	"mmpll_d6",
	"univpll_d5",
	"mmpll_d5",
	"tvdpll_ck",
	"tvdpll_mainpll_d2_ck",
	"univpll_d4",
	"mainpll_d3",
	"apupll_ck",
	"univpll_d3"
};

static const char * const dsp4_parents[] __initconst = {
	"clk26m",
	"mainpll_d4_d4",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d6",
	"mmpll_d7",
	"mmpll_d6",
	"univpll_d5",
	"mmpll_d5",
	"tvdpll_ck",
	"tvdpll_mainpll_d2_ck",
	"univpll_d4",
	"mainpll_d3",
	"apupll_ck",
	"univpll_d3"
};

static const char * const dsp5_parents[] __initconst = {
	"clk26m",
	"mainpll_d4_d4",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d6",
	"univpll_d6",
	"mmpll_d6",
	"univpll_d5",
	"mainpll_d4",
	"tvdpll_ck",
	"univpll_d4",
	"mmpll_d4",
	"mainpll_d3",
	"univpll_d3",
	"apupll_ck"
};

static const char * const dsp6_parents[] __initconst = {
	"clk26m",
	"mainpll_d4_d4",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d6",
	"univpll_d6",
	"mmpll_d6",
	"univpll_d5",
	"mainpll_d4",
	"tvdpll_ck",
	"univpll_d4",
	"mmpll_d4",
	"mainpll_d3",
	"univpll_d3",
	"apupll_ck"
};

static const char * const dsp7_parents[] __initconst = {
	"clk26m",
	"mainpll_d4_d4",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d6",
	"mmpll_d7",
	"univpll_d6",
	"mmpll_d6",
	"univpll_d5",
	"mmpll_d5",
	"tvdpll_ck",
	"tvdpll_mainpll_d2_ck",
	"univpll_d4",
	"mainpll_d3",
	"univpll_d3"
};

static const char * const ipu_if_parents[] __initconst = {
	"clk26m",
	"univpll_d6_d4",
	"mainpll_d4_d4",
	"univpll_d6_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d6",
	"mmpll_d7",
	"univpll_d6",
	"mmpll_d6",
	"univpll_d5",
	"mmpll_d5",
	"tvdpll_ck",
	"tvdpll_mainpll_d2_ck",
	"univpll_d4",
	"apupll_ck"
};

static const char * const mfg_parents[] __initconst = {
	"clk26m",
	"mfgpll_ck",
	"univpll_d6",
	"mainpll_d5_d2"
};

static const char * const camtg_parents[] __initconst = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d6_d8",
	"univpll_192m_d4",
	"univpll_d6_d16",
	"csw_f26m_ck_d2",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const camtg2_parents[] __initconst = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d6_d8",
	"univpll_192m_d4",
	"univpll_d6_d16",
	"csw_f26m_ck_d2",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const camtg3_parents[] __initconst = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d6_d8",
	"univpll_192m_d4",
	"univpll_d6_d16",
	"csw_f26m_ck_d2",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const camtg4_parents[] __initconst = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d6_d8",
	"univpll_192m_d4",
	"univpll_d6_d16",
	"csw_f26m_ck_d2",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const uart_parents[] __initconst = {
	"clk26m",
	"univpll_d6_d8"
};

static const char * const spi_parents[] __initconst = {
	"clk26m",
	"mainpll_d5_d4",
	"mainpll_d6_d4",
	"msdcpll_d4"
};

static const char * const msdc50_0_hclk_parents[] __initconst = {
	"clk26m",
	"mainpll_d4_d2",
	"mainpll_d6_d2"
};

static const char * const msdc50_0_parents[] __initconst = {
	"clk26m",
	"msdcpll_ck",
	"msdcpll_d2",
	"univpll_d4_d4",
	"mainpll_d6_d2",
	"univpll_d4_d2"
};

static const char * const msdc30_1_parents[] __initconst = {
	"clk26m",
	"univpll_d6_d2",
	"mainpll_d6_d2",
	"mainpll_d7_d2",
	"msdcpll_d2"
};

static const char * const audio_parents[] __initconst = {
	"clk26m",
	"mainpll_d5_d8",
	"mainpll_d7_d8",
	"mainpll_d4_d16"
};

static const char * const aud_intbus_parents[] __initconst = {
	"clk26m",
	"mainpll_d4_d4",
	"mainpll_d7_d4"
};

static const char * const pwrap_ulposc_parents[] __initconst = {
	"osc_d10",
	"clk26m",
	"osc_d4",
	"osc_d8",
	"osc_d16"
};

static const char * const atb_parents[] __initconst = {
	"clk26m",
	"mainpll_d4_d2",
	"mainpll_d5_d2"
};

static const char * const sspm_parents[] __initconst = {
	"clk26m",
	"mainpll_d5_d2",
	"univpll_d5_d2",
	"mainpll_d4_d2",
	"univpll_d4_d2",
	"mainpll_d6"
};

static const char * const dp_parents[] __initconst = {
	"clk26m",
	"tvdpll_d2",
	"tvdpll_d4",
	"tvdpll_d8",
	"tvdpll_d16"
};

static const char * const scam_parents[] __initconst = {
	"clk26m",
	"mainpll_d5_d4"
};

static const char * const disp_pwm_parents[] __initconst = {
	"clk26m",
	"univpll_d6_d4",
	"osc_d2",
	"osc_d4",
	"osc_d16"
};

static const char * const usb_top_parents[] __initconst = {
	"clk26m",
	"univpll_d5_d4",
	"univpll_d6_d4",
	"univpll_d5_d2"
};

static const char * const ssusb_xhci_parents[] __initconst = {
	"clk26m",
	"univpll_d5_d4",
	"univpll_d6_d4",
	"univpll_d5_d2"
};

static const char * const i2c_parents[] __initconst = {
	"clk26m",
	"mainpll_d4_d8",
	"univpll_d5_d4"
};

static const char * const seninf_parents[] __initconst = {
	"clk26m",
	"univpll_d4_d4",
	"univpll_d6_d2",
	"univpll_d4_d2",
	"univpll_d7",
	"univpll_d6",
	"mmpll_d6",
	"univpll_d5"
};

static const char * const seninf1_parents[] __initconst = {
	"clk26m",
	"univpll_d4_d4",
	"univpll_d6_d2",
	"univpll_d4_d2",
	"univpll_d7",
	"univpll_d6",
	"mmpll_d6",
	"univpll_d5"
};

static const char * const seninf2_parents[] __initconst = {
	"clk26m",
	"univpll_d4_d4",
	"univpll_d6_d2",
	"univpll_d4_d2",
	"univpll_d7",
	"univpll_d6",
	"mmpll_d6",
	"univpll_d5"
};

static const char * const seninf3_parents[] __initconst = {
	"clk26m",
	"univpll_d4_d4",
	"univpll_d6_d2",
	"univpll_d4_d2",
	"univpll_d7",
	"univpll_d6",
	"mmpll_d6",
	"univpll_d5"
};

static const char * const dxcc_parents[] __initconst = {
	"clk26m",
	"mainpll_d4_d2",
	"mainpll_d4_d4",
	"mainpll_d4_d8"
};

static const char * const aud_engen1_parents[] __initconst = {
	"clk26m",
	"apll1_d2",
	"apll1_d4",
	"apll1_d8"
};

static const char * const aud_engen2_parents[] __initconst = {
	"clk26m",
	"apll2_d2",
	"apll2_d4",
	"apll2_d8"
};

static const char * const aes_ufsfde_parents[] __initconst = {
	"clk26m",
	"mainpll_d4",
	"mainpll_d4_d2",
	"mainpll_d6",
	"mainpll_d4_d4",
	"univpll_d4_d2",
	"univpll_d6"
};

static const char * const ufs_parents[] __initconst = {
	"clk26m",
	"mainpll_d4_d4",
	"mainpll_d4_d8",
	"univpll_d4_d4",
	"mainpll_d6_d2",
	"mainpll_d5_d2",
	"msdcpll_d2"
};

static const char * const aud_1_parents[] __initconst = {
	"clk26m",
	"apll1_ck"
};

static const char * const aud_2_parents[] __initconst = {
	"clk26m",
	"apll2_ck"
};

static const char * const adsp_parents[] __initconst = {
	"clk26m",
	"mainpll_d6",
	"mainpll_d5_d2",
	"univpll_d4_d4",
	"univpll_d4",
	"univpll_d6",
	"adsppll_ck"
};

static const char * const dpmaif_main_parents[] __initconst = {
	"clk26m",
	"univpll_d4_d4",
	"mainpll_d6",
	"mainpll_d4_d2",
	"univpll_d4_d2"
};

static const char * const venc_parents[] __initconst = {
	"clk26m",
	"mmpll_d7",
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
	"tvdpll_mainpll_d2_ck"
};

static const char * const vdec_parents[] __initconst = {
	"clk26m",
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

static const char * const vdec_lat_parents[] __initconst = {
	"clk26m",
	"univpll_192m_d2",
	"univpll_d5_d4",
	"univpll_d4_d4",
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
	"mmpll_d4"
};

static const char * const camtm_parents[] __initconst = {
	"clk26m",
	"univpll_d7",
	"univpll_d6_d2",
	"univpll_d4_d2"
};

static const char * const pwm_parents[] __initconst = {
	"clk26m",
	"univpll_d4_d8"
};

static const char * const audio_h_parents[] __initconst = {
	"clk26m",
	"univpll_d7",
	"apll1_ck",
	"apll2_ck"
};

static const char * const camtg5_parents[] __initconst = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d6_d8",
	"univpll_192m_d4",
	"univpll_d6_d16",
	"csw_f26m_ck_d2",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const camtg6_parents[] __initconst = {
	"clk26m",
	"univpll_192m_d8",
	"univpll_d6_d8",
	"univpll_192m_d4",
	"univpll_d6_d16",
	"csw_f26m_ck_d2",
	"univpll_192m_d16",
	"univpll_192m_d32"
};

static const char * const mcupm_parents[] __initconst = {
	"clk26m",
	"mainpll_d7_d2",
	"mainpll_d7_d4"
};

static const char * const spmi_mst_parents[] __initconst = {
	"clk26m",
	"csw_f26m_ck_d2",
	"osc_d8",
	"osc_d10",
	"osc_d16",
	"osc_d20",
	"clkrtc"
};

static const char * const dvfsrc_parents[] __initconst = {
	"clk26m",
	"osc_d10"
};


/* TOP AUDIO I2S clock mux */
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

static const char * const i2s6_m_ck_parents[] __initconst = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const i2s7_m_ck_parents[] __initconst = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const i2s8_m_ck_parents[] __initconst = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const i2s9_m_ck_parents[] __initconst = {
	"aud_1_sel",
	"aud_2_sel"
};


#define INV_UPD_REG	0xFFFFFFFF
#define INV_UPD_SHF	-1
#define INV_MUX_GATE	-1


#define TOP_MUX_AXI_UPD_SHIFT		0
#define TOP_MUX_SPM_UPD_SHIFT		1
#define TOP_MUX_SCP_UPD_SHIFT		2
#define TOP_MUX_BUS_AXIMEM_UPD_SHIFT	3
#define TOP_MUX_DISP_UPD_SHIFT		4
#define TOP_MUX_MDP_UPD_SHIFT		5
#define TOP_MUX_IMG1_UPD_SHIFT		6
#define TOP_MUX_IMG2_UPD_SHIFT		7
#define TOP_MUX_IPE_UPD_SHIFT		8
#define TOP_MUX_DPE_UPD_SHIFT		9
#define TOP_MUX_CAM_UPD_SHIFT		10
#define TOP_MUX_CCU_UPD_SHIFT		11
#define TOP_MUX_DSP_UPD_SHIFT		12
#define TOP_MUX_DSP1_UPD_SHIFT		13
#define TOP_MUX_DSP2_UPD_SHIFT		14
#define TOP_MUX_DSP3_UPD_SHIFT		15
#define TOP_MUX_DSP4_UPD_SHIFT		16
#define TOP_MUX_DSP5_UPD_SHIFT		17
#define TOP_MUX_DSP6_UPD_SHIFT		18
#define TOP_MUX_DSP7_UPD_SHIFT		19
#define TOP_MUX_IPU_IF_UPD_SHIFT	20
#define TOP_MUX_MFG_UPD_SHIFT		21
#define TOP_MUX_CAMTG_UPD_SHIFT		22
#define TOP_MUX_CAMTG2_UPD_SHIFT	23
#define TOP_MUX_CAMTG3_UPD_SHIFT	24
#define TOP_MUX_CAMTG4_UPD_SHIFT	25
#define TOP_MUX_UART_UPD_SHIFT		26
#define TOP_MUX_SPI_UPD_SHIFT		27
#define TOP_MUX_MSDC50_0_HCLK_UPD_SHIFT	28
#define TOP_MUX_MSDC50_0_UPD_SHIFT	29
#define TOP_MUX_MSDC30_1_UPD_SHIFT	30
#define TOP_MUX_AUDIO_UPD_SHIFT		0
#define TOP_MUX_AUD_INTBUS_UPD_SHIFT	1
#define TOP_MUX_PWRAP_ULPOSC_UPD_SHIFT	2
#define TOP_MUX_ATB_UPD_SHIFT		3
#define TOP_MUX_SSPM_UPD_SHIFT		4
#define TOP_MUX_DP_UPD_SHIFT		5
#define TOP_MUX_SCAM_UPD_SHIFT		6
#define TOP_MUX_DISP_PWM_UPD_SHIFT	7
#define TOP_MUX_USB_TOP_UPD_SHIFT	8
#define TOP_MUX_SSUSB_XHCI_UPD_SHIFT	9
#define TOP_MUX_I2C_UPD_SHIFT		10
#define TOP_MUX_SENINF_UPD_SHIFT	11
#define TOP_MUX_SENINF1_UPD_SHIFT	12
#define TOP_MUX_SENINF2_UPD_SHIFT	13
#define TOP_MUX_SENINF3_UPD_SHIFT	14
#define TOP_MUX_DXCC_UPD_SHIFT		15
#define TOP_MUX_AUD_ENGEN1_UPD_SHIFT	16
#define TOP_MUX_AUD_ENGEN2_UPD_SHIFT	17
#define TOP_MUX_AES_UFSFDE_UPD_SHIFT	18
#define TOP_MUX_UFS_UPD_SHIFT		19
#define TOP_MUX_AUD_1_UPD_SHIFT		20
#define TOP_MUX_AUD_2_UPD_SHIFT		21
#define TOP_MUX_ADSP_UPD_SHIFT		22
#define TOP_MUX_DPMAIF_MAIN_UPD_SHIFT	23
#define TOP_MUX_VENC_UPD_SHIFT		24
#define TOP_MUX_VDEC_UPD_SHIFT		25
#define TOP_MUX_VDEC_LAT_UPD_SHIFT	26
#define TOP_MUX_CAMTM_UPD_SHIFT		27
#define TOP_MUX_PWM_UPD_SHIFT		28
#define TOP_MUX_AUDIO_H_UPD_SHIFT	29
#define TOP_MUX_CAMTG5_UPD_SHIFT	0
#define TOP_MUX_CAMTG6_UPD_SHIFT	1
#define TOP_MUX_MCUPM_UPD_SHIFT		2
#define TOP_MUX_SPMI_MST_UPD_SHIFT	3
#define TOP_MUX_DVFSRC_UPD_SHIFT	30


static const struct mtk_mux top_muxes[] __initconst = {
#if MT_CCF_BRINGUP
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD(TOP_MUX_AXI, "axi_sel", axi_parents,
		CLK_CFG_0, CLK_CFG_0_SET, CLK_CFG_0_CLR,
		0, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_SPM, "spm_sel", spm_parents,
		CLK_CFG_0, CLK_CFG_0_SET, CLK_CFG_0_CLR,
		8, 2, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_SCP, "scp_sel", scp_parents,
		CLK_CFG_0, CLK_CFG_0_SET, CLK_CFG_0_CLR,
		16, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_BUS_AXIMEM, "bus_aximem_sel",
		bus_aximem_parents,
		CLK_CFG_0, CLK_CFG_0_SET, CLK_CFG_0_CLR,
		24, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	/* CLK_CFG_1 */
	MUX_CLR_SET_UPD(TOP_MUX_DISP, "disp_sel", disp_parents,
		CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR,
		0, 4, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_MDP, "mdp_sel", mdp_parents,
		CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR,
		8, 4, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_IMG1, "img1_sel", img1_parents,
		CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR,
		16, 4, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_IMG2, "img2_sel", img2_parents,
		CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR,
		24, 4, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	/* CLK_CFG_2 */
	MUX_CLR_SET_UPD(TOP_MUX_IPE, "ipe_sel", ipe_parents,
		CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR,
		0, 4, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_DPE, "dpe_sel", dpe_parents,
		CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR,
		8, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_CAM, "cam_sel", cam_parents,
		CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR,
		16, 4, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_CCU, "ccu_sel", ccu_parents,
		CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR,
		24, 4, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	/* CLK_CFG_3 */
	MUX_CLR_SET_UPD(TOP_MUX_DSP, "dsp_sel", dsp_parents,
		CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR,
		0, 4, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_DSP1, "dsp1_sel", dsp1_parents,
		CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR,
		8, 4, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_DSP2, "dsp2_sel", dsp2_parents,
		CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR,
		16, 4, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_DSP3, "dsp3_sel", dsp3_parents,
		CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR,
		24, 4, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	/* CLK_CFG_4 */
	MUX_CLR_SET_UPD(TOP_MUX_DSP4, "dsp4_sel", dsp4_parents,
		CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR,
		0, 4, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_DSP5, "dsp5_sel", dsp5_parents,
		CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR,
		8, 4, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_DSP6, "dsp6_sel", dsp6_parents,
		CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR,
		16, 4, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_DSP7, "dsp7_sel", dsp7_parents,
		CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR,
		24, 4, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	/* CLK_CFG_5 */
	MUX_CLR_SET_UPD(TOP_MUX_IPU_IF, "ipu_if_sel", ipu_if_parents,
		CLK_CFG_5, CLK_CFG_5_SET, CLK_CFG_5_CLR,
		0, 4, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_MFG, "mfg_sel", mfg_parents,
		CLK_CFG_5, CLK_CFG_5_SET, CLK_CFG_5_CLR,
		8, 2, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_CAMTG, "camtg_sel", camtg_parents,
		CLK_CFG_5, CLK_CFG_5_SET, CLK_CFG_5_CLR,
		16, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_CAMTG2, "camtg2_sel", camtg2_parents,
		CLK_CFG_5, CLK_CFG_5_SET, CLK_CFG_5_CLR,
		24, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	/* CLK_CFG_6 */
	MUX_CLR_SET_UPD(TOP_MUX_CAMTG3, "camtg3_sel", camtg3_parents,
		CLK_CFG_6, CLK_CFG_6_SET, CLK_CFG_6_CLR,
		0, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_CAMTG4, "camtg4_sel", camtg4_parents,
		CLK_CFG_6, CLK_CFG_6_SET, CLK_CFG_6_CLR,
		8, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_UART, "uart_sel", uart_parents,
		CLK_CFG_6, CLK_CFG_6_SET, CLK_CFG_6_CLR,
		16, 1, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_SPI, "spi_sel", spi_parents,
		CLK_CFG_6, CLK_CFG_6_SET, CLK_CFG_6_CLR,
		24, 2, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	/* CLK_CFG_7 */
	MUX_CLR_SET_UPD(TOP_MUX_MSDC50_0_HCLK, "msdc50_0_hclk_sel",
		msdc50_0_hclk_parents,
		CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR,
		0, 2, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_MSDC50_0, "msdc50_0_sel", msdc50_0_parents,
		CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR,
		8, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_MSDC30_1, "msdc30_1_sel", msdc30_1_parents,
		CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR,
		16, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_AUDIO, "audio_sel", audio_parents,
		CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR,
		24, 2, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	/* CLK_CFG_8 */
	MUX_CLR_SET_UPD(TOP_MUX_AUD_INTBUS, "aud_intbus_sel",
		aud_intbus_parents,
		CLK_CFG_8, CLK_CFG_8_SET, CLK_CFG_8_CLR,
		0, 2, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_PWRAP_ULPOSC, "pwrap_ulposc_sel",
		pwrap_ulposc_parents,
		CLK_CFG_8, CLK_CFG_8_SET, CLK_CFG_8_CLR,
		8, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_ATB, "atb_sel", atb_parents,
		CLK_CFG_8, CLK_CFG_8_SET, CLK_CFG_8_CLR,
		16, 2, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_SSPM, "sspm_sel", sspm_parents,
		CLK_CFG_8, CLK_CFG_8_SET, CLK_CFG_8_CLR,
		24, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	/* CLK_CFG_9 */
	MUX_CLR_SET_UPD(TOP_MUX_DP, "dp_sel", dp_parents,
		CLK_CFG_9, CLK_CFG_9_SET, CLK_CFG_9_CLR,
		0, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_SCAM, "scam_sel", scam_parents,
		CLK_CFG_9, CLK_CFG_9_SET, CLK_CFG_9_CLR,
		8, 1, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_DISP_PWM, "disp_pwm_sel", disp_pwm_parents,
		CLK_CFG_9, CLK_CFG_9_SET, CLK_CFG_9_CLR,
		16, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_USB_TOP, "usb_top_sel", usb_top_parents,
		CLK_CFG_9, CLK_CFG_9_SET, CLK_CFG_9_CLR,
		24, 2, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	/* CLK_CFG_10 */
	MUX_CLR_SET_UPD(TOP_MUX_SSUSB_XHCI, "ssusb_xhci_sel",
		ssusb_xhci_parents,
		CLK_CFG_10, CLK_CFG_10_SET, CLK_CFG_10_CLR,
		0, 2, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_I2C, "i2c_sel", i2c_parents,
		CLK_CFG_10, CLK_CFG_10_SET, CLK_CFG_10_CLR,
		8, 2, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_SENINF, "seninf_sel", seninf_parents,
		CLK_CFG_10, CLK_CFG_10_SET, CLK_CFG_10_CLR,
		16, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_SENINF1, "seninf1_sel", seninf1_parents,
		CLK_CFG_10, CLK_CFG_10_SET, CLK_CFG_10_CLR,
		24, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	/* CLK_CFG_11 */
	MUX_CLR_SET_UPD(TOP_MUX_SENINF2, "seninf2_sel", seninf2_parents,
		CLK_CFG_11, CLK_CFG_11_SET, CLK_CFG_11_CLR,
		0, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_SENINF3, "seninf3_sel", seninf3_parents,
		CLK_CFG_11, CLK_CFG_11_SET, CLK_CFG_11_CLR,
		8, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_DXCC, "dxcc_sel", dxcc_parents,
		CLK_CFG_11, CLK_CFG_11_SET, CLK_CFG_11_CLR,
		16, 2, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_AUD_ENGEN1, "aud_engen1_sel",
		aud_engen1_parents,
		CLK_CFG_11, CLK_CFG_11_SET, CLK_CFG_11_CLR,
		24, 2, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	/* CLK_CFG_12 */
	MUX_CLR_SET_UPD(TOP_MUX_AUD_ENGEN2, "aud_engen2_sel",
		aud_engen2_parents,
		CLK_CFG_12, CLK_CFG_12_SET, CLK_CFG_12_CLR,
		0, 2, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_AES_UFSFDE, "aes_ufsfde_sel",
		aes_ufsfde_parents,
		CLK_CFG_12, CLK_CFG_12_SET, CLK_CFG_12_CLR,
		8, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_UFS, "ufs_sel", ufs_parents,
		CLK_CFG_12, CLK_CFG_12_SET, CLK_CFG_12_CLR,
		16, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_AUD_1, "aud_1_sel", aud_1_parents,
		CLK_CFG_12, CLK_CFG_12_SET, CLK_CFG_12_CLR,
		24, 1, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	/* CLK_CFG_13 */
	MUX_CLR_SET_UPD(TOP_MUX_AUD_2, "aud_2_sel", aud_2_parents,
		CLK_CFG_13, CLK_CFG_13_SET, CLK_CFG_13_CLR,
		0, 1, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_ADSP, "adsp_sel", adsp_parents,
		CLK_CFG_13, CLK_CFG_13_SET, CLK_CFG_13_CLR,
		8, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_DPMAIF_MAIN, "dpmaif_main_sel",
		dpmaif_main_parents,
		CLK_CFG_13, CLK_CFG_13_SET, CLK_CFG_13_CLR,
		16, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_VENC, "venc_sel", venc_parents,
		CLK_CFG_13, CLK_CFG_13_SET, CLK_CFG_13_CLR,
		24, 4, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	/* CLK_CFG_14 */
	MUX_CLR_SET_UPD(TOP_MUX_VDEC, "vdec_sel", vdec_parents,
		CLK_CFG_14, CLK_CFG_14_SET, CLK_CFG_14_CLR,
		0, 4, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_VDEC_LAT, "vdec_lat_sel", vdec_lat_parents,
		CLK_CFG_14, CLK_CFG_14_SET, CLK_CFG_14_CLR,
		8, 4, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_CAMTM, "camtm_sel", camtm_parents,
		CLK_CFG_14, CLK_CFG_14_SET, CLK_CFG_14_CLR,
		16, 2, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_PWM, "pwm_sel", pwm_parents,
		CLK_CFG_14, CLK_CFG_14_SET, CLK_CFG_14_CLR,
		24, 1, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	/* CLK_CFG_15 */
	MUX_CLR_SET_UPD(TOP_MUX_AUDIO_H, "audio_h_sel", audio_h_parents,
		CLK_CFG_15, CLK_CFG_15_SET, CLK_CFG_15_CLR,
		0, 2, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_CAMTG5, "camtg5_sel", camtg5_parents,
		CLK_CFG_15, CLK_CFG_15_SET, CLK_CFG_15_CLR,
		8, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_CAMTG6, "camtg6_sel", camtg6_parents,
		CLK_CFG_15, CLK_CFG_15_SET, CLK_CFG_15_CLR,
		16, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_MCUPM, "mcupm_sel", mcupm_parents,
		CLK_CFG_15, CLK_CFG_15_SET, CLK_CFG_15_CLR,
		24, 2, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	/* CLK_CFG_16 */
	MUX_CLR_SET_UPD(TOP_MUX_SPMI_MST, "spmi_mst_sel", spmi_mst_parents,
		CLK_CFG_16, CLK_CFG_16_SET, CLK_CFG_16_CLR,
		0, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_DVFSRC, "dvfsrc_sel", dvfsrc_parents,
		CLK_CFG_16, CLK_CFG_16_SET, CLK_CFG_16_CLR,
		8, 1, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
#else
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD(TOP_MUX_AXI, "axi_sel", axi_parents,
		CLK_CFG_0, CLK_CFG_0_SET, CLK_CFG_0_CLR,
		0, 3, INV_MUX_GATE, INV_UPD_REG, INV_UPD_SHF),
	MUX_CLR_SET_UPD(TOP_MUX_SPM, "spm_sel", spm_parents,
		CLK_CFG_0, CLK_CFG_0_SET, CLK_CFG_0_CLR,
		8, 2, 15, CLK_CFG_UPDATE, TOP_MUX_SPM_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_SCP, "scp_sel", scp_parents,
		CLK_CFG_0, CLK_CFG_0_SET, CLK_CFG_0_CLR,
		16, 3, 23 /* INV_MUX_GATE */, CLK_CFG_UPDATE,
		TOP_MUX_SCP_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_BUS_AXIMEM, "bus_aximem_sel",
		bus_aximem_parents,
		CLK_CFG_0, CLK_CFG_0_SET, CLK_CFG_0_CLR,
		24, 3, 31, CLK_CFG_UPDATE, TOP_MUX_BUS_AXIMEM_UPD_SHIFT),
	/* CLK_CFG_1 */
	MUX_CLR_SET_UPD(TOP_MUX_DISP, "disp_sel", disp_parents,
		CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR,
		0, 4, 7, CLK_CFG_UPDATE, TOP_MUX_DISP_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_MDP, "mdp_sel", mdp_parents,
		CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR,
		8, 4, 15, CLK_CFG_UPDATE, TOP_MUX_MDP_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_IMG1, "img1_sel", img1_parents,
		CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR,
		16, 4, 23, CLK_CFG_UPDATE, TOP_MUX_IMG1_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_IMG2, "img2_sel", img2_parents,
		CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR,
		24, 4, 31, CLK_CFG_UPDATE, TOP_MUX_IMG2_UPD_SHIFT),
	/* CLK_CFG_2 */
	MUX_CLR_SET_UPD(TOP_MUX_IPE, "ipe_sel", ipe_parents,
		CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR,
		0, 4, 7, CLK_CFG_UPDATE, TOP_MUX_IPE_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_DPE, "dpe_sel", dpe_parents,
		CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR,
		8, 3, 15, CLK_CFG_UPDATE, TOP_MUX_DPE_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_CAM, "cam_sel", cam_parents,
		CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR,
		16, 4, 23, CLK_CFG_UPDATE, TOP_MUX_CAM_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_CCU, "ccu_sel", ccu_parents,
		CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR,
		24, 4, 31, CLK_CFG_UPDATE, TOP_MUX_CCU_UPD_SHIFT),
	/* CLK_CFG_3 */
	MUX_CLR_SET_UPD(TOP_MUX_DSP, "dsp_sel", dsp_parents,
		CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR,
		0, 4, 7, CLK_CFG_UPDATE, TOP_MUX_DSP_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_DSP1, "dsp1_sel", dsp1_parents,
		CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR,
		8, 4, 15, CLK_CFG_UPDATE, TOP_MUX_DSP1_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_DSP2, "dsp2_sel", dsp2_parents,
		CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR,
		16, 4, 23, CLK_CFG_UPDATE, TOP_MUX_DSP2_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_DSP3, "dsp3_sel", dsp3_parents,
		CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR,
		24, 4, 31, CLK_CFG_UPDATE, TOP_MUX_DSP3_UPD_SHIFT),
	/* CLK_CFG_4 */
	MUX_CLR_SET_UPD(TOP_MUX_DSP4, "dsp4_sel", dsp4_parents,
		CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR,
		0, 4, 7, CLK_CFG_UPDATE, TOP_MUX_DSP4_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_DSP5, "dsp5_sel", dsp5_parents,
		CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR,
		8, 4, 15, CLK_CFG_UPDATE, TOP_MUX_DSP5_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_DSP6, "dsp6_sel", dsp6_parents,
		CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR,
		16, 4, 23, CLK_CFG_UPDATE, TOP_MUX_DSP6_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_DSP7, "dsp7_sel", dsp7_parents,
		CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR,
		24, 4, 31, CLK_CFG_UPDATE, TOP_MUX_DSP7_UPD_SHIFT),
	/* CLK_CFG_5 */
	MUX_CLR_SET_UPD(TOP_MUX_IPU_IF, "ipu_if_sel", ipu_if_parents,
		CLK_CFG_5, CLK_CFG_5_SET, CLK_CFG_5_CLR,
		0, 4, 7, CLK_CFG_UPDATE, TOP_MUX_IPU_IF_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_MFG, "mfg_sel", mfg_parents,
		CLK_CFG_5, CLK_CFG_5_SET, CLK_CFG_5_CLR,
		8, 2, 15, CLK_CFG_UPDATE, TOP_MUX_MFG_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_CAMTG, "camtg_sel", camtg_parents,
		CLK_CFG_5, CLK_CFG_5_SET, CLK_CFG_5_CLR,
		16, 3, 23, CLK_CFG_UPDATE, TOP_MUX_CAMTG_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_CAMTG2, "camtg2_sel", camtg2_parents,
		CLK_CFG_5, CLK_CFG_5_SET, CLK_CFG_5_CLR,
		24, 3, 31, CLK_CFG_UPDATE, TOP_MUX_CAMTG2_UPD_SHIFT),
	/* CLK_CFG_6 */
	MUX_CLR_SET_UPD(TOP_MUX_CAMTG3, "camtg3_sel", camtg3_parents,
		CLK_CFG_6, CLK_CFG_6_SET, CLK_CFG_6_CLR,
		0, 3, 7, CLK_CFG_UPDATE, TOP_MUX_CAMTG3_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_CAMTG4, "camtg4_sel", camtg4_parents,
		CLK_CFG_6, CLK_CFG_6_SET, CLK_CFG_6_CLR,
		8, 3, 15, CLK_CFG_UPDATE, TOP_MUX_CAMTG4_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_UART, "uart_sel", uart_parents,
		CLK_CFG_6, CLK_CFG_6_SET, CLK_CFG_6_CLR,
		16, 1, 23, CLK_CFG_UPDATE, TOP_MUX_UART_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_SPI, "spi_sel", spi_parents,
		CLK_CFG_6, CLK_CFG_6_SET, CLK_CFG_6_CLR,
		24, 2, 31, CLK_CFG_UPDATE, TOP_MUX_SPI_UPD_SHIFT),
	/* CLK_CFG_7 */
	MUX_CLR_SET_UPD(TOP_MUX_MSDC50_0_HCLK, "msdc50_0_hclk_sel",
		msdc50_0_hclk_parents,
		CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR,
		0, 2, 7, CLK_CFG_UPDATE, TOP_MUX_MSDC50_0_HCLK_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_MSDC50_0, "msdc50_0_sel", msdc50_0_parents,
		CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR,
		8, 3, 15, CLK_CFG_UPDATE, TOP_MUX_MSDC50_0_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_MSDC30_1, "msdc30_1_sel", msdc30_1_parents,
		CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR,
		16, 3, 23, CLK_CFG_UPDATE, TOP_MUX_MSDC30_1_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_AUDIO, "audio_sel", audio_parents,
		CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR,
		24, 2, 31, CLK_CFG_UPDATE1, TOP_MUX_AUDIO_UPD_SHIFT),
	/* CLK_CFG_8 */
	MUX_CLR_SET_UPD(TOP_MUX_AUD_INTBUS, "aud_intbus_sel",
		aud_intbus_parents,
		CLK_CFG_8, CLK_CFG_8_SET, CLK_CFG_8_CLR,
		0, 2, 7, CLK_CFG_UPDATE1, TOP_MUX_AUD_INTBUS_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_PWRAP_ULPOSC, "pwrap_ulposc_sel",
		pwrap_ulposc_parents,
		CLK_CFG_8, CLK_CFG_8_SET, CLK_CFG_8_CLR,
		8, 3, 15, CLK_CFG_UPDATE1, TOP_MUX_PWRAP_ULPOSC_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_ATB, "atb_sel", atb_parents,
		CLK_CFG_8, CLK_CFG_8_SET, CLK_CFG_8_CLR,
		16, 2, 23, CLK_CFG_UPDATE1, TOP_MUX_ATB_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_SSPM, "sspm_sel", sspm_parents,
		CLK_CFG_8, CLK_CFG_8_SET, CLK_CFG_8_CLR,
		24, 3, 31, CLK_CFG_UPDATE1, TOP_MUX_SSPM_UPD_SHIFT),
	/* CLK_CFG_9 */
	MUX_CLR_SET_UPD(TOP_MUX_DP, "dp_sel", dp_parents,
		CLK_CFG_9, CLK_CFG_9_SET, CLK_CFG_9_CLR,
		0, 3, 7, CLK_CFG_UPDATE1, TOP_MUX_DP_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_SCAM, "scam_sel", scam_parents,
		CLK_CFG_9, CLK_CFG_9_SET, CLK_CFG_9_CLR,
		8, 1, 15, CLK_CFG_UPDATE1, TOP_MUX_SCAM_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_DISP_PWM, "disp_pwm_sel", disp_pwm_parents,
		CLK_CFG_9, CLK_CFG_9_SET, CLK_CFG_9_CLR,
		16, 3, 23, CLK_CFG_UPDATE1, TOP_MUX_DISP_PWM_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_USB_TOP, "usb_top_sel", usb_top_parents,
		CLK_CFG_9, CLK_CFG_9_SET, CLK_CFG_9_CLR,
		24, 2, 31, CLK_CFG_UPDATE1, TOP_MUX_USB_TOP_UPD_SHIFT),
	/* CLK_CFG_10 */
	MUX_CLR_SET_UPD(TOP_MUX_SSUSB_XHCI, "ssusb_xhci_sel",
		ssusb_xhci_parents,
		CLK_CFG_10, CLK_CFG_10_SET, CLK_CFG_10_CLR,
		0, 2, 7, CLK_CFG_UPDATE1, TOP_MUX_SSUSB_XHCI_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_I2C, "i2c_sel", i2c_parents,
		CLK_CFG_10, CLK_CFG_10_SET, CLK_CFG_10_CLR,
		8, 2, 15, CLK_CFG_UPDATE1, TOP_MUX_I2C_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_SENINF, "seninf_sel", seninf_parents,
		CLK_CFG_10, CLK_CFG_10_SET, CLK_CFG_10_CLR,
		16, 3, 23, CLK_CFG_UPDATE1, TOP_MUX_SENINF_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_SENINF1, "seninf1_sel", seninf1_parents,
		CLK_CFG_10, CLK_CFG_10_SET, CLK_CFG_10_CLR,
		24, 3, 31, CLK_CFG_UPDATE1, TOP_MUX_SENINF1_UPD_SHIFT),
	/* CLK_CFG_11 */
	MUX_CLR_SET_UPD(TOP_MUX_SENINF2, "seninf2_sel", seninf2_parents,
		CLK_CFG_11, CLK_CFG_11_SET, CLK_CFG_11_CLR,
		0, 3, 7, CLK_CFG_UPDATE1, TOP_MUX_SENINF2_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_SENINF3, "seninf3_sel", seninf3_parents,
		CLK_CFG_11, CLK_CFG_11_SET, CLK_CFG_11_CLR,
		8, 3, 15, CLK_CFG_UPDATE1, TOP_MUX_SENINF3_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_DXCC, "dxcc_sel", dxcc_parents,
		CLK_CFG_11, CLK_CFG_11_SET, CLK_CFG_11_CLR,
		16, 2, 23, CLK_CFG_UPDATE1, TOP_MUX_DXCC_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_AUD_ENGEN1, "aud_engen1_sel",
		aud_engen1_parents,
		CLK_CFG_11, CLK_CFG_11_SET, CLK_CFG_11_CLR,
		24, 2, 31, CLK_CFG_UPDATE1, TOP_MUX_AUD_ENGEN1_UPD_SHIFT),
	/* CLK_CFG_12 */
	MUX_CLR_SET_UPD(TOP_MUX_AUD_ENGEN2, "aud_engen2_sel",
		aud_engen2_parents,
		CLK_CFG_12, CLK_CFG_12_SET, CLK_CFG_12_CLR,
		0, 2, 7, CLK_CFG_UPDATE1, TOP_MUX_AUD_ENGEN2_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_AES_UFSFDE, "aes_ufsfde_sel",
		aes_ufsfde_parents,
		CLK_CFG_12, CLK_CFG_12_SET, CLK_CFG_12_CLR,
		8, 3, 15, CLK_CFG_UPDATE1, TOP_MUX_AES_UFSFDE_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_UFS, "ufs_sel", ufs_parents,
		CLK_CFG_12, CLK_CFG_12_SET, CLK_CFG_12_CLR,
		16, 3, 23, CLK_CFG_UPDATE1, TOP_MUX_UFS_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_AUD_1, "aud_1_sel", aud_1_parents,
		CLK_CFG_12, CLK_CFG_12_SET, CLK_CFG_12_CLR,
		24, 1, 31, CLK_CFG_UPDATE1, TOP_MUX_AUD_1_UPD_SHIFT),
	/* CLK_CFG_13 */
	MUX_CLR_SET_UPD(TOP_MUX_AUD_2, "aud_2_sel", aud_2_parents,
		CLK_CFG_13, CLK_CFG_13_SET, CLK_CFG_13_CLR,
		0, 1, 7, CLK_CFG_UPDATE1, TOP_MUX_AUD_2_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_ADSP, "adsp_sel", adsp_parents,
		CLK_CFG_13, CLK_CFG_13_SET, CLK_CFG_13_CLR,
		8, 3, 15, CLK_CFG_UPDATE1, TOP_MUX_ADSP_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_DPMAIF_MAIN, "dpmaif_main_sel",
		dpmaif_main_parents,
		CLK_CFG_13, CLK_CFG_13_SET, CLK_CFG_13_CLR,
		16, 3, 23, CLK_CFG_UPDATE1, TOP_MUX_DPMAIF_MAIN_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_VENC, "venc_sel", venc_parents,
		CLK_CFG_13, CLK_CFG_13_SET, CLK_CFG_13_CLR,
		24, 4, 31, CLK_CFG_UPDATE1, TOP_MUX_VENC_UPD_SHIFT),
	/* CLK_CFG_14 */
	MUX_CLR_SET_UPD(TOP_MUX_VDEC, "vdec_sel", vdec_parents,
		CLK_CFG_14, CLK_CFG_14_SET, CLK_CFG_14_CLR,
		0, 4, 7, CLK_CFG_UPDATE1, TOP_MUX_VDEC_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_VDEC_LAT, "vdec_lat_sel", vdec_lat_parents,
		CLK_CFG_14, CLK_CFG_14_SET, CLK_CFG_14_CLR,
		8, 4, 15, CLK_CFG_UPDATE1, TOP_MUX_VDEC_LAT_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_CAMTM, "camtm_sel", camtm_parents,
		CLK_CFG_14, CLK_CFG_14_SET, CLK_CFG_14_CLR,
		16, 2, 23, CLK_CFG_UPDATE1, TOP_MUX_CAMTM_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_PWM, "pwm_sel", pwm_parents,
		CLK_CFG_14, CLK_CFG_14_SET, CLK_CFG_14_CLR,
		24, 1, 31, CLK_CFG_UPDATE1, TOP_MUX_PWM_UPD_SHIFT),
	/* CLK_CFG_15 */
	MUX_CLR_SET_UPD(TOP_MUX_AUDIO_H, "audio_h_sel", audio_h_parents,
		CLK_CFG_15, CLK_CFG_15_SET, CLK_CFG_15_CLR,
		0, 2, 7, CLK_CFG_UPDATE1, TOP_MUX_AUDIO_H_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_CAMTG5, "camtg5_sel", camtg5_parents,
		CLK_CFG_15, CLK_CFG_15_SET, CLK_CFG_15_CLR,
		8, 3, 15, CLK_CFG_UPDATE2, TOP_MUX_CAMTG5_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_CAMTG6, "camtg6_sel", camtg6_parents,
		CLK_CFG_15, CLK_CFG_15_SET, CLK_CFG_15_CLR,
		16, 3, 23, CLK_CFG_UPDATE2, TOP_MUX_CAMTG6_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_MCUPM, "mcupm_sel", mcupm_parents,
		CLK_CFG_15, CLK_CFG_15_SET, CLK_CFG_15_CLR,
		24, 2, 31, CLK_CFG_UPDATE2, TOP_MUX_MCUPM_UPD_SHIFT),
	/* CLK_CFG_16 */
	MUX_CLR_SET_UPD(TOP_MUX_SPMI_MST, "spmi_mst_sel", spmi_mst_parents,
		CLK_CFG_16, CLK_CFG_16_SET, CLK_CFG_16_CLR,
		0, 3, 7, CLK_CFG_UPDATE2, TOP_MUX_SPMI_MST_UPD_SHIFT),
	MUX_CLR_SET_UPD(TOP_MUX_DVFSRC, "dvfsrc_sel", dvfsrc_parents,
		CLK_CFG_16, CLK_CFG_16_SET, CLK_CFG_16_CLR,
		8, 1, 15, CLK_CFG_UPDATE2, TOP_MUX_DVFSRC_UPD_SHIFT),
#endif
};


static const struct mtk_composite top_audmuxes[] __initconst = {
	MUX(TOP_I2S0_M_SEL, "i2s0_m_ck_sel", i2s0_m_ck_parents, 0x320, 8, 1),
	MUX(TOP_I2S1_M_SEL, "i2s1_m_ck_sel", i2s1_m_ck_parents, 0x320, 9, 1),
	MUX(TOP_I2S2_M_SEL, "i2s2_m_ck_sel", i2s2_m_ck_parents, 0x320, 10, 1),
	MUX(TOP_I2S3_M_SEL, "i2s3_m_ck_sel", i2s3_m_ck_parents, 0x320, 11, 1),
	MUX(TOP_I2S4_M_SEL, "i2s4_m_ck_sel", i2s4_m_ck_parents, 0x320, 12, 1),
	MUX(TOP_I2S5_M_SEL, "i2s5_m_ck_sel", i2s5_m_ck_parents, 0x328, 20, 1),
	MUX(TOP_I2S6_M_SEL, "i2s6_m_ck_sel", i2s6_m_ck_parents, 0x334, 28, 1),
	MUX(TOP_I2S7_M_SEL, "i2s7_m_ck_sel", i2s7_m_ck_parents, 0x334, 29, 1),
	MUX(TOP_I2S8_M_SEL, "i2s8_m_ck_sel", i2s8_m_ck_parents, 0x334, 30, 1),
	MUX(TOP_I2S9_M_SEL, "i2s9_m_ck_sel", i2s9_m_ck_parents, 0x334, 31, 1),

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
		0x328, 16, 0x328, 4, 28), /* MT6885 FIXME: only 4bits here. */

	DIV_GATE(TOP_APLL12_DIV6, "apll12_div6", "i2s6_m_ck_sel",
		0x334, 20, 0x338, 8, 0),
	DIV_GATE(TOP_APLL12_DIV7, "apll12_div7", "i2s7_m_ck_sel",
		0x334, 21, 0x338, 8, 8),
	DIV_GATE(TOP_APLL12_DIV8, "apll12_div8", "i2s8_m_ck_sel",
		0x334, 22, 0x338, 8, 16),
	DIV_GATE(TOP_APLL12_DIV9, "apll12_div9", "i2s9_m_ck_sel",
		0x334, 23, 0x338, 8, 24),
};

static int mtk_cg_bit_is_cleared(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 val;

	regmap_read(cg->regmap, cg->sta_ofs, &val);

	val &= BIT(cg->bit);

	return val == 0;
}
/* copy from clk-gate.c */
static int mtk_cg_bit_is_set(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 val;

	regmap_read(cg->regmap, cg->sta_ofs, &val);

	val &= BIT(cg->bit);

	return val != 0;
}

static void mtk_cg_set_bit(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);

	regmap_write(cg->regmap, cg->set_ofs, BIT(cg->bit));
}

static void mtk_cg_clr_bit(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);

	regmap_write(cg->regmap, cg->clr_ofs, BIT(cg->bit));
}

static void mtk_cg_set_bit_no_setclr(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 cgbit = BIT(cg->bit);

	regmap_update_bits(cg->regmap, cg->sta_ofs, cgbit, cgbit);
}

static void mtk_cg_clr_bit_no_setclr(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 cgbit = BIT(cg->bit);

	regmap_update_bits(cg->regmap, cg->sta_ofs, cgbit, 0);
}

static int mtk_cg_enable(struct clk_hw *hw)
{
	mtk_cg_clr_bit(hw);

	return 0;
}

static int mtk_cg_enable_inv(struct clk_hw *hw)
{
	mtk_cg_set_bit(hw);

	return 0;
}

static int mtk_cg_enable_no_setclr(struct clk_hw *hw)
{
	mtk_cg_clr_bit_no_setclr(hw);

	return 0;
}

static int mtk_cg_enable_inv_no_setclr(struct clk_hw *hw)
{
	mtk_cg_set_bit_no_setclr(hw);

	return 0;
}

/* copy from clk-gate.c */

static void mtk_cg_disable_dummy(struct clk_hw *hw)
{
	/* do nothing */
}

const struct clk_ops mtk_clk_gate_ops_setclr_dummy = {
	.is_enabled	= mtk_cg_bit_is_cleared,
	.enable		= mtk_cg_enable,
	.disable	= mtk_cg_disable_dummy,
};

const struct clk_ops mtk_clk_gate_ops_setclr_inv_dummy = {
	.is_enabled	= mtk_cg_bit_is_set,
	.enable		= mtk_cg_enable_inv,
	.disable	= mtk_cg_disable_dummy,
};

const struct clk_ops mtk_clk_gate_ops_no_setclr_dummy = {
	.is_enabled	= mtk_cg_bit_is_cleared,
	.enable		= mtk_cg_enable_no_setclr,
	.disable	= mtk_cg_disable_dummy,
};

const struct clk_ops mtk_clk_gate_ops_no_setclr_inv_dummy = {
	.is_enabled	= mtk_cg_bit_is_set,
	.enable		= mtk_cg_enable_inv_no_setclr,
	.disable	= mtk_cg_disable_dummy,
};

#if MT_CCF_BRINGUP
#define GATE		GATE_DUMMY		/* set/clr */
#define GATE_INV	GATE_INV_DUMMY		/* set/clr inverse */
#define GATE_STA	GATE_STA_DUMMY		/* no set/clr */
#define GATE_STA_INV	GATE_STA_INV_DUMMY	/* no set/clr and inverse */
#else /* MT_CCF_BRINGUP */
#define GATE(_id, _name, _parent, _regs, _shift, _flags) {	\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &_regs,					\
		.shift = _shift,				\
		.flags = _flags,				\
		.ops = &mtk_clk_gate_ops_setclr,		\
	}
#define GATE_INV(_id, _name, _parent, _regs, _shift, _flags) {  \
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &_regs,					\
		.shift = _shift,				\
		.flags = _flags,				\
		.ops = &mtk_clk_gate_ops_setclr_inv,		\
	}

#define GATE_STA(_id, _name, _parent, _regs, _shift, _flags) {	\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &_regs,					\
		.shift = _shift,				\
		.flags = _flags,				\
		.ops = &mtk_clk_gate_ops_no_setclr,		\
	}
#define GATE_STA_INV(_id, _name, _parent, _regs, _shift, _flags) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &_regs,					\
		.shift = _shift,				\
		.flags = _flags,				\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,		\
	}

#endif /* MT_CCF_BRINGUP */

#define GATE_DUMMY(_id, _name, _parent, _regs, _shift, _flags) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &_regs,					\
		.shift = _shift,				\
		.flags = _flags,				\
		.ops = &mtk_clk_gate_ops_setclr_dummy,		\
	}
#define GATE_INV_DUMMY(_id, _name, _parent, _regs, _shift, _flags) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &_regs,					\
		.shift = _shift,				\
		.flags = _flags,				\
		.ops = &mtk_clk_gate_ops_setclr_inv_dummy,	\
	}
#define GATE_STA_DUMMY(_id, _name, _parent, _regs, _shift, _flags) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &_regs,					\
		.shift = _shift,				\
		.flags = _flags,				\
		.ops = &mtk_clk_gate_ops_no_setclr_dummy,	\
	}
#define GATE_STA_INV_DUMMY(_id, _name, _parent, _regs, _shift, _flags) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &_regs,					\
		.shift = _shift,				\
		.flags = _flags,				\
		.ops = &mtk_clk_gate_ops_no_setclr_inv_dummy,	\
	}

static void __iomem *mtk_gate_common_init(struct device_node *node,
			char *name, const struct mtk_gate *clk_array,
			int nr_array, int nr_clks)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

#if MT_CCF_BRINGUP
	pr_notice("CCF %s: init\n", name);
#endif
	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", name);
		return NULL;
	}
	clk_data = mtk_alloc_clk_data(nr_clks);
	if (!clk_data) {
		pr_notice("%s(): allocate memory failed\n", name);
		return NULL;
	}
	mtk_clk_register_gates(node, clk_array,	nr_array, clk_data);
	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r) {
		pr_notice("%s(): could not register clock provider: %d\n",
						name, r);
		kfree(clk_data);
		return NULL;
	}

#if MT_CCF_BRINGUP
	pr_notice("CCF %s: init done\n", name);
#endif
	return base;
}

/*
 *
 *	Subsys Clocks
 *
 */
/*******************  APU Core 0 *******************************/
static struct mtk_gate_regs apu0_core_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static struct mtk_gate apu0_clks[] __initdata = {
	/* CORE_CG */
	GATE_DUMMY(APU0_APU_CG, "apu0_apu_cg", "dsp1_sel",
			apu0_core_cg_regs, 0, 0),
	GATE_DUMMY(APU0_AXI_M_CG, "apu0_axi_m_cg", "dsp1_sel",
			apu0_core_cg_regs, 1, 0),
	GATE_DUMMY(APU0_JTAG_CG, "apu0_jtag_cg", "dsp1_sel",
			apu0_core_cg_regs, 2, 0),
};

static void __iomem *apu0_base;
static void mtk_apu0_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	apu0_base = mtk_gate_common_init(node, "apu0", apu0_clks,
					ARRAY_SIZE(apu0_clks), APU0_NR_CLK);
	if (!apu0_base)
		return;
#if 0
	clk_writel(APU_CORE0_CG_CLR, APU_CORE0_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_apu0, "mediatek,apu0", mtk_apu0_init);

/*******************  APU Core 1 *******************************/
static struct mtk_gate_regs apu1_core_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static struct mtk_gate apu1_clks[] __initdata = {
	/* CORE_CG */
	GATE_DUMMY(APU1_APU_CG, "apu1_apu_cg", "dsp2_sel",
			 apu1_core_cg_regs, 0, 0),
	GATE_DUMMY(APU1_AXI_M_CG, "apu1_axi_m_cg", "dsp2_sel",
			 apu1_core_cg_regs, 1, 0),
	GATE_DUMMY(APU1_JTAG_CG, "apu1_jtag_cg", "dsp2_sel",
			 apu1_core_cg_regs, 2, 0),
};

static void __iomem *apu1_base;
static void mtk_apu1_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	apu1_base = mtk_gate_common_init(node, "apu1", apu1_clks,
					ARRAY_SIZE(apu1_clks), APU1_NR_CLK);
	if (!apu1_base)
		return;
#if 0
	clk_writel(APU_CORE1_CG_CLR, APU_CORE1_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_apu1, "mediatek,apu1", mtk_apu1_init);

/*******************  APU Core 2 *******************************/
static struct mtk_gate_regs apu2_core_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};
static struct mtk_gate apu2_clks[] __initdata = {
	/* CORE_CG */
	GATE_DUMMY(APU2_APU_CG, "apu2_apu_cg", "dsp3_sel",
			 apu2_core_cg_regs, 0, 0),
	GATE_DUMMY(APU2_AXI_M_CG, "apu2_axi_m_cg", "dsp3_sel",
			 apu2_core_cg_regs, 1, 0),
	GATE_DUMMY(APU2_JTAG_CG, "apu2_jtag_cg", "dsp3_sel",
			 apu2_core_cg_regs, 2, 0),
};

static void __iomem *apu2_base;
static void mtk_apu2_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	apu2_base = mtk_gate_common_init(node, "apu2", apu2_clks,
					ARRAY_SIZE(apu2_clks), APU2_NR_CLK);
	if (!apu2_base)
		return;
#if 0
	clk_writel(APU_CORE2_CG_CLR, APU_CORE2_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_apu2, "mediatek,apu2", mtk_apu2_init);

/*******************  APU VCore *******************************/
static struct mtk_gate_regs apusys_vcore_apusys_vcore_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};
static struct mtk_gate apusys_vcore_clks[] __initdata = {
	/* APUSYS_VCORE_CG */
	GATE_DUMMY(APUSYS_VCORE_AHB_CG, "apusys_vcore_ahb_cg", "ipu_if_sel",
			 apusys_vcore_apusys_vcore_cg_regs, 0, 0),
	GATE_DUMMY(APUSYS_VCORE_AXI_CG, "apusys_vcore_axi_cg", "ipu_if_sel",
			 apusys_vcore_apusys_vcore_cg_regs, 1, 0),
	GATE_DUMMY(APUSYS_VCORE_ADL_CG, "apusys_vcore_adl_cg", "ipu_if_sel",
			 apusys_vcore_apusys_vcore_cg_regs, 2, 0),
	GATE_DUMMY(APUSYS_VCORE_QOS_CG, "apusys_vcore_qos_cg", "ipu_if_sel",
			 apusys_vcore_apusys_vcore_cg_regs, 3, 0),
};

static void __iomem *apu_vcore_base;
static void mtk_apu_vcore_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	apu_vcore_base = mtk_gate_common_init(node, "vpuvcore",
			apusys_vcore_clks, ARRAY_SIZE(apusys_vcore_clks),
			APUSYS_VCORE_NR_CLK);
	if (!apu_vcore_base)
		return;
#if 0
	clk_writel(APU_VCORE_CG_CLR, APU_VCORE_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_apu_vcore, "mediatek,apu_vcore", mtk_apu_vcore_init);

/*******************  APU CONN *******************************/
static struct mtk_gate_regs apu_conn_apu_conn_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};
static struct mtk_gate apu_conn_clks[] __initdata = {
	/* APU_CONN_CG */
	GATE_DUMMY(APU_CONN_AHB_CG, "apu_conn_ahb_cg", "dsp_sel",
			 apu_conn_apu_conn_cg_regs, 1, 0),
	GATE_DUMMY(APU_CONN_AXI_CG, "apu_conn_axi_cg", "dsp_sel",
			 apu_conn_apu_conn_cg_regs, 2, 0),
	GATE_DUMMY(APU_CONN_ISP_CG, "apu_conn_isp_cg", "dsp_sel",
			 apu_conn_apu_conn_cg_regs, 3, 0),
	GATE_DUMMY(APU_CONN_CAM_ADL_CG, "apu_conn_cam_adl_cg", "dsp_sel",
			 apu_conn_apu_conn_cg_regs, 4, 0),
	GATE_DUMMY(APU_CONN_IMG_ADL_CG, "apu_conn_img_adl_cg", "dsp_sel",
			 apu_conn_apu_conn_cg_regs, 5, 0),
	GATE_DUMMY(APU_CONN_EMI_26M_CG, "apu_conn_emi_26m_cg", "dsp_sel",
			 apu_conn_apu_conn_cg_regs, 6, 0),
	GATE_DUMMY(APU_CONN_VPU_UDI_CG, "apu_conn_vpu_udi_cg", "dsp_sel",
			 apu_conn_apu_conn_cg_regs, 7, 0),
	GATE_DUMMY(APU_CONN_EDMA_0_CG, "apu_conn_edma_0_cg", "dsp_sel",
			 apu_conn_apu_conn_cg_regs, 8, 0),
	GATE_DUMMY(APU_CONN_EDMA_1_CG, "apu_conn_edma_1_cg", "dsp_sel",
			 apu_conn_apu_conn_cg_regs, 9, 0),
	GATE_DUMMY(APU_CONN_EDMAL_0_CG, "apu_conn_edmal_0_cg", "dsp_sel",
			 apu_conn_apu_conn_cg_regs, 10, 0),
	GATE_DUMMY(APU_CONN_EDMAL_1_CG, "apu_conn_edmal_1_cg", "dsp_sel",
			 apu_conn_apu_conn_cg_regs, 11, 0),
	GATE_DUMMY(APU_CONN_MNOC_CG, "apu_conn_mnoc_cg", "dsp_sel",
			 apu_conn_apu_conn_cg_regs, 12, 0),
	GATE_DUMMY(APU_CONN_TCM_CG, "apu_conn_tcm_cg", "dsp_sel",
			 apu_conn_apu_conn_cg_regs, 13, 0),
	GATE_DUMMY(APU_CONN_MD32_CG, "apu_conn_md32_cg", "dsp_sel",
			 apu_conn_apu_conn_cg_regs, 14, 0),
	GATE_DUMMY(APU_CONN_IOMMU_0_CG, "apu_conn_iommu_0_cg", "dsp7_sel",
			 apu_conn_apu_conn_cg_regs, 15, 0),
	GATE_DUMMY(APU_CONN_IOMMU_1_CG, "apu_conn_iommu_1_cg", "dsp7_sel",
			 apu_conn_apu_conn_cg_regs, 16, 0),
	GATE_DUMMY(APU_CONN_MD32_32K_CG, "apu_conn_md32_32k_cg", "dsp_sel",
			 apu_conn_apu_conn_cg_regs, 17, 0),
};

static void __iomem *apu_conn_base;
static void mtk_apu_conn_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	apu_conn_base = mtk_gate_common_init(node, "vpuconn", apu_conn_clks,
				ARRAY_SIZE(apu_conn_clks), APU_CONN_NR_CLK);
	if (!apu_conn_base)
		return;
#if 0
	clk_writel(APU_CONN_CG_CLR, APU_CONN_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_apu_conn, "mediatek,apu_conn", mtk_apu_conn_init);

/*******************  APU MDLA0 *******************************/
static struct mtk_gate_regs apu_mdla0_mdla_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};
static struct mtk_gate apu_mdla0_clks[] __initdata = {
	/* MDLA_CG */
	GATE_DUMMY(APU_MDLA0_MDLA_CG0, "apu_mdla0_mdla_cg0", "dsp6_sel",
			 apu_mdla0_mdla_cg_regs, 0, 0),
	GATE_DUMMY(APU_MDLA0_MDLA_CG1, "apu_mdla0_mdla_cg1", "dsp6_sel",
			 apu_mdla0_mdla_cg_regs, 1, 0),
	GATE_DUMMY(APU_MDLA0_MDLA_CG2, "apu_mdla0_mdla_cg2", "dsp6_sel",
			 apu_mdla0_mdla_cg_regs, 2, 0),
	GATE_DUMMY(APU_MDLA0_MDLA_CG3, "apu_mdla0_mdla_cg3", "dsp6_sel",
			 apu_mdla0_mdla_cg_regs, 3, 0),
	GATE_DUMMY(APU_MDLA0_MDLA_CG4, "apu_mdla0_mdla_cg4", "dsp6_sel",
			 apu_mdla0_mdla_cg_regs, 4, 0),
	GATE_DUMMY(APU_MDLA0_MDLA_CG5, "apu_mdla0_mdla_cg5", "dsp6_sel",
			 apu_mdla0_mdla_cg_regs, 5, 0),
	GATE_DUMMY(APU_MDLA0_MDLA_CG6, "apu_mdla0_mdla_cg6", "dsp6_sel",
			 apu_mdla0_mdla_cg_regs, 6, 0),
	GATE_DUMMY(APU_MDLA0_MDLA_CG7, "apu_mdla0_mdla_cg7", "dsp6_sel",
			 apu_mdla0_mdla_cg_regs, 7, 0),
	GATE_DUMMY(APU_MDLA0_MDLA_CG8, "apu_mdla0_mdla_cg8", "dsp6_sel",
			 apu_mdla0_mdla_cg_regs, 8, 0),
	GATE_DUMMY(APU_MDLA0_MDLA_CG9, "apu_mdla0_mdla_cg9", "dsp6_sel",
			 apu_mdla0_mdla_cg_regs, 9, 0),
	GATE_DUMMY(APU_MDLA0_MDLA_CG10, "apu_mdla0_mdla_cg10", "dsp6_sel",
			 apu_mdla0_mdla_cg_regs, 10, 0),
	GATE_DUMMY(APU_MDLA0_MDLA_CG11, "apu_mdla0_mdla_cg11", "dsp6_sel",
			 apu_mdla0_mdla_cg_regs, 11, 0),
	GATE_DUMMY(APU_MDLA0_MDLA_CG12, "apu_mdla0_mdla_cg12", "dsp6_sel",
			 apu_mdla0_mdla_cg_regs, 12, 0),
	GATE_DUMMY(APU_MDLA0_APB_CG, "apu_mdla0_apb_cg", "dsp6_sel",
			 apu_mdla0_mdla_cg_regs, 13, 0),
	GATE_DUMMY(APU_MDLA0_AXI_M_CG, "apu_mdla0_axim_cg", "dsp6_sel",
			 apu_mdla0_mdla_cg_regs, 14, 0),

};

static void __iomem *apu_mdla0_base;
static void mtk_apu_mdla0_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	apu_mdla0_base = mtk_gate_common_init(node, "vpu_mdla0",
			apu_mdla0_clks, ARRAY_SIZE(apu_mdla0_clks),
			APU_MDLA0_NR_CLK);
	if (!apu_mdla0_base)
		return;
#if 0
	clk_writel(APU_MDLA0_CG_CLR, APU_MDLA0_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_apu_mdla0, "mediatek,apu_mdla0", mtk_apu_mdla0_init);

/*******************  APU MDLA1 *******************************/
static struct mtk_gate_regs apu_mdla1_mdla_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};
static struct mtk_gate apu_mdla1_clks[] __initdata = {
	/* MDLA_CG */
	GATE_DUMMY(APU_MDLA1_MDLA_CG0, "apu_mdla1_mdla_cg0", "dsp6_sel",
			 apu_mdla1_mdla_cg_regs, 0, 0),
	GATE_DUMMY(APU_MDLA1_MDLA_CG1, "apu_mdla1_mdla_cg1", "dsp6_sel",
			 apu_mdla1_mdla_cg_regs, 1, 0),
	GATE_DUMMY(APU_MDLA1_MDLA_CG2, "apu_mdla1_mdla_cg2", "dsp6_sel",
			 apu_mdla1_mdla_cg_regs, 2, 0),
	GATE_DUMMY(APU_MDLA1_MDLA_CG3, "apu_mdla1_mdla_cg3", "dsp6_sel",
			 apu_mdla1_mdla_cg_regs, 3, 0),
	GATE_DUMMY(APU_MDLA1_MDLA_CG4, "apu_mdla1_mdla_cg4", "dsp6_sel",
			 apu_mdla1_mdla_cg_regs, 4, 0),
	GATE_DUMMY(APU_MDLA1_MDLA_CG5, "apu_mdla1_mdla_cg5", "dsp6_sel",
			 apu_mdla1_mdla_cg_regs, 5, 0),
	GATE_DUMMY(APU_MDLA1_MDLA_CG6, "apu_mdla1_mdla_cg6", "dsp6_sel",
			 apu_mdla1_mdla_cg_regs, 6, 0),
	GATE_DUMMY(APU_MDLA1_MDLA_CG7, "apu_mdla1_mdla_cg7", "dsp6_sel",
			 apu_mdla1_mdla_cg_regs, 7, 0),
	GATE_DUMMY(APU_MDLA1_MDLA_CG8, "apu_mdla1_mdla_cg8", "dsp6_sel",
			 apu_mdla1_mdla_cg_regs, 8, 0),
	GATE_DUMMY(APU_MDLA1_MDLA_CG9, "apu_mdla1_mdla_cg9", "dsp6_sel",
			 apu_mdla1_mdla_cg_regs, 9, 0),
	GATE_DUMMY(APU_MDLA1_MDLA_CG10, "apu_mdla1_mdla_cg10", "dsp6_sel",
			 apu_mdla1_mdla_cg_regs, 10, 0),
	GATE_DUMMY(APU_MDLA1_MDLA_CG11, "apu_mdla1_mdla_cg11", "dsp6_sel",
			 apu_mdla1_mdla_cg_regs, 11, 0),
	GATE_DUMMY(APU_MDLA1_MDLA_CG12, "apu_mdla1_mdla_cg12", "dsp6_sel",
			 apu_mdla1_mdla_cg_regs, 12, 0),
	GATE_DUMMY(APU_MDLA1_APB_CG, "apu_mdla1_apb_cg", "dsp6_sel",
			 apu_mdla1_mdla_cg_regs, 13, 0),
	GATE_DUMMY(APU_MDLA1_AXI_M_CG, "apu_mdla1_axim_cg", "dsp6_sel",
			 apu_mdla1_mdla_cg_regs, 14, 0),
};

static void __iomem *apu_mdla1_base;
static void mtk_apu_mdla1_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	apu_mdla1_base = mtk_gate_common_init(node, "vpu_mdla1",
			apu_mdla1_clks, ARRAY_SIZE(apu_mdla1_clks),
			APU_MDLA1_NR_CLK);
	if (!apu_mdla1_base)
		return;
#if 0
	clk_writel(APU_MDLA1_CG_CLR, APU_MDLA1_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_apu_mdla1, "mediatek,apu_mdla1", mtk_apu_mdla1_init);

/*******************  AUDIO Subsys *******************************/
static struct mtk_gate_regs audio_audio_top_con0_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};
static struct mtk_gate_regs audio_audio_top_con1_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x4,
	.sta_ofs = 0x4,
};
static struct mtk_gate_regs audio_audio_top_con2_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0x8,
	.sta_ofs = 0x8,
};

static struct mtk_gate audio_clks[] __initdata = {
	/* AUDIO_TOP_CON0 */
	GATE_STA(AUDIO_PDN_AFE, "audio_pdn_afe", "audio_sel",
			 audio_audio_top_con0_regs, 2, 0),
	GATE_STA(AUDIO_PDN_22M, "audio_pdn_22m", "aud_engen1_sel",
			 audio_audio_top_con0_regs, 8, 0),
	GATE_STA(AUDIO_PDN_24M, "audio_pdn_24m", "aud_engen2_sel",
			 audio_audio_top_con0_regs, 9, 0),
	GATE_STA(AUDIO_PDN_APLL2_TUNER, "audio_pdn_apll2_tuner",
			"aud_engen2_sel",
			 audio_audio_top_con0_regs, 18, 0),
	GATE_STA(AUDIO_PDN_APLL_TUNER, "audio_pdn_apll_tuner",
			"aud_engen1_sel",
			 audio_audio_top_con0_regs, 19, 0),
	GATE_STA(AUDIO_PDN_TDM_CK, "audio_pdn_tdm_ck", "aud_1_sel",
			 audio_audio_top_con0_regs, 20, 0),
	GATE_STA(AUDIO_PDN_ADC, "audio_pdn_adc", "audio_sel",
			 audio_audio_top_con0_regs, 24, 0),
	GATE_STA(AUDIO_PDN_DAC, "audio_pdn_dac", "audio_sel",
			 audio_audio_top_con0_regs, 25, 0),
	GATE_STA(AUDIO_PDN_DAC_PREDIS, "audio_pdn_dac_predis", "audio_sel",
			 audio_audio_top_con0_regs, 26, 0),
	GATE_STA(AUDIO_PDN_TML, "audio_pdn_tml", "audio_sel",
			 audio_audio_top_con0_regs, 27, 0),
	GATE_STA(AUDIO_PDN_NLE, "audio_pdn_nle", "audio_sel",
			 audio_audio_top_con0_regs, 28, 0),
	/* AUDIO_TOP_CON1 */
	GATE_STA(AUDIO_I2S1_BCLK_SW_CG, "audio_i2s1_bclk_sw_cg", "audio_sel",
			 audio_audio_top_con1_regs, 4, 0),
	GATE_STA(AUDIO_I2S2_BCLK_SW_CG, "audio_i2s2_bclk_sw_cg", "audio_sel",
			 audio_audio_top_con1_regs, 5, 0),
	GATE_STA(AUDIO_I2S3_BCLK_SW_CG, "audio_i2s3_bclk_sw_cg", "audio_sel",
			 audio_audio_top_con1_regs, 6, 0),
	GATE_STA(AUDIO_I2S4_BCLK_SW_CG, "audio_i2s4_bclk_sw_cg", "audio_sel",
			 audio_audio_top_con1_regs, 7, 0),
	GATE_STA(AUDIO_PDN_CONNSYS_I2S_ASRC, "audio_pdn_connsys_i2s_asrc",
			"audio_sel",
			 audio_audio_top_con1_regs, 12, 0),
	GATE_STA(AUDIO_PDN_GENERAL1_ASRC, "audio_pdn_general1_asrc",
			"audio_sel",
			 audio_audio_top_con1_regs, 13, 0),
	GATE_STA(AUDIO_PDN_GENERAL2_ASRC, "audio_pdn_general2_asrc",
			"audio_sel",
			 audio_audio_top_con1_regs, 14, 0),
	GATE_STA(AUDIO_PDN_DAC_HIRES, "audio_pdn_dac_hires", "audio_h_sel",
			 audio_audio_top_con1_regs, 15, 0),
	GATE_STA(AUDIO_PDN_ADC_HIRES, "audio_pdn_adc_hires", "audio_h_sel",
			 audio_audio_top_con1_regs, 16, 0),
	GATE_STA(AUDIO_PDN_ADC_HIRES_TML, "audio_pdn_adc_hires_tml",
			"audio_h_sel",
			 audio_audio_top_con1_regs, 17, 0),
	GATE_STA(AUDIO_PDN_ADDA6_ADC, "audio_pdn_adda6_adc", "audio_sel",
			 audio_audio_top_con1_regs, 20, 0),
	GATE_STA(AUDIO_PDN_ADDA6_ADC_HIRES, "audio_pdn_adda6_adc_hires",
			"audio_h_sel",
			 audio_audio_top_con1_regs, 21, 0),
	GATE_STA(AUDIO_PDN_3RD_DAC, "audio_pdn_3rd_dac", "audio_sel",
			 audio_audio_top_con1_regs, 28, 0),
	GATE_STA(AUDIO_PDN_3RD_DAC_PREDIS, "audio_pdn_3rd_dac_predis",
			"audio_sel",
			 audio_audio_top_con1_regs, 29, 0),
	GATE_STA(AUDIO_PDN_3RD_DAC_TML, "audio_pdn_3rd_dac_tml", "audio_sel",
			 audio_audio_top_con1_regs, 30, 0),
	GATE_STA(AUDIO_PDN_3RD_DAC_HIRES, "audio_pdn_3rd_dac_hires",
			"audio_h_sel",
			 audio_audio_top_con1_regs, 31, 0),
	/* AUDIO_TOP_CON2 */
	GATE_STA(AUDIO_I2S5_BCLK_SW_CG, "audio_i2s5_bclk_sw_cg", "audio_sel",
			 audio_audio_top_con2_regs, 0, 0),
	GATE_STA(AUDIO_I2S6_BCLK_SW_CG, "audio_i2s6_bclk_sw_cg", "audio_sel",
			 audio_audio_top_con2_regs, 1, 0),
	GATE_STA(AUDIO_I2S7_BCLK_SW_CG, "audio_i2s7_bclk_sw_cg", "audio_sel",
			 audio_audio_top_con2_regs, 2, 0),
	GATE_STA(AUDIO_I2S8_BCLK_SW_CG, "audio_i2s8_bclk_sw_cg", "audio_sel",
			 audio_audio_top_con2_regs, 3, 0),
	GATE_STA(AUDIO_I2S9_BCLK_SW_CG, "audio_i2s9_bclk_sw_cg", "audio_sel",
			 audio_audio_top_con2_regs, 4, 0),
};

static void __iomem *audio_base;
static void mtk_audio_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	audio_base = mtk_gate_common_init(node, "audio", audio_clks,
					ARRAY_SIZE(audio_clks), AUDIO_NR_CLK);
	if (!audio_base)
		return;
#if MT_CCF_BRINGUP
	clk_writel(AUDIO_TOP_CON0, clk_readl(AUDIO_TOP_CON0) & ~AUDIO_CG0);
	clk_writel(AUDIO_TOP_CON1, clk_readl(AUDIO_TOP_CON1) & ~AUDIO_CG1);
	clk_writel(AUDIO_TOP_CON2, clk_readl(AUDIO_TOP_CON2) & ~AUDIO_CG2);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_audio, "mediatek,audio", mtk_audio_init);

/*******************  CAM Main Subsys *******************************/
static struct mtk_gate_regs camsys_main_camsys_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};
static struct mtk_gate camsys_main_clks[] __initdata = {
	/* CAMSYS_CG */
	GATE_DUMMY(CAMSYS_MAIN_LARB13_CGPDN, "camsys_main_larb13_cgpdn",
			"cam_sel", camsys_main_camsys_cg_regs, 0, 0),
	GATE(CAMSYS_MAIN_DFP_VAD_CGPDN, "camsys_main_dfp_vad_cgpdn", "cam_sel",
			 camsys_main_camsys_cg_regs, 1, 0),
	GATE_DUMMY(CAMSYS_MAIN_LARB14_CGPDN, "camsys_main_larb14_cgpdn",
			"cam_sel", camsys_main_camsys_cg_regs, 2, 0),
	GATE_DUMMY(CAMSYS_MAIN_LARB15_CGPDN, "camsys_main_larb15_cgpdn",
			"cam_sel", camsys_main_camsys_cg_regs, 3, 0),

	GATE(CAMSYS_MAIN_CAM_CGPDN, "camsys_main_cam_cgpdn", "cam_sel",
			 camsys_main_camsys_cg_regs, 6, 0),
	GATE(CAMSYS_MAIN_CAMTG_CGPDN, "camsys_main_camtg_cgpdn", "cam_sel",
			 camsys_main_camsys_cg_regs, 7, 0),

	GATE(CAMSYS_MAIN_SENINF_CGPDN, "camsys_main_seninf_cgpdn", "cam_sel",
			 camsys_main_camsys_cg_regs, 8, 0),
	GATE(CAMSYS_MAIN_CAMSV0_CGPDN, "camsys_main_camsv0_cgpdn", "cam_sel",
			 camsys_main_camsys_cg_regs, 9, 0),
	GATE(CAMSYS_MAIN_CAMSV1_CGPDN, "camsys_main_camsv1_cgpdn", "cam_sel",
			 camsys_main_camsys_cg_regs, 10, 0),
	GATE(CAMSYS_MAIN_CAMSV2_CGPDN, "camsys_main_camsv2_cgpdn", "cam_sel",
			 camsys_main_camsys_cg_regs, 11, 0),
	GATE(CAMSYS_MAIN_CAMSV3_CGPDN, "camsys_main_camsv3_cgpdn", "cam_sel",
			 camsys_main_camsys_cg_regs, 12, 0),
	GATE(CAMSYS_MAIN_CCU0_CGPDN, "camsys_main_ccu0_cgpdn", "cam_sel",
			 camsys_main_camsys_cg_regs, 13, 0),
	GATE(CAMSYS_MAIN_CCU1_CGPDN, "camsys_main_ccu1_cgpdn", "cam_sel",
			 camsys_main_camsys_cg_regs, 14, 0),
	GATE(CAMSYS_MAIN_MRAW0_CGPDN, "camsys_main_mraw0_cgpdn", "cam_sel",
			 camsys_main_camsys_cg_regs, 15, 0),
	GATE(CAMSYS_MAIN_MRAW1_CGPDN, "camsys_main_mraw1_cgpdn", "cam_sel",
			 camsys_main_camsys_cg_regs, 16, 0),
	GATE(CAMSYS_MAIN_FAKE_ENG_CGPDN, "camsys_main_fake_eng_cgpdn",
			"cam_sel",
			 camsys_main_camsys_cg_regs, 17, 0),
};

void __iomem *cam_base;
static void mtk_camsys_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	cam_base = mtk_gate_common_init(node, "cam", camsys_main_clks,
			ARRAY_SIZE(camsys_main_clks), CAMSYS_MAIN_NR_CLK);
	if (!cam_base)
		return;
#if MT_CCF_BRINGUP
	clk_writel(CAMSYS_CG_CLR, CAMSYS_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_camsys, "mediatek,camsys", mtk_camsys_init);

/*******************  CAM RawA Subsys *******************************/
static struct mtk_gate_regs camsys_rawa_camsys_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};
static struct mtk_gate camsys_rawa_clks[] __initdata = {
	/* CAMSYS_CG */
	GATE_DUMMY(CAMSYS_RAWA_LARBX_CGPDN, "camsys_rawa_larbx_cgpdn",
			"cam_sel", camsys_rawa_camsys_cg_regs, 0, 0),
	GATE(CAMSYS_RAWA_CAM_CGPDN, "camsys_rawa_cam_cgpdn", "cam_sel",
			 camsys_rawa_camsys_cg_regs, 1, 0),
	GATE(CAMSYS_RAWA_CAMTG_CGPDN, "camsys_rawa_camtg_cgpdn", "cam_sel",
			 camsys_rawa_camsys_cg_regs, 2, 0),
};

void __iomem *cam_rawa_base;
static void mtk_camsys_rawa_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	cam_rawa_base = mtk_gate_common_init(node, "cam_rawa",
				camsys_rawa_clks, ARRAY_SIZE(camsys_rawa_clks),
				CAMSYS_RAWA_NR_CLK);
	if (!cam_rawa_base)
		return;
#if MT_CCF_BRINGUP
	clk_writel(CAMSYS_RAWA_CG_CLR, CAMRAWA_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_camsys_rawa, "mediatek,camsys_rawa",
						mtk_camsys_rawa_init);

/*******************  CAM RawB Subsys *******************************/
static struct mtk_gate_regs camsys_rawb_camsys_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};
static struct mtk_gate camsys_rawb_clks[] __initdata = {
	/* CAMSYS_CG */
	GATE_DUMMY(CAMSYS_RAWB_LARBX_CGPDN, "camsys_rawb_larbx_cgpdn",
			"cam_sel", camsys_rawb_camsys_cg_regs, 0, 0),
	GATE(CAMSYS_RAWB_CAM_CGPDN, "camsys_rawb_cam_cgpdn", "cam_sel",
			 camsys_rawb_camsys_cg_regs, 1, 0),
	GATE(CAMSYS_RAWB_CAMTG_CGPDN, "camsys_rawb_camtg_cgpdn", "cam_sel",
			 camsys_rawb_camsys_cg_regs, 2, 0),
};

void __iomem *cam_rawb_base;
static void mtk_camsys_rawb_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	cam_rawb_base = mtk_gate_common_init(node, "cam_rawb",
				camsys_rawb_clks, ARRAY_SIZE(camsys_rawb_clks),
				CAMSYS_RAWB_NR_CLK);
	if (!cam_rawb_base)
		return;
#if MT_CCF_BRINGUP
	clk_writel(CAMSYS_RAWB_CG_CLR, CAMRAWB_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_camsys_rawb, "mediatek,camsys_rawb",
							mtk_camsys_rawb_init);

/*******************  CAM RawC Subsys *******************************/
static struct mtk_gate_regs camsys_rawc_camsys_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};
static struct mtk_gate camsys_rawc_clks[] __initdata = {
	/* CAMSYS_CG */
	GATE_DUMMY(CAMSYS_RAWC_LARBX_CGPDN, "camsys_rawc_larbx_cgpdn",
			"cam_sel", camsys_rawc_camsys_cg_regs, 0, 0),
	GATE(CAMSYS_RAWC_CAM_CGPDN, "camsys_rawc_cam_cgpdn", "cam_sel",
			 camsys_rawc_camsys_cg_regs, 1, 0),
	GATE(CAMSYS_RAWC_CAMTG_CGPDN, "camsys_rawc_camtg_cgpdn", "cam_sel",
			 camsys_rawc_camsys_cg_regs, 2, 0),
};

void __iomem *cam_rawc_base;
static void mtk_camsys_rawc_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	cam_rawc_base = mtk_gate_common_init(node, "cam_rawc",
				camsys_rawc_clks, ARRAY_SIZE(camsys_rawc_clks),
				CAMSYS_RAWC_NR_CLK);
	if (!cam_rawc_base)
		return;
#if MT_CCF_BRINGUP
	clk_writel(CAMSYS_RAWC_CG_CLR, CAMRAWC_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_camsys_rawc, "mediatek,camsys_rawc",
						mtk_camsys_rawc_init);

/******************* GCE Subsys *******************************/
static struct mtk_gate_regs gce_gce_ctl_int0_regs = {
	.set_ofs = 0xf0,
	.clr_ofs = 0xf0,
	.sta_ofs = 0xf0,
};
static struct mtk_gate gce_clks[] __initdata = {
	/* GCE_CTL_INT0 */
	GATE_STA(GCE_SW_CG_0, "gce_sw_cg_0", "axi_sel",
			 gce_gce_ctl_int0_regs, 16, 0),
};

static void __iomem *gce_base;
static void mtk_gcesys_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	gce_base = mtk_gate_common_init(node, "gce", gce_clks,
					ARRAY_SIZE(gce_clks), GCE_NR_CLK);
	if (!gce_base)
		return;
#if MT_CCF_BRINGUP
	clk_writel(GCE_CG_CON, clk_readl(GCE_CG_CON) & ~(GCE_CG));
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_gcesys, "mediatek,gce_clock", mtk_gcesys_init);

/******************* IMG1 Subsys *******************************/
static struct mtk_gate_regs imgsys1_img_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};
static struct mtk_gate imgsys1_clks[] __initdata = {
	/* IMG_CG */
	GATE_DUMMY(IMGSYS1_LARB9_CGPDN, "imgsys1_larb9_cgpdn", "img1_sel",
			 imgsys1_img_cg_regs, 0, 0),
	GATE(IMGSYS1_LARB10_CGPDN, "imgsys1_larb10_cgpdn", "img1_sel",
			 imgsys1_img_cg_regs, 1, 0),
	GATE(IMGSYS1_DIP_CGPDN, "imgsys1_dip_cgpdn", "img1_sel",
			 imgsys1_img_cg_regs, 2, 0),
	GATE(IMGSYS1_MFB_CGPDN, "imgsys1_mfb_cgpdn", "img1_sel",
			 imgsys1_img_cg_regs, 6, 0),
	GATE(IMGSYS1_WPE_CGPDN, "imgsys1_wpe_cgpdn", "img1_sel",
			 imgsys1_img_cg_regs, 7, 0),
	GATE(IMGSYS1_MSS_CGPDN, "imgsys1_mss_cgpdn", "img1_sel",
			 imgsys1_img_cg_regs, 8, 0),
};

static void __iomem *img1_base;
static void mtk_imgsys1_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	img1_base = mtk_gate_common_init(node, "imgsys1", imgsys1_clks,
				ARRAY_SIZE(imgsys1_clks), IMGSYS1_NR_CLK);
	if (!img1_base)
		return;
#if MT_CCF_BRINGUP
	clk_writel(IMG1_CG_CLR, IMG1_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_imgsys1, "mediatek,imgsys", mtk_imgsys1_init);

/******************* IMG2 Subsys *******************************/
static struct mtk_gate_regs imgsys2_img_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};
static struct mtk_gate imgsys2_clks[] __initdata = {
	/* IMG_CG */
	GATE_DUMMY(IMGSYS2_LARB11_CGPDN, "imgsys2_larb11_cgpdn", "img2_sel",
			 imgsys2_img_cg_regs, 0, 0),
	GATE(IMGSYS2_LARB12_CGPDN, "imgsys2_larb12_cgpdn", "img2_sel",
			 imgsys2_img_cg_regs, 1, 0),
	GATE(IMGSYS2_DIP_CGPDN, "imgsys2_dip_cgpdn", "img2_sel",
			 imgsys2_img_cg_regs, 2, 0),
	GATE(IMGSYS2_MFB_CGPDN, "imgsys2_mfb_cgpdn", "img2_sel",
			 imgsys2_img_cg_regs, 6, 0),
	GATE(IMGSYS2_WPE_CGPDN, "imgsys2_wpe_cgpdn", "img2_sel",
			 imgsys2_img_cg_regs, 7, 0),
	GATE(IMGSYS2_MSS_CGPDN, "imgsys2_mss_cgpdn", "img2_sel",
			 imgsys2_img_cg_regs, 8, 0),
};

static void __iomem *img2_base;
static void mtk_imgsys2_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	img2_base = mtk_gate_common_init(node, "imgsys2", imgsys2_clks,
				ARRAY_SIZE(imgsys2_clks), IMGSYS2_NR_CLK);
	if (!img2_base)
		return;
#if MT_CCF_BRINGUP
	clk_writel(IMG2_CG_CLR, IMG2_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_imgsys2, "mediatek,imgsys2", mtk_imgsys2_init);

/******************* IIC Central Subsys *******************************/
/*
 * For MT6885, we use traditional "infra_ao i2c CG" as the parent of "imp_iic"
 * CGs, instead of the real i2c clkmux.
 *
 * So that "infra_ao i2c CG" and "imp_iic" can enable/disable simutanesouly,
 * which meets the MT6885 idle condition.
 */
static struct mtk_gate_regs imp_iic_wrap_c_ap_clock_cg_ro_cen_regs = {
	.set_ofs = 0xe08,
	.clr_ofs = 0xe04,
	.sta_ofs = 0xe00,
};
static struct mtk_gate imp_iic_wrap_c_clks[] __initdata = {
	/* AP_CLOCK_CG_RO_CEN */
	GATE(IMP_IIC_WRAP_C_AP_I2C0_CG_RO, "imp_iic_wrap_c_ap_i2c0_cg_ro",
			"infracfg_ao_i2c0_cg", /* "i2c_sel", */
			 imp_iic_wrap_c_ap_clock_cg_ro_cen_regs, 0, 0),
	GATE(IMP_IIC_WRAP_C_AP_I2C10_CG_RO, "imp_iic_wrap_c_ap_i2c10_cg_ro",
			"infracfg_ao_i2c0_cg", /* "i2c_sel", */
			 imp_iic_wrap_c_ap_clock_cg_ro_cen_regs, 1, 0),
	GATE(IMP_IIC_WRAP_C_AP_I2C11_CG_RO, "imp_iic_wrap_c_ap_i2c11_cg_ro",
			"infracfg_ao_i2c0_cg", /* "i2c_sel", */
			 imp_iic_wrap_c_ap_clock_cg_ro_cen_regs, 2, 0),
	GATE(IMP_IIC_WRAP_C_AP_I2C12_CG_RO, "imp_iic_wrap_c_ap_i2c12_cg_ro",
			"infracfg_ao_i2c0_cg", /* "i2c_sel", */
			 imp_iic_wrap_c_ap_clock_cg_ro_cen_regs, 3, 0),
	GATE(IMP_IIC_WRAP_C_AP_I2C13_CG_RO, "imp_iic_wrap_c_ap_i2c13_cg_ro",
			"infracfg_ao_i2c0_cg", /* "i2c_sel", */
			 imp_iic_wrap_c_ap_clock_cg_ro_cen_regs, 4, 0),
};

static void __iomem *iic_wrap_c_base;
static void mtk_imp_iic_wrap_c_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	iic_wrap_c_base = mtk_gate_common_init(node, "iic_c",
			imp_iic_wrap_c_clks, ARRAY_SIZE(imp_iic_wrap_c_clks),
			IMP_IIC_WRAP_C_NR_CLK);
	if (!iic_wrap_c_base)
		return;
#if MT_CCF_BRINGUP
	clk_writel(IIC_WRAP_C_CG_CLR, IIC_WRAP_C_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_imp_iic_wrap_c, "mediatek,imp_iic_wrap_c",
						mtk_imp_iic_wrap_c_init);

/******************* IIC East Subsys *******************************/
static struct mtk_gate_regs imp_iic_wrap_e_ap_clock_cg_ro_est_regs = {
	.set_ofs = 0xe08,
	.clr_ofs = 0xe04,
	.sta_ofs = 0xe00,
};
static struct mtk_gate imp_iic_wrap_e_clks[] __initdata = {
	/* AP_CLOCK_CG_RO_EST */
	GATE(IMP_IIC_WRAP_E_AP_I2C3_CG_RO, "imp_iic_wrap_e_ap_i2c3_cg_ro",
			"infracfg_ao_i2c0_cg", /* "i2c_sel", */
			 imp_iic_wrap_e_ap_clock_cg_ro_est_regs, 0, 0),
	GATE(IMP_IIC_WRAP_E_AP_I2C9_CG_RO, "imp_iic_wrap_e_ap_i2c9_cg_ro",
			"infracfg_ao_i2c0_cg", /* "i2c_sel", */
			 imp_iic_wrap_e_ap_clock_cg_ro_est_regs, 1, 0),
};

static void __iomem *iic_wrap_e_base;
static void mtk_imp_iic_wrap_e_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	iic_wrap_e_base = mtk_gate_common_init(node, "iic_e",
				imp_iic_wrap_e_clks,
				ARRAY_SIZE(imp_iic_wrap_e_clks),
				IMP_IIC_WRAP_E_NR_CLK);
	if (!iic_wrap_e_base)
		return;
#if MT_CCF_BRINGUP
	clk_writel(IIC_WRAP_E_CG_CLR, IIC_WRAP_E_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_imp_iic_wrap_e, "mediatek,imp_iic_wrap_e",
						mtk_imp_iic_wrap_e_init);

/******************* IIC North Subsys *******************************/
static struct mtk_gate_regs imp_iic_wrap_n_ap_clock_cg_ro_nor_regs = {
	.set_ofs = 0xe08,
	.clr_ofs = 0xe04,
	.sta_ofs = 0xe00,
};
static struct mtk_gate imp_iic_wrap_n_clks[] __initdata = {
	/* AP_CLOCK_CG_RO_NOR */
	GATE(IMP_IIC_WRAP_N_AP_I2C5_CG_RO, "imp_iic_wrap_n_ap_i2c5_cg_ro",
			"infracfg_ao_i2c0_cg", /* "i2c_sel", */
			 imp_iic_wrap_n_ap_clock_cg_ro_nor_regs, 0, 0),
	GATE(IMP_IIC_WRAP_N_AP_I2C6_CG_RO, "imp_iic_wrap_n_ap_i2c6_cg_ro",
			"infracfg_ao_i2c0_cg", /* "i2c_sel", */
			 imp_iic_wrap_n_ap_clock_cg_ro_nor_regs, 1, 0),
};

static void __iomem *iic_wrap_n_base;
static void mtk_imp_iic_wrap_n_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	iic_wrap_n_base = mtk_gate_common_init(node, "iic_n",
					imp_iic_wrap_n_clks,
					ARRAY_SIZE(imp_iic_wrap_n_clks),
					IMP_IIC_WRAP_N_NR_CLK);
	if (!iic_wrap_n_base)
		return;
#if MT_CCF_BRINGUP
	clk_writel(IIC_WRAP_N_CG_CLR, IIC_WRAP_N_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_imp_iic_wrap_n, "mediatek,imp_iic_wrap_n",
						mtk_imp_iic_wrap_n_init);
/******************* IIC South Subsys *******************************/
static struct mtk_gate_regs imp_iic_wrap_s_ap_clock_cg_ro_sou_regs = {
	.set_ofs = 0xe08,
	.clr_ofs = 0xe04,
	.sta_ofs = 0xe00,
};
static struct mtk_gate imp_iic_wrap_s_clks[] __initdata = {
	/* AP_CLOCK_CG_RO_SOU */
	GATE(IMP_IIC_WRAP_S_AP_I2C1_CG_RO, "imp_iic_wrap_s_ap_i2c1_cg_ro",
			"infracfg_ao_i2c0_cg", /* "i2c_sel", */
			 imp_iic_wrap_s_ap_clock_cg_ro_sou_regs, 0, 0),
	GATE(IMP_IIC_WRAP_S_AP_I2C2_CG_RO, "imp_iic_wrap_s_ap_i2c2_cg_ro",
			"infracfg_ao_i2c0_cg", /* "i2c_sel", */
			 imp_iic_wrap_s_ap_clock_cg_ro_sou_regs, 1, 0),
	GATE(IMP_IIC_WRAP_S_AP_I2C4_CG_RO, "imp_iic_wrap_s_ap_i2c4_cg_ro",
			"infracfg_ao_i2c0_cg", /* "i2c_sel", */
			 imp_iic_wrap_s_ap_clock_cg_ro_sou_regs, 2, 0),
	GATE(IMP_IIC_WRAP_S_AP_I2C7_CG_RO, "imp_iic_wrap_s_ap_i2c7_cg_ro",
			"infracfg_ao_i2c0_cg", /* "i2c_sel", */
			 imp_iic_wrap_s_ap_clock_cg_ro_sou_regs, 3, 0),
	GATE(IMP_IIC_WRAP_S_AP_I2C8_CG_RO, "imp_iic_wrap_s_ap_i2c8_cg_ro",
			"infracfg_ao_i2c0_cg", /* "i2c_sel", */
			 imp_iic_wrap_s_ap_clock_cg_ro_sou_regs, 4, 0),
};

static void __iomem *iic_wrap_s_base;
static void mtk_imp_iic_wrap_s_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	iic_wrap_s_base = mtk_gate_common_init(node, "iic_s",
					imp_iic_wrap_s_clks,
					ARRAY_SIZE(imp_iic_wrap_s_clks),
					IMP_IIC_WRAP_S_NR_CLK);
	if (!iic_wrap_s_base)
		return;
#if MT_CCF_BRINGUP
	clk_writel(IIC_WRAP_S_CG_CLR, IIC_WRAP_S_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_imp_iic_wrap_s, "mediatek,imp_iic_wrap_s",
						mtk_imp_iic_wrap_s_init);

/******************* Infra AO Subsys *******************************/
static const struct mtk_gate_regs infracfg_ao_module_sw_cg_0_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x84,
	.sta_ofs = 0x90,
};

static const struct mtk_gate_regs infracfg_ao_module_sw_cg_1_regs = {
	.set_ofs = 0x88,
	.clr_ofs = 0x8c,
	.sta_ofs = 0x94,
};

static const struct mtk_gate_regs infracfg_ao_module_sw_cg_2_regs = {
	.set_ofs = 0xa4,
	.clr_ofs = 0xa8,
	.sta_ofs = 0xac,
};

static const struct mtk_gate_regs infracfg_ao_module_sw_cg_3_regs = {
	.set_ofs = 0xc0,
	.clr_ofs = 0xc4,
	.sta_ofs = 0xc8,
};

static const struct mtk_gate_regs infracfg_ao_module_sw_cg_4_regs = {
	.set_ofs = 0xd0,
	.clr_ofs = 0xd4,
	.sta_ofs = 0xd8,
};

static const struct mtk_gate infra_clks[] __initconst = {
	/* MODULE_SW_CG_0 */
	GATE(INFRACFG_AO_PMIC_CG_TMR, "infracfg_ao_pmic_cg_tmr", "f26m_sel",
			 infracfg_ao_module_sw_cg_0_regs, 0, 0),
	GATE(INFRACFG_AO_PMIC_CG_AP, "infracfg_ao_pmic_cg_ap", "f26m_sel",
			 infracfg_ao_module_sw_cg_0_regs, 1, 0),
	GATE(INFRACFG_AO_PMIC_CG_MD, "infracfg_ao_pmic_cg_md", "f26m_sel",
			 infracfg_ao_module_sw_cg_0_regs, 2, 0),
	GATE(INFRACFG_AO_PMIC_CG_CONN, "infracfg_ao_pmic_cg_conn", "f26m_sel",
			 infracfg_ao_module_sw_cg_0_regs, 3, 0),
	GATE(INFRACFG_AO_SCPSYS_CG, "infracfg_ao_scpsys_cg", "clk_null",
			 infracfg_ao_module_sw_cg_0_regs, 4, 0),
	GATE(INFRACFG_AO_SEJ_CG, "infracfg_ao_sej_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_0_regs, 5, 0),
	GATE(INFRACFG_AO_APXGPT_CG, "infracfg_ao_apxgpt_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_0_regs, 6, 0),
	GATE(INFRACFG_AO_MCUPM_CG, "infracfg_ao_mcupm_cg", "mcupm_sel",
			 infracfg_ao_module_sw_cg_0_regs, 7, 0),
	GATE(INFRACFG_AO_GCE_CG, "infracfg_ao_gce_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_0_regs, 8, 0),
	GATE(INFRACFG_AO_GCE2_CG, "infracfg_ao_gce2_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_0_regs, 9, 0),
	GATE(INFRACFG_AO_THERM_CG, "infracfg_ao_therm_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_0_regs, 10, 0),
	GATE(INFRACFG_AO_I2C0_CG, "infracfg_ao_i2c0_cg", "i2c_sel",
			 infracfg_ao_module_sw_cg_0_regs, 11, 0),
	GATE(INFRACFG_AO_I2C1_CG, "infracfg_ao_i2c1_cg", "i2c_sel",
			 infracfg_ao_module_sw_cg_0_regs, 12, 0),
	GATE(INFRACFG_AO_I2C2_CG, "infracfg_ao_i2c2_cg", "i2c_sel",
			 infracfg_ao_module_sw_cg_0_regs, 13, 0),
	GATE(INFRACFG_AO_I2C3_CG, "infracfg_ao_i2c3_cg", "i2c_sel",
			 infracfg_ao_module_sw_cg_0_regs, 14, 0),
	GATE(INFRACFG_AO_PWM_HCLK_CG, "infracfg_ao_pwm_hclk_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_0_regs, 15, 0),
	GATE(INFRACFG_AO_PWM1_CG, "infracfg_ao_pwm1_cg", "pwm_sel",
			 infracfg_ao_module_sw_cg_0_regs, 16, 0),
	GATE(INFRACFG_AO_PWM2_CG, "infracfg_ao_pwm2_cg", "pwm_sel",
			 infracfg_ao_module_sw_cg_0_regs, 17, 0),
	GATE(INFRACFG_AO_PWM3_CG, "infracfg_ao_pwm3_cg", "pwm_sel",
			 infracfg_ao_module_sw_cg_0_regs, 18, 0),
	GATE(INFRACFG_AO_PWM4_CG, "infracfg_ao_pwm4_cg", "pwm_sel",
			 infracfg_ao_module_sw_cg_0_regs, 19, 0),
	GATE(INFRACFG_AO_PWM_CG, "infracfg_ao_pwm_cg", "pwm_sel",
			 infracfg_ao_module_sw_cg_0_regs, 21, 0),
	GATE(INFRACFG_AO_UART0_CG, "infracfg_ao_uart0_cg", "uart_sel",
			 infracfg_ao_module_sw_cg_0_regs, 22, 0),
	GATE(INFRACFG_AO_UART1_CG, "infracfg_ao_uart1_cg", "uart_sel",
			 infracfg_ao_module_sw_cg_0_regs, 23, 0),
	GATE(INFRACFG_AO_UART2_CG, "infracfg_ao_uart2_cg", "uart_sel",
			 infracfg_ao_module_sw_cg_0_regs, 24, 0),
	GATE(INFRACFG_AO_UART3_CG, "infracfg_ao_uart3_cg", "uart_sel",
			 infracfg_ao_module_sw_cg_0_regs, 25, 0),
	GATE(INFRACFG_AO_GCE_26M, "infracfg_ao_gce_26m", "axi_sel",
			 infracfg_ao_module_sw_cg_0_regs, 27, 0),
	GATE(INFRACFG_AO_CQ_DMA_FPC, "infracfg_ao_cq_dma_fpc", "axi_sel",
			 infracfg_ao_module_sw_cg_0_regs, 28, 0),
	GATE(INFRACFG_AO_BTIF_CG, "infracfg_ao_btif_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_0_regs, 31, 0),
	/* MODULE_SW_CG_1 */
	GATE(INFRACFG_AO_SPI0_CG, "infracfg_ao_spi0_cg", "spi_sel",
			 infracfg_ao_module_sw_cg_1_regs, 1, 0),
	GATE(INFRACFG_AO_MSDC0_CG, "infracfg_ao_msdc0_cg", "msdc50_0_hclk_sel",
			 infracfg_ao_module_sw_cg_1_regs, 2, 0),
	GATE(INFRACFG_AO_MSDC1_CG, "infracfg_ao_msdc1_cg", "msdc50_0_hclk_sel",
			 infracfg_ao_module_sw_cg_1_regs, 4, 0),
	GATE(INFRACFG_AO_MSDC0_SRC_CLK_CG, "infracfg_ao_msdc0_src_clk_cg",
			"msdc50_0_sel",
			 infracfg_ao_module_sw_cg_1_regs, 6, 0),
	GATE(INFRACFG_AO_DVFSRC_CG, "infracfg_ao_dvfsrc_cg", "f26m_sel",
			 infracfg_ao_module_sw_cg_1_regs, 7, 0),
	GATE(INFRACFG_AO_GCPU_CG, "infracfg_ao_gcpu_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_1_regs, 8, 0),
	GATE(INFRACFG_AO_TRNG_CG, "infracfg_ao_trng_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_1_regs, 9, 0),
	GATE(INFRACFG_AO_AUXADC_CG, "infracfg_ao_auxadc_cg",
			"f26m_sel",
			 infracfg_ao_module_sw_cg_1_regs, 10, 0),
	GATE(INFRACFG_AO_CPUM_CG, "infracfg_ao_cpum_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_1_regs, 11, 0),
	GATE(INFRACFG_AO_CCIF1_AP_CG, "infracfg_ao_ccif1_ap_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_1_regs, 12, 0),
	GATE(INFRACFG_AO_CCIF1_MD_CG, "infracfg_ao_ccif1_md_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_1_regs, 13, 0),
	GATE(INFRACFG_AO_AUXADC_MD_CG, "infracfg_ao_auxadc_md_cg",
			"f26m_sel",
			 infracfg_ao_module_sw_cg_1_regs, 14, 0),
	GATE(INFRACFG_AO_MSDC1_SRC_CLK_CG, "infracfg_ao_msdc1_src_clk_cg",
			"msdc30_1_sel",
			 infracfg_ao_module_sw_cg_1_regs, 16, 0),
	GATE(INFRACFG_AO_AP_DMA_PSEUDO_CG, "infracfg_ao_ap_dma_pseudo_cg",
			"axi_sel",	/* add manually */
			 infracfg_ao_module_sw_cg_1_regs, 18, 0),
	GATE(INFRACFG_AO_DEVICE_APC_CG, "infracfg_ao_device_apc_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_1_regs, 20, 0),
	GATE(INFRACFG_AO_CCIF_AP_CG, "infracfg_ao_ccif_ap_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_1_regs, 23, 0),
	GATE(INFRACFG_AO_DEBUGSYS_CG, "infracfg_ao_debugsys_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_1_regs, 24, 0),
	GATE(INFRACFG_AO_AUDIO_CG, "infracfg_ao_audio_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_1_regs, 25, 0),
	GATE(INFRACFG_AO_CCIF_MD_CG, "infracfg_ao_ccif_md_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_1_regs, 26, 0),
	GATE(INFRACFG_AO_DXCC_SEC_CORE_CG, "infracfg_ao_dxcc_sec_core_cg",
			"dxcc_sel",
			 infracfg_ao_module_sw_cg_1_regs, 27, 0),
	GATE(INFRACFG_AO_DXCC_AO_CG, "infracfg_ao_dxcc_ao_cg", "dxcc_sel",
			 infracfg_ao_module_sw_cg_1_regs, 28, 0),
	GATE(INFRACFG_AO_DBG_TRACE_CG, "infracfg_ao_dbg_trace_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_1_regs, 29, 0),
	GATE(INFRACFG_AO_DEVMPU_BCLK_CG, "infracfg_ao_devmpu_bclk_cg",
			"axi_sel",
			 infracfg_ao_module_sw_cg_1_regs, 30, 0),
	GATE(INFRACFG_AO_DRAMC_F26M_CG, "infracfg_ao_dramc_f26m_cg", "f26m_sel",
			 infracfg_ao_module_sw_cg_1_regs, 31, 0),
	/* MODULE_SW_CG_2 */
	GATE(INFRACFG_AO_IRTX_CG, "infracfg_ao_irtx_cg", "f26m_sel",
			 infracfg_ao_module_sw_cg_2_regs, 0, 0),
	GATE(INFRACFG_AO_SSUSB_CG, "infracfg_ao_ssusb_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_2_regs, 1, 0),
	GATE(INFRACFG_AO_DISP_PWM_CG, "infracfg_ao_disp_pwm_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_2_regs, 2, 0),
	GATE(INFRACFG_AO_CLDMA_BCLK_CK, "infracfg_ao_cldma_bclk_ck", "axi_sel",
			 infracfg_ao_module_sw_cg_2_regs, 3, 0),
	GATE(INFRACFG_AO_AUDIO_26M_BCLK_CK, "infracfg_ao_audio_26m_bclk_ck",
			"f26m_sel",
			 infracfg_ao_module_sw_cg_2_regs, 4, 0),
	GATE(INFRACFG_AO_MODEM_TEMP_SHARE_CG, "infracfg_ao_modem_temp_share_cg",
			"f26m_sel",
			 infracfg_ao_module_sw_cg_2_regs, 5, 0),
	GATE(INFRACFG_AO_SPI1_CG, "infracfg_ao_spi1_cg", "spi_sel",
			 infracfg_ao_module_sw_cg_2_regs, 6, 0),
	GATE(INFRACFG_AO_I2C4_CG, "infracfg_ao_i2c4_cg", "i2c_sel",
			 infracfg_ao_module_sw_cg_2_regs, 7, 0),
	GATE(INFRACFG_AO_SPI2_CG, "infracfg_ao_spi2_cg", "spi_sel",
			 infracfg_ao_module_sw_cg_2_regs, 9, 0),
	GATE(INFRACFG_AO_SPI3_CG, "infracfg_ao_spi3_cg", "spi_sel",
			 infracfg_ao_module_sw_cg_2_regs, 10, 0),
	GATE(INFRACFG_AO_UNIPRO_SYSCLK_CG, "infracfg_ao_unipro_sysclk_cg",
			"ufs_sel",
			 infracfg_ao_module_sw_cg_2_regs, 11, 0),
	GATE(INFRACFG_AO_UNIPRO_TICK_CG, "infracfg_ao_unipro_tick_cg",
			"f26m_sel",
			 infracfg_ao_module_sw_cg_2_regs, 12, 0),
	GATE(INFRACFG_AO_UFS_MP_SAP_BCLK_CG, "infracfg_ao_ufs_mp_sap_bclk_cg",
			"f26m_sel",
			 infracfg_ao_module_sw_cg_2_regs, 13, 0),
	GATE(INFRACFG_AO_MD32_BCLK_CG, "infracfg_ao_md32_bclk_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_2_regs, 14, 0),
	GATE(INFRACFG_AO_SSPM_CG, "infracfg_ao_sspm_cg", "sspm_sel",
			 infracfg_ao_module_sw_cg_2_regs, 15, 0),
	GATE(INFRACFG_AO_UNIPRO_MBIST_CG, "infracfg_ao_unipro_mbist_cg",
			"axi_sel",
			 infracfg_ao_module_sw_cg_2_regs, 16, 0),
	GATE(INFRACFG_AO_SSPM_BUS_HCLK_CG, "infracfg_ao_sspm_bus_hclk_cg",
			"axi_sel",
			 infracfg_ao_module_sw_cg_2_regs, 17, 0),
	GATE(INFRACFG_AO_I2C5_CG, "infracfg_ao_i2c5_cg", "i2c_sel",
			 infracfg_ao_module_sw_cg_2_regs, 18, 0),
	GATE(INFRACFG_AO_I2C5_ARBITER_CG, "infracfg_ao_i2c5_arbiter_cg",
			"i2c_sel",
			 infracfg_ao_module_sw_cg_2_regs, 19, 0),
	GATE(INFRACFG_AO_I2C5_IMM_CG, "infracfg_ao_i2c5_imm_cg", "i2c_sel",
			 infracfg_ao_module_sw_cg_2_regs, 20, 0),
	GATE(INFRACFG_AO_I2C1_ARBITER_CG, "infracfg_ao_i2c1_arbiter_cg",
			"i2c_sel",
			 infracfg_ao_module_sw_cg_2_regs, 21, 0),
	GATE(INFRACFG_AO_I2C1_IMM_CG, "infracfg_ao_i2c1_imm_cg", "i2c_sel",
			 infracfg_ao_module_sw_cg_2_regs, 22, 0),
	GATE(INFRACFG_AO_I2C2_ARBITER_CG, "infracfg_ao_i2c2_arbiter_cg",
			"i2c_sel",
			 infracfg_ao_module_sw_cg_2_regs, 23, 0),
	GATE(INFRACFG_AO_I2C2_IMM_CG, "infracfg_ao_i2c2_imm_cg", "i2c_sel",
			 infracfg_ao_module_sw_cg_2_regs, 24, 0),
	GATE(INFRACFG_AO_SPI4_CG, "infracfg_ao_spi4_cg", "spi_sel",
			 infracfg_ao_module_sw_cg_2_regs, 25, 0),
	GATE(INFRACFG_AO_SPI5_CG, "infracfg_ao_spi5_cg", "spi_sel",
			 infracfg_ao_module_sw_cg_2_regs, 26, 0),
	GATE(INFRACFG_AO_CQ_DMA_CG, "infracfg_ao_cq_dma_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_2_regs, 27, 0),
	GATE(INFRACFG_AO_UFS_CG, "infracfg_ao_ufs_cg", "ufs_sel",
			 infracfg_ao_module_sw_cg_2_regs, 28, 0),
	GATE(INFRACFG_AO_AES_UFSFDE_CG, "infracfg_ao_aes_cg", "aes_ufsfde_sel",
			 infracfg_ao_module_sw_cg_2_regs, 29, 0),
	GATE(INFRACFG_AO_UFS_TICK_CG, "infracfg_ao_ufs_tick_cg", "ufs_sel",
			 infracfg_ao_module_sw_cg_2_regs, 30, 0),
	GATE(INFRACFG_AO_SSUSB_XHCI_CG, "infracfg_ao_ssusb_xhci_cg",
			"ssusb_xhci_sel",
			 infracfg_ao_module_sw_cg_2_regs, 31, 0),
	/* MODULE_SW_CG_3 */
	GATE(INFRACFG_AO_MSDC0_SELF_CG, "infracfg_ao_msdc0_self_cg",
			"msdc50_0_sel",
			 infracfg_ao_module_sw_cg_3_regs, 0, 0),
	GATE(INFRACFG_AO_MSDC1_SELF_CG, "infracfg_ao_msdc1_self_cg",
			"msdc50_0_sel",
			 infracfg_ao_module_sw_cg_3_regs, 1, 0),
	GATE(INFRACFG_AO_MSDC2_SELF_CG, "infracfg_ao_msdc2_self_cg",
			"msdc50_0_sel",
			 infracfg_ao_module_sw_cg_3_regs, 2, 0),
	GATE(INFRACFG_AO_SSPM_26M_SELF_CG, "infracfg_ao_sspm_26m_self_cg",
			"f26m_sel",
			 infracfg_ao_module_sw_cg_3_regs, 3, 0),
	GATE(INFRACFG_AO_SSPM_32K_SELF_CG, "infracfg_ao_sspm_32k_self_cg",
			"clk32k",
			 infracfg_ao_module_sw_cg_3_regs, 4, 0),
	GATE(INFRACFG_AO_UFS_AXI_CG, "infracfg_ao_ufs_axi_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_3_regs, 5, 0),
	GATE(INFRACFG_AO_I2C6_CG, "infracfg_ao_i2c6_cg", "i2c_sel",
			 infracfg_ao_module_sw_cg_3_regs, 6, 0),
	GATE(INFRACFG_AO_AP_MSDC0_CG, "infracfg_ao_ap_msdc0_cg",
			"msdc50_0_sel",
			 infracfg_ao_module_sw_cg_3_regs, 7, 0),
	GATE(INFRACFG_AO_MD_MSDC0_CG, "infracfg_ao_md_msdc0_cg",
			"msdc50_0_sel",
			 infracfg_ao_module_sw_cg_3_regs, 8, 0),
	GATE(INFRACFG_AO_CCIF5_AP_CG, "infracfg_ao_ccif5_ap_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_3_regs, 9, 0),
	GATE(INFRACFG_AO_CCIF5_MD_CG, "infracfg_ao_ccif5_md_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_3_regs, 10, 0),
	GATE(INFRACFG_AO_CCIF2_AP_CG, "infracfg_ao_ccif2_ap_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_3_regs, 16, 0),
	GATE(INFRACFG_AO_CCIF2_MD_CG, "infracfg_ao_ccif2_md_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_3_regs, 17, 0),
	GATE(INFRACFG_AO_CCIF3_AP_CG, "infracfg_ao_ccif3_ap_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_3_regs, 18, 0),
	GATE(INFRACFG_AO_CCIF3_MD_CG, "infracfg_ao_ccif3_md_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_3_regs, 19, 0),
	GATE(INFRACFG_AO_SEJ_F13M_CG, "infracfg_ao_sej_f13m_cg", "f26m_sel",
			 infracfg_ao_module_sw_cg_3_regs, 20, 0),
	GATE(INFRACFG_AO_AES_CG, "infracfg_ao_aes_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_3_regs, 21, 0),
	GATE(INFRACFG_AO_I2C7_CG, "infracfg_ao_i2c7_cg", "i2c_sel",
			 infracfg_ao_module_sw_cg_3_regs, 22, 0),
	GATE(INFRACFG_AO_I2C8_CG, "infracfg_ao_i2c8_cg", "i2c_sel",
			 infracfg_ao_module_sw_cg_3_regs, 23, 0),
	GATE(INFRACFG_AO_FBIST2FPC_CG, "infracfg_ao_fbist2fpc_cg",
			"msdc50_0_sel",
			 infracfg_ao_module_sw_cg_3_regs, 24, 0),
	GATE(INFRACFG_AO_DEVICE_APC_SYNC_CG, "infracfg_ao_device_apc_sync_cg",
			"axi_sel",
			 infracfg_ao_module_sw_cg_3_regs, 25, 0),
	GATE(INFRACFG_AO_DPMAIF_MAIN_CG, "infracfg_ao_dpmaif_main_cg",
			"dpmaif_main_sel",
			infracfg_ao_module_sw_cg_3_regs, 26, 0),
	GATE(INFRACFG_AO_CCIF4_AP_CG, "infracfg_ao_ccif4_ap_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_3_regs, 28, 0),
	GATE(INFRACFG_AO_CCIF4_MD_CG, "infracfg_ao_ccif4_md_cg", "axi_sel",
			 infracfg_ao_module_sw_cg_3_regs, 29, 0),
	GATE(INFRACFG_AO_SPI6_CK_CG, "infracfg_ao_spi6_ck_cg", "spi_sel",
			 infracfg_ao_module_sw_cg_3_regs, 30, 0),
	GATE(INFRACFG_AO_SPI7_CK_CG, "infracfg_ao_spi7_ck_cg", "spi_sel",
			 infracfg_ao_module_sw_cg_3_regs, 31, 0),

	/* MODULE_SW_CG_4 */
/*	GATE(INFRACFG_AO_AP_DMA_CG, "infracfg_ao_ap_dma_cg", "axi_sel",
 *			infracfg_ao_module_sw_cg_4_regs, 31, 0),
 */
	/*
	 * MT6885: pll -> mux -> iic_cg(pseudo) -> iic_cg
	 * This is a workaround for MT6885.
	 *
	 * IIC_CG(PSEUDO) : the signal bit for SPM
	 * IIC_CG: the signal bit that real clock gating.
	 */
	GATE(INFRACFG_AO_AP_DMA_CG, "infracfg_ao_ap_dma_cg",
			"infracfg_ao_ap_dma_pseudo_cg",
			infracfg_ao_module_sw_cg_4_regs, 31, 0),
};

static void __iomem *infracfg_base;
static void mtk_infracfg_ao_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	infracfg_base = mtk_gate_common_init(node, "infracfg_ao", infra_clks,
				ARRAY_SIZE(infra_clks), INFRACFG_AO_NR_CLK);
	if (!infracfg_base)
		return;
#if MT_CCF_BRINGUP
	pr_notice("%s(): init done\n", __func__);
	/* do nothing */
#else
	clk_writel(INFRA_PDN_SET0, INFRA_CG0);
	clk_writel(INFRA_PDN_SET1, INFRA_CG1);
	clk_writel(INFRA_PDN_SET2, INFRA_CG2);
	clk_writel(INFRA_PDN_SET3, INFRA_CG3);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_infracfg_ao, "mediatek,infracfg_ao",
						mtk_infracfg_ao_init);

/******************* IPE Subsys *******************************/
static struct mtk_gate_regs ipesys_img_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};
static struct mtk_gate ipesys_clks[] __initdata = {
	/* IMG_CG */
	GATE_DUMMY(IPESYS_LARB19_CGPDN, "ipesys_larb19_cgpdn", "ipe_sel",
			 ipesys_img_cg_regs, 0, 0),
	GATE_DUMMY(IPESYS_LARB20_CGPDN, "ipesys_larb20_cgpdn", "ipe_sel",
			 ipesys_img_cg_regs, 1, 0),
	GATE_DUMMY(IPESYS_IPE_SMI_SUBCOM_CGPDN, "ipesys_ipe_smi_subcom_cgpdn",
			"ipe_sel", ipesys_img_cg_regs, 2, 0),
	GATE(IPESYS_FD_CGPDN, "ipesys_fd_cgpdn", "ipe_sel",
			 ipesys_img_cg_regs, 3, 0),
	GATE(IPESYS_FE_CGPDN, "ipesys_fe_cgpdn", "ipe_sel",
			 ipesys_img_cg_regs, 4, 0),
	GATE(IPESYS_RSC_CGPDN, "ipesys_rsc_cgpdn", "ipe_sel",
			 ipesys_img_cg_regs, 5, 0),
	GATE(IPESYS_DPE_CGPDN, "ipesys_dpe_cgpdn", "ipe_sel",
			 ipesys_img_cg_regs, 6, 0),

};

static void __iomem *ipe_base;
static void mtk_ipesys_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	ipe_base = mtk_gate_common_init(node, "ipe", ipesys_clks,
				ARRAY_SIZE(ipesys_clks), IPESYS_NR_CLK);
	if (!ipe_base)
		return;
#if MT_CCF_BRINGUP
	clk_writel(IPE_CG_CLR, IPE_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_ipesys, "mediatek,ipesys_config", mtk_ipesys_init);

/******************* MDP GCE Subsys *******************************/

static struct mtk_gate_regs mdp_gce_gce_ctl_int0_regs = {
	.set_ofs = 0xf0,
	.clr_ofs = 0xf0,
	.sta_ofs = 0xf0,
};
static struct mtk_gate mdp_gce_clks[] __initdata = {
	/* GCE_CTL_INT0 */
	GATE_STA(MDP_GCE_SW_CG_0, "mdp_gce_sw_cg_0", "axi_sel",
			 mdp_gce_gce_ctl_int0_regs, 16, 0),
};

static void __iomem *mdp_gce_base;
static void mtk_mdp_gcesys_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	mdp_gce_base = mtk_gate_common_init(node, "mdp_gce", mdp_gce_clks,
				ARRAY_SIZE(mdp_gce_clks), MDP_GCE_NR_CLK);
	if (!mdp_gce_base)
		return;
#if MT_CCF_BRINGUP
	clk_writel(MDP_GCE_CG_CON, clk_readl(MDP_GCE_CG_CON) & ~(MDP_GCE_CG));
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_mdp_gcesys, "mediatek,mdp_gce_clock",
							mtk_mdp_gcesys_init);

/******************* MFG Subsys *******************************/
static struct mtk_gate_regs mfgcfg_mfg_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};
static struct mtk_gate mfgcfg_clks[] __initdata = {
	/* MFG_CG */
	GATE(MFGCFG_BG3D, "mfgcfg_bg3d", "mfg_sel",
			 mfgcfg_mfg_cg_regs, 0, 0),
};


static void __iomem *mfgcfg_base;
static void mtk_mfg_cfg_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	mfgcfg_base = mtk_gate_common_init(node, "mfg", mfgcfg_clks,
				ARRAY_SIZE(mfgcfg_clks), MFGCFG_NR_CLK);
	if (!mfgcfg_base)
		return;
#if MT_CCF_BRINGUP
	clk_writel(MFG_CG_CLR, MFG_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_mfg_cfg, "mediatek,g3d_config", mtk_mfg_cfg_init);

/******************* MM Subsys *******************************/
static struct mtk_gate_regs mmsys_config_mmsys_cg_con0_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};
static struct mtk_gate_regs mmsys_config_mmsys_cg_con1_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};
static struct mtk_gate_regs mmsys_config_mmsys_cg_con2_regs = {
	.set_ofs = 0x1a4,
	.clr_ofs = 0x1a8,
	.sta_ofs = 0x1a0,
};
static struct mtk_gate mmsys_config_clks[] __initdata = {
	/* MMSYS_CG_CON0 */
	GATE(MM_DISP_RSZ0, "MM_DISP_RSZ0", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 0, 0),
	GATE(MM_DISP_RSZ1, "MM_DISP_RSZ1", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 1, 0),
	GATE(MM_DISP_OVL0, "MM_DISP_OVL0", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 2, 0),
	GATE(MM_INLINEROT, "MM_INLINEROT", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 3, 0),
	GATE(MM_MDP_TDSHP4, "MM_MDP_TDSHP4", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 4, 0),
	GATE(MM_MDP_TDSHP5, "MM_MDP_TDSHP5", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 5, 0),
	GATE(MM_MDP_AAL4, "MM_MDP_AAL4", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 6, 0),
	GATE(MM_MDP_AAL5, "MM_MDP_AAL5", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 7, 0),
	GATE(MM_MDP_HDR4, "MM_MDP_HDR4", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 8, 0),
	GATE(MM_MDP_HDR5, "MM_MDP_HDR5", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 9, 0),
	GATE(MM_MDP_RSZ4, "MM_MDP_RSZ4", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 10, 0),
	GATE(MM_MDP_RSZ5, "MM_MDP_RSZ5", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 11, 0),
	GATE(MM_MDP_RDMA4, "MM_MDP_RDMA4", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 12, 0),
	GATE(MM_MDP_RDMA5, "MM_MDP_RDMA5", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 13, 0),
	GATE(MM_DISP_FAKE_ENG0, "MM_DISP_FAKE_ENG0", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 14, 0),
	GATE(MM_DISP_FAKE_ENG1, "MM_DISP_FAKE_ENG1", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 15, 0),
	GATE(MM_DISP_OVL0_2L, "MM_DISP_OVL0_2L", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 16, 0),
	GATE(MM_DISP_OVL1_2L, "MM_DISP_OVL1_2L", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 17, 0),
	GATE(MM_DISP_OVL2_2L, "MM_DISP_OVL2_2L", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 18, 0),
	GATE(MM_DISP_MUTEX0, "MM_DISP_MUTEX0", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 19, 0),
	GATE(MM_DISP_OVL1, "MM_DISP_OVL1", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 20, 0),
	GATE(MM_DISP_OVL3_2L, "MM_DISP_OVL3_2L", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 21, 0),
	GATE(MM_DISP_CCORR0, "MM_DISP_CCORR0", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 22, 0),
	GATE(MM_DISP_CCORR1, "MM_DISP_CCORR1", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 23, 0),
	GATE(MM_DISP_COLOR0, "MM_DISP_COLOR0", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 24, 0),
	GATE(MM_DISP_COLOR1, "MM_DISP_COLOR1", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 25, 0),
	GATE(MM_DISP_POSTMASK0, "MM_DISP_POSTMASK0", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 26, 0),
	GATE(MM_DISP_POSTMASK1, "MM_DISP_POSTMASK1", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 27, 0),
	GATE(MM_DISP_DITHER0, "MM_DISP_DITHER0", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 28, 0),
	GATE(MM_DISP_DITHER1, "MM_DISP_DITHER1", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 29, 0),
	GATE(MM_DISP_DSI0, "MM_DISP_DSI0", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 30, 0),
	GATE(MM_DISP_DSI1, "MM_DISP_DSI1", "disp_sel",
			 mmsys_config_mmsys_cg_con0_regs, 31, 0),
	/* MMSYS_CG_CON1 */
	GATE(MM_DISP_GAMMA0, "MM_DISP_GAMMA0", "disp_sel",
			 mmsys_config_mmsys_cg_con1_regs, 0, 0),
	GATE(MM_DISP_GAMMA1, "MM_DISP_GAMMA1", "disp_sel",
			 mmsys_config_mmsys_cg_con1_regs, 1, 0),
	GATE(MM_DISP_AAL0, "MM_DISP_AAL0", "disp_sel",
			 mmsys_config_mmsys_cg_con1_regs, 2, 0),
	GATE(MM_DISP_AAL1, "MM_DISP_AAL1", "disp_sel",
			 mmsys_config_mmsys_cg_con1_regs, 3, 0),
	GATE(MM_DISP_WDMA0, "MM_DISP_WDMA0", "disp_sel",
			 mmsys_config_mmsys_cg_con1_regs, 4, 0),
	GATE(MM_DISP_WDMA1, "MM_DISP_WDMA1", "disp_sel",
			 mmsys_config_mmsys_cg_con1_regs, 5, 0),
	GATE(MM_DISP_RDMA0, "MM_DISP_RDMA0", "disp_sel",
			 mmsys_config_mmsys_cg_con1_regs, 8, 0),
	GATE(MM_DISP_RDMA1, "MM_DISP_RDMA1", "disp_sel",
			 mmsys_config_mmsys_cg_con1_regs, 9, 0),
	GATE(MM_DISP_RDMA4, "MM_DISP_RDMA4", "disp_sel",
			 mmsys_config_mmsys_cg_con1_regs, 10, 0),
	GATE(MM_DISP_RDMA5, "MM_DISP_RDMA5", "disp_sel",
			 mmsys_config_mmsys_cg_con1_regs, 11, 0),
	GATE(MM_DISP_DSC_WRAP, "MM_DISP_DSC_WRAP", "disp_sel",
			 mmsys_config_mmsys_cg_con1_regs, 12, 0),
	GATE(MM_DISP_DP_INTF, "MM_DISP_DP_INTF", "disp_sel",
			 mmsys_config_mmsys_cg_con1_regs, 13, 0),
	GATE(MM_DISP_MERGE0, "MM_DISP_MERGE0", "disp_sel",
			 mmsys_config_mmsys_cg_con1_regs, 14, 0),
	GATE(MM_DISP_MERGE1, "MM_DISP_MERGE1", "disp_sel",
			 mmsys_config_mmsys_cg_con1_regs, 15, 0),
	GATE_DUMMY(MM_SMI_COMMON, "MM_SMI_COMMON", "disp_sel",
			 mmsys_config_mmsys_cg_con1_regs, 19, 0),
	GATE_DUMMY(MM_SMI_GALS, "MM_SMI_GALS", "disp_sel",
			 mmsys_config_mmsys_cg_con1_regs, 23, 0),
	GATE_DUMMY(MM_SMI_INFRA, "MM_SMI_INFRA", "disp_sel",
			 mmsys_config_mmsys_cg_con1_regs, 27, 0),
	GATE_DUMMY(MM_SMI_IOMMU, "MM_SMI_IOMMU", "disp_sel",
			 mmsys_config_mmsys_cg_con1_regs, 31, 0),
	/* MMSYS_CG_CON2 */
	GATE(MM_DSI_DSI0, "MM_DSI_DSI0", "disp_sel",
			 mmsys_config_mmsys_cg_con2_regs, 0, 0),
	GATE(MM_DSI_DSI1, "MM_DSI_DSI1", "disp_sel",
			 mmsys_config_mmsys_cg_con2_regs, 8, 0),

	GATE(MM_DP_INTF, "MM_DP_INTF", "dp_sel",
			 mmsys_config_mmsys_cg_con2_regs, 16, 0),
	GATE(MM_26MHZ, "MM_26MHZ", "f26m_sel",
			 mmsys_config_mmsys_cg_con2_regs, 24, 0),
	GATE(MM_32KHZ, "MM_32KHZ", "clk32k",
			 mmsys_config_mmsys_cg_con2_regs, 25, 0),
};


static void __iomem *mmsys_config_base;
static void mtk_mmsys_config_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	mmsys_config_base = mtk_gate_common_init(node, "mm", mmsys_config_clks,
				ARRAY_SIZE(mmsys_config_clks), MM_NR_CLK);
	if (!mmsys_config_base)
		return;
#if MT_CCF_BRINGUP
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_mmsys_config, "mediatek,dispsys_config",
							mtk_mmsys_config_init);


/******************* MSDC Subsys *******************************/
static struct mtk_gate_regs msdc0_patch_bit1_regs = {
	.set_ofs = 0xb4,
	.clr_ofs = 0xb4,
	.sta_ofs = 0xb4,
};
static struct mtk_gate msdc0_clks[] __initdata = {
	/* PATCH_BIT1 */
	GATE_STA_INV(MSDC0_AXI_WRAP_CKEN, "msdc0_axi_wrap_cken", "axi_sel",
			 msdc0_patch_bit1_regs, 22, 1),
};

static void __iomem *msdc0_base;
static void mtk_msdcsys_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	msdc0_base = mtk_gate_common_init(node, "msdc", msdc0_clks,
				ARRAY_SIZE(msdc0_clks), MSDC0_NR_CLK);
	if (!msdc0_base)
		return;
#if MT_CCF_BRINGUP
	clk_writel(MSDC0_CG_CON0, clk_readl(MSDC0_CG_CON0) | MSDC0_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_msdcsys, "mediatek,msdc", mtk_msdcsys_init);

/******************* PERICFG Subsys *******************************/
static struct mtk_gate_regs pericfg_periaxi_si0_ctl_regs = {
	.set_ofs = 0x20c,
	.clr_ofs = 0x20c,
	.sta_ofs = 0x20c,
};
static struct mtk_gate pericfg_clks[] __initdata = {
	/* PERIAXI_SI0_CTL */
	GATE_STA_INV(PERICFG_PERIAXI_CG_DISABLE, "pericfg_periaxi_cg_disable",
			"axi_sel", pericfg_periaxi_si0_ctl_regs, 31, 1),
};

static void __iomem *pericfg_base;
static void mtk_perisys_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);
	pericfg_base = mtk_gate_common_init(node, "pericfg", pericfg_clks,
				ARRAY_SIZE(pericfg_clks), PERICFG_NR_CLK);
	if (!pericfg_base)
		return;
#if MT_CCF_BRINGUP
	clk_writel(PERICFG_CG_CON0, clk_readl(PERICFG_CG_CON0) | PERICFG_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_perisys, "mediatek,pericfg", mtk_perisys_init);

/******************* TOPCKGEN Subsys *******************************/
static void __iomem *cksys_base;
static void __init mtk_topckgen_init(struct device_node *node)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	pr_notice("%s(): init begin\n", __func__);
	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return;
	}

	clk_data = mtk_alloc_clk_data(TOP_NR_CLK);
	if (!clk_data) {
		pr_notice("%s(): allocate memory failed\n", __func__);
		return;
	}

	mtk_clk_register_fixed_clks(fixed_clks, ARRAY_SIZE(fixed_clks),
						clk_data);
	mtk_clk_register_factors(top_divs, ARRAY_SIZE(top_divs), clk_data);
	mtk_clk_register_muxes(top_muxes, ARRAY_SIZE(top_muxes), node,
						&mt6885_clk_lock, clk_data);

	mtk_clk_register_composites(top_audmuxes, ARRAY_SIZE(top_audmuxes),
					base, &mt6885_clk_lock, clk_data);
	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
	cksys_base = base;

	/* For MT6885 SPM request, has been initialized in preloader already.*/
	clk_writel(CLK_SCP_CFG_0, clk_readl(CLK_SCP_CFG_0) | 0x3FF);
	clk_writel(CLK_SCP_CFG_1,
			(clk_readl(CLK_SCP_CFG_1) & 0xFFFFEFF3) | 0x3);

#if 1
	clk_writel(cksys_base + CLK_CFG_0_CLR, 0x00000000);
	clk_writel(cksys_base + CLK_CFG_0_SET, 0x00000000);

	clk_writel(cksys_base + CLK_CFG_1_CLR, 0x00000000);
	clk_writel(cksys_base + CLK_CFG_1_SET, 0x00000000);

	clk_writel(cksys_base + CLK_CFG_2_CLR, 0x80008000);
	clk_writel(cksys_base + CLK_CFG_2_SET, 0x80008000);

	clk_writel(cksys_base + CLK_CFG_3_CLR, 0x80808080);
	clk_writel(cksys_base + CLK_CFG_3_SET, 0x80808080);

	clk_writel(cksys_base + CLK_CFG_4_CLR, 0x80808080);
	clk_writel(cksys_base + CLK_CFG_4_SET, 0x80808080);

	clk_writel(cksys_base + CLK_CFG_5_CLR, 0x80808080);
	clk_writel(cksys_base + CLK_CFG_5_SET, 0x80808080);

	clk_writel(cksys_base + CLK_CFG_6_CLR, 0x80808080);
	clk_writel(cksys_base + CLK_CFG_6_SET, 0x80808080);

	clk_writel(cksys_base + CLK_CFG_7_CLR, 0x80808080);
	clk_writel(cksys_base + CLK_CFG_7_SET, 0x80808080);

	clk_writel(cksys_base + CLK_CFG_8_CLR, 0x00800080);
	clk_writel(cksys_base + CLK_CFG_8_SET, 0x00000080);

	clk_writel(cksys_base + CLK_CFG_9_CLR, 0x00808080);
	clk_writel(cksys_base + CLK_CFG_9_SET, 0x00008080);

	clk_writel(cksys_base + CLK_CFG_10_CLR, 0x80808080);
	clk_writel(cksys_base + CLK_CFG_10_SET, 0x80808080);

	clk_writel(cksys_base + CLK_CFG_11_CLR, 0x80008080);/*dxcc*/
	clk_writel(cksys_base + CLK_CFG_11_SET, 0x80008080);

	clk_writel(cksys_base + CLK_CFG_12_CLR, 0x80808080);
	clk_writel(cksys_base + CLK_CFG_12_SET, 0x80808080);

	clk_writel(cksys_base + CLK_CFG_13_CLR, 0x00808080);
	clk_writel(cksys_base + CLK_CFG_13_SET, 0x00808080);

	clk_writel(cksys_base + CLK_CFG_14_CLR, 0x00808000);
	clk_writel(cksys_base + CLK_CFG_14_SET, 0x00808000);

	clk_writel(cksys_base + CLK_CFG_15_CLR, 0x00808080);/*mcupm*/
	clk_writel(cksys_base + CLK_CFG_15_SET, 0x00808080);

	clk_writel(cksys_base + CLK_CFG_16_CLR, 0x00000000);
	clk_writel(cksys_base + CLK_CFG_16_SET, 0x00000000);

#endif
	pr_notice("%s(): init done\n", __func__);
}
CLK_OF_DECLARE_DRIVER(mtk_topckgen, "mediatek,topckgen", mtk_topckgen_init);


/******************* VDEC Subsys *******************************/
static struct mtk_gate_regs vdec_gcon_larb_cken_con_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xc,
	.sta_ofs = 0x8,
};
static struct mtk_gate_regs vdec_gcon_lat_cken_regs = {
	.set_ofs = 0x200,
	.clr_ofs = 0x204,
	.sta_ofs = 0x200,
};
static struct mtk_gate_regs vdec_gcon_vdec_cken_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};
static struct mtk_gate vdec_gcon_clks[] __initdata = {
	/* LARB_CKEN_CON */
	GATE_INV_DUMMY(VDEC_GCON_LARB1_CKEN, "vdec_gcon_larb1_cken",
			"vdec_sel", vdec_gcon_larb_cken_con_regs, 0, 1),
	/* LAT_CKEN */
	GATE_INV_DUMMY(VDEC_GCON_LAT_CKEN, "vdec_gcon_lat_cken", "vdec_sel",
			 vdec_gcon_lat_cken_regs, 0, 1),
	GATE_INV(VDEC_GCON_LAT_ACTIVE, "vdec_gcon_lat_active", "vdec_sel",
			 vdec_gcon_lat_cken_regs, 4, 1),
	GATE_INV(VDEC_GCON_LAT_CKEN_ENG, "vdec_gcon_lat_cken_eng", "vdec_sel",
			 vdec_gcon_lat_cken_regs, 8, 1),
	/* VDEC_CKEN */
	GATE_INV_DUMMY(VDEC_GCON_VDEC_CKEN, "vdec_gcon_vdec_cken", "vdec_sel",
			 vdec_gcon_vdec_cken_regs, 0, 1),
	GATE_INV(VDEC_GCON_VDEC_ACTIVE, "vdec_gcon_vdec_active", "vdec_sel",
			 vdec_gcon_vdec_cken_regs, 4, 1),
	GATE_INV(VDEC_GCON_VDEC_CKEN_ENG, "vdec_gcon_vdec_cken_eng",
			"vdec_sel",
			 vdec_gcon_vdec_cken_regs, 8, 1),
};

static void __iomem *vdec_gcon_base;
static void mtk_vdec_top_global_con_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);

	vdec_gcon_base = mtk_gate_common_init(node, "vdec", vdec_gcon_clks,
					ARRAY_SIZE(vdec_gcon_clks),
					VDEC_GCON_NR_CLK);
	if (!vdec_gcon_base)
		return;
#if MT_CCF_BRINGUP
	clk_writel(VDEC_CKEN_SET, VDEC_CKEN_CG);
	clk_writel(VDEC_LARB1_CKEN_SET, VDEC_LARB1_CKEN_CG);
	clk_writel(VDEC_LAT_CKEN_SET, VDEC_LAT_CKEN_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_vdec_top_global_con, "mediatek,vdec_gcon",
						mtk_vdec_top_global_con_init);
/******************* VDEC SOC Subsys *******************************/
static struct mtk_gate_regs vdec_soc_gcon_larb_cken_con_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xc,
	.sta_ofs = 0x8,
};
static struct mtk_gate_regs vdec_soc_gcon_lat_cken_regs = {
	.set_ofs = 0x200,
	.clr_ofs = 0x204,
	.sta_ofs = 0x200,
};
static struct mtk_gate_regs vdec_soc_gcon_vdec_cken_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};
static struct mtk_gate vdec_soc_gcon_clks[] __initdata = {
	/* LARB_CKEN_CON */
	GATE_INV_DUMMY(VDEC_SOC_GCON_LARB1_CKEN, "vdec_soc_gcon_larb1_cken",
			"vdec_sel",
			 vdec_soc_gcon_larb_cken_con_regs, 0, 1),
	/* LAT_CKEN */
	GATE_INV_DUMMY(VDEC_SOC_GCON_LAT_CKEN, "vdec_soc_gcon_lat_cken",
			"vdec_sel",
			 vdec_soc_gcon_lat_cken_regs, 0, 1),
	GATE_INV(VDEC_SOC_GCON_LAT_ACTIVE, "vdec_soc_gcon_lat_active",
			"vdec_sel",
			 vdec_soc_gcon_lat_cken_regs, 4, 1),
	GATE_INV(VDEC_SOC_GCON_LAT_CKEN_ENG, "vdec_soc_gcon_lat_cken_eng",
			"vdec_sel",
			 vdec_soc_gcon_lat_cken_regs, 8, 1),
	/* VDEC_CKEN */
	GATE_INV_DUMMY(VDEC_SOC_GCON_VDEC_CKEN, "vdec_soc_gcon_vdec_cken",
			"vdec_sel",
			 vdec_soc_gcon_vdec_cken_regs, 0, 1),
	GATE_INV(VDEC_SOC_GCON_VDEC_ACTIVE, "vdec_soc_gcon_vdec_active",
			"vdec_sel",
			 vdec_soc_gcon_vdec_cken_regs, 4, 1),
	GATE_INV(VDEC_SOC_GCON_VDEC_CKEN_ENG, "vdec_soc_gcon_vdec_cken_eng",
			"vdec_sel",
			 vdec_soc_gcon_vdec_cken_regs, 8, 1),
};

static void __iomem *vdec_soc_gcon_base;
static void mtk_vdec_soc_global_con_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);

	vdec_soc_gcon_base = mtk_gate_common_init(node, "vdec_soc",
				vdec_soc_gcon_clks,
				ARRAY_SIZE(vdec_soc_gcon_clks),
				VDEC_SOC_GCON_NR_CLK);
	if (!vdec_soc_gcon_base)
		return;
#if MT_CCF_BRINGUP
	clk_writel(VDEC_SOC_CKEN_SET, VDEC_SOC_CKEN_CG);
	clk_writel(VDEC_SOC_LARB1_CKEN_SET, VDEC_SOC_LARB1_CKEN_CG);
	clk_writel(VDEC_SOC_LAT_CKEN_SET, VDEC_SOC_LAT_CKEN_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_vdec_soc_global_con, "mediatek,vdec_soc_gcon",
						mtk_vdec_soc_global_con_init);

/******************* VENC C1 Subsys *******************************/
static struct mtk_gate_regs venc_c1_gcon_vencsys_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};
static struct mtk_gate venc_c1_gcon_clks[] __initdata = {
	/* VENCSYS_CG */
	GATE_INV(VENC_C1_GCON_SET0_LARB, "venc_c1_gcon_set0_larb", "venc_sel",
			 venc_c1_gcon_vencsys_cg_regs, 0, 1),
	GATE_INV_DUMMY(VENC_C1_GCON_SET1_VENC, "venc_c1_gcon_set1_venc",
			"venc_sel", venc_c1_gcon_vencsys_cg_regs, 4, 1),
	GATE_INV(VENC_C1_GCON_SET2_JPGENC, "venc_c1_gcon_set2_jpgenc",
			"venc_sel",
			 venc_c1_gcon_vencsys_cg_regs, 8, 1),
	GATE_INV(VENC_C1_GCON_SET3_JPGDEC, "venc_c1_gcon_set3_jpgdec",
			"venc_sel",
			 venc_c1_gcon_vencsys_cg_regs, 12, 1),
	GATE_INV(VENC_C1_GCON_SET4_JPGDEC_C1, "venc_c1_gcon_set4_jpgdec_c1",
			"venc_sel", venc_c1_gcon_vencsys_cg_regs, 16, 1),
	GATE_INV(VENC_C1_GCON_SET5_GALS, "venc_c1_gcon_set5_gals", "venc_sel",
			 venc_c1_gcon_vencsys_cg_regs, 28, 1),
};

static void __iomem *venc_c1_gcon_base;
static void mtk_venc_c1_global_con_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);

	venc_c1_gcon_base = mtk_gate_common_init(node, "venc_c1",
					venc_c1_gcon_clks,
					ARRAY_SIZE(venc_c1_gcon_clks),
					VENC_C1_GCON_NR_CLK);
	if (!venc_c1_gcon_base)
		return;
#if MT_CCF_BRINGUP
	clk_writel(VENC_C1_CG_SET, VENC_C1_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_venc_c1_global_con, "mediatek,venc_c1_gcon",
						mtk_venc_c1_global_con_init);

/******************* VENC Subsys *******************************/
static struct mtk_gate_regs venc_gcon_vencsys_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};
static struct mtk_gate venc_gcon_clks[] __initdata = {
	/* VENCSYS_CG */
	GATE_INV(VENC_GCON_SET0_LARB, "venc_gcon_set0_larb", "venc_sel",
			 venc_gcon_vencsys_cg_regs, 0, 1),
	GATE_INV_DUMMY(VENC_GCON_SET1_VENC, "venc_gcon_set1_venc", "venc_sel",
			 venc_gcon_vencsys_cg_regs, 4, 1),
	GATE_INV(VENC_GCON_SET2_JPGENC, "venc_gcon_set2_jpgenc", "venc_sel",
			 venc_gcon_vencsys_cg_regs, 8, 1),
	GATE_INV(VENC_GCON_SET3_JPGDEC, "venc_gcon_set3_jpgdec", "venc_sel",
			 venc_gcon_vencsys_cg_regs, 12, 1),
	GATE_INV(VENC_GCON_SET4_JPGDEC_C1, "venc_gcon_set4_jpgdec_c1",
			"venc_sel", venc_gcon_vencsys_cg_regs, 16, 1),
	GATE_INV(VENC_GCON_SET5_GALS, "venc_gcon_set5_gals", "venc_sel",
			 venc_gcon_vencsys_cg_regs, 28, 1),
};

static void __iomem *venc_gcon_base;
static void mtk_venc_global_con_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);

	venc_gcon_base = mtk_gate_common_init(node, "venc", venc_gcon_clks,
				ARRAY_SIZE(venc_gcon_clks), VENC_GCON_NR_CLK);
	if (!venc_gcon_base)
		return;
#if MT_CCF_BRINGUP
	clk_writel(VENC_CG_SET, VENC_CG);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_venc_global_con, "mediatek,venc_gcon",
						mtk_venc_global_con_init);
/******************* MDP Subsys *******************************/
static struct mtk_gate_regs mdp_cg0 = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};
static struct mtk_gate_regs mdp_cg1 = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};
static struct mtk_gate_regs mdp_cg2 = {
	.set_ofs = 0x124,
	.clr_ofs = 0x128,
	.sta_ofs = 0x120,
};
static struct mtk_gate mdpsys_config_clks[] __initdata = {
	/* MDPSYS_CG_CON0 */
	GATE(MDP_MDP_RDMA0, "MDP_MDP_RDMA0", "mdp_sel", mdp_cg0, 0, 0),
	GATE(MDP_MDP_FG0, "MDP_MDP_FG0", "mdp_sel", mdp_cg0, 1, 0),
	GATE(MDP_MDP_HDR0, "MDP_MDP_HDR0", "mdp_sel", mdp_cg0, 2, 0),
	GATE(MDP_MDP_AAL0, "MDP_MDP_AAL0", "mdp_sel", mdp_cg0, 3, 0),
	GATE(MDP_MDP_RSZ0, "MDP_MDP_RSZ0", "mdp_sel", mdp_cg0, 4, 0),
	GATE(MDP_MDP_TDSHP0, "MDP_MDP_TDSHP0", "mdp_sel", mdp_cg0, 5, 0),
	GATE(MDP_MDP_TCC0, "MDP_MDP_TCC0", "mdp_sel", mdp_cg0, 6, 0),
	GATE(MDP_MDP_WROT0, "MDP_MDP_WROT0", "mdp_sel", mdp_cg0, 7, 0),
	GATE(MDP_MDP_RDMA2, "MDP_MDP_RDMA2", "mdp_sel", mdp_cg0, 8, 0),
	GATE(MDP_MDP_AAL2, "MDP_MDP_AAL2", "mdp_sel", mdp_cg0, 9, 0),
	GATE(MDP_MDP_RSZ2, "MDP_MDP_RSZ2", "mdp_sel", mdp_cg0, 10, 0),
	GATE(MDP_MDP_COLOR0, "MDP_MDP_COLOR0", "mdp_sel", mdp_cg0, 11, 0),
	GATE(MDP_MDP_TDSHP2, "MDP_MDP_TDSHP2", "mdp_sel", mdp_cg0, 12, 0),
	GATE(MDP_MDP_TCC2, "MDP_MDP_TCC2", "mdp_sel", mdp_cg0, 13, 0),
	GATE(MDP_MDP_WROT2, "MDP_MDP_WROT2", "mdp_sel", mdp_cg0, 14, 0),
	GATE(MDP_MDP_MUTEX0, "MDP_MDP_MUTEX0", "mdp_sel", mdp_cg0, 15, 0),
	GATE(MDP_MDP_RDMA1, "MDP_MDP_RDMA1", "mdp_sel", mdp_cg0, 16, 0),
	GATE(MDP_MDP_FG1, "MDP_MDP_FG1", "mdp_sel", mdp_cg0, 17, 0),
	GATE(MDP_MDP_HDR1, "MDP_MDP_HDR1", "mdp_sel", mdp_cg0, 18, 0),
	GATE(MDP_MDP_AAL1, "MDP_MDP_AAL1", "mdp_sel", mdp_cg0, 19, 0),
	GATE(MDP_MDP_RSZ1, "MDP_MDP_RSZ1", "mdp_sel", mdp_cg0, 20, 0),
	GATE(MDP_MDP_TDSHP1, "MDP_MDP_TDSHP1", "mdp_sel", mdp_cg0, 21, 0),
	GATE(MDP_MDP_TCC1, "MDP_MDP_TCC1", "mdp_sel", mdp_cg0, 22, 0),
	GATE(MDP_MDP_WROT1, "MDP_MDP_WROT1", "mdp_sel", mdp_cg0, 23, 0),
	GATE(MDP_MDP_RDMA3, "MDP_MDP_RDMA3", "mdp_sel", mdp_cg0, 24, 0),
	GATE(MDP_MDP_AAL3, "MDP_MDP_AAL3", "mdp_sel", mdp_cg0, 25, 0),
	GATE(MDP_MDP_RSZ3, "MDP_MDP_RSZ3", "mdp_sel", mdp_cg0, 26, 0),
	GATE(MDP_MDP_COLOR1, "MDP_MDP_COLOR1", "mdp_sel", mdp_cg0, 27, 0),
	GATE(MDP_MDP_TDSHP3, "MDP_MDP_TDSHP3", "mdp_sel", mdp_cg0, 28, 0),
	GATE(MDP_MDP_TCC3, "MDP_MDP_TCC3", "mdp_sel", mdp_cg0, 29, 0),
	GATE(MDP_MDP_WROT3, "MDP_MDP_WROT3", "mdp_sel", mdp_cg0, 30, 0),
	GATE(MDP_APB_BUS, "MDP_APB_BUS", "mdp_sel", mdp_cg0, 31, 0),
	/* MDPSYS_CG_CON1 */
	GATE(MDP_MMSYSRAM, "MDP_MMSYSRAM", "mdp_sel", mdp_cg1, 0, 0),
	GATE_DUMMY(MDP_APMCU_GALS, "MDP_APMCU_GALS", "mdp_sel", mdp_cg1, 1, 0),
	GATE(MDP_MDP_FAKE_ENG0, "MDP_MDP_FAKE_ENG0", "mdp_sel",
		mdp_cg1, 2, 0),
	GATE(MDP_MDP_FAKE_ENG1, "MDP_MDP_FAKE_ENG1", "mdp_sel",
		mdp_cg1, 3, 0),
	GATE_DUMMY(MDP_SMI0, "MDP_SMI0", "mdp_sel", mdp_cg1, 4, 0),
	GATE(MDP_IMG_DL_ASYNC0, "MDP_IMG_DL_ASYNC0", "mdp_sel",
		mdp_cg1, 5, 0),
	GATE(MDP_IMG_DL_ASYNC1, "MDP_IMG_DL_ASYNC1", "mdp_sel",
		mdp_cg1, 6, 0),
	GATE(MDP_IMG_DL_ASYNC2, "MDP_IMG_DL_ASYNC2", "mdp_sel",
		mdp_cg1, 7, 0),
	GATE_DUMMY(MDP_SMI1, "MDP_SMI1", "mdp_sel", mdp_cg1, 8, 0),
	GATE(MDP_IMG_DL_ASYNC3, "MDP_IMG_DL_ASYNC3", "mdp_sel",
		mdp_cg1, 9, 0),
	GATE_DUMMY(MDP_SMI2, "MDP_SMI2", "mdp_sel", mdp_cg1, 12, 0),
	/* MDPSYS_CG_CON2 */
	GATE(MDP_IMG0_IMG_DL_ASYNC0, "MDP_IMG0_IMG_DL_ASYNC0", "mdp_sel",
		mdp_cg2, 0, 0),
	GATE(MDP_IMG0_IMG_DL_ASYNC1, "MDP_IMG0_IMG_DL_ASYNC1", "mdp_sel",
		mdp_cg2, 1, 0),
	GATE(MDP_IMG1_IMG_DL_ASYNC2, "MDP_IMG1_IMG_DL_ASYNC2", "mdp_sel",
		mdp_cg2, 8, 0),
	GATE(MDP_IMG1_IMG_DL_ASYNC3, "MDP_IMG1_IMG_DL_ASYNC3", "mdp_sel",
		mdp_cg2, 9, 0),
};

static void __iomem *mdp_base;
static void mtk_mdpsys_init(struct device_node *node)
{
	pr_notice("%s(): init begin\n", __func__);

	mdp_base = mtk_gate_common_init(node, "mdp", mdpsys_config_clks,
				ARRAY_SIZE(mdpsys_config_clks), MDP_NR_CLK);
	if (!mdp_base)
		return;
#if MT_CCF_BRINGUP
	clk_writel(MDP_CG_CLR0, MDP_CG0);
	clk_writel(MDP_CG_CLR1, MDP_CG1);
	clk_writel(MDP_CG_CLR2, MDP_CG2);
	pr_notice("%s(): init done\n", __func__);
#endif
}
CLK_OF_DECLARE_DRIVER(mtk_mdpsys, "mediatek,mdpsys_config", mtk_mdpsys_init);


/******************* APMIXED Subsys *******************************/

static const struct mtk_gate_regs apmixed_cg_regs = {
	.set_ofs = 0x14,
	.clr_ofs = 0x14,
	.sta_ofs = 0x14,
};

static const struct mtk_gate apmixed_clks[] __initconst = {
	GATE_STA_INV(APMIXED_MIPID0_26M, "apmixed_mipi0_26m", "f26m_sel",
			 apmixed_cg_regs, 16, 1),
};


#define MT6885_PLL_FMAX		(3800UL * MHZ)
#define MT6885_PLL_FMIN		(1500UL * MHZ)
#define MT6885_INTEGER_BITS	8

#if MT_CCF_BRINGUP
#define PLL_CFLAGS		PLL_AO
#else
#define PLL_CFLAGS		(0)
#endif

#define PLL(_id, _name, _reg, _pwr_reg, _en_mask, _flags,	\
			_rst_bar_mask, _pcwbits, _pd_reg, _pd_shift,	\
			_tuner_reg, _tuner_en_reg, _tuner_en_bit,	\
			_pcw_reg, _pcw_shift) {				\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.pwr_reg = _pwr_reg,					\
		.en_mask = _en_mask,					\
		.flags = _flags,					\
		.rst_bar_mask = _rst_bar_mask,				\
		.fmax = MT6885_PLL_FMAX,				\
		.fmin = MT6885_PLL_FMIN,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6885_INTEGER_BITS,			\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,				\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.div_table = NULL,					\
	}


static const struct mtk_pll_data plls[] = {
	PLL(APMIXED_MAINPLL, "mainpll", 0x0340 /*con0*/, 0x034c /*con3*/,
		BIT(0)/*enmsk*/, PLL_AO, 0/*rstb*/, 22/*pcwbits*/,
		0x0344, 24/*pd*/, 0, 0, 0/* tuner*/, 0x0344, 0/* pcw */),

	PLL(APMIXED_UNIVPLL, "univpll", 0x0308 /*con0*/, 0x0314 /*con3*/,
		BIT(0)/*enmsk*/, (HAVE_RST_BAR | PLL_CFLAGS), BIT(23)/*rstb*/,
		22/*pcwbits*/,
		0x030c, 24/*pd*/, 0, 0, 0/* tuner*/, 0x030c, 0/* pcw */),

	PLL(APMIXED_MSDCPLL, "msdcpll", 0x0350 /*con0*/, 0x035c /*con3*/,
		BIT(0)/*enmsk*/, PLL_CFLAGS, 0/*rstb*/, 22/*pcwbits*/,
		0x0354, 24/*pd*/, 0, 0, 0/* tuner*/, 0x0354, 0/* pcw */),

	PLL(APMIXED_MMPLL, "mmpll", 0x0360 /*con0*/, 0x036c /*con3*/,
		BIT(0)/*enmsk*/, (HAVE_RST_BAR | PLL_CFLAGS), BIT(23)/*rstb*/,
		22/*pcwbits*/,
		0x0364, 24/*pd*/, 0, 0, 0/* tuner*/, 0x0364, 0/*pcw*/),

	PLL(APMIXED_ADSPPLL, "adsppll", 0x0370 /*con0*/, 0x037c /*con3*/,
		BIT(0)/*enmsk*/, PLL_CFLAGS, 0/*rstb*/, 22/*pcwbits*/,
		0x0374, 24/* pd */, 0, 0, 0/* tuner*/, 0x0374, 0/* pcw */),

	PLL(APMIXED_MFGPLL, "mfgpll", 0x0268 /*con0*/, 0x0274 /*con3*/,
		BIT(0)/*enmsk*/, PLL_CFLAGS, 0/*rstb*/, 22/*pcwbits*/,
		0x026C, 24/*pd*/, 0, 0, 0/* tuner*/, 0x026C, 0/* pcw */),

	PLL(APMIXED_TVDPLL, "tvdpll", 0x0380 /*con0*/, 0x038c /*con3*/,
		BIT(0)/*enmsk*/, PLL_CFLAGS, 0/*rstb*/, 22/*pcwbits*/,
		0x0384, 24/* pd */, 0, 0, 0/* tuner*/, 0x0384, 0/* pcw */),

	PLL(APMIXED_APUPLL, "apupll", 0x03A0 /*con0*/, 0x03AC /*con3*/,
		BIT(0)/*enmask*/, PLL_CFLAGS, 0/*rstb*/, 22/*pcwbits*/,
		0x03A4, 24/* pd */, 0, 0, 0/* tuner*/, 0x03A4, 0/* pcw */),

	PLL(APMIXED_APLL1, "apll1", 0x0318 /*con0*/, 0x0328 /*con4*/,
		BIT(0)/*enmask*/, PLL_CFLAGS, 0/*rstb*/, 32/*pcwbits*/,
		0x031C, 24/* pd */, 0x40, 0xc, 0/* tuner*/, 0x0320, 0/* pcw */),

	PLL(APMIXED_APLL2, "apll2", 0x032c /*con0*/, 0x033c /*con4*/,
		BIT(0)/*enmsk*/, PLL_CFLAGS, 0/*rstb*/, 32/*pcwbits*/,
		0x0330, 24/* pd */, 0x44, 0xc, 5/* tuner*/, 0x0334, 0/* pcw */),
};

static void __iomem *apmixed_base;
static void __init mtk_apmixedsys_init(struct device_node *node)
{

	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;

	pr_notice("%s(): init begin\n", __func__);
	base = of_iomap(node, 0);
	if (!base) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return;
	}

	clk_data = mtk_alloc_clk_data(APMIXED_NR_CLK);
	if (!clk_data) {
		pr_notice("%s(): allocate memory failed\n", __func__);
		return;
	}

	/* FIXME: add code for APMIXEDSYS */
	mtk_clk_register_plls(node, plls, ARRAY_SIZE(plls), clk_data);
	mtk_clk_register_gates(node, apmixed_clks,
		ARRAY_SIZE(apmixed_clks), clk_data);
	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);
	apmixed_base = base;

	/* MT6885, should porting to CTP also */
	/* clk_writel(PLLON_CON0, 0x07EFF7FB); */
	/* clk_writel(PLLON_CON1, 0x07EFF7FB); */

#define PLL_EN  (0x1 << 0)
#define PLL_PWR_ON  (0x1 << 0)
#define PLL_ISO_EN  (0x1 << 1)
#define PLL_DIV_RSTB  (0x1 << 23)

#if 1
/*MMPLL*/
	clk_clrl(MMPLL_CON0, PLL_DIV_RSTB);
	clk_clrl(MMPLL_CON0, PLL_EN);
	clk_setl(MMPLL_CON3, PLL_ISO_EN);
	clk_clrl(MMPLL_CON3, PLL_PWR_ON);
/*MSDCPLL*/
	clk_clrl(MSDCPLL_CON0, PLL_EN);
	clk_setl(MSDCPLL_CON3, PLL_ISO_EN);
	clk_clrl(MSDCPLL_CON3, PLL_PWR_ON);
/*MFGPLL*/
	clk_clrl(MFGPLL_CON0, PLL_EN);
	clk_setl(MFGPLL_CON3, PLL_ISO_EN);
	clk_clrl(MFGPLL_CON3, PLL_PWR_ON);
/*UNIVPLL*/
	clk_clrl(UNIVPLL_CON0, PLL_DIV_RSTB);
	clk_clrl(UNIVPLL_CON0, PLL_EN);
	clk_setl(UNIVPLL_CON3, PLL_ISO_EN);
	clk_clrl(UNIVPLL_CON3, PLL_PWR_ON);

/*TVDPLL*/
	clk_clrl(TVDPLL_CON0, PLL_EN);
	clk_setl(TVDPLL_CON3, PLL_ISO_EN);
	clk_clrl(TVDPLL_CON3, PLL_PWR_ON);

/*APLL1*/
	clk_clrl(APLL1_CON0, PLL_EN);
	clk_setl(APLL1_CON4, PLL_ISO_EN);
	clk_clrl(APLL1_CON4, PLL_PWR_ON);
/*APLL2*/
	clk_clrl(APLL2_CON0, PLL_EN);
	clk_setl(APLL2_CON4, PLL_ISO_EN);
	clk_clrl(APLL2_CON4, PLL_PWR_ON);
/*ADSPPLL*/
	clk_clrl(ADSPPLL_CON0, PLL_EN);
	clk_setl(ADSPPLL_CON3, PLL_ISO_EN);
	clk_clrl(ADSPPLL_CON3, PLL_PWR_ON);
/*APUPLL*/
	clk_clrl(APUPLL_CON0, PLL_EN);
	clk_setl(APUPLL_CON3, PLL_ISO_EN);
	clk_clrl(APUPLL_CON3, PLL_PWR_ON);
#endif
	pr_notice("%s(): init done\n", __func__);
}
CLK_OF_DECLARE_DRIVER(mtk_apmixedsys, "mediatek,apmixed",
		mtk_apmixedsys_init);

#if 0
void mp_enter_suspend(int id, int suspend)
{
	/* no more in MT6885 */
}
void armpll_control(int id, int on)
{
	/* no more in MT6885 */
}
#endif

void check_seninf_ck(void)
{
	/* MT6885: TODO */
}

void check_mm0_clk_sts(void)
{
	/* MT6885: TODO */
}

void check_vpu_clk_sts(void)
{
	/* MT6885: TODO */
}

void check_img_clk_sts(void)
{
	/* MT6885: TODO */
}

void check_ipe_clk_sts(void)
{
	/* MT6885: TODO */
}

void check_ven_clk_sts(void)
{
	/* MT6885: TODO */
}

void check_cam_clk_sts(void)
{
	/* MT6885: TODO */
}


#if 0 /*MT6885*/
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
#endif

void pll_force_off(void)
{
/*MFGPLL*/
	clk_clrl(MFGPLL_CON0, PLL_EN);
	clk_setl(MFGPLL_CON3, PLL_ISO_EN);
	clk_clrl(MFGPLL_CON3, PLL_PWR_ON);
/*UNIVPLL*/
	clk_clrl(UNIVPLL_CON0, PLL_EN);
	clk_setl(UNIVPLL_CON3, PLL_ISO_EN);
	clk_clrl(UNIVPLL_CON3, PLL_PWR_ON);
/*MSDCPLL*/
	clk_clrl(MSDCPLL_CON0, PLL_EN);
	clk_setl(MSDCPLL_CON3, PLL_ISO_EN);
	clk_clrl(MSDCPLL_CON3, PLL_PWR_ON);
/*MMPLL*/
	clk_clrl(MMPLL_CON0, PLL_EN);
	clk_setl(MMPLL_CON3, PLL_ISO_EN);
	clk_clrl(MMPLL_CON3, PLL_PWR_ON);
/*ADSPPLL*/
	clk_clrl(ADSPPLL_CON0, PLL_EN);
	clk_setl(ADSPPLL_CON3, PLL_ISO_EN);
	clk_clrl(ADSPPLL_CON3, PLL_PWR_ON);
/*TVDPLL*/
	clk_clrl(TVDPLL_CON0, PLL_EN);
	clk_setl(TVDPLL_CON3, PLL_ISO_EN);
	clk_clrl(TVDPLL_CON3, PLL_PWR_ON);
/*APLL1*/
	clk_clrl(APLL1_CON0, PLL_EN);
	clk_setl(APLL1_CON4, PLL_ISO_EN);
	clk_clrl(APLL1_CON4, PLL_PWR_ON);
/*APLL2*/
	clk_clrl(APLL2_CON0, PLL_EN);
	clk_setl(APLL2_CON4, PLL_ISO_EN);
	clk_clrl(APLL2_CON4, PLL_PWR_ON);
}

static int __init clk_mt6885_init(void)
{
	return 0;
}
arch_initcall(clk_mt6885_init);

