/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef _MTK_EEM_
#define _MTK_EEM_

#include <linux/kernel.h>
#include <mt-plat/sync_write.h>
#include "mtk_eem_config.h"

#define EN_EEM (1) /* enable/disable EEM (SW) */

/* have 5 banks */
enum eem_ctrl_id {
	EEM_CTRL_L,
	EEM_CTRL_2L,
	EEM_CTRL_CCI,
#if ENABLE_LOO
	EEM_CTRL_L_HI,
	EEM_CTRL_2L_HI,
#endif

	NR_EEM_CTRL,
};

enum eem_det_id {
	EEM_DET_L	=	EEM_CTRL_L,
	EEM_DET_2L	=	EEM_CTRL_2L,
	EEM_DET_CCI	=	EEM_CTRL_CCI,
#if ENABLE_LOO
	EEM_DET_L_HI	=	EEM_CTRL_L_HI,
	EEM_DET_2L_HI	=	EEM_CTRL_2L_HI,
#endif

	NR_EEM_DET,
};

enum mt_eem_cpu_id {
	MT_EEM_CPU_L,
	MT_EEM_CPU_LL,
	MT_EEM_CPU_CCI,

	NR_MT_EEM_CPU,
};

/* internal use */
/* EEM detector is disabled by who */
enum {
	BY_PROCFS	= BIT(0),
	BY_INIT_ERROR	= BIT(1),
	BY_MON_ERROR	= BIT(2),
	BY_PROCFS_INIT2 = BIT(3),
};

enum eem_phase {
	EEM_PHASE_INIT01,
	EEM_PHASE_INIT02,
	EEM_PHASE_MON,

	NR_EEM_PHASE,
};

enum eem_features {
	FEA_INIT01	= BIT(EEM_PHASE_INIT01),
	FEA_INIT02	= BIT(EEM_PHASE_INIT02),
	FEA_MON		= BIT(EEM_PHASE_MON),
};

enum {
	EEM_VOLT_NONE	= 0,
	EEM_VOLT_UPDATE  = BIT(0),
	EEM_VOLT_RESTORE = BIT(1),
};

extern const unsigned int reg_dump_addr_off[105];

/* EEM Extern Function */
extern int mt_eem_status(enum eem_det_id id);
extern unsigned int get_efuse_status(void);
extern unsigned int mt_eem_is_enabled(void);

extern void eem_set_pi_efuse(enum eem_det_id id, unsigned int pi_efuse);

/* DRCC */
extern unsigned int drcc_offset_done;
#endif
