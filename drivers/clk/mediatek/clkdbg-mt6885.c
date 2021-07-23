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

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/seq_file.h>

#ifdef CONFIG_MTK_DEVAPC
#include <mt-plat/devapc_public.h>
#endif

#include "clkdbg.h"
#include "clkdbg-mt6885.h"
#include "clk-fmeter.h"
#include <clk-mux.h>

#define DUMP_INIT_STATE		0
#define CHECK_VCORE_FREQ		1

int __attribute__((weak)) get_sw_req_vcore_opp(void)
{
	return -1;
}

/*
 * clkdbg dump_regs
 */

#define REGBASE_V(_phys, _id_name, _pg) { .phys = _phys,	\
		.name = #_id_name, .pg = _pg}

/*
 * checkpatch.pl ERROR:COMPLEX_MACRO
 *
 * #define REGBASE(_phys, _id_name) [_id_name] = REGBASE_V(_phys, _id_name)
 */

static struct regbase rb[] = {
	[topckgen] = REGBASE_V(0x10000000, topckgen, NULL),
	[infracfg] = REGBASE_V(0x10001000, infracfg, NULL),
	[infracfg_dbg]  = REGBASE_V(0x10001000, infracfg_dbg, NULL),
	[scpsys]   = REGBASE_V(0x10006000, scpsys, NULL),
	[apmixed]  = REGBASE_V(0x1000c000, apmixed, NULL),
	[audio]    = REGBASE_V(0x11210000, audio, "PG_AUDIO"),
	[mfgsys]   = REGBASE_V(0x13fbf000, mfgsys, "PG_MFG6"),
	[mmsys]    = REGBASE_V(0x14116000, mmsys, "PG_DIS"),
	[mdpsys]    = REGBASE_V(0x1F000000, mdpsys, "PG_MDP"),
	[img1sys]   = REGBASE_V(0x15020000, img1sys, "PG_ISP"),
	[img2sys]   = REGBASE_V(0x15820000, img2sys, "PG_ISP2"),
	[ipesys]   = REGBASE_V(0x1b000000, ipesys, "PG_IPE"),
	[camsys]   = REGBASE_V(0x1a000000, camsys, "PG_CAM"),
	[cam_rawa_sys]   = REGBASE_V(0x1a04f000, cam_rawa_sys, "PG_CAM_RAWA"),
	[cam_rawb_sys]   = REGBASE_V(0x1a06f000, cam_rawb_sys, "PG_CAM_RAWB"),
	[cam_rawc_sys]   = REGBASE_V(0x1a08f000, cam_rawc_sys, "PG_CAM_RAWC"),
	[vencsys]  = REGBASE_V(0x17000000, vencsys, "PG_VENC"),
	[venc_c1_sys]  = REGBASE_V(0x17800000, venc_c1_sys, "PG_VENC_C1"),
	[vdecsys]  = REGBASE_V(0x1602f000, vdecsys, "PG_VDEC2"),
	[vdec_soc_sys]  = REGBASE_V(0x1600f000, vdec_soc_sys, "PG_VDEC1"),
	[ipu_vcore]  = REGBASE_V(0x19029000, ipu_vcore, "PG_VPU"),
	[ipu_conn]  = REGBASE_V(0x19020000, ipu_conn, "PG_VPU"),
	[ipu0]  = REGBASE_V(0x19030000, ipu0, "PG_VPU"),
	[ipu1]  = REGBASE_V(0x19031000, ipu1, "PG_VPU"),
	[ipu2]  = REGBASE_V(0x19032000, ipu2, "PG_VPU"),
	[scp_par] = REGBASE_V(0x10720000, scp_par, NULL),
	{},
};

#define REGNAME(_base, _ofs, _name)	\
	{ .base = &rb[_base], .ofs = _ofs, .name = #_name }

static struct regname rn[] = {
	REGNAME(topckgen,  0x000, CLK_MODE),
	REGNAME(topckgen,  0x010, CLK_CFG_0),
	REGNAME(topckgen,  0x020, CLK_CFG_1),
	REGNAME(topckgen,  0x030, CLK_CFG_2),
	REGNAME(topckgen,  0x040, CLK_CFG_3),
	REGNAME(topckgen,  0x050, CLK_CFG_4),
	REGNAME(topckgen,  0x060, CLK_CFG_5),
	REGNAME(topckgen,  0x070, CLK_CFG_6),
	REGNAME(topckgen,  0x080, CLK_CFG_7),
	REGNAME(topckgen,  0x090, CLK_CFG_8),
	REGNAME(topckgen,  0x0A0, CLK_CFG_9),
	REGNAME(topckgen,  0x0B0, CLK_CFG_10),
	REGNAME(topckgen,  0x0C0, CLK_CFG_11),
	REGNAME(topckgen,  0x0D0, CLK_CFG_12),
	REGNAME(topckgen,  0x0E0, CLK_CFG_13),
	REGNAME(topckgen,  0x0F0, CLK_CFG_14),
	REGNAME(topckgen,  0x100, CLK_CFG_15),
	REGNAME(topckgen,  0x110, CLK_CFG_16),
	REGNAME(topckgen,  0x320, CLK_AUDDIV_0),
	REGNAME(topckgen,  0x328, CLK_AUDDIV_2),
	REGNAME(topckgen,  0x334, CLK_AUDDIV_3),
	REGNAME(topckgen,  0x338, CLK_AUDDIV_4),

	REGNAME(apmixed, 0x00C, AP_PLL_CON3),
	REGNAME(apmixed, 0x014, AP_PLL_CON5),
	REGNAME(apmixed, 0x040, APLL1_TUNER_CON0),
	REGNAME(apmixed, 0x044, APLL2_TUNER_CON0),
	REGNAME(apmixed, 0x050, PLLON_CON0),
	REGNAME(apmixed, 0x054, PLLON_CON1),
	REGNAME(apmixed, 0x058, PLLON_CON2),
	REGNAME(apmixed, 0x05C, PLLON_CON3),
	REGNAME(apmixed, 0x208, ARMPLL_LL_CON0),
	REGNAME(apmixed, 0x20C, ARMPLL_LL_CON1),
	REGNAME(apmixed, 0x210, ARMPLL_LL_CON2),
	REGNAME(apmixed, 0x214, ARMPLL_LL_CON3),
	REGNAME(apmixed, 0x218, ARMPLL_BL0_CON0),
	REGNAME(apmixed, 0x21C, ARMPLL_BL0_CON1),
	REGNAME(apmixed, 0x220, ARMPLL_BL0_CON2),
	REGNAME(apmixed, 0x224, ARMPLL_BL0_CON3),
	REGNAME(apmixed, 0x228, ARMPLL_BL1_CON0),
	REGNAME(apmixed, 0x22C, ARMPLL_BL1_CON1),
	REGNAME(apmixed, 0x230, ARMPLL_BL1_CON2),
	REGNAME(apmixed, 0x234, ARMPLL_BL1_CON3),
	REGNAME(apmixed, 0x238, ARMPLL_BL2_CON0),
	REGNAME(apmixed, 0x23C, ARMPLL_BL2_CON1),
	REGNAME(apmixed, 0x240, ARMPLL_BL2_CON2),
	REGNAME(apmixed, 0x244, ARMPLL_BL2_CON3),
	REGNAME(apmixed, 0x248, ARMPLL_BL3_CON0),
	REGNAME(apmixed, 0x24C, ARMPLL_BL3_CON1),
	REGNAME(apmixed, 0x250, ARMPLL_BL3_CON2),
	REGNAME(apmixed, 0x254, ARMPLL_BL3_CON3),
	REGNAME(apmixed, 0x258, CCIPLL_CON0),
	REGNAME(apmixed, 0x25C, CCIPLL_CON1),
	REGNAME(apmixed, 0x260, CCIPLL_CON2),
	REGNAME(apmixed, 0x264, CCIPLL_CON3),
	REGNAME(apmixed, 0x268, MFGPLL_CON0),
	REGNAME(apmixed, 0x26C, MFGPLL_CON1),
	REGNAME(apmixed, 0x274, MFGPLL_CON3),
	REGNAME(apmixed, 0x308, UNIVPLL_CON0),
	REGNAME(apmixed, 0x30C, UNIVPLL_CON1),
	REGNAME(apmixed, 0x314, UNIVPLL_CON3),
	REGNAME(apmixed, 0x318, APLL1_CON0),
	REGNAME(apmixed, 0x31C, APLL1_CON1),
	REGNAME(apmixed, 0x320, APLL1_CON2),
	REGNAME(apmixed, 0x324, APLL1_CON3),
	REGNAME(apmixed, 0x328, APLL1_CON4),
	REGNAME(apmixed, 0x32C, APLL2_CON0),
	REGNAME(apmixed, 0x330, APLL2_CON1),
	REGNAME(apmixed, 0x334, APLL2_CON2),
	REGNAME(apmixed, 0x338, APLL2_CON3),
	REGNAME(apmixed, 0x33C, APLL2_CON4),
	REGNAME(apmixed, 0x340, MAINPLL_CON0),
	REGNAME(apmixed, 0x344, MAINPLL_CON1),
	REGNAME(apmixed, 0x34C, MAINPLL_CON3),
	REGNAME(apmixed, 0x350, MSDCPLL_CON0),
	REGNAME(apmixed, 0x354, MSDCPLL_CON1),
	REGNAME(apmixed, 0x35C, MSDCPLL_CON3),
	REGNAME(apmixed, 0x360, MMPLL_CON0),
	REGNAME(apmixed, 0x364, MMPLL_CON1),
	REGNAME(apmixed, 0x36C, MMPLL_CON3),
	REGNAME(apmixed, 0x370, ADSPPLL_CON0),
	REGNAME(apmixed, 0x374, ADSPPLL_CON1),
	REGNAME(apmixed, 0x37C, ADSPPLL_CON3),
	REGNAME(apmixed, 0x380, TVDPLL_CON0),
	REGNAME(apmixed, 0x384, TVDPLL_CON1),
	REGNAME(apmixed, 0x38C, TVDPLL_CON3),
	REGNAME(apmixed, 0x390, MPLL_CON0),
	REGNAME(apmixed, 0x394, MPLL_CON1),
	REGNAME(apmixed, 0x39C, MPLL_CON3),
	REGNAME(apmixed, 0x3A0, APUPLL_CON0),
	REGNAME(apmixed, 0x3A4, APUPLL_CON1),
	REGNAME(apmixed, 0x3AC, APUPLL_CON3),

	REGNAME(scpsys, 0x0000, POWERON_CONFIG_EN),
	REGNAME(scpsys, 0x016C, PWR_STATUS),
	REGNAME(scpsys, 0x0170, PWR_STATUS_2ND),
	REGNAME(scpsys, 0x0178, OTHER_PWR_STATUS),
	REGNAME(scpsys, 0x300, MD1_PWR_CON),
	REGNAME(scpsys, 0x304, CONN_PWR_CON),
	REGNAME(scpsys, 0x308, MFG0_PWR_CON),
	REGNAME(scpsys, 0x30C, MFG1_PWR_CON),
	REGNAME(scpsys, 0x310, MFG2_PWR_CON),
	REGNAME(scpsys, 0x314, MFG3_PWR_CON),
	REGNAME(scpsys, 0x318, MFG4_PWR_CON),
	REGNAME(scpsys, 0x31C, MFG5_PWR_CON),
	REGNAME(scpsys, 0x320, MFG6_PWR_CON),
	REGNAME(scpsys, 0x324, IFR_PWR_CON),
	REGNAME(scpsys, 0x328, IFR_SUB_PWR_CON),
	REGNAME(scpsys, 0x32C, DPY_PWR_CON),
	REGNAME(scpsys, 0x330, ISP_PWR_CON),
	REGNAME(scpsys, 0x334, ISP2_PWR_CON),
	REGNAME(scpsys, 0x338, IPE_PWR_CON),
	REGNAME(scpsys, 0x33C, VDE_PWR_CON),
	REGNAME(scpsys, 0x340, VDE2_PWR_CON),
	REGNAME(scpsys, 0x344, VEN_PWR_CON),
	REGNAME(scpsys, 0x348, VEN_CORE1_PWR_CON),
	REGNAME(scpsys, 0x34C, MDP_PWR_CON),
	REGNAME(scpsys, 0x350, DIS_PWR_CON),
	REGNAME(scpsys, 0x354, AUDIO_PWR_CON),
	REGNAME(scpsys, 0x358, ADSP_PWR_CON),
	REGNAME(scpsys, 0x35C, CAM_PWR_CON),
	REGNAME(scpsys, 0x360, CAM_RAWA_PWR_CON),
	REGNAME(scpsys, 0x364, CAM_RAWB_PWR_CON),
	REGNAME(scpsys, 0x368, CAM_RAWC_PWR_CON),
	REGNAME(scpsys, 0x3AC, DP_TX_PWR_CON),
	REGNAME(scpsys, 0x3C4, DPY2_PWR_CON),
	REGNAME(scpsys, 0x670, SPM_CROSS_WAKE_M01_REQ),
	REGNAME(scpsys, 0x398, MD_EXT_BUCK_ISO_CON),
	REGNAME(scpsys, 0x39C, EXT_BUCK_ISO),

	REGNAME(audio, 0x0000, AUDIO_TOP_CON0),
	REGNAME(audio, 0x0004, AUDIO_TOP_CON1),
	REGNAME(audio, 0x0008, AUDIO_TOP_CON2),

	REGNAME(scp_par, 0x0180, ADSP_SW_CG),

	REGNAME(camsys, 0x0000, CAMSYS_CG_CON),
	REGNAME(cam_rawa_sys, 0x0000, CAMSYS_RAWA_CG_CON),
	REGNAME(cam_rawb_sys, 0x0000, CAMSYS_RAWB_CG_CON),
	REGNAME(cam_rawc_sys, 0x0000, CAMSYS_RAWC_CG_CON),

	REGNAME(img1sys, 0x0000, IMG1_CG_CON),
	REGNAME(img2sys, 0x0000, IMG2_CG_CON),
	REGNAME(ipesys, 0x0000, IPE_CG_CON),

	REGNAME(infracfg,  0x090, MODULE_SW_CG_0),
	REGNAME(infracfg,  0x094, MODULE_SW_CG_1),
	REGNAME(infracfg,  0x0ac, MODULE_SW_CG_2),
	REGNAME(infracfg,  0x0c8, MODULE_SW_CG_3),
	REGNAME(infracfg,  0x0d8, MODULE_SW_CG_4),
	REGNAME(infracfg,  0xe98, MM_HANG_FREE),

	/* BUS STATUS */
	REGNAME(infracfg_dbg,  0x0220, INFRA_TOPAXI_PROTECTEN),
	REGNAME(infracfg_dbg,  0x0224, INFRA_TOPAXI_PROTECTEN_STA0),
	REGNAME(infracfg_dbg,  0x0228, INFRA_TOPAXI_PROTECTEN_STA1),
	REGNAME(infracfg_dbg,  0x0250, INFRA_TOPAXI_PROTECTEN_1),
	REGNAME(infracfg_dbg,  0x0254, INFRA_TOPAXI_PROTECTEN_STA0_1),
	REGNAME(infracfg_dbg,  0x0258, INFRA_TOPAXI_PROTECTEN_STA1_1),
	REGNAME(infracfg_dbg,  0x02C0, INFRA_TOPAXI_PROTECTEN_MCU),
	REGNAME(infracfg_dbg,  0x02C4, INFRA_TOPAXI_PROTECTEN_MCU_STA0),
	REGNAME(infracfg_dbg,  0x02C8, INFRA_TOPAXI_PROTECTEN_MCU_STA1),
	REGNAME(infracfg_dbg,  0x02D0, INFRA_TOPAXI_PROTECTEN_MM),
	REGNAME(infracfg_dbg,  0x02E8, INFRA_TOPAXI_PROTECTEN_MM_STA0),
	REGNAME(infracfg_dbg,  0x02EC, INFRA_TOPAXI_PROTECTEN_MM_STA1),
	REGNAME(infracfg_dbg,  0x0710, INFRA_TOPAXI_PROTECTEN_2),
	REGNAME(infracfg_dbg,  0x0720, INFRA_TOPAXI_PROTECTEN_STA0_2),
	REGNAME(infracfg_dbg,  0x0724, INFRA_TOPAXI_PROTECTEN_STA1_2),
	REGNAME(infracfg_dbg,  0x0DC8, INFRA_TOPAXI_PROTECTEN_MM_2),
	REGNAME(infracfg_dbg,  0x0DD4, INFRA_TOPAXI_PROTECTEN_MM_2_STA0),
	REGNAME(infracfg_dbg,  0x0DD8, INFRA_TOPAXI_PROTECTEN_MM_2_STA1),
	REGNAME(infracfg_dbg,  0x0B80, INFRA_TOPAXI_PROTECTEN_VDNR),
	REGNAME(infracfg_dbg,  0x0B8C, INFRA_TOPAXI_PROTECTEN_VDNR_STA0),
	REGNAME(infracfg_dbg,  0x0B90, INFRA_TOPAXI_PROTECTEN_VDNR_STA1),
	REGNAME(infracfg_dbg,  0x0BA0, INFRA_TOPAXI_PROTECTEN_VDNR1),
	REGNAME(infracfg_dbg,  0x0BAC, INFRA_TOPAXI_PROTECTEN_VDNR_1_STA0),
	REGNAME(infracfg_dbg,  0x0BB0, INFRA_TOPAXI_PROTECTEN_VDNR_1_STA1),
	REGNAME(infracfg_dbg,  0x0BB4, INFRA_TOPAXI_PROTECTEN_SUB_VDNR),
	REGNAME(infracfg_dbg,  0x0BC0, INFRA_TOPAXI_PROTECTEN_SUB_VDNR_STA0),
	REGNAME(infracfg_dbg,  0x0BC4, INFRA_TOPAXI_PROTECTEN_SUB_VDNR_STA1),

	REGNAME(ipu0,  0x000, IPU0_CORE_CG),
	REGNAME(ipu1,  0x000, IPU1_CORE_CG),
	REGNAME(ipu2,  0x000, IPU2_CORE_CG),
	REGNAME(ipu_conn,  0x000, IPU_CONN_CG),
	REGNAME(ipu_vcore,  0x000, IPU_VCORE_CG),

	REGNAME(mfgsys, 0x0000, MFG_CG_CON),

	REGNAME(mmsys, 0x100, MM_CG_CON0),
	REGNAME(mmsys, 0x110, MM_CG_CON1),
	REGNAME(mmsys, 0x1a0, MM_CG_CON2),

	REGNAME(mdpsys, 0x100, MDP_CG_CON0),
	REGNAME(mdpsys, 0x104, MDP_CG_SET0),
	REGNAME(mdpsys, 0x114, MDP_CG_SET1),
	REGNAME(mdpsys, 0x124, MDP_CG_SET2),


	REGNAME(vdecsys, 0x0000, VDEC_CKEN_SET),
	REGNAME(vdecsys, 0x0008, VDEC_LARB1_CKEN_SET),
	REGNAME(vdecsys, 0x0200, VDEC_LAT_CKEN_SET),

	REGNAME(vdec_soc_sys, 0x0000, VDEC_SOC_CKEN_SET),
	REGNAME(vdec_soc_sys, 0x0008, VDEC_SOC_LARB1_CKEN_SET),
	REGNAME(vdec_soc_sys, 0x0200, VDEC_SOC_LAT_CKEN_SET),

	REGNAME(venc_c1_sys, 0x0000, VENC_C1_CG_CON),

	REGNAME(vencsys, 0x0000, VENC_CG_CON),
	{}
};

static const struct regname *get_all_regnames(void)
{
	return rn;
}

static void __init init_regbase(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rb) - 1; i++) {
		if (!rb[i].phys)
			continue;

		rb[i].virt = ioremap_nocache(rb[i].phys, 0x1000);
	}
}

/*
 * clkdbg vf table
 */

struct mtk_vf {
	const char *name;
	int freq_table[4];
};

#define MTK_VF_TABLE(_n, _freq0, _freq1, _freq2, _freq3) {		\
		.name = _n,		\
		.freq_table = {_freq0, _freq1, _freq2, _freq3},	\
	}

/*
 * Opp0 : 0.725v
 * Opp1 : 0.65v
 * Opp2 : 0.60v
 * Opp3 : 0.575v
 */
static struct mtk_vf vf_table[] = {
	/* name, opp0, opp1, opp2, opp3 */
	MTK_VF_TABLE("axi_sel", 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("spm_sel", 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("scp_sel", 416000, 312000, 273000, 218400),
	MTK_VF_TABLE("bus_aximem_sel", 364000, 273000, 273000, 218400),
	MTK_VF_TABLE("disp_sel", 546000, 416000, 312000, 249600),
	MTK_VF_TABLE("mdp_sel", 594000, 416000, 312000, 273000),
	MTK_VF_TABLE("img1_sel", 624000, 416000, 343750, 273000),
	MTK_VF_TABLE("img2_sel", 624000, 416000, 343750, 273000),
	MTK_VF_TABLE("ipe_sel", 546000, 416000, 312000, 273000),
	MTK_VF_TABLE("dpe_sel", 546000, 458333, 364000, 312000),
	MTK_VF_TABLE("cam_sel", 624000, 499200, 392857, 312000),
	MTK_VF_TABLE("ccu_sel", 499200, 392857, 364000, 312000),
	/* APU CORE Power: 0.575v, 0.725v, 0.825v - vcore-less */
	/* GPU DVFS - vcore-less */
	MTK_VF_TABLE("camtg_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg2_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg3_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg4_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("uart_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("spi_sel", 109200, 109200, 109200, 109200),
	MTK_VF_TABLE("msdc50_0_hclk_sel", 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("msdc50_0_sel", 416000, 416000, 416000, 416000),
	MTK_VF_TABLE("msdc30_1_sel", 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("audio_sel", 54600, 54600, 54600, 54600),
	MTK_VF_TABLE("aud_intbus_sel", 136500, 136500, 136500, 136500),
	MTK_VF_TABLE("pwrap_ulposc_sel", 65000, 65000, 65000, 65000),
	MTK_VF_TABLE("atb_sel", 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("sspm_sel", 364000, 312000, 273000, 273000),
	MTK_VF_TABLE("dp_sel", 148500, 148500, 148500, 148500),
	MTK_VF_TABLE("scam_sel", 109200, 109200, 109200, 109200),
	MTK_VF_TABLE("disp_pwm_sel", 130000, 130000, 130000, 130000),
	MTK_VF_TABLE("usb_top_sel", 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("ssusb_xhci_sel", 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("i2c_sel", 124800, 124800, 124800, 124800),
	MTK_VF_TABLE("seninf_sel", 499200, 499200, 499200, 416000),
	MTK_VF_TABLE("seninf1_sel", 499200, 499200, 499200, 416000),
	MTK_VF_TABLE("seninf2_sel", 499200, 499200, 499200, 416000),
	MTK_VF_TABLE("seninf3_sel", 499200, 499200, 499200, 416000),
	MTK_VF_TABLE("dxcc_sel", 273000, 273000, 273000, 273000),
	MTK_VF_TABLE("aud_engen1_sel", 45158, 45158, 45158, 45158),
	MTK_VF_TABLE("aud_engen2_sel", 49152, 49152, 49152, 49152),
	MTK_VF_TABLE("aes_ufsfde_sel", 546000, 546000, 546000, 416000),
	MTK_VF_TABLE("ufs_sel", 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("aud_1_sel", 180634, 180634, 180634, 180634),
	MTK_VF_TABLE("aud_2_sel", 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("adsp_sel", 700000, 700000, 700000, 700000),
	MTK_VF_TABLE("dpmaif_main_sel", 364000, 364000, 364000, 273000),
	MTK_VF_TABLE("venc_sel", 624000, 416000, 312000, 273000),
	MTK_VF_TABLE("vdec_sel", 546000, 416000, 312000, 249600),
	MTK_VF_TABLE("vdec_lat_sel", 546000, 458333, 356571, 312000),
	MTK_VF_TABLE("camtm_sel", 208000, 208000, 208000, 208000),
	MTK_VF_TABLE("pwm_sel", 78000, 78000, 78000, 78000),
	MTK_VF_TABLE("audio_h_sel", 196608, 196608, 196608, 196608),
	MTK_VF_TABLE("camtg5_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("camtg6_sel", 52000, 52000, 52000, 52000),
	MTK_VF_TABLE("mcupm_sel", 156000, 156000, 156000, 156000),
	MTK_VF_TABLE("spmi_mst_sel", 32500, 32500, 32500, 32500),
	MTK_VF_TABLE("dvfsrc_sel", 26000, 26000, 26000, 26000),
	{},
};

#define clk_readl(addr)		readl(addr)
#define clk_writel(addr, val)	\
	do { writel(val, addr); wmb(); } while (0) /* sync write */

#define _CKGEN(x)		(rb[topckgen].virt + (x))
#define CLK_CFG_0		_CKGEN(0x10)

#define _SCPSYS(x)		(rb[scpsys].virt + (x))
#define SPM_PWR_STATUS		_SCPSYS(0x16C)
#define SPM_PWR_STATUS_2ND	_SCPSYS(0x170)

#ifdef CONFIG_MTK_DEVAPC
static void devapc_dump_regs(void)
{
	print_subsys_reg(scpsys);
	print_subsys_reg(topckgen);
	print_subsys_reg(infracfg);
	print_subsys_reg(infracfg_dbg);
	print_subsys_reg(apmixed);
}

static struct devapc_vio_callbacks devapc_vio_handle = {
	.id = DEVAPC_SUBSYS_CLKMGR,
	.debug_dump = devapc_dump_regs,
};
#endif

/******************* TOPCKGEN Subsys *******************************/
void warn_vcore(int opp, const char *clk_name, int rate, int id)
{
#if defined(CONFIG_MACH_MT6893)
	if (opp >= 1)
		opp = opp - 1;
#endif

	if ((opp >= 0) && (id >= 0) && (vf_table[id].freq_table[opp] > 0) &&
			((rate/1000) > (vf_table[id].freq_table[opp]))) {
		pr_notice("%s Choose %d FAIL!!!![MAX(%d/%d): %d]\r\n",
				clk_name, rate/1000, id, opp,
				vf_table[id].freq_table[opp]);

		BUG_ON(1);
	}
}

static int mtk_mux2id(const char **mux_name)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(vf_table) - 1 && vf_table[i].name; i++) {
		if (strcmp(*mux_name, vf_table[i].name) == 0)
			return i;
	}
	return -2;
}

/* The clocks have a mechanism for synchronizing rate changes. */
static int mtk_clk_rate_change(struct notifier_block *nb,
					  unsigned long flags, void *data)
{
	struct clk_notifier_data *ndata = data;
	struct clk_hw *hw = __clk_get_hw(ndata->clk);
	const char *clk_name = __clk_get_name(hw->clk);
	int vcore_opp = get_sw_req_vcore_opp();

	if (flags == PRE_RATE_CHANGE && clk_name) {
		warn_vcore(vcore_opp, clk_name,
			ndata->new_rate, mtk_mux2id(&clk_name));
	}
	return NOTIFY_OK;
}

static struct notifier_block mtk_clk_notifier = {
	.notifier_call = mtk_clk_rate_change,
};

int mtk_clk_check_muxes(void)
{
#if CHECK_VCORE_FREQ
	struct clk *clk;
	int i;

	for (i = 0; i < ARRAY_SIZE(vf_table) - 1 && vf_table[i].name; i++) {
		clk = __clk_lookup(vf_table[i].name);
		clk_notifier_register(clk, &mtk_clk_notifier);
	}
#else
	clk_notifier_register(NULL, &mtk_clk_notifier);
#endif
	return 0;
}

static const struct fmeter_clk *get_all_fmeter_clks(void)
{
	return get_fmeter_clks();
}

static u32 fmeter_freq_op(const struct fmeter_clk *fclk)
{
	if (fclk->type == ABIST)
		return mt_get_abist_freq(fclk->id);
	else if (fclk->type == CKGEN)
		return mt_get_ckgen_freq(fclk->id);
	return 0;
}

/*
 * clkdbg dump_state
 */

static const char * const *get_all_clk_names(void)
{
	return get_mt6885_all_clk_names();
}

/*
 * clkdbg pwr_status
 */
static const char * const *get_pwr_names(void)
{
	static const char * const pwr_names[] = {
		[0] = "MD",
		[1] = "CONN",
		[2] = "MFG0",
		[3] = "MFG1",
		[4] = "MFG2",
		[5] = "MFG3",
		[6] = "MFG4",
		[7] = "MFG5",
		[8] = "MFG6",
		[9] = "INFRA",
		[10] = "SUB_INFRA",
		[11] = "DDRPHY",
		[12] = "ISP",
		[13] = "ISP2",
		[14] = "IPE",
		[15] = "VDEC",
		[16] = "VDEC2",
		[17] = "VEN",
		[18] = "VEN_CORE1",
		[19] = "MDP",
		[20] = "DISP",
		[21] = "AUDIO",
		[22] = "ADSP",
		[23] = "CAM",
		[24] = "CAM_RAWA",
		[25] = "CAM_RAWB",
		[26] = "CAM_RAWC",
		[27] = "DP_TX",
		[28] = "DDRPHY2",
		[29] = "(Reserved)",
		[30] = "(Reserved)",
		[31] = "(Reserved)",
	};

	return pwr_names;
}

/*
 * clkdbg dump_clks
 */

void setup_provider_clk(struct provider_clk *pvdck)
{
	static const struct {
		const char *pvdname;
		u32 pwr_mask;
	} pvd_pwr_mask[] = {
	};

	int i;
	const char *pvdname = pvdck->provider_name;

	if (!pvdname)
		return;

	for (i = 0; i < ARRAY_SIZE(pvd_pwr_mask); i++) {
		if (strcmp(pvdname, pvd_pwr_mask[i].pvdname) == 0) {
			pvdck->pwr_mask = pvd_pwr_mask[i].pwr_mask;
			return;
		}
	}
}

/*
 * pwr stat check functions
 */
static  bool is_pwr_on(struct provider_clk *pvdck)
{
	struct clk *c = pvdck->ck;
	struct clk_hw *c_hw = __clk_get_hw(c);

	return clk_hw_is_prepared(c_hw);
}

/*
 * chip_ver functions
 */

static int clkdbg_chip_ver(struct seq_file *s, void *v)
{
	static const char * const sw_ver_name[] = {
		"CHIP_SW_VER_01",
		"CHIP_SW_VER_02",
		"CHIP_SW_VER_03",
		"CHIP_SW_VER_04",
	};

	seq_printf(s, "mt_get_chip_sw_ver(): %d (%s)\n", 0, sw_ver_name[0]);

	return 0;
}

/*
 * init functions
 */

static struct clkdbg_ops clkdbg_mt6885_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_regnames = get_all_regnames,
	.get_all_clk_names = get_all_clk_names,
	.get_pwr_names = get_pwr_names,
	.setup_provider_clk = setup_provider_clk,
	.is_pwr_on = is_pwr_on,
};

static void __init init_custom_cmds(void)
{
	static const struct cmd_fn cmds[] = {
		CMDFN("chip_ver", clkdbg_chip_ver),
		{}
	};

	set_custom_cmds(cmds);
}

static int __init clkdbg_mt6885_init(void)
{
	init_regbase();

	init_custom_cmds();
	set_clkdbg_ops(&clkdbg_mt6885_ops);

#ifdef CONFIG_MTK_DEVAPC
	register_devapc_vio_callback(&devapc_vio_handle);
#endif

#if DUMP_INIT_STATE
	print_regs();
	print_fmeter_all();
#endif /* DUMP_INIT_STATE */

	mtk_clk_check_muxes();

	return 0;
}
subsys_initcall(clkdbg_mt6885_init);

/*
 * MT6885: for mtcmos debug
 */
static bool is_valid_reg(void __iomem *addr)
{
#ifdef CONFIG_64BIT
	return ((u64)addr & 0xf0000000) != 0UL ||
			(((u64)addr >> 32U) & 0xf0000000) != 0UL;
#else
	return ((u32)addr & 0xf0000000) != 0U;
#endif
}

void print_subsys_reg(enum dbg_sys_id id)
{
	struct regbase *rb_dump;
	const struct regname *rns = &rn[0];
	int i;

	if (rns == NULL)
		return;

	if (id >= dbg_sys_num || id < 0) {
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
