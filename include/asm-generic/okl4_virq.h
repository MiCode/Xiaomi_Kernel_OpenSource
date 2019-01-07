/*
 * include/asm-generic/okl4_virq.h
 *
 * Copyright (c) 2017 General Dynamics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __OKL4_VIRQ_H__
#define __OKL4_VIRQ_H__

#include <linux/irq.h>
#include <microvisor/microvisor.h>

static inline okl4_virq_flags_t okl4_get_virq_payload(unsigned int irq)
{
	struct irq_data *irqd = irq_get_irq_data(irq);

	if (WARN_ON_ONCE(!irqd))
		return 0;

	return _okl4_sys_interrupt_get_payload(irqd_to_hwirq(irqd)).payload;
}

#endif
