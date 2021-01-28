/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
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
