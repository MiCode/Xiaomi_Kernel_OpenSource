// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <dt-bindings/power/mt6893-power.h>

#include "mtk-pd-chk.h"
#include "clkchk-mt6893.h"

#define BUG_ON_CHK_ENABLE	1
#define MAX_CLK_NUM		100

/*
 * The clk names in Mediatek CCF.
 */
/* camsys_main */
struct pd_check_swcg camsys_main_swcgs[] = {
	//SWCG("cam_m_larb13"),
	SWCG("cam_m_dfp_vad"),
	//SWCG("cam_m_larb14"),
	//SWCG("cam_m_larb15"),
	SWCG("cam_m_cam"),
	SWCG("cam_m_camtg"),
	SWCG("cam_m_seninf"),
	SWCG("cam_m_camsv0"),
	SWCG("cam_m_camsv1"),
	SWCG("cam_m_camsv2"),
	SWCG("cam_m_camsv3"),
	SWCG("cam_m_ccu0"),
	SWCG("cam_m_ccu1"),
	SWCG("cam_m_mraw0"),
	SWCG("cam_m_mraw1"),
	SWCG("cam_m_fake_eng"),
	SWCG(NULL),
};
/* camsys_rawa */
struct pd_check_swcg camsys_rawa_swcgs[] = {
	//SWCG("cam_ra_larbx"),
	SWCG("cam_ra_cam"),
	SWCG("cam_ra_camtg"),
	SWCG(NULL),
};
/* camsys_rawb */
struct pd_check_swcg camsys_rawb_swcgs[] = {
	//SWCG("cam_rb_larbx"),
	SWCG("cam_rb_cam"),
	SWCG("cam_rb_camtg"),
	SWCG(NULL),
};
/* camsys_rawc */
struct pd_check_swcg camsys_rawc_swcgs[] = {
	//SWCG("cam_rc_larbx"),
	SWCG("cam_rc_cam"),
	SWCG("cam_rc_camtg"),
	SWCG(NULL),
};
/* imgsys1 */
struct pd_check_swcg imgsys1_swcgs[] = {
	//SWCG("imgsys1_larb9"),
	SWCG("imgsys1_larb10"),
	SWCG("imgsys1_dip"),
	SWCG("imgsys1_mfb"),
	SWCG("imgsys1_wpe"),
	SWCG("imgsys1_mss"),
	SWCG(NULL),
};
/* imgsys2 */
struct pd_check_swcg imgsys2_swcgs[] = {
	//SWCG("imgsys2_larb9"),
	SWCG("imgsys2_larb10"),
	SWCG("imgsys2_dip"),
	SWCG("imgsys2_wpe"),
	SWCG(NULL),
};
/* ipesys */
struct pd_check_swcg ipesys_swcgs[] = {
	//SWCG("ipe_larb19"),
	//SWCG("ipe_larb20"),
	//SWCG("ipe_smi_subcom"),
	SWCG("ipe_fd"),
	SWCG("ipe_fe"),
	SWCG("ipe_rsc"),
	SWCG("ipe_dpe"),
	SWCG(NULL),
};
/* mdpsys_config */
struct pd_check_swcg mdpsys_config_swcgs[] = {
	SWCG("mdp_rdma0"),
	SWCG("mdp_fg0"),
	SWCG("mdp_hdr0"),
	SWCG("mdp_aal0"),
	SWCG("mdp_rsz0"),
	SWCG("mdp_tdshp0"),
	SWCG("mdp_tcc0"),
	SWCG("mdp_wrot0"),
	SWCG("mdp_rdma2"),
	SWCG("mdp_aal2"),
	SWCG("mdp_rsz2"),
	SWCG("mdp_color0"),
	SWCG("mdp_tdshp2"),
	SWCG("mdp_tcc2"),
	SWCG("mdp_wrot2"),
	SWCG("mdp_mutex0"),
	SWCG("mdp_rdma1"),
	SWCG("mdp_fg1"),
	SWCG("mdp_hdr1"),
	SWCG("mdp_aal1"),
	SWCG("mdp_rsz1"),
	SWCG("mdp_tdshp1"),
	SWCG("mdp_tcc1"),
	SWCG("mdp_wrot1"),
	SWCG("mdp_rdma3"),
	SWCG("mdp_aal3"),
	SWCG("mdp_rsz3"),
	SWCG("mdp_color1"),
	SWCG("mdp_tdshp3"),
	SWCG("mdp_tcc3"),
	SWCG("mdp_wrot3"),
	SWCG("mdp_apb_bus"),
	SWCG("mdp_mmsysram"),
	//SWCG("mdp_apmcu_gals"),
	SWCG("mdp_fake_eng0"),
	SWCG("mdp_fake_eng1"),
	//SWCG("mdp_smi0"),
	SWCG("mdp_img_dl_async0"),
	SWCG("mdp_img_dl_async1"),
	SWCG("mdp_img_dl_async2"),
	//SWCG("mdp_smi1"),
	SWCG("mdp_img_dl_async3"),
	SWCG("mdp_reserved42"),
	SWCG("mdp_reserved43"),
	//SWCG("mdp_smi2"),
	SWCG("mdp_reserved45"),
	SWCG("mdp_reserved46"),
	SWCG("mdp_reserved47"),
	SWCG("mdp_img0_dl_as0"),
	SWCG("mdp_img0_dl_as1"),
	SWCG("mdp_img1_dl_as2"),
	SWCG("mdp_img1_dl_as3"),
	SWCG(NULL),
};
/* mfgcfg */
struct pd_check_swcg mfgcfg_swcgs[] = {
	SWCG("mfgcfg_bg3d"),
	SWCG(NULL),
};
/* mmsys_config */
struct pd_check_swcg mmsys_config_swcgs[] = {
	SWCG("mm_disp_rsz0"),
	SWCG("mm_disp_rsz1"),
	SWCG("mm_disp_ovl0"),
	SWCG("mm_inline"),
	SWCG("mm_mdp_tdshp4"),
	SWCG("mm_mdp_tdshp5"),
	SWCG("mm_mdp_aal4"),
	SWCG("mm_mdp_aal5"),
	SWCG("mm_mdp_hdr4"),
	SWCG("mm_mdp_hdr5"),
	SWCG("mm_mdp_rsz4"),
	SWCG("mm_mdp_rsz5"),
	SWCG("mm_mdp_rdma4"),
	SWCG("mm_mdp_rdma5"),
	SWCG("mm_disp_fake_eng0"),
	SWCG("mm_disp_fake_eng1"),
	SWCG("mm_disp_ovl0_2l"),
	SWCG("mm_disp_ovl1_2l"),
	SWCG("mm_disp_ovl2_2l"),
	SWCG("mm_disp_mutex"),
	SWCG("mm_disp_ovl1"),
	SWCG("mm_disp_ovl3_2l"),
	SWCG("mm_disp_ccorr0"),
	SWCG("mm_disp_ccorr1"),
	SWCG("mm_disp_color0"),
	SWCG("mm_disp_color1"),
	SWCG("mm_disp_postmask0"),
	SWCG("mm_disp_postmask1"),
	SWCG("mm_disp_dither0"),
	SWCG("mm_disp_dither1"),
	SWCG("mm_dsi0_mm_clk"),
	SWCG("mm_dsi1_mm_clk"),
	SWCG("mm_disp_gamma0"),
	SWCG("mm_disp_gamma1"),
	SWCG("mm_disp_aal0"),
	SWCG("mm_disp_aal1"),
	SWCG("mm_disp_wdma0"),
	SWCG("mm_disp_wdma1"),
	SWCG("mm_disp_ufbc_wdma0"),
	SWCG("mm_disp_ufbc_wdma1"),
	SWCG("mm_disp_rdma0"),
	SWCG("mm_disp_rdma1"),
	SWCG("mm_disp_rdma4"),
	SWCG("mm_disp_rdma5"),
	SWCG("mm_disp_dsc_wrap"),
	SWCG("mm_dp_intf_mm_clk"),
	SWCG("mm_disp_merge0"),
	SWCG("mm_disp_merge1"),
	//SWCG("mm_smi_common"),
	//SWCG("mm_smi_gals"),
	//SWCG("mm_smi_infra"),
	//SWCG("mm_smi_iommu"),
	SWCG("mm_dsi0_intf_clk"),
	SWCG("mm_dsi1_intf_clk"),
	SWCG("mm_dp_intf_intf_clk"),
	SWCG("mm_26_mhz"),
	SWCG("mm_32_khz"),
	SWCG(NULL),
};
/* vdec_gcon */
struct pd_check_swcg vdec_gcon_swcgs[] = {
	//SWCG("vde2_larb1_cken"),
	//SWCG("vde2_lat_cken"),
	SWCG("vde2_lat_active"),
	SWCG("vde2_lat_cken_eng"),
	//SWCG("vde2_vdec_cken"),
	SWCG("vde2_vdec_active"),
	SWCG("vde2_vdec_cken_eng"),
	SWCG(NULL),
};
/* vdec_soc_gcon */
struct pd_check_swcg vdec_soc_gcon_swcgs[] = {
	//SWCG("vde1_larb1_cken"),
	//SWCG("vde1_lat_cken"),
	SWCG("vde1_lat_active"),
	SWCG("vde1_lat_cken_eng"),
	//SWCG("vde1_vdec_cken"),
	SWCG("vde1_vdec_active"),
	SWCG("vde1_vdec_cken_eng"),
	SWCG(NULL),
};
/* venc_c1_gcon */
struct pd_check_swcg venc_c1_gcon_swcgs[] = {
	SWCG("ven2_cke0_larb"),
	//SWCG("ven2_cke1_venc"),
	SWCG("ven2_cke2_jpgenc"),
	SWCG("ven2_cke3_jpgdec"),
	SWCG("ven2_cke4_jpgdec_c1"),
	SWCG("ven2_cke5_gals"),
	SWCG(NULL),
};
/* venc_gcon */
struct pd_check_swcg venc_gcon_swcgs[] = {
	SWCG("ven1_cke0_larb"),
	//SWCG("ven1_cke1_venc"),
	SWCG("ven1_cke2_jpgenc"),
	SWCG("ven1_cke3_jpgdec"),
	SWCG("ven1_cke4_jpgdec_c1"),
	SWCG("ven1_cke5_gals"),
	SWCG(NULL),
};

/*
 * The clk names in Mediatek CCF.
 */

struct subsys_cgs_check {
	unsigned int pd_id;		/* power domain id */
	struct pd_check_swcg *swcgs;	/* those CGs that would be checked */
	enum chk_sys_id chk_id;		/*
					 * chk_id is used in
					 * print_subsys_reg() and can be NULL
					 * if not porting ready yet.
					 */
};

struct subsys_cgs_check mtk_subsys_check[] = {
	//{MT6893_POWER_DOMAIN_AUDIO, audiosys_swcgs, audsys},
	{MT6893_POWER_DOMAIN_CAM, camsys_main_swcgs, cam_m},
	{MT6893_POWER_DOMAIN_CAM_RAWA, camsys_rawa_swcgs, cam_ra},
	{MT6893_POWER_DOMAIN_CAM_RAWB, camsys_rawb_swcgs, cam_rb},
	{MT6893_POWER_DOMAIN_CAM_RAWC, camsys_rawc_swcgs, cam_rc},
	{MT6893_POWER_DOMAIN_ISP, imgsys1_swcgs, imgsys1},
	{MT6893_POWER_DOMAIN_ISP2, imgsys2_swcgs, imgsys2},
	{MT6893_POWER_DOMAIN_IPE, ipesys_swcgs, ipe},
	{MT6893_POWER_DOMAIN_MDP, mdpsys_config_swcgs, mdp},
	{MT6893_POWER_DOMAIN_MFG1, mfgcfg_swcgs, mfgcfg},
	{MT6893_POWER_DOMAIN_DISP, mmsys_config_swcgs, mm},
	{MT6893_POWER_DOMAIN_VDEC2, vdec_gcon_swcgs, vde2},
	{MT6893_POWER_DOMAIN_VDEC, vdec_soc_gcon_swcgs, vde1},
	{MT6893_POWER_DOMAIN_VENC_CORE1, venc_c1_gcon_swcgs, ven2},
	{MT6893_POWER_DOMAIN_VENC, venc_gcon_swcgs, ven1},
};

static struct pd_check_swcg *get_subsys_cg(unsigned int id)
{
	int i;

	if (id >= MT6893_POWER_DOMAIN_NR)
		return NULL;

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].pd_id == id)
			return mtk_subsys_check[i].swcgs;
	}

	return NULL;
}

static void dump_subsys_reg(unsigned int id)
{
	int i;

	if (id >= MT6893_POWER_DOMAIN_NR)
		return;

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].pd_id == id)
			print_subsys_reg_mt6893(mtk_subsys_check[i].chk_id);
	}
}

unsigned int pd_list[] = {
	MT6893_POWER_DOMAIN_MFG0,
	MT6893_POWER_DOMAIN_MFG1,
	MT6893_POWER_DOMAIN_MFG2,
	MT6893_POWER_DOMAIN_MFG3,
	MT6893_POWER_DOMAIN_MFG4,
	MT6893_POWER_DOMAIN_MFG5,
	MT6893_POWER_DOMAIN_MFG6,
	MT6893_POWER_DOMAIN_ISP,
	MT6893_POWER_DOMAIN_ISP2,
	MT6893_POWER_DOMAIN_IPE,
	MT6893_POWER_DOMAIN_VDEC,
	MT6893_POWER_DOMAIN_VDEC2,
	MT6893_POWER_DOMAIN_VENC,
	MT6893_POWER_DOMAIN_VENC_CORE1,
	MT6893_POWER_DOMAIN_MDP,
	MT6893_POWER_DOMAIN_DISP,
	MT6893_POWER_DOMAIN_AUDIO,
	MT6893_POWER_DOMAIN_ADSP_DORMANT,
	MT6893_POWER_DOMAIN_CAM,
	MT6893_POWER_DOMAIN_CAM_RAWA,
	MT6893_POWER_DOMAIN_CAM_RAWB,
	MT6893_POWER_DOMAIN_CAM_RAWC,
	MT6893_POWER_DOMAIN_MD,
	MT6893_POWER_DOMAIN_CONN,
};

static bool is_in_pd_list(unsigned int id)
{
	int i;

	if (id >= MT6893_POWER_DOMAIN_NR)
		return false;

	for (i = 0; i < ARRAY_SIZE(pd_list); i++) {
		if (id == pd_list[i])
			return true;
	}

	return false;
}

static void debug_dump(unsigned int id, unsigned int pwr_sta)
{
	int i;

	print_subsys_reg_mt6893(spm);
	print_subsys_reg_mt6893(ifrao);
	print_subsys_reg_mt6893(infracfg_ao_bus);
	print_subsys_reg_mt6893(apmixed);
	print_subsys_reg_mt6893(top);

	if (id >= MT6893_POWER_DOMAIN_NR)
		return;

	if (pwr_sta == PD_PWR_ON) {
		for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
			if (mtk_subsys_check[i].pd_id == id)
				print_subsys_reg_mt6893(mtk_subsys_check[i].chk_id);
		}
	}

	BUG_ON(1);
}

static void log_dump(unsigned int id, unsigned int pwr_sta)
{
	if (id >= MT6893_POWER_DOMAIN_NR)
		return;

	if (id == MT6893_POWER_DOMAIN_MD) {
		print_subsys_reg_mt6893(spm);
		print_subsys_reg_mt6893(infracfg_ao_bus);
	}
}

static struct pd_sta pd_pwr_msk[] = {
	{MT6893_POWER_DOMAIN_MFG0, PWR_STA, 0x00000004},
	{MT6893_POWER_DOMAIN_MFG1, PWR_STA, 0x00000008},
	{MT6893_POWER_DOMAIN_MFG2, PWR_STA, 0x00000010},
	{MT6893_POWER_DOMAIN_MFG3, PWR_STA, 0x00000020},
	{MT6893_POWER_DOMAIN_MFG4, PWR_STA, 0x00000040},
	{MT6893_POWER_DOMAIN_MFG5, PWR_STA, 0x00000080},
	{MT6893_POWER_DOMAIN_MFG6, PWR_STA, 0x00000100},
	{MT6893_POWER_DOMAIN_ISP, PWR_STA, 0x00001000},
	{MT6893_POWER_DOMAIN_ISP2, PWR_STA, 0x00002000},
	{MT6893_POWER_DOMAIN_IPE, PWR_STA, 0x00004000},
	{MT6893_POWER_DOMAIN_VDEC, PWR_STA, 0x00008000},
	{MT6893_POWER_DOMAIN_VDEC2, PWR_STA, 0x00010000},
	{MT6893_POWER_DOMAIN_VENC, PWR_STA, 0x00020000},
	{MT6893_POWER_DOMAIN_VENC_CORE1, PWR_STA, 0x00040000},
	{MT6893_POWER_DOMAIN_MDP, PWR_STA, 0x00080000},
	{MT6893_POWER_DOMAIN_DISP, PWR_STA, 0x00100000},
	{MT6893_POWER_DOMAIN_AUDIO, PWR_STA, 0x00200000},
	{MT6893_POWER_DOMAIN_ADSP_DORMANT, PWR_STA, 0x00400000},
	{MT6893_POWER_DOMAIN_CAM, PWR_STA, 0x00800000},
	{MT6893_POWER_DOMAIN_CAM_RAWA, PWR_STA, 0x01000000},
	{MT6893_POWER_DOMAIN_CAM_RAWB, PWR_STA, 0x02000000},
	{MT6893_POWER_DOMAIN_CAM_RAWC, PWR_STA, 0x04000000},
	{MT6893_POWER_DOMAIN_DP_TX, PWR_STA, 0x08000000},
	{MT6893_POWER_DOMAIN_MD, PWR_STA, 0x00000001},
	{MT6893_POWER_DOMAIN_CONN, PWR_STA, 0x00000002},
	{MT6893_POWER_DOMAIN_APU, OTHER_STA, 0x00000020},
};

static struct pd_sta *get_pd_pwr_msk(int pd_id)
{
	int i;

	if (pd_id == PD_NULL || pd_id > ARRAY_SIZE(pd_pwr_msk))
		return NULL;

	for (i = 0; i < ARRAY_SIZE(pd_pwr_msk); i++) {
		if (pd_id == pd_pwr_msk[i].pd_id)
			return &pd_pwr_msk[pd_id];
	}

	return NULL;
}

static int off_mtcmos_id[] = {
	MT6893_POWER_DOMAIN_MFG0,
	MT6893_POWER_DOMAIN_MFG1,
	MT6893_POWER_DOMAIN_MFG2,
	MT6893_POWER_DOMAIN_MFG3,
	MT6893_POWER_DOMAIN_MFG4,
	MT6893_POWER_DOMAIN_MFG5,
	MT6893_POWER_DOMAIN_MFG6,
	MT6893_POWER_DOMAIN_ISP,
	MT6893_POWER_DOMAIN_ISP2,
	MT6893_POWER_DOMAIN_IPE,
	MT6893_POWER_DOMAIN_VDEC,
	MT6893_POWER_DOMAIN_VDEC2,
	MT6893_POWER_DOMAIN_VENC,
	MT6893_POWER_DOMAIN_VENC_CORE1,
	MT6893_POWER_DOMAIN_MDP,
	MT6893_POWER_DOMAIN_DISP,
	MT6893_POWER_DOMAIN_CAM,
	MT6893_POWER_DOMAIN_CAM_RAWA,
	MT6893_POWER_DOMAIN_CAM_RAWB,
	MT6893_POWER_DOMAIN_CAM_RAWC,
	MT6893_POWER_DOMAIN_DP_TX,
	MT6893_POWER_DOMAIN_APU,
	PD_NULL,
};

static int notice_mtcmos_id[] = {
	MT6893_POWER_DOMAIN_AUDIO,
	MT6893_POWER_DOMAIN_ADSP_DORMANT,
	MT6893_POWER_DOMAIN_MD,
	MT6893_POWER_DOMAIN_CONN,
	PD_NULL,
};

static int *get_off_mtcmos_id(void)
{
	return off_mtcmos_id;
}

static int *get_notice_mtcmos_id(void)
{
	return notice_mtcmos_id;
}

static bool is_mtcmos_chk_bug_on(void)
{
#if BUG_ON_CHK_ENABLE
	return true;
#endif
	return false;
}

/*
 * init functions
 */

static struct pdchk_ops pdchk_mt6893_ops = {
	.get_subsys_cg = get_subsys_cg,
	.dump_subsys_reg = dump_subsys_reg,
	.is_in_pd_list = is_in_pd_list,
	.debug_dump = debug_dump,
	.log_dump = log_dump,
	.get_pd_pwr_msk = get_pd_pwr_msk,
	.get_off_mtcmos_id = get_off_mtcmos_id,
	.get_notice_mtcmos_id = get_notice_mtcmos_id,
	.is_mtcmos_chk_bug_on = is_mtcmos_chk_bug_on,
};

static int pd_chk_mt6893_probe(struct platform_device *pdev)
{
	pdchk_common_init(&pdchk_mt6893_ops);

	return 0;
}

static struct platform_driver pd_chk_mt6893_drv = {
	.probe = pd_chk_mt6893_probe,
	.driver = {
		.name = "pd-chk-mt6893",
		.owner = THIS_MODULE,
		.pm = &pdchk_dev_pm_ops,
	},
};

/*
 * init functions
 */

static int __init pd_chk_init(void)
{
	static struct platform_device *pd_chk_dev;

	pd_chk_dev = platform_device_register_simple("pd-chk-mt6893", -1, NULL, 0);
	if (IS_ERR(pd_chk_dev))
		pr_warn("unable to register pd-chk device");

	return platform_driver_register(&pd_chk_mt6893_drv);
}

static void __exit pd_chk_exit(void)
{
	platform_driver_unregister(&pd_chk_mt6893_drv);
}

subsys_initcall(pd_chk_init);
module_exit(pd_chk_exit);
MODULE_LICENSE("GPL");
