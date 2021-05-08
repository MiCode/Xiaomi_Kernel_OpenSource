/*
 *  include header file for MediaTek MT6370 PMU Debugfs
 *
 *  Copyright (C) 2018 MediaTek Inc.
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
