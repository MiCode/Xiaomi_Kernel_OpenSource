// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <dt-bindings/power/mt6833-power.h>

#include "mtk-pd-chk.h"
#include "clkchk-mt6833.h"

#define TAG				"[pdchk] "
#define BUG_ON_CHK_ENABLE		0

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
	SWCG("afe_tdm_ck"),
	SWCG("afe_adc"),
	SWCG("afe_dac"),
	SWCG("afe_dac_predis"),
	SWCG("afe_tml"),
	SWCG("afe_nle"),
	SWCG("afe_i2s1_bclk"),
	SWCG("afe_i2s2_bclk"),
	SWCG("afe_i2s3_bclk"),
	SWCG("afe_i2s4_bclk"),
	SWCG("afe_connsys_i2s_asrc"),
	SWCG("afe_general1_asrc"),
	SWCG("afe_general2_asrc"),
	SWCG("afe_dac_hires"),
	SWCG("afe_adc_hires"),
	SWCG("afe_adc_hires_tml"),
	SWCG("afe_adda6_adc"),
	SWCG("afe_adda6_adc_hires"),
	SWCG("afe_3rd_dac"),
	SWCG("afe_3rd_dac_predis"),
	SWCG("afe_3rd_dac_tml"),
	SWCG("afe_3rd_dac_hires"),
	SWCG("afe_i2s5_bclk"),
	SWCG(NULL),
};
/* camsys_main */
struct pd_check_swcg camsys_main_swcgs[] = {
	SWCG("cam_m_larb13"),
	SWCG("cam_m_dfp_vad"),
	SWCG("cam_m_larb14"),
	SWCG("cam_m_cam"),
	SWCG("cam_m_camtg"),
	SWCG("cam_m_seninf"),
	SWCG("cam_m_camsv1"),
	SWCG("cam_m_camsv2"),
	SWCG("cam_m_camsv3"),
	SWCG("cam_m_ccu0"),
	SWCG("cam_m_ccu1"),
	SWCG("cam_m_mraw0"),
	SWCG("cam_m_fake_eng"),
	SWCG("cam_m_ccu_gals"),
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
/* dispsys_config */
struct pd_check_swcg dispsys_config_swcgs[] = {
	SWCG("mm_disp_mutex0"),
	SWCG("mm_apb_bus"),
	SWCG("mm_disp_ovl0"),
	SWCG("mm_disp_rdma0"),
	SWCG("mm_disp_ovl0_2l"),
	SWCG("mm_disp_wdma0"),
	SWCG("mm_disp_rsz0"),
	SWCG("mm_disp_aal0"),
	SWCG("mm_disp_ccorr0"),
	SWCG("mm_disp_color0"),
	SWCG("mm_smi_infra"),
	SWCG("mm_disp_gamma0"),
	SWCG("mm_disp_postmask0"),
	SWCG("mm_disp_dither0"),
	SWCG("mm_smi_common"),
	SWCG("mm_dsi0"),
	SWCG("mm_disp_fake_eng0"),
	SWCG("mm_disp_fake_eng1"),
	SWCG("mm_smi_gals"),
	SWCG("mm_smi_iommu"),
	SWCG("mm_dsi0_dsi_domain"),
	SWCG("mm_disp_26m_ck"),
	SWCG(NULL),
};
/* imgsys1 */
struct pd_check_swcg imgsys1_swcgs[] = {
	SWCG("imgsys1_larb9"),
	SWCG("imgsys1_larb10"),
	SWCG("imgsys1_dip"),
	SWCG("imgsys1_gals"),
	SWCG(NULL),
};
/* imgsys2 */
struct pd_check_swcg imgsys2_swcgs[] = {
	SWCG("imgsys2_larb9"),
	SWCG("imgsys2_larb10"),
	SWCG("imgsys2_mfb"),
	SWCG("imgsys2_wpe"),
	SWCG("imgsys2_mss"),
	SWCG("imgsys2_gals"),
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
	SWCG("ipe_dpe"),
	SWCG("ipe_gals"),
	SWCG(NULL),
};
/* mdpsys_config */
struct pd_check_swcg mdpsys_config_swcgs[] = {
	SWCG("mdp_rdma0"),
	SWCG("mdp_tdshp0"),
	SWCG("mdp_img_dl_async0"),
	SWCG("mdp_img_dl_async1"),
	SWCG("mdp_smi0"),
	SWCG("mdp_apb_bus"),
	SWCG("mdp_wrot0"),
	SWCG("mdp_rsz0"),
	SWCG("mdp_hdr0"),
	SWCG("mdp_mutex0"),
	SWCG("mdp_wrot1"),
	SWCG("mdp_rsz1"),
	SWCG("mdp_fake_eng0"),
	SWCG("mdp_aal0"),
	SWCG("mdp_img_dl_rel0_as0"),
	SWCG("mdp_img_dl_rel1_as1"),
	SWCG(NULL),
};
/* vdec_gcon_base */
struct pd_check_swcg vdec_gcon_base_swcgs[] = {
	SWCG("vde2_larb1_cken"),
	SWCG("vde2_vdec_cken"),
	SWCG("vde2_vdec_active"),
	SWCG(NULL),
};
/* venc_gcon */
struct pd_check_swcg venc_gcon_swcgs[] = {
	SWCG("ven1_cke0_larb"),
	SWCG("ven1_cke1_venc"),
	SWCG("ven1_cke2_jpgenc"),
	SWCG("ven1_cke5_gals"),
	SWCG(NULL),
};

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
	{MT6833_POWER_DOMAIN_AUDIO, afe_swcgs, afe},
	{MT6833_POWER_DOMAIN_CAM, camsys_main_swcgs, cam_m},
	{MT6833_POWER_DOMAIN_CAM_RAWA, camsys_rawa_swcgs, cam_ra},
	{MT6833_POWER_DOMAIN_CAM_RAWB, camsys_rawb_swcgs, cam_rb},
	{MT6833_POWER_DOMAIN_DISP, dispsys_config_swcgs, mm},
	{MT6833_POWER_DOMAIN_ISP, imgsys1_swcgs, imgsys1},
	{MT6833_POWER_DOMAIN_ISP2, imgsys2_swcgs, imgsys2},
	{MT6833_POWER_DOMAIN_IPE, ipesys_swcgs, ipe},
	{MT6833_POWER_DOMAIN_DISP, mdpsys_config_swcgs, mdp},
	{MT6833_POWER_DOMAIN_VDEC, vdec_gcon_base_swcgs, vde2},
	{MT6833_POWER_DOMAIN_VENC, venc_gcon_swcgs, ven1},
};

static struct pd_check_swcg *get_subsys_cg(unsigned int id)
{
	int i;

	if (id >= MT6833_POWER_DOMAIN_NR)
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

	if (id >= MT6833_POWER_DOMAIN_NR)
		return;

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].pd_id == id)
			print_subsys_reg_mt6833(mtk_subsys_check[i].chk_id);
	}
}

unsigned int pd_list[] = {
	MT6833_POWER_DOMAIN_MD,
	MT6833_POWER_DOMAIN_CONN,
	MT6833_POWER_DOMAIN_MFG0,
	MT6833_POWER_DOMAIN_MFG1,
	MT6833_POWER_DOMAIN_MFG2,
	MT6833_POWER_DOMAIN_MFG3,
	MT6833_POWER_DOMAIN_ISP,
	MT6833_POWER_DOMAIN_ISP2,
	MT6833_POWER_DOMAIN_IPE,
	MT6833_POWER_DOMAIN_VDEC,
	MT6833_POWER_DOMAIN_VENC,
	MT6833_POWER_DOMAIN_DISP,
	MT6833_POWER_DOMAIN_AUDIO,
	MT6833_POWER_DOMAIN_CAM,
	MT6833_POWER_DOMAIN_CAM_RAWA,
	MT6833_POWER_DOMAIN_CAM_RAWB,
	MT6833_POWER_DOMAIN_APU,
};

static bool is_in_pd_list(unsigned int id)
{
	int i;

	if (id >= MT6833_POWER_DOMAIN_NR)
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

	print_subsys_reg_mt6833(spm);
	print_subsys_reg_mt6833(top);
	print_subsys_reg_mt6833(infracfg);
	print_subsys_reg_mt6833(apmixed);

	if (id >= MT6833_POWER_DOMAIN_NR)
		return;

	if (pwr_sta == PD_PWR_ON) {
		for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
			if (mtk_subsys_check[i].pd_id == id)
				print_subsys_reg_mt6833(mtk_subsys_check[i].chk_id);
		}
	}

	BUG_ON(1);
}

static void log_dump(unsigned int id, unsigned int pwr_sta)
{
	if (id >= MT6833_POWER_DOMAIN_NR)
		return;

	if (id == MT6833_POWER_DOMAIN_MD) {
		print_subsys_reg_mt6833(infracfg);
		print_subsys_reg_mt6833(spm);
	}
}

static struct pd_sta pd_pwr_msk[] = {
	{MT6833_POWER_DOMAIN_MD, PWR_STA, 0x00000001},
	{MT6833_POWER_DOMAIN_CONN, PWR_STA, 0x00000002},
	{MT6833_POWER_DOMAIN_MFG0, PWR_STA, 0x00000004},
	{MT6833_POWER_DOMAIN_MFG1, PWR_STA, 0x00000008},
	{MT6833_POWER_DOMAIN_MFG2, PWR_STA, 0x00000010},
	{MT6833_POWER_DOMAIN_MFG3, PWR_STA, 0x00000020},
	{MT6833_POWER_DOMAIN_ISP, PWR_STA, 0x00002000},
	{MT6833_POWER_DOMAIN_ISP2, PWR_STA, 0x00004000},
	{MT6833_POWER_DOMAIN_IPE, PWR_STA, 0x00008000},
	{MT6833_POWER_DOMAIN_VDEC, PWR_STA, 0x00010000},
	{MT6833_POWER_DOMAIN_VENC, PWR_STA, 0x00040000},
	{MT6833_POWER_DOMAIN_DISP, PWR_STA, 0x00200000},
	{MT6833_POWER_DOMAIN_AUDIO, PWR_STA, 0x00400000},
	{MT6833_POWER_DOMAIN_CAM, PWR_STA, 0x00800000},
	{MT6833_POWER_DOMAIN_CAM_RAWA, PWR_STA, 0x01000000},
	{MT6833_POWER_DOMAIN_CAM_RAWB, PWR_STA, 0x02000000},
	{MT6833_POWER_DOMAIN_APU, OTHER_STA, 0x00000020},
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
	MT6833_POWER_DOMAIN_MFG0,
	MT6833_POWER_DOMAIN_MFG1,
	MT6833_POWER_DOMAIN_MFG2,
	MT6833_POWER_DOMAIN_MFG3,
	MT6833_POWER_DOMAIN_ISP,
	MT6833_POWER_DOMAIN_ISP2,
	MT6833_POWER_DOMAIN_IPE,
	MT6833_POWER_DOMAIN_VDEC,
	MT6833_POWER_DOMAIN_VENC,
	MT6833_POWER_DOMAIN_DISP,
	MT6833_POWER_DOMAIN_CAM,
	MT6833_POWER_DOMAIN_CAM_RAWA,
	MT6833_POWER_DOMAIN_CAM_RAWB,
	MT6833_POWER_DOMAIN_APU,
	PD_NULL,
};

static int notice_mtcmos_id[] = {
	MT6833_POWER_DOMAIN_MD,
	MT6833_POWER_DOMAIN_CONN,
	MT6833_POWER_DOMAIN_AUDIO,
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

static struct pdchk_ops pdchk_mt6833_ops = {
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

static int pd_chk_mt6833_probe(struct platform_device *pdev)
{
	pdchk_common_init(&pdchk_mt6833_ops);

	return 0;
}

static struct platform_driver pd_chk_mt6833_drv = {
	.probe = pd_chk_mt6833_probe,
	.driver = {
		.name = "pd-chk-mt6833",
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

	pd_chk_dev = platform_device_register_simple("pd-chk-mt6833", -1, NULL, 0);
	if (IS_ERR(pd_chk_dev))
		pr_warn("unable to register pd-chk device");

	return platform_driver_register(&pd_chk_mt6833_drv);
}

static void __exit pd_chk_exit(void)
{
	platform_driver_unregister(&pd_chk_mt6833_drv);
}

subsys_initcall(pd_chk_init);
module_exit(pd_chk_exit);
MODULE_LICENSE("GPL");
