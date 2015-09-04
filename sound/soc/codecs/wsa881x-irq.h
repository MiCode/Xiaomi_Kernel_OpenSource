/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __WSA881X_IRQ_H__
#define __WSA881X_IRQ_H__

#include <linux/irqdomain.h>
#include <linux/interrupt.h>
#include <sound/soc.h>

/**
 * enum wsa_interrupts - wsa interrupt number
 * @WSA_INT_SAF2WAR:	Temp irq interrupt, from safe state to warning state.
 * @WSA_INT_WAR2SAF:	Temp irq interrupt, from warning state to safe state.
 * @WSA_INT_DISABLE:	Disable Temp sensor interrupts.
 * @WSA_INT_OCP:	OCP interrupt.
 * @WSA_INT_CLIP:	CLIP detect interrupt.
 * @WSA_NUM_IRQS:	MAX Interrupt number.
 *
 * WSA IRQ Interrupt numbers.
 */
enum wsa_interrupts {
	WSA_INT_SAF2WAR = 0,
	WSA_INT_WAR2SAF,
	WSA_INT_DISABLE,
	WSA_INT_OCP,
	WSA_INT_CLIP,
	WSA_NUM_IRQS,
};

/**
 * struct wsa_resource - the basic wsa_resource structure
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
 * codec:	codec pointer.
 *
 * Contains required members used in wsa irq driver.
 */

struct wsa_resource {
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
	struct snd_soc_codec *codec;
};

void wsa_set_codec(struct snd_soc_codec *codec);
void wsa_free_irq(int irq, void *data);
void wsa_enable_irq(struct wsa_resource *wsa_res, int irq);
void wsa_disable_irq(struct wsa_resource *wsa_res, int irq);
void wsa_disable_irq_sync(struct wsa_resource *wsa_res, int irq);
int wsa_request_irq(struct wsa_resource *wsa_res,
			int irq, irq_handler_t handler,
			const char *name, void *data);

void wsa_irq_exit(struct wsa_resource *wsa_res);

#endif /* __WSA881X_IRQ_H__ */
