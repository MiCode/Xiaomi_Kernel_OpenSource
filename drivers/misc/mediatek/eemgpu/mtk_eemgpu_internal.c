// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/**
 * @file	mtk_eemg_internal.c
 * @brief   Driver for EEM
 *
 */
#define __MTK_EEMG_INTERNAL_C__

#include "mtk_eemgpu_config.h"
#include "mtk_eemgpu.h"
#include "mtk_eemgpu_internal_ap.h"
#include "mtk_eemgpu_internal.h"

/**
 * EEM controllers
 */
struct eemg_ctrl eemg_ctrls[NR_EEMG_CTRL] = {
	[EEMG_CTRL_GPU] = {
		.name = __stringify(EEMG_CTRL_GPU),
		.det_id = EEMG_DET_GPU,
	},
	[EEMG_CTRL_GPU_HI] = {
		.name = __stringify(EEMG_CTRL_GPU_HI),
		.det_id = EEMG_DET_GPU_HI,
	},
};

#define BASE_OP(fn)	.fn = base_ops_ ## fn
struct eemg_det_ops eemg_det_base_ops = {
	BASE_OP(enable_gpu),
	BASE_OP(disable_gpu),
	BASE_OP(disable_locked_gpu),
	BASE_OP(switch_bank_gpu),

	BASE_OP(init01_gpu),
	BASE_OP(init02_gpu),
	BASE_OP(mon_mode_gpu),

	BASE_OP(get_status_gpu),
	BASE_OP(dump_status_gpu),

	BASE_OP(set_phase_gpu),

	BASE_OP(get_temp_gpu),

	BASE_OP(get_volt_gpu),
	BASE_OP(set_volt_gpu),
	BASE_OP(restore_default_volt_gpu),
	BASE_OP(get_freq_table_gpu),
	BASE_OP(get_orig_volt_table_gpu),

	/* platform independent code */
	BASE_OP(volt_2_pmic_gpu),
	BASE_OP(volt_2_eemg),
	BASE_OP(pmic_2_volt_gpu),
	BASE_OP(eemg_2_pmic),
};

struct eemg_det eemg_detectors[NR_EEMG_DET] = {
	[EEMG_DET_GPU] = {
		.name		= __stringify(EEMG_DET_GPU),
		.ops		= &gpu_det_ops,
		.ctrl_id	= EEMG_CTRL_GPU,
		.features	= FEA_INIT02,
		.VMAX		= VMAX_VAL_GPU,
		.VBOOT		= VBOOT_VAL, /* 10uV */
		.VMIN		= VMIN_VAL_GPU,
		.eemg_v_base	= EEMG_V_BASE,
		.eemg_step	= EEMG_STEP,
		.pmic_base	= GPU_PMIC_BASE,
		.pmic_step	= GPU_PMIC_STEP,
		.DETWINDOW	= DETWINDOW_VAL,
		.DTHI		= DTHI_VAL,
		.DTLO		= DTLO_VAL,
		.DETMAX		= DETMAX_VAL,
		.AGECONFIG	= AGECONFIG_VAL,
		.AGEM		= AGEM_VAL,
		.loo_role       = LOW_BANK,
		.loo_couple     = EEMG_CTRL_GPU_HI,
		.loo_mutex      = &gpu_mutex_g,
		.DVTFIXED	= DVTFIXED_VAL_GL,
		.turn_pt	= BANK_GPU_TURN_PT,
		.VCO		= VCO_VAL_GL,
		.DCCONFIG	= DCCONFIG_VAL,
		.EEMCTL0	= EEMG_CTL0_GPU,
		.low_temp_off	= LOW_TEMP_OFF_GPU,
		.high_temp_off	= HIGH_TEMP_OFF_GPU,
		.volt_policy	= 1,

	},

	[EEMG_DET_GPU_HI] = {
		.name		= __stringify(EEMG_DET_GPU_HI),
		.ops		= &gpu_det_ops,
		.ctrl_id	= EEMG_CTRL_GPU_HI,
		.features	= FEA_INIT02 | FEA_MON,
		.VBOOT		= VBOOT_VAL, /* 10uV */
		.VMAX		= VMAX_VAL_GH,
		.VMIN		= VMIN_VAL_GH,
		.eemg_v_base	= EEMG_V_BASE,
		.eemg_step	= EEMG_STEP,
		.pmic_base	= CPU_PMIC_BASE_6359,
		.pmic_step	= CPU_PMIC_STEP,
		.DETWINDOW	= DETWINDOW_VAL,
		.DTHI		= DTHI_VAL,
		.DTLO		= DTLO_VAL,
		.DETMAX		= DETMAX_VAL,
		.AGECONFIG	= AGECONFIG_VAL,
		.AGEM		= AGEM_VAL,
		.DVTFIXED	= DVTFIXED_VAL_GPU,
		.loo_role       = HIGH_BANK,
		.loo_couple     = EEMG_CTRL_GPU,
		.loo_mutex      = &gpu_mutex_g,
		.turn_pt	= BANK_GPU_TURN_PT,
		.VCO		= VCO_VAL_GH,
		.DCCONFIG	= DCCONFIG_VAL,
		.EEMCTL0	= EEMG_CTL0_GPU,
		.low_temp_off	= LOW_TEMP_OFF_GPU,
		.high_temp_off	= HIGH_TEMP_OFF_GPU,
		.volt_policy	= 1,
	},
};

#if DUMP_DATA_TO_DE
const unsigned int reg_gpu_addr_off[DUMP_LEN] = {
	0x0000,
	0x0004,
	0x0008,
	0x000C,
	0x0010,
	0x0014,
	0x0018,
	0x001c,
	0x0024,
	0x0028,
	0x002c,
	0x0030,
	0x0034,
	0x0038,
	0x003c,
	0x0040,
	0x0044,
	0x0048,
	0x004c,
	0x0050,
	0x0054,
	0x0058,
	0x005c,
	0x0060,
	0x0064,
	0x0068,
	0x006c,
	0x0070,
	0x0074,
	0x0078,
	0x007c,
	0x0080,
	0x0084,
	0x0088,
	0x008c,
	0x0090,
	0x0094,
	0x0098,
	0x00a0,
	0x00a4,
	0x00a8,
	0x00B0,
	0x00B4,
	0x00B8,
	0x00BC,
	0x00C0,
	0x00C4,
	0x00C8,
	0x00CC,
	0x00F0,
	0x00F4,
	0x00F8,
	0x00FC,
	0x0190, /* dump this for gpu thermal */
	0x0194, /* dump this for gpu thermal */
	0x0198, /* dump this for gpu thermal */
	0x01B8, /* dump this for gpu thermal */
	0x0C00,
	0x0C04,
	0x0C08,
	0x0C0C,
	0x0C10,
	0x0C14,
	0x0C18,
	0x0C1C,
	0x0C20,
	0x0C24,
	0x0C28,
	0x0C2C,
	0x0C30,
	0x0C34,
	0x0C38,
	0x0C3C,
	0x0C40,
	0x0C44,
	0x0C48,
	0x0C4C,
	0x0C50,
	0x0C54,
	0x0C58,
	0x0C5C,
	0x0C60,
	0x0C64,
	0x0C68,
	0x0C6C,
	0x0C70,
	0x0C74,
	0x0C78,
	0x0C7C,
	0x0C80,
	0x0C84,
	0x0C88, /* dump thermal sensor */
	0x0F00,
	0x0F04,
	0x0F08,
	0x0F0C,
	0x0F10,
	0x0F14,
	0x0F18,
	0x0F1C,
	0x0F20,
	0x0F24,
	0x0F28,
	0x0F2C,
	0x0F30,
};
#endif
#undef __MT_EEMG_INTERNAL_C__
