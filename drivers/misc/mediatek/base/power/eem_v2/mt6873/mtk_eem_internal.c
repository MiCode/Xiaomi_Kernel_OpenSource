// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/**
 * @file	mtk_eem_internal.c
 * @brief   Driver for EEM
 *
 */
#define __MTK_EEM_INTERNAL_C__
#include "mtk_defeem.h"
#include "mtk_eem_config.h"
#include "mtk_eem.h"
#include "mtk_eem_internal_ap.h"
#include "mtk_eem_internal.h"

#define BASE_OP(fn)	.fn = base_ops_ ## fn
struct eemsn_det_ops eem_det_base_ops = {
#if 0
	BASE_OP(enable),
	BASE_OP(disable),
	BASE_OP(disable_locked),
	BASE_OP(switch_bank),

	BASE_OP(init01),
	BASE_OP(init02),
	BASE_OP(mon_mode),


	BASE_OP(set_phase),
	BASE_OP(dump_status),
	BASE_OP(get_status),

	BASE_OP(get_volt),
	BASE_OP(set_volt),
	BASE_OP(restore_default_volt),
	BASE_OP(get_freq_table),
	BASE_OP(get_orig_volt_table),
#endif
	BASE_OP(get_temp),
	/* platform independent code */
	BASE_OP(volt_2_pmic),
	BASE_OP(volt_2_eem),
	BASE_OP(pmic_2_volt),
	BASE_OP(eem_2_pmic),
};

struct eemsn_det eemsn_detectors[NR_EEMSN_DET] = {
	[EEMSN_DET_L] = {
		.name			= "EEM_DET_L",
		.ops        = &big_det_ops,
		.det_id    = EEMSN_DET_L,
		.features	= FEA_INIT01 | FEA_INIT02 | FEA_MON | FEA_SEN,
		.max_freq_khz = L_FREQ_BASE,
		.volt_offset = 0,

		.eemsn_v_base    = EEMSN_V_BASE,
		.eemsn_step   = EEMSN_STEP,
		.pmic_base    = CPU_PMIC_BASE,
		.pmic_step    = CPU_PMIC_STEP,

	},

	[EEMSN_DET_B] = {
		.name			= "EEM_DET_B",
		.ops        = &big_det_ops,
		.det_id    = EEMSN_DET_B,
		.features	= FEA_INIT01 | FEA_INIT02 | FEA_MON | FEA_SEN,
		.max_freq_khz = B_FREQ_BASE,
		.mid_freq_khz = B_M_FREQ_BASE,
		.volt_offset = 0,

		.eemsn_v_base    = EEMSN_V_BASE,
		.eemsn_step   = EEMSN_STEP,
		.pmic_base    = CPU_PMIC_BASE,
		.pmic_step    = CPU_PMIC_STEP,

		.loo_role	= LOW_BANK,
		.turn_pt	= BANK_B_TURN_PT,
		.turn_freq = B_M_FREQ_BASE,
		.isSupLoo	= 1,

	},

	[EEMSN_DET_CCI] = {
		.name			= "EEM_DET_CCI",
		.ops        = &big_det_ops,
		.det_id    = EEMSN_DET_CCI,
		.features	= FEA_INIT02 | FEA_MON,
		.max_freq_khz = CCI_FREQ_BASE, /* 1248Mhz */
		.volt_offset = 0,

		.eemsn_v_base    = EEMSN_V_BASE,
		.eemsn_step   = EEMSN_STEP,
		.pmic_base    = CPU_PMIC_BASE,
		.pmic_step    = CPU_PMIC_STEP,
	},

};

unsigned int sn_mcysys_reg_base[NUM_SN_CPU] = {
	MCUSYS_CPU0_BASEADDR,
	MCUSYS_CPU1_BASEADDR,
	MCUSYS_CPU2_BASEADDR,
	MCUSYS_CPU3_BASEADDR,
	MCUSYS_CPU4_BASEADDR,
	MCUSYS_CPU5_BASEADDR,
	MCUSYS_CPU6_BASEADDR,
	MCUSYS_CPU7_BASEADDR
};

unsigned short sn_mcysys_reg_dump_off[SIZE_SN_MCUSYS_REG] = {
#if FULL_REG_DUMP_SNDATA
	0x508,
	0x50C,
	0x510,
	0x514,
	0x518,
	0x51C,
#endif
	0x520,
	0x524,
	0x528,
	0x52C,
	0x530,
	0x534,
	0x538,
	0x53C,
	0x540,
	0x544
};

#undef __MT_EEM_INTERNAL_C__
