/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_EEM_
#define _MTK_EEM_

#include <linux/kernel.h>
#include <mt-plat/sync_write.h>
#include "mtk_eem_config.h"

#define EN_EEM (1) /* enable/disable EEM (SW) */

enum mt_cpu_dvfs_id {
	MT_CPU_DVFS_LL,
	MT_CPU_DVFS_L,
	MT_CPU_DVFS_CCI,

	NR_MT_CPU_DVFS,
};

enum eemsn_det_id {
	EEMSN_DET_L,
	EEMSN_DET_B,
	EEMSN_DET_CCI,

	NR_EEMSN_DET,
};

enum eemsn_id_sw {
	EEMSN_DET_LOG_L = 0,
	EEMSN_DET_LOG_BIG,
	EEMSN_DET_LOG_CCI,

	NR_EEMSN_DET_LOG_ID,
};

enum sn_det_id {
	SN_DET_L = 0,
	SN_DET_B,
	NR_SN_DET,
};

enum mt_eem_cpu_id {
	MT_EEM_CPU_LL,
	MT_EEM_CPU_L,
	MT_EEM_CPU_CCI,

	NR_MT_EEM_CPU,
};

enum mt_eem_add_extra_mode {
	NO_EXTRA,
	ADD_EXTRA,
	UNDEF_EXTRA,
};

/* internal use */
/* EEM detector is disabled by who */
enum {
	BY_PROCFS	= BIT(0),
	BY_INIT_ERROR	= BIT(1),
	BY_MON_ERROR	= BIT(2),
};

enum eem_phase {
	EEM_PHASE_INIT01,
	EEM_PHASE_INIT02,
	EEM_PHASE_MON,
	EEM_PHASE_SEN,

	NR_EEM_PHASE,
};

enum eem_features {
	FEA_INIT01	= BIT(EEM_PHASE_INIT01),
	FEA_INIT02	= BIT(EEM_PHASE_INIT02),
	FEA_MON		= BIT(EEM_PHASE_MON),
	FEA_SEN	= BIT(EEM_PHASE_SEN),
};

enum {
	EEM_VOLT_NONE	= 0,
	EEM_VOLT_UPDATE  = BIT(0),
	EEM_VOLT_RESTORE = BIT(1),
};

enum {
	EEM_NORMAL_T	= 0,
	EEM_LOW_T,
	EEM_HIGH_T
};


enum eem_loo_role {
	NO_LOO_BANK	=	0,
	LOW_BANK	=	1,
	HIGH_BANK	=	2,

	NR_EEM_LOO_BANK,
};

extern unsigned int eem_corn_flag;

/* EEM Extern Function */
extern int mt_eem_status(enum eemsn_det_id id);
extern unsigned int get_efuse_status(void);
extern unsigned int mt_eem_is_enabled(void);
#endif
