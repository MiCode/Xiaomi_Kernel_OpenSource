/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __EMI_BWL_H__
#define __EMI_BWL_H__

unsigned int get_dram_type(void);
void __iomem *mt_cen_emi_base_get(void);
void __iomem *mt_chn_emi_base_get(void);
void __iomem *mt_emi_mpu_base_get(void);
unsigned int mt_emi_mpu_irq_get(void);
extern void mpu_init(void);
extern unsigned int get_dram_type(void);
#endif  /* !__EMI_BWL_H__ */
