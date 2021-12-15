/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

/*
 * IRQ/FIQ for TrustZone
 */

#ifndef __KREE_TZ_IRQ_H__
#define __KREE_TZ_IRQ_H__

#if defined(CONFIG_MTK_IN_HOUSE_TEE_SUPPORT) || defined(CONFIG_TRUSTY)

void kree_irq_init(void);
int kree_set_fiq(int irq, unsigned long irq_flags);
void kree_enable_fiq(int irq);
void kree_disable_fiq(int irq);
void kree_query_fiq(int irq, int *enable, int *pending);
unsigned int kree_fiq_get_intack(void);
void kree_fiq_eoi(unsigned int iar);
int kree_raise_softfiq(unsigned int mask, unsigned int irq);
void kree_irq_mask_all(unsigned int *pmask, unsigned int size);
void kree_irq_mask_restore(unsigned int *pmask, unsigned int size);
void kree_set_sysirq_node(struct device_node *pnode);

#else

#define kree_set_fiq(irq, irq_flags)     -1
#define kree_enable_fiq(irq)
#define kree_disable_fiq(irq)

#endif	/* CONFIG_MTK_IN_HOUSE_TEE_SUPPORT || CONFIG_TRUSTY */

#endif				/* __KREE_TZ_IRQ_H__ */
