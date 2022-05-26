// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <dt-bindings/power/mt6855-power.h>

#include "mtk-pd-chk.h"
#include "clkchk-mt6855.h"

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
	SWCG("afe_general3_asrc"),
	SWCG("afe_connsys_i2s_asrc"),
	SWCG("afe_general1_asrc"),
	SWCG("afe_general2_asrc"),
	SWCG("afe_dac_hires"),
	SWCG("afe_adc_hires"),
	SWCG("afe_adc_hires_tml"),
	SWCG("afe_3rd_dac"),
	SWCG("afe_3rd_dac_predis"),
	SWCG("afe_3rd_dac_tml"),
	SWCG("afe_3rd_dac_hires"),
	SWCG("afe_i2s5_bclk"),
	SWCG("afe_i2s6_bclk"),
	SWCG("afe_i2s7_bclk"),
	SWCG("afe_i2s8_bclk"),
	SWCG("afe_i2s9_bclk"),
	SWCG("afe_etdm_in0_bclk"),
	SWCG("afe_etdm_out0_bclk"),
	SWCG("afe_i2s1_bclk"),
	SWCG("afe_i2s2_bclk"),
	SWCG("afe_i2s3_bclk"),
	SWCG("afe_i2s4_bclk"),
	SWCG("afe_etdm_in1_bclk"),
	SWCG("afe_etdm_out1_bclk"),
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
	SWCG("mm_disp_ovl0"),
	SWCG("mm_disp_merge0"),
	SWCG("mm_disp_fake_eng0"),
	SWCG("mm_disp_inlinerot0"),
	SWCG("mm_disp_wdma0"),
	SWCG("mm_disp_fake_eng1"),
	SWCG("mm_disp_dpi0"),
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
	SWCG("mm_disp_ccorr1"),
	SWCG("mm_disp_aal0"),
	SWCG("mm_disp_gamma0"),
	SWCG("mm_disp_postmask0"),
	SWCG("mm_disp_dither0"),
	SWCG("mm_disp_cm0"),
	SWCG("mm_disp_spr0"),
	SWCG("mm_disp_dsc_wrap0"),
	SWCG("mm_CLK0"),
	SWCG("mm_disp_ufbc_wdma0"),
	SWCG("mm_disp_wdma1"),
	SWCG("mm_DP_CLK"),
	SWCG("mm_disp_apb_bus"),
	SWCG("mm_disp_tdshp0"),
	SWCG("mm_disp_c3d0"),
	SWCG("mm_disp_y2r0"),
	SWCG("mm_mdp_aal0"),
	SWCG("mm_disp_chist0"),
	SWCG("mm_disp_chist1"),
	SWCG("mm_disp_ovl0_2l"),
	SWCG("mm_disp_dli_async3"),
	SWCG("mm_disp_dl0_async3"),
	SWCG("mm_disp_ovl1_2l"),
	SWCG("mm_disp_ovl1_2l_nw"),
	SWCG("mm_smi_common"),
	SWCG("mm_dsi_ck"),
	SWCG("mm_dpi_ck"),
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
	SWCG("mdp_rdma1"),
	SWCG("mdp_fg1"),
	SWCG("mdp_hdr1"),
	SWCG("mdp_aal1"),
	SWCG("mdp_rsz1"),
	SWCG("mdp_tdshp1"),
	SWCG("mdp_color1"),
	SWCG("mdp_wrot1"),
	SWCG("mdp_rsz2"),
	SWCG("mdp_wrot2"),
	SWCG("mdp_dlo_async0"),
	SWCG("mdp_rsz3"),
	SWCG("mdp_wrot3"),
	SWCG("mdp_dlo_async1"),
	SWCG("mdp_hre_mdpsys"),
	SWCG("mdp_mm_img_dl_as0"),
	SWCG("mdp_mm_img_dl_as1"),
	SWCG("mdp_img_dl_as0"),
	SWCG("mdp_img_dl_as1"),
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
/* vdec_gcon_base */
struct pd_check_swcg vdec_gcon_base_swcgs[] = {
	SWCG("vde2_larb1_cken"),
	SWCG("vde2_mini_mdp"),
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
	{MT6855_POWER_DOMAIN_AUDIO, afe_swcgs, afe},
	{MT6855_POWER_DOMAIN_CAM_SUBA, camsys_rawa_swcgs, cam_ra},
	{MT6855_POWER_DOMAIN_CAM_SUBB, camsys_rawb_swcgs, cam_rb},
	{MT6855_POWER_DOMAIN_DISP, dispsys_config_swcgs, mm},
	{MT6855_POWER_DOMAIN_ISP_IPE, ipesys_swcgs, ipe},
	{MT6855_POWER_DOMAIN_DISP, mdpsys_config_swcgs, mdp},
	{MT6855_POWER_DOMAIN_MM_INFRA, mminfra_config_swcgs, mminfra_config},
	{MT6855_POWER_DOMAIN_VDE0, vdec_gcon_base_swcgs, vde2},
	{MT6855_POWER_DOMAIN_VEN0, venc_gcon_swcgs, ven1},
};

static struct pd_check_swcg *get_subsys_cg(unsigned int id)
{
	int i;

	if (id >= MT6855_POWER_DOMAIN_NR)
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

	if (id >= MT6855_POWER_DOMAIN_NR)
		return;

	for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
		if (mtk_subsys_check[i].pd_id == id)
			print_subsys_reg_mt6855(mtk_subsys_check[i].chk_id);
	}
}

unsigned int pd_list[] = {
	MT6855_POWER_DOMAIN_MD,
	MT6855_POWER_DOMAIN_CONN,
	//MT6855_POWER_DOMAIN_UFS0_SHUTDOWN,
	MT6855_POWER_DOMAIN_AUDIO,
	MT6855_POWER_DOMAIN_ISP_MAIN,
	MT6855_POWER_DOMAIN_ISP_DIP1,
	MT6855_POWER_DOMAIN_ISP_IPE,
	MT6855_POWER_DOMAIN_VDE0,
	MT6855_POWER_DOMAIN_VEN0,
	MT6855_POWER_DOMAIN_CAM_MAIN,
	MT6855_POWER_DOMAIN_CAM_SUBA,
	MT6855_POWER_DOMAIN_CAM_SUBB,
	MT6855_POWER_DOMAIN_DISP,
	MT6855_POWER_DOMAIN_MM_INFRA,
	MT6855_POWER_DOMAIN_MM_PROC_DORMANT,
	MT6855_POWER_DOMAIN_CSI_RX,
	MT6855_POWER_DOMAIN_MFG1,
	MT6855_POWER_DOMAIN_MFG2,
};

static bool is_in_pd_list(unsigned int id)
{
	int i;

	if (id >= MT6855_POWER_DOMAIN_NR)
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

	print_subsys_reg_mt6855(spm);
	print_subsys_reg_mt6855(top);
	print_subsys_reg_mt6855(infracfg);
	print_subsys_reg_mt6855(apmixed);
	print_subsys_reg_mt6855(mfg_ao);
	print_subsys_reg_mt6855(vlpcfg);
	print_subsys_reg_mt6855(vlp_ck);

	if (id >= MT6855_POWER_DOMAIN_NR)
		return;

	if (pwr_sta == PD_PWR_ON) {
		for (i = 0; i < ARRAY_SIZE(mtk_subsys_check); i++) {
			if (mtk_subsys_check[i].pd_id == id)
				print_subsys_reg_mt6855(mtk_subsys_check[i].chk_id);
		}
	}

	BUG_ON(1);
}

static void log_dump(unsigned int id, unsigned int pwr_sta)
{
	if (id >= MT6855_POWER_DOMAIN_NR)
		return;

	if (id == MT6855_POWER_DOMAIN_MD) {
		print_subsys_reg_mt6855(infracfg);
		print_subsys_reg_mt6855(spm);
		print_subsys_reg_mt6855(vlpcfg);
	}
}

static struct pd_sta pd_pwr_msk[] = {
	{MT6855_POWER_DOMAIN_MD, PWR_STA, 0x00000001},
	{MT6855_POWER_DOMAIN_CONN, PWR_STA, 0x00000002},
	{MT6855_POWER_DOMAIN_UFS0_SHUTDOWN, PWR_STA, 0x00000010},
	{MT6855_POWER_DOMAIN_AUDIO, PWR_STA, 0x00000020},
	{MT6855_POWER_DOMAIN_ISP_MAIN, PWR_STA, 0x00000200},
	{MT6855_POWER_DOMAIN_ISP_DIP1, PWR_STA, 0x00000400},
	{MT6855_POWER_DOMAIN_ISP_IPE, PWR_STA, 0x00000800},
	{MT6855_POWER_DOMAIN_VDE0, PWR_STA, 0x00002000},
	{MT6855_POWER_DOMAIN_VEN0, PWR_STA, 0x00008000},
	{MT6855_POWER_DOMAIN_CAM_MAIN, PWR_STA, 0x00020000},
	{MT6855_POWER_DOMAIN_CAM_SUBA, PWR_STA, 0x00080000},
	{MT6855_POWER_DOMAIN_CAM_SUBB, PWR_STA, 0x00100000},
	{MT6855_POWER_DOMAIN_DISP, PWR_STA, 0x02000000},
	{MT6855_POWER_DOMAIN_MM_INFRA, PWR_STA, 0x08000000},
	{MT6855_POWER_DOMAIN_MM_PROC_DORMANT, PWR_STA, 0x10000000},
	{MT6855_POWER_DOMAIN_CSI_RX, PWR_CON_STA, 0xC0000000},
	{MT6855_POWER_DOMAIN_MFG1, XPU_PWR_STA, 0x00000004},
	{MT6855_POWER_DOMAIN_MFG2, XPU_PWR_STA, 0x00000008},
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
	MT6855_POWER_DOMAIN_UFS0_SHUTDOWN,
	MT6855_POWER_DOMAIN_ISP_MAIN,
	MT6855_POWER_DOMAIN_ISP_DIP1,
	MT6855_POWER_DOMAIN_ISP_IPE,
	MT6855_POWER_DOMAIN_VDE0,
	MT6855_POWER_DOMAIN_VEN0,
	MT6855_POWER_DOMAIN_CAM_MAIN,
	MT6855_POWER_DOMAIN_CAM_SUBA,
	MT6855_POWER_DOMAIN_CAM_SUBB,
	MT6855_POWER_DOMAIN_DISP,
	MT6855_POWER_DOMAIN_MM_INFRA,
	MT6855_POWER_DOMAIN_MM_PROC_DORMANT,
	MT6855_POWER_DOMAIN_CSI_RX,
	MT6855_POWER_DOMAIN_MFG1,
	MT6855_POWER_DOMAIN_MFG2,
	PD_NULL,
};

static int notice_mtcmos_id[] = {
	MT6855_POWER_DOMAIN_MD,
	MT6855_POWER_DOMAIN_CONN,
	MT6855_POWER_DOMAIN_AUDIO,
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

static struct pdchk_ops pdchk_mt6855_ops = {
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

static int pd_chk_mt6855_probe(struct platform_device *pdev)
{
	pdchk_common_init(&pdchk_mt6855_ops);

	return 0;
}

static const struct of_device_id of_match_pdchk_mt6855[] = {
	{
		.compatible = "mediatek,mt6855-pdchk",
	}, {
		/* sentinel */
	}
};

static struct platform_driver pd_chk_mt6855_drv = {
	.probe = pd_chk_mt6855_probe,
	.driver = {
		.name = "pd-chk-mt6855",
		.owner = THIS_MODULE,
		.pm = &pdchk_dev_pm_ops,
		.of_match_table = of_match_pdchk_mt6855,
	},
};

/*
 * init functions
 */

static int __init pd_chk_init(void)
{
	return platform_driver_register(&pd_chk_mt6855_drv);
}

static void __exit pd_chk_exit(void)
{
	platform_driver_unregister(&pd_chk_mt6855_drv);
}

subsys_initcall(pd_chk_init);
module_exit(pd_chk_exit);
MODULE_LICENSE("GPL");
