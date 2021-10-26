
/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/**
 * @file	mtk_eem_internal.c
 * @brief   Driver for EEM
 *
 */
#define __MTK_EEM_INTERNAL_C__
#include "mtk_eem_prj_config.h"
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
		.volt_offset = 0,
		.eemsn_v_base    = EEMSN_V_BASE,
		.eemsn_step   = EEMSN_STEP,
		.pmic_base    = CPU_PMIC_BASE,
		.pmic_step    = CPU_PMIC_STEP,
	},

	[EEMSN_DET_CCI] = {
		.name			= "EEM_DET_CCI",
		.ops        = &big_det_ops,
		.det_id    = EEMSN_DET_CCI,
		.features	= FEA_INIT02 | FEA_MON,
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
	0x60C,
	0x610,
	0x614,
	0x618,
	0x61C,
#endif
	0x608,
	0x620,
	0x624,
	0x628,
	0x62C,
	0x630,
	0x634,
	0x638,
	0x63C,
	0x640,
	0x644,
	0x670
};

#if 0
struct dvfs_vf_tbl mc50_tbl[NR_EEMSN_DET] = {
	[0] = {
		.pi_vf_num			= 3,
		.pi_freq_tbl		= {2000, 1600, 500},
		.pi_volt_tbl	  = {0x8C, 0x76, 0x6F},

	},
	[1] = {
		.pi_vf_num			= 4,
		.pi_freq_tbl		= {3000, 2600, 1660, 650},
		.pi_volt_tbl	  = {0xAE, 0xA6, 0x76, 0x70},

	},
	[2] = {
		.pi_vf_num			= 3,
		.pi_freq_tbl		= {1700, 1350, 520},
		.pi_volt_tbl	  = {0x8D, 0x74, 0x6F},

	},
};
#endif

#if defined(MC50_LOAD)
struct dvfs_vf_tbl mc50_tbl[NR_EEMSN_DET] = {
	[0] = {
		.pi_vf_num			= 3,
		.pi_freq_tbl		= {2000, 1600, 650},
		.pi_volt_tbl	  = {0x7F, 0x71, 0x59},
	},
	[1] = {
		.pi_vf_num			= 3,
		.pi_freq_tbl		= {2600, 1660, 650},
		.pi_volt_tbl	  = {0x95, 0x73, 0x56},
	},
	[2] = {
		.pi_vf_num			= 3,
		.pi_freq_tbl		= {1700, 1350, 520},
		.pi_volt_tbl	  = {0x7F, 0x71, 0x59},
	},
};
#endif

#undef __MT_EEM_INTERNAL_C__
