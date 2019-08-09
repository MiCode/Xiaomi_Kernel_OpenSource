/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __MTK_SPM_IRQ_H__
#define __MTK_SPM_IRQ_H__

/* for mtk_spm.c */
int mtk_spm_irq_register(unsigned int spm_irq_0);

/* for mtk_spm_idle.c and mtk_spm_suspend.c */
void mtk_spm_irq_backup(void);
void mtk_spm_irq_restore(void);
unsigned int mtk_spm_get_irq_0(void);

#endif /* __MTK_SPM_IRQ_H__ */
