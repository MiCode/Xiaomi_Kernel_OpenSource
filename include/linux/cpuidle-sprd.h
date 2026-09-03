/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __CPUIDLE_SPRD_H
#define __CPUIDLE_SPRD_H

struct sprd_cpuidle_operations {
	char name[16];

	void (*init)(void);
	void (*light_en)(void);
	void (*light_dis)(void);
	void (*doze_en)(void);
	void (*doze_dis)(void);
};

int sprd_cpuidle_ops_init(struct sprd_cpuidle_operations *cpuidle_ops);
#endif
