/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#ifndef IRQ_REGISTER_H
#define IRQ_REGISTER_H

#define SCHED_ENT_CNT  10

struct load_soter_entry {
	unsigned long vfs_addr;
	struct work_struct work;
};

int register_ut_irq_handler(int irq);
int register_switch_irq_handler(void);
void load_func(struct work_struct *entry);
void work_func(struct work_struct *entry);
void secondary_load_func(void);

int teei_smc(unsigned long long smc_id, unsigned long long p1,
			unsigned long long p2, unsigned long long p3);

#endif /* end of IRQ_REGISTER_H */
