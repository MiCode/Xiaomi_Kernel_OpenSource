/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_LPM_INTERNAL_H__
#define __MTK_LPM_INTERNAL_H__


#define mtk_lpm_system_lock(x) ({\
		unsigned long irqfalg;\
		mtk_lpm_system_spin_lock(&irqfalg);\
		x = irqfalg; })


#define mtk_lpm_system_unlock(x) ({\
		unsigned long irqfalg = x;\
		mtk_lpm_system_spin_unlock(&irqfalg); })


int mtk_lpm_system_spin_lock(long *irqflag);
int mtk_lpm_system_spin_unlock(long *irqflag);

#endif
