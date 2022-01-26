/*
 * Copyright (C) 2019 MediaTek Inc.
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
#ifndef __APUSYS_MNOC_PMU_H__
#define __APUSYS_MNOC_PMU_H__

struct pmu_reg_list {
	struct list_head list;
	struct mutex list_mtx;
};

struct pmu_reg {
	unsigned int addr;
	unsigned int val;

	struct list_head list;
};

extern bool mnoc_cfg_timer_en;

void enque_pmu_reg(unsigned int addr, unsigned int val);
void clear_pmu_reg_list(void);
void mnoc_pmu_reg_init(void);
void print_pmu_reg_list(struct seq_file *m);
void mnoc_pmu_timer_start(void);
void mnoc_pmu_suspend(void);
void mnoc_pmu_resume(void);
void mnoc_pmu_init(void);
void mnoc_pmu_exit(void);

#endif
