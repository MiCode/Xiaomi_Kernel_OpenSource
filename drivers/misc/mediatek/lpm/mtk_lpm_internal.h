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


void mtk_lpm_system_spin_lock(unsigned long *irqflag);
void mtk_lpm_system_spin_unlock(unsigned long *irqflag);

int mtk_lpm_platform_init(void);
#endif
