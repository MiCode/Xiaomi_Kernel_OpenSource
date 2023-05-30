// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Chuan-Wen Chen <chuan-wen.chen@mediatek.com>
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <dt-bindings/power/mt6835-power.h>

#include "mtk-pd-chk.h"
#include "clkchk-mt6835.h"

#define TAG				"[pdchk] "
#define BUG_ON_CHK_ENABLE		0
static unsigned int suspend_cnt;

/*
 * The clk names in Mediatek CCF.
 */

/* afe */
struct pd_check_swcg afe_swcgs[] = {
	SWCG("afe_afe"),
	SWCG("afe_22m"),
	SWCG("afe_24m"),
	SWCG("afe_apll2_tuner"),
	SWCG("afe_apll_tuner"),
	SWCG("afe_adc"),
	SWCG("afe_dac"),
	SWCG("afe_dac_predis"),
	SWCG("afe_tml"),
	SWCG("afe_nle"),
	SWCG("afe_general3_asrc"),
	SWCG("afe_connsys_i2s_asrc"),
	SWCG("afe_general1_asrc"),
	SWCG("afe_general2_asrc"),
	SWCG("afe_dac_hires"),
	SWCG("afe_adc_hires"),
	SWCG("afe_adc_hires_tml"),
	SWCG("afe_i2s5_bclk"),
	SWCG("afe_i2s1_bclk"),
	SWCG("afe_i2s2_bclk"),
	SWCG("afe_i2s3_bclk"),
	SWCG("afe_i2s4_bclk"),
	SWCG(NULL),
};
/* dispsys_config */
struct pd_check_swcg dispsys_config_swcgs[] = {
	SWCG("mm_disp_mutex0"),
	SWCG("mm_disp_ovl0"),
	SWCG("mm_disp_fake_eng0"),
	SWCG("mm_inlinerot0"),
	SWCG("mm_disp_wdma0"),
	SWCG("mm_disp_fake_eng1"),
	SWCG("mm_disp_dbi0"),
	SWCG("mm_disp_ovl0_2l_nw"),
	SWCG("mm_disp_rdma0"),
	SWCG("mm_disp_rdma1"),
	SWCG("mm_disp_dli_async0"),
	SWCG("mm_disp_dli_async1"),
	SWCG("mm_disp_dli_async2"),
	SWCG("mm_disp_dlo_async0"),
	SWCG("mm_disp_dlo_async1"),
	SWCG("mm_disp_dlo_async2"),
	SWCG("mm_disp_rsz0"),
	SWCG("mm_disp_color0"),
	SWCG("mm_disp_ccorr0"),
	SWCG("mm_disp_aal0"),
	SWCG("mm_disp_gamma0"),
	SWCG("mm_disp_postmask0"),
	SWCG("mm_disp_dither0"),
	SWCG("mm_disp_dummy_mod_b0"),
	SWCG("mm_clk0"),
	SWCG("mm_dp_clk"),
	SWCG("mm_apb_bus"),
	SWCG("mm_disp_tdshp0"),
	SWCG("mm_disp_c3d0"),
	SWCG("mm_disp_y2r0"),
	SWCG("mm_mdp_aal0"),
	SWCG("mm_disp_chist0"),
	SWCG("mm_disp_chist1"),
	SWCG("mm_disp_ovl0_2l"),
	SWCG("mm_dli_async3"),
	SWCG("mm_dlo_async3"),
	SWCG("mm_dummy_mod_b1"),
	SWCG("mm_disp_ovl1_2l"),
	SWCG("mm_dummy_mod_b2"),
	SWCG("mm_dummy_mod_b3"),
	SWCG("mm_dummy_mod_b4"),
	SWCG("mm_disp_ovl1_2l_nw"),
	SWCG("mm_dummy_mod_b5"),
	SWCG("mm_dummy_mod_b6"),
	SWCG("mm_dummy_mod_b7"),
	SWCG("mm_smi_iommu"),
	SWCG("mm_clk"),
	SWCG("mm_disp_dbpi"),
	SWCG("mm_disp_hrt_urgent"),
	SWCG(NULL),
};
/* imgsys1 */
struct pd_check_swcg imgsys1_swcgs[] = {
	SWCG("imgsys1_larb9"),
	SWCG("imgsys1_dip"),
	SWCG("imgsys1_gals"),
	SWCG(NULL),
};
/* vdec_gcon_base */
struct pd_check_swcg vdec_gcon_base_swcgs[] = {
	SWCG("vde2_larb1_cken"),
	SWCG("vde2_vdec_cken"),
	SWCG("vde2_vdec_active"),
	SWCG("vde2_vdec_cken_eng"),
	SWCG(NULL),
};
/* venc_gcon */
struct pd_check_swcg venc_gcon_swcgs[] = {
	SWCG("ven1_cke0_larb"),
	SWCG("ven1_cke1_venc"),
	SWCG("ven1_cke2_jpgenc"),
	SWCG(NULL),
};
/* camsys_main */
struct pd_check_swcg camsys_main_swcgs[] = {
	SWCG("cam_m_larb13"),
	SWCG("cam_m_larb14"),
	SWCG("cam_m_cam"),
	SWCG("cam_m_camtg"),
	SWCG("cam_m_seninf"),
	SWCG("cam_m_camsv1"),
	SWCG("cam_m_camsv2"),
	SWCG("cam_m_camsv3"),
	SWCG("cam_m_fake_eng"),
	SWCG("cam_m_cam2mm_gals"),
	SWCG(NULL),
};
/* camsys_rawa */
struct pd_check_swcg camsys_rawa_swcgs[] = {
	SWCG("cam_ra_larbx"),
	SWCG("cam_ra_cam"),
	SWCG("cam_ra_camtg"),
	SWCG(NULL),
};
/* camsys_rawb */
struct pd_check_swcg camsys_rawb_swcgs[] = {
	SWCG("cam_rb_larbx"),
	SWCG("cam_rb_cam"),
	SWCG("cam_rb_camtg"),
	SWCG(NULL),
};
/* ipesys */
struct pd_check_swcg ipesys_swcgs[] = {
	SWCG("ipe_larb19"),
	SWCG("ipe_larb20"),
	SWCG("ipe_smi_subcom"),
	SWCG("ipe_fd"),
	SWCG("ipe_fe"),
	SWCG("ipe_rsc"),
	SWCG("ipe_gals"),
	SWCG(NULL),
};
/* mminfra_config */
struct pd_check_swcg mminfra_config_swcgs[] = {
	SWCG("mminfra_gce_d"),
	SWCG("mminfra_gce_m"),
	SWCG("mminfra_smi"),
	SWCG("mminfra_gce_26m"),
	SWCG(NULL),
};
/* mdpsys_config */
struct pd_check_swcg mdpsys_config_swcgs[] = {
	SWCG("mdp_mutex0"),
	SWCG("mdp_apb_bus"),
	SWCG("mdp_smi0"),
	SWCG("mdp_rdma0"),
	SWCG("mdp_fg0"),
	SWCG("mdp_hdr0"),
	SWCG("mdp_aal0"),
	SWCG("mdp_rsz0"),
	SWCG("mdp_tdshp0"),
	SWCG("mdp_color0"),
	SWCG("mdp_wrot0"),
	SWCG("mdp_fake_eng0"),
	SWCG("mdp_dli_async0"),
	SWCG("mdp_dli_async1"),
	SWCG("mdp_rsz2"),
	SWCG("mdp_wrot2"),
	SWCG("mdp_fmm_dl_async0"),
	SWCG("mdp_fmm_dl_async1"),
	SWCG("mdp_fimg_dl_async0"),
	SWCG("mdp_fimg_dl_async1"),
	SWCG(NULL),
};

struct subsys_cgs_check {
	unsigned int pd_id;		/* power domain id */
	int pd_parent;		/* power domain parent id */
	struct pd_check_swcg *swcgs;	/* those CGs that would be checked */
	enum chk_sys_id chk_id;		/*
					 * chk_id is used in
					 * print_subsys_reg() and can be NULL
					 * if not porting ready yet.
					 */
};

struct subsys_cgs_check mtk_subsys_check[] = {
	{MT6835_CHK_PD_AUDIO, PD_NULL, afe_swcgs, afe},
	{MT6835_CHK_PD_DIS0, MT6835_CHK_PD_MM_INFRA, dispsys_config_swcgs, mm},
	{MT6835_CHK_PD_ISP_DIP1, MT6835_CHK_PD_MM_INFRA, imgsys1_swcgs, imgsys1},
	{MT6835_CHK_PD_VDE0, MT6835_CHK_PD_MM_INFRA, vdec_gcon_base_swcgs, vde2},
	{MT6835_CHK_PD_VEN0, MT6835_CHK_PD_MM_INFRA, venc_gcon_swcgs, ven1},
	{MT6835_CHK_PD_CAM_MAIN, MT6835_CHK_PD_MM_INFRA, camsys_main_swcgs, cam_m},
	{MT6835_CHK_PD_CAM_SUBA, MT6835_CHK_PD_CAM_MAIN, camsys_rawa_swcgs, cam_ra},
	{MT6835_CHK_PD_CAM_SUBB, MT6835_CHK_PD_CAM_MAIN, camsys_rawb_swcgs, cam_rb},
	{MT6835_CHK_PD_ISP_IPE, MT6835_CHK_PD_MM_INFRA, ipesys_swcgs, ipe},
	{MT6835_CHK_PD_MM_INFRA, PD_NULL, mminfra_config_swcgs, mminfra_config},
	{MT6835_CHK_PD_DIS0, MT6835_CHK_PD_MM_INFRA, mdpsys_config_swcgs, mdp},
};

static struct pd_check_swcg *get_subsys_cg(unsigned int id)
{
	int i;

	if (id >= MT6835_CHK_PD_NUM)
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

	if (id >= MT6835_CHK_PD_NUM)
		return;

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].pd_id == id)
			print_subsys_reg_mt6835(mtk_subsys_check[i].chk_id);
	}
}

unsigned int pd_list[] = {
	MT6835_CHK_PD_MD1,
	MT6835_CHK_PD_CONN,
	MT6835_CHK_PD_UFS0,
	MT6835_CHK_PD_AUDIO,
	MT6835_CHK_PD_ISP_DIP1,
	MT6835_CHK_PD_ISP_IPE,
	MT6835_CHK_PD_VDE0,
	MT6835_CHK_PD_VEN0,
	MT6835_CHK_PD_CAM_MAIN,
	MT6835_CHK_PD_CAM_SUBA,
	MT6835_CHK_PD_CAM_SUBB,
	MT6835_CHK_PD_DIS0,
	MT6835_CHK_PD_MM_INFRA,
	MT6835_CHK_PD_MM_PROC,
	MT6835_CHK_PD_MFG0,
	MT6835_CHK_PD_MFG1,
	MT6835_CHK_PD_MFG2,
	MT6835_CHK_PD_MFG3,
};

static bool is_in_pd_list(unsigned int id)
{
	int i;

	if (id >= MT6835_CHK_PD_NUM)
		return false;

	for (i = 0; i < ARRAY_SIZE(pd_list); i++) {
		if (id == pd_list[i])
			return true;
	}

	return false;
}

static enum chk_sys_id debug_dump_id[] = {
	spm,
	top,
	infracfg,
	apmixed,
	vlpcfg,
	vlp_ck,
	chk_sys_num,
};

static void debug_dump(unsigned int id, unsigned int pwr_sta)
{
	int i;

	if (id >= MT6835_CHK_PD_NUM)
		return;

	set_subsys_reg_dump_mt6835(debug_dump_id);

	if (pwr_sta == PD_PWR_ON) {
		for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
			if (mtk_subsys_check[i].pd_id == id)
				print_subsys_reg_mt6835(mtk_subsys_check[i].chk_id);
		}
	}

	get_subsys_reg_dump_mt6835();

	BUG_ON(1);
}

static enum chk_sys_id log_dump_id[] = {
	infracfg,
	spm,
	vlpcfg,
	chk_sys_num,
};

static void log_dump(unsigned int id, unsigned int pwr_sta)
{
	if (id >= MT6835_CHK_PD_NUM)
		return;

	if (id == MT6835_CHK_PD_MD1) {
		set_subsys_reg_dump_mt6835(log_dump_id);
		get_subsys_reg_dump_mt6835();
	}
}

static struct pd_sta pd_pwr_sta[] = {
	{MT6835_CHK_PD_MD1, spm, 0x0E00, GENMASK(31, 30)},
	{MT6835_CHK_PD_CONN, spm, 0x0E04, GENMASK(31, 30)},
	{MT6835_CHK_PD_UFS0, spm, 0x0E10, GENMASK(31, 30)},
	{MT6835_CHK_PD_AUDIO, spm, 0x0E14, GENMASK(31, 30)},
	{MT6835_CHK_PD_ISP_DIP1, spm, 0x0E28, GENMASK(31, 30)},
	{MT6835_CHK_PD_ISP_IPE, spm, 0x0E2C, GENMASK(31, 30)},
	{MT6835_CHK_PD_VDE0, spm, 0x0E34, GENMASK(31, 30)},
	{MT6835_CHK_PD_VEN0, spm, 0x0E3C, GENMASK(31, 30)},
	{MT6835_CHK_PD_CAM_MAIN, spm, 0x0E44, GENMASK(31, 30)},
	{MT6835_CHK_PD_CAM_SUBA, spm, 0x0E4C, GENMASK(31, 30)},
	{MT6835_CHK_PD_CAM_SUBB, spm, 0x0E50, GENMASK(31, 30)},
	{MT6835_CHK_PD_DIS0, spm, 0x0E64, GENMASK(31, 30)},
	{MT6835_CHK_PD_MM_INFRA, spm, 0x0E6C, GENMASK(31, 30)},
	{MT6835_CHK_PD_MM_PROC, spm, 0x0E70, GENMASK(31, 30)},
	{MT6835_CHK_PD_MFG0, spm, 0x0EB8, GENMASK(31, 30)},
	{MT6835_CHK_PD_MFG1, spm, 0x0EBC, GENMASK(31, 30)},
	{MT6835_CHK_PD_MFG2, spm, 0x0EC0, GENMASK(31, 30)},
	{MT6835_CHK_PD_MFG3, spm, 0x0EC4, GENMASK(31, 30)},
};

static u32 get_pd_pwr_status(int pd_id)
{
	u32 val;
	int i;

	if (pd_id == PD_NULL || pd_id > ARRAY_SIZE(pd_pwr_sta))
		return 0;

	for (i = 0; i < ARRAY_SIZE(pd_pwr_sta); i++) {
		if (pd_id == pd_pwr_sta[i].pd_id) {
			val = get_mt6835_reg_value(pd_pwr_sta[i].base, pd_pwr_sta[i].ofs);
			if ((val & pd_pwr_sta[i].msk) == pd_pwr_sta[i].msk)
				return 1;
			else
				return 0;
		}
	}

	return 0;
}

static int off_mtcmos_id[] = {
	MT6835_CHK_PD_UFS0,
	MT6835_CHK_PD_ISP_DIP1,
	MT6835_CHK_PD_ISP_IPE,
	MT6835_CHK_PD_VDE0,
	MT6835_CHK_PD_VEN0,
	MT6835_CHK_PD_CAM_MAIN,
	MT6835_CHK_PD_CAM_SUBA,
	MT6835_CHK_PD_CAM_SUBB,
	MT6835_CHK_PD_DIS0,
	MT6835_CHK_PD_MM_INFRA,
	MT6835_CHK_PD_MM_PROC,
	MT6835_CHK_PD_MFG0,
	MT6835_CHK_PD_MFG1,
	MT6835_CHK_PD_MFG2,
	MT6835_CHK_PD_MFG3,
	PD_NULL,
};

static int notice_mtcmos_id[] = {
	MT6835_CHK_PD_MD1,
	MT6835_CHK_PD_CONN,
	MT6835_CHK_PD_AUDIO,
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

static int suspend_allow_id[] = {
	PD_NULL,
};

static int *get_suspend_allow_id(void)
{
	return suspend_allow_id;
}

static bool pdchk_suspend_retry(bool reset_cnt)
{
	if (reset_cnt == true) {
		suspend_cnt = 0;
		return true;
	}

	suspend_cnt++;
	pr_notice("%s: suspend cnt: %d\n", __func__, suspend_cnt);

	if (suspend_cnt < 2)
		return false;

	return true;
}

/*
 * init functions
 */

static struct pdchk_ops pdchk_mt6835_ops = {
	.get_subsys_cg = get_subsys_cg,
	.dump_subsys_reg = dump_subsys_reg,
	.is_in_pd_list = is_in_pd_list,
	.debug_dump = debug_dump,
	.log_dump = log_dump,
	.get_pd_pwr_status = get_pd_pwr_status,
	.get_off_mtcmos_id = get_off_mtcmos_id,
	.get_notice_mtcmos_id = get_notice_mtcmos_id,
	.is_mtcmos_chk_bug_on = is_mtcmos_chk_bug_on,
	.get_suspend_allow_id = get_suspend_allow_id,
	.pdchk_suspend_retry = pdchk_suspend_retry,
};

static int pd_chk_mt6835_probe(struct platform_device *pdev)
{
	suspend_cnt = 0;

	pdchk_common_init(&pdchk_mt6835_ops);

	return 0;
}

static struct platform_driver pd_chk_mt6835_drv = {
	.probe = pd_chk_mt6835_probe,
	.driver = {
		.name = "pd-chk-mt6835",
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

	pd_chk_dev = platform_device_register_simple("pd-chk-mt6835", -1, NULL, 0);
	if (IS_ERR(pd_chk_dev))
		pr_warn("unable to register pd-chk device");

	return platform_driver_register(&pd_chk_mt6835_drv);
}

static void __exit pd_chk_exit(void)
{
	platform_driver_unregister(&pd_chk_mt6835_drv);
}

subsys_initcall(pd_chk_init);
module_exit(pd_chk_exit);
MODULE_LICENSE("GPL");
