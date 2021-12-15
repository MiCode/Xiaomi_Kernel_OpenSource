// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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
