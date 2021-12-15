/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef MTK_POWER_GS_ARRAY_H
#define MTK_POWER_GS_ARRAY_H

extern void mt_power_gs_sp_dump(void);
extern unsigned int golden_read_reg(unsigned int addr);

/* PMIC 6365 */
extern const unsigned int *AP_PMIC_REG_6365_gs_deepidle___lp_mp3_32kless;
extern unsigned int AP_PMIC_REG_6365_gs_deepidle___lp_mp3_32kless_len;

extern const unsigned int *AP_PMIC_REG_6365_gs_sodi3p0_32kless;
extern unsigned int AP_PMIC_REG_6365_gs_sodi3p0_32kless_len;

extern const unsigned int *AP_PMIC_REG_6365_gs_suspend_32kless;
extern unsigned int AP_PMIC_REG_6365_gs_suspend_32kless_len;

/* PMIC 6315 */
extern const unsigned int *AP_PMIC_REG_MT6315_gs_deepidle___lp_mp3_32kless;
extern unsigned int AP_PMIC_REG_MT6315_gs_deepidle___lp_mp3_32kless_len;

extern const unsigned int *AP_PMIC_REG_MT6315_gs_sodi3p0_32kless;
extern unsigned int AP_PMIC_REG_MT6315_gs_sodi3p0_32kless_len;

extern const unsigned int *AP_PMIC_REG_MT6315_gs_suspend_32kless;
extern unsigned int AP_PMIC_REG_MT6315_gs_suspend_32kless_len;

extern const unsigned int *AP_CG_Golden_Setting_tcl_gs_dpidle;
extern unsigned int AP_CG_Golden_Setting_tcl_gs_dpidle_len;

extern const unsigned int *AP_CG_Golden_Setting_tcl_gs_suspend;
extern unsigned int AP_CG_Golden_Setting_tcl_gs_suspend_len;

extern const unsigned int *AP_CG_Golden_Setting_tcl_gs_sodi;
extern unsigned int AP_CG_Golden_Setting_tcl_gs_sodi_len;

extern const unsigned int *AP_DCM_Golden_Setting_tcl_gs_dpidle;
extern unsigned int AP_DCM_Golden_Setting_tcl_gs_dpidle_len;

extern const unsigned int *AP_DCM_Golden_Setting_tcl_gs_suspend;
extern unsigned int AP_DCM_Golden_Setting_tcl_gs_suspend_len;

extern const unsigned int *AP_DCM_Golden_Setting_tcl_gs_sodi;
extern unsigned int AP_DCM_Golden_Setting_tcl_gs_sodi_len;


#endif
