// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "mtk_power_gs_array.h"
#include <mtk_lpm_call.h>
#include <mtk_lpm_call_type.h>

#include <gs/mtk_lpm_pwr_gs.h>
#include <gs/v1/mtk_power_gs.h>

int mt6885_pwr_gs_set(unsigned int type, const struct mtk_lpm_data *val)
{
	int ret = 0;

	if (val->d.u32 & GS_PMIC)
		ret = mtk_lpm_pwr_gs_compare(MTK_LPM_GS_CMP_PMIC, type);

	if (ret)
		return ret;

#ifdef MTK_LPM_GS_PLAT_CLK_DUMP_SUPPORT
	if (val->d.v_u32 & GS_DCM)
		ret = mtk_lpm_pwr_gs_compare_by_type(
			MTK_LPM_GS_CMP_CLK, type, GS_DCM);
	if (ret)
		return ret;

	if (val->d.v_u32 & GS_CG)
		ret = mtk_lpm_pwr_gs_compare_by_type(
			MTK_LPM_GS_CMP_CLK, type, GS_CG);
#endif
	return ret;
}

struct mtk_lpm_callee mt6885_pwr_gs_callee = {
	.uid = MTK_LPM_CALLEE_PWR_GS,
	.i.simple = {
		.set = mt6885_pwr_gs_set,
	},
};

#ifdef MTK_LPM_GS_PLAT_CLK_DUMP_SUPPORT
struct mtk_lpm_gs_clk mt6885_clk_cg = {
	.type = GS_CG,
	.name = "CG",
};

struct mtk_lpm_gs_clk mt6885_clk_dcm = {
	.type = GS_DCM,
	.name = "DCM",
};

struct mtk_lpm_gs_clk *mt6885_clks[] = {
	&mt6885_clk_dcm,
	&mt6885_clk_cg,
	NULL,
};

int mt6885_power_gs_clk_user_attach(struct mtk_lpm_gs_clk *p)
{
	/* Set compare golden setting for scenario */
	if (!strcmp(p->name, "CG")) {
		p->user[MTK_LPM_PWR_GS_TYPE_SUSPEND].name = "suspend";
		p->user[MTK_LPM_PWR_GS_TYPE_SUSPEND].array =
			AP_CG_Golden_Setting_tcl_gs_suspend;
		p->user[MTK_LPM_PWR_GS_TYPE_SUSPEND].array_sz =
			AP_CG_Golden_Setting_tcl_gs_suspend_len;

		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP_26M].name = "sodi3";
		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP_26M].array =
			AP_CG_Golden_Setting_tcl_gs_sodi;
		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP_26M].array_sz =
			AP_CG_Golden_Setting_tcl_gs_sodi_len;

		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP].name = "dpidle";
		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP].array =
			AP_CG_Golden_Setting_tcl_gs_dpidle;
		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP].array_sz =
			AP_CG_Golden_Setting_tcl_gs_dpidle_len;
	} else if (!strcmp(p->name, "DCM")) {
		p->user[MTK_LPM_PWR_GS_TYPE_SUSPEND].name = "suspend";
		p->user[MTK_LPM_PWR_GS_TYPE_SUSPEND].array =
			AP_DCM_Golden_Setting_tcl_gs_suspend;
		p->user[MTK_LPM_PWR_GS_TYPE_SUSPEND].array_sz =
			AP_DCM_Golden_Setting_tcl_gs_suspend_len;

		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP_26M].name = "sodi3";
		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP_26M].array =
			AP_DCM_Golden_Setting_tcl_gs_sodi;
		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP_26M].array_sz =
			AP_DCM_Golden_Setting_tcl_gs_sodi_len;

		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP].name = "dpidle";
		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP].array =
			AP_DCM_Golden_Setting_tcl_gs_dpidle;
		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP].array_sz =
			AP_DCM_Golden_Setting_tcl_gs_dpidle_len;
	}
	return 0;
}

struct mtk_lpm_gs_clk_info mt6885_clk_infos = {
	.attach = mt6885_power_gs_clk_user_attach,
	.dcm = mt6885_clks,
};
#endif

/* PMIC */
struct mtk_lpm_gs_pmic mt6885_pmic6359 = {
	.type = GS_PMIC,
	.regulator = "vgpu11",
	.pwr_domain = "6359",
};

struct mtk_lpm_gs_pmic mt6885_pmic6315_1 = {
	.type = GS_PMIC,
	.regulator = "6_vbuck1",
	.pwr_domain = "6315",
};

struct mtk_lpm_gs_pmic mt6885_pmic6315_2 = {
	.type = GS_PMIC,
	.regulator = "7_vbuck1",
	.pwr_domain = "6315",
};

struct mtk_lpm_gs_pmic mt6885_pmic6315_3 = {
	.type = GS_PMIC,
	.regulator = "3_vbuck1",
	.pwr_domain = "6315",
};

struct mtk_lpm_gs_pmic *mt6885_pmic[] = {
	&mt6885_pmic6359,
	&mt6885_pmic6315_1,
	&mt6885_pmic6315_2,
	&mt6885_pmic6315_3,
	NULL,
};

int mt6885_power_gs_pmic_user_attach(struct mtk_lpm_gs_pmic *p)
{
	if (!p || !p->regulator)
		return -EINVAL;

	/* Set compare golden setting for scenario */
	if (!strcmp(p->regulator, "vgpu11")) {
		p->user[MTK_LPM_PWR_GS_TYPE_SUSPEND].name = "suspend";
		p->user[MTK_LPM_PWR_GS_TYPE_SUSPEND].array =
			AP_PMIC_REG_6359P_gs_suspend_32kless;
		p->user[MTK_LPM_PWR_GS_TYPE_SUSPEND].array_sz =
			AP_PMIC_REG_6359P_gs_suspend_32kless_len;

		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP_26M].name = "sodi3";
		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP_26M].array =
			AP_PMIC_REG_6359P_gs_sodi3p0_32kless;
		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP_26M].array_sz =
			AP_PMIC_REG_6359P_gs_sodi3p0_32kless_len;

		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP].name = "dpidle";
		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP].array =
			AP_PMIC_REG_6359P_gs_deepidle___lp_mp3_32kless;
		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP].array_sz =
			AP_PMIC_REG_6359P_gs_deepidle___lp_mp3_32kless_len;
	} else if (!strcmp(p->regulator, "6_vbuck1")) {
		p->user[MTK_LPM_PWR_GS_TYPE_SUSPEND].name = "suspend";
		p->user[MTK_LPM_PWR_GS_TYPE_SUSPEND].array =
			AP_PMIC_REG_MT6315_1_gs_suspend_32kless;
		p->user[MTK_LPM_PWR_GS_TYPE_SUSPEND].array_sz =
			AP_PMIC_REG_MT6315_1_gs_suspend_32kless_len;

		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP_26M].name = "sodi3";
		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP_26M].array =
			AP_PMIC_REG_MT6315_1_gs_sodi3p0_32kless;
		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP_26M].array_sz =
			AP_PMIC_REG_MT6315_1_gs_sodi3p0_32kless_len;

		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP].name = "dpidle";
		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP].array =
			AP_PMIC_REG_MT6315_1_gs_deepidle___lp_mp3_32kless;
		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP].array_sz =
			AP_PMIC_REG_MT6315_1_gs_deepidle___lp_mp3_32kless_len;
	} else if (!strcmp(p->regulator, "7_vbuck1")) {
		p->user[MTK_LPM_PWR_GS_TYPE_SUSPEND].name = "suspend";
		p->user[MTK_LPM_PWR_GS_TYPE_SUSPEND].array =
			AP_PMIC_REG_MT6315_2_gs_suspend_32kless;
		p->user[MTK_LPM_PWR_GS_TYPE_SUSPEND].array_sz =
			AP_PMIC_REG_MT6315_2_gs_suspend_32kless_len;

		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP_26M].name = "sodi3";
		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP_26M].array =
			AP_PMIC_REG_MT6315_2_gs_sodi3p0_32kless;
		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP_26M].array_sz =
			AP_PMIC_REG_MT6315_2_gs_sodi3p0_32kless_len;

		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP].name = "dpidle";
		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP].array =
			AP_PMIC_REG_MT6315_2_gs_deepidle___lp_mp3_32kless;
		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP].array_sz =
			AP_PMIC_REG_MT6315_2_gs_deepidle___lp_mp3_32kless_len;
	} else if (!strcmp(p->regulator, "3_vbuck1")) {
		p->user[MTK_LPM_PWR_GS_TYPE_SUSPEND].name = "suspend";
		p->user[MTK_LPM_PWR_GS_TYPE_SUSPEND].array =
			AP_PMIC_REG_MT6315_3_gs_suspend_32kless;
		p->user[MTK_LPM_PWR_GS_TYPE_SUSPEND].array_sz =
			AP_PMIC_REG_MT6315_3_gs_suspend_32kless_len;

		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP_26M].name = "sodi3";
		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP_26M].array =
			AP_PMIC_REG_MT6315_3_gs_sodi3p0_32kless;
		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP_26M].array_sz =
			AP_PMIC_REG_MT6315_3_gs_sodi3p0_32kless_len;

		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP].name = "dpidle";
		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP].array =
			AP_PMIC_REG_MT6315_3_gs_deepidle___lp_mp3_32kless;
		p->user[MTK_LPM_PWR_GS_TYPE_VCORELP].array_sz =
			AP_PMIC_REG_MT6315_3_gs_deepidle___lp_mp3_32kless_len;
	} else
		return -EINVAL;

	return 0;
}

struct mtk_lpm_gs_pmic_info mt6885_pmic_infos = {
	.pmic = mt6885_pmic,
	.attach = mt6885_power_gs_pmic_user_attach,
};

int mt6885_power_gs_init(void)
{
	mtk_lpm_callee_registry(&mt6885_pwr_gs_callee);

	/* initial gs compare method */
	mtk_lpm_pwr_gs_common_init();

	/* initial gs pmic information */
	mtk_lpm_pwr_gs_compare_init(MTK_LPM_GS_CMP_PMIC, &mt6885_pmic_infos);

#ifdef MTK_LPM_GS_PLAT_CLK_DUMP_SUPPORT
	/* initial gs dcm information */
	mtk_lpm_pwr_gs_compare_init(MTK_LPM_GS_CMP_CLK, &mt6885_clk_infos);
#endif
	return 0;
}

int mt6885_power_gs_deinit(void)
{
	mtk_lpm_pwr_gs_common_deinit();
	return 0;
}

