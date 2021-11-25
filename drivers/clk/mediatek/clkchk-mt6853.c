// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>

#include <dt-bindings/power/mt6853-power.h>

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
#include <devapc_public.h>
#endif

#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER)
#include <mt-plat/dvfsrc-exp.h>
#endif

#include "clkchk.h"
#include "clkchk-mt6853.h"

#define BUG_ON_CHK_ENABLE	1
#define CHECK_VCORE_FREQ	1

/*
 * clkchk dump_regs
 */

#define REGBASE_V(_phys, _id_name, _pg) { .phys = _phys,	\
		.name = #_id_name, .pg = _pg}

static struct regbase rb[] = {
	[top] = REGBASE_V(0x10000000, top, PD_NULL),
	[ifrao] = REGBASE_V(0x10001000, ifrao, PD_NULL),
	[infracfg_ao_bus] = REGBASE_V(0x10001000, infracfg_ao_bus, PD_NULL),
	[spm] = REGBASE_V(0x10006000, spm, PD_NULL),
	[apmixed] = REGBASE_V(0x1000C000, apmixed, PD_NULL),
	[scp_par] = REGBASE_V(0x10720000, scp_par, PD_NULL),
	[impc] = REGBASE_V(0x11007000, impc, PD_NULL),
	[audsys] = REGBASE_V(0x11210000, audsys, MT6853_POWER_DOMAIN_AUDIO),
	[impe] = REGBASE_V(0x11cb1000, impe, PD_NULL),
	[imps] = REGBASE_V(0x11d04000, imps, PD_NULL),
	[impws] = REGBASE_V(0x11d23000, impws, PD_NULL),
	[impw] = REGBASE_V(0x11e01000, impw, PD_NULL),
	[impn] = REGBASE_V(0x11f01000, impn, PD_NULL),
	[mfg] = REGBASE_V(0x13fbf000, mfg, MT6853_POWER_DOMAIN_MFG0),
	[mm] = REGBASE_V(0x14000000, mm, MT6853_POWER_DOMAIN_DISP),
	[imgsys1] = REGBASE_V(0x15020000, imgsys1, MT6853_POWER_DOMAIN_ISP),
	[imgsys2] = REGBASE_V(0x15820000, imgsys2, MT6853_POWER_DOMAIN_ISP2),
	[vdec] = REGBASE_V(0x1602f000, vdec, MT6853_POWER_DOMAIN_VDEC),
	[venc] = REGBASE_V(0x17000000, venc, MT6853_POWER_DOMAIN_VENC),
	[apuc] = REGBASE_V(0x19020000, apuc, MT6853_POWER_DOMAIN_APU),
	[apuv] = REGBASE_V(0x19029000, apuv, MT6853_POWER_DOMAIN_APU),
	[apu0] = REGBASE_V(0x19030000, apu0, MT6853_POWER_DOMAIN_APU),
	[apu1] = REGBASE_V(0x19031000, apu1, MT6853_POWER_DOMAIN_APU),
	[cam_m] = REGBASE_V(0x1a000000, cam_m, MT6853_POWER_DOMAIN_CAM),
	[cam_ra] = REGBASE_V(0x1a04f000, cam_ra, MT6853_POWER_DOMAIN_CAM_RAWA),
	[cam_rb] = REGBASE_V(0x1a06f000, cam_rb, MT6853_POWER_DOMAIN_CAM_RAWB),
	[ipe] = REGBASE_V(0x1b000000, ipe, MT6853_POWER_DOMAIN_IPE),
	[mdp] = REGBASE_V(0x1f000000, mdp, MT6853_POWER_DOMAIN_DISP),
	{},
};

#define REGNAME(_base, _ofs, _name)	\
	{ .base = &rb[_base], .ofs = _ofs, .name = #_name }

static struct regname rn[] = {
	/* TOPCKGEN register */
	REGNAME(top,  0x0010, CLK_CFG_0),
	REGNAME(top,  0x0020, CLK_CFG_1),
	REGNAME(top,  0x0030, CLK_CFG_2),
	REGNAME(top,  0x0040, CLK_CFG_3),
	REGNAME(top,  0x0050, CLK_CFG_4),
	REGNAME(top,  0x0060, CLK_CFG_5),
	REGNAME(top,  0x0070, CLK_CFG_6),
	REGNAME(top,  0x0080, CLK_CFG_7),
	REGNAME(top,  0x0090, CLK_CFG_8),
	REGNAME(top,  0x00A0, CLK_CFG_9),
	REGNAME(top,  0x00B0, CLK_CFG_10),
	REGNAME(top,  0x00C0, CLK_CFG_11),
	REGNAME(top,  0x00D0, CLK_CFG_12),
	REGNAME(top,  0x00E0, CLK_CFG_13),
	REGNAME(top,  0x00F0, CLK_CFG_14),
	REGNAME(top,  0x0100, CLK_CFG_15),
	REGNAME(top,  0x0110, CLK_CFG_16),
	REGNAME(top,  0x0320, CLK_AUDDIV_0),
	REGNAME(top,  0x0328, CLK_AUDDIV_2),
	REGNAME(top,  0x0334, CLK_AUDDIV_3),
	REGNAME(top,  0x0338, CLK_AUDDIV_4),
	/* INFRACFG_AO register */
	REGNAME(ifrao,  0x90, MODULE_SW_CG_0),
	REGNAME(ifrao,  0x94, MODULE_SW_CG_1),
	REGNAME(ifrao,  0xac, MODULE_SW_CG_2),
	REGNAME(ifrao,  0xc8, MODULE_SW_CG_3),
	REGNAME(ifrao,  0xe8, MODULE_SW_CG_4),
	REGNAME(ifrao,  0xd8, MODULE_SW_CG_5),
	/* INFRACFG_AO_BUS register */
	REGNAME(infracfg_ao_bus,  0x0220, INFRA_TOPAXI_PROTECTEN),
	REGNAME(infracfg_ao_bus,  0x0224, INFRA_TOPAXI_PROTECTEN_STA0),
	REGNAME(infracfg_ao_bus,  0x0228, INFRA_TOPAXI_PROTECTEN_STA1),
	REGNAME(infracfg_ao_bus,  0x0B80, INFRA_TOPAXI_PROTECTEN_VDNR),
	REGNAME(infracfg_ao_bus,  0x0B8C, INFRA_TOPAXI_PROTECTEN_VDNR_STA0),
	REGNAME(infracfg_ao_bus,  0x0B90, INFRA_TOPAXI_PROTECTEN_VDNR_STA1),
	REGNAME(infracfg_ao_bus,  0x0250, INFRA_TOPAXI_PROTECTEN_1),
	REGNAME(infracfg_ao_bus,  0x0254, INFRA_TOPAXI_PROTECTEN_STA0_1),
	REGNAME(infracfg_ao_bus,  0x0258, INFRA_TOPAXI_PROTECTEN_STA1_1),
	REGNAME(infracfg_ao_bus,  0x0710, INFRA_TOPAXI_PROTECTEN_2),
	REGNAME(infracfg_ao_bus,  0x0720, INFRA_TOPAXI_PROTECTEN_STA0_2),
	REGNAME(infracfg_ao_bus,  0x0724, INFRA_TOPAXI_PROTECTEN_STA1_2),
	REGNAME(infracfg_ao_bus,  0x0BA0, INFRA_TOPAXI_PROTECTEN_VDNR_1),
	REGNAME(infracfg_ao_bus,  0x0BAC, INFRA_TOPAXI_PROTECTEN_VDNR_STA0_1),
	REGNAME(infracfg_ao_bus,  0x0BB0, INFRA_TOPAXI_PROTECTEN_VDNR_STA1_1),
	REGNAME(infracfg_ao_bus,  0x02C0, INFRA_TOPAXI_PROTECTEN_MCU),
	REGNAME(infracfg_ao_bus,  0x02E0, INFRA_TOPAXI_PROTECTEN_MCU_STA0),
	REGNAME(infracfg_ao_bus,  0x02E4, INFRA_TOPAXI_PROTECTEN_MCU_STA1),
	REGNAME(infracfg_ao_bus,  0x0DC8, INFRA_TOPAXI_PROTECTEN_MM_2),
	REGNAME(infracfg_ao_bus,  0x0DD4, INFRA_TOPAXI_PROTECTEN_MM_STA0_2),
	REGNAME(infracfg_ao_bus,  0x0DD8, INFRA_TOPAXI_PROTECTEN_MM_STA1_2),
	REGNAME(infracfg_ao_bus,  0x02D0, INFRA_TOPAXI_PROTECTEN_MM),
	REGNAME(infracfg_ao_bus,  0x02E8, INFRA_TOPAXI_PROTECTEN_MM_STA0),
	REGNAME(infracfg_ao_bus,  0x02EC, INFRA_TOPAXI_PROTECTEN_MM_STA1),
	/* SPM register */
	REGNAME(spm,  0x16C, PWR_STATUS),
	REGNAME(spm,  0x170, PWR_STATUS_2ND),
	REGNAME(spm,  0x178, OTHER_PWR_STATUS),
	REGNAME(spm,  0x300, MD1_PWR_CON),
	REGNAME(spm,  0x304, CONN_PWR_CON),
	REGNAME(spm,  0x308, MFG0_PWR_CON),
	REGNAME(spm,  0x30C, MFG1_PWR_CON),
	REGNAME(spm,  0x310, MFG2_PWR_CON),
	REGNAME(spm,  0x314, MFG3_PWR_CON),
	REGNAME(spm,  0x31C, MFG5_PWR_CON),
	REGNAME(spm,  0x330, ISP_PWR_CON),
	REGNAME(spm,  0x334, ISP2_PWR_CON),
	REGNAME(spm,  0x338, IPE_PWR_CON),
	REGNAME(spm,  0x33C, VDE_PWR_CON),
	REGNAME(spm,  0x340, VDE2_PWR_CON),
	REGNAME(spm,  0x344, VEN_PWR_CON),
	REGNAME(spm,  0x348, VEN_CORE1_PWR_CON),
	REGNAME(spm,  0x34C, MDP_PWR_CON),
	REGNAME(spm,  0x350, DIS_PWR_CON),
	REGNAME(spm,  0x354, AUDIO_PWR_CON),
	REGNAME(spm,  0x358, ADSP_PWR_CON),
	REGNAME(spm,  0x35C, CAM_PWR_CON),
	REGNAME(spm,  0x360, CAM_RAWA_PWR_CON),
	REGNAME(spm,  0x364, CAM_RAWB_PWR_CON),
	/* APMIXEDSYS register */
	REGNAME(apmixed,  0x208, ARMPLL_LL_CON0),
	REGNAME(apmixed,  0x20c, ARMPLL_LL_CON1),
	REGNAME(apmixed,  0x210, ARMPLL_LL_CON2),
	REGNAME(apmixed,  0x214, ARMPLL_LL_CON3),
	REGNAME(apmixed,  0x218, ARMPLL_BL0_CON0),
	REGNAME(apmixed,  0x21c, ARMPLL_BL0_CON1),
	REGNAME(apmixed,  0x220, ARMPLL_BL0_CON2),
	REGNAME(apmixed,  0x224, ARMPLL_BL0_CON3),
	REGNAME(apmixed,  0x258, CCIPLL_CON0),
	REGNAME(apmixed,  0x25c, CCIPLL_CON1),
	REGNAME(apmixed,  0x260, CCIPLL_CON2),
	REGNAME(apmixed,  0x264, CCIPLL_CON3),
	REGNAME(apmixed,  0x390, MPLL_CON0),
	REGNAME(apmixed,  0x394, MPLL_CON1),
	REGNAME(apmixed,  0x398, MPLL_CON2),
	REGNAME(apmixed,  0x39c, MPLL_CON3),
	REGNAME(apmixed,  0x340, MAINPLL_CON0),
	REGNAME(apmixed,  0x344, MAINPLL_CON1),
	REGNAME(apmixed,  0x348, MAINPLL_CON2),
	REGNAME(apmixed,  0x34c, MAINPLL_CON3),
	REGNAME(apmixed,  0x308, UNIVPLL_CON0),
	REGNAME(apmixed,  0x30c, UNIVPLL_CON1),
	REGNAME(apmixed,  0x310, UNIVPLL_CON2),
	REGNAME(apmixed,  0x314, UNIVPLL_CON3),
	REGNAME(apmixed,  0x350, MSDCPLL_CON0),
	REGNAME(apmixed,  0x354, MSDCPLL_CON1),
	REGNAME(apmixed,  0x358, MSDCPLL_CON2),
	REGNAME(apmixed,  0x35c, MSDCPLL_CON3),
	REGNAME(apmixed,  0x360, MMPLL_CON0),
	REGNAME(apmixed,  0x364, MMPLL_CON1),
	REGNAME(apmixed,  0x368, MMPLL_CON2),
	REGNAME(apmixed,  0x36c, MMPLL_CON3),
	REGNAME(apmixed,  0x370, ADSPPLL_CON0),
	REGNAME(apmixed,  0x374, ADSPPLL_CON1),
	REGNAME(apmixed,  0x378, ADSPPLL_CON2),
	REGNAME(apmixed,  0x37c, ADSPPLL_CON3),
	REGNAME(apmixed,  0x268, MFGPLL_CON0),
	REGNAME(apmixed,  0x26c, MFGPLL_CON1),
	REGNAME(apmixed,  0x270, MFGPLL_CON2),
	REGNAME(apmixed,  0x274, MFGPLL_CON3),
	REGNAME(apmixed,  0x380, TVDPLL_CON0),
	REGNAME(apmixed,  0x384, TVDPLL_CON1),
	REGNAME(apmixed,  0x388, TVDPLL_CON2),
	REGNAME(apmixed,  0x38c, TVDPLL_CON3),
	REGNAME(apmixed,  0x318, APLL1_CON0),
	REGNAME(apmixed,  0x31c, APLL1_CON1),
	REGNAME(apmixed,  0x320, APLL1_CON2),
	REGNAME(apmixed,  0x324, APLL1_CON3),
	REGNAME(apmixed,  0x328, APLL1_CON4),
	REGNAME(apmixed,  0x32c, APLL2_CON0),
	REGNAME(apmixed,  0x330, APLL2_CON1),
	REGNAME(apmixed,  0x334, APLL2_CON2),
	REGNAME(apmixed,  0x338, APLL2_CON3),
	REGNAME(apmixed,  0x33c, APLL2_CON4),
	REGNAME(apmixed,  0x3b4, NPUPLL_CON0),
	REGNAME(apmixed,  0x3b8, NPUPLL_CON1),
	REGNAME(apmixed,  0x3bc, NPUPLL_CON2),
	REGNAME(apmixed,  0x3c0, NPUPLL_CON3),
	REGNAME(apmixed,  0x3c4, USBPLL_CON0),
	REGNAME(apmixed,  0x3c8, USBPLL_CON1),
	REGNAME(apmixed,  0x3cc, USBPLL_CON2),
	REGNAME(apmixed,  0x14, AP_PLL_5),
	/* SCP_PAR_TOP register */
	REGNAME(scp_par,  0x180, AUDIODSP_CK_CG),
	/* IMP_IIC_WRAP_C register */
	REGNAME(impc,  0xe00, AP_CLOCK_CG_CEN),
	/* AUDIO register */
	REGNAME(audsys,  0x0, AUDIO_TOP_0),
	REGNAME(audsys,  0x4, AUDIO_TOP_1),
	REGNAME(audsys,  0x8, AUDIO_TOP_2),
	/* IMP_IIC_WRAP_E register */
	REGNAME(impe,  0xe00, AP_CLOCK_CG_EST),
	/* IMP_IIC_WRAP_S register */
	REGNAME(imps,  0xe00, AP_CLOCK_CG_SOU),
	/* IMP_IIC_WRAP_WS register */
	REGNAME(impws,  0xe00, AP_CLOCK_CG_WEST_SOU),
	/* IMP_IIC_WRAP_W register */
	REGNAME(impw,  0xe00, AP_CLOCK_CG_WST),
	/* IMP_IIC_WRAP_N register */
	REGNAME(impn,  0xe00, AP_CLOCK_CG_NOR),
	/* MFGCFG register */
	REGNAME(mfg,  0x0, MFG_CG),
	/* MMSYS_CONFIG register */
	REGNAME(mm,  0x100, MMSYS_CG_0),
	REGNAME(mm,  0x1a0, MMSYS_CG_2),
	/* IMGSYS1 register */
	REGNAME(imgsys1,  0x0, IMG_CG),
	/* IMGSYS2 register */
	REGNAME(imgsys2,  0x0, IMG_CG),
	/* VDEC_GCON register */
	REGNAME(vdec,  0x8, LARB_CKEN_CON),
	REGNAME(vdec,  0x0, VDEC_CKEN),
	/* VENC_GCON register */
	REGNAME(venc,  0x0, VENCSYS_CG),
	/* APU_CONN register */
	REGNAME(apuc,  0x0, APU_CONN_CG),
	/* APUSYS_VCORE register */
	REGNAME(apuv,  0x0, APUSYS_VCORE_CG),
	/* APU0 register */
	REGNAME(apu0,  0x100, CORE_CG),
	REGNAME(apu0,  0x910, CORE_CTRL),
	/* APU1 register */
	REGNAME(apu1,  0x100, CORE_CG),
	REGNAME(apu1,  0x910, CORE_CTRL),
	/* CAMSYS_MAIN register */
	REGNAME(cam_m,  0x0, CAMSYS_CG),
	/* CAMSYS_RAWA register */
	REGNAME(cam_ra,  0x0, CAMSYS_CG),
	/* CAMSYS_RAWB register */
	REGNAME(cam_rb,  0x0, CAMSYS_CG),
	/* IPESYS register */
	REGNAME(ipe,  0x0, IMG_CG),
	/* MDPSYS_CONFIG register */
	REGNAME(mdp,  0x100, MDPSYS_CG_0),
	REGNAME(mdp,  0x120, MDPSYS_CG_2),
	{},
};

static const struct regname *get_all_mt6853_regnames(void)
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

u32 *get_spm_pwr_status_array(void)
{
	static void __iomem *pwr_addr[STA_NUM];
	static u32 pwr_sta[STA_NUM];
	int i;

	for (i = 0; i < STA_NUM; i++) {
		if (pwr_ofs[i] && !pwr_sta[i]) {
			pwr_addr[i] = rb[spm].virt + pwr_ofs[i];
			pwr_sta[i] = clk_readl(pwr_addr[i]);
		}
	}

	return pwr_sta;
}

/*
 * clkchk pwr_state
 */
static struct pvd_msk pvd_pwr_mask[] = {
	{"mfgcfg", PWR_STA, 0x000000BC},
	{"imgsys1", PWR_STA, 0x00001000},
	{"imgsys2", PWR_STA, 0x00002000},
	{"ipesys", PWR_STA, 0x00004000},
	{"vdec_gcon", PWR_STA, 0x00008000},
	{"venc_gcon", PWR_STA, 0x00020000},
	{"mdpsys_config", PWR_STA, 0x00100000},
	{"mmsys_config", PWR_STA, 0x00100000},
	{"audiosys", PWR_STA, 0x00200000},
	{"audiodsp", PWR_STA, 0x00400000},
	{"camsys_main", PWR_STA, 0x00800000},
	{"camsys_rawa", PWR_STA, 0x01000000},
	{"camsys_rawb", PWR_STA, 0x02000000},
	{"apu_conn", OTHER_STA, 0x00000020},
	{"apu_vcore", OTHER_STA, 0x00000020},
	{"apu0", OTHER_STA, 0x00000020},
	{"apu1", OTHER_STA, 0x00000020},
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
	MTK_VF_TABLE("cam_sel", 624000, 499200, 392857, 273000),
	MTK_VF_TABLE("ccu_sel", 499200, 392857, 364000, 275000),
	MTK_VF_TABLE("ipu_if_sel", 546000, 416000, 312000, 208000),
	MTK_VF_TABLE("camtg_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg2_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg3_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg4_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg5_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("uart_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("spi_sel", 109200, 109200, 109200, 109200),
	MTK_VF_TABLE("msdc50_0_h_sel", 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("msdc50_0_sel", 384000, 384000, 384000, 384000),
	MTK_VF_TABLE("msdc30_1_sel", 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("audio_sel", 54600, 54600, 54600, 54600),
	MTK_VF_TABLE("aud_intbus_sel", 136500, 136500, 136500, 136500),
	MTK_VF_TABLE("pwrap_ulposc_sel", 65000, 65000, 65000, 65000),
	MTK_VF_TABLE("atb_sel", 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("sspm_sel", 364000, 312000, 273000, 242666),
	MTK_VF_TABLE("scam_sel", 109200, 109200, 109200, 109200),
	MTK_VF_TABLE("disp_pwm_sel", 130000, 130000, 130000, 130000),
	MTK_VF_TABLE("usb_top_sel", 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("ssusb_xhci_sel", 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("i2c_sel", 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("seninf_sel", 499200, 499200, 392857, 273000),
	MTK_VF_TABLE("seninf1_sel", 499200, 499200, 392857, 273000),
	MTK_VF_TABLE("seninf2_sel", 499200, 499200, 392857, 273000),
	MTK_VF_TABLE("dxcc_sel", 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("aud_engen1_sel", 45158, 45158, 45158, 45158),
	MTK_VF_TABLE("aud_engen2_sel", 49152, 49152, 49152, 49152),
	MTK_VF_TABLE("aes_ufsfde_sel", 546000, 546000, 546000, 416000),
	MTK_VF_TABLE("ufs_sel", 192000, 192000, 192000, 192000),
	MTK_VF_TABLE("aud_1_sel", 180633, 180633, 180633, 180633),
	MTK_VF_TABLE("aud_2_sel", 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("adsp_sel", 750000, 750000, 750000, 750000),
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
	int opp = VCORE_NULL;

#if IS_ENABLED(CONFIG_MTK_DVFSRC_HELPER) && CHECK_VCORE_FREQ
	opp = mtk_dvfsrc_query_opp_info(MTK_DVFSRC_SW_REQ_VCORE_OPP);
#endif
	return opp;
}

void print_subsys_reg_mt6853(enum chk_sys_id id)
{
	struct regbase *rb_dump;
	const struct regname *rns = &rn[0];
	int i;

	if (rns == NULL)
		return;

	if (id >= chk_sys_num || id < 0) {
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
EXPORT_SYMBOL(print_subsys_reg_mt6853);

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
static void devapc_dump_regs(void)
{
	print_subsys_reg_mt6853(spm);
	print_subsys_reg_mt6853(top);
	print_subsys_reg_mt6853(infracfg_ao_bus);
	print_subsys_reg_mt6853(apmixed);
	pr_notice("devapc dump\n");
}

static struct devapc_vio_callbacks devapc_vio_handle = {
	.id = DEVAPC_SUBSYS_CLKMGR,
	.debug_dump = devapc_dump_regs,
};
#endif

/*
 * clkchk dump_clks
 */
static const char * const off_pll_names[] = {
	"univpll",
	"msdcpll",
	"mmpll",
	"mfgpll",
	"tvdpll",
	"apll1",
	"apll2",
	"npupll",
	"usbpll",
	NULL
};

static const char * const notice_pll_names[] = {
	"adsppll",
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

static struct clkchk_ops clkchk_mt6853_ops = {
	.get_all_regnames = get_all_mt6853_regnames,
	.get_spm_pwr_status_array = get_spm_pwr_status_array,
	.get_pvd_pwr_mask = get_pvd_pwr_mask,
	.get_off_pll_names = get_off_pll_names,
	.get_notice_pll_names = get_notice_pll_names,
	.is_pll_chk_bug_on = is_pll_chk_bug_on,
	.get_vf_table = get_vf_table,
	.get_vcore_opp = get_vcore_opp,
	.devapc_dump = devapc_dump_regs,
};

static int clk_chk_mt6853_probe(struct platform_device *pdev)
{
	init_regbase();

	set_clkchk_ops(&clkchk_mt6853_ops);

#if IS_ENABLED(CONFIG_MTK_DEVAPC)
	register_devapc_vio_callback(&devapc_vio_handle);
#endif

	return 0;
}

static struct platform_driver clk_chk_mt6853_drv = {
	.probe = clk_chk_mt6853_probe,
	.driver = {
		.name = "clk-chk-mt6853",
		.owner = THIS_MODULE,
		.pm = &clk_chk_dev_pm_ops,
	},
};

/*
 * init functions
 */

static int __init clkchk_mt6853_init(void)
{
	static struct platform_device *clk_chk_dev;

	clk_chk_dev = platform_device_register_simple("clk-chk-mt6853", -1, NULL, 0);
	if (IS_ERR(clk_chk_dev))
		pr_warn("unable to register clk-chk device");

	return platform_driver_register(&clk_chk_mt6853_drv);
}

static void __exit clkchk_mt6853_exit(void)
{
	platform_driver_unregister(&clk_chk_mt6853_drv);
}

subsys_initcall(clkchk_mt6853_init);
module_exit(clkchk_mt6853_exit);
MODULE_LICENSE("GPL");