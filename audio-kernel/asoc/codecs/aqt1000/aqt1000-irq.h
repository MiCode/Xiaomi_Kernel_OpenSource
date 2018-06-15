/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __AQT1000_IRQ_H_
#define __AQT1000_IRQ_H_

#include <linux/interrupt.h>
#include <linux/regmap.h>

enum {
	/* INTR_CTRL_INT_MASK_2 */
	AQT1000_IRQ_MBHC_BUTTON_RELEASE_DET = 0,
	AQT1000_IRQ_MBHC_BUTTON_PRESS_DET,
	AQT1000_IRQ_MBHC_ELECT_INS_REM_DET,
	AQT1000_IRQ_MBHC_ELECT_INS_REM_LEG_DET,
	AQT1000_IRQ_MBHC_SW_DET,
	AQT1000_IRQ_HPH_PA_OCPL_FAULT,
	AQT1000_IRQ_HPH_PA_OCPR_FAULT,
	AQT1000_IRQ_HPH_PA_CNPL_COMPLETE,

	/* INTR_CTRL_INT_MASK_3 */
	AQT1000_IRQ_HPH_PA_CNPR_COMPLETE,
	AQT1000_CDC_HPHL_SURGE,
	AQT1000_CDC_HPHR_SURGE,
	AQT1000_PLL_LOCK_LOSS,
	AQT1000_FLL_LOCK_LOSS,
	AQT1000_DSD_INT,
	AQT1000_NUM_IRQS,
};

/**
 * struct aqt_irq - AQT IRQ resource structure
 * @irq_lock:	lock used by irq_chip functions.
 * @nested_irq_lock: lock used while handling nested interrupts.
 * @irq:	interrupt number.
 * @irq_masks_cur: current mask value to be written to mask registers.
 * @irq_masks_cache: cached mask value.
 * @num_irqs: number of supported interrupts.
 * @num_irq_regs: number of irq registers.
 * @parent:	parent pointer.
 * @dev:	device pointer.
 * @domain:	irq domain pointer.
 *
 * Contains required members used in wsa irq driver.
 */

struct aqt1000_irq {
	struct mutex irq_lock;
	struct mutex nested_irq_lock;
	unsigned int irq;
	u8 irq_masks_cur;
	u8 irq_masks_cache;
	bool irq_level_high[8];
	int num_irqs;
	int num_irq_regs;
	void *parent;
	struct device *dev;
	struct irq_domain *domain;
};

int aqt_irq_init(void);
void aqt_irq_exit(void);

#endif /* __AQT1000_IRQ_H_ */
