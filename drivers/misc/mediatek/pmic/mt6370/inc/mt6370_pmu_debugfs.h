/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */



#ifndef __MT6370_PMU_DEBUGFS_H
#define __MT6370_PMU_DEBUGFS_H


#include "mt6370_pmu.h"
#include "mt6370_pmu_dsv_debugfs.h"


extern void mt6370_pmu_dsv_auto_vbst_adjustment(struct mt6370_pmu_chip *chip,
						enum dsv_dbg_mode_t mode);
extern int mt6370_pmu_dsv_scp_ocp_irq_debug(struct mt6370_pmu_chip *chip,
					enum dsv_dbg_mode_t mode);
extern int mt6370_pmu_dsv_debug_init(struct mt6370_pmu_chip *chip);


#endif /* #ifndef ____MT6370_PMU_DEBUGFS_H */
