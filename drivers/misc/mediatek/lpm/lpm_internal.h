/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __LPM_INTERNAL_H__
#define __LPM_INTERNAL_H__


#define lpm_system_lock(x) ({\
		unsigned long irqfalg;\
		lpm_system_spin_lock(&irqfalg);\
		x = irqfalg; })


#define lpm_system_unlock(x) ({\
		unsigned long irqfalg = x;\
		lpm_system_spin_unlock(&irqfalg); })


void lpm_system_spin_lock(unsigned long *irqflag);
void lpm_system_spin_unlock(unsigned long *irqflag);

int lpm_platform_init(void);
#endif
