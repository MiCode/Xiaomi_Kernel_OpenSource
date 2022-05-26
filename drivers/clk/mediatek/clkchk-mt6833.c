// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>

#include <dt-bindings/power/mt6833-power.h>

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
#include <devapc_public.h>
#endif

#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER)
#include <mt-plat/dvfsrc-exp.h>
#endif

#include "clkchk.h"
#include "clkchk-mt6833.h"

#define BUG_ON_CHK_ENABLE		0
#define CHECK_VCORE_FREQ		0

/*
 * clkchk dump_regs
 */

#define REGBASE_V(_phys, _id_name, _pg, _pn) { .phys = _phys,	\
		.name = #_id_name, .pg = _pg, .pn = _pn}

static struct regbase rb[] = {
	[top] = REGBASE_V(0x10000000, top, PD_NULL, CLK_NULL),
	[ifrao] = REGBASE_V(0x10001000, ifrao, PD_NULL, CLK_NULL),
	[infracfg] = REGBASE_V(0x10001000, infracfg, PD_NULL, CLK_NULL),
	[peri] = REGBASE_V(0x10003000, peri, PD_NULL, CLK_NULL),
	[spm] = REGBASE_V(0x10006000, spm, PD_NULL, CLK_NULL),
	[apmixed] = REGBASE_V(0x1000C000, apmixed, PD_NULL, CLK_NULL),
	[ifr] = REGBASE_V(0x1020e000, ifr, PD_NULL, CLK_NULL),
	[impc] = REGBASE_V(0x11007000, impc, PD_NULL, "i2c_sel"),
	[afe] = REGBASE_V(0x11210000, afe, MT6833_POWER_DOMAIN_AUDIO, CLK_NULL),
	[msdc0] = REGBASE_V(0x11230000, msdc0, PD_NULL, CLK_NULL),
	[impe] = REGBASE_V(0x11CB1000, impe, PD_NULL, "i2c_sel"),
	[imps] = REGBASE_V(0x11D02000, imps, PD_NULL, "i2c_sel"),
	[impws] = REGBASE_V(0x11D23000, impws, PD_NULL, "i2c_sel"),
	[impw] = REGBASE_V(0x11E03000, impw, PD_NULL, "i2c_sel"),
	[impn] = REGBASE_V(0x11F01000, impn, PD_NULL, "i2c_sel"),
	[mfgcfg] = REGBASE_V(0x13fbf000, mfgcfg, MT6833_POWER_DOMAIN_MFG3, CLK_NULL),
	[mm] = REGBASE_V(0x14000000, mm, MT6833_POWER_DOMAIN_DISP, CLK_NULL),
	[imgsys1] = REGBASE_V(0x15020000, imgsys1, MT6833_POWER_DOMAIN_ISP, CLK_NULL),
	[imgsys2] = REGBASE_V(0x15820000, imgsys2, MT6833_POWER_DOMAIN_ISP2, CLK_NULL),
	[vde2] = REGBASE_V(0x1602f000, vde2, MT6833_POWER_DOMAIN_VDEC, CLK_NULL),
	[ven1] = REGBASE_V(0x17000000, ven1, MT6833_POWER_DOMAIN_VENC, CLK_NULL),
	[cam_m] = REGBASE_V(0x1a000000, cam_m, MT6833_POWER_DOMAIN_CAM, CLK_NULL),
	[cam_ra] = REGBASE_V(0x1a04f000, cam_ra, MT6833_POWER_DOMAIN_CAM_RAWA, CLK_NULL),
	[cam_rb] = REGBASE_V(0x1a06f000, cam_rb, MT6833_POWER_DOMAIN_CAM_RAWB, CLK_NULL),
	[ipe] = REGBASE_V(0x1b000000, ipe, MT6833_POWER_DOMAIN_IPE, CLK_NULL),
	[mdp] = REGBASE_V(0x1f000000, mdp, MT6833_POWER_DOMAIN_DISP, CLK_NULL),
	{},
};

#define REGNAME(_base, _ofs, _name)	\
	{ .base = &rb[_base], .ofs = _ofs, .name = #_name }

static struct regname rn[] = {
	/* TOPCKGEN register */
	REGNAME(top, 0x0010, CLK_CFG_0),
	REGNAME(top, 0x0020, CLK_CFG_1),
	REGNAME(top, 0x0030, CLK_CFG_2),
	REGNAME(top, 0x0050, CLK_CFG_4),
	REGNAME(top, 0x0060, CLK_CFG_5),
	REGNAME(top, 0x0070, CLK_CFG_6),
	REGNAME(top, 0x0080, CLK_CFG_7),
	REGNAME(top, 0x0090, CLK_CFG_8),
	REGNAME(top, 0x00A0, CLK_CFG_9),
	REGNAME(top, 0x00B0, CLK_CFG_10),
	REGNAME(top, 0x00C0, CLK_CFG_11),
	REGNAME(top, 0x00D0, CLK_CFG_12),
	REGNAME(top, 0x00E0, CLK_CFG_13),
	REGNAME(top, 0x00F0, CLK_CFG_14),
	REGNAME(top, 0x0100, CLK_CFG_15),
	REGNAME(top, 0x0110, CLK_CFG_16),
	REGNAME(top, 0x0320, CLK_AUDDIV_0),
	REGNAME(top, 0x0328, CLK_AUDDIV_2),
	REGNAME(top, 0x0334, CLK_AUDDIV_3),
	REGNAME(top, 0x0338, CLK_AUDDIV_4),
	/* INFRACFG_AO register */
	REGNAME(ifrao, 0x70, INFRA_BUS_DCM_CTRL),
	REGNAME(ifrao, 0x90, MODULE_CG_0),
	REGNAME(ifrao, 0x94, MODULE_CG_1),
	REGNAME(ifrao, 0xAC, MODULE_CG_2),
	REGNAME(ifrao, 0xC8, MODULE_CG_3),
	REGNAME(ifrao, 0xE8, MODULE_CG_4),
	REGNAME(ifrao, 0xd8, MODULE_SW_CG_5),
	REGNAME(ifrao, 0x74, PERI_BUS_DCM_CTRL),
	/* INFRACFG_AO_BUS register */
	REGNAME(infracfg, 0x0220, INFRA_TOPAXI_PROTECTEN),
	REGNAME(infracfg, 0x0228, INFRA_TOPAXI_PROTECTEN_STA1),
	REGNAME(infracfg, 0x0B80, INFRA_TOPAXI_PROTECTEN_VDNR),
	REGNAME(infracfg, 0x0B90, INFRA_TOPAXI_PROTECTEN_VDNR_STA1),
	REGNAME(infracfg, 0x0250, INFRA_TOPAXI_PROTECTEN_1),
	REGNAME(infracfg, 0x0258, INFRA_TOPAXI_PROTECTEN_STA1_1),
	REGNAME(infracfg, 0x0710, INFRA_TOPAXI_PROTECTEN_2),
	REGNAME(infracfg, 0x0724, INFRA_TOPAXI_PROTECTEN_STA1_2),
	REGNAME(infracfg, 0x0DC8, INFRA_TOPAXI_PROTECTEN_MM_2),
	REGNAME(infracfg, 0x0DD8, INFRA_TOPAXI_PROTECTEN_MM_STA1_2),
	REGNAME(infracfg, 0x02D0, INFRA_TOPAXI_PROTECTEN_MM),
	REGNAME(infracfg, 0x02EC, INFRA_TOPAXI_PROTECTEN_MM_STA1),
	/* PERICFG register */
	REGNAME(peri, 0x20C, PERIAXI_SI0_CTL),
	/* SPM register */
	REGNAME(spm, 0x300, MD1_PWR_CON),
	REGNAME(spm, 0x16C, PWR_STATUS),
	REGNAME(spm, 0x170, PWR_STATUS_2ND),
	REGNAME(spm, 0x398, MD_EXT_BUCK_ISO_CON),
	REGNAME(spm, 0x304, CONN_PWR_CON),
	REGNAME(spm, 0x308, MFG0_PWR_CON),
	REGNAME(spm, 0x30C, MFG1_PWR_CON),
	REGNAME(spm, 0x310, MFG2_PWR_CON),
	REGNAME(spm, 0x314, MFG3_PWR_CON),
	REGNAME(spm, 0x334, ISP_PWR_CON),
	REGNAME(spm, 0x338, ISP2_PWR_CON),
	REGNAME(spm, 0x33C, IPE_PWR_CON),
	REGNAME(spm, 0x340, VDE_PWR_CON),
	REGNAME(spm, 0x348, VEN_PWR_CON),
	REGNAME(spm, 0x354, DIS_PWR_CON),
	REGNAME(spm, 0x358, AUDIO_PWR_CON),
	REGNAME(spm, 0x35C, CAM_PWR_CON),
	REGNAME(spm, 0x360, CAM_RAWA_PWR_CON),
	REGNAME(spm, 0x364, CAM_RAWB_PWR_CON),
	REGNAME(spm, 0x670, SPM_CROSS_WAKE_M01_REQ),
	REGNAME(spm, 0x178, OTHER_PWR_STATUS),
	/* APMIXEDSYS register */
	REGNAME(apmixed, 0x208, ARMPLL_LL_CON0),
	REGNAME(apmixed, 0x20c, ARMPLL_LL_CON1),
	REGNAME(apmixed, 0x210, ARMPLL_LL_CON2),
	REGNAME(apmixed, 0x214, ARMPLL_LL_CON3),
	REGNAME(apmixed, 0x218, ARMPLL_BL0_CON0),
	REGNAME(apmixed, 0x21c, ARMPLL_BL0_CON1),
	REGNAME(apmixed, 0x220, ARMPLL_BL0_CON2),
	REGNAME(apmixed, 0x224, ARMPLL_BL0_CON3),
	REGNAME(apmixed, 0x258, CCIPLL_CON0),
	REGNAME(apmixed, 0x25c, CCIPLL_CON1),
	REGNAME(apmixed, 0x260, CCIPLL_CON2),
	REGNAME(apmixed, 0x264, CCIPLL_CON3),
	REGNAME(apmixed, 0x390, MPLL_CON0),
	REGNAME(apmixed, 0x394, MPLL_CON1),
	REGNAME(apmixed, 0x398, MPLL_CON2),
	REGNAME(apmixed, 0x39c, MPLL_CON3),
	REGNAME(apmixed, 0x340, MAINPLL_CON0),
	REGNAME(apmixed, 0x344, MAINPLL_CON1),
	REGNAME(apmixed, 0x348, MAINPLL_CON2),
	REGNAME(apmixed, 0x34c, MAINPLL_CON3),
	REGNAME(apmixed, 0x308, UNIVPLL_CON0),
	REGNAME(apmixed, 0x30c, UNIVPLL_CON1),
	REGNAME(apmixed, 0x310, UNIVPLL_CON2),
	REGNAME(apmixed, 0x314, UNIVPLL_CON3),
	REGNAME(apmixed, 0x350, MSDCPLL_CON0),
	REGNAME(apmixed, 0x354, MSDCPLL_CON1),
	REGNAME(apmixed, 0x358, MSDCPLL_CON2),
	REGNAME(apmixed, 0x35c, MSDCPLL_CON3),
	REGNAME(apmixed, 0x360, MMPLL_CON0),
	REGNAME(apmixed, 0x364, MMPLL_CON1),
	REGNAME(apmixed, 0x368, MMPLL_CON2),
	REGNAME(apmixed, 0x36c, MMPLL_CON3),
	REGNAME(apmixed, 0x370, ADSPPLL_CON0),
	REGNAME(apmixed, 0x374, ADSPPLL_CON1),
	REGNAME(apmixed, 0x378, ADSPPLL_CON2),
	REGNAME(apmixed, 0x37c, ADSPPLL_CON3),
	REGNAME(apmixed, 0x268, MFGPLL_CON0),
	REGNAME(apmixed, 0x26c, MFGPLL_CON1),
	REGNAME(apmixed, 0x270, MFGPLL_CON2),
	REGNAME(apmixed, 0x274, MFGPLL_CON3),
	REGNAME(apmixed, 0x380, TVDPLL_CON0),
	REGNAME(apmixed, 0x384, TVDPLL_CON1),
	REGNAME(apmixed, 0x388, TVDPLL_CON2),
	REGNAME(apmixed, 0x38c, TVDPLL_CON3),
	REGNAME(apmixed, 0x318, APLL1_CON0),
	REGNAME(apmixed, 0x31c, APLL1_CON1),
	REGNAME(apmixed, 0x320, APLL1_CON2),
	REGNAME(apmixed, 0x324, APLL1_CON3),
	REGNAME(apmixed, 0x328, APLL1_CON4),
	REGNAME(apmixed, 0x0040, APLL1_TUNER_CON0),
	REGNAME(apmixed, 0x000C, AP_PLL_CON3),
	REGNAME(apmixed, 0x32c, APLL2_CON0),
	REGNAME(apmixed, 0x330, APLL2_CON1),
	REGNAME(apmixed, 0x334, APLL2_CON2),
	REGNAME(apmixed, 0x338, APLL2_CON3),
	REGNAME(apmixed, 0x33c, APLL2_CON4),
	REGNAME(apmixed, 0x0044, APLL2_TUNER_CON0),
	REGNAME(apmixed, 0x000C, AP_PLL_CON3),
	REGNAME(apmixed, 0x3b4, NPUPLL_CON0),
	REGNAME(apmixed, 0x3b8, NPUPLL_CON1),
	REGNAME(apmixed, 0x3bc, NPUPLL_CON2),
	REGNAME(apmixed, 0x3c0, NPUPLL_CON3),
	REGNAME(apmixed, 0x3c4, USBPLL_CON0),
	REGNAME(apmixed, 0x3c8, USBPLL_CON1),
	REGNAME(apmixed, 0x3cc, USBPLL_CON2),
	REGNAME(apmixed, 0x14, AP_PLL_5),
	/* INFRACFG register */
	REGNAME(ifr, 0xB00, BUS_MON_CKEN),
	/* IMP_IIC_WRAP_C register */
	REGNAME(impc, 0xE00, AP_CLOCK_CG_CEN),
	/* AFE register */
	REGNAME(afe, 0x0, AUDIO_TOP_0),
	REGNAME(afe, 0x4, AUDIO_TOP_1),
	REGNAME(afe, 0x8, AUDIO_TOP_2),
	/* MSDC0 register */
	REGNAME(msdc0, 0xB4, PATCH_BIT1),
	/* IMP_IIC_WRAP_E register */
	REGNAME(impe, 0xE00, AP_CLOCK_CG_EST),
	/* IMP_IIC_WRAP_S register */
	REGNAME(imps, 0xE00, AP_CLOCK_CG_SOU),
	/* IMP_IIC_WRAP_WS register */
	REGNAME(impws, 0xE00, AP_CLOCK_CG_WEST_SOU),
	/* IMP_IIC_WRAP_W register */
	REGNAME(impw, 0xE00, AP_CLOCK_CG_WST),
	/* IMP_IIC_WRAP_N register */
	REGNAME(impn, 0xE00, AP_CLOCK_CG_NOR),
	/* MFG_TOP_CONFIG register */
	REGNAME(mfgcfg, 0x0, MFG_CG),
	/* DISPSYS_CONFIG register */
	REGNAME(mm, 0x100, MMSYS_CG_0),
	REGNAME(mm, 0x1A0, MMSYS_CG_2),
	/* IMGSYS1 register */
	REGNAME(imgsys1, 0x0, IMG_CG),
	/* IMGSYS2 register */
	REGNAME(imgsys2, 0x0, IMG_CG),
	/* VDEC_GCON_BASE register */
	REGNAME(vde2, 0x8, LARB_CKEN_CON),
	REGNAME(vde2, 0x0, VDEC_CKEN),
	/* VENC_GCON register */
	REGNAME(ven1, 0x0, VENCSYS_CG),
	/* CAMSYS_MAIN register */
	REGNAME(cam_m, 0x0, CAMSYS_CG),
	/* CAMSYS_RAWA register */
	REGNAME(cam_ra, 0x0, CAMSYS_CG),
	/* CAMSYS_RAWB register */
	REGNAME(cam_rb, 0x0, CAMSYS_CG),
	/* IPESYS register */
	REGNAME(ipe, 0x0, IMG_CG),
	/* MDPSYS_CONFIG register */
	REGNAME(mdp, 0x100, MDPSYS_CG_0),
	REGNAME(mdp, 0x120, MDPSYS_CG_2),
	{},
};

static const struct regname *get_all_mt6833_regnames(void)
{
	return rn;
}

static void init_regbase(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rb) - 1; i++) {
		if (!rb[i].phys)
			continue;

		rb[i].virt = ioremap(rb[i].phys, 0x1000);
	}
}

/*
 * clkchk pwr_status
 */
static u32 pwr_ofs[STA_NUM] = {
	[PWR_STA] = 0x16C,
	[PWR_STA2] = 0x170,
	[OTHER_STA] = 0x178,
};

static u32 pwr_sta[STA_NUM];

u32 *get_spm_pwr_status_array(void)
{
	static void __iomem *pwr_addr[STA_NUM];
	int i;

	for (i = 0; i < STA_NUM; i++) {
		if (pwr_ofs[i]) {
			pwr_addr[i] = rb[spm].virt + pwr_ofs[i];
			pwr_sta[i] = clk_readl(pwr_addr[i]);
		}
	}

	return pwr_sta;
}

/*
 * clkchk pwr_msk
 */
static struct pvd_msk pvd_pwr_mask[] = {
	{"topckgen", PWR_STA, 0x00000000},
	{"apmixedsys", PWR_STA, 0x00000000},
	{"afe", PWR_STA, 0x00400000},
	{"camsys_main", PWR_STA, 0x00800000},
	{"camsys_rawa", PWR_STA, 0x01000000},
	{"camsys_rawb", PWR_STA, 0x02000000},
	{"mmsys", PWR_STA, 0x00200000},
	{"imgsys1", PWR_STA, 0x00002000},
	{"imgsys2", PWR_STA, 0x00004000},
	{"imp_iic_wrap_c", PWR_STA, 0x00000000},
	{"imp_iic_wrap_e", PWR_STA, 0x00000000},
	{"imp_iic_wrap_n", PWR_STA, 0x00000000},
	{"imp_iic_wrap_s", PWR_STA, 0x00000000},
	{"imp_iic_wrap_w", PWR_STA, 0x00000000},
	{"imp_iic_wrap_ws", PWR_STA, 0x00000000},
	{"infracfg", PWR_STA, 0x00000000},
	{"infracfg_ao", PWR_STA, 0x00000000},
	{"ipesys", PWR_STA, 0x00008000},
	{"mdpsys_config", PWR_STA, 0x00200000},
	{"mfg", PWR_STA, 0x00000000},
	{"msdc0sys", PWR_STA, 0x00000000},
	{"pericfg", PWR_STA, 0x00000000},
	{"vdec_gcon_base", PWR_STA, 0x00010000},
	{"vencsys", PWR_STA, 0x00040000},
	{},
};

static struct pvd_msk *get_pvd_pwr_mask(void)
{
	return pvd_pwr_mask;
}

/*
 * clkchk vf table
 */

/*
 * Opp0 : 0p725v
 * Opp1 : 0p65v
 * Opp2 : 0p60v
 * Opp3 : 0p55v
 */
#if CHECK_VCORE_FREQ
static struct mtk_vf vf_table[] = {
	/* Opp0, Opp1, Opp2, Opp3 */
	MTK_VF_TABLE("axi_sel", 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("spm_sel", 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("scp_sel", 624000, 416000, 364000, 273000),
	MTK_VF_TABLE("bus_aximem_sel", 364000, 273000, 273000, 218400),
	MTK_VF_TABLE("disp_sel", 546000, 416000, 312000, 208000),
	MTK_VF_TABLE("mdp_sel", 594000, 436800, 343750, 275000),
	MTK_VF_TABLE("img1_sel", 624000, 458333, 343750, 275000),
	MTK_VF_TABLE("img2_sel", 624000, 458333, 343750, 275000),
	MTK_VF_TABLE("ipe_sel", 546000, 416000, 312000, 275000),
	MTK_VF_TABLE("dpe_sel", 546000, 458333, 364000, 249600),
	MTK_VF_TABLE("cam_sel", 624000, 546000, 392857, 385000),
	MTK_VF_TABLE("ccu_sel", 499200, 392857, 364000, 275000),
	MTK_VF_TABLE("mfg_ref_sel", 364000, 364000, 364000, 364000),
	MTK_VF_TABLE("camtg_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg2_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg3_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg4_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg5_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("uart_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("spi_sel", 192000, 192000, 192000, 192000),
	MTK_VF_TABLE("msdc5hclk_sel", 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("msdc50_0_sel", 384000, 384000, 384000, 384000),
	MTK_VF_TABLE("msdc30_1_sel", 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("audio_sel", 54600, 54600, 54600, 54600),
	MTK_VF_TABLE("aud_intbus_sel", 136500, 136500, 136500, 136500),
	MTK_VF_TABLE("pwrap_ulposc_sel", 65000, 65000, 65000, 65000),
	MTK_VF_TABLE("atb_sel", 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("sspm_sel", 273000, 242667, 218400, 182000),
	MTK_VF_TABLE("scam_sel", 109200, 109200, 109200, 109200),
	MTK_VF_TABLE("disp_pwm_sel", 130000, 130000, 130000, 130000),
	MTK_VF_TABLE("usb_sel", 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("ssusb_xhci_sel", 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("i2c_sel", 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("seninf_sel", 499200, 499200, 392857, 385000),
	MTK_VF_TABLE("seninf1_sel", 499200, 499200, 392857, 385000),
	MTK_VF_TABLE("seninf2_sel", 499200, 499200, 392857, 385000),
	MTK_VF_TABLE("dxcc_sel", 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("aud_engen1_sel", 22579, 22579, 22579, 22579),
	MTK_VF_TABLE("aud_engen2_sel", 24576, 24576, 24576, 24576),
	MTK_VF_TABLE("aes_ufsfde_sel", 546000, 546000, 546000, 416000),
	MTK_VF_TABLE("ufs_sel", 192000, 192000, 192000, 192000),
	MTK_VF_TABLE("aud_1_sel", 180634, 180634, 180634, 180634),
	MTK_VF_TABLE("aud_2_sel", 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("adsp_sel", 385000, 385000, 385000, 385000),
	MTK_VF_TABLE("dpmaif_main_sel", 364000, 364000, 364000, 273000),
	MTK_VF_TABLE("venc_sel", 624000, 458333, 364000, 249600),
	MTK_VF_TABLE("vdec_sel", 546000, 416000, 312000, 218400),
	MTK_VF_TABLE("camtm_sel", 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("pwm_sel", 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("audio_h_sel", 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("spmi_mst_sel", 32500, 32500, 32500, 32500),
	MTK_VF_TABLE("dvfsrc_sel", 26000, 26000, 26000, 26000),
	MTK_VF_TABLE("aes_msdcfde_sel", 416000, 416000, 416000, 416000),
	MTK_VF_TABLE("mcupm_sel", 182000, 182000, 182000, 182000),
	MTK_VF_TABLE("sflash_sel", 62400, 62400, 62400, 62400),
	MTK_VF_TABLE("dsi_occ_sel", 312000, 312000, 249600, 182000),
	{},
};
#endif

static struct mtk_vf *get_vf_table(void)
{
#if CHECK_VCORE_FREQ
	return vf_table;
#else
	return NULL;
#endif
}

static int get_vcore_opp(void)
{
#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER) && CHECK_VCORE_FREQ
	return mtk_dvfsrc_query_opp_info(MTK_DVFSRC_SW_REQ_VCORE_OPP);
#else
	return VCORE_NULL;
#endif
}

void print_subsys_reg_mt6833(enum chk_sys_id id)
{
	struct regbase *rb_dump;
	const struct regname *rns = &rn[0];
	int i;

	if (id >= chk_sys_num) {
		pr_info("wrong id:%d\n", id);
		return;
	}

	rb_dump = &rb[id];

	for (i = 0; i < ARRAY_SIZE(rn) - 1; i++, rns++) {
		if (!is_valid_reg(ADDR(rns)))
			return;

		/* filter out the subsys that we don't want */
		if (rns->base != rb_dump)
			continue;

		pr_info("%-18s: [0x%08x] = 0x%08x\n",
			rns->name, PHYSADDR(rns), clk_readl(ADDR(rns)));
	}
}
EXPORT_SYMBOL(print_subsys_reg_mt6833);

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
static void devapc_dump(void)
{
	print_subsys_reg_mt6833(spm);
	print_subsys_reg_mt6833(top);
	print_subsys_reg_mt6833(infracfg);
	print_subsys_reg_mt6833(apmixed);
}

static struct devapc_vio_callbacks devapc_vio_handle = {
	.id = DEVAPC_SUBSYS_CLKMGR,
	.debug_dump = devapc_dump,
};

#endif

static const char * const off_pll_names[] = {
	"univpll",
	"msdcpll",
	"mmpll",
	"adsppll",
	"mfgpll",
	"tvdpll",
	"apll1",
	"apll2",
	"npupll",
	"usbpll",
	NULL
};

static const char * const notice_pll_names[] = {

	NULL
};

static const char * const *get_off_pll_names(void)
{
	return off_pll_names;
}

static const char * const *get_notice_pll_names(void)
{
	return notice_pll_names;
}

static bool is_pll_chk_bug_on(void)
{
#if BUG_ON_CHK_ENABLE
	return true;
#endif
	return false;
}

/*
 * init functions
 */

static struct clkchk_ops clkchk_mt6833_ops = {
	.get_all_regnames = get_all_mt6833_regnames,
	.get_spm_pwr_status_array = get_spm_pwr_status_array,
	.get_pvd_pwr_mask = get_pvd_pwr_mask,
	.get_off_pll_names = get_off_pll_names,
	.get_notice_pll_names = get_notice_pll_names,
	.is_pll_chk_bug_on = is_pll_chk_bug_on,
	.get_vf_table = get_vf_table,
	.get_vcore_opp = get_vcore_opp,
	.devapc_dump = devapc_dump,
};

static int clk_chk_mt6833_probe(struct platform_device *pdev)
{
	init_regbase();

	set_clkchk_notify();

	set_clkchk_ops(&clkchk_mt6833_ops);

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
	register_devapc_vio_callback(&devapc_vio_handle);
#endif

	return 0;
}

static struct platform_driver clk_chk_mt6833_drv = {
	.probe = clk_chk_mt6833_probe,
	.driver = {
		.name = "clk-chk-mt6833",
		.owner = THIS_MODULE,
		.pm = &clk_chk_dev_pm_ops,
	},
};

/*
 * init functions
 */

static int __init clkchk_mt6833_init(void)
{
	static struct platform_device *clk_chk_dev;

	clk_chk_dev = platform_device_register_simple("clk-chk-mt6833", -1, NULL, 0);
	if (IS_ERR(clk_chk_dev))
		pr_warn("unable to register clk-chk device");

	return platform_driver_register(&clk_chk_mt6833_drv);
}

static void __exit clkchk_mt6833_exit(void)
{
	platform_driver_unregister(&clk_chk_mt6833_drv);
}

subsys_initcall(clkchk_mt6833_init);
module_exit(clkchk_mt6833_exit);
MODULE_LICENSE("GPL");
