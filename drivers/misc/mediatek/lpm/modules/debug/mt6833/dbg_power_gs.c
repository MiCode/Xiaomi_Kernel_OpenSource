// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "mtk_power_gs_array.h"
#include <lpm_call.h>
#include <lpm_call_type.h>

#include <gs/lpm_pwr_gs.h>
#include <gs/v1/lpm_power_gs.h>

int mt6833_pwr_gs_set(unsigned int type, const struct lpm_data *val)
{
	int ret = 0;

	if (val->d.v_u32 & GS_PMIC)
		ret = lpm_pwr_gs_compare(LPM_GS_CMP_PMIC, type);

	if (ret)
		return ret;

#ifdef MTK_LPM_GS_PLAT_CLK_DUMP_SUPPORT
	if (val->d.v_u32 & GS_DCM)
		ret = lpm_pwr_gs_compare_by_type(
			LPM_GS_CMP_CLK, type, GS_DCM);
	if (ret)
		return ret;

	if (val->d.v_u32 & GS_CG)
		ret = lpm_pwr_gs_compare_by_type(
			LPM_GS_CMP_CLK, type, GS_CG);
#endif
	return ret;
}

struct lpm_callee mt6833_pwr_gs_callee = {
	.uid = LPM_CALLEE_PWR_GS,
	.i.simple = {
		.set = mt6833_pwr_gs_set,
	},
};

#ifdef MTK_LPM_GS_PLAT_CLK_DUMP_SUPPORT
struct lpm_gs_clk mt6833_clk_cg = {
	.type = GS_CG,
	.name = "CG",
};

struct lpm_gs_clk mt6833_clk_dcm = {
	.type = GS_DCM,
	.name = "DCM",
};

struct lpm_gs_clk *mt6833_clks[] = {
	&mt6833_clk_dcm,
	&mt6833_clk_cg,
	NULL,
};

int mt6833_power_gs_clk_user_attach(struct lpm_gs_clk *p)
{
	/* Set compare golden setting for scenario */
	if (!strcmp(p->name, "CG")) {
		p->user[LPM_PWR_GS_TYPE_SUSPEND].name = "suspend";
		p->user[LPM_PWR_GS_TYPE_SUSPEND].array =
			AP_CG_Golden_Setting_tcl_gs_suspend;
		p->user[LPM_PWR_GS_TYPE_SUSPEND].array_sz =
			AP_CG_Golden_Setting_tcl_gs_suspend_len;

		p->user[LPM_PWR_GS_TYPE_VCORELP_26M].name = "sodi3";
		p->user[LPM_PWR_GS_TYPE_VCORELP_26M].array =
			AP_CG_Golden_Setting_tcl_gs_sodi;
		p->user[LPM_PWR_GS_TYPE_VCORELP_26M].array_sz =
			AP_CG_Golden_Setting_tcl_gs_sodi_len;

		p->user[LPM_PWR_GS_TYPE_VCORELP].name = "dpidle";
		p->user[LPM_PWR_GS_TYPE_VCORELP].array =
			AP_CG_Golden_Setting_tcl_gs_dpidle;
		p->user[LPM_PWR_GS_TYPE_VCORELP].array_sz =
			AP_CG_Golden_Setting_tcl_gs_dpidle_len;
	} else if (!strcmp(p->name, "DCM")) {
		p->user[LPM_PWR_GS_TYPE_SUSPEND].name = "suspend";
		p->user[LPM_PWR_GS_TYPE_SUSPEND].array =
			AP_DCM_Golden_Setting_tcl_gs_suspend;
		p->user[LPM_PWR_GS_TYPE_SUSPEND].array_sz =
			AP_DCM_Golden_Setting_tcl_gs_suspend_len;

		p->user[LPM_PWR_GS_TYPE_VCORELP_26M].name = "sodi3";
		p->user[LPM_PWR_GS_TYPE_VCORELP_26M].array =
			AP_DCM_Golden_Setting_tcl_gs_sodi;
		p->user[LPM_PWR_GS_TYPE_VCORELP_26M].array_sz =
			AP_DCM_Golden_Setting_tcl_gs_sodi_len;

		p->user[LPM_PWR_GS_TYPE_VCORELP].name = "dpidle";
		p->user[LPM_PWR_GS_TYPE_VCORELP].array =
			AP_DCM_Golden_Setting_tcl_gs_dpidle;
		p->user[LPM_PWR_GS_TYPE_VCORELP].array_sz =
			AP_DCM_Golden_Setting_tcl_gs_dpidle_len;
	}
	return 0;
}

struct lpm_gs_clk_info mt6833_clk_infos = {
	.attach = mt6833_power_gs_clk_user_attach,
	.dcm = mt6833_clks,
};
#endif

/* PMIC */
struct lpm_gs_pmic mt6833_pmic6365 = {
	.type = GS_PMIC,
	.regulator = "vcore",
	.pwr_domain = "6365",
};

struct lpm_gs_pmic mt6833_pmic6315 = {
	.type = GS_PMIC,
	.regulator = "3_vbuck1",
	.pwr_domain = "6315",
};

struct lpm_gs_pmic *mt6833_pmic[] = {
	&mt6833_pmic6365,
	&mt6833_pmic6315,
	NULL,
};

int mt6833_power_gs_pmic_user_attach(struct lpm_gs_pmic *p)
{
	if (!p || !p->regulator)
		return -EINVAL;

	/* Set compare golden setting for scenario */
	if (!strcmp(p->regulator, "vcore")) {
		p->user[LPM_PWR_GS_TYPE_SUSPEND].name = "suspend";
		p->user[LPM_PWR_GS_TYPE_SUSPEND].array =
			AP_PMIC_REG_6365_gs_suspend_32kless;
		p->user[LPM_PWR_GS_TYPE_SUSPEND].array_sz =
			AP_PMIC_REG_6365_gs_suspend_32kless_len;

		p->user[LPM_PWR_GS_TYPE_VCORELP_26M].name = "sodi3";
		p->user[LPM_PWR_GS_TYPE_VCORELP_26M].array =
			AP_PMIC_REG_6365_gs_sodi3p0_32kless;
		p->user[LPM_PWR_GS_TYPE_VCORELP_26M].array_sz =
			AP_PMIC_REG_6365_gs_sodi3p0_32kless_len;

		p->user[LPM_PWR_GS_TYPE_VCORELP].name = "dpidle";
		p->user[LPM_PWR_GS_TYPE_VCORELP].array =
			AP_PMIC_REG_6365_gs_deepidle___lp_mp3_32kless;
		p->user[LPM_PWR_GS_TYPE_VCORELP].array_sz =
			AP_PMIC_REG_6365_gs_deepidle___lp_mp3_32kless_len;
	} else if (!strcmp(p->regulator, "3_vbuck1")) {
		p->user[LPM_PWR_GS_TYPE_SUSPEND].name = "suspend";
		p->user[LPM_PWR_GS_TYPE_SUSPEND].array =
			AP_PMIC_REG_MT6315_gs_suspend_32kless;
		p->user[LPM_PWR_GS_TYPE_SUSPEND].array_sz =
			AP_PMIC_REG_MT6315_gs_suspend_32kless_len;

		p->user[LPM_PWR_GS_TYPE_VCORELP_26M].name = "sodi3";
		p->user[LPM_PWR_GS_TYPE_VCORELP_26M].array =
			AP_PMIC_REG_MT6315_gs_sodi3p0_32kless;
		p->user[LPM_PWR_GS_TYPE_VCORELP_26M].array_sz =
			AP_PMIC_REG_MT6315_gs_sodi3p0_32kless_len;

		p->user[LPM_PWR_GS_TYPE_VCORELP].name = "dpidle";
		p->user[LPM_PWR_GS_TYPE_VCORELP].array =
			AP_PMIC_REG_MT6315_gs_deepidle___lp_mp3_32kless;
		p->user[LPM_PWR_GS_TYPE_VCORELP].array_sz =
			AP_PMIC_REG_MT6315_gs_deepidle___lp_mp3_32kless_len;
	} else
		return -EINVAL;

	return 0;
}

struct lpm_gs_pmic_info mt6833_pmic_infos = {
	.pmic = mt6833_pmic,
	.attach = mt6833_power_gs_pmic_user_attach,
};

int power_gs_init(void)
{
	lpm_callee_registry(&mt6833_pwr_gs_callee);

	/* initial gs compare method */
	lpm_pwr_gs_common_init();

	/* initial gs pmic information */
	lpm_pwr_gs_compare_init(LPM_GS_CMP_PMIC, &mt6833_pmic_infos);

#ifdef MTK_LPM_GS_PLAT_CLK_DUMP_SUPPORT
	/* initial gs dcm information */
	lpm_pwr_gs_compare_init(LPM_GS_CMP_CLK, &mt6833_clk_infos);
#endif
	return 0;
}

int power_gs_deinit(void)
{
	lpm_pwr_gs_common_deinit();
	return 0;
}

