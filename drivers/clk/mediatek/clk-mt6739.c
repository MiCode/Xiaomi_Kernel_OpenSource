/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/


#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"
#include "clk-mux.h"

#include <dt-bindings/clock/mt6739-clk.h>
#include "clk-mt6739-pg.h"

#define MT_CCF_BRINGUP	0
/*fmeter div select 4*/
#define _DIV4_ 1

#ifdef CONFIG_ARM64
#define IOMEM(a)	((void __force __iomem *)((a)))
#endif

#define mt_reg_sync_writel(v, a) \
	do { __raw_writel((v), IOMEM(a)); mb(); } while (0) /* sync_writel*/
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

const char *ckgen_array[] = {
"hf_faxi_ck",
"hf_fmem_ck",
"hf_fddrphycfg_ck",
"hf_fmm_ck",
"f_fpwm_ck",
"f_fdispwm_ck",
"hf_fvdec_ck",
"hf_fvenc_ck",
"hf_fmfg_ck",
"hf_fcamtg_ck",
"hf_fi2c_ck",
"hf_fuart_ck",
"hf_fspi_ck",
"f_fusb20_ck",
"f_fusb30_p0_ck",
"hf_fmsdc50_0_hclk_ck",
"hf_fmsdc50_0_ck",
"hf_fmsdc30_1_ck",
"f_fi3c_ck",
"hf_fmsdc_30_3_ck",
"hf_fmsdc50_3_hclk_ck",
"hf_faudio_ck",
"hf_faud_intbus_ck",
"hf_pmicspi_ck",
"hf_fscp_ck",
"hf_fatb_ck",
"hf_fdsp_ck",
"hf_faud_1_ck",
"hf_faus_2_ck",
"hf_faud_engen1_ck",
"hf_faud_engen2_ck",
"hf_fdfp_mfg_ck",
"hf_fcam_ck",
"hf_fipu_if_ck",
"hf_faxi_mfg_in_as_ck",
"hf_fimg_ck",
"hf_faes_ufsfde_ck",
"hf_faes_fde_ck",
"hf_faudio_h_ck",
"hf_fsspm_ck",
"hf_fufs_card_ck",
"hf_fbsi_spi_ck",
"hf_fdxcc_ck",
"f_fseninf_ck",
"hf_fdfp_ck",
};

const char *abist_array[] = {
"AD_CSI0A_CDPHY_DELAYCAL_CK",
"AD_CSI0B_CDPHY_DELAYCAL_CK",
"AD_CSI1A_CDPHY_DELAYCAL_CK",
"AD_CSI1B_CDPHY_DELAYCAL_CK",
"AD_CSI2A_CDPHY_DELAYCAL_CK",
"AD_CSI2B_CDPHY_DELAYCAL_CK",
"AD_DSI0_CKG_DSICLK",
"AD_DSI0_TEST_CK",
"AD_DSI1_CKG_DSICLK",
"AD_DSI1_TEST_CK",
"AD_CCIPLL_CK_VCORE",
"AD_MMPLL_CK",
"AD_MDMCUPLL_CK",
"AD_MDINFRAPLL_CK",
"AD_BRPPLL_CK",
"AD_IMCPLL_CK",
"AD_ICCPLL_CK",
"AD_MPCPLL_CK",
"AD_DFEPLL_CK",
"AD_MD2GPLL_CK",
"AD_RAKEPLL_CK",
"AD_C2KCPPLL_CK",
"MCK_BEF_DCM_CHA",
"MCK_BEF_DCM_CHB",
"MCK_AFT_DCM_CHA",
"MCK_AFT_DCM_CHB",
"AD_RPHYPLL_DIV4_CK",
"AD_RCLRPLL_DIV4_CK",
"AD_PLLGP_TSTDIV2_CK",
"AD_MP_PLL0_CK_ABIST_OUT",
"AD_MP_RX0_TSTCK_DIV2",
"mp_tx_mon_div2_ck",
"mp_rx0_mon_div2_ck",
"AD_ARMPLL_L_CK_VCORE",
"AD_ARMPLL_M_CK_VCORE",
"AD_ARMPLL_B_CK_VCORE",
"AD_OSC_SYNC_CK_gated(PLL_ULPOSC_CON0[7]=1)",
"AD_OSC_SYNC_CK_2_gated(PLL_ULPOSC_CON2[7]=1)",
"msdc01_in_ck",
"msdc02_in_ck",
"msdc11_in_ck",
"msdc12_in_ck",
"msdc31_in_ck ",
"msdc32_in_ck",
"hd_fmem_ck_mon_gated(MEM_DCM_CTRL[27]=1)",
"AD_MPLL_CK",
"NA",
"NA",
"NA",
"NA",
"NA",
"AD_USB_192M_CK",
"AD_APLL1_CK",
"AD_APLL2_CK",
"AD_EMIPLL_CK",
"AD_GPUPLL_CK",
"AD_ LTEPLL_FS26M_CK",
"AD_ MDPLL1_FS208M_CK",
"AD_ MAINPLL_CK",
"AD_ UNIVPLL_CK",
"AD_ MSDCPLL_806M_CK",
"AD_ OSC_CK",
"AD_ OSC_CK_2",
"NA",
"DRAMC_CH0_CGCLK",
"NA",
"NA",
"AD_MAIN_H546M_CK",
"AD_MAIN_H364M_CK",
"AD_MAIN_H218P4M_CK",
"AD_MAIN_H156M_CK",
"AD_UNIVPLL_356P6M_CK",
"AD_UNIVPLL_624M_CK",
"AD_UNIVPLL_832M_CK",
"AD_UNIVPLL_499P2M_CK",
"AD_APLL1_CK",
"AD_APLL2_CK",
"AD_LTEPLL_FS26M_CK",
"rc32k_ck_i",
"AD_GPUPLL_CK",
"NA",
"AD_MMPLL_D5_CK",
"AD_MMPLL_D6_CK",
"AD_MMPLL_D7_CK",
"AD_MDPLL1_FS208M_CK_gated	(TST_SEL_3[31]=1)",
"NA",
"NA",
"AD_EMIPLL_CK",
"AD_MSDCPLL_806M_CK",
"AD_OSC_CK",
"AD_OSC_CK_2",
"fpc_ck",
"AD_USB_192M_CK",
"test_sel_1[1]",
"test_sel_1[2]",
};

static DEFINE_SPINLOCK(mt6739_clk_lock);

/* Total 7 subsys */
void __iomem *top_base;
void __iomem *infra_base;
void __iomem *apmixed_base;
void __iomem *audio_base;
void __iomem *img_base;
void __iomem *mm_base;
void __iomem *venc_base;

/* APMIXEDSYS Register */
#define AP_PLL_CON0		(apmixed_base + 0x0)
#define AP_PLL_CON1		(apmixed_base + 0x004)
#define AP_PLL_CON2		(apmixed_base + 0x008)
#define AP_PLL_CON3		(apmixed_base + 0x00C)
#define AP_PLL_CON4		(apmixed_base + 0x010)
#define AP_PLL_CON5		(apmixed_base + 0x014)
#define AP_PLL_CON6		(apmixed_base + 0x018)
#define AP_PLL_CON7		(apmixed_base + 0x01C)
#define AP_PLL_CON8		(apmixed_base + 0x020)
#define CLKSQ_STB_CON0		(apmixed_base + 0x024)
#define PLL_PWR_CON0		(apmixed_base + 0x028)
#define PLL_PWR_CON1		(apmixed_base + 0x02C)
#define PLL_ISO_CON0		(apmixed_base + 0x030)
#define PLL_ISO_CON1		(apmixed_base + 0x034)
#define PLL_STB_CON0		(apmixed_base + 0x038)
#define DIV_STB_CON0		(apmixed_base + 0x03C)
#define PLL_CHG_CON0		(apmixed_base + 0x040)
#define PLL_TEST_CON0		(apmixed_base + 0x044)
#define PLL_TEST_CON1		(apmixed_base + 0x048)
#define ARMPLL_LL_CON0		(apmixed_base + 0x200)
#define ARMPLL_LL_CON1		(apmixed_base + 0x204)
#define ARMPLL_LL_CON2		(apmixed_base + 0x208)
#define ARMPLL_LL_PWR_CON0	(apmixed_base + 0x20C)
#define MAINPLL_CON0		(apmixed_base + 0x220)
#define MAINPLL_CON1		(apmixed_base + 0x224)
#define MAINPLL_CON2		(apmixed_base + 0x228)
#define MAINPLL_PWR_CON0	(apmixed_base + 0x22C)
#define UNIVPLL_CON0		(apmixed_base + 0x230)
#define UNIVPLL_CON1		(apmixed_base + 0x234)
#define UNIVPLL_CON2		(apmixed_base + 0x238)
#define UNIVPLL_PWR_CON0	(apmixed_base + 0x23C)
#define MFGPLL_CON0		(apmixed_base + 0x240)
#define MFGPLL_CON1		(apmixed_base + 0x244)
#define MFGPLL_CON2		(apmixed_base + 0x248)
#define MFGPLL_PWR_CON0		(apmixed_base + 0x24C)
#define MSDCPLL_CON0		(apmixed_base + 0x250)
#define MSDCPLL_CON1		(apmixed_base + 0x254)
#define MSDCPLL_CON2		(apmixed_base + 0x258)
#define MSDCPLL_PWR_CON0	(apmixed_base + 0x25C)
#define MMPLL_CON0		(apmixed_base + 0x270)
#define MMPLL_CON1		(apmixed_base + 0x274)
#define MMPLL_CON2		(apmixed_base + 0x278)
#define MMPLL_PWR_CON0		(apmixed_base + 0x27C)
#define APLL1_CON0		(apmixed_base + 0x2A0)
#define APLL1_CON1		(apmixed_base + 0x2A4)
#define APLL1_CON2		(apmixed_base + 0x2A8)
#define APLL1_CON3		(apmixed_base + 0x2AC)
#define APLL1_PWR_CON0		(apmixed_base + 0x2B0)
#define AP_AUXADC_CON0		(apmixed_base + 0x400)
#define AP_AUXADC_CON1		(apmixed_base + 0x404)
#define AP_AUXADC_CON2		(apmixed_base + 0x408)
#define AP_AUXADC_CON3		(apmixed_base + 0x40C)
#define AP_AUXADC_CON4		(apmixed_base + 0x410)
#define AP_AUXADC_CON5		(apmixed_base + 0x414)
#define TS_CON0			(apmixed_base + 0x600)
#define TS_CON1			(apmixed_base + 0x604)
#define TS_CON2			(apmixed_base + 0x608)
#define AP_ABIST_MON_CON0	(apmixed_base + 0x800)
#define AP_ABIST_MON_CON1	(apmixed_base + 0x804)
#define AP_ABIST_MON_CON2	(apmixed_base + 0x808)
#define AP_ABIST_MON_CON3	(apmixed_base + 0x80C)
#define OCCSCAN_CON0		(apmixed_base + 0x810)
#define CLKDIV_CON0		(apmixed_base + 0x814)
#define OCCSCAN_CON1		(apmixed_base + 0x818)
#define OCCSCAN_CON2		(apmixed_base + 0x81C)
#define MCU_OCCSCAN_CON0	(apmixed_base + 0x820)
#define RSV_RW0_CON0		(apmixed_base + 0x900)
#define RSV_RW1_CON0		(apmixed_base + 0x904)
#define RSV_RO_CON0		(apmixed_base + 0x908)

/* infracfg_ao Base address: (+10001000h) */
#define INFRA_GLOBALCON_DCMCTL			(infra_base + 0x50)
#define INFRA_BUS_DCM_CTRL				(infra_base + 0x70)
#define PERI_BUS_DCM_CTRL				(infra_base + 0x74)
#define MODULE_SW_CG_0_SET				(infra_base + 0x80)
#define MODULE_SW_CG_0_CLR				(infra_base + 0x84)
#define MODULE_SW_CG_1_SET				(infra_base + 0x88)
#define MODULE_SW_CG_1_CLR				(infra_base + 0x8C)
#define MODULE_SW_CG_0_STA				(infra_base + 0x90)
#define MODULE_SW_CG_1_STA				(infra_base + 0x94)
#define MODULE_CLK_SEL					(infra_base + 0x98)
#define MODULE_SW_CG_2_SET				(infra_base + 0xA4)
#define MODULE_SW_CG_2_CLR				(infra_base + 0xA8)
#define MODULE_SW_CG_2_STA				(infra_base + 0xAC)
#define MODULE_SW_CG_3_SET				(infra_base + 0xC0)
#define MODULE_SW_CG_3_CLR				(infra_base + 0xC4)
#define MODULE_SW_CG_3_STA				(infra_base + 0xC8)
#define INFRA_TOPAXI_PROTECTEN			(infra_base + 0x0220)
#define INFRA_TOPAXI_PROTECTEN_STA1		(infra_base + 0x0228)
#define INFRA_TOPAXI_PROTECTEN_1		(infra_base + 0x0250)
#define INFRA_TOPAXI_PROTECTEN_STA1_1	(infra_base + 0x0258)

/* TOPCKGEN Register */
#define CLK_MODE		(top_base + 0x0)
#define CLK_CFG_UPDATE		(0x004)
#define CLK_CFG_UPDATE1		(0x008)
#define CLK_CFG_0			(0x040)
#define CLK_CFG_0_SET		(0x044)
#define CLK_CFG_0_CLR		(0x048)
#define CLK_CFG_1			(0x050)
#define CLK_CFG_1_SET		(0x054)
#define CLK_CFG_1_CLR		(0x058)
#define CLK_CFG_2			(0x060)
#define CLK_CFG_2_SET		(0x064)
#define CLK_CFG_2_CLR		(0x068)
#define CLK_CFG_3			(0x070)
#define CLK_CFG_3_SET		(0x074)
#define CLK_CFG_3_CLR		(0x078)
#define CLK_CFG_4			(0x080)
#define CLK_CFG_4_SET		(0x084)
#define CLK_CFG_4_CLR		(0x088)
#define CLK_CFG_5			(0x090)
#define CLK_CFG_5_SET		(0x094)
#define CLK_CFG_5_CLR		(0x098)
#define CLK_CFG_6			(0x0A0)
#define CLK_CFG_6_SET		(0x0A4)
#define CLK_CFG_6_CLR		(0x0A8)
#define CLK_CFG_7			(0x0B0)
#define CLK_CFG_7_SET		(0x0B4)
#define CLK_CFG_7_CLR		(0x0B8)
#define CLK_CFG_8			(0x0C0)
#define CLK_CFG_8_SET		(0x0C4)
#define CLK_CFG_8_CLR		(0x0C8)
#define CLK_CFG_9			(0x0D0)
#define CLK_CFG_9_SET		(0x0D4)
#define CLK_CFG_9_CLR		(0x0D8)
#define CLK_CFG_10			(0x0E0)
#define CLK_CFG_10_SET		(0x0E4)
#define CLK_CFG_10_CLR		(0x0E8)
#define CLK_MISC_CFG_0		(top_base + 0x104)
#define CLK_MISC_CFG_1		(top_base + 0x108)
#define CLK_DBG_CFG		(top_base + 0x10C)
#define CLK_SCP_CFG_0		(top_base + 0x200)
#define CLK_SCP_CFG_1		(top_base + 0x204)
#define CLK26CALI_0		(top_base + 0x220)
#define CLK26CALI_1		(top_base + 0x224)
#define CKSTA_REG		(top_base + 0x230)
#define CKSTA_REG1		(top_base + 0x234)
#define CLKMON_CLK_SEL_REG	(top_base + 0x300)
#define CLKMON_K1_REG		(top_base + 0x304)
#define CLK_AUDDIV_0		(top_base + 0x320)
#define CLK_AUDDIV_1		(top_base + 0x324)
#define CLK_AUDDIV_2		(top_base + 0x328)
#define AUD_TOP_CFG		(top_base + 0x32C)
#define AUD_TOP_MON		(top_base + 0x330)
#define CLK_PDN_REG		(top_base + 0x400)
#define CLK_EXTCK_REG		(top_base + 0x500)

#define AUDIO_TOP_CON0		(audio_base + 0x0000)
#define AUDIO_TOP_CON1		(audio_base + 0x0004)

#define IMG_CG_CON			(img_base + 0x0000)
#define IMG_CG_SET			(img_base + 0x0004)
#define IMG_CG_CLR			(img_base + 0x0008)

#define VCODECSYS_CG_CON	(venc_base + 0x0000)
#define VCODECSYS_CG_SET	(venc_base + 0x0004)
#define VCODECSYS_CG_CLR	(venc_base + 0x0008)

#define MM_CG_CON0			(mm_base + 0x100)
#define MM_CG_SET0			(mm_base + 0x104)
#define MM_CG_CLR0			(mm_base + 0x108)

#define INFRA_CG0 0x9BBFFF20
#define INFRA_CG1 0x1E8F7F56
#define INFRA_CG2 0x07FCC7DD
#define INFRA_CG3 0x00000DFF
/* AO */
#define AUDIO_DISABLE_CG0	0xF0F7FEFB
#define AUDIO_DISABLE_CG1	0xFFFFFF0F
#define IMG_DISABLE_CG		0x00000FE1
#define VEN_DISABLE_CG		0x00001111
#define MM_DISABLE_CG0		0xFFFFFFFF

static const struct mtk_fixed_clk fixed_clks[] = {
	FIXED_CLK(CLK_TOP_CLK26M, "f_f26m_ck", "clk26m", 26000000),
};



#define INVALID_PDN_SHIFT -1
#define INVALID_PDN_REG -1


static const struct mtk_fixed_factor_pdn top_divs[] = {
	FACTOR_PDN(CLK_TOP_SYSPLL, "syspll_ck", "mainpll", 1, 1, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_SYSPLL_D2, "syspll_d2", "syspll_ck", 1, 2, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_SYSPLL1_D2, "syspll1_d2", "syspll_d2", 1, 2, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_SYSPLL1_D4, "syspll1_d4", "syspll_d2", 1, 4, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_SYSPLL1_D8, "syspll1_d8", "syspll_d2", 1, 8, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_SYSPLL1_D16, "syspll1_d16", "syspll_d2", 1, 16, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_SYSPLL_D3, "syspll_d3", "mainpll", 1, 3, 30, 0x0220),
	FACTOR_PDN(CLK_TOP_SYSPLL2_D2, "syspll2_d2", "syspll_d3", 1, 2, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_SYSPLL2_D4, "syspll2_d4", "syspll_d3", 1, 4, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_SYSPLL_D5, "syspll_d5", "mainpll", 1, 5, 29, 0x0220),
	FACTOR_PDN(CLK_TOP_SYSPLL3_D2, "syspll3_d2", "syspll_d5", 1, 2, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_SYSPLL3_D4, "syspll3_d4", "syspll_d5", 1, 4, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_SYSPLL_D7, "syspll_d7", "mainpll", 1, 7, 28, 0x0220),
	FACTOR_PDN(CLK_TOP_SYSPLL4_D2, "syspll4_d2", "syspll_d7", 1, 2, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_SYSPLL4_D4, "syspll4_d4", "syspll_d7", 1, 4, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_UNIVPLL, "univpll_ck", "univpll", 1, 1, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_UNIVPLL_D26, "univpll_d26", "univpll_ck", 1, 26, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_UNIVPLL_48M_D2, "univpll_48m_d2", "univpll_d26", 1, 2, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_UNIVPLL_48M_D4, "univpll_48m_d4", "univpll_d26", 1, 4, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_UNIVPLL_48M_D8, "univpll_48m_d8", "univpll_d26", 1, 8, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_UNIVPLL_D2, "univpll_d2", "univpll", 1, 2, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_UNIVPLL1_D2, "univpll1_d2", "univpll_d2", 1, 2, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_UNIVPLL1_D4, "univpll1_d4", "univpll_d2", 1, 4, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_UNIVPLL_D3, "univpll_d3", "univpll", 1, 3, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_UNIVPLL2_D2, "univpll2_d2", "univpll_d3", 1, 2, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_UNIVPLL2_D4, "univpll2_d4", "univpll_d3", 1, 4, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_UNIVPLL2_D8, "univpll2_d8", "univpll_d3", 1, 8, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_UNIVPLL2_D32, "univpll2_d32", "univpll_d3", 1, 32, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_UNIVPLL_D5, "univpll_d5", "univpll", 1, 5, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_UNIVPLL3_D2, "univpll3_d2", "univpll_d5", 1, 2, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_UNIVPLL3_D4, "univpll3_d4", "univpll_d5", 1, 4, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_UNIVPLL3_D8, "univpll3_d8", "univpll_d5", 1, 8, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_MMPLL, "mmpll_ck", "mfgpll", 1, 1, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_VENCPLL, "vencpll_ck", "mmpll", 1, 1, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_MSDCPLL, "msdcpll_ck", "msdcpll", 1, 1, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_MSDCPLL_D2, "msdcpll_d2", "msdcpll", 1, 2, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_APLL1, "apll1_ck", "apll1", 1, 1, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_APLL1_D2, "apll1_d2", "apll1", 1, 2, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_APLL1_D4, "apll1_d4", "apll1", 1, 4, INVALID_PDN_SHIFT, INVALID_PDN_REG),
	FACTOR_PDN(CLK_TOP_APLL1_D8, "apll1_d8", "apll1", 1, 8, INVALID_PDN_SHIFT, INVALID_PDN_REG),
#if 0
	FACTOR_PDN(CLK_TOP_EMIPLL, "emipll_ck", "emipll",
		1, 1),

#endif
};

static const char * const axi_parents[] = {
	"clk26m",
	"syspll_d7",
	"syspll1_d4",
	"syspll3_d2"
};

static const char * const mem_parents[] = {
	"clk26m",
	"dmpll_ck",
	"apll1_ck"
};

static const char * const ddrphycfg_parents[] = {
	"clk26m",
	"syspll1_d8"
};

static const char * const mm_parents[] = {
	"clk26m",
	"vencpll_ck",
	"syspll1_d2",
	"syspll_d5",
	"syspll1_d4",
	"univpll_d5",
	"syspll2_d2",
	"univpll2_d2"
};

static const char * const mfg_parents[] = {
	"clk26m",
	"mmpll_ck",
	"syspll_d3",
	"syspll_d5"
};

static const char * const camtg_parents[] = {
	"clk26m",
	"univpll_48m_d2",
	"univpll2_d8",
	"univpll_d26",
	"univpll2_d32",
	"univpll_48m_d4",
	"univpll_48m_d8"
};

static const char * const uart_parents[] = {
	"clk26m",
	"univpll2_d8"
};

static const char * const spi_parents[] = {
	"clk26m",
	"syspll3_d2",
	"syspll4_d2",
	"syspll2_d4"
};

static const char * const msdc5hclk_parents[] = {
	"clk26m",
	"syspll1_d2",
	"syspll2_d2"
};

static const char * const msdc50_0_parents[] = {
	"clk26m",
	"msdcpll_ck",
	"syspll2_d2",
	"syspll4_d2",
	"univpll1_d2",
	"syspll1_d2",
	"univpll_d5",
	"univpll1_d4"
};

static const char * const msdc30_1_parents[] = {
	"clk26m",
	"msdcpll_d2",
	"univpll2_d2",
	"syspll2_d2",
	"syspll1_d4",
	"univpll1_d4",
	"univpll_d26",
	"syspll2_d4"
};

static const char * const audio_parents[] = {
	"clk26m",
	"syspll3_d4",
	"syspll4_d4",
	"syspll1_d16"
};

static const char * const aud_intbus_parents[] = {
	"clk26m",
	"syspll1_d4",
	"syspll4_d2"
};

static const char * const dbi0_parents[] = {
	"clk26m",
	"univpll3_d2",
	"univpll2_d4",
	"syspll4_d2",
	"univpll2_d8"
};

static const char * const scam_parents[] = {
	"clk26m",
	"syspll3_d2",
	"univpll2_d4"
};

static const char * const aud_1_parents[] = {
	"clk26m",
	"apll1_ck"
};

static const char * const disp_pwm_parents[] = {
	"clk26m",
	"univpll2_d4",
	"univpll2_d8",
	"univpll3_d8"
};

static const char * const nfi2x_parents[] = {
	"clk26m",
	"syspll2_d2",
	"syspll_d7",
	"syspll_d3",
	"syspll2_d4",
	"msdcpll_d2",
	"univpll1_d2",
	"univpll_d5"
};

static const char * const nfiecc_parents[] = {
	"clk26m",
	"syspll4_d2",
	"univpll2_d4",
	"syspll_d7",
	"univpll1_d2",
	"syspll1_d2",
	"univpll2_d2",
	"syspll_d5"
};

static const char * const usb_top_parents[] = {
	"clk26m",
	"univpll3_d4"
};

static const char * const i2c_parents[] = {
	"clk26m",
	"univpll_d26",
	"univpll3_d4"
};

static const char * const senif_parents[] = {
	"clk26m",
	"univpll1_d4",
	"univpll2_d2"
};

static const char * const dxcc_parents[] = {
	"clk26m",
	"syspll1_d2",
	"syspll1_d4",
	"syspll1_d8"
};

static const char * const aud_engen1_parents[] = {
	"clk26m",
	"apll1_d2",
	"apll1_d4",
	"apll1_d8"
};

#define INVALID_UPDATE_REG 0xFFFFFFFF
#define INVALID_UPDATE_SHIFT -1
#define INVALID_MUX_GATE -1

static const struct mtk_mux top_muxes[] = {
#if 1
	/* CLK_CFG_0 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AXI_SEL, "axi_sel", axi_parents,
	CLK_CFG_0, CLK_CFG_0_SET, CLK_CFG_0_CLR, 0, 2,
	INVALID_MUX_GATE, INVALID_UPDATE_REG, INVALID_UPDATE_SHIFT),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MM_SEL, "mm_sel", mm_parents,
	CLK_CFG_0, CLK_CFG_0_SET, CLK_CFG_0_CLR, 24, 3, 31, CLK_CFG_UPDATE, 3),
	/* CLK_CFG_1 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MFG_SEL, "mfg_sel", mfg_parents,
	CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR, 24, 2, 31, CLK_CFG_UPDATE, 7),
	/* CLK_CFG_2 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG_SEL, "camtg_sel", camtg_parents,
	CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR, 0, 3, 7, CLK_CFG_UPDATE, 8),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_UART_SEL, "uart_sel", uart_parents,
	CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR, 8, 1, 15, CLK_CFG_UPDATE, 9),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI_SEL, "spi_sel", spi_parents,
	CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR, 16, 2, 23, CLK_CFG_UPDATE, 10),
	/* CLK_CFG_3 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC50_0_HCLK_SEL, "msdc5hclk",
		msdc5hclk_parents,
	CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR, 8, 2, 15, CLK_CFG_UPDATE, 12),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC50_0_SEL, "msdc50_0_sel",
		msdc50_0_parents,
	CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR, 16, 3, 23, CLK_CFG_UPDATE, 13),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC30_1_SEL, "msdc30_1_sel",
		msdc30_1_parents,
	CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR, 24, 3, 31, CLK_CFG_UPDATE, 14),
	/* CLK_CFG_4 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUDIO_SEL, "audio_sel", audio_parents,
	CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR, 16, 2, 23, CLK_CFG_UPDATE, 17),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_INTBUS_SEL, "aud_intbus_sel",
		aud_intbus_parents,
	CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR, 24, 2, 31, CLK_CFG_UPDATE, 18),
	/* CLK_CFG_6 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DBI0_SEL, "dbi0_sel", dbi0_parents,
	CLK_CFG_6, CLK_CFG_6_SET, CLK_CFG_6_CLR, 0, 3, 7, CLK_CFG_UPDATE, 23),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SCAM_SEL, "scam_sel", scam_parents,
	CLK_CFG_6, CLK_CFG_6_SET, CLK_CFG_6_CLR, 8, 2, 15, CLK_CFG_UPDATE, 24),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_1_SEL, "aud_1_sel", aud_1_parents,
	CLK_CFG_6, CLK_CFG_6_SET, CLK_CFG_6_CLR, 16, 1, 23, CLK_CFG_UPDATE, 25),
	/* CLK_CFG_7 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DISP_PWM_SEL, "disp_pwm_sel",
		disp_pwm_parents,
	CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR, 0, 2, 7, CLK_CFG_UPDATE, 27),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_NFI2X_SEL, "nfi2x_sel", nfi2x_parents,
	CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR, 8, 3, 15, CLK_CFG_UPDATE, 28),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_NFIECC_SEL, "nfiecc_sel", nfiecc_parents,
	CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR, 16, 3, 23, CLK_CFG_UPDATE, 29),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USB_TOP_SEL, "usb_top_sel",
		usb_top_parents,
	CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR, 24, 1, 31, CLK_CFG_UPDATE, 30),
	/* CLK_CFG_8 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPM_SEL, "spm_sel", ddrphycfg_parents,
	CLK_CFG_8, CLK_CFG_8_SET, CLK_CFG_8_CLR, 0, 1, 7, CLK_CFG_UPDATE, 31),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_I2C_SEL, "i2c_sel", i2c_parents,
	CLK_CFG_8, CLK_CFG_8_SET, CLK_CFG_8_CLR, 16, 2, INVALID_MUX_GATE,
	CLK_CFG_UPDATE1, 1),
	/* CLK_CFG_9 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SENIF_SEL, "senif_sel", senif_parents,
	CLK_CFG_9, CLK_CFG_9_SET, CLK_CFG_9_CLR, 8, 2, 15, CLK_CFG_UPDATE1, 3),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DXCC_SEL, "dxcc_sel", dxcc_parents,
	CLK_CFG_9, CLK_CFG_9_SET, CLK_CFG_9_CLR, 16, 2, 23, CLK_CFG_UPDATE1, 4),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_CAMTG2_SEL, "camtg2_sel", camtg_parents,
	CLK_CFG_9, CLK_CFG_9_SET, CLK_CFG_9_CLR, 24, 3, 31, CLK_CFG_UPDATE1, 9),
	/* CLK_CFG_10 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_ENGEN1_SEL, "aud_engen1_sel",
		aud_engen1_parents,
		CLK_CFG_10, CLK_CFG_10_SET, CLK_CFG_10_CLR, 0, 2, 7,
		CLK_CFG_UPDATE1, 5),

#else
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_AXI_SEL, "axi_sel", axi_parents,
		CLK_CFG_0, CLK_CFG_0_SET, CLK_CFG_0_CLR, 0, 2,
		INVALID_MUX_GATE, INVALID_UPDATE_REG, INVALID_UPDATE_SHIFT),
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_MEM_SEL, "mem_sel", mem_parents,
		CLK_CFG_0, CLK_CFG_0_SET, CLK_CFG_0_CLR, 8, 2, 15,
		CLK_CFG_UPDATE, 1),
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_DDRPHYCFG_SEL, "ddrphycfg_sel",
		ddrphycfg_parents,
		CLK_CFG_0, CLK_CFG_0_SET, CLK_CFG_0_CLR, 16, 1, 23,
		CLK_CFG_UPDATE, 2),
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_MM_SEL, "mm_sel", mm_parents,
		CLK_CFG_0, CLK_CFG_0_SET, CLK_CFG_0_CLR, 24, 3, 31,
		CLK_CFG_UPDATE, 3),
	/* CLK_CFG_1 */
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_MFG_SEL, "mfg_sel", mfg_parents,
		CLK_CFG_1, CLK_CFG_1_SET, CLK_CFG_1_CLR, 24, 2, 31,
		CLK_CFG_UPDATE, 7),
	/* CLK_CFG_2 */
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_CAMTG_SEL, "camtg_sel", camtg_parents,
		CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR, 0, 3, 7,
		CLK_CFG_UPDATE, 8),
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_UART_SEL, "uart_sel", uart_parents,
		CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR, 8, 1, 15,
		CLK_CFG_UPDATE, 9),
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_SPI_SEL, "spi_sel", spi_parents,
		CLK_CFG_2, CLK_CFG_2_SET, CLK_CFG_2_CLR, 16, 2, 23,
		CLK_CFG_UPDATE, 10),
	/* CLK_CFG_3 */
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_MSDC50_0_HCLK_SEL, "msdc5hclk",
		msdc5hclk_parents,
		CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR, 8, 2, 15,
		CLK_CFG_UPDATE, 12),
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_MSDC50_0_SEL, "msdc50_0_sel",
		msdc50_0_parents,
		CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR, 16, 3, 23,
		CLK_CFG_UPDATE, 13),
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_MSDC30_1_SEL, "msdc30_1_sel",
		msdc30_1_parents,
		CLK_CFG_3, CLK_CFG_3_SET, CLK_CFG_3_CLR, 24, 3, 31,
		CLK_CFG_UPDATE, 14),
	/* CLK_CFG_4 */
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_AUDIO_SEL, "audio_sel", audio_parents,
		CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR, 16, 2, 23,
		CLK_CFG_UPDATE, 17),
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_AUD_INTBUS_SEL, "aud_intbus_sel",
		aud_intbus_parents,
		CLK_CFG_4, CLK_CFG_4_SET, CLK_CFG_4_CLR, 24, 2, 31,
		CLK_CFG_UPDATE, 18),
	/* CLK_CFG_6 */
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_DBI0_SEL, "dbi0_sel", dbi0_parents,
		CLK_CFG_6, CLK_CFG_6_SET, CLK_CFG_6_CLR, 0, 3, 7,
		CLK_CFG_UPDATE, 23),
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_SCAM_SEL, "scam_sel", scam_parents,
		CLK_CFG_6, CLK_CFG_6_SET, CLK_CFG_6_CLR, 8, 2, 15,
		CLK_CFG_UPDATE, 24),
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_AUD_1_SEL, "aud_1_sel", aud_1_parents,
		CLK_CFG_6, CLK_CFG_6_SET, CLK_CFG_6_CLR, 16, 1, 23,
		CLK_CFG_UPDATE, 25),
	/* CLK_CFG_7 */
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_DISP_PWM_SEL, "disp_pwm_sel",
		disp_pwm_parents,
		CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR, 0, 2, 7,
		CLK_CFG_UPDATE, 27),
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_NFI2X_SEL, "nfi2x_sel", nfi2x_parents,
		CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR, 8, 3, 15,
		CLK_CFG_UPDATE, 28),
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_NFIECC_SEL, "nfiecc_sel",
		nfiecc_parents,
		CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR, 16, 3, 23,
		CLK_CFG_UPDATE, 29),
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_USB_TOP_SEL, "usb_top_sel",
		usb_top_parents,
		CLK_CFG_7, CLK_CFG_7_SET, CLK_CFG_7_CLR, 24, 1, 31,
		CLK_CFG_UPDATE, 30),
	/* CLK_CFG_8 */
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_SPM_SEL, "spm_sel", ddrphycfg_parents,
		CLK_CFG_8, CLK_CFG_8_SET, CLK_CFG_8_CLR, 0, 1, 7,
		CLK_CFG_UPDATE, 31),
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_I2C_SEL, "i2c_sel", i2c_parents,
		CLK_CFG_8, CLK_CFG_8_SET, CLK_CFG_8_CLR, 16, 2, 23,
		CLK_CFG_UPDATE1, 1),
	/* CLK_CFG_9 */
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_SENIF_SEL, "senif_sel", senif_parents,
		CLK_CFG_9, CLK_CFG_9_SET, CLK_CFG_9_CLR, 8, 2, 15,
		CLK_CFG_UPDATE1, 3),
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_DXCC_SEL, "dxcc_sel", dxcc_parents,
		CLK_CFG_9, CLK_CFG_9_SET, CLK_CFG_9_CLR, 16, 2, 23,
		CLK_CFG_UPDATE1, 4),
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_CAMTG2_SEL, "camtg2_sel", camtg_parents,
		CLK_CFG_9, CLK_CFG_9_SET, CLK_CFG_9_CLR, 24, 3, 31,
		CLK_CFG_UPDATE1, 9),
	/* CLK_CFG_10 */
	MUX_CLR_SET_UPD_MT6739(CLK_TOP_AUD_ENGEN1_SEL, "aud_engen1_sel",
		aud_engen1_parents,
		CLK_CFG_10, CLK_CFG_10_SET, CLK_CFG_10_CLR, 0, 2, 7,
		CLK_CFG_UPDATE1, 5),
#endif
};
static int mtk_cg_bit_is_cleared(struct clk_hw *hw)
{
#if 1
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 val;

	regmap_read(cg->regmap, cg->sta_ofs, &val);

	val &= BIT(cg->bit);

	return val == 0;
#endif
	return 0;
}

static int mtk_cg_bit_is_set(struct clk_hw *hw)
{
#if 1
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 val;

	regmap_read(cg->regmap, cg->sta_ofs, &val);

	val &= BIT(cg->bit);

	return val != 0;
#endif
	return 0;
}
#if 1
static void mtk_cg_set_bit(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);

	regmap_update_bits(cg->regmap, cg->sta_ofs, BIT(cg->bit), BIT(cg->bit));
}

static void mtk_cg_clr_bit(struct clk_hw *hw)
{
#if 1
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);

	regmap_update_bits(cg->regmap, cg->sta_ofs, BIT(cg->bit), 0);
#endif
}
#endif
static int mtk_cg_enable(struct clk_hw *hw)
{
	mtk_cg_clr_bit(hw);

	return 0;
}

static void mtk_cg_disable(struct clk_hw *hw)
{
	mtk_cg_set_bit(hw);
}

static int mtk_cg_enable_inv(struct clk_hw *hw)
{
	mtk_cg_set_bit(hw);

	return 0;
}

static void mtk_cg_disable_inv(struct clk_hw *hw)
{
	mtk_cg_clr_bit(hw);
}

const struct clk_ops mtk_clk_gate_ops = {
	.is_enabled	= mtk_cg_bit_is_cleared,
	.enable		= mtk_cg_enable,
	.disable	= mtk_cg_disable,
};

const struct clk_ops mtk_clk_gate_ops_inv = {
	.is_enabled	= mtk_cg_bit_is_set,
	.enable		= mtk_cg_enable_inv,
	.disable	= mtk_cg_disable_inv,
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
		.regs = &infra0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_INFRA1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_INFRA2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_INFRA3(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &infra3_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate infra_clks[] = {
	/* INFRA0 */
	GATE_INFRA0(CLK_INFRA_PMIC_TMR, "infra_pmic_tmr", "clk26m", 0),
	GATE_INFRA0(CLK_INFRA_PMIC_AP, "infra_pmic_ap", "clk26m", 1),
	GATE_INFRA0(CLK_INFRA_PMIC_MD, "infra_pmic_md", "clk26m", 2),
	GATE_INFRA0(CLK_INFRA_PMIC_CONN, "infra_pmic_conn", "clk26m", 3),
	GATE_INFRA0(CLK_INFRA_SEJ, "infra_sej", "clk26m", 5),
	GATE_INFRA0(CLK_INFRA_APXGPT, "infra_apxgpt", "axi_sel", 6),
	GATE_INFRA0(CLK_INFRA_ICUSB, "infra_icusb", "axi_sel", 8),
	GATE_INFRA0(CLK_INFRA_GCE, "infra_gce", "axi_sel", 9),
	GATE_INFRA0(CLK_INFRA_THERM, "infra_therm", "axi_sel", 10),
	GATE_INFRA0(CLK_INFRA_I2C0, "infra_i2c0", "i2c_sel", 11),
	GATE_INFRA0(CLK_INFRA_I2C1, "infra_i2c1", "i2c_sel", 12),
	GATE_INFRA0(CLK_INFRA_I2C2, "infra_i2c2", "i2c_sel", 13),
	GATE_INFRA0(CLK_INFRA_I2C3, "infra_i2c3", "i2c_sel", 14),
	GATE_INFRA0(CLK_INFRA_PWM_HCLK, "infra_pwm_hclk", "axi_sel", 15),
	GATE_INFRA0(CLK_INFRA_PWM1, "infra_pwm1", "i2c_sel", 16),
	GATE_INFRA0(CLK_INFRA_PWM2, "infra_pwm2", "i2c_sel", 17),
	GATE_INFRA0(CLK_INFRA_PWM3, "infra_pwm3", "i2c_sel", 18),
	GATE_INFRA0(CLK_INFRA_PWM4, "infra_pwm4", "i2c_sel", 19),
	GATE_INFRA0(CLK_INFRA_PWM5, "infra_pwm5", "i2c_sel", 20),
	GATE_INFRA0(CLK_INFRA_PWM, "infra_pwm", "i2c_sel", 21),
	GATE_INFRA0(CLK_INFRA_UART0, "infra_uart0", "uart_sel", 22),
	GATE_INFRA0(CLK_INFRA_UART1, "infra_uart1", "uart_sel", 23),
	GATE_INFRA0(CLK_INFRA_UART2, "infra_uart2", "uart_sel", 24),
	GATE_INFRA0(CLK_INFRA_UART3, "infra_uart3", "uart_sel", 25),
	GATE_INFRA0(CLK_INFRA_GCE_26M, "infra_gce_26m", "axi_sel", 27),
	GATE_INFRA0(CLK_INFRA_CQ_DMA_FPC, "infra_dma", "axi_sel", 28),
	GATE_INFRA0(CLK_INFRA_BTIF, "infra_btif", "axi_sel", 31),
	/* INFRA1 */
	GATE_INFRA1(CLK_INFRA_SPI0, "infra_spi0", "spi_sel", 1),
	GATE_INFRA1(CLK_INFRA_MSDC0, "infra_msdc0", "axi_sel", 2),
	GATE_INFRA1(CLK_INFRA_MSDC1, "infra_msdc1", "axi_sel", 4),
	GATE_INFRA1(CLK_INFRA_NFIECC_312M, "infra_nfiecc", "nfiecc_sel", 6),
	GATE_INFRA1(CLK_INFRA_DVFSRC, "infra_dvfsrc", "clk26m", 7),
	GATE_INFRA1(CLK_INFRA_GCPU, "infra_gcpu", "axi_sel", 8),
	GATE_INFRA1(CLK_INFRA_TRNG, "infra_trng", "axi_sel", 9),
	GATE_INFRA1(CLK_INFRA_AUXADC, "infra_auxadc", "clk26m", 10),
	GATE_INFRA1(CLK_INFRA_CPUM, "infra_cpum", "axi_sel", 11),
	GATE_INFRA1(CLK_INFRA_CCIF1_AP, "infra_ccif1_ap", "axi_sel", 12),
	GATE_INFRA1(CLK_INFRA_CCIF1_MD, "infra_ccif1_md", "axi_sel", 13),
	GATE_INFRA1(CLK_INFRA_AUXADC_MD, "infra_auxadc_md", "clk26m", 14),
	GATE_INFRA1(CLK_INFRA_NFI, "infra_nfi", "nfi2x_sel", 16),
	GATE_INFRA1(CLK_INFRA_NFI_1X, "infra_nfi_1x", "nfi2x_sel", 17),
	GATE_INFRA1(CLK_INFRA_AP_DMA, "infra_ap_dma", "axi_sel", 18),
	GATE_INFRA1(CLK_INFRA_XIU, "infra_xiu", "axi_sel", 19),
	GATE_INFRA1(CLK_INFRA_DEVICE_APC, "infra_dapc", "axi_sel", 20),
	GATE_INFRA1(CLK_INFRA_CCIF_AP, "infra_ccif_ap", "axi_sel", 23),
	GATE_INFRA1(CLK_INFRA_DEBUGSYS, "infra_debugsys", "axi_sel", 24),
	GATE_INFRA1(CLK_INFRA_AUDIO, "infra_audio", "axi_sel", 25),
	GATE_INFRA1(CLK_INFRA_CCIF_MD, "infra_ccif_md", "axi_sel", 26),
	GATE_INFRA1(CLK_INFRA_DXCC_SEC_CORE, "infra_secore", "dxcc_sel", 27),
	GATE_INFRA1(CLK_INFRA_DXCC_AO, "infra_dxcc_ao", "dxcc_sel", 28),
	GATE_INFRA1(CLK_INFRA_DRAMC_F26M, "infra_dramc26", "clk26m", 31),
	/* INFRA2 */
	GATE_INFRA2(CLK_INFRA_RG_PWM_FBCLK6, "infra_pwmfb", "clk26m", 0),
	GATE_INFRA2(CLK_INFRA_DISP_PWM, "infra_disp_pwm", "disp_pwm_sel", 2),
	GATE_INFRA2(CLK_INFRA_CLDMA_BCLK, "infra_cldmabclk", "axi_sel", 3),
	GATE_INFRA2(CLK_INFRA_AUDIO_26M_BCLK, "infra_audio26m", "clk26m", 4),
	GATE_INFRA2(CLK_INFRA_SPI1, "infra_spi1", "spi_sel", 6),
	GATE_INFRA2(CLK_INFRA_I2C4, "infra_i2c4", "i2c_sel", 7),
	GATE_INFRA2(CLK_INFRA_MODEM_TEMP_SHARE, "infra_mdtemp", "clk26m", 8),
	GATE_INFRA2(CLK_INFRA_SPI2, "infra_spi2", "spi_sel", 9),
	GATE_INFRA2(CLK_INFRA_SPI3, "infra_spi3", "spi_sel", 10),
	GATE_INFRA2(CLK_INFRA_I2C5, "infra_i2c5", "i2c_sel", 18),
	GATE_INFRA2(CLK_INFRA_I2C5_ARBITER, "infra_i2c5a", "i2c_sel", 19),
	GATE_INFRA2(CLK_INFRA_I2C5_IMM, "infra_i2c5_imm", "i2c_sel", 20),
	GATE_INFRA2(CLK_INFRA_I2C1_ARBITER, "infra_i2c1a", "i2c_sel", 21),
	GATE_INFRA2(CLK_INFRA_I2C1_IMM, "infra_i2c1_imm", "i2c_sel", 22),
	GATE_INFRA2(CLK_INFRA_I2C2_ARBITER, "infra_i2c2a", "i2c_sel", 23),
	GATE_INFRA2(CLK_INFRA_I2C2_IMM, "infra_i2c2_imm", "i2c_sel", 24),
	GATE_INFRA2(CLK_INFRA_SPI4, "infra_spi4", "spi_sel", 25),
	GATE_INFRA2(CLK_INFRA_SPI5, "infra_spi5", "spi_sel", 26),
	GATE_INFRA2(CLK_INFRA_CQ_DMA, "infra_cq_dma", "axi_sel", 27),
	/* INFRA3 */
	GATE_INFRA3(CLK_INFRA_MSDC0_SELF, "infra_msdc0sf", "msdc50_0_sel", 0),
	GATE_INFRA3(CLK_INFRA_MSDC1_SELF, "infra_msdc1sf", "msdc50_0_sel", 1),
	GATE_INFRA3(CLK_INFRA_MSDC2_SELF, "infra_msdc2sf", "msdc50_0_sel", 2),
	GATE_INFRA3(CLK_INFRA_I2C6, "infra_i2c6", "i2c_sel", 6),
	GATE_INFRA3(CLK_INFRA_AP_MSDC0, "infra_ap_msdc0", "msdc50_0_sel", 7),
	GATE_INFRA3(CLK_INFRA_MD_MSDC0, "infra_md_msdc0", "msdc50_0_sel", 8),
	/*DUMMY CG*/
	GATE_INFRA3(CLK_INFRA_MSDC0_SRC, "infra_msdc0_clk", "msdc50_0_sel", 31),
	GATE_INFRA3(CLK_INFRA_MSDC1_SRC, "infra_msdc1_clk", "msdc30_1_sel", 10),
	GATE_INFRA3(CLK_INFRA_MSDC2_SRC, "infra_msdc2_clk", "msdc50_0_sel", 11),
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
		.regs = &audio0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops,		\
	}

#define GATE_AUDIO1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &audio1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops,		\
	}

static const struct mtk_gate audio_clks[] = {
	/* AUDIO0 */
	GATE_AUDIO0(CLK_AUDIO_AFE, "aud_afe", "audio_sel",
		2),
	GATE_AUDIO0(CLK_AUDIO_22M, "aud_22m", "aud_engen1_sel",
		8),
	GATE_AUDIO0(CLK_AUDIO_APLL_TUNER, "aud_apll_tuner", "aud_engen1_sel",
		19),
	GATE_AUDIO0(CLK_AUDIO_ADC, "aud_adc", "audio_sel",
		24),
	GATE_AUDIO0(CLK_AUDIO_DAC, "aud_dac", "audio_sel",
		25),
	GATE_AUDIO0(CLK_AUDIO_DAC_PREDIS, "aud_dac_predis", "audio_sel",
		26),
	GATE_AUDIO0(CLK_AUDIO_TML, "aud_tml", "audio_sel",
		27),
	/* AUDIO1 */
	GATE_AUDIO1(CLK_AUDIO_I2S1_BCLK, "aud_i2s1_bclk", "audio_sel",
		4),
	GATE_AUDIO1(CLK_AUDIO_I2S2_BCLK, "aud_i2s2_bclk", "audio_sel",
		5),
	GATE_AUDIO1(CLK_AUDIO_I2S3_BCLK, "aud_i2s3_bclk", "audio_sel",
		6),
	GATE_AUDIO1(CLK_AUDIO_I2S4_BCLK, "aud_i2s4_bclk", "audio_sel",
		7),
};

static const struct mtk_gate_regs img_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IMG(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &img_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate img_clks[] = {
	GATE_IMG(CLK_IMG_LARB2_SMI, "img_larb2_smi", "mm_sel", 0),
	GATE_IMG(CLK_IMG_CAM_SMI, "img_cam_smi", "mm_sel", 5),
	GATE_IMG(CLK_IMG_CAM_CAM, "img_cam_cam", "mm_sel", 6),
	GATE_IMG(CLK_IMG_SEN_TG, "img_sen_tg", "mm_sel", 7),
	GATE_IMG(CLK_IMG_SEN_CAM, "img_sen_cam", "mm_sel", 8),
	GATE_IMG(CLK_IMG_CAM_SV, "img_cam_sv", "mm_sel", 9),
	GATE_IMG(CLK_IMG_SUFOD, "img_sufod", "mm_sel", 10),
	GATE_IMG(CLK_IMG_FD, "img_fd", "mm_sel", 11),
};

static const struct mtk_gate_regs mm_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

#define GATE_MM(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate mm_clks[] = {
	GATE_MM(CLK_MM_SMI_COMMON, "mm_smi_common", "mm_sel", 0),
	GATE_MM(CLK_MM_SMI_LARB0, "mm_smi_larb0", "mm_sel", 1),
	GATE_MM(CLK_MM_GALS_COMM0, "mm_gals_comm0", "mm_sel", 2),
	GATE_MM(CLK_MM_GALS_COMM1, "mm_gals_comm1", "mm_sel", 3),
	GATE_MM(CLK_MM_ISP_DL, "mm_isp_dl", "mm_sel", 4),
	GATE_MM(CLK_MM_MDP_RDMA0, "mm_mdp_rdma0", "mm_sel", 5),
	GATE_MM(CLK_MM_MDP_RSZ0, "mm_mdp_rsz0", "mm_sel", 6),
	GATE_MM(CLK_MM_MDP_RSZ1, "mm_mdp_rsz1", "mm_sel", 7),
	GATE_MM(CLK_MM_MDP_TDSHP, "mm_mdp_tdshp", "mm_sel", 8),
	GATE_MM(CLK_MM_MDP_WROT0, "mm_mdp_wrot0", "mm_sel", 9),
	GATE_MM(CLK_MM_MDP_WDMA0, "mm_mdp_wdma0", "mm_sel", 10),
	GATE_MM(CLK_MM_FAKE_ENG, "mm_fake_eng", "mm_sel", 11),
	GATE_MM(CLK_MM_DISP_OVL0, "mm_disp_ovl0", "mm_sel", 12),
	GATE_MM(CLK_MM_DISP_RDMA0, "mm_disp_rdma0", "mm_sel", 13),
	GATE_MM(CLK_MM_DISP_WDMA0, "mm_disp_wdma0", "mm_sel", 14),
	GATE_MM(CLK_MM_DISP_COLOR0, "mm_disp_color0", "mm_sel", 15),
	GATE_MM(CLK_MM_DISP_CCORR0, "mm_disp_ccorr0", "mm_sel", 16),
	GATE_MM(CLK_MM_DISP_AAL0, "mm_disp_aal0", "mm_sel", 17),
	GATE_MM(CLK_MM_DISP_GAMMA0, "mm_disp_gamma0", "mm_sel", 18),
	GATE_MM(CLK_MM_DISP_DITHER0, "mm_disp_dither0", "mm_sel", 19),
	GATE_MM(CLK_MM_DSI_MM_CLOCK, "mm_dsi_mm_clock", "mm_sel", 20),
	GATE_MM(CLK_MM_DSI_INTERF, "mm_dsi_interf", "mm_sel", 21),
	GATE_MM(CLK_MM_DBI_MM_CLOCK, "mm_dbi_mm_clock", "mm_sel", 22),
	GATE_MM(CLK_MM_DBI_INTERF, "mm_dbi_interf", "dbi0_sel", 23),
	GATE_MM(CLK_MM_F26M_HRT, "mm_f26m_hrt", "clk26m", 24),
	GATE_MM(CLK_MM_CG0_B25, "mm_cg0_b25", "mm_sel", 25),
	GATE_MM(CLK_MM_CG0_B26, "mm_cg0_b26", "mm_sel", 26),
	GATE_MM(CLK_MM_CG0_B27, "mm_cg0_b27", "mm_sel", 27),
	GATE_MM(CLK_MM_CG0_B28, "mm_cg0_b28", "mm_sel", 28),
	GATE_MM(CLK_MM_CG0_B29, "mm_cg0_b29", "mm_sel", 29),
	GATE_MM(CLK_MM_CG0_B30, "mm_cg0_b30", "mm_sel", 30),
	GATE_MM(CLK_MM_CG0_B31, "mm_cg0_b31", "mm_sel", 31),
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
		.ops = &mtk_clk_gate_ops_setclr_inv,			\
	}

static const struct mtk_gate venc_clks[] = {
	GATE_VENC(CLK_VENC_SET0_LARB, "venc_set0_larb", "mm_sel", 0),
	GATE_VENC(CLK_VENC_SET1_VENC, "venc_set1_venc", "mm_sel", 4),
	GATE_VENC(CLK_VENC_SET2_JPGENC, "jpgenc", "mm_sel", 8),
	GATE_VENC(CLK_VENC_SET3_VDEC, "venc_set3_vdec", "mm_sel", 12),
};
/* FIXME: modify FMAX */
#define MT6739_PLL_FMAX		(3000UL * MHZ)
#define MT6739_PLL_FMIN		(1000UL * MHZ)

#define CON0_MT6739_RST_BAR	BIT(24)

#define PLL_B(_id, _name, _reg, _pwr_reg, _en_mask, _flags, \
		_pcwbits, _pd_reg, _pd_shift, _tuner_reg,\
		_pcw_reg, _pcw_shift, _div_table, _pcwintbits,\
		_pcwchgreg) {\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.pwr_reg = _pwr_reg,					\
		.en_mask = _en_mask,					\
		.flags = _flags,					\
		.rst_bar_mask = CON0_MT6739_RST_BAR,			\
		.fmax = MT6739_PLL_FMAX,				\
		.fmin = MT6739_PLL_FMIN,			\
		.pcwbits = _pcwbits,					\
		.pcwibits = _pcwintbits,			\
		.pcw_chg_reg = _pcwchgreg,			\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.div_table = _div_table,				\
	}

#define PLL(_id, _name, _reg, _pwr_reg, _en_mask, _flags, \
		_pcwbits, _pd_reg, _pd_shift, _tuner_reg,\
		_pcw_reg, _pcw_shift, _pcwintbits, _pcwchgreg)\
	PLL_B(_id, _name, _reg, _pwr_reg, _en_mask, _flags, \
		_pcwbits,	_pd_reg, _pd_shift, _tuner_reg,\
		_pcw_reg, _pcw_shift, NULL, _pcwintbits, _pcwchgreg)

static const struct mtk_pll_data plls[] = {
	/* FIXME: need to fix flags/div_table/tuner_reg/table */
#if 1
	PLL(CLK_APMIXED_MAINPLL, "mainpll", 0x0220, 0x022C, 0x80000111,
		HAVE_RST_BAR, 21, 0x0224, 24, 0, 0x0224, 0, 7, 0x4),
#endif
	PLL(CLK_APMIXED_MFGPLL, "mfgpll", 0x0240, 0x024C, 0x40000111, 0,
		21, 0x0244, 24, 0, 0x0244, 0, 7, 0x4),
	PLL(CLK_APMIXED_MMPLL, "mmpll", 0x0270, 0x027C, 0x40000111, 0,
		21, 0x0274, 24, 0, 0x0274, 0, 7, 0x4),
	PLL(CLK_APMIXED_UNIVPLL, "univpll", 0x0230, 0x023C, 0xf0000111,
		HAVE_RST_BAR, 21, 0x0234, 24, 0, 0x0234, 0, 7, 0x4),
	PLL(CLK_APMIXED_MSDCPLL, "msdcpll", 0x0250, 0x025C, 0x40000111, 0,
		21, 0x0254, 24, 0, 0x0254, 0, 7, 0x4),
	PLL(CLK_APMIXED_APLL1, "apll1", 0x02A0, 0x02B0, 0x40000111, 0,
		32, 0x02A0, 1, 0, 0x02A4, 0, 8, 0x0),
};

static int  mtk_apmixed_init(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	void __iomem *base;
	struct device_node *node = pdev->dev.of_node;
	pr_info("%s done\n", __func__);
	base = of_iomap(node, 0);
	if (!base) {
		pr_info("%s(): ioremap failed\n", __func__);
		return -EINVAL;
	}

	clk_data = mtk_alloc_clk_data(CLK_APMIXED_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	/* FIXME: add code for APMIXEDSYS */
	mtk_clk_register_plls(node, plls, ARRAY_SIZE(plls), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_info("%s(): could not register clock provider: %d\n",
			__func__, r);
	apmixed_base = base;

/*MAINPLL*/
	clk_clrl(MAINPLL_CON0, 0x70000000);
	clk_clrl(AP_PLL_CON6, 0x00020000);/* BY_MAINDIV_DLY */
#if 0
	clk_clrl(AP_PLL_CON1, 3 << 6);/*CLKSQ_EN, CLKSQ_LPF HW Mode*/
	clk_writel(AP_PLL_CON3, clk_readl(AP_PLL_CON3) & 0x0E6398E6);/* ARMPLLs, UNIVPLL,EMI SW Mode */
	clk_writel(AP_PLL_CON4, clk_readl(AP_PLL_CON4) & 0x4E6000E6);/* ARMPLLs, UNIVPLL,EMI SW Mode, skip out off */
#endif
#if 0
/*GPUPLL*/
	clk_clrl(MFGPLL_CON0, PLL_EN);
	clk_setl(MFGPLL_PWR_CON0, PLL_ISO_EN);
	clk_clrl(MFGPLL_PWR_CON0, PLL_PWR_ON);
/*UNIVPLL*/
	clk_clrl(UNIVPLL_CON0, PLL_EN);
	clk_setl(UNIVPLL_PWR_CON0, PLL_ISO_EN);
	clk_clrl(UNIVPLL_PWR_CON0, PLL_PWR_ON);
/*MSDCPLL*/
	clk_clrl(MSDCPLL_CON0, PLL_EN);
	clk_setl(MSDCPLL_PWR_CON0, PLL_ISO_EN);
	clk_clrl(MSDCPLL_PWR_CON0, PLL_PWR_ON);
/*APLL1*/
	clk_clrl(APLL1_CON0, PLL_EN);
	clk_setl(APLL1_PWR_CON0, PLL_ISO_EN);
	clk_clrl(APLL1_PWR_CON0, PLL_PWR_ON);
#endif
	pr_info("%s done\n", __func__);
	return r;
}

static int mtk_top_init(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	void __iomem *base, *apmixedbase;
	struct device_node *node = pdev->dev.of_node;

	base = of_iomap(node, 0);
	apmixedbase = of_iomap(node, 1);

	if (!base || !apmixedbase) {
		pr_info("%s(): ioremap failed\n", __func__);
		return -EINVAL;
	}

	clk_data = mtk_alloc_clk_data(CLK_TOP_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_fixed_clks(fixed_clks, ARRAY_SIZE(fixed_clks), clk_data);

	mtk_clk_register_factors_pdn(top_divs, ARRAY_SIZE(top_divs), clk_data, apmixedbase);

	mtk_clk_register_muxes(top_muxes,
		ARRAY_SIZE(top_muxes), node, &mt6739_clk_lock, clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_info("%s(): could not register clock provider: %d\n",
			__func__, r);
	top_base = base;
	clk_writel(CLK_SCP_CFG_0, clk_readl(CLK_SCP_CFG_0) | 0x3EF);/*[10]:no need*/
	clk_writel(CLK_SCP_CFG_1, clk_readl(CLK_SCP_CFG_1) | 0x11);/*[1,2,3,8]: no need*/
	/*mtk_clk_enable_critical();*/
#if 0

	/* PWM7, MFG31 MUX PDN */
	clk_writel(cksys_base + CLK_CFG_1_CLR, 0x00008080);
	clk_writel(cksys_base + CLK_CFG_1_SET, 0x00008080);

	/* msdc50_0_hclk15, msdc50_023 MUX PDN */
	clk_writel(cksys_base + CLK_CFG_3_CLR, 0x00808000);
	clk_writel(cksys_base + CLK_CFG_3_SET, 0x00808000);

	/* msdc30_2 7, msdc30_3 15 MUX PDN */
	clk_writel(cksys_base + CLK_CFG_4_CLR, 0x00008000);
	clk_writel(cksys_base + CLK_CFG_4_SET, 0x00008000);

	/* scp15, atb23 MUX PDN */
	clk_writel(cksys_base + CLK_CFG_5_CLR, 0x80800000);
	clk_writel(cksys_base + CLK_CFG_5_SET, 0x80800000);

	/* dpi0 7, scam 15 MUX PDN */
	clk_writel(cksys_base + CLK_CFG_6_CLR, 0x80008000);
	clk_writel(cksys_base + CLK_CFG_6_SET, 0x80008000);

	/* ssusb_top_sys 15, ssusb_top_xhci 23 MUX PDN */
	clk_writel(cksys_base + CLK_CFG_7_CLR, 0x80800080);
	clk_writel(cksys_base + CLK_CFG_7_SET, 0x80800080);

	clk_writel(cksys_base + CLK_CFG_8_CLR, 0x00808080);
	clk_writel(cksys_base + CK_CFG_8_SET, 0x00808080);

	clk_writel(cksys_base + CLK_CFG_9_CLR, 0x80800080);
	clk_writel(cksys_base + CLK_CFG_9_SET, 0x80800080);

	clk_writel(cksys_base + CLK_CFG_10_CLR, 0x00008000);
	clk_writel(cksys_base + CLK_CFG_10_SET, 0x00008000);
#endif
	pr_info("%s done\n", __func__);
	return r;
}


static int mtk_infra_init(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;
	struct device_node *node = pdev->dev.of_node;

	base = of_iomap(node, 0);
	if (!base) {
		pr_info("%s(): ioremap failed\n", __func__);
		return -EINVAL;
	}

	clk_data = mtk_alloc_clk_data(CLK_INFRA_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_gates(node, infra_clks, ARRAY_SIZE(infra_clks),
		clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_info("%s(): could not register clock provider: %d\n",
			__func__, r);
	infra_base = base;
	/* clk_writel(INFRA_TOPAXI_SI0_CTL, clk_readl(INFRA_TOPAXI_SI0_CTL) | 0x2);*//*CDC, MFG issue*/
	/*mtk_clk_enable_critical();*/
#if MT_CCF_BRINGUP
	clk_writel(MODULE_SW_CG_0_CLR, INFRA_CG0);
	clk_writel(MODULE_SW_CG_1_CLR, INFRA_CG1);
	clk_writel(MODULE_SW_CG_2_CLR, INFRA_CG2);
	clk_writel(MODULE_SW_CG_3_CLR, INFRA_CG3);
#else
	clk_writel(MODULE_SW_CG_0_SET, INFRA_CG0);
	clk_writel(MODULE_SW_CG_1_SET, INFRA_CG1);
	clk_writel(MODULE_SW_CG_2_SET, INFRA_CG2);
	clk_writel(MODULE_SW_CG_3_SET, INFRA_CG3);
#endif
	pr_info("%s done\n", __func__);
	return r;
}

static int mtk_audio_init(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;
	struct device_node *node = pdev->dev.of_node;
	
	base = of_iomap(node, 0);
	if (!base) {
		pr_info("%s(): ioremap failed\n", __func__);
		return -EINVAL;
	}

	clk_data = mtk_alloc_clk_data(CLK_AUDIO_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_gates(node, audio_clks, ARRAY_SIZE(audio_clks),
		clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_info("%s(): could not register clock provider: %d\n",
			__func__, r);
	audio_base = base;

#if MT_CCF_BRINGUP /*MT_CCF_BRINGUP*/
	clk_writel(AUDIO_TOP_CON0, clk_readl(AUDIO_TOP_CON0) & AUDIO_DISABLE_CG0);
	clk_writel(AUDIO_TOP_CON1, clk_readl(AUDIO_TOP_CON1) & AUDIO_DISABLE_CG1);
#else
/*	clk_writel(AUDIO_TOP_CON0, clk_readl(AUDIO_TOP_CON0) | ~AUDIO_DISABLE_CG0);*/
/*	clk_writel(AUDIO_TOP_CON1, clk_readl(AUDIO_TOP_CON1) | ~AUDIO_DISABLE_CG1);*/
#endif

	pr_info("%s done\n", __func__);
	return r;
}


static int mtk_mm_init(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;
	struct device_node *node = pdev->dev.of_node;

	base = of_iomap(node, 0);
	if (!base) {
		pr_info("%s(): ioremap failed\n", __func__);
		return -EINVAL;
	}

	clk_data = mtk_alloc_clk_data(CLK_MM_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_gates(node, mm_clks, ARRAY_SIZE(mm_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_info("%s(): could not register clock provider: %d\n",
			__func__, r);
	mm_base = base;
#if MT_CCF_BRINGUP
	clk_writel(MM_CG_CLR0, MM_DISABLE_CG0);
#else
	/*won't touch MMSYS*/
#endif
	pr_info("%s done\n", __func__);
	return r;
}

static int  mtk_img_init(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;
	struct device_node *node = pdev->dev.of_node;

	base = of_iomap(node, 0);
	if (!base) {
		pr_info("%s(): ioremap failed\n", __func__);
		return -EINVAL;
	}

	clk_data = mtk_alloc_clk_data(CLK_IMG_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_gates(node, img_clks, ARRAY_SIZE(img_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_info("%s(): could not register clock provider: %d\n",
			__func__, r);
	img_base = base;

#if MT_CCF_BRINGUP
	clk_writel(IMG_CG_CLR, IMG_DISABLE_CG);
#else
	clk_writel(IMG_CG_SET, IMG_DISABLE_CG);
#endif
	pr_info("%s done\n", __func__);
	return r;
}


static int  mtk_venc_init(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	void __iomem *base;
	int r;
	struct device_node *node = pdev->dev.of_node;

	base = of_iomap(node, 0);
	if (!base) {
		pr_info("%s(): ioremap failed\n", __func__);
		return -EINVAL;
	}

	clk_data = mtk_alloc_clk_data(CLK_VENC_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_gates(node, venc_clks, ARRAY_SIZE(venc_clks),
		clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_info("%s(): could not register clock provider: %d\n",
			__func__, r);
	venc_base = base;

#if MT_CCF_BRINGUP
	clk_writel(VCODECSYS_CG_SET, VEN_DISABLE_CG);
#else
	clk_writel(VCODECSYS_CG_CLR, VEN_DISABLE_CG);
#endif
	pr_info("%s done\n", __func__);
	return r;
}


unsigned int mt_get_ckgen_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned int temp, clk26cali_0, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1;

	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	clk_writel(CLK_DBG_CFG,
		(clk_dbg_cfg & 0xFFFFC0FC)|(ID << 8)|(0x1));

	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF)|0x00000010);
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

	output = ((temp * 26000)) / 1024;

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0|0x00000010);
	clk_writel(CLK26CALI_0, clk26cali_0);
	clk_writel(CLK26CALI_1, clk26cali_1);

	/*pr_debug("%s = %d Khz\n", ckgen_array[ID-1], output);*/
	if (i > 10000)
		return 0;
	else
		return output;
}

unsigned int mt_get_abist_freq(unsigned int ID)
{
	int output = 0, i = 0;
	unsigned int temp, clk26cali_0, clk_dbg_cfg, clk_misc_cfg_0, clk26cali_1;

	clk_dbg_cfg = clk_readl(CLK_DBG_CFG);
	clk_writel(CLK_DBG_CFG, (clk_dbg_cfg & 0xFFC0FFFC)|(ID << 16));
	clk_misc_cfg_0 = clk_readl(CLK_MISC_CFG_0);
	clk_writel(CLK_MISC_CFG_0, (clk_misc_cfg_0 & 0x00FFFFFF)|0x00000010);
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

	output = ((temp * 26000)) / 1024;

	clk_writel(CLK_DBG_CFG, clk_dbg_cfg);
	clk_writel(CLK_MISC_CFG_0, clk_misc_cfg_0|0x00000010);
	clk_writel(CLK26CALI_0, clk26cali_0);
	clk_writel(CLK26CALI_1, clk26cali_1);

	/*pr_debug("%s = %d Khz\n", abist_array[ID-1], output);*/
	if (i > 10000)
		return 0;
	else
		return output;
}

void pll_if_on(void)
{
	if (clk_readl(UNIVPLL_CON0) & 0x1)
		pr_debug("suspend warning: UNIVPLL is on!!!\n");
	if (clk_readl(MFGPLL_CON0) & 0x1)
		pr_debug("suspend warning: MFGPLL is on!!!\n");
	if (clk_readl(MMPLL_CON0) & 0x1)
		pr_debug("suspend warning: MMPLL is on!!!\n");
	if (clk_readl(MSDCPLL_CON0) & 0x1)
		pr_debug("suspend warning: MSDCPLL is on!!!\n");
	if (clk_readl(APLL1_CON0) & 0x1)
		pr_debug("suspend warning: APLL1 is on!!!\n");

#if 0
	pr_debug("%s: AP_PLL_CON3 = 0x%08x\r\n", __func__, clk_readl(AP_PLL_CON3));
	pr_debug("%s: AP_PLL_CON4 = 0x%08x\r\n", __func__, clk_readl(AP_PLL_CON4));
	pr_debug("%s: ARMPLL = %dHZ\r\n", __func__, mt_get_abist_freq(22));
	pr_debug("%s: UNIVPLL = %dHZ\r\n", __func__, mt_get_abist_freq(24));
	pr_debug("%s: MFGPLL = %dHZ\r\n", __func__, mt_get_abist_freq(27));
	pr_debug("%s: MMPLL = %dHZ\r\n", __func__, mt_get_abist_freq(34));
	pr_debug("%s: MSDCPLL = %dHZ\r\n", __func__, mt_get_abist_freq(26));
	pr_debug("%s: APLL1 = %dHZ\r\n", __func__, mt_get_abist_freq(28));
#endif
}

void clock_force_off(void)
{
	/*DISP CG*/
	clk_writel(MM_CG_SET0, MM_DISABLE_CG0);
	/*ISP*/
	clk_writel(IMG_CG_SET, IMG_DISABLE_CG);
	/*VENC inverse*/
	clk_writel(VCODECSYS_CG_CLR, VEN_DISABLE_CG);
	/*AUDIO*/
	clk_writel(AUDIO_TOP_CON0, 0xF080104);
	clk_writel(AUDIO_TOP_CON1, 0xF0);
}

void pll_force_off(void)
{
/*MFGPLL*/
	clk_clrl(MFGPLL_CON0, PLL_EN);
	clk_setl(MFGPLL_PWR_CON0, PLL_ISO_EN);
	clk_clrl(MFGPLL_PWR_CON0, PLL_PWR_ON);
/*UNIVPLL*/
	clk_clrl(UNIVPLL_CON0, PLL_EN);
	clk_setl(UNIVPLL_PWR_CON0, PLL_ISO_EN);
	clk_clrl(UNIVPLL_PWR_CON0, PLL_PWR_ON);
/*MSDCPLL*/
	clk_clrl(MSDCPLL_CON0, PLL_EN);
	clk_setl(MSDCPLL_PWR_CON0, PLL_ISO_EN);
	clk_clrl(MSDCPLL_PWR_CON0, PLL_PWR_ON);
/*MMPLL*/
	clk_clrl(MMPLL_CON0, PLL_EN);
	clk_setl(MMPLL_PWR_CON0, PLL_ISO_EN);
	clk_clrl(MMPLL_PWR_CON0, PLL_PWR_ON);
/*APLL1*/
	clk_clrl(APLL1_CON0, PLL_EN);
	clk_setl(APLL1_PWR_CON0, PLL_ISO_EN);
	clk_clrl(APLL1_PWR_CON0, PLL_PWR_ON);
}

void all_force_off(void)
{
	/*CG*/
	clock_force_off();

	/*mtcmos*/
	mtcmos_force_off();

	/*pll*/
	pll_force_off();
}

void check_mm0_clk_sts(void)
{
	/* confirm mm0 clk */
	pr_debug("%s: MM_CG = 0x%08x\n", __func__, clk_readl(MM_CG_CON0));
	pr_debug("%s: CLK_CFG_0 = 0x%08x\r\n", __func__, clk_readl(CLK_MODE + CLK_CFG_0));
	pr_debug("%s: MMPLL_CON0 = 0x%08x, 0x%08x\r\n", __func__, clk_readl(MMPLL_CON0), clk_readl(MMPLL_CON1));
	pr_debug("%s: MMPLL_PWR_CON0 = 0x%08x\r\n", __func__, clk_readl(MMPLL_PWR_CON0));
	pr_debug("%s: mmck = %dkhz\r\n", __func__, mt_get_ckgen_freq(3));
	pr_debug("%s: mmpll = %dkhz\r\n", __func__, mt_get_abist_freq(34));
}

void check_img_clk_sts(void)
{
	/* confirm mm0 clk */
	pr_debug("IMG_CG_CON = 0x%08x\n", clk_readl(IMG_CG_CON));
}

void check_ven_clk_sts(void)
{
	/* confirm mm0 clk */
	pr_debug("VCODECSYS_CG_CON = 0x%08x\n", clk_readl(VCODECSYS_CG_CON));
}

static const struct of_device_id of_match_clk_mt6739[] = {
	{
		.compatible = "mediatek,apmixed",
		.data = mtk_apmixed_init,
	}, {
		.compatible = "mediatek,topckgen",
		.data = mtk_top_init,
	}, {
		.compatible = "mediatek,infracfg_ao",
		.data = mtk_infra_init,
	}
};

static const struct of_device_id of_match_clk_mt6739_subsys[] = {
	{
		.compatible = "mediatek,mt6739-audsys",
		.data = mtk_audio_init,

	},{
		.compatible = "mediatek,mmsys_config",
		.data = mtk_mm_init,
	},{
		.compatible = "mediatek,imgsys",
		.data = mtk_img_init,
	},{
		.compatible = "mediatek,venc_global_con",
		.data = mtk_venc_init,
	}

};

static int clk_mt6739_probe(struct platform_device *pdev)
{
	int (*clk_probe)(struct platform_device *d);
	int r;

	clk_probe = of_device_get_match_data(&pdev->dev);
	if (!clk_probe)
		return -EINVAL;

	r = clk_probe(pdev);
	if (r)
		pr_debug("could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static int clk_mt6739_subsys_probe(struct platform_device *pdev)
{
	int (*clk_probe)(struct platform_device *d);
	int r;

	clk_probe = of_device_get_match_data(&pdev->dev);
	if (!clk_probe)
		return -EINVAL;

	r = clk_probe(pdev);
	if (r)
		pr_debug("could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static struct platform_driver clk_mt6739_drv = {
	.probe = clk_mt6739_probe,
	.driver = {
		.name = "clk-mt6739",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt6739,
	},
};

static struct platform_driver clk_mt6739_subsys_drv = {
	.probe = clk_mt6739_subsys_probe,
	.driver = {
		.name = "clk-mt6739-subsys",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt6739_subsys,
	},
};

static int __init clk_mt6739_init(void)
{
	return platform_driver_register(&clk_mt6739_drv);
}

static int __init clk_mt6739_subsys_init(void)
{
	return platform_driver_register(&clk_mt6739_subsys_drv);
}
static void __exit clk_mt6739_exit(void)
{
	pr_debug("%s\n", __func__);
}


postcore_initcall_sync(clk_mt6739_init);
arch_initcall(clk_mt6739_subsys_init);
module_exit(clk_mt6739_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("MTK");
MODULE_DESCRIPTION("MTK CCF  Driver v1.0");

