/*
 * Copyright (C) 2015 MediaTek Inc.
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

#include "mtk_power_gs.h"
#include "mtk_power_gs_array.h"
#include "mt-plat/mtk_rtc.h"

void mt_power_gs_table_init(void)
{
	mt_power_gs_base_remap_init("Suspend ", "CG  ",
				AP_CG_Golden_Setting_tcl_gs_suspend,
				AP_CG_Golden_Setting_tcl_gs_suspend_len);
	mt_power_gs_base_remap_init("Suspend ", "DCM ",
				AP_DCM_Golden_Setting_tcl_gs_suspend,
				AP_DCM_Golden_Setting_tcl_gs_suspend_len);
	mt_power_gs_base_remap_init("DPIdle ", "CG  ",
				AP_CG_Golden_Setting_tcl_gs_dpidle,
				AP_CG_Golden_Setting_tcl_gs_dpidle_len);
	mt_power_gs_base_remap_init("DPIdle ", "DCM ",
				AP_DCM_Golden_Setting_tcl_gs_dpidle,
				AP_DCM_Golden_Setting_tcl_gs_dpidle_len);
	mt_power_gs_base_remap_init("SODI ", "CG  ",
				AP_CG_Golden_Setting_tcl_gs_sodi,
				AP_CG_Golden_Setting_tcl_gs_sodi_len);
	mt_power_gs_base_remap_init("SODI ", "DCM ",
				AP_DCM_Golden_Setting_tcl_gs_sodi,
				AP_DCM_Golden_Setting_tcl_gs_sodi_len);
}

void mt_power_gs_suspend_compare(unsigned int dump_flag)
{
	if (dump_flag & GS_PMIC) {
		/* 32k-less */
		pr_info("Power_gs: %s in 32k-less\n", __func__);
		mt_power_gs_compare("Suspend ", "6357",
			MT6357_MT6357_PMIC_Register_Mapping_E1_gs_suspend_32kless,
			MT6357_MT6357_PMIC_Register_Mapping_E1_gs_suspend_32kless_len);
	}

	if (dump_flag & GS_CG) {
		mt_power_gs_compare("Suspend ", "CG  ",
				AP_CG_Golden_Setting_tcl_gs_suspend,
				AP_CG_Golden_Setting_tcl_gs_suspend_len);
	}

	if (dump_flag & GS_DCM) {
		mt_power_gs_compare("Suspend ", "DCM ",
				AP_DCM_Golden_Setting_tcl_gs_suspend,
				AP_DCM_Golden_Setting_tcl_gs_suspend_len);
	}

	mt_power_gs_sp_dump();
}

void mt_power_gs_dpidle_compare(unsigned int dump_flag)
{
	if (dump_flag & GS_PMIC) {
		/* 32k-less */
		pr_info("Power_gs: %s in 32k-less\n", __func__);
		mt_power_gs_compare("DPIdle  ", "6357",
			MT6357_MT6357_PMIC_Register_Mapping_E1_gs_deepidle___lp_mp3_32kless,
			MT6357_MT6357_PMIC_Register_Mapping_E1_gs_deepidle___lp_mp3_32kless_len);
	}

	if (dump_flag & GS_CG) {
		mt_power_gs_compare("DPIdle ", "CG  ",
				AP_CG_Golden_Setting_tcl_gs_dpidle,
				AP_CG_Golden_Setting_tcl_gs_dpidle_len);
	}

	if (dump_flag & GS_DCM) {
		mt_power_gs_compare("DPIdle ", "DCM ",
				AP_DCM_Golden_Setting_tcl_gs_dpidle,
				AP_DCM_Golden_Setting_tcl_gs_dpidle_len);
	}

	mt_power_gs_sp_dump();
}

void mt_power_gs_sodi_compare(unsigned int dump_flag)
{
	if (dump_flag & GS_PMIC) {
		/* 32k-less */
		pr_info("Power_gs: %s in 32k-less\n", __func__);
		mt_power_gs_compare("SODI  ", "6357",
			MT6357_MT6357_PMIC_Register_Mapping_E1_gs_sodi3p0_32kless,
			MT6357_MT6357_PMIC_Register_Mapping_E1_gs_sodi3p0_32kless_len);
	}

	if (dump_flag & GS_CG) {
		mt_power_gs_compare("SODI ", "CG  ",
				AP_CG_Golden_Setting_tcl_gs_sodi,
				AP_CG_Golden_Setting_tcl_gs_sodi_len);
	}

	if (dump_flag & GS_DCM) {
		mt_power_gs_compare("SODI ", "DCM ",
				AP_DCM_Golden_Setting_tcl_gs_sodi,
				AP_DCM_Golden_Setting_tcl_gs_sodi_len);
	}

	mt_power_gs_sp_dump();
}
